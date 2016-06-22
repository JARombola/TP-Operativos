/*
* consola.c
 *
 *  Created on: 29/4/2016
 *      Author: utnso
 */

#include <stdlib.h>
#include <stdio.h>
#include <commons/string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Funciones/Comunicacion.h"

#define PUERTO_NUCLEO 6662
#define IP_NUCLEO "127.0.0.1"

//int protocolo(int nucleo);
int autentificar(int);
int enviarAnsisop(FILE*, int);

int main(int argc, char* argv[]) {//Se le envia por parametro el archivo a ejecutar (#!, ver "Nuevo")
	printf("Consola creada. Conectando al Nucleo...\n");
	int nucleo=conectar(PUERTO_NUCLEO,IP_NUCLEO);
	if (!autentificar(nucleo)) {
		printf("Conexion al nucleo fail, error handshake\n");
		return -1;
	}
	printf("Conexion Ok\n");
	FILE* ansisop=fopen(argv[1],"r");
	if (!ansisop){perror("Archivo");}
	if(enviarAnsisop(ansisop, nucleo)){printf("Error en el envio del codigo\n");}
	char respuesta;
	recv(nucleo,&respuesta,1,0);
	if (!string_itoa(respuesta)){
		printf("Ansisop rechazado\n Consola finalizada\n");
		return -1;
	}
	printf("Ansisop enviado con éxito\n");

	while (1){
		//recibo de nucleo
		int tamanio;
		char* texto;
		int protocolo = recibirProtocolo(nucleo);
		switch (protocolo) {
					case 0:															//Error
						perror("El nucleo se desconecto o hubo error de protocolo\n");
						close(nucleo);
						return -1;
						break;
					case 1:														//IMPRIMIR
						printf("el nucleo quiere que imprima\n");
						tamanio = recibirProtocolo(nucleo);
						texto = recibirMensaje(nucleo,tamanio);
						texto[tamanio]='\0';
						printf("%s\n",texto);
						free (texto);
						break;
					case 2:														//TERMINO BIEN
						printf("el programa finalizo con exito\n");
						send(nucleo,"0001",4,0);
						return -1;
						break;
					case 3:														//TERMINO MAL
						printf("hubo un error en la ejecucion del programa\n");
						send(nucleo,"0001",4,0);
						return -1;
						break;
					}
	}
	return 0;
}

int autentificar(int conexion){
	send(conexion, "soy_consola", 11, 0);
	char respuesta;
	int bytesRecibidosH = recv(conexion, &respuesta, 1, 0);
	if (bytesRecibidosH <= 0) {
		return 0;
	}
	return htonl(respuesta);
}

int enviarAnsisop(FILE* archivo, int sockNucleo){
	fseek (archivo, 0, SEEK_END);
	int bytesArchivo = ftell (archivo);
	fseek (archivo, 0, SEEK_SET);
	char* codigo = (char*)malloc(bytesArchivo+4); 											//+4 Para el header (longitud)
	if (codigo){
		fread (codigo, sizeof(char), bytesArchivo, archivo);
	}
	else {
		perror("Error Malloc");
		free(codigo);
		fclose(archivo);
		return 1;
	}
	fclose (archivo);
	agregarHeader(&codigo);
	codigo[bytesArchivo+4]='\0';
	int enviados=send(sockNucleo, codigo, string_length(codigo), 0);
	//printf("Codigo: %s %d\n",codigo,string_length(codigo));
	free(codigo);
	if(enviados==string_length(codigo)){return 0;}										//Envio ok
	return 1;																			//Error
}
