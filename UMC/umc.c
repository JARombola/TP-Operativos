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
#include "Funciones/Paginas.h"

#define esIgual(a,b) string_equals_ignore_case(a,b)
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)
#define marcosTotal datosMemoria->marco_size*datosMemoria->marcos



int leerConfiguracion(char*, datosConfiguracion**);
int autentificar(int);
int comprobarCliente(int);
void mostrarTablaPag(traductor_marco*);
int aceptarNucleo(int,struct sockaddr_in);
//COMPLETAR...........................................................
void* enviarBytes(int proceso,int pagina,int offset,int size);
int almacenarBytes(int proceso,int pagina, int offset, int tamanio, int buffer);
int finalizarPrograma(int);
void consola();
void atenderNucleo(int);
void atenderCpu(int);
int inicializarPrograma(int);					// a traves del socket recibe el PID + Cant de Paginas + Codigo
int esperarRespuestaSwap();
//-----MENSAJES----

//COMANDOS--------------

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_list *tabla_de_paginas;
int totalPaginas,conexionSwap, *vectorMarcos;
void* memoria;
datosConfiguracion* datosMemoria;
t_log* archivoLog;



int main(int argc, char* argv[]) {
	/*void* ja=(void*) malloc(13); int aa=100;
	memcpy(ja,&aa,4);
	memcpy(ja+4,"holaaasss",9);
	int x;memcpy(&x,ja,4);
	char* v=malloc(6);
	memcpy(v,ja+4,6);
	printf("%d %s,\n",x,v);*/

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
	////////////////////////////////////////////////
	datosMemoria->algoritmo=0;														//todo CAMBIAR ALGORITMO
	///////////////////////////////////////////////////
	vectorMarcos=(int*) malloc(datosMemoria->marcos*sizeof(int*));
	memoria = (void*) malloc(marcosTotal);
	int j;
	for (j=0;j<datosMemoria->marcos;j++){
		vectorMarcos[j]=0;															//Para la busqueda de marcos (como un bitMap)
	}

	//void* a=(void*) malloc(10);
	/*char* p=malloc(5);
	memcpy(p,"hola",4);
	memcpy(a,p,4);
	memcpy(a+4,"\0",1);
	printf("%s-",a);
	void* q;
	memcpy(q,a,4);
	char* m=malloc(5);
	memcpy(m,q,4);
	memcpy(m+4,"\0",1);
	printf("%s\n",m);*/
	//int b=100;int *p=&b;
//	printf("%d-",b);int *p=&b;
/*	memcpy(a,p,4);
	printf("%d- ",a);
	void* q;
	memcpy(q,a,4);
	int d;//=malloc(4);
	memcpy(&d,q,4);
	printf("%d -",d);*/
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

	pthread_create(&thread, &attr, (void*) atenderNucleo,(void*) nucleo);						//Hilo para atender al nucleo
	pthread_create(&thread, &attr, (void*) consola, NULL);										//Hilo para atender comandos
	listen(umc_servidor, 100);																	//Para recibir conexiones (CPU's)
	int cpuRespuesta=htonl(datosMemoria->marco_size);

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


//-----------------------------------------------OPERACIONES UMC-------------------------------------------------

void* enviarBytes(int proceso,int pagina,int offset,int size){
	int posicion=buscar(proceso, pagina);
	if (posicion!=-1){
		void* mje=(void*) malloc(size);
		memcpy(mje,memoria+posicion+offset,size);
		return mje;
	}
	return "-1";				//No existe la pag
}


int almacenarBytes(int proceso, int pagina, int offset, int size, int buffer){
	int buscarMarco(traductor_marco* fila){
			return (fila->proceso==proceso && fila->pagina==pagina);}
	void modificada(traductor_marco* fila){
		if(buscarMarco(fila)){fila->modificada=1;}
	}

	int posicion=buscar(proceso,pagina);
	if(posicion==-1){								//no existe la pagina
		return -1;
	}
	posicion+=offset;
	memcpy(memoria+posicion,&buffer,size);
	list_iterate(tabla_de_paginas,(void*)modificada);
	printf("Pagina modificados\n");
	return posicion;
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
				int pos=buscar(0,VELOCIDAD);
				void* mje=malloc(16);
				memcpy(mje,memoria+pos,15);
				memcpy(mje+15, "\0",1);
				printf("%s\n",mje);//todo
				free(mje);
				/*printf("Estructuras de Memoria\n");
				printf("Datos de Memoria\n");*/
			} else {
				if (esIgual(comando, "tlb")) {
					int pos=almacenarBytes(0,0,0,4,10);
					void* asd=malloc(15);
					memcpy(asd,memoria+pos,15);
					int p;memcpy(&p,asd,4);
					printf("%d -\n",p);
					char* q=malloc(12);
					memcpy(q,asd+4,11);
					memcpy(q+11,"\0",1);
					printf("Q: %s\n",q);


					printf("TLB Borrada :)\n");
				} else {
					if (esIgual(comando, "memoria")) {
						list_iterate(tabla_de_paginas,(void*)mostrarTablaPag);
						finalizarPrograma(0);
						list_iterate(tabla_de_paginas,(void*)mostrarTablaPag);/*
						printf("Proceso?");
						scanf("%d",&procesoBuscar);//todo
						void comandoMemory(traductor_marco* pagina){
							if(pagina->proceso==procesoBuscar)pagina->modificada=1;
						}
						list_iterate(tabla_de_paginas,(void*)comandoMemory);
						printf("Paginas modificadas (proceso: %d)\n",procesoBuscar);*/
					}
				}
			}
		}
	}
}



