
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <parser/parser.h>
#include <commons/collections/list.h>
#include "Funciones/json.h"
#include "Funciones/sockets.h"


typedef struct{
	int pag;
	int off;
	int size;
}Pagina;

typedef struct{
	char id;
	Pagina pag;
}Vars;

typedef struct{
	t_list* args; //Lista de Pagina
	t_list* vars; //Lista de Vars
	int retPos;
	Pagina retvar;
}Stack;

typedef struct{
	int id;
	t_metadata_program pc;
	t_list* stack; //lista de Stack
	int indice_stack;
}PCB;

static const int CONTENIDO_VARIABLE = 20;
static const int POSICION_MEMORIA = 0x10;

void parsear(char* instruccion);

int nucleo;
int umc;
PCB pcb;
int finalizado;
int CODIGO_IMPRESION = 0;
int CODIGO_ASIGNACION_UMC = 0;
int CODIGO_ASIGNACION_NUCLEO = 0;
int CODIGO_FINALIZACION = 0;
int CODIGO_DESREFERENCIA_UMC = 0;
int CODIGO_DESREFERENCIA_NUCLEO = 0;
int TAMANIO_PAGINA = 1;

t_puntero definirVariable(t_nombre_variable variable);
t_puntero obtenerPosicionVariable(t_nombre_variable variable);
t_valor_variable dereferenciar(t_puntero pagina);
void asignar(t_puntero pagina, t_valor_variable variable);
void imprimir(t_valor_variable valor);
void imprimirTexto(char* texto);
void finalizar() ;
void llamadasSinRetorno(char* texto);

Vars crearVariable(char variable);
Vars cargarVariable(char variable);
Pagina obtenerPagDisponible();
Pagina fromIntegerPagina(int int_pagina);
int toIntegerPagina(Pagina pagina);
void sumarEnLasVariables(Vars* var);
void saltoDeLinea(int cantidad, char* nombre);


AnSISOP_funciones functions = {
		.AnSISOP_definirVariable		= definirVariable,
		.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable,
		.AnSISOP_dereferenciar			= dereferenciar,
		.AnSISOP_asignar				= asignar,
		.AnSISOP_imprimir				= imprimir,
		.AnSISOP_imprimirTexto			= imprimirTexto,
		.AnSISOP_finalizar				 = finalizar,
		.AnSISOP_llamarSinRetorno       = llamadasSinRetorno,
};
AnSISOP_kernel kernel_functions = {};

void parsear(char* instruccion){
	analizadorLinea(strdup(instruccion), &functions, &kernel_functions);
}

int tienePermiso(char* autentificacion){
	return 1;
}
