/*
 * Comunicacion.c
 *
 *  Created on: 21/5/2016
 *      Author: utnso
 */

#include "Comunicacion.h"
#define esIgual(a,b) string_equals_ignore_case(a,b)
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

int leerConfiguracion(char *ruta, datosConfiguracion** datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		return 0;
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys < 10) {
			return 0;
		} else {
			(*datos)->puerto_umc=buscarInt(archivoConfiguracion, "PUERTO_UMC");
			(*datos)->puerto_swap=buscarInt(archivoConfiguracion, "PUERTO_SWAP");
			(*datos)->marcos=buscarInt(archivoConfiguracion, "MARCOS");
			(*datos)->marco_size=buscarInt(archivoConfiguracion, "MARCO_SIZE");
			(*datos)->marco_x_proc=buscarInt(archivoConfiguracion, "MARCO_X_PROC");
			(*datos)->entradas_tlb=buscarInt(archivoConfiguracion, "ENTRADAS_TLB");
			(*datos)->retardo=buscarInt(archivoConfiguracion, "RETARDO");
			char* algoritmo=config_get_string_value(archivoConfiguracion, "ALGORITMO");
			if(esIgual(algoritmo,"Clock")){
				(*datos)->algoritmo=0;					//Clock Normal
			}else{
				(*datos)->algoritmo=1;					//Clock Mejorado
			}
			char* ip=string_new();
			string_append(&ip,config_get_string_value(archivoConfiguracion,"IP"));
			(*datos)->ip=ip;
			char* ipSwap=string_new();
			string_append(&ipSwap,config_get_string_value(archivoConfiguracion,"IP_SWAP"));
			(*datos)->ip_swap=ipSwap;
			config_destroy(archivoConfiguracion);
		}
	}
	return 1;
}

struct sockaddr_in crearDireccion(int puerto,char* ip){
	struct sockaddr_in direccion;
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr =inet_addr(ip);
	direccion.sin_port = htons(puerto);
	return direccion;
}

int conectar(int puerto,char* ip){   							//Con la swap
	struct sockaddr_in direccion=crearDireccion(puerto, ip);
	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccion, sizeof(direccion)));
	return conexion;
}

int autentificar(int conexion) {
	send(conexion, "soy_la_umc", 10, 0);
	char* bufferHandshakeSwap = malloc(8);
	int bytesRecibidosH = recv(conexion, bufferHandshakeSwap, 8, MSG_WAITALL);
	if (bytesRecibidosH <= 0) {
		registrarError(archivoLog,"Error al conectarse con la Swap");
		free (bufferHandshakeSwap);
		return 0;
	}
	free (bufferHandshakeSwap);
	return 1;
}

int comprobarCliente(int nuevoCliente) {
	char* bufferHandshake = malloc(11);
	int bytesRecibidosHs = recv(nuevoCliente, bufferHandshake, 11, MSG_WAITALL);
	bufferHandshake[bytesRecibidosHs] = '\0'; //lo paso a string para comparar
	if (string_equals_ignore_case("soy_una_cpu", bufferHandshake)) {
		free(bufferHandshake);
		return 1;
	} else if (string_equals_ignore_case("soy_nucleo1", bufferHandshake)) {
		free(bufferHandshake);
		return 2;
	}
	free(bufferHandshake);
	return 0;
}

int aceptarNucleo(int umc,struct sockaddr_in direccionCliente){
	int nuevo_cliente;
	int tam=sizeof(struct sockaddr_in);
	do {
		nuevo_cliente = accept(umc, (void *) &direccionCliente,(void *) &tam);

	} while (comprobarCliente(nuevo_cliente) != 2);												//Espero la conexion del nucleo
	int tamPagEnvio = ntohl(datosMemoria->marco_size);
	send(nuevo_cliente, &tamPagEnvio, 4, 0);													//Le envio el tamaño de pagina
	registrarInfo(archivoLog,"Nucleo aceptado!");
	return nuevo_cliente;
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

char* recibirMensaje(int conexion, int tamanio){
	char* mensaje=(char*)malloc(tamanio+1);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, MSG_WAITALL);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		free(mensaje);
		return "0";}
	mensaje[tamanio]='\0';
	return mensaje;
}

char* header(int numero){										//Recibe numero de bytes, y lo devuelve en 4 bytes (Ej. recibe "2" y devuelve "0002")
	char* longitud=string_new();
	string_append(&longitud,string_reverse(string_itoa(numero)));
	string_append(&longitud,"0000");
	longitud=string_substring(longitud,0,4);
	longitud=string_reverse(longitud);
	return longitud;
}

void agregarHeader(char** mensaje){
	char* head=string_new();
	string_append(&head,header(string_length(*mensaje)));
	string_append(&head,*mensaje);
	*mensaje=string_duplicate(head);
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
