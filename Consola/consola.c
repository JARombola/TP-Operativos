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

int protocolo(int nucleo);
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
	recv(nucleo,respuesta,1,0);
	if (!string_itoa(respuesta)){
		printf("Ansisop rechazado\n Consola finalizada\n");
		return -1;
	}
	printf("Ansisop enviado con éxito\n");
	while (1){
//		char *mensaje;
//		mensaje = string_new();
//		scanf("%s", mensaje);
//		int longitud = htonl(string_length(mensaje));
//		send(nucleo, &longitud, sizeof(int32_t), 0);
//		send(nucleo, mensaje, string_length(mensaje), 0); //envia datos por teclado*/
//
//		//recibo de nucleo
//
//		switch (protocolo(nucleo)) { //misma idea de aceptar clientes nuevos del nucleo
//					case 0:															//Error
//						perror("El nucleo se desconecto\n");
//						close(nucleo);
//						// intentar conectar? o sin nucleo el programa tiene que terminar forzosamente?
//						return -1;
//						break;
//					case 1:														//IMPRIMIR
//						printf("el nucleo quiere que imprima\n");
//						break;
//					case 2:														//IMPRIMIR TEXTO
//						printf("el nucleo quiere que imprima texto\n");
//						break;
//					}
//				/*	luego, tengo otro protocolo de tamanio? o directo recibo lo que tengo que imprimir?
//					char* bufferC = malloc(protocoloC * sizeof(char) + 1);
//					bytesRecibidosC = recv(unaConsola, bufferC, protocoloC, 0);
//					bufferC[protocoloC + 1] = '\0';
//					printf("cliente: %d, me llegaron %d bytes con %s\n", unaConsola,bytesRecibidosC, bufferC);*/
//
	}
	return 0;
}

int autentificar(int conexion){
	send(conexion, "soy_una_consola", 15, 0);
	return (recibirProtocolo(conexion));			//ME DEVUELVE EL PUERTO DE LA UMC o 0 si hubo error
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

int protocolo(int nucleo) {
	char* buffer = malloc(2);
	int bytesRecibidos = recv(nucleo, buffer, 1, 0);
	buffer[bytesRecibidos] = '\0'; 								//lo paso a string para comparar
	if(bytesRecibidos <= 0){ 						//se desconecto
		printf("Error\n");
		free(buffer);
		return 0;
	}
	if (strcmp("1", buffer) == 0) {//quiere imprimir
		free(buffer);
		return 1;
	} else if (strcmp("2", buffer) == 0) { //quiere imprimir texto
		free(buffer);
		return 2;
	}
	free(buffer);
	return -1;
}

