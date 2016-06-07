#include "json.h"

/*
 * Simbolos no validos como separador: '?', '#', '!','&',$','/','¿','-','_','*','+'
 */

char* toJsonArchivo(FILE* archivo){
	char linea[200];
	char* ansisop = malloc(200*sizeof(char));
	char* final = malloc(1);
	ansisop[0]='\0';
	while (!feof(archivo)){
		fgets(linea,200,archivo);
        filtrar(linea);
		strcat(ansisop,linea);
		final = realloc(final,(strlen(ansisop)+1)* sizeof(char));
		strcpy(final,ansisop);
		ansisop = realloc(ansisop, (strlen(ansisop)+200)* sizeof(char));
		strcpy(ansisop, final);
	}
	free(ansisop);
	fclose(archivo);
	return final;
}

void filtrar(char* linea){
	sacarEspacios(linea);
	eliminarSaltosDeLinea(linea);
	eliminarComentarios(linea);
}

void eliminarComentarios(char* linea){
	if (linea[0]=='#'){
		linea[0]='\0';
	}
}

void eliminarSaltosDeLinea(char* linea){
	if (linea[0]=='\n'){
		linea[0]='\0';
	}
}

void sacarEspacios(char* linea){
	char lineaAux[200];
	int i;
	int j = 0;
	for (i=0; linea[i]!='\0';i++){
		if (linea[i] != ' '){
			lineaAux[j] = linea[i];
			j++;
		}
	}
	lineaAux[j] = '\0';
	strcpy(linea, lineaAux);
}


void buscar(char* archivo, char* key, char*value){
	int longitud = strlen(archivo);
	int i;
	int j;
	int k;
	int l =0;
	char valor[200];
	for (i=0; i<longitud; i++){
		for (j = 0; j < strlen(key); j++){
			if (!(archivo[i+j] == key[j])){
				break;
			}
		}
		if (j == strlen(key)){
			if (archivo[i+j] == '='){
				for (k=i+j+1; archivo[k]!='\n';k++){
					valor[l] = archivo[k];
					l++;
				}
				break;
			}
		}
	}
	valor[l] = '\0';
	strcpy(value,valor);
}


char* toStringInstruccion(t_intructions instruccion,char separador){
	char* char_instruccion =  string_new();
	char barra[2];
	barra[0] = separador;
	barra[1] = '\0';
	string_append(&char_instruccion, barra);
	char num[10];
	sprintf(num,"%d!", instruccion.start);
	string_append(&char_instruccion,num);
	sprintf(num,"%d!", instruccion.offset);
	string_append(&char_instruccion,num);
	return char_instruccion;
}

char* toStringInstrucciones(t_intructions* instrucciones, t_size tamanio){
	char* char_instrucciones=string_new();
	int i;
	for (i = 0 ; i< tamanio; i++){
		string_append(&char_instrucciones, toStringInt(instrucciones->offset));
		string_append(&char_instrucciones, toStringInt(instrucciones->start));
		instrucciones++;
	}
	return char_instrucciones;
}

t_intructions* fromStringInstrucciones(char* char_instrucciones, t_size tamanio){
	t_intructions* instrucciones = malloc(tamanio*sizeof(t_intructions));
	int i;
	for (i=0;i<tamanio;i++){
		instrucciones[i].offset = atoi(string_substring(char_instrucciones,i*8+4,4));
		instrucciones[i].start = atoi(string_substring(char_instrucciones,i*8,4));
	}
	return instrucciones;
}

char* toStringMetadata(t_metadata_program meta, char separador){
	char* char_meta=string_new();
	string_append(&char_meta,toStringInt(meta.instruccion_inicio));
	string_append(&char_meta,toStringInt(meta.instrucciones_size));
	string_append(&char_meta,toStringInt(meta.etiquetas_size));
	string_append(&char_meta,toStringInt(meta.cantidad_de_funciones));
	string_append(&char_meta,toStringInt(meta.cantidad_de_etiquetas));
	if (meta.etiquetas != NULL){
		char* asd=malloc(meta.etiquetas_size);
		memcpy(asd,meta.etiquetas,meta.etiquetas_size);
		string_append(&char_meta,asd);
		t_puntero_instruccion* b=metadata_buscar_etiqueta("perro",asd,meta.etiquetas_size);

			printf("PUNTERO : %d\n",b);
	}
	if (meta.instrucciones_size!=0){
		char* char_instrucciones = (toStringInstrucciones(meta.instrucciones_serializado,meta.instrucciones_size));
		string_append(&char_meta, char_instrucciones);
		free(char_instrucciones);
	}
	return char_meta;
}

