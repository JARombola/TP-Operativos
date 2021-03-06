/*
 * Comunicacion.c
 *
 *  Created on: 21/5/2016
 *      Author: utnso
 */

#include "Comunicacion.h"
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

int leerConfiguracion(char *ruta, datosConfiguracion **datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		return 0;
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys < 7) {
			return 0;
		} else {
			(*datos)->puerto = buscarInt(archivoConfiguracion, "PUERTO");
			char* nombreSwap=string_new();
			string_append(&nombreSwap,config_get_string_value(archivoConfiguracion, "NOMBRE_SWAP"));
			(*datos)->nombre_swap =nombreSwap;
			(*datos)->cantidadPaginas = buscarInt(archivoConfiguracion, "CANTIDAD_PAGINAS");
			(*datos)->tamPagina = buscarInt(archivoConfiguracion, "TAMANIO_PAGINA");
			(*datos)->retardoAcceso = buscarInt(archivoConfiguracion, "RETARDO_ACCESO");
			(*datos)->retardoCompactacion = buscarInt(archivoConfiguracion, "RETARDO_COMPACTACION");
			char* ip=string_new();
			string_append(&ip,config_get_string_value(archivoConfiguracion,"IP"));
			(*datos)->ip=ip;
			config_destroy(archivoConfiguracion);
			return 1;
		}
	}
}

struct sockaddr_in crearDireccion(int puerto,char* ip){
	struct sockaddr_in direccion;
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr =inet_addr(ip);
	direccion.sin_port = htons(puerto);
	return direccion;
}

int comprobarCliente(int cliente){
	char bufferHandshake[11];
	recv(cliente, bufferHandshake, 10, MSG_WAITALL);				//lo paso a string para comparar
	bufferHandshake[10] = '\0';
	if (string_equals_ignore_case("soy_la_umc", bufferHandshake)) {
		//free(bufferHandshake);
		send(cliente, "Aceptada", 8, 0);
		return 1;}
	//free(bufferHandshake);
	return 0;													//No era la UMC :/
}




int conectar(int puerto,char* ip){   							//Con la swap
	struct sockaddr_in direccion=crearDireccion(puerto, ip);
	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccion, sizeof(direccion)));
	return conexion;
}

int recibirProtocolo(int conexion){
	char* protocolo = malloc(5);
	int bytesRecibidos = recv(conexion, protocolo, sizeof(int32_t), MSG_WAITALL);
	if (bytesRecibidos <= 0) {printf("Error al recibir protocolo\n");
		free(protocolo);
		return -1;}
	protocolo[4]='\0';
	int numero = atoi(protocolo);
	free(protocolo);
	return numero;
}

void* recibirMensaje(int conexion, int tamanio){
	void* mensaje=(void*)malloc(tamanio);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, MSG_WAITALL);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		free(mensaje);
		char* adios=string_new();
		string_append(&adios,"0\0");
		return adios;}
	return mensaje;
}

char* header(int numero){										//Recibe numero de bytes, y lo devuelve en 4 bytes (Ej. recibe "2" y devuelve "0002")
	char* asd=string_new();
	char* num_char = string_itoa(numero);
	num_char = string_reverse(num_char);
	string_append(&asd,num_char);
	free(num_char);
	string_append(&asd,"0000");
	char* longitud=string_substring(asd,0,4);
	free(asd);
	char* longitudPosta=string_reverse(longitud);
	free(longitud);
	return longitudPosta;
}

void agregarHeader(char** mensaje){
	char* head=string_new();
	char* numero=header(string_length(*mensaje));
	string_append(&head,numero);
	string_append(&head,*mensaje);
	*mensaje=string_duplicate(head);
	free(numero);
	free (head);
}

int bindear(int socket, struct sockaddr_in direccionServer){
	int activado = 1;
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado)); //para cerrar los binds al cerrar
			if (bind(socket, (void *)&direccionServer, sizeof(direccionServer))){
				return 0;									//  FAIL!
			}
	return 1;				//Joya!
}
