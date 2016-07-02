/*
 * nucleo.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */

#include <sys/select.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <parser/metadata_program.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include "Funciones/Comunicacion.h"
#include "Funciones/json.h"

#define EVENT_SIZE  ( sizeof (struct inotify_event) + 24 )
#define BUF_LEN     ( 1024 * EVENT_SIZE )

typedef struct{
	PCB* pcb;
	int ut;
}pcbParaES;


t_list *cpus, *consolas, *listConsolasParaEliminarPCB, *cpusDisponibles;

//estructuras para planificacion
pthread_attr_t attr;
pthread_t thread;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_queue *colaNuevos, *colaListos,*colaTerminados;
sem_t sem_Nuevos, sem_Listos,sem_Terminado;



t_dictionary* crearDiccionarioGlobales(char** keys);
t_dictionary* crearDiccionarioSEMyES(char** keys, char** init, int esIO);

PCB* crearPCB(char*);
int calcularPaginas(char*);
void enviarAnsisopAUMC(int, char*,int);
void maximoDescriptor(int* maximo, t_list* lista, fd_set *descriptores);
void atender_Nuevos();
void atender_Ejecuciones();
void atender_Bloq_ES(int posicion);
void atender_Bloq_SEM(int posicion);
void atender_Terminados();
void atenderOperacion(int op,int cpu);
void procesar_operacion_privilegiada(int operacion, int cpu);
void sacar_socket_de_lista(t_list* lista,int socket);
int esa_consola_existe(int consola);
int ese_PCB_hay_que_eliminarlo(int consola);
int revisarActividadConsolas(fd_set*);
int revisarActividadCPUs(fd_set*);
char* serializarMensajeCPU(PCB* pcbListo, int quantum, int quantum_sleep);
void enviarPCBaCPU(int, char*);
void finalizarProgramaUMC(int id);
void finalizarProgramaConsola(int consola, int codigo);
void enviarTextoConsola(int consola, char* texto);
void Modificacion_quantum();
int buscar_pcb_en_bloqueados(int pid);
int buscar_pcb_en_cola(t_queue* cola, int pid);


datosConfiguracion* datosNucleo;
t_dictionary *globales,*semaforos,*dispositivosES;
int tamPagina=0,*dispositivosSleeps, *globalesValores, *contadorSemaforo, conexionUMC, cantidad_io, cantidad_sem;
sem_t *semaforosES,*semaforosGlobales;
t_queue **colasES,**colasSEM;
t_log* archivoLog;


