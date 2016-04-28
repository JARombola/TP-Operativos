/*
 * umc.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <commons/string.h>
#include <stdlib.h>
#include <commons/config.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define esIgual(a,b) string_equals_ignore_case(a,b)
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)


typedef struct{
	int puerto;
	char* ip;				//PASAR A IP CON: inet_addr() / o inet_ntoa()
	int puerto_swap;
	int marcos;
	int marco_size;
	int marco_x_proc;
	int entradas_tlb;
	int retardo;
}datosConfiguracion;

void leerConfiguracion(char*, datosConfiguracion*);

int main(int argc, char* argv[]) { //SOCKETS, CONEXION, BLA...
	datosConfiguracion datosMemoria;
	char* comando;
	int velocidad;
	leerConfiguracion(argv[1], &datosMemoria);
//	printf("Puerto: %d\n",datosMemoria.puerto);
	while (1) {
		comando = string_new(), scanf("%s", comando);
		if (esIgual(comando, "retardo")) {
			printf("velocidad nueva:");
			scanf("%d", &velocidad);
			printf("Velocidad actualizada:%d\n", velocidad);
		} else {
			if (esIgual(comando, "dump")) {
				printf("Estructuras de Memoria\n");
				printf("Datos de Memoria\n");
			} else {
				if (esIgual(comando, "tlb")) {
					printf("TLB Borrada :)\n");
				} else {
					if (esIgual(comando, "memoria")) {
						printf("Memoria Limpiada :)\n");
					}
				}
			}
		}
	}
	return 0;
}

void leerConfiguracion(char *ruta, datosConfiguracion *datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		perror("FIN PROGRAMA");
		exit(0);
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys != 8) {
			perror("ERROR CANTIDAD DATOS DE CONFIGURACION");
		} else {
			datos->puerto = buscarInt(archivoConfiguracion, "PUERTO");
			datos->puerto_swap = buscarInt(archivoConfiguracion, "PUERTO_SWAP");
			datos->marcos = buscarInt(archivoConfiguracion, "MARCOS");
			datos->marco_size = buscarInt(archivoConfiguracion, "MARCO_SIZE");
			datos->marco_x_proc = buscarInt(archivoConfiguracion, "MARCO_X_PROC");
			datos->entradas_tlb = buscarInt(archivoConfiguracion, "ENTRADAS_TLB");
			datos->retardo = buscarInt(archivoConfiguracion, "RETARDO");
			struct sockaddr_in ipLinda;			//recurso TURBIO para guardar la ip :/
			char *direccion;
			inet_aton(config_get_string_value(archivoConfiguracion,"IP"), &ipLinda.sin_addr); // store IP in antelope
			direccion = inet_ntoa(ipLinda.sin_addr); // return the IP
			datos->ip=direccion;
			config_destroy(archivoConfiguracion);
		}
	}
}

