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

int main(int argc, char* argv[]) {					//Se le envia por parametro el archivo a ejecutar (#!, ver "Nuevo")
//	if (argc<2) {printf("No se envio el archivo ANSISOP\n");return 1;}
//	system(argv[1]);
	struct sockaddr_in direccNucleo;
	direccNucleo.sin_family = AF_INET;
	direccNucleo.sin_addr.s_addr = INADDR_ANY;
	direccNucleo.sin_port = htons(PUERTO_NUCLEO);

	int consola_cliente = socket(AF_INET, SOCK_STREAM, 0);
	printf("me cree, estoy en el %d\n", consola_cliente);

	if (connect(consola_cliente, (void*) &direccNucleo, sizeof(direccNucleo)) == -1) {
		perror("No se pudo conectar con el Nucleo");
		return 1;
	}

	//hanshake
	send(consola_cliente, "soy_una_consola", 15, 0);
	char* bufferHandshake = malloc(12);
	int bytesRecibidos = recv(consola_cliente, bufferHandshake, 12, 0);
	if (bytesRecibidos <= 0) {
		printf("se recibieron %d bytes, no estamos aceptados\n", bytesRecibidos);
		return 1;
	} else {
		printf("se recibieron %d bytes, estamos aceptados!\n", bytesRecibidos);
	}

	while (1) {
		char *mensaje;
		mensaje = string_new();
		scanf("%s", mensaje);
		int longitud = htonl(string_length(mensaje));
		send(consola_cliente, &longitud, sizeof(int32_t), 0);
		send(consola_cliente, mensaje, strlen(mensaje), 0); //envia datos por teclado
	}
	return 0;
}