int main(int argc, char* argv[]) {
	archivoLog = log_create("Nucleo.log", "Nucleo", true, log_level_from_string("INFO"));
	fd_set descriptores;
	cpus = list_create();
	consolas = list_create();
	listConsolasParaEliminarPCB= list_create();
	int max_desc, nuevo_cliente,sin_size = sizeof(struct sockaddr_in) ;
	struct sockaddr_in direccionCliente;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

	//--------------------------------CONFIGURACION-----------------------------

	datosNucleo=malloc(sizeof(datosConfiguracion));
	if (!(leerConfiguracion("ConfigNucleo", &datosNucleo) || leerConfiguracion("../ConfigNucleo", &datosNucleo))){
			log_error(archivoLog,"No se pudo leer archivo de Configuracion");
			return 1;}
//-------------------------------------------DICCIONARIOS---------------------------------------------------------------
	globales = crearDiccionarioGlobales(datosNucleo->shared_vars);
	semaforos = crearDiccionarioSEMyES(datosNucleo->sem_ids,datosNucleo->sem_init, 0);
	dispositivosES = crearDiccionarioSEMyES(datosNucleo->io_ids,datosNucleo->io_sleep,1);

	//---------------------------------PLANIFICACION PCB-----------------------------------

	sem_init(&sem_Nuevos, 0, 0);
	sem_init(&sem_Listos, 0, 0);
	sem_init(&sem_Terminado, 0, 0);

	cpusDisponibles=list_create();
	colaNuevos=queue_create();
	colaListos=queue_create();
	colaTerminados=queue_create();

	pthread_create(&thread, &attr, (void*)Modificacion_quantum, NULL);

	pthread_create(&thread, &attr, (void*)atender_Ejecuciones, NULL);
	pthread_create(&thread, &attr, (void*)atender_Nuevos, NULL);
	pthread_create(&thread, &attr, (void*)atender_Terminados, NULL);

	//------------------------------------CONEXION UMC--------------------------------
	int nucleo_servidor = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in direccionNucleo = crearDireccion(datosNucleo->puerto_nucleo, datosNucleo->ip);
	log_info(archivoLog,"Nucleo creado, conectando con la UMC...");
	conexionUMC = conectar(datosNucleo->puerto_umc, datosNucleo->ip_umc);
	tamPagina=autentificarUMC(conexionUMC);

	if (!tamPagina) {log_error(archivoLog,"Falló el handshake");
		return -1;}

	log_info(archivoLog,"Aceptados por la umc");

	if(!bindear(nucleo_servidor,direccionNucleo)){
		log_error(archivoLog,"Error en el bind, Fijate bien la proxima...!");
		return 1;}

	max_desc = conexionUMC;

	log_info(archivoLog,"Esperando nuevas conexiones");
	listen(nucleo_servidor, 100);

	int socketARevisar;

	//****/////////////////////////////////////////////////////////////////////////////////////////////////////////////*
	//****-------------------------------------------ACA Arranca la Magia----------------------------------------------*
	//****/////////////////////////////////////////////////////////////////////////////////////////////////////////////*

	while (1) {

		FD_ZERO(&descriptores);
		FD_SET(nucleo_servidor, &descriptores);
		FD_SET(conexionUMC, &descriptores);
		max_desc = conexionUMC;

		maximoDescriptor(&max_desc, consolas, &descriptores);
		maximoDescriptor(&max_desc, cpus, &descriptores);

		if (select(max_desc + 1, &descriptores, NULL, NULL, NULL) < 0) {
			log_error(archivoLog,"Error en el select");
			//exit(EXIT_FAILURE);
		}
		socketARevisar = revisarActividadConsolas(&descriptores);
		if (socketARevisar) {								//Reviso actividad en consolas
			log_info(archivoLog,"Se desconecto la consola en %d, eliminada",socketARevisar);
			int estaBloqueada = buscar_pcb_en_bloqueados(socketARevisar);
			if(estaBloqueada==socketARevisar){
				log_info(archivoLog,"se elimino el proceso %d que estaba bloqueado");//todo avisar
			 	finalizarProgramaUMC(socketARevisar);
			}
			//close(socketARevisar);
		}
		else {
			socketARevisar = revisarActividadCPUs(&descriptores);
			if (socketARevisar) {								//Reviso actividad en cpus
				log_info(archivoLog,"Se desconecto el CPU en %d, eliminado",socketARevisar);
				//close(socketARevisar);
			}
			else {
					if (FD_ISSET(conexionUMC, &descriptores)) {					//Me mando algo la UMC
						if (recibirProtocolo(conexionUMC) == -1) {
							log_error(archivoLog,"Murio la UMC, bye");
							return 0;
						}
					}else{
						if (FD_ISSET(nucleo_servidor, &descriptores)) { 			//aceptar cliente
						nuevo_cliente = accept(nucleo_servidor,(void *) &direccionCliente, (void *) &sin_size);
						if (nuevo_cliente == -1) {
							log_error(archivoLog,"Fallo el accept");
						}
						int mjeCpu=htonl(1);
						switch (comprobarCliente(nuevo_cliente)) {

						case 0:										//ERROR!!
							log_error(archivoLog,"Falló el handshake\n");
							close(nuevo_cliente);
							break;

						case 1:											//CPU
							send(nuevo_cliente, &mjeCpu, 4, 0);
							log_info(archivoLog,"Acepté un nuevo cpu, el %d",nuevo_cliente);
							list_add(cpusDisponibles, (void *)nuevo_cliente);
							list_add(cpus, (void *) nuevo_cliente);
							break;

						case 2:						//CONSOLA, RECIBO EL CODIGO
							send(nuevo_cliente, "0001", 4, 0);
							list_add(consolas, (void *) nuevo_cliente);
							log_info(archivoLog,"Acepté una nueva consola, la %d",nuevo_cliente);
							int tamanio = recibirProtocolo(nuevo_cliente);
							if (tamanio > 0) {
								char* codigo = (char*) recibirMensaje(nuevo_cliente, tamanio);
								enviarAnsisopAUMC(conexionUMC, codigo,nuevo_cliente);
								free(codigo);
							}
							break;
						}
					}
				}
			}
		}
	}
	free(datosNucleo);
	return 0;
}

