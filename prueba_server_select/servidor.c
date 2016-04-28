/*
 * servidor.c
 *
 *  Created on: 26/4/2016
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
#include<commons/collections/node.h>
#include<commons/collections/list.h>

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
	listen(servidor,15);

	//-------------------select
	fd_set descriptores;
	int nuevo_cliente;
	t_list* clientes;
	clientes = list_create();
	int max_desc = servidor;
	struct sockaddr_in direccionCliente; //direccion donde guarde el cliente
	int sin_size = sizeof(struct sockaddr_in);
	int i;

	while(1){

		FD_ZERO (&descriptores);
		FD_SET(servidor,&descriptores);	//agrego el server primero
		max_desc = servidor;
			for(i=0; i<list_size(clientes);i++){ //despues los clientes nuevos y ya conectados
				int cli = list_get(clientes,i);
				FD_SET(cli,&descriptores);
				if(cli > max_desc){ max_desc = cli; }
			}


		if (select (max_desc+1, &descriptores, NULL, NULL, NULL) < 0){
		 	perror ("Error en el select");
		    exit (EXIT_FAILURE);
		}

		for(i=0; i<list_size(clientes);i++){
			//ver los clientes que recibieron informacion
			int cli = list_get(clientes,i);
			if(FD_ISSET(cli , &descriptores)){
					printf("se activo el cliente %d\n",cli);
					int protocolo=0; //donde quiero recibir y cantidad que puedo recibir
					int bytesRecibidos = recv(cli, &protocolo, sizeof(int32_t), 0);
					protocolo=ntohl(protocolo);
						if(bytesRecibidos <= 0){
							perror("el cliente se desconecto o algo. Se lo elimino\n");
							list_remove(clientes, i); //todo no entendi de la funcion de commons: list_remove_and_destroy_element, el parametro: void(*element_destroyer)(void*)
				} else {
					char* buffer = malloc(protocolo * sizeof(char) + 1);
					bytesRecibidos = recv(cli, buffer, protocolo, 0);
					buffer[protocolo + 1] = '\0';
					; //para pasarlo a string (era un stream)
					printf("cliente: %d, me llegaron %d bytes con %s\n", cli,
					bytesRecibidos, buffer);
					free(buffer);
						}
			}
		}

		if(FD_ISSET(servidor,&descriptores)){ //aceptar cliente

					nuevo_cliente = accept(servidor, (void *) &direccionCliente, (void *)&sin_size); //acepto el cliente en un descriptor
						if (nuevo_cliente == -1){
							perror("Fallo el accept");
						}
						printf("Recibi una conexion en %d!!\n", nuevo_cliente);

					char* bufferHandshake = malloc(20);
						int bytesRecibidos1 = recv(nuevo_cliente, bufferHandshake, 20, 0);
						bufferHandshake[bytesRecibidos1] = '\0'; //lo paso a string para comparar
								if(strcmp("Hola_soy_una_consola",bufferHandshake) != 0){
									perror("No lo tengo que aceptar, fallo el handshake\n");
									close(nuevo_cliente);
								}else{
									send(nuevo_cliente, "Hola consola",12,0); //handshake para consola
									list_add(clientes, (void *)nuevo_cliente);
									printf("y lo acepte\n");
									//aca iria un close y un free mejor?
								}
				}

	}

	return 0;

}
