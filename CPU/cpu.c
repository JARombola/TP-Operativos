#include <stdlib.h>
#include <stdio.h>
#include <commons/string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

const int PUERTO_UMC = 6661;
const int PUERTO_NUCLEO = 6662;

int conectar(int puerto);
int autentificar(int conexion);
int esperarPeticion(int nucleo);
int procesarPeticion(char* pcb, int nucleo, int umc);
int esperarConfirmacion(int conexion);
int esperarRespuesta(int conexion);

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


	int nucleo= conectar(PUERTO_NUCLEO);
	int puertoUMC=autentificar(nucleo);
	if (!puertoUMC){
		printf("Conexion al Nucleo Fail \n");
		return -1;
	}
	printf("Conexion al Nucleo OK:%d \n",puertoUMC);

	if (puertoUMC<0){
		printf("Error de conexion con el nucleo \n");
		return -2;
	}

	printf("Intentando conectarse a la UMC ... \n");
	int umc = 0;
	umc = conectar(puertoUMC);
	if (autentificar(umc)< 0){
		printf("Conexion a la UMC Fail \n");
		return -3;
	}
	printf("Conexion a la UMC OK \n");

	char* pcb;
	int statusPeticion;

	while (1){
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
	return (esperarConfirmacion(conexion));			//ME DEVUELVE EL PUERTO DE LA UMC o 0 si hubo error
}

int esperarConfirmacion(int conexion){
	int puertoUMC=0;
	int bytesRecibidos = recv(conexion, &puertoUMC, sizeof(int32_t), 0);
	if (bytesRecibidos <= 0) {
		printf("Rechazado\n");
		return 0;
	}
	printf("Aceptado\n");
	return (ntohl(puertoUMC));
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