t_metadata_program fromStringMetadata(char* char_meta,char separador){
	t_metadata_program meta;
		meta.instruccion_inicio = atoi(toSubString(char_meta,0,3));
		meta.instrucciones_size = atoi(toSubString(char_meta,4,7));
		meta.etiquetas_size = atoi(toSubString(char_meta,8,11));
		meta.cantidad_de_funciones = atoi(toSubString(char_meta,12,15));
		meta.cantidad_de_etiquetas = atoi(toSubString(char_meta,16,19));
		meta.etiquetas = toSubString(char_meta,20,(20+meta.etiquetas_size-1));
		meta.instrucciones_serializado = fromStringInstrucciones(string_substring_from(char_meta,24),meta.instrucciones_size);
	return meta;
}

u_int32_t valorMetadata(char*char_meta,int indice, char separador){
	int cont = 0;
	int i = 0;
	int key = 0;
	int subkey;
	while(cont<= indice){
		subkey = key;
		if (char_meta[i]==separador){
			cont++;
			key = i;
		}
		i++;
	}
	return atoi(toSubString(char_meta,subkey,key));
}

void invertir(char* palabra){
	int longitud = strlen(palabra);
	char temporal;
	int i,j;
	for (i=0,j=longitud-1; i<longitud/2; i++,j--){
		temporal=palabra[i];
		palabra[i]=palabra[j];
		palabra[j]=temporal;
	}
}
char* valorStringMetadata(char *char_meta, char separador){
	int i;
	int key =0;
	int subindice;
	int indice=0;

	for (i=0;key<=5;i++){
		subindice= indice;
		if (char_meta[i]==separador){
			key ++;
			indice = i;
		}
	}
	char* aux = malloc(i*sizeof(char));
	strcpy(aux,char_meta);
	aux[i] = '\0';
	invertir(aux);
	aux[indice-subindice] = '\0';
	invertir(aux);
	aux[indice-subindice-1] = '\0';
	return aux;
}

t_intructions* valorInstruccionMeta(char* char_meta, int tamanio){
	t_intructions* instrucciones = malloc(tamanio*sizeof(t_intructions));
	t_intructions instruccion;
	int i;
	for (i=0; i<tamanio;i++){
		instruccion.offset =  valorInstruccion(char_meta,1,i);
		instruccion.start = valorInstruccion(char_meta,0,i);
		instrucciones[i] = instruccion;
	}
	return instrucciones;
}
u_int32_t valorInstruccion(char * char_meta,int subindice,int indice){
	int i;
	int signo =0;
	for (i=0; signo<=5;i++){
		if (char_meta[i]=='&'){
			signo++;
		}
	}
	signo =0;
	i++;
	int separador=0;
	int key;
	int subkey= i-1;
	for (;;i++){
		if(char_meta[i]=='/'){
			subkey = i;
		}
		if (char_meta[i]=='!'){
			if (separador == ((indice*2)+subindice)){
				key = i;
				break;
			}else{
				subkey = i;
			}
			separador++;
		}
	}
	char* aux = malloc(key*sizeof(char));
	strcpy(aux,char_meta);
	aux[key] = '\0';
	invertir(aux);
	aux[key-subkey-1]='\0';
	invertir(aux);
	int result = atoi(aux);
	free(aux);
	return result;
}

char* toStringList(t_list* lista, char simbol){
	int i;
	char* char_list = malloc(5*sizeof(char));
	char* elemento;
	char barra[2];
	barra[0] = simbol;
	barra[1] = '\0';
	int longitud;
	strcpy(char_list,barra);
	for (i=0; i< list_size(lista);i++){
		elemento = list_get(lista,i);
		longitud = strlen(elemento);
		char_list = realloc(char_list,((strlen(elemento)+longitud+1)*sizeof(char)));
		strcat(char_list,elemento);
		strcat(char_list,barra);
	}
	return char_list;
}

