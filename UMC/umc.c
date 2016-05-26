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
	int puerto_umc, puerto_swap, marcos, marco_size, marco_x_proc, entradas_tlb, retardo, algoritmo;
}datosConfiguracion;

typedef struct{
	int proceso, pagina, marco, enMemoria, modificada;
}traductor_marco;

typedef struct{
	int proceso, clock;
}unClock;


int leerConfiguracion(char*, datosConfiguracion**);
int autentificar(int);
int comprobarCliente(int);
void mostrarTablaPag(traductor_marco*);
int aceptarNucleo(int,struct sockaddr_in);
//COMPLETAR...........................................................
void comprobarOperacion(int);
void enviarBytes(int pagina, int offset, int tamanio);
void almacenarBytes(int pagina, int offset, int tamanio, int buffer);
void finalizarPrograma(int);
void consola();
void atenderNucleo(int);
void atenderCpu(int);
int ponerEnMemoria(char* codigo,int id,int cantPags);
int buscarMarcoLibre(int);
int inicializarPrograma(int);					// a traves del socket recibe el PID + Cant de Paginas + Codigo
int esperarRespuestaSwap();
//-----MENSAJES----
traductor_marco* buscar(int, int, int, int);
int marcosAsignados(int pid, int operacion);

//COMANDOS--------------

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_list *tabla_de_paginas,*tablaClocks;
int totalPaginas,conexionSwap, *vectorPaginas;
void* memoria;
datosConfiguracion *datosMemoria;
t_log* archivoLog;


int main(int argc, char* argv[]) {
	archivoLog = log_create("UMC.log", "UMC", true, log_level_from_string("INFO"));

	int nucleo,nuevo_cliente,sin_size = sizeof(struct sockaddr_in);
	tabla_de_paginas = list_create();
	tablaClocks=list_create();
	pthread_attr_t attr;
	pthread_t thread;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

	datosMemoria=(datosConfiguracion*) malloc(sizeof(datosConfiguracion));
	if (!(leerConfiguracion("ConfigUMC", &datosMemoria) || leerConfiguracion("../ConfigUMC", &datosMemoria))){
		registrarError(archivoLog,"No se pudo leer archivo de Configuracion");return 1;}																//El posta por parametro es: leerConfiguracion(argv[1], &datosMemoria)
	datosMemoria->algoritmo=1;														//todo CAMBIAR ALGORITMO
	vectorPaginas=(int*) malloc(datosMemoria->marcos*4);
	memoria = (void*) malloc(marcosTotal);
	int j;
	for (j=0;j<datosMemoria->marcos;j++){
		vectorPaginas[j]=0;
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
	listen(umc_servidor, 100);																	//Para recibir conexiones (CPU's)
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
	int procesoBuscar;
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
				scanf("%d",&VELOCIDAD);
				traductor_marco* fila=buscar(0,VELOCIDAD,0,15);
				char* mje=malloc(15);
									int pos=fila->marco*datosMemoria->marco_size;
									memcpy(mje,memoria+pos,15);
									printf("%s\n",mje);//todo
				/*printf("Estructuras de Memoria\n");
				printf("Datos de Memoria\n");*/
			} else {
				if (esIgual(comando, "tlb")) {
					printf("TLB Borrada :)\n");
				} else {
					if (esIgual(comando, "memoria")) {
						printf("Proceso?");
						scanf("%d",&procesoBuscar);//todo
						void comandoMemory(traductor_marco* pagina){
							if(pagina->proceso==procesoBuscar)pagina->modificada=1;
						}
						list_iterate(tabla_de_paginas,(void*)comandoMemory);
						printf("Paginas modificadas (proceso: %d)\n",procesoBuscar);
					}
				}
			}
		}
	}
}



void atenderCpu(int conexion){

	//				[PROTOCOLO]:
	//				- 1) Codigo de operacion (2 o 3 para CPU)
	//			   	- 2) ID PROCESO
	//				- 3) despues se reciben Pag, offset, buffer (Long no xq es el tamaño de la pagina, no es necesario recibirlo)

	registrarTrace(archivoLog, "Nuevo CPU-");
	int salir = 0, operacion, proceso, pagina, offset, buffer, size;
	traductor_marco* fila;
	while (!salir) {
		operacion = atoi(recibirMensaje(conexion, 1));
		if (operacion) {
			proceso = atoi(recibirMensaje(conexion,1));
			pagina = recibirProtocolo(conexion);
			offset = recibirProtocolo(conexion);
			size=recibirProtocolo(conexion);
			switch (operacion) {
			case 2:													//2 = Enviar Bytes (busco pag, y devuelvo el valor)
				fila=buscar(proceso, pagina,offset,size);
				if (fila->pagina!=-1){
					char* mje=malloc(size);
					int pos=fila->marco*datosMemoria->marco_size;
					memcpy(mje,memoria+pos,size);
					send(conexion,mje,size,0);
				}
				else{							//No existe la pagina
					printf("Pagina incorrecta\n");
				}
				break;
			case 3:													//3 = Guardar Valor
				recv(conexion,&buffer,sizeof(int),0);
				//guardar(proceso,pagina,offset,size,buffer);				//todo
				break;
			}
		} else {
			salir = 1;
		}
	}
	registrarTrace(archivoLog, "CPU eliminada");
}

