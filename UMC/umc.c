/*
 * umc.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <commons/string.h>
#include <commons/config.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/collections/node.h>
#include <commons/collections/list.h>
#include <pthread.h>

#define esIgual(a,b) string_equals_ignore_case(a,b)
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

//datos a leer por el archivo
#define PUERTO_SWAP 6660
#define PUERTO_UMC 6661
#define MARCOS 100
#define MARCO_SIZE 3

typedef struct{
	char* ip;				//PASAR A IP CON: inet_addr() / o inet_ntoa()
	int puerto, puerto_swap, marcos, marco_size, marco_x_proc, entradas_tlb, retardo;
}datosConfiguracion;

typedef struct{
	int proceso, pagina, marco;
}traductor_marco;




/*	FALTAN CREAR "ESTRUCTURAS" PARA: - INDICE DE CODIGO
 *  							     - INDICE DE ETIQUETAS
 *            					     - INDICE DE STACK
 */

void leerConfiguracion(char*, datosConfiguracion*);
struct sockaddr_in crearDireccion(int);
int conectar(int);
int autentificar(int);
int comprobarCliente(int);
int recibirProtocolo(int);
char* recibirMensaje(int, int);
int procesoActivo(int);
//COMPLETAR...........................................................
void comprobarOperacion(int);
void inicializarPrograma(int PID, int cantPaginas);
void enviarBytes(int pagina, int offset, int tamanio);
void almacenarBytes(int pagina, int offset, int tamanio, int buffer);
void finalizarPrograma(int);
void consola();
void atenderNucleo(int);
void atenderCpu(int);
int hayEspacio(int paginas);
int ponerEnMemoria(char* codigo);

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
	t_list *tabla_de_paginas;
	int mucho = MARCOS*MARCO_SIZE;
	char *memoria;

int main(int argc, char* argv[]) {
	int umc_cliente,
		activado=1,
		nuevo_cliente,														//Recibir conexiones
		sin_size = sizeof(struct sockaddr_in),
		i;
	pthread_attr_t attr;
	pthread_t thread;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

	datosConfiguracion* datosMemoria=malloc(sizeof(datosConfiguracion));
	leerConfiguracion(argv[1], datosMemoria);

	tabla_de_paginas = list_create();
	memoria = malloc(mucho);

	//-------------------------SOCKETS
	struct sockaddr_in direccionUMC = crearDireccion(PUERTO_UMC);//Para el bind
	struct sockaddr_in direccionCliente;			//Donde guardo al cliente
	int umc_servidor = socket(AF_INET, SOCK_STREAM, 0); //creo el descriptor con esa direccion
	printf("UMC Creada. Conectando con la Swap...\n");
	umc_cliente = conectar(PUERTO_SWAP);
	//calloc(datosMemoria.marcos,datosMemoria.marco_size);
	//-----------------------------------SWAP---------------------------------
	if (!autentificar(umc_cliente)) {
		printf("Falló el handshake\n");
		return -1;
	}
	printf("Conexion con la Swap Ok\n");
	setsockopt(umc_servidor, SOL_SOCKET, SO_REUSEADDR, &activado,
			sizeof(activado));			//Para evitar esperas al cerrar socket
	if (bind(umc_servidor, (void *) &direccionUMC, sizeof(direccionUMC)) != 0) {
		perror("Fallo el bind");
		return 1;
	}
	//------------------------------------NUCLEO--------------------------------------------
	printf("Esperando nucleo...\n");
	listen(umc_servidor, 1);
	do {
		nuevo_cliente = accept(umc_servidor, (void *) &direccionCliente,
				(void *) &sin_size);
		if (nuevo_cliente == -1) {
			perror("Fallo el accept");
		}
	} while (comprobarCliente(nuevo_cliente) != 2);												//Espero la conexion del nucleo
	int tamPagEnvio = ntohl(MARCO_SIZE);
	send(nuevo_cliente, &tamPagEnvio, 4, 0);													//Le envio el tamaño de pagina
	printf("Acepte al nucleo\n");
	//-----------------------------Funcionamiento de la UMC--------------------------------------------
	pthread_create(&thread, &attr, (void*) atenderNucleo,(void*) nuevo_cliente);				//Hilo para atender al nucleo
	pthread_create(&thread, &attr, (void*) consola, NULL);										//Hilo para atender comandos
	listen(umc_servidor, 15);																	//Para recibir conexiones (CPU's)

	while (1) {
		nuevo_cliente = accept(umc_servidor, (void *) &direccionCliente,
				(void *) &sin_size);
		if (nuevo_cliente == -1) {
			perror("Fallo el accept");
		}
		printf("Recibi una conexion en %d!!\n", nuevo_cliente);
		switch (comprobarCliente(nuevo_cliente)) {
		case 0:															//Error
			perror("No lo tengo que aceptar, fallo el handshake\n");
			close(nuevo_cliente);
			break;
		case 1:
			send(nuevo_cliente, 1, 4, 0);								//1=CPU
			pthread_create(&thread, &attr, (void*) atenderCpu,(void*) nuevo_cliente);
			break;
		}
	}
	free(datosMemoria);
	return 0;
}

