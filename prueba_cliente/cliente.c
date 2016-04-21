/*
 * cliente.c
 *
 *  Created on: 21/4/2016
 *      Author: utnso
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PUERTO_SERVIDOR 8080

int main2(void){
	struct sockaddr_in direccServ;
	direccServ.sin_family = AF_INET;
	//direccServ.sin_addr.s_addr = inet_addr("127.0.0.1");INADDR_ANY
	direccServ.sin_addr.s_addr = INADDR_ANY;
	direccServ.sin_port = htons(PUERTO_SERVIDOR);

	int cliente = socket(AF_INET, SOCK_STREAM, 0);
	printf("me cree, estoy en el %d\n",cliente);

	if(connect(cliente, (void*) &direccServ, sizeof(direccServ)) != 0){
		perror("No se pudo conectar");
		return 1;
	}
	//hanshake
	send(cliente, "Hola_soy_una_consola", 20, 0);//si no envio 20 bytes, falla el handshake
	char* bufferHandshake = malloc(12);
	int bytesRecibidos = recv(cliente, bufferHandshake, 12, 0); //se tienen que recibir 12 bytes, estoy probando que pasa con mas
		if(bytesRecibidos <= 0){
			printf("se recibieron %d bytes, no estamos aceptados",bytesRecibidos);
			return 1;
		}else{
			printf("se recibieron %d bytes, estamos aceptados!",bytesRecibidos);
		}

	while(1){
		char mensaje[1000];
		scanf("%s",mensaje);

		send(cliente, mensaje, strlen(mensaje),0);//envia datos por teclado
		//probar si llego lo que se envio, sin serv no tengo que andar
	}

	return 0;
}