t_list* fromStringList(char* char_list, char simbol){
	t_list* lista;
	lista = list_create();
	char* char_aux;
	int i;
	int subindice =0;
	int indice=0;
	for (i=0; i<strlen(char_list);i++){
		subindice = indice;
		if (char_list[i] == simbol){
			indice = i;
			if (i!=0){
				char_aux = malloc((indice-subindice)*sizeof(char));
				strcpy(char_aux,char_list);
				char_aux[indice] = '\0';
				invertir(char_aux);
				char_aux[indice-subindice-1]='\0';
				invertir(char_aux);
				list_add(lista,char_aux);
				free(char_aux);
			}
		}
	}
	return lista;
}
char* toStringPCB(PCB pcb){
	char* char_pcb=string_new();
	char* char_id;
	char_id = toStringInt(pcb.id);
	char *char_metadata;
	char_metadata = toStringMetadata(pcb.indices,'&');
	char* char_paginas_codigo;
	char_paginas_codigo = toStringInt(pcb.paginas_codigo);
	char* char_pc;
	char_pc = toStringInt(pcb.pc);
	char* char_stack;
	char_stack = toStringListStack(pcb.stack);
	string_append(&char_pcb,char_id);
	string_append(&char_pcb, toStringInt(strlen(char_metadata)));
	string_append(&char_pcb,char_metadata);
	string_append(&char_pcb, char_paginas_codigo);
	string_append(&char_pcb, char_pc);
	string_append(&char_pcb,char_stack);
	free (char_id);
	free(char_metadata);
	free(char_pc);
	free(char_stack);
	return char_pcb;
}
PCB fromStringPCB(char* char_pcb){
	PCB pcb;
	pcb.id = atoi(toSubString(char_pcb,0,3));
	int tamanioMeta = atoi(toSubString(char_pcb,4,7));
	pcb.indices = fromStringMetadata(toSubString(char_pcb,8,(8+tamanioMeta-1)),'&');
	pcb.paginas_codigo = atoi(toSubString(char_pcb,8+tamanioMeta,8+tamanioMeta+3));
	pcb.pc = atoi(toSubString(char_pcb,8+tamanioMeta+4, 8+ tamanioMeta+4 +3));
	char *subString = toSubString(char_pcb,tamanioMeta+16,strlen(char_pcb));
	pcb.stack = fromStringListStack(subString);
	return pcb;
}

char* toSubString(char* string, int inicio, int fin){
	return (string_substring(string,inicio,1+fin-inicio));
}

char* toStringInt(int numero){
	char* longitud=string_new();
	string_append(&longitud,string_reverse(string_itoa(numero)));
	string_append(&longitud,"0000");
	longitud=string_substring(longitud,0,4);
	longitud=string_reverse(longitud);
	return longitud;
}

t_list* fromStringListStack(char* char_stack){
	int i;
	int indice = 0;
	int subIndice;
	t_list* lista_stack = list_create();
	for(i=0; i<strlen(char_stack);i++){
		subIndice = indice;
		if (char_stack[i]=='-'){
			indice = i-1;
			Stack* stack = fromStringStack(toSubString(char_stack,subIndice,indice));
			list_add(lista_stack,stack);
			indice = i+1;
		}
	}
	return lista_stack;
}

char* toStringListStack(t_list* lista_stack){
	int i;
	char* char_lista_stack = malloc(sizeof(char));
	char_lista_stack[0] = '\0';
	Stack* stack;
	char barra[2] = "-";
	char* char_stack;
	for (i=0; i<list_size(lista_stack);i++){
		stack = list_get(lista_stack,i);
		char_stack = toStringStack(*stack);
		char_lista_stack = realloc(char_lista_stack, (strlen(char_lista_stack)+ strlen(char_stack)+2)*sizeof(char));
		strcat(char_lista_stack,char_stack);
		strcat(char_lista_stack,barra);
		free(char_stack);
	}
	return char_lista_stack;
}

char* toStringStack(Stack stack){
	char* char_stack;
	char* char_args = toStringListPagina(stack.args);
	char* char_retpos = toStringInt(stack.retPos);
	char* char_ret_var = toStringPagina(stack.retVar);
	char* char_var_list = toStringListVariables(stack.vars);
	char_stack = malloc((strlen(char_args)+strlen(char_retpos)+strlen(char_ret_var)+strlen(char_var_list)+10)*sizeof(char));
	sprintf(char_stack,"%s_%s_%s_%s_",char_args,char_retpos,char_ret_var,char_var_list);
	free(char_args);
	free(char_retpos);
	free(char_ret_var);
	free(char_var_list);
	return char_stack;
}