//--------------------------------------LECTURA CONFIGURACION


t_dictionary* crearDiccionarioGlobales(char** keys){
	int i=0;
	t_dictionary* diccionario=dictionary_create();
	while(keys[i]!=NULL){
		dictionary_put(diccionario,keys[i],(int*) i);
		i++;
	}
	globalesValores=malloc(i * sizeof(uint32_t));
	i--;
	for(;i>=0;i--){
		globalesValores[i]=0;
	}
	return diccionario;
}

t_dictionary* crearDiccionarioSEMyES(char** keys, char** init, int esIO){
	int i=0;
	t_dictionary* diccionario=dictionary_create();
	while(keys[i]!=NULL){
		dictionary_put(diccionario,keys[i],(int*) i);
		i++;
	}
	i--; //me pase, voy a la ultima que tiene algo
	if(esIO){
		cantidad_io=i;
		dispositivosSleeps = malloc((i+1)*sizeof(uint32_t));
		semaforosES = malloc((i+1)*sizeof(sem_t));
		colasES = malloc((i+1)*sizeof(t_queue));
		for(;i>=0;i--){
			dispositivosSleeps[i] = atoi(init[i]);		//vector de ints con los sleeps
			sem_init(&semaforosES[i], 0, 0);			//vector con los semaforos de cada e/s
			colasES[i] = queue_create();				//vector de colas
			pthread_create(&thread, &attr, (void*)atender_Bloq_ES, (void*)i);
		}
	}else{
		cantidad_sem=i;
		colasSEM = malloc((i+1)*sizeof(t_queue));
		semaforosGlobales=malloc((i+1)*sizeof(sem_t));
		contadorSemaforo = malloc((i+1)*sizeof(uint32_t));
		for(;i>=0;i--){
			sem_init(&semaforosGlobales[i], 0, 0);		//vector de semaforos de los hilos
			contadorSemaforo[i] = atoi(init[i]);			//vector de "semaforos" de las variables globales
			colasSEM[i] = queue_create();						//vector de colas
			pthread_create(&thread, &attr, (void*)atender_Bloq_SEM, (void*)i);
		}
	}
	return diccionario;
}



//----------------------------------------PCB------------------------------------------------------

void enviarAnsisopAUMC(int conexionUMC, char* codigo,int consola){
	int paginasNecesarias=calcularPaginas(codigo);
	char* mensaje = string_new();
	string_append(&mensaje, "1");
	char* consol=header(consola);
	string_append(&mensaje, consol);
	free(consol);
	char* pags=header(paginasNecesarias+datosNucleo->tamStack);
	string_append(&mensaje, pags);
	free(pags);
	agregarHeader(&codigo);
	string_append(&mensaje,codigo);
	//printf("%s\n",codigo);
	send(conexionUMC, mensaje, string_length(mensaje), 0);
	free(mensaje);
	int aceptado;
	recv(conexionUMC, &aceptado, sizeof(int), MSG_WAITALL);
	aceptado=ntohl(aceptado);
	PCB* pcbNuevo;
	if(!aceptado){													//consola rechazada
		log_info(archivoLog,"Ansisop rechazado");
		send(consola,"0000",4,0);}
	else{
			send(consola,"0001",4,0);
			pcbNuevo = crearPCB(string_substring_from(codigo,4));
			pcbNuevo->id=consola;							//Se le asigna al proceso como ID el numero de consola que lo envía.
	if(aceptado==1){
			log_info(archivoLog,"Código enviado a la UMC");
			log_info(archivoLog,"Nuevo PCB en cola de Listos!");
			queue_push(colaListos, pcbNuevo);
			sem_post(&sem_Listos);
	}else{
			log_info(archivoLog,"Código enviado a la UMC");
			log_info(archivoLog,"Nuevo PCB en cola de Nuevos!");
			queue_push(colaNuevos, pcbNuevo);
			sem_post(&sem_Nuevos);
		}
	}
	free(codigo);
}


PCB* crearPCB(char* codigo) {
	PCB* pcb=malloc(sizeof(PCB));
	t_metadata_program *metadata = metadata_desde_literal(codigo);
	pcb->indices = *metadata;
	pcb->paginas_codigo = calcularPaginas(codigo);
	pcb->pc = metadata->instruccion_inicio;
	pcb->stack = list_create();
	return pcb;
}

