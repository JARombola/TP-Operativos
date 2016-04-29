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
int esperarRespuesta(int cliente);

int main() {   // int main(int argc, char* argv[]) -> argv[1] = puerto Nucleo

	int umc_cliente = conectar(PUERTO_UMC);
	int nucleo_cliente = conectar(PUERTO_NUCLEO);

	send(umc_cliente, "soy_un_cpu", 10, 0);
	esperarRespuesta(umc_cliente);
	send(nucleo_cliente, "soy_un_cpu", 10, 0);
	esperarRespuesta(nucleo_cliente);


	//me llega mensaje de Nucleo y lo envio a la UMC
		int protocoloNucleo=0; //donde quiero recibir y cantidad que puedo recibir
			int bytesRecibidosCpu = recv(nucleo_cliente, &protocoloNucleo, sizeof(int32_t), 0);
			protocoloNucleo=ntohl(protocoloNucleo);
				if(bytesRecibidosCpu <= 0){
					perror("el nucleo se desconecto y/o no mando el mensaje. Se lo elimino\n");
			} else {
					char* bufferNucleo = malloc(protocoloNucleo * sizeof(char) + 1);
					bytesRecibidosCpu = recv(nucleo_cliente, bufferNucleo, protocoloNucleo, 0);
					bufferNucleo[protocoloNucleo + 1] = '\0'; //para pasarlo a string (era un stream)
					printf("Nucleo en: %d, me llegaron %d bytes con %s\n", nucleo_cliente,bytesRecibidosCpu, bufferNucleo);

				//y lo mando a la UMC
					int longitud = htonl(string_length(bufferNucleo));
					send(umc_cliente, &longitud, sizeof(int32_t), 0);
					send(umc_cliente, bufferNucleo, strlen(bufferNucleo), 0);
					free(bufferNucleo);
			}

	while (1) {
			char *mensaje;
			mensaje = string_new();
			scanf("%s", mensaje);
			int longitud = htonl(string_length(mensaje));
			if(mensaje[0]=='u'){
				send(umc_cliente, &longitud, sizeof(int32_t), 0);
				send(umc_cliente, mensaje, strlen(mensaje), 0);
			}else if(mensaje[0]=='n'){
				send(nucleo_cliente, &longitud, sizeof(int32_t), 0);
				send(nucleo_cliente, mensaje, strlen(mensaje), 0);
			}
		}

	return 0;
}

int conectar(int puerto){

	struct sockaddr_in direccServ;
	direccServ.sin_family = AF_INET;
	direccServ.sin_addr.s_addr = INADDR_ANY;
	direccServ.sin_port = htons(puerto);

	int cliente = socket(AF_INET, SOCK_STREAM, 0);
	printf("me cree, estoy en el %d\n", cliente);

	if (connect(cliente, (void*) &direccServ, sizeof(direccServ)) != 0) {
		perror("No se pudo conectar");
		return -1;
	}

	return cliente;
}

int esperarRespuesta(int cliente){

	char* bufferHandshake = malloc(8);
	int bytesRecibidos = recv(cliente, bufferHandshake, 11, 0);

	if (bytesRecibidos <= 0) {
		printf("se recibieron %d bytes, no estamos aceptados\n", bytesRecibidos);
		return 1;
	} else {
		printf("se recibieron %d bytes, estamos aceptados!\n", bytesRecibidos);
		return 0;
	}
}



