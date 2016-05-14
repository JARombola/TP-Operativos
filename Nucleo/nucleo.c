/*
 * nucleo.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <commons/string.h>
#include <commons/config.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <parser/metadata_program.h>


#define PUERTO_UMC 6661
#define PUERTO_NUCLEO 6662
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra) 	//MACRO

typedef struct {
	int puerto_prog;
	int puerto_cpu;
	int quantum;
	int quantum_sleep;
	char** sem_ids;
	char** sem_init;		//TRANSFORMAR CON (atoi)
	char** io_ids;
	char** io_sleep;		//LO MISMO
	char** shared_vars;
} datosConfiguracion;

typedef struct {
	int PID;
	int PC;
	int SP;
	int pagsCodigo;
	int indiceCodigo;
	int indiceEtiquetas;
	int consola;
} pcb;


int autentificarUMC(int);
void leerConfiguracion(char*, datosConfiguracion*);
struct sockaddr_in crearDireccion(int puerto);
int conectarUMC(int);
int comprobarCliente(int);
int recibirProtocolo(int);
void* recibirMensaje(int, int);
pcb* crearPCB(char*,int);
t_list* crearIndiceDeCodigo(t_metadata_program*,int);
void mostrar(int*);


int main(int argc, char* argv[]) {
	int i,tamPagina;
	char* literal;
	//--------------------------------CONFIGURACION-----------------------------
//	datosConfiguracion* datosMemoria=malloc(sizeof(datosConfiguracion));
//	leerConfiguracion(argv[1], &datosMemoria);
//	manejarPCB("/home/utnso/tp-2016-1c-CodeBreakers/Consola/Nuevo");
	/*t_queue* dispositivos[datosMemoria.io_ids.CANTIDAD];
	for(i=0;i<=cant;i++){
	dispositivos[i]=queue_create();}*/
	//---------------------------------COLAS PCB-----------------------------------
	t_queue* colaNuevos=queue_create();
	t_queue* colaListos=queue_create();
	t_queue* colaExec=queue_create();
	t_queue* colaBloq=queue_create();
	t_queue* colaTerminados=queue_create();
	//------------------------------------CONEXION UMC--------------------------------
	int nucleo_servidor = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in direccionNucleo = crearDireccion(PUERTO_NUCLEO);
	printf("Nucleo creado, conectando con la UMC...\n");
	int conexionUMC = conectarUMC(PUERTO_UMC);
	tamPagina=autentificarUMC(conexionUMC);
	if (!tamPagina) {
		printf("Falló el handshake\n");
		return -1;
	}
	printf("Aceptados por la umc\n");

	int activado = 1;
	setsockopt(nucleo_servidor, SOL_SOCKET, SO_REUSEADDR, &activado,
			sizeof(activado));
	if (bind(nucleo_servidor, (void *) &direccionNucleo,
			sizeof(direccionNucleo))) {
		perror("Fallo el bind");
		return 1;
	}

	printf("Estoy escuchando\n");
	listen(nucleo_servidor, 15);

	//ahora creo el select
	fd_set descriptores;
	int nuevo_cliente;
	t_list* cpus, *consolas;
	cpus = list_create();
	consolas = list_create();
	int max_desc = conexionUMC;
	struct sockaddr_in direccionCliente;	//direccion donde guarde el cliente
	int sin_size = sizeof(struct sockaddr_in);


	while (1) {
		FD_ZERO(&descriptores);
		FD_SET(nucleo_servidor, &descriptores);
		FD_SET(conexionUMC, &descriptores);
		max_desc = conexionUMC;

		for (i = 0; i < list_size(consolas); i++) {
			int conset = (int) list_get(consolas, i); //conset = consola para setear
			FD_SET(conset, &descriptores);
			if (conset > max_desc) {
				max_desc = conset;
			}
		}
		for (i = 0; i < list_size(cpus); i++) {
			int cpuset = (int) list_get(cpus, i);
			FD_SET(cpuset, &descriptores);
			if (cpuset > max_desc) {
				max_desc = cpuset;
			}
		}

		if (select(max_desc + 1, &descriptores, NULL, NULL, NULL) < 0) {
			perror("Error en el select");
			//exit(EXIT_FAILURE);
		}
		for (i = 0; i < list_size(consolas); i++) {
																			//entro si una consola me mando algo
			int unaConsola = (int) list_get(consolas, i);
			if (FD_ISSET(unaConsola, &descriptores)) {
				int protocolo = recibirProtocolo(unaConsola);
				if (protocolo == -1) {
					perror("La consola se desconecto o algo. Eliminada\n");
					list_remove(consolas, i);
				} else {
					char* bufferConsola = malloc(protocolo + 1);
					int mensaje = recibirMensaje(unaConsola, protocolo);
					free(bufferConsola);
					//mando mensaje a los CPUs
					for (i = 0; i < list_size(cpus); i++) {
						//ver los clientes que recibieron informacion
						int unCPU = list_get(cpus, i);
						int longitud = htonl(string_length(bufferConsola));
						send(unCPU, &longitud, sizeof(int32_t), 0);
						send(unCPU, bufferConsola, strlen(bufferConsola), 0);
					}
				}
			}
		}
		for (i = 0; i < list_size(cpus); i++) {
			//que cpu me mando informacion
			int unCPU = (int) list_get(cpus, i);
			if (FD_ISSET(unCPU, &descriptores)) {
				int protocolo=recibirProtocolo(unCPU);
				if (protocolo==-1) {
					perror("el cpu se desconecto o algo. Se lo elimino\n");
					list_remove(cpus, i);
				} else {
					char* bufferCpu = malloc(protocolo + 1);
					int mensaje = recibirMensaje(unCPU,protocolo);
					free(bufferCpu);
				}
			}
		}

		if (FD_ISSET(conexionUMC, &descriptores)) {
			//se activo la UMC, me esta mandando algo
		}

		if (FD_ISSET(nucleo_servidor, &descriptores)) { //aceptar cliente
			nuevo_cliente = accept(nucleo_servidor, (void *) &direccionCliente,
					(void *) &sin_size);
			if (nuevo_cliente == -1) {
				perror("Fallo el accept");
			}
			printf("Recibi una conexion en %d!!\n", nuevo_cliente);
			switch (comprobarCliente(nuevo_cliente)) {
			case 0:															//ERROR!!
				perror("No lo tengo que aceptar, fallo el handshake\n");
				close(nuevo_cliente);
				break;
			case 1:															//CPU
				send(nuevo_cliente, tamPagina, 4, 0);
				list_add(cpus, (void *) nuevo_cliente);
				printf("Acepté un nuevo cpu\n");
				break;
			case 2:															//CONSOLA, RECIBO EL CODIGO
				send(nuevo_cliente, "1", 1, 0);
				list_add(consolas, (void *) nuevo_cliente);
				printf("Acepté una nueva consola\n");
				int tamanio=recibirProtocolo(nuevo_cliente);
				if (tamanio>0){
				char* codigo=malloc(tamanio);
				codigo=recibirMensaje(nuevo_cliente, tamanio);
				pcb* pcbNuevo=crearPCB(codigo,tamPagina);
				queue_push(colaNuevos,(pcb*) pcbNuevo);
			//	list_iterate(pcbNuevo->indiceCodigo, (void*) mostrar);		//Ver inicio y offset de cada sentencia
				free(codigo);}
				break;
			}
		}
	}
	//free(datosMemoria);
	return 0;
}

