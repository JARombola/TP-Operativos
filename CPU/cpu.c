#include <stdio.h>
#include <netinet/in.h>

int conectarCliente();
int esperarRespuesta(int cliente);

int main() {   // int main(int argc, char* argv[]) -> argv[1] = puerto Nucleo

	int cliente = conectarCliente();

	if (cliente == -1){
		printf("Error de conexion\n");
		return -1;
	}

	send(cliente, "soy_una_CPU", 12, 0);

	esperarRespuesta(cliente);

	return 0;
}

int conectarCliente(){

	const int PUERTO_SERVIDOR = 8080;

	struct sockaddr_in direccServ;
	direccServ.sin_family = AF_INET;
	direccServ.sin_addr.s_addr = INADDR_ANY;
	direccServ.sin_port = htons(PUERTO_SERVIDOR);

	int cliente = socket(AF_INET, SOCK_STREAM, 0);
	printf("me cree, estoy en el %d\n", cliente);

	if (connect(cliente, (void*) &direccServ, sizeof(direccServ)) != 0) {
		perror("No se pudo conectar");
		return -1;
	}

	return cliente;
}

int esperarRespuesta(int cliente){

	char* bufferHandshake = malloc(12);
	int bytesRecibidos = recv(cliente, bufferHandshake, 12, 0);

	if (bytesRecibidos <= 0) {
		printf("se recibieron %d bytes, no estamos aceptados\n", bytesRecibidos);
		return 1;
	} else {
		printf("se recibieron %d bytes, estamos aceptados!\n", bytesRecibidos);
		return 0;
	}
}



