#include "json.h"

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


char* toStringInstruccion(t_intructions instruccion){
	char* char_instruccion =  malloc(10*sizeof(char));
	char barra[2] = "/";
	strcpy(char_instruccion, barra);
	char num[10];
	sprintf(num,"%d!", instruccion.start);
	strcat(char_instruccion,num);
	sprintf(num,"%d!", instruccion.offset);
	strcat(char_instruccion,num);
	return char_instruccion;
}

char* toStringInstrucciones(t_intructions* instrucciones, t_size tamanio){
	char* char_instrucciones= malloc(10*tamanio*sizeof(char)) ;
	char barra[2] = "&";
	strcpy(char_instrucciones,barra);
	char* char_instruccion;
	int i;
	for (i = 0 ; i< tamanio; i++){
		char_instruccion = toStringInstruccion(instrucciones[i]);
		strcat(char_instrucciones, char_instruccion);
	}
	char_instrucciones[(strlen(char_instrucciones))] = '\0';
	return char_instrucciones;
}

char* toStringMetadata(t_metadata_program meta){
	char* char_meta = malloc((meta.instrucciones_size*10)+30+(strlen(meta.etiquetas)) *sizeof(char));
	char num[10];
	sprintf(num,"%d&", meta.instruccion_inicio);
	strcpy(char_meta,num);
	sprintf(num,"%d&", meta.instrucciones_size);
	strcat(char_meta,num);
	sprintf(num,"%d&", meta.etiquetas_size);
	strcat(char_meta,num);
	sprintf(num,"%d&", meta.cantidad_de_funciones);
	strcat(char_meta,num);
	sprintf(num,"%d&", meta.cantidad_de_etiquetas);
	strcat(char_meta,num);
	strcat(char_meta,meta.etiquetas);

	char* char_instrucciones = (toStringInstrucciones(meta.instrucciones_serializado,meta.instrucciones_size));
	strcat(char_meta, char_instrucciones);
	free(char_instrucciones);
	return char_meta;
}

t_metadata_program fromStringMetadata(char* char_meta){
	t_metadata_program meta;
		meta.instruccion_inicio = valorMetadata(char_meta,0);
		meta.instrucciones_size = valorMetadata(char_meta,1);
		meta.etiquetas_size = valorMetadata(char_meta,2);
		meta.cantidad_de_funciones = valorMetadata(char_meta,3);
		meta.cantidad_de_etiquetas = valorMetadata(char_meta,4);
		meta.etiquetas = valorStringMetadata(char_meta);
		meta.instrucciones_serializado = valorInstruccionMeta(char_meta,meta.instrucciones_size);
	return meta;
}

u_int32_t valorMetadata(char*char_meta,int indice){
	int cont = 0;
	int i = 0;
	int key = 0;
	int subkey;
	while(cont<= indice){
		subkey = key;
		if (char_meta[i]=='&'){
			cont++;
			key = i;
		}
		i++;
	}
	char* aux = malloc((key)*sizeof(char));
	strcpy(aux,char_meta);
	aux[key] = '\0';
	invertir(aux);
	int aux_int = atoi(aux);
	sprintf(aux,"%d", aux_int);
	aux[key-subkey] = '\0';
	invertir(aux);
	return atoi(aux);
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
char* valorStringMetadata(char *char_meta){
	int i;
	int key =0;
	int subindice;
	int indice=0;

	for (i=0;key<=5;i++){
		subindice= indice;
		if (char_meta[i]=='&'){
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
	return atoi(aux);
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
			}
		}
	}

}
