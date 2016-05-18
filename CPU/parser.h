
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <parser/parser.h>

static const int CONTENIDO_VARIABLE = 20;
static const int POSICION_MEMORIA = 0x10;

void parsear(char* instruccion);

int nucleo;
int umc;
int pcb;
int finalizado;

t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);
	char mensaje[100] = "Espacio Libre, id:" + pcb.id;
	enviarMensaje(umc, mensaje);
	char* resp = esperarRespuesta(umc);
	char pagina [54];
	char desplazamiento [10];
	parsearRespuesta(resp, pagina, desplazamiento);
	// ponerlo en el pbc
	strcat(pagina,desplazamiento);
	return  pagina;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable) {
	printf("Obtener posicion de %c\n", variable);
	char* pagina[54] = //pcb.pagina + pcb.desplazamiento
	return POSICION_MEMORIA;
}

t_valor_variable dereferenciar(t_puntero puntero) { //nose que es esto
	printf("Dereferenciar %d y su valor es: [VALOR]\n", puntero);
	return CONTENIDO_VARIABLE;
}

void asignar(t_puntero puntero, t_valor_variable variable) {
	printf("Asignando en %d el valor [VALOR]\n", puntero);
	char mensaje[100] = "Asignar:" +/*pcb.pagina*/ +"/"+/*pcb.desplazamiento*/+"/"+ valor;
	enviarMensaje(umc,mensaje);
}

void imprimir(t_valor_variable valor)
{
	printf("Imprimir %d \n", valor);
	char mensaje[100] = "Imprimi:" + itoa(valor);
	enviarMensaje(nucleo,mensaje);
}

void imprimirTexto(char* texto) { 
	printf("ImprimirTexto: %s \n", texto);
	char mensaje[100] = "Imprimi:" + texto;
	enviarMensaje(nucleo,mensaje);
}

void finalizar() {
	printf("Finalizado \n");
	finalizado = 1;
}
void llamasSinRetorno(char* texto) {
	if ( (strcmp(_string_trim(texto),"begin") == 0) || (strcmp(_string_trim(texto),"begin\n") == 0) ){
		printf("Inicio de Programa\n");
		return;
	}
    printf("Llamada a la funcion: %s \n", texto);
    saltoDeLinea(); // VER AK COMO SE HACER
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
};
AnSISOP_kernel kernel_functions = {};
//---------------------------------------

void parsear(char* instruccion){
	analizadorLinea(strdup(instruccion), &functions, &kernel_functions);
}



