#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

const int PUERTO_UMC = 6661;
const int PUERTO_NUCLEO = 6662;

int conectar(int puerto);
int autentificar(int conexion);
int esperarPeticion(int nucleo);
int procesarPeticion(int pcb, int nucleo, int umc);
int esperarConfirmacion(int conexion);
int esperarRespuesta(int conexion);


int main() {

	printf("CPU Estable\n");
	printf("Intentando conectarse al Nucleo ... \n");


	int nucleo= conectar(PUERTO_NUCLEO);

	if (autentificar(nucleo)){
		printf("Fui rechazado por el Nucleo \n");
		printf("Conexion al Nucleo Fail \n");
		return -1;
	}
	printf("Conexion al Nucleo OK \n");
	int puertoUMC = esperarRespuesta(nucleo);

	printf("Intentando conectarse a la UML ... \n");
	int umc = conectar(PUERTO_UMC); //Harcodiado
	if (autentificar(umc)< 0){
		printf("Fue rechazado por la UMC \n");
		printf("Conexion a la UMC Fail \n");
		return -2;
	}
	printf("Conexion a la UMC OK \n");

	char* pcb;
	int statusPeticion;

	while (1){
		pcb = esperarPeticion(nucleo);
		if (pcb < 0){
			perror("Error en la conexion con el Nucleo");
			return -3;
		}
		statusPeticion  = procesarPeticion(pcb,nucleo,umc);
		if (statusPeticion < 0){
			perror("Error en el proceso de peticion \n");
			return -4;
		}
	}
	return 0;
}


int conectar(int puerto){

	struct sockaddr_in direccServ;
	direccServ.sin_family = AF_INET;
	direccServ.sin_addr.s_addr = INADDR_ANY;
	direccServ.sin_port = htons(puerto);

	int conexion = socket(AF_INET, SOCK_STREAM, 0);

	while (connect(conexion, (void*) &direccServ, sizeof(direccServ)) != 0);

	return conexion;
}

int autentificar(int conexion){
	send(conexion, "soy_un_cpu", 10, 0);

	if (esperarConfirmacion(conexion) < 0){
		return -1;
	}

	return 0;
}

int esperarConfirmacion(int conexion){

	char* bufferHandshake = malloc(8);
	int bytesRecibidos = recv(conexion, bufferHandshake, 11, 0);

	if (bytesRecibidos <= 0) {
		printf("Rechazado\n");
		return -1;
	} else {
		printf("Aceptado\n");
		return 0;
	}
}

int esperarRespuesta(int conexion){
	int protocolo = 0;
	int buffer = recv(conexion, &protocolo, sizeof(int32_t), 0);
	protocolo = ntohl(protocolo);

	if (buffer < 1){
		perror("Error de Conexion \n");
		return -1;
	}

	char* mensaje = malloc(protocolo* sizeof(char) + 1);
	buffer = recv(conexion, mensaje, protocolo, 0);
	mensaje[protocolo + 1] = '\0';
	printf("1: %s \n", mensaje);
	return 0;
}

int esperarPeticion(int conexion){
	return esperarRespuesta(conexion);
}

int procesarPeticion(int pcb, int nucleo, int umc){
//	char* linea = solicitarLinea(pcb,umc);
//	char * instrucciones = parsear(linea);
//	ejecutar(instrucciones);
	return 0;
}