void atenderCpu(int conexion){

	//				[PROTOCOLO]:
	//				- 1) Codigo de operacion (2=consulta, 3=guardar valor)
	//			   	- 2) ID PROCESO (4 byes)
	//				- 3) despues se reciben Pag, offset, size, buffer (valor a guardar, solo cuando es necesario).

	registrarTrace(archivoLog, "Nuevo CPU-");
	int salir = 0, operacion, proceso, pagina, offset, buffer, size;
	void* datos;
	while (!salir) {
		operacion = atoi(recibirMensaje(conexion, 1));
		if (operacion) {
			proceso = recibirProtocolo(conexion);
			pagina = recibirProtocolo(conexion);
			offset = recibirProtocolo(conexion);
			size=recibirProtocolo(conexion);
			switch (operacion) {
			case 2:													//2 = Enviar Bytes (busco pag, y devuelvo el valor)
				datos=enviarBytes(proceso,pagina,offset,size);
				send(conexion,datos,size,0);
				free(datos);
				break;
			case 3:													//3 = Guardar Valor
				recv(conexion,&buffer,sizeof(int),0);
				buffer=ntohl(buffer);
				if (almacenarBytes(proceso,pagina,offset,size,buffer)==-1){						//La pag no existe
					send(conexion,"-1",1,0);
				}
				break;
			}
		} else {
			salir = 1;
		}
	}
	registrarTrace(archivoLog, "CPU eliminada");
}


void atenderNucleo(int nucleo){
	registrarInfo(archivoLog,"Hilo de Nucleo creado");
		//[PROTOCOLO]: - siempre recibo PRIMERO el codigo de operacion (1 o 4) inicializar o finalizar
		int salir=0,guardar,procesoEliminar;
		while (!salir) {
			int operacion = atoi(recibirMensaje(nucleo,1));
				if (operacion) {
					switch (operacion) {
					case 1:												//inicializar programa
							guardar=inicializarPrograma(nucleo);
							if (guardar){							//1 = hay marcos (cola ready), 2 = no hay marcos (cola new)
							guardar=htonl(guardar);
							send(nucleo,&guardar,sizeof(int),0);
					}else{												//no lo aceptó la swap (adios ansisop)
							guardar=htonl(guardar);
							registrarWarning(archivoLog,"Ansisop rechazado, memoria insuficiente");
							send(nucleo, &guardar,sizeof(int),0);}
						break;

					case 4:												//Finalizar programa
							recv(nucleo,&procesoEliminar,sizeof(int),0);
							procesoEliminar=ntohl(procesoEliminar);
							if(finalizarPrograma(procesoEliminar)){
							printf("Proceso %d eliminado\n",procesoEliminar);
							}
						break;
					}
				}else{salir=1;}
		}
		registrarWarning(archivoLog,"Se desconectó el Nucleo");
}


