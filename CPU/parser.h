
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <parser/parser.h>
#include <commons/collections/list.h>
#include "Funciones/json.h"
#include "Funciones/sockets.h"

static const int CONTENIDO_VARIABLE = 20;
static const int POSICION_MEMORIA = 0x10;

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
int CODIGO_CONSULTA_UMC = 0;
int TAMANIO_PAGINA = 1;

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

Variable* crearVariable(char variable);
Pagina obtenerPagDisponible();
void sumarEnLasVariables(Variable* var);
char* pedirLinea();
Stack* obtenerStack();

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
};
AnSISOP_kernel kernel_functions = {};

void parsear(char* instruccion){
	analizadorLinea(strdup(instruccion), &functions, &kernel_functions);
}

int tienePermiso(char* autentificacion){
	return 1;
}

void saltoDeLinea(int cantidad, void* funcion){
	if (cantidad == 0){
		pcb.pc = metadata_buscar_etiqueta(funcion,pcb.indices.etiquetas,pcb.indices.etiquetas_size);
		return;
	}
	pcb.pc++;
}

void enviarMensajeUMCConsulta(int pag, int off, int size, int proceso){
	char* mensaje = malloc(18*sizeof(char));
	sprintf(mensaje,"2%s%s%s%s",toStringInt(proceso),toStringInt(pag),toStringInt(off),toStringInt(size));
	send(umc,mensaje,strlen(mensaje),0);
	free(mensaje);
}

void enviarMensajeUMCAsignacion(int pag, int off, int size, int proceso, int valor){
	char* mensaje = malloc(22*sizeof(char));
	sprintf(mensaje,"3%s%s%s%s%s",toStringInt(proceso),toStringInt(pag),toStringInt(off),toStringInt(size),toStringInt(valor));
	send(umc,mensaje,strlen(mensaje),0);
	free(mensaje);
}

void enviarMensajeNucleoConsulta(char* variable){

}
void enviarMensajeNucleoAsignacion(char* variable, int valor){

}
