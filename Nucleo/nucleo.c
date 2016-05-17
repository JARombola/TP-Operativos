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
#include <semaphore.h>
#include <pthread.h>

//todo #define IP_UMC
#define PUERTO_UMC 6661
#define PUERTO_NUCLEO 6662
#define QUANTUM 3
#define QUANTUM_SLEEP 500

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

t_list *cpus, *consolas;

//estructuras para planificacion
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_queue* colaListos;
t_queue* colaExec;
t_queue* colaBloq;
t_queue* colaTerminados;
sem_t sem_Listos;
sem_t sem_Exec;
sem_t sem_Bloq;
sem_t sem_Terminado;
sem_t sem_cpuDisponible;

int autentificarUMC(int);
void leerConfiguracion(char*, datosConfiguracion*);
struct sockaddr_in crearDireccion(int puerto);
int conectarUMC(int);
int comprobarCliente(int);
int recibirProtocolo(int);
void* recibirMensaje(int, int);

pcb* crearPCB(char*,int,int*);
t_list* crearIndiceDeCodigo(t_metadata_program*);
int cantPaginas(t_metadata_program*,int);
void mostrar(int*);
char* header(int);
char* agregarHeader(char*);

void atender_Listos();
void atender_Exec();
void atender_Bloq();
void atender_Terminados();
void  mandar_instruccion_a_CPU();
void procesar_respuesta();


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
	//---------------------------------PLANIFICACION PCB-----------------------------------
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);//todo agregar el destroy
	pthread_t hiloListos, hiloExec, hiloBloq, hiloTerminados;

	pthread_create(&hiloListos, &attr, (void*)atender_Listos, NULL);
	//pthread_create(&hiloExec, &attr, (void*)atender_Exec, NULL);
	pthread_create(&hiloBloq, &attr, (void*)atender_Bloq, NULL);//todo un hilo por cada e/s, y por parametro el sleep
	pthread_create(&hiloTerminados, &attr, (void*)atender_Terminados, NULL);

	colaListos=queue_create();
	colaExec=queue_create();
	colaBloq=queue_create();
	colaTerminados=queue_create();

	sem_init(&sem_Listos, 0, 0);
	sem_init(&sem_Exec, 0, 0);
	sem_init(&sem_Bloq, 0, 0);
	sem_init(&sem_Terminado, 0, 0);
	sem_init(&sem_cpuDisponible, 0, 0);


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
					//todo si el proceso no termino, hay que eliminarlo, y avisar a todos
					list_remove(consolas, i);
					close(unaConsola);
				} else {
					char* bufferConsola = malloc(protocolo + 1);
					char* mensaje = recibirMensaje(unaConsola, protocolo);
					free(bufferConsola);
					printf("mensaje de consola: %s",mensaje);
					free(mensaje);
					//no me deberia mandar nada la consola

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
					sem_wait(&sem_cpuDisponible);
					list_remove(cpus, i);
					close(unCPU);
				} else {
					char* bufferCpu = malloc(protocolo + 1);
					int mensaje = atoi(recibirMensaje(unCPU,protocolo));
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
				sem_post(&sem_cpuDisponible);
				pthread_create(&hiloExec, &attr, (void*)atender_Exec, (void*)nuevo_cliente);
				printf("Acepté un nuevo cpu\n");
				break;
			case 2:															//CONSOLA, RECIBO EL CODIGO
				send(nuevo_cliente, "1", 1, 0);
				list_add(consolas, (void *) nuevo_cliente);
				printf("Acepté una nueva consola\n");
				pcb* pcbNuevo = malloc(sizeof(pcb));
				int tamanio = recibirProtocolo(nuevo_cliente);
				if (tamanio > 0) {
					char* codigo = recibirMensaje(nuevo_cliente, tamanio);
					int paginasNecesarias;
					char* mensajeInicial = malloc(10);
					pcbNuevo = crearPCB(codigo, tamPagina, &paginasNecesarias);
					memcpy(mensajeInicial, "1", 1);
					memcpy(mensajeInicial + 1, header(pcbNuevo->PID), 4);
					memcpy(mensajeInicial + 5, header(paginasNecesarias), 4);
					memcpy(mensajeInicial + 9, "\0", 1);
					printf("%s\n", mensajeInicial);
					send(conexionUMC, mensajeInicial, string_length(mensajeInicial), 0);
					//todo si recibo 0, no hay espacio, elimino pcb
					//si recibo 1
					printf("el pcb %d se guardo en la umc y paso a la cola de Listos",pcbNuevo->PID);
					queue_push(colaListos, pcbNuevo);
					sem_post(&sem_Listos);
					free(mensajeInicial);
					free(codigo);
			/*		recv(conexionUMC, mensajeInicial, 1, 0);
					if (strcmp(mensajeInicial, "1")) {
					agregarHeader(codigo);
					send(conexionUMC, codigo, string_length(codigo), 0);
					queue_push(colaNuevos,(pcb*) pcbNuevo);
				}*/
				list_iterate(pcbNuevo->indiceCodigo, (void*) mostrar);		//Ver inicio y offset de cada sentencia
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

char* header(int tamanio){							//Recibe numero de bytes, y lo devuelve en 4 bytes (Ej. recibe "2" y devuelve "0002")
	char* longitud=string_new();
	longitud=string_reverse(string_itoa(tamanio));
	string_append(&longitud,"0000");
	longitud=string_substring(longitud,0,4);
	longitud=string_reverse(longitud);
	return longitud;
}

char* agregarHeader(char* mensaje){
	char* head=malloc(4);
	memcpy(head,header(string_length(mensaje)),4);
	mensaje=string_reverse(mensaje);
	string_append(&mensaje,string_reverse(head));
	mensaje=string_reverse(mensaje);
	free (head);
	return mensaje;
}
//----------------------------------------PCB------------------------------------------------------

pcb* crearPCB(char* codigo,int tamPagina,int *paginasNecesarias) {
	pcb* pcbProceso=malloc(sizeof(pcb));
//	printf("***CODIGO:\n%s\n", codigo);
	t_metadata_program *metadata = metadata_desde_literal(codigo);
	pcbProceso->PID=10;
	pcbProceso->PC = metadata->instruccion_inicio;					//Pos de la primer instruccion
	pcbProceso->indiceCodigo=crearIndiceDeCodigo(metadata);
	pcbProceso->pagsCodigo = metadata->instrucciones_size;
	*paginasNecesarias=cantPaginas(metadata,tamPagina);
	return pcbProceso;
}


int cantPaginas(t_metadata_program* meta, int tamPagina) {
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
	int cantPaginas=list_size(lineas)-1;				//-1 para que empiece desde 0
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
//----------------------------PLANIFICACION

 void atender_Listos(){
	 sem_wait(&sem_Listos);
	 pcb* pcbListo = malloc(sizeof(pcb));
	 pcbListo = queue_pop(colaListos);
	 int paso=1;
	 while(paso){
		 if(&sem_cpuDisponible>0 ){ //todo se puede el semaforo? sino un contador
			 printf("el proceso %d paso de Listo a Execute",pcbListo->PID);
			 queue_push(colaExec, pcbListo);
			 sem_post(&sem_Exec);
			 paso=0;
		 }
	 }
	 free(pcbListo);
 }
 void atender_Exec(int cpu){
	 sem_wait(&sem_Exec);
	 sem_wait(&sem_cpuDisponible);
	 pcb* pcbExec = malloc(sizeof(pcb));
	 pcbExec = queue_pop(colaListos);
	 int i,todoSigueIgual=1;
	 for(i=0; i<QUANTUM; i++){
		 mandar_instruccion_a_CPU(cpu,pcbExec,&todoSigueIgual);
		 pcbExec->PC++;
		 sleep(QUANTUM_SLEEP);
	 } //si en el medio del q se bloqueo o termino, todoSigueIgual=0
	 if(todoSigueIgual){
		 printf("el proceso %d paso de Execute a Listo",pcbExec->PID);
		 queue_push(colaListos, pcbExec);
		 sem_post(&sem_Listos);
	 }
	 sem_post(&sem_cpuDisponible);
	 free(pcbExec);

 }
 void atender_Bloq(){
	 //varias colas?
	 /*obtener_valor [identificador de variable compartida]
		grabar_valor [identificador de variable compartida] [valor a grabar]
		wait [identificador de semáforo]
		signal [identificador de semáforo]
		entrada_salida [identificador de dispositivo] [unidades de tiempo a utilizar]
	  */
 }
 void atender_Terminados(){
	 sem_wait(&sem_Terminado);
	 pcb* pcbTerminado = malloc(sizeof(pcb));
	 pcbTerminado = queue_pop(colaTerminados);
	 //todo avisar umc y consola que termino el programa
 }

void mandar_instruccion_a_CPU(int cpu, pcb*pcb, int igual){
	//todo concatenar pcbExec->PID,pcbExec->PC,pcbExec->SP
	//send(cpu, pcbTrucho, 12);

	procesar_respuesta(recibirProtocolo(cpu),cpu,  pcb, &igual);
}
void procesar_respuesta(int op,int cpu, pcb*pcb, int todoSigueIgual){
		int mostrar;
		int tamanio;
		char* texto;
	switch (op){
	case 1:
		//tengo que bloquearme,
		printf("el proceso %d paso de Execute a Bloqueado",pcb->PID);
		todoSigueIgual=0;
		queue_push(colaBloq, pcb);
		sem_post(&sem_Bloq);
		break;
	case 2:
		//termino el ansisop, va a listos
		printf("el proceso %d paso de Execute a Terminado",pcb->PID);
		todoSigueIgual=0;
		queue_push(colaTerminados, pcb);
		sem_post(&sem_Terminado);
		break;
	case 3:
		//imprimir
		//agregar header cod_op
		mostrar = recibirProtocolo(cpu);
		send(pcb->consola, mostrar, 4, 0);
		break;
	case 4:
		//imprimirTexto
		//agregar header cod_op
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu, tamanio);
		//send(pcb->consola, concatentar headers,(tamanio+4);
		break;

	}

}
