/*
 * consola.c
 *
 *  Created on: 29/4/2016
 *      Author: utnso
 */

#include <stdlib.h>
#include <stdio.h>
#include <commons/string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PUERTO_NUCLEO 6662
int protocolo(int nucleo);
int conectar (int);
int autentificar(int);
int esperarConfirmacion(int);

int main(int argc, char* argv[]) {//Se le envia por parametro el archivo a ejecutar (#!, ver "Nuevo")

	int nucleo=conectar(PUERTO_NUCLEO);
	printf("Consola creada\n Intentando conectar al nucleo...\n");
	if (!autentificar(nucleo)) {
		printf("Conexion al nucleo fail, error handshake\n");
		return -1;
	}
	printf("Aceptados! :)\n");
	while (1){
		char *mensaje;
		mensaje = string_new();
		scanf("%s", mensaje);
		int longitud = htonl(string_length(mensaje));
		send(nucleo, &longitud, sizeof(int32_t), 0);
		send(nucleo, mensaje, string_length(mensaje), 0); //envia datos por teclado*/

		//recibo de nucleo
/*
		switch (protocolo(nucleo)) { //misma idea de aceptar clientes nuevos del nucleo
					case 0:															//Error
						perror("El nucleo se desconecto\n");
						close(nucleo);
						// intentar conectar? o sin nucleo el programa tiene que terminar forzosamente?
						return -1;
						break;
					case 1:														//IMPRIMIR
						printf("el nucleo quiere que imprima\n");
						break;
					case 2:														//IMPRIMIR TEXTO
						printf("el nucleo quiere que imprima texto\n");
						break;
					}
				/*	luego, tengo otro protocolo de tamanio? o directo recibo lo que tengo que imprimir?
					char* bufferC = malloc(protocoloC * sizeof(char) + 1);
					bytesRecibidosC = recv(unaConsola, bufferC, protocoloC, 0);
					bufferC[protocoloC + 1] = '\0';
					printf("cliente: %d, me llegaron %d bytes con %s\n", unaConsola,bytesRecibidosC, bufferC);*/

	//}
	return 0;
}

int conectar(int puerto){

	struct sockaddr_in direccNucleo;
	direccNucleo.sin_family = AF_INET;
	direccNucleo.sin_addr.s_addr = INADDR_ANY;
	direccNucleo.sin_port = htons(puerto);

	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccNucleo, sizeof(direccNucleo)));
	return conexion;
}

int autentificar(int conexion){
	send(conexion, "soy_una_consola", 15, 0);
	return (esperarConfirmacion(conexion));			//ME DEVUELVE EL PUERTO DE LA UMC o 0 si hubo error
}

int esperarConfirmacion(int conexion){
	int buffer=0;
	int bytesRecibidos = recv(conexion, &buffer, sizeof(int32_t), 0);
	if (bytesRecibidos <= 0) {
		printf("Rechazado\n");
		return 0;
	}
	printf("Aceptado\n");
	return 1;
}


int protocolo(int nucleo) {
	char* buffer = malloc(2);
	int bytesRecibidos = recv(nucleo, buffer, 1, 0);
	buffer[bytesRecibidos] = '\0'; //lo paso a string para comparar
	if(bytesRecibidos <= 0){ //se desconecto
		free(buffer);
		return 0;
	}
	if (strcmp("1", buffer) == 0) {//quiere imprimir
		free(buffer);
		return 1;
	} else if (strcmp("2", buffer) == 0) { //quiere imprimir texto
		free(buffer);
		return 2;
	}
	return -1;
}
