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
#include "Funciones/Comunicacion.h"
#include "Funciones/json.h"


typedef struct{
	PCB* pcb;
	int ut;
}pcbParaES;

typedef struct{
	int pid;
	int cpu;
}pidEjecutandose;

t_list *cpus, *consolas, *listaEjecuciones, *listConsolasParaEliminarPCB;

//estructuras para planificacion
pthread_attr_t attr;
pthread_t thread;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_queue *colaNuevos, *colaListos,*colaTerminados, *colaCPUs;
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
int esa_consola_existe(int consola);
int ese_PCB_hay_que_eliminarlo(int consola);
int ese_cpu_tenia_pcb_ejecutando(int cpu);
int revisarActividad(t_list*, fd_set*);
char* serializarMensajeCPU(PCB* pcbListo, int quantum, int quantum_sleep);
PCB* desSerializarMensajeCPU(char* char_pcb);
void enviarPCBaCPU(int, char*);
void finalizarProgramaUMC(int id);
void finalizarProgramaConsola(int consola, int codigo);
void enviarTextoConsola(int consola, char* texto);


datosConfiguracion* datosNucleo;
t_dictionary *globales,*semaforos,*dispositivosES;
int tamPagina=0,*dispositivosSleeps, *globalesValores, conexionUMC;
sem_t *semaforosES,*semaforosGlobales;
t_queue **colasES,**colasSEM;
int cpu;


int main(int argc, char* argv[]) {
	fd_set descriptores;
	cpus = list_create();
	consolas = list_create();
	listConsolasParaEliminarPCB= list_create();
	int max_desc, nuevo_cliente,sin_size = sizeof(struct sockaddr_in) ;
	struct sockaddr_in direccionCliente;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);//todo agregar el destroy

	//--------------------------------CONFIGURACION-----------------------------

	datosNucleo=malloc(sizeof(datosConfiguracion));
	if (!(leerConfiguracion("ConfigNucleo", &datosNucleo) || leerConfiguracion("../ConfigNucleo", &datosNucleo))){
			printf("Error archivo de configuracion\n FIN.");return 1;}
//-------------------------------------------DICCIONARIOS---------------------------------------------------------------
/*	globales = crearDiccionarioGlobales(datosNucleo->shared_vars);
	semaforos = crearDiccionarioSEMyES(datosNucleo->sem_ids,datosNucleo->sem_init, 0);
	dispositivosES = crearDiccionarioSEMyES(datosNucleo->io_ids,datosNucleo->io_sleep,1);

	//---------------------------------PLANIFICACION PCB-----------------------------------

	sem_init(&sem_Nuevos, 0, 0);
	sem_init(&sem_Listos, 0, 0);
	sem_init(&sem_Terminado, 0, 0);

	listaEjecuciones=list_create();
	colaNuevos=queue_create();
	colaListos=queue_create();
	colaTerminados=queue_create();
	colaCPUs=queue_create();

	pthread_create(&thread, &attr, (void*)atender_Ejecuciones, NULL);
*/

