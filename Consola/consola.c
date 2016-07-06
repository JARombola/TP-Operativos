/*
* consola.c
 *
 *  Created on: 29/4/2016
 *      Author: utnso
 */
#include <stdlib.h>
#include <stdio.h>
#include <commons/string.h>
#include <commons/config.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Funciones/Comunicacion.h"

int puerto_nucleo;
char* ip_nucleo;

//int protocolo(int nucleo);
int leerConfiguracion(char* ruta);
int autentificar(int);
int enviarAnsisop(FILE*, int);

int main(int argc, char* argv[]) {//Se le envia por parametro el archivo a ejecutar (#!, ver "Nuevo")
	if(!(leerConfiguracion("../ConfigConsola") || leerConfiguracion("ConfigConsola"))){
		printf("Error al leer archivo de configuracion\n");
		return 1;
	}

	printf("Consola creada. Conectando al Nucleo...\n");
	int nucleo=conectar(puerto_nucleo,ip_nucleo);
	int proceso=autentificar(nucleo);
	if (!proceso) {
		printf("Conexion al nucleo fail, error handshake\n");
		return -1;
	}
	printf("Conexion Ok. Proceso N°:%d\n",proceso);
	FILE* ansisop=fopen(argv[1],"r");
	if (!ansisop){perror("Archivo");}
	if(enviarAnsisop(ansisop, nucleo)){printf("Error en el envio del codigo\n");}
	int respuesta=recibirProtocolo(nucleo);
	if (!respuesta){
		printf("Ansisop rechazado\n Consola finalizada\n");
		return -1;
	}
	printf("Ansisop enviado con éxito\n");

	while (1){
		//recibo de nucleo
		int tamanio;
		char* texto;
		int protocolo = recibirProtocolo(nucleo);
		switch (protocolo) {
					case 0:															//Error
						perror("El nucleo se desconecto o hubo error de protocolo\n");
						close(nucleo);
						return -1;
						break;
					case 1:														//IMPRIMIR
						tamanio = recibirProtocolo(nucleo);
						texto = recibirMensaje(nucleo,tamanio);
						texto[tamanio]='\0';
						printf("%s\n",texto);
						free (texto);
						break;
					case 2:														//TERMINO BIEN
						printf("el programa finalizo con exito\n");
						send(nucleo,"0001",4,0);
						return -1;
						break;
					case 3:														//TERMINO MAL
						printf("hubo un error en la ejecucion del programa\n");
						send(nucleo,"0001",4,0);
						return -1;
						break;
					}
	}
	return 0;
}

int autentificar(int conexion){
	send(conexion, "soy_consola", 11, 0);
	int respuesta=recibirProtocolo(conexion);
	return respuesta;
}

int enviarAnsisop(FILE* archivo, int sockNucleo){
	fseek (archivo, 0, SEEK_END);
	int bytesArchivo = ftell (archivo);
	fseek (archivo, 0, SEEK_SET);
	char* codigo = (char*)malloc(bytesArchivo+4); 											//+4 Para el header (longitud)
	if (codigo){
		fread (codigo, sizeof(char), bytesArchivo, archivo);
	}
	else {
		perror("Error Malloc");
		free(codigo);
		fclose(archivo);
		return 1;
	}
	fclose (archivo);
	agregarHeader(&codigo);
	codigo[bytesArchivo+4]='\0';
	int enviados=send(sockNucleo, codigo, string_length(codigo), 0);
	//printf("Codigo: %s %d\n",codigo,string_length(codigo));
	free(codigo);
	if(enviados==string_length(codigo)){return 0;}										//Envio ok
	return 1;																			//Error
}

int leerConfiguracion(char *ruta){
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		return 0;
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys < 2) {
			return 0;
		} else {
			puerto_nucleo=config_get_int_value(archivoConfiguracion, "PUERTO_NUCLEO");
			ip_nucleo=string_new();
			string_append(&ip_nucleo,config_get_string_value(archivoConfiguracion,"IP_NUCLEO"));
			config_destroy(archivoConfiguracion);
		}
	}
	return 1;
}

