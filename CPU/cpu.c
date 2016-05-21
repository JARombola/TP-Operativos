#include <stdlib.h>
#include <stdio.h>
#include <commons/string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
//#include "parser.h"

const int PUERTO_UMC = 6661;
const int PUERTO_NUCLEO = 6662;
#define IP_NUCLEO "127.0.0.1"
#define IP_UMC "127.0.0.2"

int conectarNUCLEO(int puerto);
int conectarUMC(int puerto);
int autentificar(int conexion);
int autentificarUMC(int conexion);
char* esperarPeticion(int nucleo);
int procesarPeticion(char* pcb, int nucleo, int umc);
int esperarConfirmacion(int conexion);
char* esperarRespuesta(int conexion);
int recibirProtocolo(int conexion);
char* recibirMensaje(int conexion, int tamanio);

/*Precondiciones:
 * 				El nucleo debe confirmar al CPU
 * 				El nucleo debe enviar el puerto de la UMC una vez confirmado
 * 				La UMC debe confirmar al CPU
 * 				El nucleo solo envia el PBC (esto igual es temporal)
 *
 *Se autentifica con un "soy_un_cpu"
 *
 *La logica se encuentra en "pocesarPeticion"
 */
int main() {

	printf("CPU Estable\n");
	printf("Intentando conectarse al Nucleo ... \n");


	int nucleo=conectarNUCLEO(PUERTO_NUCLEO);
	if (!autentificar(nucleo)){
		printf("Conexion al Nucleo Fail, error Handshake \n");
		return -1;
	}
	printf("Conexion al Nucleo OK \n");


	printf("Intentando conectarse a la UMC ... \n");
	int umc = conectarUMC(PUERTO_UMC);
	if (!autentificarUMC(umc)){
		printf("Conexion a la UMC Fail \n");
		return -3;
	}
	printf("Conexion a la UMC OK \n");

	char* pcb;
	int statusPeticion;

	while (1){
		char* op = recibirMensaje(nucleo, 1);
		int comparacion = strcmp(op,"a");
		if(!comparacion){
			printf("ERROR al recibir el protocolo\n");
			return -1;
		}else{
		printf("codigo op: %s\n", op);
		int pid = recibirProtocolo(nucleo);
		printf("pid: %d\n", pid);
		int pc = recibirProtocolo(nucleo);
		printf("---------pc: %d\n", pc);
		int sp = recibirProtocolo(nucleo);
		printf("sp: %d\n", sp);
		//hago muchas cosas con el pcb
		int codigo=1;
		codigo=htonl(codigo);
		int bytes = send(nucleo, &codigo, 4, 0);
		printf("le respondi al nucleo, le mande %d\n",bytes);
		}
		/*
		pcb = esperarPeticion(nucleo);
		statusPeticion  = procesarPeticion(pcb,nucleo,umc);
		if (statusPeticion < 0){
			perror("Error en el proceso de peticion \n");
			return -5;
		}
		printf("PCB:%s\n",pcb);
		int longitud = htonl(string_length(pcb));
		send(umc, &longitud, sizeof(int32_t), 0);
		send(umc, pcb, strlen(pcb), 0);
		*/
	}
	return 0;
}


int conectarNUCLEO(int puerto){

	struct sockaddr_in direccServ;
	direccServ.sin_family = AF_INET;
	direccServ.sin_addr.s_addr = inet_addr(IP_NUCLEO);
	direccServ.sin_port = htons(puerto);

	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccServ, sizeof(direccServ)));
	return conexion;
}
int conectarUMC(int puerto){

	struct sockaddr_in direccServ;
	direccServ.sin_family = AF_INET;
	direccServ.sin_addr.s_addr = inet_addr(IP_UMC);
	direccServ.sin_port = htons(puerto);

	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccServ, sizeof(direccServ)));
	return conexion;
}



int autentificar(int conexion){
	int codigo=1;
	codigo=htonl(codigo);
	send(conexion, &codigo, 4, 0);
	return (esperarConfirmacion(conexion));			//ME DEVUELVE EL PUERTO DE LA UMC o 0 si hubo error
}

int autentificarUMC(int conexion){
	send(conexion, "soy_un_cpu", 10, 0);
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

char* esperarRespuesta(int conexion){
	int protocolo = 0;
	int buffer = recv(conexion, &protocolo, sizeof(int32_t), 0);
	protocolo = ntohl(protocolo);

	if (buffer < 1){
		perror("Error de Conexion \n");
		return 0; // todo se desconecto, habria que reconectar pero falra saber de que puerto
	}else{

	char* mensaje = malloc(protocolo* sizeof(char) + 1);
	buffer = recv(conexion, mensaje, protocolo, 0);
	mensaje[protocolo + 1] = '\0';
	printf("1: %s \n", mensaje);
	return mensaje;
	}
}

char* esperarPeticion(int conexion){
	return esperarRespuesta(conexion);
}

int procesarPeticion(char* pcb, int nucleo, int umc){
//	char* linea = solicitarLinea(pcb,umc);
//	char * instrucciones = parsear(linea);
//	ejecutar(instrucciones);
	return 0;
}
int recibirProtocolo(int conexion){
	char* protocolo = malloc(4);
	int bytesRecibidos = recv(conexion, protocolo, sizeof(int32_t), 0);
	if (bytesRecibidos <= 0) {	printf("Error al recibir protocolo\n");
	return 0;
	}
	return atoi(protocolo);}

char* recibirMensaje(int conexion, int tamanio){
	char *mensaje=malloc(tamanio);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, 0);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		return "a";}
	return mensaje;
}