void atenderNucleo(int conexion){
	registrarInfo(archivoLog,"Hilo de Nucleo creado");
		//[PROTOCOLO]: - siempre recibo PRIMERO el codigo de operacion (1 o 4) inicializar o finalizar
		int salir=0;
		while (!salir) {
			int operacion = atoi(recibirMensaje(conexion,1));
				if (operacion) {
					switch (operacion) {
					case 1:												//inicializar programa
							if(inicializarPrograma(conexion)){
								send(conexion,"1",1,0);}
							else{
							registrarWarning(archivoLog,"Ansisop rechazado, memoria insuficiente");
							send(conexion, "0",1,0);}
						break;

					case 4:
						break;
					}
				}else{salir=1;}
		}
		registrarWarning(archivoLog,"Se desconectó el Nucleo");
}

//--------------------------------FUNCIONES PARA EL NUCLEO----------------------------------
traductor_marco* buscar(int proceso, int pag, int off, int size){
	int paginaDelProceso(traductor_marco* fila){
		if ((fila->proceso==proceso) && (fila->pagina==pag)){
		return 1;
		}
	return 0;
	}
	traductor_marco* encontrada=list_find(tabla_de_paginas,(void*)paginaDelProceso);
	if (encontrada!=NULL){return encontrada;}
	else{
																//todo peticion a swap

		encontrada->pagina=-1;
	return encontrada;
	}
}

int ponerEnMemoria2(void* datos,int proceso,int pag, int size){
	int pos=0,tamMarco=datosMemoria->marco_size;
	traductor_marco* traductorMarco=malloc(sizeof(traductor_marco));
	pos=buscarMarcoLibre(proceso);
	memcpy(memoria+pos*tamMarco,datos,size);
	traductorMarco->pagina=pag;
	traductorMarco->proceso=proceso;
	traductorMarco->marco=pos;
	traductorMarco->modificada=0;
	list_add(tabla_de_paginas,traductorMarco);
	return 1;
}

void crearTabla(int pag, int proceso, int marco){
	traductor_marco* traductorMarco=malloc(sizeof(traductor_marco));
	traductorMarco->pagina=pag;
	traductorMarco->proceso=proceso;
	traductorMarco->marco=marco;
	traductorMarco->modificada=0;
	list_add(tabla_de_paginas,traductorMarco);
}