//--------------------------------------LECTURA CONFIGURACION

void leerConfiguracion(char *ruta, datosConfiguracion *datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		perror("Faltó Ruta CONFIGURACION");
		exit(0);
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys != 9) {
			perror("ERROR CANTIDAD DATOS DE CONFIGURACION");
		} else {
			datos->puerto_prog = buscarInt(archivoConfiguracion, "PUERTO_PROG");
			datos->puerto_cpu = buscarInt(archivoConfiguracion, "PUERTO_CPU");
			datos->quantum = buscarInt(archivoConfiguracion, "QUANTUM");
			datos->quantum_sleep = buscarInt(archivoConfiguracion,
					"QUANTUM_SLEEP");
			datos->sem_ids = config_get_array_value(archivoConfiguracion,
					"SEM_ID");
			datos->sem_init = config_get_array_value(archivoConfiguracion,
					"SEM_INIT");
			datos->io_ids = config_get_array_value(archivoConfiguracion,
					"IO_ID");
			datos->io_sleep = config_get_array_value(archivoConfiguracion,
					"IO_SLEEP");
			datos->shared_vars = config_get_array_value(archivoConfiguracion,
					"SHARED_VARS");
			config_destroy(archivoConfiguracion);
		}
	}
}
struct sockaddr_in crearDireccion(int puerto) {
	struct sockaddr_in direccion;
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr = INADDR_ANY;
	direccion.sin_port = htons(puerto);
	return direccion;
}