int calcularPaginas(char* codigo){
	int totalPaginas=string_length(codigo)/tamPagina;
	if (string_length(codigo)%tamPagina) totalPaginas++;
	return totalPaginas;
}

//----------------------------DESCRIPTORES (SELECT)------------------------------------------------------------------

void maximoDescriptor(int* maximo, t_list* lista, fd_set *descriptores){
int i;
for (i = 0; i < list_size(lista); i++) {
	int conset = (int) list_get(lista, i); //conset = consola para setear
	FD_SET(conset, descriptores);
	if (conset > *maximo) {
		*maximo = conset;
		}
	}
}

int revisarActividadConsolas(fd_set *descriptores) {
	int i;
	for (i = 0; i < list_size(consolas); i++) {
		int componente = (int) list_get(consolas, i);
		if (FD_ISSET(componente, descriptores)) {
			int protocolo = recibirProtocolo(componente);
			if (protocolo == -1) {				//si murio de golpe, tengo que eliminar el pcb
				list_add(listConsolasParaEliminarPCB,(void *) componente);
				list_remove(consolas, i);
				return componente;
			}else{
				list_remove(consolas, i);		//sino, me manda un 1, que termino bien
				log_info(archivoLog,"Se desconecto la consola en %d, ya termino su programa, eliminada",componente);
			}
		}
	}
	return 0;
}
int revisarActividadCPUs(fd_set *descriptores) {
	int i;
	for (i = 0; i < list_size(cpus); i++) {
		int componente = (int) list_get(cpus, i);
		if (FD_ISSET(componente, descriptores)) {
			int protocolo = recibirProtocolo(componente);
			if (protocolo == -1) {
				list_remove(cpus, i);
				sacar_socket_de_lista(cpusDisponibles,componente);//lo remueve de disponibles si no lo saco el hilo execute
				return componente;
			} else {							//el cpu me mando un mensaje, la consola nunca lo va a hacer
				atenderOperacion(protocolo, componente);
			}
		}
	}
	return 0;
}
//--------------------------------------------PLANIFICACION----------------------------------------------------

void atender_Nuevos(){
	while(1){
		 sem_wait(&sem_Nuevos); //se libero un pcb en la umc
		 if(!queue_is_empty(colaNuevos)){ //si hay alguno
			 PCB* pcbNuevo;
			 pcbNuevo = queue_pop(colaNuevos);
		 	 queue_push(colaListos,pcbNuevo); //entonces lo mando a Listos
			 log_info(archivoLog,"El proceso %d paso de Nuevo a Listo",pcbNuevo->id);
		 	 sem_post(&sem_Listos);
		 }
	}
}

void atender_Ejecuciones(){
	 char* mensajeCPU;
	 while(1){
		 sem_wait(&sem_Listos);
		 PCB* pcbListo;
		 pcbListo = queue_pop(colaListos);
		 if(ese_PCB_hay_que_eliminarlo(pcbListo->id)){
			 log_info(archivoLog,"La consola del proceso %d no existe mas, se lo eliminara",pcbListo->id);
		 	 finalizarProgramaUMC(pcbListo->id);
		 	 sem_post(&sem_Nuevos);
		 }else{
		 int paso=1;
		 	 while(paso){
		 		 if(!list_is_empty(cpusDisponibles)){
					int cpu = (int)list_remove(cpusDisponibles, 0); //saco el socket de ese cpu disponible
					mensajeCPU = serializarMensajeCPU(pcbListo, datosNucleo->quantum, datosNucleo->quantum_sleep);
				 	send(cpu,mensajeCPU,string_length(mensajeCPU),0);
					log_info(archivoLog,"El proceso %d paso de Listo a Execute",pcbListo->id);
					paso=0;
					free(mensajeCPU);
				}
		 	 }
		 }
		 liberarPCBPuntero(pcbListo);
	 }
 }

 void atender_Bloq_ES(int posicion){
	 int miSLEEP = dispositivosSleeps[posicion];
	 while(1){
	 	 sem_wait(&semaforosES[posicion]);
	 	 pcbParaES* pcbBloqueando = queue_pop(colasES[posicion]);
	 	 usleep(miSLEEP*pcbBloqueando->ut*1000);
		 queue_push(colaListos, pcbBloqueando->pcb);
		 log_info(archivoLog,"El proceso %d paso de Bloqueado (IO) a Listo",pcbBloqueando->pcb->id);
		 sem_post(&sem_Listos);
		 liberarPCBPuntero(pcbBloqueando->pcb);
	 }
 }

 void atender_Bloq_SEM(int posicion){
	 while(1){
		 sem_wait(&semaforosGlobales[posicion]);
		 	 if(!queue_is_empty(colasSEM[posicion])){
		 		 PCB* pcbBloqueando = queue_pop(colasSEM[posicion]);
		 		 queue_push(colaListos, pcbBloqueando);
		 		 log_info(archivoLog,"El proceso %d paso de Bloqueado (SEM) a Listo",pcbBloqueando->id);
		 	 	 sem_post(&sem_Listos);
		 	 }
	 }
}

 void atender_Terminados(){
	 int cod=2;
	 while(1){
		 sem_wait(&sem_Terminado);
	 	 PCB* pcbTerminado;
	 	 pcbTerminado = queue_pop(colaTerminados);
	 	 finalizarProgramaUMC(pcbTerminado->id);
	 	 finalizarProgramaConsola(pcbTerminado->id, cod);
	 	 log_info(archivoLog,"El proceso %d Termino su ejecucion\n",pcbTerminado->id);
	 	 sem_post(&sem_Nuevos);
	 	liberarPCBPuntero(pcbTerminado);
	 }
 }