//	pthread_create(&thread, &attr, (void*)atender_Nuevos, NULL);
//	pthread_create(&thread, &attr, (void*)atender_Terminados, NULL);

	//-----------------------------------pcb para probar bloqueo de E/S
	/*PCB*pcbprueba=malloc(sizeof(PCB));
	pcbprueba->id=5;
	pcbParaES*pcbParaBloquear=malloc(sizeof(pcbParaES));
	pcbParaBloquear->pcb = pcbprueba;
	pcbParaBloquear->ut = 6;
	queue_push(colasES[1], pcbParaBloquear);
	sem_post(&semaforosES[1]);*/

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
		socketARevisar = revisarActividad(consolas, &descriptores);
		if (socketARevisar) {								//Reviso actividad en consolas
			printf("Se desconecto la consola en %d, eliminada\n",socketARevisar);
			list_add(listConsolasParaEliminarPCB,(void *) socketARevisar);
			close(socketARevisar);
		}
		else {
			socketARevisar = revisarActividad(cpus, &descriptores);
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
							cpu=nuevo_cliente;
							printf("Acepté un nuevo cpu\n");
//							queue_push(colaCPUs, &nuevo_cliente);
//							list_add(cpus, (void *) nuevo_cliente);
							break;

						case 2:						//CONSOLA, RECIBO EL CODIGO
							send(nuevo_cliente, "1", 1, 0);
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
		for(;i>=0;i--){
			sem_init(&semaforosGlobales[i], 0, atoi(init[i]) );		//vector de semaforos inicializados
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
	recv(conexionUMC, &aceptado, sizeof(int), 0);
	aceptado=ntohl(aceptado);
	printf("-------------------------ACEPTADO: %d\n",aceptado);
	PCB* pcbNuevo;
	if(!aceptado){													//consola rechazada
		printf("Ansisop rechazado\n");
		send(consola,"0",1,0);}
	else{
			send(consola,"1",1,0);
			pcbNuevo = crearPCB(string_substring_from(codigo,4));
			pcbNuevo->id=consola;							//Se le asigna al proceso como ID el numero de consola que lo envía.
	if(aceptado==1){
			printf("Código enviado a la UMC\nNuevo PCB en cola de READY!\n");
//			queue_push(colaListos, pcbNuevo);
//			sem_post(&sem_Listos);}
	}else{
	printf("Código enviado a la UMC\nNuevo PCB en cola de NEW!\n");
			queue_push(colaNuevos, pcbNuevo);
			sem_post(&sem_Nuevos);
		}
	}
	char* pcbSerializado=serializarMensajeCPU(pcbNuevo,2,5);
	send(cpu,pcbSerializado,string_length(pcbSerializado),0);
	free(codigo);
	//list_iterate(pcbNuevo->indiceCodigo, (void*) mostrar);		//Ver inicio y offset de cada sentencia
}


PCB* crearPCB(char* codigo) {
	PCB* pcb=malloc(sizeof(PCB));
	//printf("***CODIGO:%s\n", codigo);
	t_metadata_program *metadata = metadata_desde_literal(codigo);
	pcb->indices = *metadata;
	pcb->paginas_codigo = calcularPaginas(codigo);
	pcb->pc = 0; //metadata->instruccion_inicio;
	pcb->stack = list_create();
	return pcb;
}

int calcularPaginas(char* codigo){
	int totalPaginas=string_length(codigo)/tamPagina;
	if (string_length(codigo)%tamPagina) totalPaginas++;
	return totalPaginas;
}

 void mostrar(int* sentencia){
 	printf("Inicio:%d | Offset:%d\n",sentencia[0],sentencia[1]);
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

int revisarActividad(t_list* lista, fd_set *descriptores) {
	int i;
	for (i = 0; i < list_size(lista); i++) {
		int componente = (int) list_get(lista, i);
		if (FD_ISSET(componente, descriptores)) {
			int protocolo = recibirProtocolo(componente);
			if (protocolo == -1) {
				list_remove(lista, i);  //todo un cpu puede entrar aca? habria que verificar igual que abajo entonces (atenderOperacion)
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
		 PCB* pcbNuevo = malloc(sizeof(PCB));
		 pcbNuevo = queue_pop(colaNuevos);
		 queue_push(colaListos,pcbNuevo); //entonces lo mando a Listos
		 sem_post(&sem_Listos);
	}
}

void atender_Ejecuciones(){
	 printf("[HILO EJECUCIONES]: Se creo el hilo para ejecutar programas, esperando..\n");
	 char* mensajeCPU;
	 while(1){
		 sem_wait(&sem_Listos);
		 printf("[HILO EJECUCIONES]: se activo el semaforo listo y lo frene, voy a ver los cpus disponibles\n"); //prueba
		 PCB* pcbListo = malloc(sizeof(PCB));
		 pcbListo = queue_pop(colaListos);
		 pidEjecutandose*pidEnCpu = malloc(sizeof(pidEjecutandose));
		 if(ese_PCB_hay_que_eliminarlo(pcbListo->id)){
			 printf("La consola del proceso %d no existe mas, se lo eliminara\n",pcbListo->id);
		 	 finalizarProgramaUMC(pcbListo->id);
		 }else{
		 int paso=1;
		 	 while(paso){
		 		 if(!queue_is_empty(colaCPUs)){
					int cpu = (int)queue_pop(colaCPUs); //saco el socket de ese cpu disponible
					mensajeCPU = serializarMensajeCPU(pcbListo, datosNucleo->quantum, datosNucleo->quantum_sleep);
				 	 enviarPCBaCPU(cpu, mensajeCPU);
						pidEnCpu->pid = pcbListo->id;
						pidEnCpu->cpu = cpu;
						list_add(listaEjecuciones,pidEnCpu); //guardo que pid fue a que cpu
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
		 printf("[HILO DE E/S nro %d]: el pcb %d paso de Bloqueado a Listo\n",posicion,pcbBloqueando->pcb->id);
		 sem_post(&sem_Listos);
		 free(pcbBloqueando);
	 }
 }

 void atender_Bloq_SEM(int posicion){
	 printf("[HILO DE SEMAFORO nro %d]: se creo el hilo %d de Semaforos de variables globales\n",posicion,posicion);
	 while(1){
		 sem_wait(&semaforosGlobales[posicion]);
		 if(!queue_is_empty(colasSEM[posicion])){
			 PCB* pcbBloqueando = queue_pop(colasSEM[posicion]);
		 	 queue_push(colaListos, pcbBloqueando);
		 	 printf("[HILO DE SEMAFORO nro %d]: el proceso %d paso de Bloqueado a Listo\n",posicion, pcbBloqueando->id);
		 	 sem_post(&sem_Listos);
		 }else{
			 printf("[HILO DE SEMAFORO nro %d]: no hay ningun pcb para desbloquear\n",posicion);
		 }
	 }

 }
 void atender_Terminados(){
	 int cod=2;
	 while(1){
		 sem_wait(&sem_Terminado);
	 	 PCB* pcbTerminado = malloc(sizeof(PCB));
	 	 pcbTerminado = queue_pop(colaTerminados);
	 	 finalizarProgramaUMC(pcbTerminado->id);
	 	 finalizarProgramaConsola(pcbTerminado->id, cod);
	 	 sem_post(&sem_Nuevos);
	 	 free(pcbTerminado);
	 }
 }


void atenderOperacion(int op,int cpu){
		int tamanio, consola, operacion, pidMalo;
		char* texto;
		PCB *pcbDesSerializado;
	switch (op){
	case 0:
		//el cpu se desconecto y termino mal el q? o hubo un error        (en pruebas, cuando cerraba un cpu devolvia 0, en vez de -1)
		//todo juntar con case 5
		pidMalo = ese_cpu_tenia_pcb_ejecutando(cpu);
		if(pidMalo){
			operacion = 3;
		 	finalizarProgramaConsola(pidMalo, operacion);
		 	finalizarProgramaUMC(pidMalo);
		 	sem_post(&sem_Nuevos);
		}
		list_remove(cpus, cpu);
		printf("Se desconecto o envio algo mal el CPU en %d, eliminado\n",cpu);
		break;
	case 1:
		//termino bien el quantum, no necesita nada
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu,tamanio);
		pcbDesSerializado = desSerializarMensajeCPU(texto);
		printf("el cpu termino su quantum, no necesita nada\n");
 		printf("el proceso %d paso de Execute a Listo\n",pcbDesSerializado->id);
		queue_push(colaCPUs, &cpu);
		queue_push(colaListos, pcbDesSerializado);
		sem_post(&sem_Listos);
		break;
	case 2:
		//me pide una operacion privilegiada
		operacion = recibirProtocolo(cpu);
		procesar_operacion_privilegiada(operacion, cpu);
		break;
	case 3:
		//termino el ansisop, va a listos
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu,tamanio);
		pcbDesSerializado = desSerializarMensajeCPU(texto);
		printf("el proceso %d paso de Execute a Terminado\n",pcbDesSerializado->id);
		queue_push(colaCPUs, &cpu);
		queue_push(colaTerminados, pcbDesSerializado);
		sem_post(&sem_Terminado);
		break;
	case 4:
		//imprimir o imprimirTexto
		consola = recibirProtocolo(cpu);
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu, tamanio);   //texto o valor
		if(esa_consola_existe(consola)){
			enviarTextoConsola(consola, texto);
		}
		free(texto);
		break;
	case 5:
		//error en el ansisop (memoria o sintaxis)
		consola = recibirProtocolo(cpu);
		operacion = 3;
	 	finalizarProgramaConsola(consola, operacion);
		break;
	}
}

void procesar_operacion_privilegiada(int operacion, int cpu){
		int tamanioNombre, posicion, unidadestiempo,valor,tamanio;
		char *identificador,*texto;
		PCB*pcbDesSerializado;
		if (operacion){
			tamanioNombre = recibirProtocolo(cpu);
			identificador = recibirMensaje(cpu,tamanioNombre);
		}
	switch (operacion){
	case 0:
		printf("el cpu mando mal la operacion privilegiada, todo mal\n");
		//todo error? o no deberia entrar aca
		break;
	case 1:
		//obtener valor de variable compartida
		//recibo nombre de variable compartida, devuelvo su valor
		posicion = (int)dictionary_get(globales,identificador);
		valor = globalesValores[posicion];
		valor = htonl(valor);
		send(cpu, &valor, 4, 0);
		break;
	case 2:
		//grabar valor en variable compartida
		//recibo el nombre de una variable y un valor -> guardo y devuelvo valor
		valor = ntohl(recibirMensaje(cpu,4));
		posicion = (int)dictionary_get(globales,identificador);
		globalesValores[posicion] = valor;
		valor = htonl(valor);
		send(cpu, &valor, 4, 0);
		break;
	case 3:
		//wait a un semaforo, si no puiede acceder, se bloquea
		//recibo el identificador del semaforo
		posicion = (int)dictionary_get(semaforos,identificador);
		sem_getvalue(&semaforosGlobales[posicion],&valor);
		if(valor){					//si es mas de 0, semaforo libre
			send(cpu, "ok", 2, 0);
			sem_wait(&semaforosGlobales[posicion]);
		}else{						//si es 0, semaforo bloqueado
			send(cpu, "no", 2, 0);
			tamanio = recibirProtocolo(cpu); //entonces pido el pcb
			texto = recibirMensaje(cpu,tamanio);
			pcbDesSerializado = desSerializarMensajeCPU(texto);
			queue_push(colasSEM[posicion], pcbDesSerializado); //mando el pcb a bloqueado
			sem_post(&semaforosGlobales[posicion]);
			queue_push(colaCPUs, &cpu); //libero al cpu
		}
		break;
	case 4:
		//signal a un semaforo, post
		//recibo el identificador del semaforo
		posicion = (int)dictionary_get(semaforos,identificador);
		sem_post(&semaforosGlobales[posicion]);
		break;
	case 5:
		//pedido E/S, va a bloqueado
		//recibo nombre de dispositivo, y unidades de tiempo a utilizar
		unidadestiempo = recibirProtocolo(cpu);
		posicion = (int)dictionary_get(dispositivosES,identificador);

		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu,tamanio);
		pcbDesSerializado = desSerializarMensajeCPU(texto);

		pcbParaES*pcbParaBloquear=malloc(sizeof(pcbParaES));
		pcbParaBloquear->pcb = pcbDesSerializado;
		pcbParaBloquear->ut = unidadestiempo;
		queue_push(colasES[posicion], pcbParaBloquear);
		sem_post(&semaforosES[posicion]);
		queue_push(colaCPUs, &cpu);
		break;
	}
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

int ese_cpu_tenia_pcb_ejecutando(cpu){ //devuelve el pid si el cpu estaba ejecutando un pcb
	int buscarIgual(pidEjecutandose* elemLista){
		return(cpu==elemLista->cpu);}

	if(list_any_satisfy(listaEjecuciones, (void*)buscarIgual)){
		pidEjecutandose* pidencpu = list_find(listaEjecuciones, (void*)buscarIgual);
		list_remove_by_condition(listaEjecuciones, (void*)buscarIgual);
		return pidencpu->pid;}
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
	 int uno = 1;
	 string_append(&mensaje, header(uno));
	 agregarHeader(&texto);
	 string_append(&mensaje, texto);
	 send(consola, mensaje, string_length(mensaje), 0);
	 free(mensaje);
}

PCB* desSerializarMensajeCPU(char* char_pcb){
	PCB* pcbDevuelto = (PCB*) malloc(sizeof(PCB));
	pcbDevuelto = fromStringPCB(char_pcb);
	free(char_pcb);

	return pcbDevuelto;
}

void enviarPCBaCPU(int cpu, char* pcbSerializado){
	char* mensaje = string_new();
	string_append(&mensaje, "1");
	agregarHeader(&pcbSerializado);
	string_append(&mensaje,pcbSerializado);
	string_append(&mensaje,"\0");
	send(cpu, mensaje, string_length(mensaje), 0);
}
