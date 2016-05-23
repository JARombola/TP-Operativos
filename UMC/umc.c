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
#include <commons/bitarray.h>
#include <commons/collections/node.h>
#include <commons/collections/list.h>
#include <pthread.h>
#include <unistd.h>
#include "Funciones/Comunicacion.h"
#include "Funciones/ArchivosLogs.h"

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


int leerConfiguracion(char*, datosConfiguracion**);
int autentificar(int);
int comprobarCliente(int);
void mostrarTablaPag(traductor_marco*);
int aceptarNucleo(int,struct sockaddr_in);
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
int totalPaginas,procesoBuscar=0,conexionSwap;
char* memoria;
datosConfiguracion *datosMemoria;
t_bitarray 	*espacio;
t_log* archivoLog;


int main(int argc, char* argv[]) {
	archivoLog = log_create("UMC.log", "UMC", true, log_level_from_string("INFO"));

	int nucleo,nuevo_cliente,sin_size = sizeof(struct sockaddr_in);
	tabla_de_paginas = list_create();
	pthread_attr_t attr;
	pthread_t thread;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

	datosMemoria=(datosConfiguracion*) malloc(sizeof(datosConfiguracion));
	if (!(leerConfiguracion("ConfigUMC", &datosMemoria) || leerConfiguracion("../ConfigUMC", &datosMemoria))){
		registrarError(archivoLog,"No se pudo leer archivo de Configuracion");return 1;}																//El posta por parametro es: leerConfiguracion(argv[1], &datosMemoria)

	memoria = (char*) malloc(marcosTotal);
	espacio=bitarray_create(memoria,marcosTotal);
	int i;
	for(i=0;i<marcosTotal;i++,bitarray_clean_bit(espacio,i)){
	//	printf("%d - Cant %d - ",i, bitarray_test_bit(espacio,i));
		//bitarray_set_bit(espacio,i);
	//	printf("%d\n",bitarray_test_bit(espacio,i));
	}


	//----------------------------------------------------------------------------SOCKETS

	struct sockaddr_in direccionUMC = crearDireccion(datosMemoria->puerto_umc,datosMemoria->ip);
	struct sockaddr_in direccionCliente;
	int umc_servidor = socket(AF_INET, SOCK_STREAM, 0);

	registrarInfo(archivoLog,"UMC Creada. Conectando con la Swap...");
	conexionSwap = conectar(datosMemoria->puerto_swap, datosMemoria->ip_swap);

	//----------------------------------------------------------------SWAP

	if (!autentificar(conexionSwap)) {
		registrarError(archivoLog,"Falló el handShake");
		return -1;}

	registrarInfo(archivoLog,"Conexion con la Swap OK!");

	if (!bindear(umc_servidor, direccionUMC)) {
		registrarError(archivoLog,"Error en el bind, desaprobamos");
			return 1;
		}

	//-------------------------------------------------------------------------NUCLEO

	registrarTrace(archivoLog,"Esperando nucleo...");
	listen(umc_servidor, 1);
	nucleo=aceptarNucleo(umc_servidor,direccionCliente);


	//-------------------------------------------------------------------Funcionamiento de la UMC

	pthread_create(&thread, &attr, (void*) atenderNucleo,(void*) nucleo);				//Hilo para atender al nucleo
	pthread_create(&thread, &attr, (void*) consola, NULL);										//Hilo para atender comandos
	listen(umc_servidor, 15);																	//Para recibir conexiones (CPU's)
	int cpuRespuesta=htonl(1);

	while (1) {
		nuevo_cliente = accept(umc_servidor, (void *) &direccionCliente,(void *) &sin_size);
		if (nuevo_cliente == -1) {perror("Fallo el accept");}
		registrarTrace(archivoLog,"Conexion entrante");
		switch (comprobarCliente(nuevo_cliente)) {
		case 0:															//Error
			perror("No lo tengo que aceptar, fallo el handshake");
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

int autentificar(int conexion) {
	send(conexion, "soy_la_umc", 10, 0);
	char* bufferHandshakeSwap = malloc(8);
	int bytesRecibidosH = recv(conexion, bufferHandshakeSwap, 9, 0);
	if (bytesRecibidosH <= 0) {
		registrarError(archivoLog,"Error al conectarse con la Swap");
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

int aceptarNucleo(int umc,struct sockaddr_in direccionCliente){
	int nuevo_cliente;
	int tam=sizeof(struct sockaddr_in);
	do {
		nuevo_cliente = accept(umc, (void *) &direccionCliente,(void *) &tam);

	} while (comprobarCliente(nuevo_cliente) != 2);												//Espero la conexion del nucleo
	int tamPagEnvio = ntohl(datosMemoria->marco_size);
	send(nuevo_cliente, &tamPagEnvio, 4, 0);													//Le envio el tamaño de pagina
	registrarInfo(archivoLog,"Nucleo aceptado!");
	return nuevo_cliente;
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
			char* mensaje="Velocidad actualizada";
			string_append(&mensaje,(char*)VELOCIDAD);
			registrarInfo(archivoLog,mensaje);

		} else {
			if (esIgual(comando, "dump")) {
				printf("Estructuras de Memoria\n");
				printf("Datos de Memoria\n");
			} else {
				if (esIgual(comando, "tlb")) {
					printf("TLB Borrada :)\n");
				} else {
					if (esIgual(comando, "memoria")) {
						printf("Proceso?");
						scanf("%d",&procesoBuscar);//todo
						list_iterate(tabla_de_paginas,(void*)comandoMemory);
						printf("Paginas modificadas (proceso: %d)\n",procesoBuscar);
					}
				}
			}
		}
	}
}

void comandoMemory(traductor_marco* pagina){
	if(pagina->proceso==procesoBuscar)pagina->modificada=1;
}

void atenderCpu(int conexion){
	//[PROTOCOLO]: - siempre recibo PRIMERO el ProcesoActivo (PID)
	//			   - despues el codigo de operacion (2 o 3 para CPU)
	//			   - despues se reciben Pag, offset, buffer (Long no xq es el tamaño de la pagina, no es necesario recibirlo)
	registrarTrace(archivoLog,"Nuevo CPU-");
	int salir=0;
	int procesoActual;
	while (!salir) {
		procesoActual = recibirProtocolo(conexion);
		if (procesoActual) {
			int operacion = recibirProtocolo(conexion);
			if (operacion) {
				int paginas, offset, buffer;
				switch (operacion) {
				case 2:
					paginas = recibirProtocolo(conexion);
					offset = recibirProtocolo(conexion);
					if (paginas && offset) {
						enviarBytes(paginas, offset, datosMemoria->marco_size);					//todo
					} else {
						salir = 1;
					}
					break;
				case 3:
					paginas = recibirProtocolo(conexion);
					offset = recibirProtocolo(conexion);
					buffer = recibirProtocolo(conexion);
					if (paginas && offset && buffer) {
						almacenarBytes(paginas, offset, datosMemoria->marco_size, buffer);			//todo
					} else {
						salir = 1;
					}
					break;
				}
			}else{salir=1;}
		}
		else {salir=1;}
	}
	registrarTrace(archivoLog,"CPU eliminada");
}

void atenderNucleo(int conexion){
	registrarTrace(archivoLog,"Hilo de Nucleo creado");
		//[PROTOCOLO]: - siempre recibo PRIMERO el codigo de operacion (1 o 4) inicializar o finalizar
		int salir=0;
		while (!salir) {
			int operacion = atoi(recibirMensaje(conexion,1));
				if (operacion) {
					int cantPaginas, PID;
					switch (operacion) {
					case 1:												//inicializar programa
							PID= recibirProtocolo(conexion);
							cantPaginas = recibirProtocolo(conexion);
						if(hayEspacio(cantPaginas)){									//Si tiene espacio la UMC lo guarda ella
							send(conexion, "1",1,0);
							int espacio_del_codigo = recibirProtocolo(conexion);
							char* codigo =recibirMensaje(conexion,espacio_del_codigo);
							//printf("Codigo: %s-\n",codigo);
						//	if (ponerEnMemoria(codigo,PID,cantPaginas)){
					//			list_iterate(tabla_de_paginas,(void*)mostrarTablaPag);}
							//	list_take_and_remove(tabla_de_paginas,5);
							//	list_iterate(tabla_de_paginas,mostrarTablaPag);					PARA PROBAR BUSQUEDA DE MARCOS VACIOS*/
						//	}
								free(codigo);}
						else{														//Si no tiene espacio, consulta a la Swap
							char* mensajeInicial=string_new();
							string_append(&mensajeInicial, "1");
							string_append(&mensajeInicial,header(PID));
							string_append(&mensajeInicial, header(cantPaginas));
							string_append(&mensajeInicial, "\0");
							send(conexionSwap,mensajeInicial,string_length(mensajeInicial),0);
							recv(conexionSwap,mensajeInicial,2,0);
							mensajeInicial[2]='\0';
							if(string_equals_ignore_case(mensajeInicial,"ok")){				//Si la swap tiene espacio, recibo codigo y se lo mando
								free(mensajeInicial);
								send(conexion, "1",1,0);
								int espacio_del_codigo = recibirProtocolo(conexion);
								char* codigo =recibirMensaje(conexion,espacio_del_codigo);
								agregarHeader(&codigo);
								send(conexionSwap,codigo,string_length(codigo),0);
								registrarDebug(archivoLog,"1 Ansisop enviado a la Swap");
							}
						else{														//Si ninguna tiene espacio => rechaza
							registrarWarning(archivoLog,"Ansisop rechazado, memoria insuficiente");
							send(conexion, "0",1,0);}}
						break;
					case 3:

						break;
					}
				}else{salir=1;}
		}
		registrarWarning(archivoLog,"Se desconectó el Nucleo");
}

//--------------------------------FUNCIONES PARA EL NUCLEO----------------------------------
int hayEspacio(int paginas){
	return ((paginas<=datosMemoria->marco_x_proc) && (paginas<=datosMemoria->marcos-list_size(tabla_de_paginas)));
}
int ponerEnMemoria(char* codigo,int proceso,int paginasNecesarias){
	int i=0,pos,tamMarco=datosMemoria->marco_size;
	do{		traductor_marco* traductorMarco=malloc(sizeof(traductorMarco));
			pos=buscarMarcoLibre();
			memcpy(memoria+pos*tamMarco,codigo+i*tamMarco,tamMarco);
			traductorMarco->pagina=i;
			traductorMarco->proceso=proceso;
			traductorMarco->marco=pos;
			list_add(tabla_de_paginas,traductorMarco);
			i++;
	}while (i < paginasNecesarias);
	printf("Bites ocupados: %d\n",bitarray_get_max_bit(espacio));
	registrarInfo(archivoLog,"Ansisop guardado con exito!");
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
	int pos,a=1;
	for (pos = 0 ; (pos<=datosMemoria->marcos) && a ;pos++){
	//	printf("%d-",bitarray_test_bit(espacio,pos));
		if (!bitarray_test_bit(espacio,pos)){a=0;}
	bitarray_set_bit(espacio,pos);}
	//printf("-\n",pos);
	return (pos);
}
