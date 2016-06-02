#include "parser.h"

t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);
	Vars var = crearVariable(variable);
	sumarEnLasVariables(&var);
	return  &(var);
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable) {
	printf("Obtener posicion de %c\n", variable);
	t_list* variables = list_get(pcb.stack,(list_size(pcb.stack))-1);
	Vars* var;
	int i;
	for(i = 0; i< list_size(variables)-1; i++){
		var = (Vars*) list_get(variables,i);
		if ( var->id == variable  ){
			return (var);
		}
	}
	Pagina pagina;
	pagina.pag = -1;
	var->pag = pagina;
	var->id = variable;
	return (var);
}

t_valor_variable dereferenciar(t_puntero var) {
	Vars* variable =var;
	int valor;
	char mensaje[50];
	if (variable->pag.pag == -1){
		mensaje[0] = variable->id;
		mensaje[1] = '\0';
		enviarMensajeConProtocolo(nucleo, mensaje, CODIGO_DESREFERENCIA_NUCLEO);
		valor = atoi(esperarRespuesta(nucleo));
	}else{
		strcat(mensaje, header(variable->pag.pag));
		strcat(mensaje, header(variable->pag.off));
		enviarMensajeConProtocolo(umc,mensaje,CODIGO_DESREFERENCIA_UMC);
		valor = atoi(esperarRespuesta(umc));
	}
	return valor;
}

void asignar(t_puntero var, t_valor_variable valor) {
	char mensaje[100];
	Vars* variable = (Vars*)var;
	if (variable->pag.pag == -1){
		mensaje[0] = variable->id;
		mensaje[1] = '\0';
		strcat(mensaje,header(valor));
		enviarMensajeConProtocolo(nucleo, mensaje, CODIGO_ASIGNACION_NUCLEO);
		return;
	}
	strcpy(mensaje, header(pcb.id) );
	strcat(mensaje, header(variable->pag.pag));
	strcat(mensaje, header(variable->pag.off));
	strcat(mensaje,	header(valor));
	enviarMensajeConProtocolo(umc,mensaje,CODIGO_ASIGNACION_UMC);
}

void imprimir(t_valor_variable valor){
	printf("Imprimir %d \n", valor);
	char* mensaje = header(valor);
	enviarMensajeConProtocolo(nucleo,mensaje, CODIGO_IMPRESION);
}

void imprimirTexto(char* texto) {
	printf("ImprimirTexto: %s \n", texto);
	enviarMensajeConProtocolo(nucleo,texto, CODIGO_IMPRESION);
}

void finalizar() {
	printf("Finalizado \n");
	int tamanioStack = list_size(pcb.stack);
	list_remove(pcb.stack,tamanioStack-1);
	if (tamanioStack == 1){
		finalizado = 1;
	}
}

void llamadasSinRetorno(char* texto){
	if ( (strcmp(_string_trim(texto),"begin") == 0) || (strcmp(_string_trim(texto),"begin\n") == 0) ){
		printf("Inicio de Programa\n");
		Stack* stack;
		stack->args = list_create();
		stack->vars = list_create();
		list_add(pcb.stack, stack);
		return;
	}
    printf("Llamada a la funcion: %s \n", texto);
    saltoDeLinea(0,texto);
}

//-------------------------------------FUNCIONES AUXILIARES-------------------------------------------

Vars crearVariable(char variable){
	Vars var;
	var.id = variable;
	var.pag = obtenerPagDisponible();
	return var;
}

Pagina obtenerPagDisponible(){
	int tamanioStack = list_size(pcb.stack);
	Stack* stackActual;
	stackActual = (list_get(pcb.stack, tamanioStack-1));

	int cantidadDeVariables = list_size(stackActual->vars);

	if (cantidadDeVariables == 0){
		return fromIntegerPagina(pcb.indice_stack);
	}

	Vars* ultimaVariable = list_get(stackActual->vars, cantidadDeVariables-1);
	int indiceDeVariable = toIntegerPagina(ultimaVariable->pag);
	indiceDeVariable = indiceDeVariable +4 ;

	return fromIntegerPagina(indiceDeVariable);
}

Pagina fromIntegerPagina(int int_pagina){
	Pagina pagina;
	pagina.pag = int_pagina/TAMANIO_PAGINA;
	pagina.off = int_pagina - (pagina.pag * TAMANIO_PAGINA);
	pagina.size = 4;
	return pagina;
}

int toIntegerPagina(Pagina pagina){
	return ((pagina.pag * TAMANIO_PAGINA)+pagina.off);
}

void sumarEnLasVariables(Vars* var){
	int tamanioStack = list_size(pcb.stack);
	Stack* stackActual;
	stackActual = (list_get(pcb.stack, tamanioStack-1));

	t_list* variables = stackActual->vars;
	list_add(variables,var);
}
void saltoDeLinea(int cantidad, char* nombre){
	int a = 4;
}




