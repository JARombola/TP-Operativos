/*
 * CPUBeta.h
 *
 *  Created on: 4/6/2016
 *      Author: utnso
 */

#ifndef CPUBETA_H_
#define CPUBETA_H_


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <parser/parser.h>
#include <commons/collections/list.h>
#include "Funciones/json.h"
#include "Funciones/sockets.h"
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

static const int CONTENIDO_VARIABLE = 20;
static const int POSICION_MEMORIA = 0x10;

int nucleo;
int umc;
PCB pcb;
int PUERTO_NUCLEO= 0;
char AUTENTIFICACION[100];
char ARCHIVO_DE_CONFIGURACION[60] = "ArchivoDeConfiguracionCPU.txt";
char IP_NUCLEO[50];
char IP_UMC[50];
int PUERTO_UMC = 0;
int quantum = 1;


int finalizado;
int CODIGO_IMPRESION = 0;
int CODIGO_ASIGNACION_UMC = 0;
int CODIGO_ASIGNACION_NUCLEO = 0;
int CODIGO_FINALIZACION = 0;
int CODIGO_DESREFERENCIA_UMC = 0;
int CODIGO_DESREFERENCIA_NUCLEO = 0;
int CODIGO_WAIT = 3;
int CODIGO_SIGNAL = 0;
int CODIGO_CONSULTA_UMC = 0;
int TAMANIO_PAGINA = 1;



int levantarArchivoDeConfiguracion();
void conectarseAlNucleo();
void conectarseALaUMC();
int procesarPeticion();
int procesarCodigo();
char* pedirLinea();

t_puntero definirVariable(t_nombre_variable variable);
t_puntero obtenerPosicionVariable(t_nombre_variable variable);
t_valor_variable dereferenciar(t_puntero pagina);
void asignar(t_puntero pagina, t_valor_variable variable);
void imprimir(t_valor_variable Paginavalor);
void imprimirTexto(char* texto);
void finalizar() ;
t_valor_variable obtenerValorCompartida(t_nombre_compartida	variable);
t_valor_variable asignarValorCompartida(t_nombre_compartida	variable, t_valor_variable valor);
t_puntero_instruccion irAlLabel(t_nombre_etiqueta etiqueta);
void llamarConRetorno(t_nombre_etiqueta	etiqueta, t_puntero	donde_retornar);
void entradaSalida(t_nombre_dispositivo,int tiempo);
void wait(t_nombre_semaforo identificador_semaforo);
void signalHola(t_nombre_semaforo identificador_semaforo);

Variable* crearVariable(char variable);
Pagina obtenerPagDisponible();
void sumarEnLasVariables(Variable* var);
Stack* obtenerStack();
void enviarMensajeNucleoConsulta(char* variable);
void enviarMensajeNucleoAsignacion(char* variable, int valor);
void enviarMensajeUMCConsulta(int pag, int off, int size, int proceso);	//0 = pedir linea codigo, 1 = pedir valor almacenado
void enviarMensajeUMCAsignacion(int pag, int off, int size, int proceso, int valor);
void saltoDeLinea(t_nombre_etiqueta t_nombre_etiqueta);
void parsear(char* instruccion);
void retornar(t_valor_variable retorno);


AnSISOP_funciones functions = {
		.AnSISOP_definirVariable		= definirVariable,
		.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable,
		.AnSISOP_dereferenciar			= dereferenciar,
		.AnSISOP_asignar				= asignar,
		.AnSISOP_imprimir				= imprimir,
		.AnSISOP_imprimirTexto			= imprimirTexto,
		.AnSISOP_finalizar				= finalizar,
		.AnSISOP_obtenerValorCompartida = obtenerValorCompartida,
		.AnSISOP_asignarValorCompartida = asignarValorCompartida,
		.AnSISOP_irAlLabel 				= saltoDeLinea,
		.AnSISOP_llamarConRetorno		= llamarConRetorno,
		.AnSISOP_entradaSalida			= entradaSalida,
		.AnSISOP_retornar               = retornar,
};
AnSISOP_kernel kernel_functions = {
		.AnSISOP_signal = signalHola,
		.AnSISOP_wait = wait,
};

#endif /* CPUBETA_H_ */
