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
#define marcosTotal datosMemoria->marco_size*datosMemoria->marcos



typedef struct{
	char *ip, *ip_swap;				//PASAR A IP CON: inet_addr() / o inet_ntoa()
	int puerto_umc, puerto_swap, marcos, marco_size, marco_x_proc, entradas_tlb, retardo;
}datosConfiguracion;

typedef struct{
	int proceso, pagina, marco, enMemoria, modificada;
}traductor_marco;




/*	FALTAN CREAR "ESTRUCTURAS" PARA: - INDICE DE CODIGO
 *  							     - INDICE DE ETIQUETAS
 *            					     - INDICE DE STACK
 */

int leerConfiguracion(char*, datosConfiguracion**);
struct sockaddr_in crearDireccion(int,char*);
int conectar(int,char*);
int autentificar(int);
int comprobarCliente(int);
int recibirProtocolo(int);
char* recibirMensaje(int, int);
int procesoActivo(int);
void mostrarTablaPag(traductor_marco*);
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
int ponerEnMemoria(char* codigo,int id,int cantPags);
int buscarMarcoLibre();
//COMANDOS--------------
void comandoMemory(traductor_marco*);

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_list *tabla_de_paginas;
int totalPaginas, procesoActual;
char* memoria;
datosConfiguracion *datosMemoria;

int main(int argc, char* argv[]) {
	int umc_cliente,
		activado=1,
		nuevo_cliente,														//Recibir conexiones
		sin_size = sizeof(struct sockaddr_in);
	pthread_attr_t attr;
	pthread_t thread;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	datosMemoria=(datosConfiguracion*) malloc(sizeof(datosConfiguracion));
	if (!(leerConfiguracion("ConfigUMC", &datosMemoria) || leerConfiguracion("../ConfigUMC", &datosMemoria))){
		printf("Error archivo de configuracion\n FIN.");return 1;}																//El posta por parametro es: leerConfiguracion(argv[1], &datosMemoria)
	printf("total Marcos: %d\n",marcosTotal);
	tabla_de_paginas = list_create();
	memoria = (char*) malloc(datosMemoria->marco_size*datosMemoria->marcos);

	//----------------------------------------------------------------------------SOCKETS

	struct sockaddr_in direccionUMC = crearDireccion(datosMemoria->puerto_umc,datosMemoria->ip);//Para el bind
	struct sockaddr_in direccionCliente;			//Donde guardo al cliente
	int umc_servidor = socket(AF_INET, SOCK_STREAM, 0); //creo el descriptor con esa direccion
	printf("UMC Creada. Conectando con la Swap...\n");
	umc_cliente = conectar(datosMemoria->puerto_swap, datosMemoria->ip_swap);

	//----------------------------------------------------------------SWAP

	if (!autentificar(umc_cliente)) {
		printf("Falló el handshake\n");
		return -1;}

	printf("Conexion con la Swap Ok\n");
	setsockopt(umc_servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado));			//Para evitar esperas al cerrar socket
	if (bind(umc_servidor, (void *) &direccionUMC, sizeof(direccionUMC))) {
		perror("Fallo el bind");
		return 1;
	}

	//-------------------------------------------------------------------------NUCLEO

	printf("Esperando nucleo...\n");
	listen(umc_servidor, 1);
	do {
		nuevo_cliente = accept(umc_servidor, (void *) &direccionCliente,
				(void *) &sin_size);
		if (nuevo_cliente == -1) {
			perror("Fallo el accept");
		}
	} while (comprobarCliente(nuevo_cliente) != 2);												//Espero la conexion del nucleo
	int tamPagEnvio = ntohl(datosMemoria->marco_size);
	send(nuevo_cliente, &tamPagEnvio, 4, 0);													//Le envio el tamaño de pagina
	printf("Acepte al nucleo\n");

	//-------------------------------------------------------------------Funcionamiento de la UMC

	pthread_create(&thread, &attr, (void*) atenderNucleo,(void*) nuevo_cliente);				//Hilo para atender al nucleo
	pthread_create(&thread, &attr, (void*) consola, NULL);										//Hilo para atender comandos
	listen(umc_servidor, 15);																	//Para recibir conexiones (CPU's)
	int cpuRespuesta=htonl(1);
	while (1) {
		nuevo_cliente = accept(umc_servidor, (void *) &direccionCliente,(void *) &sin_size);
		if (nuevo_cliente == -1) {perror("Fallo el accept");}
		printf("Conexion entrante\n");
		switch (comprobarCliente(nuevo_cliente)) {
		case 0:															//Error
			perror("No lo tengo que aceptar, fallo el handshake\n");
			close(nuevo_cliente);
			break;
		case 1:
			send(nuevo_cliente, &cpuRespuesta, sizeof(int32_t), 0);								//1=CPU
			pthread_create(&thread, &attr, (void*) atenderCpu,(void*) nuevo_cliente);
			break;
		}
	}
	free(datosMemoria);
	free(memoria);
	return 0;
}