void leerConfiguracion(char *ruta, datosConfiguracion* datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		perror("FIN PROGRAMA");
		exit(0);
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys != 8) {
			perror("ERROR CANTIDAD DATOS DE CONFIGURACION");
		} else {
			datos->puerto=buscarInt(archivoConfiguracion, "PUERTO");
			datos->puerto_swap=buscarInt(archivoConfiguracion, "PUERTO_SWAP");
			datos->marcos=buscarInt(archivoConfiguracion, "MARCOS");
			datos->marco_size=buscarInt(archivoConfiguracion, "MARCO_SIZE");
			datos->marco_x_proc=buscarInt(archivoConfiguracion, "MARCO_X_PROC");
			datos->entradas_tlb=buscarInt(archivoConfiguracion, "ENTRADAS_TLB");
			datos->retardo=buscarInt(archivoConfiguracion, "RETARDO");
			struct sockaddr_in ipLinda;			//recurso TURBIO para guardar la ip :/
			char *direccion;
			inet_aton(config_get_string_value(archivoConfiguracion,"IP"), &ipLinda.sin_addr); //
			direccion = inet_ntoa(ipLinda.sin_addr);
			datos->ip=direccion;
			config_destroy(archivoConfiguracion);
		}
	}
}

struct sockaddr_in crearDireccion(int puerto){
	struct sockaddr_in direccion;
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr = INADDR_ANY;
	direccion.sin_port = htons(puerto);
	return direccion;
}

int conectar(int puerto){   							//Con la swap
	struct sockaddr_in direccion=crearDireccion(puerto);
	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccion, sizeof(direccion)));
	return conexion;
}

int autentificar(int conexion) {
	send(conexion, "soy_la_umc", 10, 0);
	char* bufferHandshakeSwap = malloc(10);
	int bytesRecibidosH = recv(conexion, bufferHandshakeSwap, 10, 0);
	if (bytesRecibidosH <= 0) {
		printf("Error al conectarse con Swap");
		free (bufferHandshakeSwap);
		return 0;
	}
	free (bufferHandshakeSwap);
	return 1;
}

int comprobarCliente(int nuevoCliente) {
	char* bufferHandshake = malloc(15);
	int bytesRecibidosHs = recv(nuevoCliente, bufferHandshake, 15, 0);
	bufferHandshake[bytesRecibidosHs] = '\0'; //lo paso a string para comparar
	if (strcmp("soy_un_cpu", bufferHandshake) == 0) {
		free(bufferHandshake);
		return 1;
	} else if (strcmp("soy_el_nucleo", bufferHandshake) == 0) {
		free(bufferHandshake);
		return 2;
	}
	free(bufferHandshake);
	return 0;
}

int recibirProtocolo(int conexion){
	char* protocolo = malloc(4);
	int bytesRecibidos = recv(conexion, protocolo, sizeof(int32_t), 0);
	if (bytesRecibidos <= 0) {	printf("Error al recibir protocolo\n");
	return 0;
	}
	return atoi(protocolo);}

char* recibirMensaje(int conexion, int tamanio){
	char *mensaje=malloc(tamanio);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, 0);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		return -1;}
	return mensaje;
}

