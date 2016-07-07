/*
 * umc.h
 *
 *  Created on: 5/7/2016
 *      Author: utnso
 */

#ifndef UMC_H_
#define UMC_H_
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/bitarray.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <pthread.h>
#include <unistd.h>
#include "Funciones/Comunicacion.h"
#include "Funciones/Paginas.h"


#define esIgual(a,b) string_equals_ignore_case(a,b)
#define marcosTotal datosMemoria->marco_size*datosMemoria->marcos
#define INICIALIZAR 1
#define ENVIAR_BYTES 2
#define GUARDAR_BYTES 3
#define FINALIZAR 4



pthread_mutex_t mutexMarcos=PTHREAD_MUTEX_INITIALIZER,								// Para sincronizar busqueda de marcos libres
				mutexTablaPaginas=PTHREAD_MUTEX_INITIALIZER,						// Sincroniza entradas a la tabla de paginas
				mutexSwap=PTHREAD_MUTEX_INITIALIZER;								// Sincroniza pedidos a Swap

t_list *tabla_de_paginas, *tablaClocks,*tlb;
int totalPaginas,conexionSwap,cantSt, *vectorMarcos;
void* memoria;
datosConfiguracion* datosMemoria;
t_log* archivoLog;
FILE* reporteDump;
t_dictionary* procesos;


void consola();
void atenderNucleo(int);
int atenderCpu(int);

int esperarRespuestaSwap();

int inicializarPrograma(int);                    // a traves del socket recibe el PID + Cant de Paginas + Codigo
void* enviarBytes(int proceso,int pagina,int offset,int size);
int almacenarBytes(int proceso,int pagina, int offset, int tamanio, int buffer);
int finalizarPrograma(int);

void dumpTabla(traductor_marco*);
void dumpDatos(traductor_marco*);

void mostrarTablaPag(traductor_marco*);
void guardarDump(t_list* proceso);



#endif /* UMC_H_ */
