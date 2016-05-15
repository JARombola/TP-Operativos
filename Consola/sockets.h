/*
 * Al usar esto tenes que definir en tu .c
 * int tienePermiso(char* autentificacion){} y retornar 0 si no tiene y 1 si tiene permiso
 */
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>

int tienePermiso(char* autentificacion){return 1;}
int conectar(int puerto, char* autor);
int autentificar(int conexion, char* autor);
int esperarConfirmacion(int conexion);
int crearServidor(int puerto);
void enviarMensaje(int conexion, char* mensaje);
int esperarConexion(int servidor);
char* esperarRespuesta(int conexion);
int aceptar(int servidor);

int conectar(int puerto, char* autor){

	struct sockaddr_in direccServ;
	direccServ.sin_family = AF_INET;
	direccServ.sin_addr.s_addr = INADDR_ANY;
	direccServ.sin_port = htons(puerto);

	int conexion = socket(AF_INET, SOCK_STREAM, 0);

	while (connect(conexion, (void*) &direccServ, sizeof(direccServ)) != 0);

	if (autentificar(conexion,autor)){
		return -1;
	}

	return conexion;
}
void enviarMensaje(int conexion, char* mensaje){
	uint32_t longitud = strlen(mensaje)+1;
	send(conexion, &longitud, 4,0);
	send(conexion,mensaje,longitud,0);
}

int autentificar(int conexion, char* autor){
	enviarMensaje(conexion,autor);;
	if (esperarConfirmacion(conexion)){
		return 1;
	}
	return 0;
}

int esperarConfirmacion(int conexion){

	char* bufferHandshake = malloc(sizeof(int));
	int bytesRecibidos = recv(conexion, bufferHandshake, sizeof(int), 0);

	if (bytesRecibidos <= 0) {
		printf("Rechazado\n");
		return 1;
	}
	printf("Aceptado\n");
	return 0;
}

int crearServidor(int puerto){
	struct sockaddr_in direccionServidor;
	direccionServidor.sin_family = AF_INET;
	direccionServidor.sin_addr.s_addr = INADDR_ANY;
	direccionServidor.sin_port = htons(puerto);

	int servidor = socket(AF_INET, SOCK_STREAM, 0);

	int activador = 1;
	setsockopt(servidor,SOL_SOCKET,SO_REUSEADDR, &activador, sizeof(activador));

	if (bind(servidor,(void*) &direccionServidor, sizeof(direccionServidor))){
		return -1;
	}

	listen(servidor,SOMAXCONN);
	return servidor;
}


int esperarConexion(int servidor){
	int cliente = aceptar(servidor);

	if (cliente <= 0){
		return -1;
		printf("Error de conexion\n");
	}

	char* autentificacion = esperarRespuesta(cliente);

	if (!(tienePermiso(autentificacion))){
		close(cliente);
		return 0;
	}

	send(cliente,"ok",3,0);
	return cliente;
}

char* esperarRespuesta(int conexion){
	uint32_t tamanioPaquete;
	char* buffer;
	int bytes= recv(conexion, &tamanioPaquete,4,0);

	if (bytes<=0){
		*buffer = NULL;
	}else{
		buffer = malloc(tamanioPaquete);

		recv(conexion,buffer,tamanioPaquete,0);
	}
	return buffer;
}

int aceptar(int servidor){
	struct sockaddr_in direccionCliente;
	unsigned int tamanioDireccion = sizeof(struct sockaddr_in);
	int cliente = accept(servidor, (void*) &direccionCliente, &tamanioDireccion);
	return cliente;
}