void mostrarTablaPag(traductor_marco* fila) {
	printf("Marco: %d, Pag: %d, Proc:%d", fila->marco, fila->pagina,fila->proceso);
	char* asd = malloc(datosMemoria->marco_size + 1);
	memcpy(asd, memoria + datosMemoria->marco_size * fila->pagina,datosMemoria->marco_size);//memcpy(asd, memoria + datosMemoria->marco_size * fila->marco,datosMemoria->marco_size);
	memcpy(asd + datosMemoria->marco_size , "\0", 1);
	printf("-[%s]\n", asd);
}



int inicializarPrograma(int conexion) {
	int espacio_del_codigo;
	int PID=recibirProtocolo(conexion);							//PID + PaginasNecesarias
	int paginasNecesarias=recibirProtocolo(conexion);
	espacio_del_codigo = recibirProtocolo(conexion);
	char* codigo = recibirMensaje(conexion, espacio_del_codigo);			//CODIGO
	//printf("Codigo: %s-\n",codigo);
	int i;
	for (i = 0; i < paginasNecesarias; i++) {//Entradas en la tabla, SIN marcos
		  actualizarTabla(i, PID, -1);
	//	memcpy(asd,codigo+i*datosMemoria->marco_size,datosMemoria->marco_size);
	//	guardarPagina(asd, PID, i);
	}

	agregarHeader(&codigo);
	char* programa = string_new();
	string_append(&programa, "1");
	string_append(&programa, header(PID));
	string_append(&programa, header(paginasNecesarias));
	string_append(&programa, codigo);
	string_append(&programa, "\0");
	//printf("%s %d\n",programa,string_length(programa));
	send(conexionSwap, programa, string_length(programa), 0);
	free(programa);
	int aceptadoSwap= esperarRespuestaSwap();
	if (!aceptadoSwap){
		return 0;
	}
	printf("Ansisop guardado\n");
	if (hayMarcosLibres()){
		return 1;
	}
	return 2;
}

int finalizarPrograma(int procesoEliminar){
		int paginasDelProceso(traductor_marco* entradaTabla){
			return (entradaTabla->proceso==procesoEliminar);
		}
		void eliminarEntrada(traductor_marco* entrada){
			free(entrada);
		}
	void limpiar(traductor_marco* marco){
		list_remove_and_destroy_by_condition(tabla_de_paginas,(void*)paginasDelProceso,(void*)eliminarEntrada);
	}
	printf("LISTA antes: %d ",list_size(tabla_de_paginas));
	list_iterate(tabla_de_paginas,(void*)limpiar);
	printf(" /Despues %d\n",list_size(tabla_de_paginas));
	char* mensajeEliminar=string_new();
	string_append(&mensajeEliminar,"3");
	string_append(&mensajeEliminar,header(procesoEliminar));
	string_append(&mensajeEliminar,"\0");
	send(conexionSwap,mensajeEliminar,string_length(mensajeEliminar),0);
	free(mensajeEliminar);
	return 1;
}

int esperarRespuestaSwap(){
	char*respuesta = malloc(3);
	recv(conexionSwap, respuesta, 2, 0);
	respuesta[2] = '\0';
	int aceptado = string_equals_ignore_case(respuesta, "ok");
	free(respuesta);
	return aceptado;
}

