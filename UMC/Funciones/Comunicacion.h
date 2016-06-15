/*
 * Comunicacion.h
 *
 *  Created on: 21/5/2016
 *      Author: utnso
 */

#ifndef COMUNICACION_H_
#define COMUNICACION_H_
#include <stdio.h>
#include <stdlib.h>
#include <commons/string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/config.h>
#include "Paginas.h"
#include "ArchivosLogs.h"

typedef struct{
	char *ip, *ip_swap;				//PASAR A IP CON: inet_addr() / o inet_ntoa()
	int puerto_umc, puerto_swap, marcos, marco_size, marco_x_proc, entradas_tlb, retardo, algoritmo;
}datosConfiguracion;



extern t_log* archivoLog;
extern datosConfiguracion* datosMemoria;

int leerConfiguracion(char*, datosConfiguracion**);
struct sockaddr_in crearDireccion(int puerto,char* ip);
int bindear(int socket, struct sockaddr_in direccionServer);
int conectar(int puerto,char* ip);
int autentificar(int);
int comprobarCliente(int);
int aceptarNucleo(int,struct sockaddr_in);
int recibirProtocolo(int conexion);
char* recibirMensaje(int conexion, int longitud);
char* header(int numero);							//Recibe numero de bytes, y lo devuelve en 4 bytes (Ej. recibe "2" y devuelve "0002"
void agregarHeader(char** mensaje);

#endif /* COMUNICACION_H_ */
