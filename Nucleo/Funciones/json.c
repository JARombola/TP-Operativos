#include "json.h"

/*
 * Simbolos no validos como separador: '?', '#','&',$','/','¿','-','_','*','+'
 */

char* toStringInstrucciones(t_intructions* instrucciones, t_size tamanio){
	char* char_instrucciones=string_new();
	int i;
	char* start,*offset;
	for (i = 0 ; i< tamanio; i++){
		start=toStringInt(instrucciones->start);
		offset=toStringInt(instrucciones->offset);
		string_append(&char_instrucciones, start);
		string_append(&char_instrucciones, offset);
		free(start);
		free(offset);
		instrucciones++;
	}
	return char_instrucciones;
}

char* toStringMetadata(t_metadata_program meta){
	char* char_meta=string_new();
	char* inicio=toStringInt(meta.instruccion_inicio);
	char* instrSize=toStringInt(meta.instrucciones_size);
	char* etiqSize=toStringInt(meta.etiquetas_size);
	char* cantFunc=toStringInt(meta.cantidad_de_funciones);
	char* cantEtiq=toStringInt(meta.cantidad_de_etiquetas);
	string_append(&char_meta,inicio);
	string_append(&char_meta,instrSize);
	string_append(&char_meta,etiqSize);
	string_append(&char_meta,cantFunc);
	string_append(&char_meta,cantEtiq);
	free(inicio);
	free(instrSize);
	free(etiqSize);
	free(cantFunc);
	free(cantEtiq);
	int i;
	for(i=0;i<meta.etiquetas_size;i++){
		if(meta.etiquetas[i]=='\0'){
			meta.etiquetas[i]='@';
		}
	}
	if(meta.etiquetas_size){
		meta.etiquetas[meta.etiquetas_size]='\0';
		string_append(&char_meta,meta.etiquetas);
	}
	char* char_instrucciones=toStringInstrucciones(meta.instrucciones_serializado,meta.instrucciones_size);
	string_append(&char_meta, char_instrucciones);
	free(char_instrucciones);
	return char_meta;
}

char* toStringPCB(PCB pcb){
	char* char_pcb=string_new();
	char* char_id;
	char_id = toStringInt(pcb.id);
	char *char_metadata;
	char_metadata = toStringMetadata(pcb.indices);
	char* char_paginas_codigo;
	char_paginas_codigo = toStringInt(pcb.paginas_codigo);
	char* char_pc;
	char_pc = toStringInt(pcb.pc);
	char* char_stack;
	char_stack = toStringListStack(pcb.stack);
	string_append(&char_pcb,char_id);
	char* aux = toStringInt(strlen(char_metadata));
	string_append(&char_pcb, aux);
	string_append(&char_pcb,char_metadata);
	string_append(&char_pcb, char_paginas_codigo);
	string_append(&char_pcb, char_pc);
	string_append(&char_pcb,char_stack);
	free (char_id);
	free(aux);
	free(char_metadata);
	free(char_paginas_codigo);
	free(char_pc);
	free(char_stack);
	return char_pcb;
}

char* toSubString(char* string, int inicio, int fin){
	if (fin<inicio){
		char* r = string_new();
		string_append(&r,"\0");
		return r;
	}
	return (string_substring(string,inicio,1+fin-inicio));
}

char* toStringInt(int numero){
	char* asd=string_new();
	char* num_char = string_itoa(numero);
	num_char = string_reverse(num_char);
	string_append(&asd,num_char);
	free(num_char);
	string_append(&asd,"0000");
	char* longitud=string_substring(asd,0,4);
	longitud=string_reverse(longitud);
	return longitud;
}

char* toStringListStack(t_list* lista_stack){
	int i;
	char* char_lista_stack = string_new();
	string_append(&char_lista_stack,"\0");
//	Stack* stack;
	char barra[2] = "-";
//	char* char_stack;
	for (i=0; i<list_size(lista_stack);i++){
		Stack* stack = list_get(lista_stack,i);
		char* char_stack = toStringStack(*stack);
		string_append(&char_lista_stack,char_stack);
		string_append(&char_lista_stack,barra);
		free(char_stack);
	}
	return char_lista_stack;
}

char* toStringStack(Stack stack){
	char* char_stack=string_new();
	char* char_args = toStringListVariables(stack.args);
	char* char_retpos = toStringInt(stack.retPos);
	char* char_ret_var = toStringPagina(stack.retVar);
	char* char_var_list = toStringListVariables(stack.vars);
	char* tamanioListaPagina = toStringInt(strlen(char_args));
	string_append_with_format(&char_stack,"%s%s%s%s%s",tamanioListaPagina,char_args,char_retpos,char_ret_var,char_var_list);
	free(char_args);
	free(tamanioListaPagina);
	free(char_retpos);
	free(char_ret_var);
	free(char_var_list);
	return char_stack;
}

char* toStringListPagina(t_list* lista_page){
	int i;
	char* char_lista_page = string_new();
	//Pagina* page;
	//char* char_page;
	for (i=0; i<list_size(lista_page);i++){
		Pagina* page = list_get(lista_page,i);
		char* char_page = toStringPagina(*page);
		string_append(&char_lista_page,char_page);
		free(char_page);
	}
	return char_lista_page;

}

char* toStringPagina(Pagina page){
	char* char_page= string_new();
	char* off = toStringInt(page.off);
	char* pag = toStringInt(page.pag);
	char* size = toStringInt(page.tamanio);
	string_append_with_format(&char_page,"%s%s%s",pag,off,size);
	free(pag);
	free(off);
	free(size);
	return char_page;
}

char* toStringListVariables(t_list* lista){
	int i;
	char* char_lista_var = string_new();
	string_append(&char_lista_var,"\0");
	//Variable* variable;
	//char* char_var;
	for (i=0; i<list_size(lista);i++){
		Variable* variable = list_get(lista,i);
		char* char_var = toStringVariable(*variable);
		string_append(&char_lista_var,char_var);
		free(char_var);
	}
	return char_lista_var;
}

char* toStringVariable(Variable variable){
	char* pagina = toStringPagina(variable.pagina);
	char* char_variable = string_new();
	char id[2];
	id[0] = variable.id;
	id[1] = '\0';
	string_append(&char_variable,id);
	string_append(&char_variable,pagina);
	free(pagina);
	return char_variable;
}


void liberarPCB(PCB pcb){
	int i;
	Stack* stack;
	for (i=0; i<list_size(pcb.stack);i++){
		stack = list_get(pcb.stack,i);
		liberarStack(stack);
		free(stack);
	}
	list_destroy(pcb.stack);
}

void liberarStack(Stack* stack){
	int i;
	Pagina* pagina;
	//list_destroy_and_destroy_elements(stack->args,free);
	//list_destroy_and_destroy_elements(stack->vars,free);
	for(i=0; i<list_size(stack->args);i++){
		pagina = list_get(stack->args,i);
		free(pagina);
	}
	list_destroy(stack->args);
	Variable* var;
	for(i=0; i<list_size(stack->vars);i++){
		var = list_get(stack->vars,i);
		free(var);
	}
	list_destroy(stack->vars);
}

