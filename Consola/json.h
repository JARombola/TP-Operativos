#include <string.h>
#include <stdio.h>

char* toJson(FILE* archivo);
void filtrar(char* linea);
void eliminarComentarios(char* linea);
void eliminarSaltosDeLinea(char* linea);
void sacarEspacios(char* linea);
void buscar(char* archivo, char* key, char* valor);

char* toJson(FILE* archivo){
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
	int k =0;
	char valor[200];
	for (i=0; i<=longitud; i++){
		if ((archivo[i]==key[0]) && (archivo[i+1]==key[1]) && (archivo[i+2]==':')){
			for (j=i+3; archivo[j]!='\n';j++){
				valor[k] = archivo[j];
				k++;
			}
		}
	}
	valor[k] = '\0';
	strcpy(value,valor);
}