void atenderOperacion(int op,int cpu){
#define ERROR 0
#define QUANTUM_OK 1
#define PRIVILEGIADA 2
#define FIN_ANSISOP 3
#define IMPRIMIR 4

		int tamanio, consola, operacion, pidMalo, sigueCPU;
		char* texto;
		PCB *pcbDesSerializado;
	switch (op){
	case ERROR:
		//el cpu se desconecto y termino mal el q, o hubo un error
		pidMalo = recibirProtocolo(cpu);
		if(pidMalo){
			operacion = 3;
		 	finalizarProgramaConsola(pidMalo, operacion);
		 	finalizarProgramaUMC(pidMalo);
		 	log_info(archivoLog,"Hubo un error en la ejecucion del proceso %d, eliminado",pidMalo);
		 	sem_post(&sem_Nuevos);
		}
		sigueCPU = recibirProtocolo(cpu);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void*)cpu);
 		}else{
 			sacar_socket_de_lista(cpus,cpu);//todo error de ansisop pero sigue el cpu
 			log_info(archivoLog,"Se desconecto el CPU en %d, eliminado (v2)",cpu);
 		}
		break;
	case QUANTUM_OK:
		//termino bien el quantum, no necesita nada
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu,tamanio);
		pcbDesSerializado = fromStringPCB(texto);
		sigueCPU = recibirProtocolo(cpu);
 		log_info(archivoLog,"El proceso %d paso de Execute a Listo",pcbDesSerializado->id);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void*)cpu);
 		}
		queue_push(colaListos, pcbDesSerializado);
		sem_post(&sem_Listos);
		free(texto);
		break;
	case PRIVILEGIADA:
		//me pide una operacion privilegiada
		operacion = recibirProtocolo(cpu);
		procesar_operacion_privilegiada(operacion, cpu);
		break;
	case FIN_ANSISOP:
		//termino el ansisop, va a Terminado
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu,tamanio);
		pcbDesSerializado = fromStringPCB(texto);
		sigueCPU = recibirProtocolo(cpu);
		log_info(archivoLog,"El proceso %d paso de Execute a Terminado",pcbDesSerializado->id);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void *)cpu);
 		}
		queue_push(colaTerminados, pcbDesSerializado);
		sem_post(&sem_Terminado);
		free(texto);
		break;
	case IMPRIMIR:
		//imprimir o imprimirTexto
		consola = recibirProtocolo(cpu);
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu, tamanio);   //texto o valor
		if(esa_consola_existe(consola)){
			enviarTextoConsola(consola, texto);
		}
		free(texto);
		send(cpu,"0001",4,0);
		break;
	}
}

