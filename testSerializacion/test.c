#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Funciones/json.h"

int main(){
	FILE* arch = fopen("arch.ansisop","r");
	char* arch_char = toJsonArchivo(arch);
	PCB pcb;
	pcb.id = 6;
	pcb.paginas_codigo = 4;
	pcb.pc = 0;
	pcb.stack = list_create();
	pcb.indices = *(metadata_desde_literal(arch_char));
	printf("a\n");
	printf("%d\n",pcb.indices.instrucciones_serializado[1].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[1].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[2].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[2].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[3].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[3].start);
	char* pcb_char = toStringPCB(pcb);
	printf("Metadata:%s\n\n\n\n\n", toStringMetadata(pcb.indices,'&'));
	printf("%s\n", pcb_char);
	pcb = fromStringPCB(pcb_char);
	pcb_char = toStringPCB(pcb);
	printf("%s\n", pcb_char);
	printf("%d\n",pcb.indices.instrucciones_serializado[1].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[1].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[2].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[2].start);
	printf("%d\n",pcb.indices.instrucciones_serializado[3].offset);
	printf("%d\n",pcb.indices.instrucciones_serializado[3].start);
	return 0;
}
