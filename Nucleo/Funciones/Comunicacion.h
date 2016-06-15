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

typedef struct {
	int puerto_nucleo;
	int puerto_umc;
	char* ip_umc;
	char* ip;
	int quantum;
	int quantum_sleep;
	char** sem_ids;
	char** sem_init;		//TRANSFORMAR CON (atoi) - gracias (:
	char** io_ids;
	char** io_sleep;		//LO MISMO
	char** shared_vars;
	int tamStack;
} datosConfiguracion;

extern datosConfiguracion* datosNucleo;

int leerConfiguracion(char*, datosConfiguracion**);
struct sockaddr_in crearDireccion(int puerto,char* ip);
int autentificarUMC(int);
int comprobarCliente(int);
int bindear(int socket, struct sockaddr_in direccionServer);
int conectar(int puerto,char* ip);
int recibirProtocolo(int conexion);
char* recibirMensaje(int conexion, int longitud);
char* header(int numero);							//Recibe numero de bytes, y lo devuelve en 4 bytes (Ej. recibe "2" y devuelve "0002"
void agregarHeader(char** mensaje);

#endif /* COMUNICACION_H_ */
