/*
 * consola.c
 *
 *  Created on: 29/4/2016
 *      Author: utnso
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PUERTO_NUCLEO 6662
int protocolo(int nucleo);
int conectarNucleo (int, struct sockaddr_in );

int main(int argc, char* argv[]) {//Se le envia por parametro el archivo a ejecutar (#!, ver "Nuevo")
//	if (argc<2) {printf("No se envio el archivo ANSISOP\n");return 1;}
//	system(argv[1]);
	struct sockaddr_in direccNucleo;
	direccNucleo.sin_family = AF_INET;
	direccNucleo.sin_addr.s_addr = INADDR_ANY;
	direccNucleo.sin_port = htons(PUERTO_NUCLEO);

	int consola_cliente = socket(AF_INET, SOCK_STREAM, 0),conexionNucleo=0;
	printf("me cree, estoy en el %d\n", consola_cliente);

	while (!conexionNucleo) {
		conexionNucleo=conectarNucleo(consola_cliente,direccNucleo);}
		printf("Consola:%d\n", conexionNucleo);
		//hasta que no se conecta al nucleo no puede mandar mensajes
	while (1){
		char *mensaje;
		mensaje = string_new();
		scanf("%s", mensaje);
		int longitud = htonl(string_length(mensaje));
		send(consola_cliente, &longitud, sizeof(int32_t), 0);
		send(consola_cliente, mensaje, strlen(mensaje), 0); //envia datos por teclado*/

		//recibo de nucleo

		switch (protocolo(consola_cliente)) { //misma idea de aceptar clientes nuevos del nucleo
					case 0:															//Error
						perror("El nucleo se desconecto\n");
						close(consola_cliente);
						//todo intentar conectar? o sin nucleo el programa tiene que terminar forzosamente?
						return -1;
						break;
					case 1:														//IMPRIMIR
						printf("el nucleo quiere que imprima\n");

						break;
					case 2:														//IMPRIMIR TEXTO
						printf("el nucleo quiere que imprima texto\n");

						break;
					}
				/*	todo luego, tengo otro protocolo de tamanio? o directo recibo lo que tengo que imprimir?
					char* bufferC = malloc(protocoloC * sizeof(char) + 1);
					bytesRecibidosC = recv(unaConsola, bufferC, protocoloC, 0);
					bufferC[protocoloC + 1] = '\0';
					printf("cliente: %d, me llegaron %d bytes con %s\n", unaConsola,bytesRecibidosC, bufferC);*/

	}
	return 0;
}

int conectarNucleo (int socket, struct sockaddr_in nucleo) {
if (connect(socket, (void*) &nucleo, sizeof(nucleo))!=0){
		return 0;}
//hanshake
	send(socket, "soy_una_consola", 15, 0);
	char* bufferHandshake = malloc(12);
	int bytesRecibidos = recv(socket, bufferHandshake, 12, 0);
	if (bytesRecibidos <= 0) {
		printf("se recibieron %d bytes, no estamos aceptados\n",bytesRecibidos);
		free(bufferHandshake);
		return 0;
	}
	printf("se recibieron %d bytes, estamos aceptados!\n", bytesRecibidos);
	free(bufferHandshake);
	return socket;
}

int protocolo(int nucleo) {
	char* buffer = malloc(1);
	int bytesRecibidos = recv(nucleo, buffer, 1, 0);
	buffer[bytesRecibidos] = '\0'; //lo paso a string para comparar
	if(bytesRecibidos <= 0){ //se desconecto
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
}
