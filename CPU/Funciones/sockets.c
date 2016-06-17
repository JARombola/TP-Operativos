#include "sockets.h"

int conectar(int puerto,char* ip){
	struct sockaddr_in direccServ;
	direccServ.sin_family = AF_INET;
	direccServ.sin_addr.s_addr = inet_addr(ip);
	direccServ.sin_port = htons(puerto);
	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccServ, sizeof(direccServ)) != 0);
	return conexion;
}
void enviarMensaje(int conexion, char* mensaje){
	char* mensajeReal =string_new();
	uint32_t longitud = strlen(mensaje);
	string_append(&mensajeReal,header(longitud));
	string_append(&mensajeReal,mensaje);
	send(conexion,mensajeReal,strlen(mensajeReal),0);
	free(mensajeReal);
}

void enviarMensajeConProtocolo(int conexion, char* mensaje, int protocolo){
	char* mensajeReal = string_new();
	uint32_t longitud = strlen(mensaje)+1;
	string_append(&mensajeReal,header(protocolo));
	string_append(&mensajeReal,header(longitud));
	string_append(&mensajeReal,mensaje);
	send(conexion,mensajeReal,strlen(mensajeReal),0);
	free(mensajeReal);
}

int autentificar(int conexion, char* autor){
	send(conexion,autor,strlen(autor),0);
	return (esperarConfirmacion(conexion));
}

int esperarConfirmacion(int conexion){
	int bufferHandshake;
	int bytesRecibidos = recv(conexion, &bufferHandshake, 4 , 0);
	if (bytesRecibidos <= 0) {
		return 0;
	}
	bufferHandshake=ntohl(bufferHandshake);
	return bufferHandshake;
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


int esperarConexion(int servidor,char* autentificacion){
	int cliente = aceptar(servidor);
	if (cliente <= 0){
		return -1;
		printf("Error de conexion\n");
	}
	autentificacion = esperarRespuesta(cliente);
	if (!(tienePermiso(autentificacion))){
		close(cliente);
		return 0;
	}
	send(cliente,"ok",2,0);
	return cliente;
}

char* esperarRespuesta(int conexion){
	char header[5];
	char* buffer;
	int bytes= recv(conexion, header,4,0);
	header[4]= '\0';
	uint32_t tamanioPaquete = atoi(header);
	if (bytes<=0){
		buffer = malloc(2*sizeof(char));
		buffer[0] = '\0';
	}else{
		buffer = malloc(tamanioPaquete+1);
		recv(conexion,buffer,tamanioPaquete,0);
		buffer[tamanioPaquete] = '\0';
	}
	return buffer;
}

int aceptar(int servidor){
	struct sockaddr_in direccionCliente;
	unsigned int tamanioDireccion = sizeof(struct sockaddr_in);
	int cliente = accept(servidor, (void*) &direccionCliente, &tamanioDireccion);
	return cliente;
}

char* header(int numero){										
	char* longitud=string_new();
	string_append(&longitud,string_reverse(string_itoa(numero)));
	string_append(&longitud,"0000");
	longitud=string_substring(longitud,0,4);
	longitud=string_reverse(longitud);
	return longitud;
}

int recibirProtocolo(int conexion){
	char protocolo[5];
	int bytes= recv(conexion, protocolo,4,0);
	protocolo[4] = '\0';
	return atoi(protocolo);
}

