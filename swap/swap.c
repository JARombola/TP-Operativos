/*
 * swap.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <commons/string.h>
#include <commons/config.h>

#define PUERTO_SWAP 6660
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

typedef struct{
	int puerto;
	char* nombre_swap;
	int cantidadPaginas;
	int tamPagina;
	int retardoCompactacion;
}datosConfiguracion;

int crear_socket_server(int puerto);
int aceptar(int server);
void leerConfiguracion(char*, datosConfiguracion*);

int main(int argc, char* argv[]){
//	datosConfiguracion* datosSwap;
//	leerConfiguracion(argv[1],datosConfiguracion);
	int swap_servidor = crear_socket_server(PUERTO_SWAP);
	int umc_cliente;

	printf("Estoy escuchando\n");
	listen(swap_servidor,100);

	//----------------------------creo cliente para umc
	umc_cliente = aceptar(swap_servidor);

	//----------------recibo datos de la UMC

	while (1){
		int protocoloUMC=0; //donde quiero recibir y cantidad que puedo recibir
			int bytesRecibidosUMC = recv(umc_cliente, &protocoloUMC, sizeof(int32_t), 0);
			protocoloUMC=ntohl(protocoloUMC);
				if(bytesRecibidosUMC <= 0){
					perror("la UMC se desconecto o algo. Se la elimino\n");
					return 0; //todo que vuelva al while anterior y espera a la umc devuelta
				} else {
					char* bufferUMC = malloc(protocoloUMC * sizeof(char) + 1);
					bytesRecibidosUMC = recv(umc_cliente, bufferUMC, protocoloUMC, 0);
					bufferUMC[protocoloUMC + 1] = '\0'; //para pasarlo a string (era un stream)
					printf("UMC: %d, me llegaron %d bytes con %s\n", umc_cliente,bytesRecibidosUMC, bufferUMC);
					free(bufferUMC);
				}
	}
	//free(datosSwap);
	return 0;
}

int crear_socket_server(int puerto){

	struct sockaddr_in direccionServer;
	direccionServer.sin_family = AF_INET;
	direccionServer.sin_addr.s_addr = INADDR_ANY;
	direccionServer.sin_port = htons(puerto);

		int servidor = socket(AF_INET, SOCK_STREAM, 0);

		printf("se creo la swap en %d\n",servidor);

		int activado = 1;
		setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado)); //para cerrar los binds al cerrar
		if (bind(servidor, (void *)&direccionServer, sizeof(direccionServer)) != 0){
			perror("Fallo el bind");
			return 1;
		}
		return servidor;
}

int aceptar(int server){
	struct sockaddr_in direccionCliente; //direccion donde guarde el cliente
		int sin_size = sizeof(struct sockaddr_in);
		int cliente;
		int seConecto=1;
		while(seConecto){
			cliente = accept(server, (void *) &direccionCliente, (void *)&sin_size);
			if (cliente == -1){
				perror("Fallo el accept");
			}
			printf("Recibi una conexion en %d!!\n", cliente);
		char* bufferHandshake = malloc(10);
			int bytesRecibidosH = recv(cliente, bufferHandshake, 10, 0);
			bufferHandshake[bytesRecibidosH] = '\0'; //lo paso a string para comparar
				if(strcmp("soy_la_umc",bufferHandshake) != 0){
					perror("No lo tengo que aceptar, no es la UMC");
				}else{
					send(cliente, "Hola umc",8,0); //handshake para consola
					seConecto = 0;
				}
		}
		return cliente;
}

void leerConfiguracion(char *ruta, datosConfiguracion *datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		perror("FIN PROGRAMA");
		exit(0);
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys != 5) {
			perror("ERROR CANTIDAD DATOS DE CONFIGURACION");
		} else {
			datos->puerto = buscarInt(archivoConfiguracion, "PUERTO");
			datos->nombre_swap = config_get_string_value(archivoConfiguracion, "NOMBRE_SWAP");
			datos->cantidadPaginas = buscarInt(archivoConfiguracion, "CANTIDAD_PAGINAS");
			datos->tamPagina = buscarInt(archivoConfiguracion, "TAMAÑO_PAGINA");
			datos->retardoCompactacion = buscarInt(archivoConfiguracion, "RETARDO_COMPACTACION");
			config_destroy(archivoConfiguracion);
		}
	}
}