void comprobarOperacion(int codigoOperacion){				//Recibe el 1er byte y lo manda acá. En cada funcion deberá recibir el resto de bytes
	switch(codigoOperacion){
	case 1:							//inicializarPrograma(); 		HACER LOS RECV NECESARIOS!
		break;
	case 2:							//enviarBytes();
		break;
	case 3:							//almacenarBytes();
		break;
	case 4:							//finalizarPrograma();
		break;
	}
}


//-----------------------------------------------OPERACIONES UMC-------------------------------------------------
void inicializarPrograma(int PID, int cantPaginas){


}

void enviarBytes(int pagina, int offset, int tamanio){
	printf("Buscando pag:%d off:%d tam:%d\n",pagina,offset,tamanio);
}

void almacenarBytes(int pagina, int offset, int tamanio, int buffer){
	printf("Almacenar: %d en pag:%d off:%d tam:%d\n",buffer,pagina,offset,tamanio);
}
void finalizarPrograma(int PID){

}
//--------------------------------------------HILOS------------------------
void consola(){
	while (1) {
		char* comando;
		int VELOCIDAD;
		comando = string_new(), scanf("%s", comando);
		if (esIgual(comando, "retardo")) {
			printf("velocidad nueva:");
			scanf("%d", &VELOCIDAD);
			printf("Velocidad actualizada:%d\n", VELOCIDAD);
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
	}
}

int procesoActivo(conexion){
	return recibirProtocolo(conexion);
}

void atenderCpu(int conexion){
	//[PROTOCOLO]: - siempre recibo PRIMERO el ProcesoActivo (PID)
	//			   - despues el codigo de operacion (2 o 3 para CPU)
	//			   - despues se reciben Pag, offset, buffer (Long no xq es el tamaño de la pagina, no es necesario recibirlo)
	printf("CPU atendido\n");
	int salir=0;
	while (!salir) {
		int proceso = procesoActivo(conexion);
		if (proceso) {
			int operacion = recibirProtocolo(conexion);
			if (operacion) {
				int paginas, offset, buffer;
				switch (operacion) {
				case 2:
					paginas = recibirProtocolo(conexion);
					offset = recibirProtocolo(conexion);
					if (paginas && offset) {
						enviarBytes(paginas, offset, MARCO_SIZE);
					} else {
						salir = 1;
					}
					break;
				case 3:
					paginas = recibirProtocolo(conexion);
					offset = recibirProtocolo(conexion);
					buffer = recibirProtocolo(conexion);
					if (paginas && offset && buffer) {
						almacenarBytes(paginas, offset, MARCO_SIZE, buffer);
					} else {
						salir = 1;
					}
					break;
				}
			}else{salir=1;}
		}
		else {salir=1;}
	}
	printf("CPU %d eliminada\n",conexion);
}

void atenderNucleo(int conexion){
	printf("Hilo de Nucleo creado\n");
		//[PROTOCOLO]: - siempre recibo PRIMERO el codigo de operacion (1 o 4) inicializar o finalizar
		int salir=0;
		while (!salir) {
			int operacion = atoi(recibirMensaje(conexion,1));
				if (operacion) {
					int paginas, pid;

					switch (operacion) {
					case 1:												//inicializar programa
							pid = recibirProtocolo(conexion);
							paginas = recibirProtocolo(conexion);
						if(hayEspacio(paginas)){
							send(conexion, '1',1,0);
							int espacio_del_codigo = paginas*MARCO_SIZE;
							char* codigo = recibirMensaje(conexion,espacio_del_codigo);
							if (ponerEnMemoria(codigo)){
								printf("se guardo el codigo");

							}else{printf("no se pudo guardar el codigo en memoria");}

						}else{send(conexion, '0',1,0);}

						break;
					case 3:

						break;
					}
				}else{salir=1;}
		}
		printf("Nucleo en %d termino, eliminado\n",conexion);
}

//--------------------------------FUNCIONES PARA EL NUCLEO----------------------------------
int hayEspacio(int paginas){


	return 1;
}
int ponerEnMemoria(char* codigo){
	int aux_libre=0, aux_sig=0;
	traductor_marco *traductorMarco= malloc(sizeof(traductor_marco));

	//for
	//memcpy(memoria + aux_libre, codigo + aux_sig, MARCO_SIZE);

	aux_sig +=MARCO_SIZE;


	return 1;
}