int conectarUMC(int puerto) {
	struct sockaddr_in direccionUMC = crearDireccion(puerto);
	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccionUMC, sizeof(direccionUMC)));
	return conexion;
}

int autentificarUMC(int conexion) {
	send(conexion, "soy_el_nucleo", 13, 0);
	int tamPagina;
	int bytesRecibidosH = recv(conexion, &tamPagina, 4, 0);
	if (bytesRecibidosH <= 0) {
		printf("Rechazado por la UMC\n");
		return 0;
	}
	return htonl(tamPagina);					//ME ENVIA EL TAMAÑO DE PAGINA
}

int comprobarCliente(int nuevoCliente) {
	char* bufferHandshake = malloc(16);
	int bytesRecibidosHs = recv(nuevoCliente, bufferHandshake, 15, 0);
	bufferHandshake[bytesRecibidosHs] = '\0'; //lo paso a string para comparar
	if (!strcmp("soy_un_cpu", bufferHandshake)) {
		free(bufferHandshake);
		return 1;
	} else if (!strcmp("soy_una_consola", bufferHandshake)) {
		free(bufferHandshake);
		return 2;
	}
	free(bufferHandshake);
	return 0;
}

int recibirProtocolo(int conexion){
	char* protocolo = malloc(4);
	int bytesRecibidos = recv(conexion, protocolo, sizeof(int32_t), 0);
	if (bytesRecibidos <= 0) {	printf("Error al recibir protocolo\n");
	return -1;
	}
	return atoi(protocolo);}

void* recibirMensaje(int conexion, int tamanio){
	char* mensaje=malloc(tamanio);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, 0);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		return -1;}
	return mensaje;
}

//----------------------------------------PCB------------------------------------------------------
pcb* crearPCB(char* codigo,int tamPagina) {
	pcb* pcbProceso;
//	printf("***CODIGO:\n%s\n", codigo);
	t_metadata_program *metadata = metadata_desde_literal(codigo);
	t_intructions* unaInstruccion = metadata->instrucciones_serializado;
	pcbProceso->PC = unaInstruccion[metadata->instruccion_inicio].start;					//Pos de la primer instruccion
	t_list* indiceCodigo = crearIndiceDeCodigo(metadata,tamPagina);
	pcbProceso->indiceCodigo=indiceCodigo;
	//int asd=metadata_buscar_etiqueta("Etiqueta",metadata->etiquetas,metadata->etiquetas_size);
	pcbProceso->pagsCodigo = list_size(indiceCodigo)-1;			//-1 para que empiece desde 0
	return pcbProceso;
}


t_list* crearIndiceDeCodigo(t_metadata_program* meta, int tamPagina) {
	t_list* lineas = list_create();
	t_intructions* unaInstruccion = meta->instrucciones_serializado;
	int i;
	for (i = 0; i < (meta->instrucciones_size); i++, unaInstruccion++) {					//Corta el codigo segun tamaño de pagina
		int j, resto = (unaInstruccion->offset) % tamPagina;
		for (j = 0; j < (unaInstruccion->offset) / tamPagina; j++) {
			int* unaLinea = malloc(sizeof(int*));										//0=inicio, 1=offset
			unaLinea[0] = unaInstruccion->start + j * tamPagina;
			unaLinea[1] = tamPagina;
			list_add(lineas, (int*) unaLinea);											//Guarda el PUNTERO al int
		}
		if (resto) {
			int* unaLinea = malloc(sizeof(int*));
			unaLinea[0] = unaInstruccion->start + j * tamPagina;
			unaLinea[1] = resto;
			list_add(lineas, (int*) unaLinea);
		}
	}
	return lineas;
	list_clean(lineas);
	list_destroy(lineas);

}

void mostrar(int* sentencia) {
	printf("Inicio:%d | Offset:%d\n", sentencia[0], sentencia[1]);
}

