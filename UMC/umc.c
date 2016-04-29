/*
 * umc.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <commons/string.h>
#include <commons/config.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/collections/node.h>
#include <commons/collections/list.h>

#define PUERTO_SWAP 6660
#define PUERTO_UMC 6661
#define esIgual(a,b) string_equals_ignore_case(a,b)
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

typedef struct{
	int puerto;
	char* ip;				//PASAR A IP CON: inet_addr() / o inet_ntoa()
	int puerto_swap;
	int marcos;
	int marco_size;
	int marco_x_proc;
	int entradas_tlb;
	int retardo;
}datosConfiguracion;

void leerConfiguracion(char*, datosConfiguracion*);


int main(int argc, char* argv[]) { //SOCKETS, CONEXION, BLA...
	//lector de comandos
	datosConfiguracion datosMemoria;
	char* comando;
	int velocidad;
//	leerConfiguracion(argv[1], &datosMemoria);
//	printf("Puerto: %d\n",datosMemoria.puerto);
	//socket
	struct sockaddr_in direccionSwap; //creo la direccion cliente y servidor
	direccionSwap.sin_family = AF_INET;
	direccionSwap.sin_addr.s_addr = INADDR_ANY;
	direccionSwap.sin_port = htons(PUERTO_SWAP);
	struct sockaddr_in direccionUMC;
	direccionUMC.sin_family = AF_INET;
	direccionUMC.sin_addr.s_addr = INADDR_ANY;
	direccionUMC.sin_port = htons(PUERTO_UMC);

	int umc_servidor = socket(AF_INET, SOCK_STREAM, 0); //creo el descriptor con esa direccion
	int umc_cliente = socket(AF_INET, SOCK_STREAM, 0);
	int cliente_nucleo; //socket del nucleo, para el accept
	printf("se creo la umc servidor: %d y cliente: %d\n",umc_servidor,umc_cliente);

	//primero me conecto a la swap
		if (connect(umc_cliente, (void*) &direccionSwap, sizeof(direccionSwap)) != 0) {
				perror("No se pudo conectar con la swap");
				return 1;
			}
			//hanshake para SWAP
			send(umc_cliente, "soy_la_umc", 10, 0);
			char* bufferHandshakeSwap = malloc(10);
			int bytesRecibidosH = recv(umc_cliente, bufferHandshakeSwap, 10, 0);
			if (bytesRecibidosH <= 0) {
				printf("se recibieron %d bytes, no se pudo conectar con la swap\n", bytesRecibidosH);
				return 1;
			} else {
				printf("se recibieron %d bytes, estamos conectados con la swap!\n", bytesRecibidosH);
			}
	//despues bindeo la umc y la pongo a escuchar
		int activado = 1;
		setsockopt(umc_servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado)); //para cerrar los binds al cerrar
		if (bind(umc_servidor, (void *)&direccionUMC, sizeof(direccionUMC)) != 0){
			perror("Fallo el bind");
			return 1;
		}
		printf("Estoy escuchando\n");
		listen(umc_servidor,15);

	//ahora espero al Nucleo
		struct sockaddr_in direccionCliente; //direccion donde guarde el cliente
		int sin_size = sizeof(struct sockaddr_in);
		int seConecto=1;
		while(seConecto){
			cliente_nucleo = accept(umc_servidor, (void *) &direccionCliente, (void *)&sin_size); //acepto el cliente en un descriptor
			if (cliente_nucleo == -1){
				perror("Fallo el accept");
			}
			printf("Recibi una conexion en %d!!\n", cliente_nucleo);
		//handshake
			char* bufferHNucleo = malloc(20);
				int bytesRecibidosN = recv(cliente_nucleo, bufferHNucleo, 20, 0);
				bufferHNucleo[bytesRecibidosN] = '\0'; //lo paso a string para comparar
					if(strcmp("soy_el_nucleo",bufferHNucleo) != 0){
						perror("No lo tengo que aceptar, no es el nucleo");
					}else{
						send(cliente_nucleo, "Hola nucleo",11,0); //handshake para nucleo
						seConecto = 0;
					}
		}
	//ahora creo el select de cpus
		fd_set descriptores;
		int nuevo_cliente;
		t_list* cpus;
		cpus = list_create();
		int max_desc = cliente_nucleo;
		int i;

	while(1){

		FD_ZERO (&descriptores);
			FD_SET(umc_servidor,&descriptores);
			FD_SET(umc_cliente,&descriptores);
			FD_SET(cliente_nucleo,&descriptores);
			max_desc = cliente_nucleo;
				for(i=0; i<list_size(cpus);i++){
					int cpuset = list_get(cpus,i);
					FD_SET(cpuset,&descriptores);
					if(cpuset > max_desc){ max_desc = cpuset; }
				}

		if (select (max_desc+1, &descriptores, NULL, NULL, NULL) < 0){
			 	perror ("Error en el select");
			    exit (EXIT_FAILURE);
		}

		for(i=0; i<list_size(cpus);i++){
			//ver los clientes que recibieron informacion
			int unCPU = list_get(cpus,i);
			if(FD_ISSET(unCPU , &descriptores)){
					printf("se activo el cpu %d\n",unCPU);
					int protocoloCPU=0; //donde quiero recibir y cantidad que puedo recibir
					int bytesRecibidosCpu = recv(unCPU, &protocoloCPU, sizeof(int32_t), 0);
					protocoloCPU=ntohl(protocoloCPU);
						if(bytesRecibidosCpu <= 0){
							perror("el cpu se desconecto o algo. Se lo elimino\n");
							list_remove(cpus, i);
					} else {
							char* bufferCpu = malloc(protocoloCPU * sizeof(char) + 1);
							bytesRecibidosCpu = recv(unCPU, bufferCpu, protocoloCPU, 0);
							bufferCpu[protocoloCPU + 1] = '\0'; //para pasarlo a string (era un stream)
							printf("cliente: %d, me llegaron %d bytes con %s\n", unCPU,bytesRecibidosCpu, bufferCpu);
							free(bufferCpu);
					}
			 }
		  }
		if (FD_ISSET(cliente_nucleo,&descriptores)){
			//se activo el Nucleo, me esta mandando algo
		}
		if (FD_ISSET(umc_cliente,&descriptores)){
			//se activo la swap, me esta mandando algo
		}
		if(FD_ISSET(umc_servidor,&descriptores)){ //aceptar cliente

			nuevo_cliente = accept(umc_servidor, (void *) &direccionCliente, (void *)&sin_size);
				if (nuevo_cliente == -1){
					perror("Fallo el accept");
				}
				printf("Recibi una conexion en %d!!\n", nuevo_cliente);

				char* bufferHandshake = malloc(15);
				int bytesRecibidosHs = recv(nuevo_cliente, bufferHandshake, 15, 0);
				bufferHandshake[bytesRecibidosHs] = '\0'; //lo paso a string para comparar
						if (strcmp("soy_un_cpu",bufferHandshake) == 0){
							send(nuevo_cliente, "Hola_cpu",12,0);
							list_add(cpus, (void *)nuevo_cliente);
							printf("acepte un nuevo cpu\n");
						}else{
							perror("No lo tengo que aceptar, fallo el handshake\n");
							close(nuevo_cliente);
						}
		}
	}
	/*-------------------corto lector de comandos para probar sockets, se deberan usar en hilos diferentes
	while (1) {
		comando = string_new(), scanf("%s", comando);
		if (esIgual(comando, "retardo")) {
			printf("velocidad nueva:");
			scanf("%d", &velocidad);
			printf("Velocidad actualizada:%d\n", velocidad);
		} else {
			if (esIgual(comando, "dump")) {
				printf("Estructuras de Memoria\n");
				printf("Datos de Memoria\n");
			} else {
				if (esIgual(comando, "tlb")) {
					printf("TLB Borrada :)\n");
				} else {
					if (esIgual(comando, "memoria")) {
						printf("Memoria Limpiada :)\n");
					}
				}
			}
		}
	}*/


	return 0;
}

