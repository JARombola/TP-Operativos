#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Funciones/json.h"

int main(){
	int i=0;
	FILE* arch = fopen("arch.ansisop","r");
	char* arch_char = toJsonArchivo(arch);
	printf("Longitud %d\n",strlen(arch_char));
	PCB pcb;
	pcb.id = 6;
	pcb.paginas_codigo = 4;
	pcb.pc = 0;
	pcb.stack = list_create();
	Stack* stack = malloc(sizeof(Stack));
	stack->retPos = 4;
	stack->retVar.off = 2;
	stack->retVar.pag = 4;
	stack->retVar.tamanio = 9;
	stack->args = list_create();
	stack->vars = list_create();
	list_add(pcb.stack,stack);
	Stack* stack = malloc(sizeof(Stack));
	stack->retPos = 4;
	stack->retVar.off = 2;
	stack->retVar.pag = 4;
	stack->retVar.tamanio = 9;
	stack->args = list_create();
	stack->vars = list_create();
	list_add(pcb.stack,stack);
	pcb.indices = *(metadata_desde_literal(arch_char));
	/*printf("Repetir Instrucciones\n");
	printf("%d\n",pcb.indices.instrucciones_serializado[0].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[1].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[2].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[3].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[0].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[1].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[2].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[3].offset);
	printf("Hasta Ak\n");*/
	char* pcb_char = toStringPCB(pcb);
/*	printf("Repetir Metadata\n");
	printf("%d\n",pcb.id);
	printf("%d\n",pcb.paginas_codigo);
	printf("%d\n",pcb.pc);
	printf("%d\n",pcb.indices.cantidad_de_etiquetas);
	printf("%d\n",pcb.indices.cantidad_de_funciones);
	printf("%d\n",pcb.indices.etiquetas_size);
	printf("%d\n",pcb.indices.instruccion_inicio);
	printf("%d\n",pcb.indices.instrucciones_size);
	printf("%s\n",pcb.indices.etiquetas);
	for(i=0; i<pcb.indices.instrucciones_size;i++){
		printf("%d",pcb.indices.instrucciones_serializado[i].offset);
		printf("%d",pcb.indices.instrucciones_serializado[i].start);
	}
	printf("\n");

	printf("Hasta Ak\n");*/

	printf("Stack\n");
	Stack* stack01 = list_get(pcb.stack,0);
	printf("%d\n", stack01->retPos);
	printf("%d\n", stack01->retVar.off);
	printf("%d\n", stack01->retVar.pag);
	printf("%d\n", stack01->retVar.tamanio);
	printf("Hasta ak\n");
	printf("%s\n", pcb_char);
	pcb = fromStringPCB(pcb_char);
	pcb_char = toStringPCB(pcb);
	printf("%s\n", pcb_char);
	printf("Stack\n");
	Stack* stack02 = list_get(pcb.stack,0);
	printf("%d\n", stack02->retPos);
	printf("%d\n", stack02->retVar.off);
	printf("%d\n", stack02->retVar.pag);
	printf("%d\n", stack02->retVar.tamanio);
	printf("Hasta ak\n");
	/*printf("Repetir Metadata\n");
	printf("%d\n",pcb.id);
	printf("%d\n",pcb.paginas_codigo);
	printf("%d\n",pcb.pc);
	printf("%d\n",pcb.indices.cantidad_de_etiquetas);
	printf("%d\n",pcb.indices.cantidad_de_funciones);
	printf("%d\n",pcb.indices.etiquetas_size);
	printf("%d\n",pcb.indices.instruccion_inicio);
	printf("%d\n",pcb.indices.instrucciones_size);
	printf("%s\n",pcb.indices.etiquetas);
	for(i=0; i<pcb.indices.instrucciones_size;i++){
		printf("%d",pcb.indices.instrucciones_serializado[i].offset);
		printf("%d",pcb.indices.instrucciones_serializado[i].start);
	}
	printf("\n");

	printf("Hasta Ak\n");*/
	/*printf("Repetir Instrucciones\\n");
	printf("%d\n",pcb.indices.instrucciones_serializado[0].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[1].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[2].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[3].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[0].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[1].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[2].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[3].offset);
	printf("Hasta Ak\n");*/
	return 0;
}
