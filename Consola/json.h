#include <string.h>
#include <stdio.h>

char* toJson(FILE* archivo);
void filtrar(char* linea);
void eliminarComentarios(char* linea);
void eliminarSaltosDeLinea(char* linea);
void sacarEspacios(char* linea);

char* toJson(FILE* archivo){
	char linea[200];
	char ansisop[200];
	ansisop[0]='\0';
	while (!feof(archivo)){
		fgets(linea,200,archivo);
        filtrar(linea);
		strcat(ansisop,linea);
	}
	fclose(archivo);
	return ansisop;
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