Stack* fromStringStack(char* char_stack){
	Stack* stack = malloc(sizeof(Stack));
	int i;
	int indice =0;
	int subIndice;
	int nivel =0;
	char* subString;
	for(i=0; i<strlen(char_stack);i++){
		subIndice = indice;
		if (char_stack[i]=='_'){
			indice = i-1;
			nivel++;
			subString = toSubString(char_stack,subIndice,indice);
			switch (nivel){
			case 1:
				stack->args = fromStringListPage(subString);
				break;
			case 2:
				stack->retPos = atoi(subString);
				break;
			case 3:
				stack->retVar = *fromStringPagina(subString);
				break;
			case 4:
				stack->vars = fromStringListVariables(subString);
				break;
			}
			indice = i+1;
		}
	}
	return stack;
}

char* toStringListPagina(t_list* lista_page){
	int i;
	char* char_lista_page = string_new();
	char_lista_page[0] = '\0';
	Pagina* page;
	char barra[2] = "*";
	char* char_page;
	for (i=0; i<list_size(lista_page);i++){
		page = list_get(lista_page,i);
		char_page = toStringPagina(*page);
		string_append(&char_lista_page,char_page);
		string_append(&char_lista_page,barra);
		free(char_page);
	}
	return char_lista_page;

}

t_list* fromStringListPage(char* char_list_page){
	int i;
	int indice = 0;
	int subIndice;
	char* subString;
	Pagina* pag;
	t_list* lista_page = list_create();
	for(i=0; i<strlen(char_list_page);i++){
		subIndice = indice;
		if (char_list_page[i]=='*'){
			indice = i-1;
			subString = toSubString(char_list_page,subIndice,indice);
			pag = fromStringPagina(subString);
			list_add(lista_page, pag);
			indice = i+1;
		}
	}
	return lista_page;
}

char* toStringPagina(Pagina page){
	char* char_page= malloc(15*sizeof(char));
	char* off = toStringInt(page.off);
	char* pag = toStringInt(page.pag);
	char* size = toStringInt(page.tamanio);
	sprintf(char_page,"%s%s%s",off,pag,size);
	return char_page;
}

Pagina* fromStringPagina(char* char_page){
	Pagina* page = malloc(sizeof(Pagina));
	page->off = atoi(toSubString(char_page,0,3));
	page->pag = atoi(toSubString(char_page,4,7));
	page->tamanio = atoi(toSubString(char_page,8,11));
	return page;
}

char* toStringListVariables(t_list* lista){
	int i;
	char* char_lista_var = string_new();
	char_lista_var[0] = '\0';
	Variable* variable;
	char* char_var;
	char barra[2] = "+";
	for (i=0; i<list_size(lista);i++){
		variable = list_get(lista,i);
		char_var = toStringVariable(*variable);
		string_append(&char_lista_var,char_var);
		string_append(&char_lista_var,barra);
		free(char_var);
	}
	return char_lista_var;
}

t_list* fromStringListVariables(char* char_list){
	int i;
	int indice = 1;
	int subIndice;
	t_list* lista_var = list_create();
	for(i=1; i<strlen(char_list);i++){
		subIndice = indice;
		if (char_list[i]=='+'){
			indice = i-1;
			list_add(lista_var, fromStringVariable(toSubString(char_list,subIndice,indice)));
			indice = i+1;
		}
	}
	free(char_list);
	return lista_var;
}

char* toStringVariable(Variable variable){
	char* pagina = toStringPagina(variable.pagina);
	char* char_variable = string_new();
	strcpy(char_variable,pagina);
	char_variable[strlen(pagina)]=variable.id;
	char_variable[strlen(pagina)+1]='\0';
	free(pagina);
	return char_variable;
}

Variable* fromStringVariable(char* char_variable){
	Variable* variable = malloc(sizeof(Variable));
	variable->pagina = *(fromStringPagina(toSubString(char_variable,0,strlen(char_variable)-2)));
	variable->id = char_variable[strlen(char_variable)-1];
	return variable;
}


