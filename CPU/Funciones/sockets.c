#include "sockets.h"


#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)
#define AUTENTIFICACION "soy_un_cpu"

int leerConfiguracion(char *ruta, datosConfiguracion** datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		return 0;
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys < 5) {
			return 0;
		} else {
			(*datos)->puerto_umc=buscarInt(archivoConfiguracion, "PUERTO_UMC");
			(*datos)->puerto_nucleo=buscarInt(archivoConfiguracion, "PUERTO_NUCLEO");
			char* autentificacion=string_new();
			string_append(&autentificacion,config_get_string_value(archivoConfiguracion,"AUTENTIFICACION"));
			(*datos)->autentificacion=autentificacion;
			char* ipUmc=string_new();
			string_append(&ipUmc,config_get_string_value(archivoConfiguracion,"IP_UMC"));
			(*datos)->ip_umc=ipUmc;
			char* ipNucleo=string_new();
			string_append(&ipNucleo,config_get_string_value(archivoConfiguracion,"IP_NUCLEO"));
			(*datos)->ip_nucleo=ipNucleo;
			config_destroy(archivoConfiguracion);
		}
	}
	return 1;
}

int conectarseAlNucleo(int puerto, char* ip){
	int nucleo = conectar(puerto,ip);
	if (nucleo<0){
		printf("Error al conectarse con el nucelo \n");
	}
	autentificar(nucleo,AUTENTIFICACION);
	printf("Conexion con el nucleo OK... \n");
	return nucleo;
}

int conectarseALaUMC(int puerto, char* ip){
	int umc = conectar(puerto,ip);
	if (umc<0){
		printf("Error: No se ha logrado establecer la conexion con la UMC\n");
	}
	TAMANIO_PAGINA = autentificar(umc,AUTENTIFICACION);
	if (!TAMANIO_PAGINA){
		printf("Error: No se ha logrado establecer la conexion con la UMC\n");
		}
	printf("Conexion con la UMC OK...\n");
	return umc;
}

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