//------------------------------------------------------------FUNCIONES
int leerConfiguracion(char *ruta, datosConfiguracion** datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		return 0;
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys < 9) {
			return 0;
		} else {
			(*datos)->puerto_umc=buscarInt(archivoConfiguracion, "PUERTO_UMC");
			(*datos)->puerto_swap=buscarInt(archivoConfiguracion, "PUERTO_SWAP");
			(*datos)->marcos=buscarInt(archivoConfiguracion, "MARCOS");
			(*datos)->marco_size=buscarInt(archivoConfiguracion, "MARCO_SIZE");
			(*datos)->marco_x_proc=buscarInt(archivoConfiguracion, "MARCO_X_PROC");
			(*datos)->entradas_tlb=buscarInt(archivoConfiguracion, "ENTRADAS_TLB");
			(*datos)->retardo=buscarInt(archivoConfiguracion, "RETARDO");
			char* ip=string_new();
			string_append(&ip,config_get_string_value(archivoConfiguracion,"IP"));
			(*datos)->ip=ip;
			char* ipSwap=string_new();
			string_append(&ipSwap,config_get_string_value(archivoConfiguracion,"IP_SWAP"));
			(*datos)->ip_swap=ipSwap;
			config_destroy(archivoConfiguracion);
		}
	}
	return 1;
}

struct sockaddr_in crearDireccion(int puerto,char* ip){
	struct sockaddr_in direccion;
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr =inet_addr(ip);
	direccion.sin_port = htons(puerto);
	return direccion;
}

int conectar(int puerto,char* ip){   							//Con la swap
	struct sockaddr_in direccion=crearDireccion(puerto, ip);
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
	char *mensaje=malloc(tamanio+1);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, 0);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		return "a";}
	mensaje[tamanio+1]='\0';
	return mensaje;
}

void comprobarOperacion(int codigoOperacion){				//Recibe el 1er byte y lo manda acá. En cada funcion deberá recibir el resto de bytes
	switch(codigoOperacion){
	case 1:							//inicializarPrograma(); 		HACER LOS RECV NECESARIOS!
		break;
	case 2:							//enviarBytes();usleep(datosMemoria->retardo*1000))
		break;
	case 3:							//almacenarBytes();usleep(datosMemoria->retardo*1000))
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
						list_iterate(tabla_de_paginas,(void*)comandoMemory);
						printf("Paginas modificadas (proceso: %d)\n",procesoActual);
					}
				}
			}
		}
	}
}

int procesoActivo(conexion){
	return recibirProtocolo(conexion);
}
void comandoMemory(traductor_marco* pagina){
	if (pagina->proceso==procesoActual){
		pagina->modificada=1;
	}
}

