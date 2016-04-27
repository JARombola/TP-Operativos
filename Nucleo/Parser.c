/*
 * Parser.c
 *
 *  Created on: 24/4/2016
 *      Author: utnso
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <parser/parser.h>
#include <parser/metadata_program.h>

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

void imprimir()//(t_valor_variable valor)
{
	printf("Imprimir [VALOR DE LA VARIABLE]\n");
}

void imprimirTexto(char* texto) {
	printf("ImprimirTexto: %s", texto);
}

void finalizar() {
	printf("Fin de Programa\n");
}
void llamasSinRetorno(char* texto) {
	if ( (strcmp(texto,"begin") == 0) || (strcmp(texto,"begin\n") == 0) ){
		printf("Inicio de Programa\n");
		return;
	}
    if (texto[0]== '#'){
    	//printf("Comentario: %s \n", texto);
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
		/*.AnSISOP_obtenerValorCompartida
		   .AnSISOP_asignarValorCompartida
		   .AnSISOP_irAlLabel
		   AnSISOP_llamarConRetorno
		   AnSISOP_retornar
		   AnSISOP_entradaSalida
        NO VAAA   .AnSISOP_llamarSinRetorno       = llamasSinRetorno,*/
};
AnSISOP_kernel kernel_functions = { /*AnSISOP_wait
AnSISOP_signal*/};
//---------------------------------------

void analizarParser(char* instruccion){
	analizadorLinea(strdup(instruccion), &functions, &kernel_functions);
}
int posicionPrimerCaracter(char palabra[]){
	int i;
	for(i=0;palabra[i]=='\t';i++);
	return i;
}
char* armarLiteral(FILE* archivoCodigo) {		//Copia el codigo ansisop
	char* unaLinea, *codigoTotal;
	codigoTotal = string_new();
	while (!feof(archivoCodigo)) {
		fgets(unaLinea, 200, archivoCodigo);
//		strcat(unaLinea,"\0");
		string_append(&codigoTotal,unaLinea);
	}
	return codigoTotal;
}


int main(int argc, char* argv[]){
/*	if(argc !=2){
		printf("Error, parametros ingresadon son erroneos, solo ingrese el nombre del archivo ansisop");
		return -1;
	}*/
	char linea[200],*literal;
	printf("Abriendo archivo \n");
	FILE *ansisop =	fopen("/home/utnso/tp-2016-1c-CodeBreakers/Nucleo/ansisop.txt", "r");
	FILE *ansisop2 =	fopen("/home/utnso/tp-2016-1c-CodeBreakers/Nucleo/ansisop.txt", "r");
	if (ansisop == NULL){
		printf("Error al abrir el archivo, verifique la existencia del mismo \n");
		return -2;
	}else{printf("ok, abierto\n");}
	literal=armarLiteral(ansisop);
	t_metadata_program* programa=metadata_desde_literal(literal);
	printf("%d\n",programa->instrucciones_size);
	fgets(linea, 200, ansisop2);		//Afuera para saltear el begin
	while (!feof(ansisop2)) {
//		linea=malloc(20);
		fgets(linea,200,ansisop2);
		if (linea[0] != '#') {
					printf("!!!!!!!!!!!!!!!!!!!!LINEAAAA:%s\n", _string_trim(linea));
					analizarParser(linea);}
	}
	/*
 	printf("Lineas: %d\n",programa->instruccion_inicio);
	char linea[200];
	fgets(linea, 200, ansisop);		//Afuera del while Para Saltear el #!
	fgets(linea, 200, ansisop);		//Afuera para saltear el begin
	while (!feof(ansisop)) {
		fgets(linea, 200, ansisop);
		int i = posicionPrimerCaracter(linea);
		if (linea[0] != '#') {
			printf("!!!!!!!!!!!!!!!!!!!!LINEAAAA:%s\n", _string_trim(linea));
			analizarParser(linea);}
	}*/
	fclose(ansisop);
	printf("Archivo cerrado\n");
	return 0;
}