void procesar_operacion_privilegiada(int operacion, int cpu){
#define ERROR 0
#define OBTENER_COMPARTIDA 1
#define GUARDAR_COMPARTIDA 2
#define WAIT 3
#define SIGNAL 4
#define E_S 5

		int tamanioNombre, posicion, unidadestiempo,valor,tamanio,sigueCPU;
		char *identificador,*texto,*valor_char, *ut;
		PCB *pcbDesSerializado;
		if (operacion){
			tamanioNombre = recibirProtocolo(cpu);
			identificador = recibirMensaje(cpu,tamanioNombre);
		}
	switch (operacion){
	case ERROR:
		perror("el cpu mando mal la operacion privilegiada, todo mal\n");
		//error? no deberia entrar aca
		break;
	case OBTENER_COMPARTIDA:
		//obtener valor de variable compartida
		//recibo nombre de variable compartida, devuelvo su valor
		posicion = (int)dictionary_get(globales,identificador);
		valor = globalesValores[posicion];
		valor=htonl(valor);
		send(cpu,&valor,4,0);
		free(identificador);
		break;
	case GUARDAR_COMPARTIDA:
		//grabar valor en variable compartida
		//recibo el nombre de una variable y un valor -> guardo valor y devuelvo
		tamanio=recibirProtocolo(cpu);
		valor_char=recibirMensaje(cpu,tamanio);
		valor=atoi(valor_char);
		posicion = (int)dictionary_get(globales,identificador);
		globalesValores[posicion] = valor;
		valor=htonl(valor);
		send(cpu,&valor,4,0);
		free(valor_char);
		free(identificador);
		break;
	case WAIT:
		//wait a un semaforo, si no puiede acceder, se bloquea
		//recibo el identificador del semaforo
		posicion = (int)dictionary_get(semaforos,identificador);
		contadorSemaforo[posicion]--;
		if(contadorSemaforo[posicion]<0){					//si es < a 0, se tiene que bloquear el pcb
			send(cpu, "no", 2, 0);						//=> Pido el pcb
			tamanio = recibirProtocolo(cpu); 			//tamaño del pcb
			texto = recibirMensaje(cpu,tamanio);		//PCB
			pcbDesSerializado = fromStringPCB(texto);
			sigueCPU = recibirProtocolo(cpu);
			queue_push(colasSEM[posicion], pcbDesSerializado); //mando el pcb a bloqueado
			log_info(archivoLog,"El proceso %d paso de Execute a Bloqueado",pcbDesSerializado->id);
	 		if(sigueCPU){
				list_add(cpusDisponibles, (void *)cpu);
	 		}
	 		free(texto);
		}else{											//si no, ok
			send(cpu, "ok", 2, 0);
		}
		free(identificador);
		break;
	case SIGNAL:
		//signal a un semaforo, post
		//recibo el identificador del semaforo
		send(cpu,"0001",4,0);
		posicion = (int)dictionary_get(semaforos,identificador);
		contadorSemaforo[posicion]++;
		if(contadorSemaforo[posicion]<=0){
			sem_post(&semaforosGlobales[posicion]); //si era < 0, tengo alguien que desbloquear
		}
		free(identificador);
		break;
	case E_S:
		//pedido E/S, va a bloqueado
		//recibo nombre de dispositivo, y unidades de tiempo a utilizar
		tamanio=recibirProtocolo(cpu);
		ut=recibirMensaje(cpu,tamanio);
		unidadestiempo = atoi(ut);
		posicion = (int)dictionary_get(dispositivosES,identificador);
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu,tamanio);
		pcbDesSerializado = fromStringPCB(texto);
		sigueCPU = recibirProtocolo(cpu);

		pcbParaES *pcbParaBloquear=malloc(sizeof(pcbParaES)+sizeof(PCB));		//todo revisar, pero creo que ahora guarda bien
		pcbParaBloquear->pcb = pcbDesSerializado;
		pcbParaBloquear->ut = unidadestiempo;
		queue_push(colasES[posicion], pcbParaBloquear);
		log_info(archivoLog,"El proceso %d paso de Execute a Bloqueado",pcbDesSerializado->id);
		sem_post(&semaforosES[posicion]);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void *)cpu);
 		}
 		free(ut);
 		free(identificador);
 		free(texto);
 	//	free(pcbDesSerializado);			//ACA
		break;
	}
}
void sacar_socket_de_lista(t_list* lista,int socket){
	int buscarIgual(int elemLista){
		return (socket==elemLista);}
	list_remove_by_condition(lista,(void*)buscarIgual);
}
int esa_consola_existe(int consola){
	int buscarIgual(int elemLista){
		return (consola==elemLista);}
	if(list_any_satisfy(consolas,(void*)buscarIgual)){
		return 1;}
	return 0;
}

