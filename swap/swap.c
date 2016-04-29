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

typedef struct{
	int puerto;
	char* nombre_swap;
	int cantidadPaginas;
	int tamañoPagina;
	int retardoCompactacion;
}datosConfiguracion;

void leerConfiguracion(char*, datosConfiguracion*);

int main(int argc, char* argv[]){
//	datosConfiguracion datosSwap;
//	leerConfiguracion(ruta,datosConfiguracion);
	struct sockaddr_in direccionSwap;

	direccionSwap.sin_family = AF_INET;
	direccionSwap.sin_addr.s_addr = INADDR_ANY;
	direccionSwap.sin_port = htons(PUERTO_SWAP);

	int swap_servidor = socket(AF_INET, SOCK_STREAM, 0);
	int umc_cliente;
	printf("se creo la swap en %d\n",swap_servidor);

	int activado = 1;
	setsockopt(swap_servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado)); //para cerrar los binds al cerrar
	if (bind(swap_servidor, (void *)&direccionSwap, sizeof(direccionSwap)) != 0){
		perror("Fallo el bind");
		return 1;
	}
	printf("Estoy escuchando\n");
	listen(swap_servidor,100);

	//----------------------------creo cliente para umc

	struct sockaddr_in direccionUMC; //direccion donde guarde el cliente
	int sin_size = sizeof(struct sockaddr_in);

	int seConecto=1;
	while(seConecto){
		umc_cliente = accept(swap_servidor, (void *) &direccionUMC, (void *)&sin_size);
		if (umc_cliente == -1){
			perror("Fallo el accept");
		}
		printf("Recibi una conexion en %d!!\n", umc_cliente);
	//---------------------------------handshake
		char* bufferHandshake = malloc(10);
		int bytesRecibidosH = recv(umc_cliente, bufferHandshake, 10, 0);
		bufferHandshake[bytesRecibidosH] = '\0'; //lo paso a string para comparar
			if(strcmp("soy_la_umc",bufferHandshake) != 0){
				perror("No lo tengo que aceptar, no es la UMC");
			}else{
				send(umc_cliente, "Hola consola",12,0); //handshake para consola
				seConecto = 0;
			}
	}
	//----------------recibo datos de la UMC

	while (1){
		int protocoloUMC=0; //donde quiero recibir y cantidad que puedo recibir
			int bytesRecibidosUMC = recv(umc_cliente, &protocoloUMC, sizeof(int32_t), 0);
			protocoloUMC=ntohl(protocoloUMC);
				if(bytesRecibidosUMC <= 0){
					perror("la UMC se desconecto o algo. Se la elimino\n");
					return 0; //todo que vuelva al while anterior y espera a la consola devuelta
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
			datos->tamañoPagina = buscarInt(archivoConfiguracion, "TAMAÑO_PAGINA");
			datos->retardoCompactacion = buscarInt(archivoConfiguracion, "RETARDO_COMPACTACION");
			config_destroy(archivoConfiguracion);
		}
	}
}
