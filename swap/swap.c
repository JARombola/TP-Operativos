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
#include <sys/mman.h>
#include <unistd.h>
#include <commons/string.h>
#include <commons/config.h>
#include <sys/stat.h>
#include <fcntl.h>

#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

typedef struct{
	int puerto;
	char* ip;
	char* nombre_swap;
	int cantidadPaginas;
	int tamPagina;
	int retardoCompactacion;
}datosConfiguracion;

struct sockaddr_in crearDireccion(int puerto, char* ip);
int comprobarCliente(int);
int leerConfiguracion(char*, datosConfiguracion**);
char* crearArchivoSwap();

datosConfiguracion *datosSwap;

int main(int argc, char* argv[]){

	datosSwap=malloc(sizeof(datosConfiguracion));
	if (!(leerConfiguracion("ConfigSwap", &datosSwap)|| leerConfiguracion("../ConfigSwap", &datosSwap))) {
		printf("Error archivo de configuracion\n FIN.");
		return 1;
	}

	char* archivoSwap=crearArchivoSwap();
	if (!strcmp(archivoSwap,"Fuiste")){printf("Error al crear el archivo Swap\n");return 0;}
	//memcpy(archivoSwap,"Prueba",6);
	munmap(archivoSwap,sizeof(archivoSwap));

	struct sockaddr_in direccionServer=crearDireccion(datosSwap->puerto, datosSwap->ip);
	int	swap_servidor=socket(AF_INET, SOCK_STREAM, 0),
		umc_cliente;
	printf("Swap OK - ");

	int activado = 1;
	setsockopt(swap_servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado)); //para cerrar los binds al cerrar
		if (bind(swap_servidor, (void *)&direccionServer, sizeof(direccionServer))){
			perror("Fallo el bind");
			return 1;
		}
	printf("Esperando UMC...\n");
	listen(swap_servidor,5);

	//----------------------------creo cliente para umc

	struct sockaddr_in direccionCliente;
	int sin_size = sizeof(struct sockaddr_in);
	do{
	umc_cliente = accept(swap_servidor, (void *) &direccionCliente, (void *)&sin_size);
				if (umc_cliente == -1){
					perror("Fallo el accept");
				}
				printf("Recibi una conexion\n");}
	while(!comprobarCliente(umc_cliente));						//Mientras el que se conecta no es la UMC
	printf("UMC Conectada!\n");

	//----------------Recibo datos de la UMC

	while (1){
		int protocoloUMC=0;
			int bytesRecibidosUMC = recv(umc_cliente, &protocoloUMC, sizeof(int32_t), 0);
			protocoloUMC=ntohl(protocoloUMC);
				if(bytesRecibidosUMC <= 0){
					perror("la UMC se desconecto o algo. Se la elimino\n Swap autodestruida\n");
					close(umc_cliente);
					return 0;
				} else {
					char* bufferUMC = malloc(protocoloUMC * sizeof(char) + 1);
					bytesRecibidosUMC = recv(umc_cliente, bufferUMC, protocoloUMC, 0);
					bufferUMC[protocoloUMC + 1] = '\0';
					printf("UMC: %d, me llegaron %d bytes con %s\n", umc_cliente,bytesRecibidosUMC, bufferUMC);
					free(bufferUMC);
				}
	}
	free(datosSwap);
	return 0;
}


//-----------------------------------FUNCIONES-----------------------------------
struct sockaddr_in crearDireccion(int puerto, char* ip) {
	struct sockaddr_in direccion;
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr = inet_addr(ip);
	direccion.sin_port = htons(puerto);
	return direccion;
}


int comprobarCliente(int cliente){
	char* bufferHandshake = malloc(11);
	int bytesRecibidosH = recv(cliente, bufferHandshake, 10, 0);
	bufferHandshake[bytesRecibidosH] = '\0'; 					//lo paso a string para comparar
	if (strcmp("soy_la_umc", bufferHandshake)) {
		return 0;}															//No era la UMC :/
	send(cliente, "Hola umc", 8, 0);
	return 1;
	free(bufferHandshake);
}

int leerConfiguracion(char *ruta, datosConfiguracion **datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		return 0;
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys < 6) {
			return 0;
		} else {
			(*datos)->puerto = buscarInt(archivoConfiguracion, "PUERTO");
			char* nombreSwap=string_new();
			string_append(&nombreSwap,config_get_string_value(archivoConfiguracion, "NOMBRE_SWAP"));
			(*datos)->nombre_swap =nombreSwap;
			(*datos)->cantidadPaginas = buscarInt(archivoConfiguracion, "CANTIDAD_PAGINAS");
			(*datos)->tamPagina = buscarInt(archivoConfiguracion, "TAM_PAGINA");
			(*datos)->retardoCompactacion = buscarInt(archivoConfiguracion, "RETARDO_COMPACTACION");
			char* ip=string_new();
			string_append(&ip,config_get_string_value(archivoConfiguracion,"IP"));
			(*datos)->ip=ip;
			config_destroy(archivoConfiguracion);
			return 1;
		}
	}
}
char* crearArchivoSwap(){
	char* instruccion=string_from_format("dd if=/dev/zero of=%s count=1 bs=100",datosSwap->nombre_swap,datosSwap->cantidadPaginas,datosSwap->tamPagina);
	system(instruccion);
	char* nombreArchivo=string_new();
	string_append(&nombreArchivo,datosSwap->nombre_swap);
	int fd_archivo=open(nombreArchivo,O_RDWR);
	if (fd_archivo!=-1) {
	char* archivo=(char*) mmap(NULL ,datosSwap->cantidadPaginas*datosSwap->tamPagina,PROT_READ|PROT_WRITE,MAP_SHARED,(int)fd_archivo,0);
	if (archivo!=MAP_FAILED){return archivo;}
	}
	return "Fuiste";
}
