/*
 * umc.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <commons/string.h>
#define esIgual(a,b) string_equals_ignore_case(a,b)

void main() { //SOCKETS, CONEXION, BLA...
	char* comando;
	int velocidad;
	while (1) {
		comando = string_new(), scanf("%s", comando);
		if (esIgual(comando, "retardo")) {
			printf("velocidad nueva:");
			scanf("%d", &velocidad);
			printf("Velocidad actualizada:%d\n", velocidad);
		}
		if (esIgual(comando, "dump")) {
			printf("Estructuras de Memoria\n");
			printf("Datos de Memoria\n");
		}
	if (esIgual(comando,"tlb")) {
		printf("TLB Borrada :)\n");}
	if (esIgual(comando, "memoria")) {
				printf("Memoria Limpiada :)\n");
			}
		}
	}
