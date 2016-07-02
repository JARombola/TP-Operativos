#ifndef JSON_H_
#define JSON_H_

#include <string.h>
#include <stdio.h>
#include <commons/collections/queue.h>
#include <parser/metadata_program.h>
#include <commons/string.h>


typedef struct{
	int pag;
	int off;
	int tamanio;
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

void copiar(char*, char*, int);
void filtrar(char* linea);
void eliminarComentarios(char* linea);
void eliminarSaltosDeLinea(char* linea);
char* toSubString(char* string, int inicio, int fin);
void sacarEspacios(char* linea);
void buscar(char* archivo, char* key, char* valor);
void invertir(char* palabra);
t_intructions* valorInstruccionMeta(char* char_meta, int tamanio);
u_int32_t valorInstruccion(char * char_meta,int subindice,int indice);

char* toStringInstrucciones(t_intructions* instrucciones, t_size tamanio);
t_intructions* fromStringInstrucciones(char* char_instrucciones, t_size tamanio);

char* toStringMetadata(t_metadata_program meta);
t_metadata_program fromStringMetadata(char* char_meta);

PCB* fromStringPCB(char* char_pcb);
char* toStringPCB(PCB pcb);

char* toStringList(t_list* lista, char simbol);
t_list* fromStringList(char* char_list, char simbol);

char* valorStringMetadata(char *char_meta);
u_int32_t valorMetadata(char*char_meta,int indice);

char* toStringInt(int numero);

char* toJsonArchivo(FILE* archivo);

Stack* fromStringStack(char* char_stack);
char* toStringStack(Stack stack);

t_list* fromStringListStack(char* char_stack);
char* toStringListStack(t_list* lista_stack);

char* toStringListPagina(t_list* list_page);
t_list* fromStringListPage(char* char_page);

char* toStringPagina(Pagina page);
Pagina* fromStringPagina(char* char_page);

char* toStringListVariables(t_list* lista);
t_list* fromStringListVariables(char* char_list);

char* toStringVariable(Variable variable);
Variable* fromStringVariable(char* char_variable);


void liberarPCBPuntero(PCB* pcb);
void liberarPCB(PCB pcb);
void liberarStack(Stack* stack);  //Private xD

#endif								
