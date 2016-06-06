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
#include "Comunicacion.h"
#include "ArchivosLogs.h"
#include <string.h>

typedef struct{
	int proceso, pagina, marco, enMemoria, modificada;
}traductor_marco;

typedef struct{
	char *ip, *ip_swap;				//PASAR A IP CON: inet_addr() / o inet_ntoa()
	int puerto_umc, puerto_swap, marcos, marco_size, marco_x_proc, entradas_tlb, retardo, algoritmo;
}datosConfiguracion;

typedef struct{
	int proceso, clock;
}unClock;

extern t_list* tablaClocks;
extern t_list* tabla_de_paginas;
extern datosConfiguracion* datosMemoria;
extern int conexionSwap;
extern void* memoria;
extern t_log* archivoLog;
extern int* vectorMarcos;

int buscar(int proceso, int pag);
void actualizarTabla(int pag, int proceso, int marco);
int guardarPagina(void* datos,int proceso,int pag);
int buscarMarcoLibre(int pid);
int marcosAsignados(int pid, int operacion);
int hayMarcosLibres();


#endif /* FUNCIONES_PAGINAS_H_ */
