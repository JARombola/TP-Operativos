/*
 * swap.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <commons/config.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "Funciones/Comunicacion.h"

#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

typedef struct{
	int puerto;
	char* ip;
	char* nombre_swap;
	int cantidadPaginas;
	int tamPagina;
	int retardoCompactacion;
}datosConfiguracion;

typedef struct{
	int proceso,inicio,offset;
}traductor_marco;

struct sockaddr_in crearDireccion(int puerto, char* ip);
int comprobarCliente(int);
int leerConfiguracion(char*, datosConfiguracion**);
char* crearArchivoSwap();

datosConfiguracion *datosSwap;

int main(int argc, char* argv[]){
	int	umc, swap_servidor=socket(AF_INET, SOCK_STREAM, 0);

	datosSwap=malloc(sizeof(datosConfiguracion));

	if (!(leerConfiguracion("ConfigSwap", &datosSwap)|| leerConfiguracion("../ConfigSwap", &datosSwap))) {
		printf("Error archivo de configuracion\n FIN.");
		return 1;}

	char* archivoSwap=crearArchivoSwap();

	if (string_equals_ignore_case(archivoSwap,"Fuiste")){printf("Error al crear el archivo Swap\n");return 0;}
	//memcpy(archivoSwap,"Prueba",6);
	munmap(archivoSwap,sizeof(archivoSwap));

	struct sockaddr_in direccionServer=crearDireccion(datosSwap->puerto, datosSwap->ip);

	printf("Swap OK - ");

	if (!bindear(swap_servidor, direccionServer)) {printf("Error en el bind, Adios!\n");
		return 1;
	}

	printf("Esperando UMC...\n");
	listen(swap_servidor,5);

	//----------------------------creo cliente para umc

	struct sockaddr_in direccionCliente;
	int sin_size = sizeof(struct sockaddr_in);
	do{
	umc= accept(swap_servidor, (void *) &direccionCliente, (void *)&sin_size);
				if (umc == -1){
					perror("Una conexion rechazada\n");}
	}while(!comprobarCliente(umc));						//Mientras el que se conecta no es la UMC
	printf("UMC Conectada!\n");

	//----------------Recibo datos de la UMC

	while (1){
		//int tamCodigo=recibirProtocolo(swap_servidor);
		//char* codigo=malloc(tamCodigo);
	}
	free(datosSwap);
	return 0;
}


//-----------------------------------FUNCIONES-----------------------------------

int comprobarCliente(int cliente){
	char* bufferHandshake = malloc(11);
	int bytesRecibidosH = recv(cliente, bufferHandshake, 10, 0);
	bufferHandshake[bytesRecibidosH] = '\0'; 					//lo paso a string para comparar
	if (string_equals_ignore_case("soy_la_umc", bufferHandshake)) {
		free(bufferHandshake);
		send(cliente, "Hola umc", 8, 0);
		return 1;}
	free(bufferHandshake);
	return 0;													//No era la UMC :/
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