void leerConfiguracion(char *ruta, datosConfiguracion *datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		perror("FIN PROGRAMA");
		exit(0);
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys != 8) {
			perror("ERROR CANTIDAD DATOS DE CONFIGURACION");
		} else {
			datos->puerto = buscarInt(archivoConfiguracion, "PUERTO");
			datos->puerto_swap = buscarInt(archivoConfiguracion, "PUERTO_SWAP");
			datos->marcos = buscarInt(archivoConfiguracion, "MARCOS");
			datos->marco_size = buscarInt(archivoConfiguracion, "MARCO_SIZE");
			datos->marco_x_proc = buscarInt(archivoConfiguracion, "MARCO_X_PROC");
			datos->entradas_tlb = buscarInt(archivoConfiguracion, "ENTRADAS_TLB");
			datos->retardo = buscarInt(archivoConfiguracion, "RETARDO");
			struct sockaddr_in ipLinda;			//recurso TURBIO para guardar la ip :/
			char *direccion;
			inet_aton(config_get_string_value(archivoConfiguracion,"IP"), &ipLinda.sin_addr); // store IP in antelope
			direccion = inet_ntoa(ipLinda.sin_addr); // return the IP
			datos->ip=direccion;
			config_destroy(archivoConfiguracion);
		}
	}
}

