/*
 * nucleo.c
 *
 *  Created on: 28/4/2016
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

#define PUERTO_UMC 6661
#define PUERTO_NUCLEO 6662

int asd(void){

	struct sockaddr_in direccionNucleo; //creo la direccion cliente y servidor
	direccionNucleo.sin_family = AF_INET;
	direccionNucleo.sin_addr.s_addr = INADDR_ANY;
	direccionNucleo.sin_port = htons(PUERTO_NUCLEO);
	struct sockaddr_in direccionUMC;
	direccionUMC.sin_family = AF_INET;
	direccionUMC.sin_addr.s_addr = INADDR_ANY;
	direccionUMC.sin_port = htons(PUERTO_UMC);

	int nucleo_servidor = socket(AF_INET, SOCK_STREAM, 0); //creo el descriptor con esa direccion
	int nucleo_cliente = socket(AF_INET, SOCK_STREAM, 0);
	printf("se creo el nucleo servidor: %d y cliente: %d\n",nucleo_servidor,nucleo_cliente);

	//primero me conecto a la UMC
	if (connect(nucleo_cliente, (void*) &direccionUMC, sizeof(direccionUMC)) != 0) {
			perror("No se pudo conectar con la UMC");
			return 1;
		}
		//hanshake para UMC
		send(nucleo_cliente, "soy_el_nucleo", 13, 0);
		char* bufferHandshake = malloc(12);
		int bytesRecibidos = recv(nucleo_cliente, bufferHandshake, 12, 0);
		if (bytesRecibidos <= 0) {
			printf("se recibieron %d bytes, no estamos aceptados\n", bytesRecibidos);
			return 1;
		} else {
			printf("se recibieron %d bytes, estamos conectados con la UMC!\n", bytesRecibidos);
		}

	//despues bindeo el nucleo y lo pongo a escuchar
	int activado = 1;
	setsockopt(nucleo_servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado)); //para cerrar los binds al cerrar
	if (bind(nucleo_servidor, (void *)&direccionNucleo, sizeof(direccionNucleo)) != 0){
		perror("Fallo el bind");
		return 1;
	}
	printf("Estoy escuchando\n");
	listen(nucleo_servidor,15);

	//ahora creo el select
	fd_set descriptores;
	int nuevo_cliente;
	t_list* cpus;
	t_list* consolas;
	cpus = list_create();
	consolas = list_create();
	int max_desc = nucleo_cliente;
	struct sockaddr_in direccionCliente; //direccion donde guarde el cliente
	int sin_size = sizeof(struct sockaddr_in);
	int i;

	while(1){

		FD_ZERO (&descriptores);
		FD_SET(nucleo_servidor,&descriptores);
		FD_SET(nucleo_cliente,&descriptores);
		max_desc = nucleo_cliente;
			for(i=0; i<list_size(consolas);i++){
				int conset = list_get(consolas,i); //conset = consola para setear
				FD_SET(conset,&descriptores);
				if(conset > max_desc){ max_desc = conset; }
			}
			for(i=0; i<list_size(cpus);i++){
					int cpuset = list_get(cpus,i);
					FD_SET(cpuset,&descriptores);
					if(cpuset > max_desc){ max_desc = cpuset; }
				}

		if (select (max_desc+1, &descriptores, NULL, NULL, NULL) < 0){
		 	perror ("Error en el select");
		    exit (EXIT_FAILURE);
		}

		for(i=0; i<list_size(consolas);i++){
			//entro si una consola me mando algo
			int unaConsola = list_get(consolas,i);
			if(FD_ISSET(unaConsola , &descriptores)){
					printf("se activo la consola %d\n",unaConsola);
					int protocoloC=0; //donde quiero recibir y cantidad que puedo recibir
					int bytesRecibidosC = recv(unaConsola, &protocoloC, sizeof(int32_t), 0);
					protocoloC=ntohl(protocoloC);
						if(bytesRecibidosC <= 0){
							perror("el cliente se desconecto o algo. Se lo elimino\n");
							list_remove(consolas, i);
						} else {
							char* buffer = malloc(protocoloC * sizeof(char) + 1);
							bytesRecibidosC = recv(unaConsola, buffer, protocoloC, 0);
							buffer[protocoloC + 1] = '\0'; //para pasarlo a string (era un stream)
							printf("cliente: %d, me llegaron %d bytes con %s\n", unaConsola,bytesRecibidos, buffer);
							free(buffer);
						}
			}
		 }
		for(i=0; i<list_size(cpus);i++){
			//ver los clientes que recibieron informacion
			int unCPU = list_get(cpus,i);
			if(FD_ISSET(unCPU , &descriptores)){
					printf("se activo el cliente %d\n",unCPU);
					int protocoloCPU=0; //donde quiero recibir y cantidad que puedo recibir
					int bytesRecibidos = recv(unCPU, &protocoloCPU, sizeof(int32_t), 0);
					protocoloCPU=ntohl(protocoloCPU);
						if(bytesRecibidos <= 0){
							perror("el cliente se desconecto o algo. Se lo elimino\n");
							list_remove(cpus, i); //todo no entendi de la funcion de commons: list_remove_and_destroy_element, el parametro: void(*element_destroyer)(void*)
					} else {
							char* buffer = malloc(protocoloCPU * sizeof(char) + 1);
							bytesRecibidos = recv(unCPU, buffer, protocoloCPU, 0);
							buffer[protocoloCPU + 1] = '\0'; //para pasarlo a string (era un stream)
							printf("cliente: %d, me llegaron %d bytes con %s\n", unCPU,bytesRecibidos, buffer);
							free(buffer);
					}
			 }
		  }


		if (FD_ISSET(nucleo_cliente,&descriptores)){
			//se activo la UMC, me esta mandando algo
		}


		if(FD_ISSET(nucleo_servidor,&descriptores)){ //aceptar cliente

			nuevo_cliente = accept(nucleo_servidor, (void *) &direccionCliente, (void *)&sin_size);
				if (nuevo_cliente == -1){
					perror("Fallo el accept");
				}
				printf("Recibi una conexion en %d!!\n", nuevo_cliente);

				char* bufferHandshake = malloc(15);
				int bytesRecibidosH = recv(nuevo_cliente, bufferHandshake, 15, 0);
				bufferHandshake[bytesRecibidosH] = '\0'; //lo paso a string para comparar
						if (strcmp("soy_un_cpu",bufferHandshake) == 0){
							send(nuevo_cliente, "Hola_cpu",12,0);
							list_add(cpus, (void *)nuevo_cliente);
							printf("acepte un nuevo cpu\n");
						}else if(strcmp("soy_una_consola",bufferHandshake) == 0){
							send(nuevo_cliente, "Hola consola",12,0); //handshake para consola
							list_add(consolas, (void *)nuevo_cliente);
							printf("acepte una nueva consola\n");
							//aca iria un close y un free mejor?
						}else{
							perror("No lo tengo que aceptar, fallo el handshake\n");
							close(nuevo_cliente);
						}
		}

	}

	return 0;

}
