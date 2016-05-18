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
	t_intructions* indiceCodigo;
	int indiceEtiquetas;
	int consola;
} pcb;


int autentificarUMC(int);
void leerConfiguracion(char*, datosConfiguracion*);
struct sockaddr_in crearDireccion(int puerto);
int conectarUMC(int);
int comprobarCliente(int);
int recibirProtocolo(int);
char* recibirMensaje(int, int);
pcb* crearPCB(char*);
t_list* crearIndiceDeCodigo(t_metadata_program*);
int cortarInstrucciones(t_metadata_program*);
int calcularPaginas(char*);
void mostrar(int*);
char* header(int);
void agregarHeader(char**);
void enviarAnsisopAUMC(int, char*);


int ultimoPID=0,tamPagina=0;
t_queue *colaNuevos,*colaListos,*colaExec,*colaBloq,*colaTerminados;

int main(int argc, char* argv[]) {
	int i;
	char* literal;
	//--------------------------------CONFIGURACION-----------------------------
//	datosConfiguracion* datosMemoria=malloc(sizeof(datosConfiguracion));
//	leerConfiguracion(argv[1], &datosMemoria);
//	manejarPCB("/home/utnso/tp-2016-1c-CodeBreakers/Consola/Nuevo");
	/*t_queue* dispositivos[datosMemoria.io_ids.CANTIDAD];
	for(i=0;i<=cant;i++){
	dispositivos[i]=queue_create();}*/
	//---------------------------------COLAS PCB-----------------------------------
	colaNuevos=queue_create();
	colaListos=queue_create();
	colaExec=queue_create();
	colaBloq=queue_create();
	colaTerminados=queue_create();
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
	listen(nucleo_servidor, 100);

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
					close(unaConsola);
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
						send(unCPU, bufferConsola, string_length(bufferConsola), 0);
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
					close(unCPU);
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
				int tamanio = recibirProtocolo(nuevo_cliente);
				if (tamanio > 0) {
					char* codigo = (char*)recibirMensaje(nuevo_cliente, tamanio);
					printf("--Codigo:%s--\n",codigo);
					enviarAnsisopAUMC(conexionUMC,codigo);
				}
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
	char* bufferHandshake = malloc(15);
	int bytesRecibidosHs = recv(nuevoCliente, bufferHandshake, 15, 0);
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
	char* protocolo = malloc(5);
	int bytesRecibidos = recv(conexion, protocolo, sizeof(int32_t), 0);
	if (bytesRecibidos <= 0) {	printf("Error al recibir protocolo\n");
		return -1;
	}
	protocolo[4]='\0';
	return atoi(protocolo);}

char* recibirMensaje(int conexion, int tamanio){
	char* mensaje=(char*)malloc(tamanio+1);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, 0);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		return (int)-1;}
	mensaje[tamanio]='\0';
	return mensaje;
}

char* header(int numero){							//Recibe numero de bytes, y lo devuelve en 4 bytes (Ej. recibe "2" y devuelve "0002")
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
	*mensaje=string_reverse(*mensaje);
	string_append(mensaje,string_reverse(head));
	*mensaje=string_reverse(*mensaje);
	free (head);
}
//----------------------------------------PCB------------------------------------------------------

void enviarAnsisopAUMC(int conexionUMC, char* codigo){
	int paginasNecesarias=calcularPaginas(codigo);
	char* mensajeInicial = string_new();
	pcb* pcbNuevo = crearPCB(codigo);
	string_append(&mensajeInicial, "1");
	string_append(&mensajeInicial, header(pcbNuevo->PID));
	string_append(&mensajeInicial, header((paginasNecesarias)));
	string_append(&mensajeInicial, "\0");
	printf("%s, Long:%d\n", mensajeInicial, string_length(mensajeInicial));
	send(conexionUMC, mensajeInicial, string_length(mensajeInicial), 0);
	free(mensajeInicial);
	char* resp = malloc(2);
	recv(conexionUMC, resp, 1, 0);
	resp[1] = '\0';
	if (!strcmp(resp, "1")) {
		agregarHeader(&codigo);
		send(conexionUMC, codigo, string_length(codigo), 0);
		free(codigo);
		queue_push(colaNuevos, (pcb*) pcbNuevo);
	} else {
		queue_push(colaNuevos, (pcb*) pcbNuevo);
		printf("Ansisop rechazado\n");
		//free(pcbNuevo);
		ultimoPID--;
	}
	free(resp);
	//list_iterate(pcbNuevo->indiceCodigo, (void*) mostrar);		//Ver inicio y offset de cada sentencia
}


pcb* crearPCB(char* codigo) {
	pcb* pcbProceso=malloc(sizeof(pcb));
//	printf("***CODIGO:%s\n", codigo);
	t_metadata_program *metadata = metadata_desde_literal(codigo);
	pcbProceso->PID=ultimoPID++;
	pcbProceso->PC = metadata->instruccion_inicio;					//Pos de la primer instruccion
	pcbProceso->indiceCodigo=crearIndiceDeCodigo(metadata);
	pcbProceso->pagsCodigo = metadata->instrucciones_size;
	return pcbProceso;
}

int calcularPaginas(char* codigo){
	int offset,acum=0,cantMarcos,totalMarcos=0;
	do {
		for (offset = 0; codigo[acum] != '\n'; offset++, acum++) {
	//		printf("%c", codigo[acum]);
		}
		cantMarcos = offset / tamPagina;
		if (offset % tamPagina)	cantMarcos++;
		totalMarcos += cantMarcos;
	//	printf("	-Cant marcos: %d | Total %d\n", cantMarcos, totalMarcos);
		acum++;
	} while (acum < string_length(codigo));
	return totalMarcos;
}

int cortarInstrucciones(t_metadata_program* meta) {							//!!![[RECORDAR]]!!! NO TIENE EN CUENTA COMENTARIOS!
	t_intructions* unaInstruccion = meta->instrucciones_serializado;
	t_list* lineas = list_create();
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
		list_iterate(lineas,mostrar);
	int cantPaginas=list_size(lineas);				//-1 para que empiece desde 0
	list_clean(lineas);
	list_destroy(lineas);
	return cantPaginas;
}

t_list* crearIndiceDeCodigo(t_metadata_program* meta){
 	t_list* lineas=list_create();
 	t_intructions* unaInstruccion=meta->instrucciones_serializado;
 	printf("\ninstrucciones:%d\n",meta->instrucciones_size); //=5
 	int i;
 	for (i=0;i<(meta->instrucciones_size);i++,unaInstruccion++){
 			int* unaLinea=malloc(sizeof(int*));									//0=inicio, 1=offset
 			unaLinea[0]=unaInstruccion->start;
 			unaLinea[1]=unaInstruccion->offset;
 			list_add(lineas, (int*)unaLinea);					//Guarda el PUNTERO al int
 		}
 	return lineas;
 	list_clean(lineas);
 	list_destroy(lineas);
 }

 void mostrar(int* sentencia){
 	printf("Inicio:%d | Offset:%d\n",sentencia[0],sentencia[1]);
 }


