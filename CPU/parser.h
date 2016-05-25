
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <parser/parser.h>
#include <commons/collections/list.h>

static const int CONTENIDO_VARIABLE = 20;
static const int POSICION_MEMORIA = 0x10;

void parsear(char* instruccion);

int nucleo;
int umc;
int pcb;
int finalizado;
int CODIGO_IMPRESION = 0;
int CODIGO_ASIGNACION_UMC = 0;
int CODIGO_ASIGNACION_NUCLEO = 0;
int CODIGO_FINALIZACION = 0;
int CODIGO_DESREFERENCIA_UMC = 0;
int CODIGO_DESREFERENCIA_NUCLEO = 0;

Pagina rearPaginaNegativa(){
	Pagina pagina;
	pagina.pag = -1;
	return pagina;
}
t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);
	Pagina pagina = siguientePaginaDisponible(variable);
	return  pagina;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable) {
	printf("Obtener posicion de %c\n", variable);
	t_list* variables = list_get(pcb.variables,(list_size(pcb.variables)));
	int i;
	for(i = 0; i< list_size(variables)-1; i++){
		if (strcmp(variables[i].id,variable)== 0){
			return &(variable[i].pagina);
		}
	}
	return &crearPaginaNegativa();
}

t_valor_variable dereferenciar(t_puntero pagina) {
	printf("Dereferenciar %d y su valor es: [VALOR]\n", pagina);
	char* mensaje = toStringPagina(pagina);
	int variable;
	if (pagina.pag == -1){
		enviarMensajeNucleoConProtocolo(nucleo, mensaje, CODIGO_DESREFERENCIA_NUCLEO);
		variable = atoi(esperarRespuesta(nucleo));
	}else{
		enviarMensajeConProtocolo(umc,mensaje,CODIGO_DESREFERENCIA_UMC);
		variable = atoi(esperarRespuesta(umc));
	}
	return variable;
}

void asignar(t_puntero pagina, t_valor_variable variable) {
	char mensaje[100];
	char separador[2] = "/";
	char valor[10];
	sprintf(valor,"%d", variable);
	if (pagina.pag == -1){
		char id[10];
		sprintf(id,"%d/", pagina.id);
		strcpy(mensaje,id);
		strcat(mensaje,valor);
		enviarMensajeCOnProtocolo(nucleo, mensaje, CODIGO_ASIGNACION_NUCLEO);
		return;
	}
	strcpy(mensaje, toStringPagina(pagina));
	strcat(mensaje, separador);
	strcat(mensaje,	valor);
	enviarMensajeConPortocolo(umc,mensaje,CODIGO_ASIGNACION_UMC);
}

void imprimir(t_valor_variable valor)
{
	printf("Imprimir %d \n", valor);
	char mensaje[6];
	sprintf(mensaje,"%d", valor);
	enviarMensajeConProtocolo(nucleo,mensaje, CODIGO_IMPRESION);
}

void imprimirTexto(char* texto) { 
	printf("ImprimirTexto: %s \n", texto);
	enviarMensajeConProtocolo(nucleo,texto, CODIGO_IMPRESION);
}

void finalizar() {
	printf("Finalizado \n");
	char* char_pcb = toStringPCB(pcb);
	finalizado = 1;
	enviarMensajeConProtocolo(nucleo, char_pcb, CODIGO_FINALIZACION);
}

void llamadasSinRetorno(char* texto) { // nose como hacer esto
	if ( (strcmp(_string_trim(texto),"begin") == 0) || (strcmp(_string_trim(texto),"begin\n") == 0) ){
		printf("Inicio de Programa\n");
		return;
	}
    printf("Llamada a la funcion: %s \n", texto);
    saltoDeLinea(0,texto);
}

AnSISOP_funciones functions = {
		.AnSISOP_definirVariable		= definirVariable,
		.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable,
		.AnSISOP_dereferenciar			= dereferenciar,
		.AnSISOP_asignar				= asignar,
		.AnSISOP_imprimir				= imprimir,
		.AnSISOP_imprimirTexto			= imprimirTexto,
		.AnSISOP_finalizar              = finalizar,
		.AnSISOP_llamarSinRetorno       = llamadasSinRetorno,
};
AnSISOP_kernel kernel_functions = {};
//---------------------------------------

void parsear(char* instruccion){
	analizadorLinea(strdup(instruccion), &functions, &kernel_functions);
}