int ponerEnMemoria(char* codigo,int proceso,int paginasNecesarias){
	int i=0,marco,tamMarco=datosMemoria->marco_size;
	do{
			marco=buscarMarcoLibre(proceso);
			memcpy(memoria+marco*tamMarco,codigo+4+i*tamMarco,tamMarco);
			crearTabla(i,proceso,marco);
			i++;
	}while (i < paginasNecesarias);
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

int buscarMarcoLibre(int pid) {
	int pos=0, cantMarcos=marcosAsignados(pid,1);
	int clockDelProceso(unClock* marco){
		return(marco->proceso==pid);
	}
	int marcoDelProceso(traductor_marco* marco){
		return(marco->proceso==pid && marco->marco>=0);				//está en memoria
	}
	int marcoPosicion(traductor_marco* marco){
			return(marco->marco==pos);
	}
	void borrarLista(traductor_marco* marco){
		free(marco);
	}

	if (cantMarcos < 5){//datosMemoria->marco_x_proc) {				//cantidad de marcos del proceso para ver si reemplazo, o asigno vacios
		for (pos= 0; pos < datosMemoria->marcos; pos++) {				//Se fija si hay marcos vacios
			if (!vectorPaginas[pos]) {
				if ((datosMemoria->algoritmo) && !cantMarcos){										//Para el clock mejorado, registro el clock del proceso
					unClock* clockProceso=malloc(sizeof(unClock));
					clockProceso->clock=pos;
					clockProceso->proceso=pid;
					list_add(tablaClocks,clockProceso);
				}
				vectorPaginas[pos] = 2;
				return pos;}
		}
	}
	if (cantMarcos){							//Si tiene marcos => hay que reemplazarle uno, SINO no hay espacio

		traductor_marco* datosMarco = malloc(sizeof(traductor_marco));
		t_list* listaFiltrada=list_filter(tabla_de_paginas,(void*)marcoDelProceso);				//Filtro los marcos de ESE proceso

		int i=0, encontrado=0;
		unClock* marcoClock=malloc(sizeof(unClock));
		if (datosMemoria->algoritmo) {									//Clock mejorado, busco el clock del proceso
			marcoClock =(unClock*)list_find(tablaClocks,(void*)clockDelProceso);
			pos=marcoClock->clock;
			for(i=0;!encontrado;i++){										//Busco de los filtrados, el que se corresponde con el clock
				datosMarco=list_get(listaFiltrada,i);
				if (datosMarco->marco==pos){
					encontrado=1;
					i--;
				}
			}
		}										//CLOCK SIMPLE=0
		do {	if(i==list_size(listaFiltrada)){i=0;}
			datosMarco = list_get(listaFiltrada,i);						//Chequeo c/u para ver cual sacar
			pos=datosMarco->marco;
				if (vectorPaginas[pos] == 1){															//Se va de la UMC
					if (datosMarco->modificada) {														//Estaba modificada => se la mando a la swap
						char* mje = malloc(datosMemoria->marco_size + 10);
						memcpy(mje, "2", 1);															//2 = guardar pagina
						memcpy(mje + 1, header(datosMarco->proceso), 4);
						memcpy(mje + 5, header(datosMarco->pagina), 4);
						memcpy(mje + 9,	memoria	+ datosMarco->marco	* datosMemoria->marco_size,	datosMemoria->marco_size);
						memcpy(mje + 9 + datosMemoria->marco_size, "\0", 1);
						send(conexionSwap, mje, string_length(mje), 0);
						free(mje);
					}
					vectorPaginas[pos] = 2;
					printf("Marco eliminado: %d\n",pos);
					list_remove_by_condition(tabla_de_paginas,(void*)marcoPosicion);
					if (datosMemoria->algoritmo) {														//Elimino de la lista
						list_remove_by_condition(tablaClocks,(void*)clockDelProceso);
						datosMarco=list_find(listaFiltrada,(void*)marcoDelProceso);
						marcoClock->clock=datosMarco->marco;
						list_add(tablaClocks,marcoClock);
					}
					free(datosMarco);
					list_clean(listaFiltrada);
					return pos;													//La nueva posicion libre
				}
				else {vectorPaginas[pos]--;
			if (!datosMemoria->algoritmo) {									//Clock comun => saco y pongo al final de la lista
				list_remove_by_condition(tabla_de_paginas,(void*)marcoPosicion);
				list_add(tabla_de_paginas, datosMarco);}
				}
			}while (++i);
	}
	return -1;
}


int marcosAsignados(int pid, int operacion){
	int marcosDelProceso(traductor_marco* marco){
		return (marco->proceso==pid && marco->marco>=0);
	}
 return (list_count_satisfying(tabla_de_paginas,(void*)marcosDelProceso));
}



int inicializarPrograma(int conexion) {
	int espacio_del_codigo;
	int PID=recibirProtocolo(conexion);							//PID + PaginasNecesarias
	int paginasNecesarias=recibirProtocolo(conexion);
	espacio_del_codigo = recibirProtocolo(conexion);
	char* codigo = recibirMensaje(conexion, espacio_del_codigo);			//CODIGO
	//printf("Codigo: %s-\n",codigo);

	agregarHeader(&codigo);
	char* programa = string_new();
	string_append(&programa, "1");
	//string_append(&programa, mjeInicial);
	string_append(&programa, codigo);
	ponerEnMemoria(codigo,PID,paginasNecesarias);
//	free(mjeInicial);
	send(conexionSwap, programa, string_length(programa), 0);
	free(programa);
	return esperarRespuestaSwap();
}

int esperarRespuestaSwap(){
	char*respuesta = malloc(3);
	recv(conexionSwap, respuesta, 2, 0);
	respuesta[2] = '\0';
	int aceptado = string_equals_ignore_case(respuesta, "ok");
	free(respuesta);
	return aceptado;
}
/*
memcpy(mje+9+datosMemoria->marco_size,"2",1);				//2 =
					memcpy(mje+10+datosMemoria->marco_size,)
					memcpy(mje+6+datosMemoria->marco_size,"2",1);
*/
