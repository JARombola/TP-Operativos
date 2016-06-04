#include "parser.h"

t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);
	Variable* var = crearVariable(variable);
	sumarEnLasVariables(var);
	return  (int)var;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable) {
	printf("Obtener posicion de %c\n", variable);
	Stack* stack = obtenerStack();
	t_list* variables = stack->vars;
	Variable* var;
	int i;
	for(i = 0; i< list_size(variables); i++){
		var = (Variable*) list_get(variables,i);
		if ( var->id == variable  ){
			return (int)&(var->pagina);
		}
	}
	return -1;
}

t_valor_variable dereferenciar(t_puntero pagina) {
	Pagina* pag = (Pagina*) pagina;
	enviarMensajeUMCConsulta(pag->pag,pag->off,pag->tamanio,pcb.id);
	return atoi(esperarRespuesta(umc));
}

void asignar(t_puntero pagina, t_valor_variable valor) {
	Pagina* pag = (Pagina*) pagina;
	enviarMensajeUMCAsignacion(pag->pag,pag->off,pag->tamanio,pcb.id,valor);
}

t_valor_variable obtenerValorCompartida(t_nombre_compartida	variable){
	enviarMensajeNucleoConsulta(variable);
	return atoi(esperarRespuesta(nucleo));
}

t_valor_variable asignarValorCompartida(t_nombre_compartida	variable, t_valor_variable valor){
	enviarMensajeNucleoAsignacion(variable,valor);
	return atoi(esperarRespuesta(nucleo));
}

t_puntero_instruccion irAlLabel(t_nombre_etiqueta etiqueta){
	return metadata_buscar_etiqueta(etiqueta,pcb.indices.etiquetas,pcb.indices.etiquetas_size);
}

void llamarConRetorno(t_nombre_etiqueta	etiqueta, t_puntero	donde_retornar){
	Stack* stack = malloc(sizeof(Stack));
	stack->retPos = donde_retornar;
	stack->vars = list_create();
	Pagina pag = obtenerPagDisponible();
	Pagina* pagina = malloc(sizeof(Pagina));
	pagina = &pag;
	list_add(stack->vars,pagina);
	list_add(pcb.stack,stack);
}
///hasta ak por ahora
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

//-------------------------------------FUNCIONES AUXILIARES-------------------------------------------

Variable* crearVariable(char variable){
	Variable* var = malloc(sizeof(Variable));
	var->id = variable;
	var->pagina = obtenerPagDisponible();
	return var;
}

Pagina obtenerPagDisponible(){
	Stack* stackActual = obtenerStack();
	int cantidadDeVariables = list_size(stackActual->vars)-1;
	Pagina pagina;
	if (cantidadDeVariables == 0){
		pagina.pag = pcb.paginas_codigo+1;
		pagina.off = 0;
	}else{
		Variable* ultimaVariable = list_get(stackActual->vars, cantidadDeVariables);
		if ((ultimaVariable->pagina.off+ultimaVariable->pagina.tamanio+4)<=TAMANIO_PAGINA){
			pagina.pag = ultimaVariable->pagina.pag;
			pagina.off = ultimaVariable->pagina.off+4;
		}else{
			pagina.pag = ultimaVariable->pagina.pag+1;
			pagina.off = 0;
		}
	}
	pagina.tamanio = 4;
	return pagina;
}

void sumarEnLasVariables(Variable* var){
	Stack* stackActual = obtenerStack();
	t_list* variables = stackActual->vars;
	list_add(variables,var);
}

Stack* obtenerStack(){
	int tamanioStack = list_size(pcb.stack)-1;
	return (list_get(pcb.stack,tamanioStack));
}




