#ifndef JSON_H_
#define JSON_H_

#include <string.h>
#include <stdio.h>
#include <commons/collections/queue.h>
#include <parser/metadata_program.h>

typedef struct{
	int pag;
	int off;
	int size;
}Pagina;

typedef struct{
	char id;
	Pagina pagina;
}Variable;

typedef struct{
	t_list* args; //Lista de Pagina
	t_list* vars; //Lista de Variable
	int retPos; //Posición del índice de código donde se debe retornar al finalizar la ejecución de la función
	Pagina retVar; // Posición de memoria donde se debe	almacenar el resultado de la función provisto por la sentencia RETURN
}Stack;

typedef struct{
	int id;
	int pc; //Número de la próxima instrucción del Programa que se debe ejecutar
	int paginas_codigo;
	t_metadata_program indices; // Índice de código + Índice de	etiquetas
	t_list* stack; // Lista de Stack
}PCB;


char* toJsonArchivo(FILE* archivo);
void filtrar(char* linea);
void eliminarComentarios(char* linea);
void eliminarSaltosDeLinea(char* linea);
void sacarEspacios(char* linea);
void buscar(char* archivo, char* key, char* valor);

char* toStringInstruccion(t_intructions instruccion, char separador);

char* toStringInstrucciones(t_intructions* instrucciones, t_size tamanio,char separador);

char* toStringMetadata(t_metadata_program meta, char separador);
t_metadata_program fromStringMetadata(char* char_meta,char separador);

u_int32_t valorMetadata(char*char_meta,int indice, char separador);
void invertir(char* palabra);
char* valorStringMetadata(char *char_meta, char separador);
t_intructions* valorInstruccionMeta(char* char_meta, int tamanio);
u_int32_t valorInstruccion(char * char_meta,int subindice,int indice);
PCB fromStringPCB(char* char_pcb);
char* toStringPCB(PCB pcb);
char* toStringList(t_list* lista, char simbol);
t_list* fromStringList(char* char_list, char simbol);

#endif								
