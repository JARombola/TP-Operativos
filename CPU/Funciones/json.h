#ifndef JSON_H_
#define JSON_H_

#include <string.h>
#include <stdio.h>
#include <commons/collections/queue.h>
#include <parser/metadata_program.h>

char* toJsonArchivo(FILE* archivo);
void filtrar(char* linea);
void eliminarComentarios(char* linea);
void eliminarSaltosDeLinea(char* linea);
void sacarEspacios(char* linea);
void buscar(char* archivo, char* key, char* valor);
char* toStringInstruccion(t_intructions instruccion);
char* toStringInstrucciones(t_intructions* instrucciones, t_size tamanio);
char* toStringMetadata(t_metadata_program meta);
t_metadata_program fromStringMetadata(char* char_meta);
u_int32_t valorMetadata(char*char_meta,int indice);
void invertir(char* palabra);
char* valorStringMetadata(char *char_meta);
t_intructions* valorInstruccionMeta(char* char_meta, int tamanio);
u_int32_t valorInstruccion(char * char_meta,int subindice,int indice);
//PCB fromStringPCB(char* char_pcb);
//char* toStringPCB(PCB pcb);
char* toStringList(t_list* lista, char simbol);
t_list* fromStringList(char* char_list, char simbol);

#endif								
