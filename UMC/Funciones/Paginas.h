/*
 * Paginas.h
 *
 *  Created on: 1/6/2016
 *      Author: utnso
 */

#ifndef FUNCIONES_PAGINAS_H_
#define FUNCIONES_PAGINAS_H_

#include <commons/collections/list.h>
#include <commons/string.h>
#include <commons/log.h>
#include "Comunicacion.h"
#include <string.h>
#include <commons/collections/queue.h>
#include <pthread.h>
#include <unistd.h>


typedef struct{
    int proceso, pagina, marco, enMemoria, modificada;
}traductor_marco;

typedef struct{
    int proceso, posClock;
    t_queue* colaMarcos;
}unClock;

extern pthread_mutex_t mutexMarcos,mutexTablaPaginas;
extern t_list* tablaClocks, *tabla_de_paginas, *tlb;

extern int conexionSwap;
extern void* memoria;
extern t_log* archivoLog;
extern int* vectorMarcos;

int buscar(int proceso, int pag);
traductor_marco* solicitarPaginaASwap(int proceso, int pag);
traductor_marco* actualizarTabla(int pag, int proceso, int marco);
traductor_marco* guardarPagina(void* datos,int proceso,int pag);
int buscarMarco(int pid);
int buscarMarcoLibre(int pid, int cantMarcos);
int marcosAsignados(int pid);
int hayMarcosLibres();
void enviarPaginaASwap(traductor_marco* datosMarco);

#endif /* FUNCIONES_PAGINAS_H_ */
