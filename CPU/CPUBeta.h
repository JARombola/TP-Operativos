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
#include <commons/log.h>

t_log* archivoLog;

int nucleo;
int umc;
PCB pcb;
int PUERTO_NUCLEO= 0;
char AUTENTIFICACION[100];
char ARCHIVO_DE_CONFIGURACION[60] = "ArchivoDeConfiguracionCPU.txt";
char IP_NUCLEO[50];
char IP_UMC[50];
int PUERTO_UMC = 0;
int status = 1;

int finalizado = 0;

int TAMANIO_PAGINA = 1;

int levantarArchivoDeConfiguracion();
void conectarseAlNucleo();
void conectarseALaUMC();
int procesarPeticion();
void procesarCodigo(int quantum, int quantum_sleep);
char* pedirLinea();

void crearHiloSignal();
void hiloSignal();
void cerrarCPU(int senial);

t_puntero definirVariable(t_nombre_variable variable);
t_puntero obtenerPosicionVariable(t_nombre_variable variable);
t_valor_variable dereferenciar(t_puntero pagina);
void asignar(t_puntero pagina, t_valor_variable variable);
void imprimir(t_valor_variable Paginavalor);
void imprimirTexto(char* texto);
void finalizar() ;
t_valor_variable obtenerValorCompartida(t_nombre_compartida	variable);
t_valor_variable asignarValorCompartida(t_nombre_compartida	variable, t_valor_variable valor);
void irAlLabel(t_nombre_etiqueta etiqueta);
void llamarConRetorno(t_nombre_etiqueta	etiqueta, t_puntero	donde_retornar);
void entradaSalida(t_nombre_dispositivo,int tiempo);
void wait(t_nombre_semaforo identificador_semaforo);
void post(t_nombre_semaforo identificador_semaforo);
void retornar(t_valor_variable retorno);

Variable* crearVariable(char variable);
Pagina obtenerPagDisponible();
void sumarEnLasVariables(Variable* var);
Stack* obtenerStack();
Stack* anteUltimoStack();
void enviarMensajeNucleoConsulta(char* variable);
void enviarMensajeNucleoAsignacion(char* variable, int valor);
void enviarMensajeUMCConsulta(int pag, int off, int size, int proceso);	//0 = pedir linea codigo, 1 = pedir valor almacenado
void enviarMensajeUMCAsignacion(int pag, int off, int size, int proceso, int valor);
void parsear(char* instruccion);
int numeroPagina(Pagina pag);


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
		.AnSISOP_irAlLabel 				= irAlLabel,
		.AnSISOP_llamarConRetorno		= llamarConRetorno,
		.AnSISOP_entradaSalida			= entradaSalida,
		.AnSISOP_retornar               = retornar,
};
AnSISOP_kernel kernel_functions = {
		.AnSISOP_signal = post,
		.AnSISOP_wait = wait,
};

/*
 * Finalizado:
 *   1) Finalizado Ok
 * 	 2) Entrada / Salida
 * 	 3) Wait Bloqueante
 * 	 4) Quantum
 * Finalizado-Error:
 * 	-1) Error de conexion UMC
 * 	-2) Error de conexion Nucleo
 * 	-9) Error turbio.. nunca deberia entrar ak
 */

#endif /* CPUBETA_H_ */
