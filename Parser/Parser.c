
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <parser/parser.h>

static const int CONTENIDO_VARIABLE = 20;
static const int POSICION_MEMORIA = 0x10;


t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);
	return POSICION_MEMORIA;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable) {
	printf("Obtener posicion de %c\n", variable);
	return POSICION_MEMORIA;
}

t_valor_variable dereferenciar(t_puntero puntero) {
	printf("Dereferenciar %d y su valor es: [VALOR]\n", puntero);
	return CONTENIDO_VARIABLE;
}

void asignar(t_puntero puntero, t_valor_variable variable) {
	printf("Asignando en %d el valor [VALOR]\n", puntero);
}

void imprimir(t_valor_variable valor)
{
	printf("Imprimir %d \n", valor);
}

void imprimirTexto(char* texto) {
	printf("ImprimirTexto: %s", texto);
}

void finalizar() {
	printf("Fin de Programa\n");
}
void llamasSinRetorno(char* texto) {
	if ( (strcmp(_string_trim(texto),"begin") == 0) || (strcmp(_string_trim(texto),"begin\n") == 0) ){
		printf("Inicio de Programa\n");
		return;
	}
    if (texto[0] == '#'){
    	printf("Comentario: %s \n", texto);
    	return;
    }
    printf("Llamada a la funcion: %s \n", texto);
}

AnSISOP_funciones functions = {
		.AnSISOP_definirVariable		= definirVariable,
		.AnSISOP_obtenerPosicionVariable= obtenerPosicionVariable,
		.AnSISOP_dereferenciar			= dereferenciar,
		.AnSISOP_asignar				= asignar,
		.AnSISOP_imprimir				= imprimir,
		.AnSISOP_imprimirTexto			= imprimirTexto,
		.AnSISOP_finalizar              = finalizar,
		.AnSISOP_llamarSinRetorno       = llamasSinRetorno,
		/*.AnSISOP_obtenerValorCompartida
		   .AnSISOP_asignarValorCompartida
		   .AnSISOP_irAlLabel
		   AnSISOP_llamarConRetorno
		   AnSISOP_retornar
		   AnSISOP_entradaSalida*/

};
AnSISOP_kernel kernel_functions = {};
//---------------------------------------

void analizarParser(char* instruccion){
	analizadorLinea(strdup(instruccion), &functions, &kernel_functions);
}