int ese_PCB_hay_que_eliminarlo(int consola){ //devuelve si esa consola esta en la lista de eliminadas
	int buscarIgual(int elemLista){
		return (consola==elemLista);}

	if(list_any_satisfy(listConsolasParaEliminarPCB,(void*)buscarIgual)){
		list_remove_by_condition(listConsolasParaEliminarPCB,(void*)buscarIgual);
		return 1;}
	return 0;
}

char* serializarMensajeCPU(PCB* pcbListo, int quantum, int quantum_sleep){
	char* mensaje=string_new();
		char* quantum_char = toStringInt(quantum);
		char* quantum_sleep_char = toStringInt(quantum_sleep);
		char* pcb_char = toStringPCB(*pcbListo);
		string_append(&mensaje,quantum_char);
		string_append(&mensaje,quantum_sleep_char);
		agregarHeader(&pcb_char);
		string_append(&mensaje,pcb_char);
		string_append(&mensaje,"\0");
		//printf("Mensaje: %s\n",mensaje); pcb serializado
		free(quantum_char);
		free(quantum_sleep_char);
		free(pcb_char);

		return mensaje;
}
void finalizarProgramaUMC(int id){
	 char* mensaje = string_new();
	 string_append(&mensaje, "4");
	 char* idchar=header(id);
	 string_append(&mensaje, idchar);
	 free(idchar);
	 send(conexionUMC, mensaje, string_length(mensaje), 0);
	 free(mensaje);
}
void finalizarProgramaConsola(int consola, int codigo){
	//codigo: el ansisop termino 2=ok / 3=mal
	char* cod = header(codigo);
	if(esa_consola_existe(consola)){
		send(consola, cod, 4, 0);
	}
	free(cod);
}

void enviarTextoConsola(int consola, char* texto){
	 char* mensaje = string_new();
	 string_append(&mensaje, "0001");
	 agregarHeader(&texto);
	 string_append(&mensaje, texto);
	 string_append(&mensaje,"\0");
	 send(consola, mensaje, string_length(mensaje), 0);
	 free(mensaje);
}

void enviarPCBaCPU(int cpu, char* pcbSerializado){
	char* mensaje = string_new();
	agregarHeader(&pcbSerializado);
	string_append(&mensaje,pcbSerializado);
	string_append(&mensaje,"\0");
	send(cpu, mensaje, string_length(mensaje), 0);
	free(mensaje);
}

void Modificacion_quantum(){
	char buffer[BUF_LEN];
	int fd_config = inotify_init();
	if (fd_config < 0) {
		perror("inotify_init");
	}
	int watch_descriptor = inotify_add_watch(fd_config, "../ConfigNucleo", IN_MODIFY);//IN_CLOSE_WRITE);IN_MODIFY

	while(watch_descriptor){
		t_config* archivoConfiguracion;
		int length = read(fd_config, buffer, BUF_LEN);
		if (length < 0) {
			perror("read");
		}
		do{
		archivoConfiguracion = config_create("../ConfigNucleo");
		}while(archivoConfiguracion == NULL);
		(datosNucleo)->quantum = config_get_int_value(archivoConfiguracion, "QUANTUM");
		(datosNucleo)->quantum_sleep = config_get_int_value(archivoConfiguracion,"QUANTUM_SLEEP");
		log_info(archivoLog,"el quantum ahora es: %d | y el quantum_sleep: %d",(datosNucleo)->quantum, (datosNucleo)->quantum_sleep);

		config_destroy(archivoConfiguracion);
	}
}

int buscar_pcb_en_bloqueados(int pid){
	//busco entre las de ES y despues las de SEM
	//semaforo para bloquearo los hilos de las colas? todo
	int i, encontro;
	for(i=0;i<=cantidad_sem;i++){
		encontro = buscar_pcb_en_cola(colasSEM[i], pid);
		if(encontro){
			return encontro;
		}
	}
	for(i=0;i<=cantidad_io;i++){
		encontro = buscar_pcb_en_cola(colasES[i],pid);
		if(encontro){
			return encontro;
		}
	}
	return 0;
}
int buscar_pcb_en_cola(t_queue* cola, int pid){
	int buscarIgual(PCB* elemLista){
		return (pid==elemLista->id);}
	PCB* eliminar = list_find(cola->elements,(void*)buscarIgual);
	list_remove_by_condition(cola->elements,(void*)buscarIgual);
	if(eliminar!=NULL){
		return eliminar->id;
		free(eliminar);
	}
	return 0;
}
