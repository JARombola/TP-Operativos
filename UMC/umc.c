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


int leerConfiguracion(char*, datosConfiguracion**);
int autentificar(int);
int comprobarCliente(int);
void mostrarTablaPag(traductor_marco*);
int aceptarNucleo(int,struct sockaddr_in);
//COMPLETAR...........................................................
void comprobarOperacion(int);
int enviarBytes(int conexion, int proceso,int pagina,int offset,int size);
int almacenarBytes(int proceso,int pagina, int offset, int tamanio, int buffer);
void finalizarPrograma(int);
void consola();
void atenderNucleo(int);
void atenderCpu(int);
int guardarPagina(void* datos,int proceso,int pag);
int buscarMarcoLibre(int);
int inicializarPrograma(int);					// a traves del socket recibe el PID + Cant de Paginas + Codigo
int esperarRespuestaSwap();
//-----MENSAJES----
int buscar(int, int);
int marcosAsignados(int pid, int operacion);
void actualizarTabla(int pag,int proceso,int marco);

//COMANDOS--------------

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_list *tabla_de_paginas;
int totalPaginas,conexionSwap, *vectorPaginas;
void* memoria;
datosConfiguracion *datosMemoria;
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
	datosMemoria->algoritmo=1;														//todo CAMBIAR ALGORITMO
	vectorPaginas=(int*) malloc(datosMemoria->marcos*4);
	memoria = (void*) malloc(marcosTotal);
	int j;
	for (j=0;j<datosMemoria->marcos;j++){
		vectorPaginas[j]=0;															//Para la busqueda de marcos (como un bitMap)
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

int enviarBytes(int conexion, int proceso,int pagina,int offset,int size){
	int posicion=buscar(proceso, pagina);
	if (posicion!=-1){
		void* mje=(void*) malloc(size);
		memcpy(mje,memoria+posicion+offset,size);
		send(conexion,mje,size,0);
		free(mje);
		return 1;
	}
	return 0;				//No existe la pag
}


int almacenarBytes(int proceso, int pagina, int offset, int size, int buffer){
	int buscarMarco(traductor_marco* fila){
			return (fila->proceso==proceso && fila->pagina==pagina);}
	void modificada(traductor_marco* fila){
		if(buscarMarco(fila)){fila->modificada=1;}}

	int posicion=buscar(proceso,pagina);
	posicion+=offset;
	memcpy(memoria+posicion,&buffer,size);
	list_iterate(tabla_de_paginas,(void*)modificada);
	printf("Pagina modificados\n");
	return posicion;
}

int buscar(int proceso, int pag) {				//todo busqueda en la TLB
	int paginaBuscada(traductor_marco* fila) {
		if ((fila->proceso == proceso) && (fila->pagina == pag)) {
			return 1;}
		return 0;
	}
	int posicion;
	traductor_marco* encontrada = list_find(tabla_de_paginas,(void*) paginaBuscada);
	if (encontrada != NULL) {				//Esta "registrada" la pag (Existe)
		void* datos = (void*) malloc(datosMemoria->marco_size);
		if (encontrada->marco >= 0) {						//Está en memoria
			posicion=encontrada->marco * datosMemoria->marco_size;
		} else {					//todo no está en memoria => peticion a swap
			char* pedido = string_new();
			string_append(&pedido, "2");
			string_append(&pedido, header(proceso));
			string_append(&pedido, header(pag));
			string_append(&pedido, "\0");
			send(conexionSwap, pedido, string_length(pedido), 0);
			free(pedido);
			recv(conexionSwap, datos, datosMemoria->marco_size, 0);
			posicion=guardarPagina(datos, proceso, pag);
			posicion*=datosMemoria->marco_size;
		}
		return posicion;								//devuelve la posicion dentro de la "memoria"
	}
	printf("No existe la pagina solicitada\n");
	return -1;
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
				int pos=buscar(0,VELOCIDAD);
				void* mje=malloc(16);
				memcpy(mje,memoria+pos,15);
				memcpy(mje+15
						, "\0",1);
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
	//				- 1) Codigo de operacion (2=consulta, 3=guardar valor)
	//			   	- 2) ID PROCESO (4 byes)
	//				- 3) despues se reciben Pag, offset, size, buffer (valor a guardar, solo cuando es necesario).

	registrarTrace(archivoLog, "Nuevo CPU-");
	int salir = 0, operacion, proceso, pagina, offset, buffer, size;
	while (!salir) {
		operacion = atoi(recibirMensaje(conexion, 1));
		if (operacion) {
			proceso = recibirProtocolo(conexion);
			pagina = recibirProtocolo(conexion);
			offset = recibirProtocolo(conexion);
			size=recibirProtocolo(conexion);
			switch (operacion) {
			case 2:													//2 = Enviar Bytes (busco pag, y devuelvo el valor)
				enviarBytes(conexion,proceso,pagina,offset,size);
				break;
			case 3:													//3 = Guardar Valor
				recv(conexion,&buffer,sizeof(int),0);
				buffer=ntohl(buffer);
				almacenarBytes(proceso,pagina,offset,size,buffer);				//todo
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
								send(conexion,"1",1,0);
							list_iterate(tabla_de_paginas,(void*)mostrarTablaPag);
					}else{
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

void actualizarTabla(int pag, int proceso, int marco){
	traductor_marco* traductorMarco=malloc(sizeof(traductor_marco));
	int existe(traductor_marco* entrada){
		return (entrada->pagina==pag && entrada->proceso==proceso);
	}
	if(list_any_satisfy(tabla_de_paginas,(void*)existe)){						//Si la pagina existe, la actualizo
		int i,encontrado=0;
		for (i=0;!encontrado;i++){
			traductorMarco=list_get(tabla_de_paginas,i);
			if (traductorMarco->pagina==pag && traductorMarco->proceso==proceso){
				encontrado=1;
				i--;
			}
		}
		traductorMarco->marco=marco;
		traductorMarco->modificada=0;
		if (datosMemoria->algoritmo){
		list_replace(tabla_de_paginas,i,traductorMarco);}
		else{
			list_remove(tabla_de_paginas,i);
			list_add(tabla_de_paginas,traductorMarco);
		}
	}
	else{																//Sino la registro
	traductorMarco->pagina=pag;
	traductorMarco->proceso=proceso;
	traductorMarco->marco=marco;
	traductorMarco->modificada=0;
	printf("Pag %d, proceso %d, marco %d\n",traductorMarco->pagina,traductorMarco->proceso,traductorMarco->marco);
	list_add(tabla_de_paginas,traductorMarco);}
}


int guardarPagina(void* datos,int proceso,int pag){
	int marco,tamMarco=datosMemoria->marco_size;
	marco = buscarMarcoLibre(proceso);
	memcpy(memoria + marco * tamMarco, datos, tamMarco);//	memcpy(memoria + marco * tamMarco, datos + pag * tamMarco, size);
	actualizarTabla(pag, proceso, marco);
	registrarInfo(archivoLog,"Ansisop guardado con exito!");
//	printf("Paginas Necesarias:%d , TotalMarcosGuardados: %d\n",paginasNecesarias,i);
//	printf("TablaDePaginas:%d\n",list_size(tabla_de_paginas));
	return marco;
}

void mostrarTablaPag(traductor_marco* fila) {
	printf("Marco: %d, Pag: %d, Proc:%d\n", fila->marco, fila->pagina,fila->proceso);
	char* asd = malloc(datosMemoria->marco_size + 1);
	memcpy(asd, memoria + datosMemoria->marco_size * fila->pagina,datosMemoria->marco_size);//memcpy(asd, memoria + datosMemoria->marco_size * fila->marco,datosMemoria->marco_size);
	memcpy(asd + datosMemoria->marco_size , "\0", 1);
	printf("%s\n", asd);
}

int buscarMarcoLibre(int pid) {
	int pos = 0, cantMarcos = marcosAsignados(pid, 1);
	int menorMayor(traductor_marco* marco1, traductor_marco* marco2){
		return marco1->marco<marco2->marco;
	}
	int marcoDelProceso(traductor_marco* marco) {
		return (marco->proceso == pid && marco->marco >= 0);										//está en memoria
	}
	int marcoPosicion(traductor_marco* marco) {													//porque la pos que ocupa y el marco no son el mismo, necesito el marco con esa posicion
		return (marco->marco == pos);
	}
	int marcoNuevo(traductor_marco* marco) {
		return (vectorPaginas[marco->marco] == 2);
	}
	int marcoViejo(traductor_marco* marco) {
		return (vectorPaginas[marco->marco] == 1);
	}

	if (cantMarcos < datosMemoria->marco_x_proc) {												//cantidad de marcos del proceso para ver si reemplazo, o asigno vacios
		for (pos = 0; pos < datosMemoria->marcos; pos++) {											//Se fija si hay marcos vacios
			if (!vectorPaginas[pos]) {
				vectorPaginas[pos] = 2;
				return pos;
			}
		}
	}
	if (cantMarcos) {																			//Si tiene marcos => hay que reemplazarle uno, SINO no hay espacio
		traductor_marco* datosMarco = malloc(sizeof(traductor_marco));
		t_list* listaFiltrada = list_filter(tabla_de_paginas,
				(void*) marcoDelProceso);														//Filtro los marcos de ESE proceso

		int i = 0;
		if (datosMemoria->algoritmo) {
			list_sort(listaFiltrada,(void*)menorMayor);				//CLOCK MEJORADO
			if (list_all_satisfy(listaFiltrada, (void*) marcoNuevo)) {								//Si son todas pags nuevas arranco x la primera (FIFO)
				i = 0;
			} else {
				int encontrado=0;
				datosMarco = list_find(listaFiltrada, (void*) marcoViejo);							//Sino, busco el primero disponible para sacar
				pos = datosMarco->marco;
				for (i = 0; !encontrado; i++) {
					datosMarco = list_get(listaFiltrada, i);
					if (datosMarco->marco == pos) {
						encontrado = 1;
						i--;
					}
				}
			}
		}
		do {traductor_marco* datosMarco = malloc(sizeof(traductor_marco));
			if (i == list_size(listaFiltrada)) {													//Para que pegue la vuelta
				i = 0;
			}
			datosMarco = list_get(listaFiltrada, i);												//Chequeo c/u para ver cual sacar
			pos = datosMarco->marco;
			if (vectorPaginas[pos] == 1) {														//Se va de la UMC
				if (datosMarco->modificada) {														//Estaba modificada => se la mando a la swap
					char* mje = string_new();
					string_append(&mje, "1");
					string_append(&mje, header(datosMarco->proceso));
					string_append(&mje,header(0));
					string_append(&mje,	memoria+ datosMarco->marco* datosMemoria->marco_size);
					string_append(&mje, header(datosMarco->pagina));
					string_append(&mje, header(0));
					string_append(&mje, "\0");
					send(conexionSwap, mje, string_length(mje), 0);
					free(mje);
				}
				vectorPaginas[pos] = 2;
				printf("Marco eliminado: %d\n", pos);
				datosMarco = list_find(tabla_de_paginas, (void*) marcoPosicion);
				actualizarTabla(datosMarco->pagina,pid,-1);
		/*		datosMarco->marco = -1;
				list_replace(tabla_de_paginas, (int) marcoPosicion, datosMarco);*/
				list_clean(listaFiltrada);
				return pos;}																					//La nueva posicion libre
			else {
				vectorPaginas[pos]--;
				if (!datosMemoria->algoritmo) {																//Clock comun => saco y pongo al final de la lista
					list_remove_by_condition(tabla_de_paginas,(void*) marcoPosicion);
					list_add(tabla_de_paginas, datosMarco);
				}
			}
		} while (++i);
	}
	return -1;																						//No hay marcos para darle
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
	int i;
	void* asd=(void*)malloc(datosMemoria->marco_size);
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
	printf("%s %d\n",programa,string_length(programa));
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

