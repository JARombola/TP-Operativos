/*
 * servidor.c
 *
 *  Created on: 21/4/2016
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

#define PUERTO_CONSOLA 8080

int main(void){

	struct sockaddr_in direccionServidor; //creo la direccion

	direccionServidor.sin_family = AF_INET;
	direccionServidor.sin_addr.s_addr = INADDR_ANY;
	direccionServidor.sin_port = htons(PUERTO_CONSOLA); //datos de la direccion

	int servidor = socket(AF_INET, SOCK_STREAM, 0); //creo el descriptor con esa direccion
	printf("se creo el servidor %d\n",servidor);

	int activado = 1;
	setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado)); //para cerrar los binds al cerrar

	if (bind(servidor, (void *)&direccionServidor, sizeof(direccionServidor)) != 0){
		perror("Fallo el bind");
		return 1;
	}

	printf("Estoy escuchando\n");
	listen(servidor,100);

	//----------------------------creo cliente/s

	struct sockaddr_in direccionCliente; //direccion donde guarde el cliente
	int sin_size = sizeof(struct sockaddr_in);
	int cliente = accept(servidor, (void *) &direccionCliente, (void *)&sin_size); //acepto el cliente en un descriptor
	if (cliente == -1){
		perror("Fallo el accept");
	}
	printf("Recibi una conexion en %d!!\n", cliente);

	//---------------------------------handshake

	char* bufferHandshake = malloc(20);
	int bytesRecibidos1 = recv(cliente, bufferHandshake, 20, 0);
	bufferHandshake[bytesRecibidos1] = '\0'; //lo paso a string para comparar
			if(strcmp("Hola_soy_una_consola",bufferHandshake) != 0){
				perror("No lo tengo que aceptar");
				return 1; //aca tiene que seguir el programa, borrar la direccion del cliente trucho?
			}else{
				send(cliente, "Hola consola",12,0); //handshake para consola
			}

	//----------------prueba de textos del cliente

	char* buffer = malloc(1000); //donde quiero recibir y cantidad que puedo recibir

	while (1){
		int bytesRecibidos = recv(cliente, buffer, 1000, 0);
		if(bytesRecibidos <= 0){
			perror("se desconecto o algo.");
			return 1;
		}

		buffer[bytesRecibidos] = '\0'; //para pasarlo a string (era un stream)
		printf("me llegaron %d bytes con %s\n", bytesRecibidos, buffer);
	}

	free(buffer);
	close(cliente);
	close(servidor);

	return 0;

}