void atenderCpu(int conexion){
	//[PROTOCOLO]: - siempre recibo PRIMERO el ProcesoActivo (PID)
	//			   - despues el codigo de operacion (2 o 3 para CPU)
	//			   - despues se reciben Pag, offset, buffer (Long no xq es el tamaño de la pagina, no es necesario recibirlo)
	printf("CPU atendido\n");
	int salir=0;
	while (!salir) {
		procesoActual = procesoActivo(conexion);
		if (procesoActual) {
			int operacion = recibirProtocolo(conexion);
			if (operacion) {
				int paginas, offset, buffer;
				switch (operacion) {
				case 2:
					paginas = recibirProtocolo(conexion);
					offset = recibirProtocolo(conexion);
					if (paginas && offset) {
						enviarBytes(paginas, offset, datosMemoria->marco_size);
					} else {
						salir = 1;
					}
					break;
				case 3:
					paginas = recibirProtocolo(conexion);
					offset = recibirProtocolo(conexion);
					buffer = recibirProtocolo(conexion);
					if (paginas && offset && buffer) {
						almacenarBytes(paginas, offset, datosMemoria->marco_size, buffer);
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
							procesoActual=pid;
							paginas = recibirProtocolo(conexion);
						if(1){							//todo hayEspacio(paginas) es la condicion real
							send(conexion, "1",1,0);
							int espacio_del_codigo = recibirProtocolo(conexion);
							char* codigo =recibirMensaje(conexion,espacio_del_codigo);
							//printf("Codigo: %s-\n",codigo);
							if (ponerEnMemoria(codigo,pid,paginas)){
							//	list_iterate(tabla_de_paginas,mostrarTablaPag);
							//	list_take_and_remove(tabla_de_paginas,5);
							//	list_iterate(tabla_de_paginas,mostrarTablaPag);					PARA PROBAR BUSQUEDA DE MARCOS VACIOS*/
								free(codigo);
							}
						}else{
							printf("1 Ansisop rechazado, memoria insuficiente\n");
							send(conexion, "0",1,0);}
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
	return ((paginas<=datosMemoria->marco_x_proc) && (paginas<=datosMemoria->marcos-list_size(tabla_de_paginas)));
}
int ponerEnMemoria(char* codigo,int proceso,int paginasNecesarias){
	int i=0,acum=0,offset,resto,anterior=-1,a,pos,tamMarco=datosMemoria->marco_size,cantPags;
	do{anterior=acum;
		offset=0;
	for (offset = 0; (codigo[acum] != '\n'); offset++, acum++) {
			printf("%c", codigo[acum]);
		}
		resto=offset%tamMarco;
		cantPags=offset/tamMarco;				//Si esto es 0 => OFFSET=0 => Resto = 0
		if (!offset){resto=1;}
		for(a=0;a<cantPags;a++){
			traductor_marco *traductorMarco =(traductor_marco*) malloc(sizeof(traductor_marco));
			pos = buscarMarcoLibre();
			memcpy(memoria+pos*tamMarco,codigo+anterior,tamMarco);
			anterior+=tamMarco;
			traductorMarco->pagina=i;
			traductorMarco->proceso=proceso;
			traductorMarco->marco=pos;
			list_add(tabla_de_paginas,traductorMarco);
			i++;}
		if (resto || !cantPags){
			traductor_marco *traductorMarco = (traductor_marco*)malloc(sizeof(traductor_marco));
			pos = buscarMarcoLibre();
			memcpy(memoria+pos*tamMarco,codigo+anterior,resto);
			char* espacio=string_repeat('*',tamMarco-resto);
			memcpy(memoria+pos*tamMarco+resto,espacio,tamMarco-resto);
			free(espacio);
			traductorMarco->pagina=i;
			traductorMarco->proceso=proceso;
			traductorMarco->marco=pos;
			i++;
			list_add(tabla_de_paginas,traductorMarco);
		}
		acum++;
		printf("\n");
	}while (i < paginasNecesarias);
	if (acum==string_length(codigo)){printf("Guardado con exito!\n");}

//	printf("Paginas Necesarias:%d , TotalMarcosGuardados: %d\n",paginasNecesarias,i);
//	printf("TablaDePaginas:%d\n",list_size(tabla_de_paginas));
	return 1;
}

void mostrarTablaPag(traductor_marco* fila){
	printf("Marco: %d, Pag: %d, Proc:%d\n",fila->marco,fila->pagina,fila->proceso);
	char* asd=malloc(datosMemoria->marco_size+1);
	memcpy(asd,memoria+datosMemoria->marco_size*fila->marco,datosMemoria->marco_size);
	memcpy(asd+datosMemoria->marco_size+1,"\0",1);
	printf("%s\n",asd);
}

int buscarMarcoLibre() {
	int posicion, encontrado = 0,j;
	traductor_marco* unaFila;
	for (posicion = 0 ; posicion <list_size(tabla_de_paginas); posicion++) {				//Si encuentra el n° marco => encontrado =0; i++
		j=0;
		encontrado = 0;
		do {
			unaFila = list_get(tabla_de_paginas, j);
			if (unaFila->marco == posicion) {														//Si la posicion ya está => encontrado=0 y salgo del while mas rapido
				encontrado = 1;}
			j++;
		} while (!encontrado && j < list_size(tabla_de_paginas));
		if (!encontrado){return posicion;}
	}																						//Va a salir cuando haya recorrido TODA la lista, o cuando haya encontrado un lugar vacio
	return posicion;
}
