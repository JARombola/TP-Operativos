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
void mostrar(int*);
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


datosConfiguracion* datosNucleo;
t_dictionary *globales,*semaforos,*dispositivosES;
int tamPagina=0,*dispositivosSleeps, *globalesValores, *contadorSemaforo, conexionUMC;
sem_t *semaforosES,*semaforosGlobales;
t_queue **colasES,**colasSEM;


int main(int argc, char* argv[]) {
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
			printf("Error archivo de configuracion\n FIN.\n");return 1;}
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
	printf("Nucleo creado, conectando con la UMC...\n");
	conexionUMC = conectar(datosNucleo->puerto_umc, datosNucleo->ip_umc);
	tamPagina=autentificarUMC(conexionUMC);

	if (!tamPagina) {printf("Falló el handshake\n");
		return -1;}

	printf("Aceptados por la umc\n");

	if(!bindear(nucleo_servidor,direccionNucleo)){
		printf("Error en el bind, Fijate bien la proxima...!\n");
		return 1;}

	max_desc = conexionUMC;

	printf("Estoy escuchando\n");
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
			perror("Error en el select");
			//exit(EXIT_FAILURE);
		}
		printf("Entré al select\n");
		send(nuevo_cliente, "1", 1, 0);	socketARevisar = revisarActividadConsolas(&descriptores);
		if (socketARevisar) {								//Reviso actividad en consolas
			printf("Se desconecto la consola en %d, eliminada\n",socketARevisar);
			close(socketARevisar);
		}
		else {
			socketARevisar = revisarActividadCPUs(&descriptores);
			if (socketARevisar) {								//Reviso actividad en cpus
				printf("Se desconecto el CPU en %d, eliminado\n",socketARevisar);
				close(socketARevisar);
			}
			else {
					if (FD_ISSET(conexionUMC, &descriptores)) {					//Me mando algo la UMC
						if (recibirProtocolo(conexionUMC) == -1) {
							printf("Murio la UMC, bye\n");
							return 0;
						}
					}else{
						if (FD_ISSET(nucleo_servidor, &descriptores)) { 			//aceptar cliente
						nuevo_cliente = accept(nucleo_servidor,(void *) &direccionCliente, (void *) &sin_size);
						if (nuevo_cliente == -1) {
							perror("Fallo el accept");
						}
						printf("Nueva conexion\n");
						int mjeCpu=htonl(1);
						switch (comprobarCliente(nuevo_cliente)) {

						case 0:										//ERROR!!
							perror("Falló el handshake\n");
							close(nuevo_cliente);
							break;

						case 1:											//CPU
							send(nuevo_cliente, &mjeCpu, 4, 0);
							printf("Acepté un nuevo cpu, el %d\n",nuevo_cliente);
							list_add(cpusDisponibles, (void *)nuevo_cliente);
							list_add(cpus, (void *) nuevo_cliente);
							break;

						case 2:						//CONSOLA, RECIBO EL CODIGO
							send(nuevo_cliente, "0001", 4, 0);
							list_add(consolas, (void *) nuevo_cliente);
							printf("Acepté una nueva consola\n");
							int tamanio = recibirProtocolo(nuevo_cliente);
							if (tamanio > 0) {
								char* codigo = (char*) recibirMensaje(nuevo_cliente, tamanio);
								printf("--Codigo:%s--\n",codigo);
								enviarAnsisopAUMC(conexionUMC, codigo,nuevo_cliente);
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
	globalesValores=malloc(i * sizeof(uint32_t)); //deberia estar arriba del i-- ?
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
	string_append(&mensaje, header(consola));
	string_append(&mensaje, header(paginasNecesarias+datosNucleo->tamStack));
	agregarHeader(&codigo);
	string_append(&mensaje,codigo);
	//printf("%s\n",codigo);
	send(conexionUMC, mensaje, string_length(mensaje), 0);
	free(mensaje);
	int aceptado;
	recv(conexionUMC, &aceptado, sizeof(int), MSG_WAITALL);
	aceptado=ntohl(aceptado);
	printf("-------------------------ACEPTADO: %d\n",aceptado);
	PCB* pcbNuevo;
	if(!aceptado){													//consola rechazada
		printf("Ansisop rechazado\n");
		send(consola,"0000",4,0);}
	else{
			send(consola,"0001",4,0);
			pcbNuevo = crearPCB(string_substring_from(codigo,4));
			pcbNuevo->id=consola;							//Se le asigna al proceso como ID el numero de consola que lo envía.
	if(aceptado==1){
			printf("Código enviado a la UMC\nNuevo PCB en cola de READY!\n");
			queue_push(colaListos, pcbNuevo);
			sem_post(&sem_Listos);
	}else{
	printf("Código enviado a la UMC\nNuevo PCB en cola de NEW!\n");
			queue_push(colaNuevos, pcbNuevo);
			sem_post(&sem_Nuevos);
		}
	}
	//char* pcbSerializado=serializarMensajeCPU(pcbNuevo,2,5);
	//send(cpu,pcbSerializado,string_length(pcbSerializado),0);
	free(codigo);
	//list_iterate(pcbNuevo->indiceCodigo, (void*) mostrar);		//Ver inicio y offset de cada sentencia
}


PCB* crearPCB(char* codigo) {
	PCB* pcb=malloc(sizeof(PCB));
	//printf("***CODIGO:%s\n", codigo);
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
				printf("Se desconecto la consola en %d, ya termino su programa, eliminada\n",componente);
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
			printf("[HILO NUEVOS]: el proceso %d paso de Nuevo a Listo\n",pcbNuevo->id);
		 	 sem_post(&sem_Listos);
		 }
	}
}

void atender_Ejecuciones(){
	 printf("[HILO EJECUCIONES]: Se creo el hilo para ejecutar programas, esperando..\n");
	 char* mensajeCPU;
	 while(1){
		 sem_wait(&sem_Listos);
		 printf("[HILO EJECUCIONES]: se activo el semaforo listo y lo frene, voy a ver los cpus disponibles\n"); //prueba
		 PCB* pcbListo;
		 pcbListo = queue_pop(colaListos);
		 if(ese_PCB_hay_que_eliminarlo(pcbListo->id)){
			 printf("La consola del proceso %d no existe mas, se lo eliminara\n",pcbListo->id);
		 	 finalizarProgramaUMC(pcbListo->id);
		 	 sem_post(&sem_Nuevos);
		 }else{
		 int paso=1;
		 	 while(paso){
		 		 if(!list_is_empty(cpusDisponibles)){
					int cpu = (int)list_remove(cpusDisponibles, 0); //saco el socket de ese cpu disponible
					mensajeCPU = serializarMensajeCPU(pcbListo, datosNucleo->quantum, datosNucleo->quantum_sleep);
				 	send(cpu,mensajeCPU,string_length(mensajeCPU),0);
					printf("[HILO EJECUCIONES]: el proceso %d paso de Listo a Execute\n",pcbListo->id);
					paso=0;
				}
		 	 }
		 }
		 free(pcbListo);
		 free(mensajeCPU);
	 }
 }

 void atender_Bloq_ES(int posicion){
	 printf("[HILO DE E/S nro %d]: se creo el hilo %d de E/S\n",posicion,posicion);
	 int miSLEEP = dispositivosSleeps[posicion];
	 while(1){
	 	 sem_wait(&semaforosES[posicion]);
	 	 pcbParaES* pcbBloqueando = queue_pop(colasES[posicion]);
	 	 printf("[HILO DE E/S nro %d]: saque el pcb nro %d y va a esperar %d ut\n", posicion, pcbBloqueando->pcb->id,pcbBloqueando->ut);
	 	 usleep(miSLEEP*pcbBloqueando->ut);
		 queue_push(colaListos, pcbBloqueando->pcb);
		 printf("[HILO DE E/S nro %d]: el proceso %d paso de Bloqueado a Listo\n",posicion,pcbBloqueando->pcb->id);
		 sem_post(&sem_Listos);
		 free(pcbBloqueando);
	 }
 }

 void atender_Bloq_SEM(int posicion){
	 printf("[HILO DE SEMAFORO nro %d]: se creo el hilo %d de Semaforos de variables globales\n",posicion,posicion);
	 while(1){
		 sem_wait(&semaforosGlobales[posicion]);//si se activa, desbloquea a todos
		 	 while(!queue_is_empty(colasSEM[posicion])){
		 		 PCB* pcbBloqueando = queue_pop(colasSEM[posicion]);
		 		 queue_push(colaListos, pcbBloqueando);
		 		 printf("[HILO DE SEMAFORO nro %d]: el proceso %d paso de Bloqueado a Listo\n",posicion, pcbBloqueando->id);
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
	 	 printf("[HILO Terminados]: el proceso %d Termino su ejecucion\n",pcbTerminado->id);
	 	 sem_post(&sem_Nuevos);
	 	 free(pcbTerminado);
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
		 	sem_post(&sem_Nuevos);
		}
		sacar_socket_de_lista(cpus,cpu);
		printf("Se desconecto o envio algo mal el CPU en %d, eliminado\n",cpu);
		break;
	case QUANTUM_OK:
		//termino bien el quantum, no necesita nada
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu,tamanio);
		pcbDesSerializado = fromStringPCB(texto);
		sigueCPU = recibirProtocolo(cpu);
 		printf("el proceso %d paso de Execute a Listo\n",pcbDesSerializado->id);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void *)cpu);
 		}
		queue_push(colaListos, pcbDesSerializado);
		sem_post(&sem_Listos);
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
		printf("el proceso %d paso de Execute a Terminado\n",pcbDesSerializado->id);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void *)cpu);
 		}
		queue_push(colaTerminados, pcbDesSerializado);
		sem_post(&sem_Terminado);
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
		printf("el cpu mando mal la operacion privilegiada, todo mal\n");
		//error? o no deberia entrar aca
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
		if(contadorSemaforo[posicion]){					//si es mas de 0, semaforo libre, solo resta
			send(cpu, "ok", 2, 0);
			contadorSemaforo[posicion]--;
		}else{											//si es 0, semaforo bloqueado, se tiene que bloquear el pcb
			send(cpu, "no", 2, 0);						//=> Pido el pcb
			tamanio = recibirProtocolo(cpu); 			//tamaño del pcb
			texto = recibirMensaje(cpu,tamanio);		//PCB
			pcbDesSerializado = fromStringPCB(texto);
			sigueCPU = recibirProtocolo(cpu);
			queue_push(colasSEM[posicion], pcbDesSerializado); //mando el pcb a bloqueado
			printf("[EXECUTE] el proceso %d paso de Execute a Bloqueado\n",pcbDesSerializado->id);
	 		if(sigueCPU){
				list_add(cpusDisponibles, (void *)cpu);
	 		}
	 		free(texto);
		}
		free(identificador);
		break;
	case SIGNAL:
		//signal a un semaforo, post
		//recibo el identificador del semaforo
		send(cpu,"0001",4,0);
		posicion = (int)dictionary_get(semaforos,identificador);
		if(!contadorSemaforo[posicion]){
			sem_post(&semaforosGlobales[posicion]); //si esta en 0, activo el hilo para que los desbloquee
		}
		contadorSemaforo[posicion]++;
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

		pcbParaES*pcbParaBloquear=malloc(sizeof(pcbParaES)+sizeof(PCB));		//todo revisar, pero creo que ahora guarda bien
		pcbParaBloquear->pcb = pcbDesSerializado;
		pcbParaBloquear->ut = unidadestiempo;
		queue_push(colasES[posicion], pcbParaBloquear);
		printf("[EXECUTE] el proceso %d paso de Execute a Bloqueado\n",pcbDesSerializado->id);
		sem_post(&semaforosES[posicion]);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void *)cpu);
 		}
 		free(ut);
 		free(identificador);
 		free(texto);
 		//todo free a pcb desserializado? pcbParaBloquear tiene una copia?
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
		printf("Mensaje: %s\n",mensaje);
		free(quantum_char);
		free(quantum_sleep_char);
		free(pcb_char);

		return mensaje;
}
void finalizarProgramaUMC(int id){
	 char* mensaje = string_new();
	 string_append(&mensaje, "4");
	 string_append(&mensaje, header(id));
	 send(conexionUMC, mensaje, string_length(mensaje), 0);
	 free(mensaje);
}
void finalizarProgramaConsola(int consola, int codigo){
	 //codigo: el ansisop termino 2=ok / 3=mal
	 char* cod = header(codigo);
	 send(consola, cod, 4, 0);
}
void enviarTextoConsola(int consola, char* texto){
	 char* mensaje = string_new();
	 string_append(&mensaje, header(1));
	 agregarHeader(&texto);
	 string_append(&mensaje, texto);
	 string_append(&mensaje,"\0");
	 send(consola, mensaje, string_length(mensaje), 0);
	 free(mensaje);
}

void enviarPCBaCPU(int cpu, char* pcbSerializado){
	char* mensaje = string_new();
	string_append(&mensaje, "1");
	agregarHeader(&pcbSerializado);
	string_append(&mensaje,pcbSerializado);
	string_append(&mensaje,"\0");
	send(cpu, mensaje, string_length(mensaje), 0);
}

void Modificacion_quantum(){
	char buffer[BUF_LEN];
	//todo verificarlo en la terminal, sino lo mando al select
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
		printf("lei alguna modificacion: %d\n", length);//los prints para testear, en mi terminal no los veo.. en eclipse si

		do{
		archivoConfiguracion = config_create("../ConfigNucleo");
		}while(archivoConfiguracion == NULL);
		(datosNucleo)->quantum = config_get_int_value(archivoConfiguracion, "QUANTUM");
		(datosNucleo)->quantum_sleep = config_get_int_value(archivoConfiguracion,"QUANTUM_SLEEP");
		printf("el q ahora es: %d\ny el q sleep es: %d\n",(datosNucleo)->quantum, (datosNucleo)->quantum_sleep);

		config_destroy(archivoConfiguracion);
	}
}
