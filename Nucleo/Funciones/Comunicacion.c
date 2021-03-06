/*
 * Comunicacion.c
 *
 *  Created on: 21/5/2016
 *      Author: utnso
 */

#include "Comunicacion.h"
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

int leerConfiguracion(char *ruta, datosConfiguracion** datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		return 0;
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys < 12) {
			return 0;
		} else {
			(*datos)->puerto_nucleo = buscarInt(archivoConfiguracion, "PUERTO_NUCLEO");
			(*datos)->puerto_umc= buscarInt(archivoConfiguracion, "PUERTO_UMC");
			(*datos)->quantum = buscarInt(archivoConfiguracion, "QUANTUM");
			(*datos)->quantum_sleep = buscarInt(archivoConfiguracion,
					"QUANTUM_SLEEP");
			(*datos)->sem_ids = config_get_array_value(archivoConfiguracion,"SEM_IDS");
			(*datos)->sem_init = config_get_array_value(archivoConfiguracion,"SEM_INIT");
			(*datos)->io_ids = config_get_array_value(archivoConfiguracion,"IO_IDS");
			(*datos)->io_sleep = config_get_array_value(archivoConfiguracion,"IO_SLEEP");
			(*datos)->shared_vars = config_get_array_value(archivoConfiguracion,"SHARED_VARS");
			(*datos)->tamStack=buscarInt(archivoConfiguracion,"STACK_SIZE");
			char* ip=string_new();
			string_append(&ip,config_get_string_value(archivoConfiguracion,"IP"));
			(*datos)->ip =ip;
			char* ipUMC=string_new();
			string_append(&ipUMC,config_get_string_value(archivoConfiguracion,"IP_UMC"));
			(*datos)->ip_umc = ipUMC;
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

int autentificarUMC(int conexion) {
	send(conexion, "soy_nucleo1", 11, 0);
	int st=htonl(datosNucleo->tamStack);
	send(conexion,&st,sizeof(int),0);
	int tamPagina;
	int bytesRecibidosH = recv(conexion, &tamPagina, 4, MSG_WAITALL);
	if (bytesRecibidosH <= 0) {
		printf("Rechazado por la UMC\n");
		return 0;
	}
	return htonl(tamPagina);					//ME ENVIA EL TAMAÑO DE PAGINA
}


int comprobarCliente(int nuevoCliente) {
	char* bufferHandshake = malloc(12);
	int bytesRecibidosHs = recv(nuevoCliente, bufferHandshake, 11, MSG_WAITALL);
	bufferHandshake[bytesRecibidosHs] = '\0';				 //lo paso a string para comparar
	if (string_equals_ignore_case("soy_una_cpu", bufferHandshake)) {
		free(bufferHandshake);
		return 1;
	} else{
		if (string_equals_ignore_case("soy_consola", bufferHandshake)) {
			free(bufferHandshake);
			return 2;
		}
	}
	free(bufferHandshake);
	return 0;
}

int conectar(int puerto,char* ip){   							//Con la swap
	struct sockaddr_in direccion=crearDireccion(puerto, ip);
	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccion, sizeof(direccion)));
	return conexion;
}

int recibirProtocolo(int conexion){
	char* protocolo = malloc(5);
	int bytesRecibidos = recv(conexion, protocolo, 4, MSG_WAITALL);
	if (bytesRecibidos <= 0) {printf("Error al recibir protocolo\n");
		free(protocolo);
		return -1;}
	protocolo[4]='\0';
	int numero = atoi(protocolo);
	free(protocolo);
	return numero;
}

char* recibirMensaje(int conexion, int tamanio){
	char* mensaje=malloc(tamanio+1);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, MSG_WAITALL);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		free(mensaje);
		return "a";}
	mensaje[tamanio]='\0';
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
