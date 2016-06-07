/*
 * nucleo.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */

#include <commons/config.h>
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


#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra) 	//MACRO

typedef struct {
	int puerto_nucleo;
	int puerto_umc;
	char* ip_umc;
	char* ip;
	int quantum;
	int quantum_sleep;
	char** sem_ids;
	char** sem_init;		//TRANSFORMAR CON (atoi) - gracias (:
	char** io_ids;
	char** io_sleep;		//LO MISMO
	char** shared_vars;
	int tamStack;
} datosConfiguracion;

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
t_queue *colaListos,*colaTerminados, *colaCPUs;
sem_t sem_Listos,sem_Terminado;

int autentificarUMC(int);
int leerConfiguracion(char*, datosConfiguracion**);
t_dictionary* crearDiccionarioGlobales(char** keys);
t_dictionary* crearDiccionarioSEMyES(char** keys, char** init, int esIO);
int comprobarCliente(int);
PCB* crearPCB(char*);
int calcularPaginas(char*);
void mostrar(int*);
void enviarAnsisopAUMC(int, char*,int);
void maximoDescriptor(int* maximo, t_list* lista, fd_set *descriptores);
void atender_Ejecuciones();
void atender_Bloq_ES(int posicion);
void atender_Bloq_SEM(int posicion);
void atender_Terminados();
void atenderOperacion(int op,int cpu);
void procesar_operacion_privilegiada(int operacion, int cpu);
int ese_PCB_hay_que_eliminarlo(int consola);
int ese_cpu_tenia_pcb_ejecutando(int cpu);
int revisarActividad(t_list*, fd_set*);
char* serializarMensajeCPU(PCB* pcbListo, int quantum, int quantum_sleep);
PCB* desSerializarMensajeCPU(char* char_pcb);
void enviarPCBaCPU(int, char*);


datosConfiguracion* datosNucleo;
t_dictionary *globales,*semaforos,*dispositivosES;
int tamPagina=0,*dispositivosSleeps, *globalesValores;
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
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);//todo agregar el destroy

	//--------------------------------CONFIGURACION-----------------------------

	datosNucleo=malloc(sizeof(datosConfiguracion));
	if (!(leerConfiguracion("ConfigNucleo", &datosNucleo) || leerConfiguracion("../ConfigNucleo", &datosNucleo))){
			printf("Error archivo de configuracion\n FIN.");return 1;}
//-------------------------------------------DICCIONARIOS---------------------------------------------------------------
	globales = crearDiccionarioGlobales(datosNucleo->shared_vars);
	semaforos = crearDiccionarioSEMyES(datosNucleo->sem_ids,datosNucleo->sem_init, 0);
	dispositivosES = crearDiccionarioSEMyES(datosNucleo->io_ids,datosNucleo->io_sleep,1);

	//---------------------------------PLANIFICACION PCB-----------------------------------

	sem_init(&sem_Listos, 0, 0);
	sem_init(&sem_Terminado, 0, 0);

	listaEjecuciones=list_create();
	colaListos=queue_create();
	colaTerminados=queue_create();
	colaCPUs=queue_create();

	pthread_create(&thread, &attr, (void*)atender_Ejecuciones, NULL);

	//-----------------------------------pcb para probar bloqueo de E/S
	PCB*pcbprueba=malloc(sizeof(PCB));
	pcbprueba->id=5;
	pcbParaES*pcbParaBloquear=malloc(sizeof(pcbParaES));
	pcbParaBloquear->pcb = pcbprueba;
	pcbParaBloquear->ut = 6;
	queue_push(colasES[1], pcbParaBloquear);
	sem_post(&semaforosES[1]);

	//------------------------------------CONEXION UMC--------------------------------
	int nucleo_servidor = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in direccionNucleo = crearDireccion(datosNucleo->puerto_nucleo, datosNucleo->ip);
	printf("Nucleo creado, conectando con la UMC...\n");
	int conexionUMC = conectar(datosNucleo->puerto_umc, datosNucleo->ip_umc);
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
						nuevo_cliente = accept(nucleo_servidor,
								(void *) &direccionCliente, (void *) &sin_size);
						if (nuevo_cliente == -1) {
							perror("Fallo el accept");
						}
						printf("Nueva conexion\n");
						int tamPagParaCpu = htonl(tamPagina);
						switch (comprobarCliente(nuevo_cliente)) {

						case 0:										//ERROR!!
							perror("Falló el handshake\n");
							close(nuevo_cliente);
							break;

						case 1:											//CPU
							send(nuevo_cliente, &tamPagParaCpu, 4, 0);
							printf("Acepté un nuevo cpu\n");
							queue_push(colaCPUs, &nuevo_cliente);
							list_add(cpus, (void *) nuevo_cliente);
							break;

						case 2:						//CONSOLA, RECIBO EL CODIGO
							send(nuevo_cliente, "1", 1, 0);
							list_add(consolas, (void *) nuevo_cliente);
							printf("Acepté una nueva consola\n");
							int tamanio = recibirProtocolo(nuevo_cliente);
							if (tamanio > 0) {
								char* codigo = (char*) recibirMensaje(
										nuevo_cliente, tamanio);//printf("--Codigo:%s--\n",codigo);
								enviarAnsisopAUMC(conexionUMC, codigo,
										nuevo_cliente);
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
			(*datos)->sem_ids = config_get_array_value(archivoConfiguracion,"SEM_ID");
			(*datos)->sem_init = config_get_array_value(archivoConfiguracion,"SEM_INIT");
			(*datos)->io_ids = config_get_array_value(archivoConfiguracion,"IO_ID");
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
	if (string_equals_ignore_case("soy_un_cpu", bufferHandshake)) {
		free(bufferHandshake);
		return 1;
	} else if (string_equals_ignore_case("soy_una_consola", bufferHandshake)) {
		free(bufferHandshake);
		return 2;
	}
	free(bufferHandshake);
	return 0;
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
	if(!aceptado){													//consola rechazada
		printf("Ansisop rechazado\n");
		send(consola,"0",1,0);}
	else{
			send(consola,"1",1,0);
			PCB* pcbNuevo = crearPCB(codigo);
			pcbNuevo->id=consola;							//Se le asigna al proceso como ID el numero de consola que lo envía.
	if(aceptado==1){
			printf("Código enviado a la UMC\nNuevo PCB en cola de READY!\n");
			queue_push(colaListos, pcbNuevo);
			sem_post(&sem_Listos);}
	else{
	printf("Código enviado a la UMC\nNuevo PCB en cola de NEW!\n");
		/*	queue_push(colaNuevos, pcbNuevo); todo
			sem_post(&sem_Nuevos);*/
		}
	}
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
//	pcbProceso->PID=ultimoPID++;
//	pcbProceso->PC = metadata->instruccion_inicio;								//Pos de la primer instruccion
//	pcbProceso->indiceCodigo=metadata->instrucciones_serializado;
//	pcbProceso->cantEtiquetas=metadata->etiquetas_size;
//	pcbProceso->etiquetas=metadata->etiquetas;
//	pcbProceso->cantInstrucciones=metadata->instrucciones_size;
//	pcbProceso->indiceStack=string_length(codigo)+1;
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
			 //todo avisar umc de eliminar este proceso
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
		 //free(pcbBloqueando)?
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
	 while(1){
		 sem_wait(&sem_Terminado);
	 	 PCB* pcbTerminado = malloc(sizeof(PCB));
	 	 pcbTerminado = queue_pop(colaTerminados);
	 	 //todo avisar umc y consola que termino el programa
	 	 free(pcbTerminado);
	 }
 }


void atenderOperacion(int op,int cpu){
		int tamanio, consola, operacion,pidMalo;
		char* texto;
		PCB*pcbDesSerializado;
	switch (op){
	case 0:
		//el cpu se desconecto y termino mal el q? o hubo un error        (en pruebas, cuando cerraba un cpu devolvia 0, en vez de -1)
		pidMalo = ese_cpu_tenia_pcb_ejecutando(cpu);
		if(pidMalo){
			//todo avisar umc y consola, de borrar ese pid y de que hubo error en ejecucion
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
		consola = recibirProtocolo(cpu); //todo el cpu me tiene que mandar a quien, el pcb->id
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu, tamanio);   //texto o valor
		//todo verificar que esa consola exista (que este en la lista de consolas)
		//send(consola, agregarHeader(texto),(tamanio+4),0);
		break;
	}

}
void procesar_operacion_privilegiada(int operacion, int cpu){
		int tamanioNombre, posicion, unidadestiempo,valor,tamanio;
		char *identificador,*texto;
		PCB*pcbDesSerializado;
	switch (operacion){
	case 0:
		printf("el cpu mando mal la operacion privilegiada, todo mal\n");
		//todo error? o no deberia entrar aca
		break;
	case 1:
		//obtener valor de variable compartida
		//recibo nombre de variable compartida, devuelvo su valor
		tamanioNombre = recibirProtocolo(cpu);
		identificador = recibirMensaje(cpu,tamanioNombre);
		posicion = (int)dictionary_get(globales,identificador);
		valor = globalesValores[posicion];
		send(cpu, header(valor), 4, 0);
		break;
	case 2:
		//grabar valor en variable compartida
		//recibo el nombre de una variable y un valor -> guardo
		tamanioNombre = recibirProtocolo(cpu);
		identificador = recibirMensaje(cpu,tamanioNombre);
		valor = recibirProtocolo(cpu);
		posicion = (int)dictionary_get(globales,identificador);
		globalesValores[posicion] = valor;
		break;
	case 3:
		//wait a un semaforo, si no puiede acceder, se bloquea
		//recibo el identificador del semaforo
		tamanioNombre = recibirProtocolo(cpu);
		identificador = recibirMensaje(cpu,tamanioNombre);
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
		tamanioNombre = recibirProtocolo(cpu);
		identificador = recibirMensaje(cpu,tamanioNombre);
		posicion = (int)dictionary_get(semaforos,identificador);
		sem_post(&semaforosGlobales[posicion]);
		break;
	case 5:
		//pedido E/S, va a bloqueado
		//recibo nombre de dispositivo, y unidades de tiempo a utilizar
		tamanioNombre = recibirProtocolo(cpu);
		identificador = recibirMensaje(cpu,tamanioNombre);
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
int ese_PCB_hay_que_eliminarlo(int consola){ //devuelve si esa consola esta en la lista de eliminadas
	int buscarIgual(int elemLista){
		if(consola==elemLista){
			return 1;
		}else{return 0;}
	}
	if(list_any_satisfy(listConsolasParaEliminarPCB,(void*)buscarIgual)){
		list_remove_by_condition(listConsolasParaEliminarPCB,(void*)buscarIgual);
		return 1;
	}return 0;
}
int ese_cpu_tenia_pcb_ejecutando(cpu){ //devuelve el pid si el cpu estaba ejecutando un pcb
	int buscarIgual(pidEjecutandose* elemLista){
		if(cpu==elemLista->cpu){
			return 1;
		}else{return 0;}
	}
	if(list_any_satisfy(listaEjecuciones, (void*)buscarIgual)){
		pidEjecutandose* pidencpu = list_find(listaEjecuciones, (void*)buscarIgual);
		list_remove_by_condition(listaEjecuciones, (void*)buscarIgual);
		return pidencpu->pid;
	}else{
		return 0;}
}

char* serializarMensajeCPU(PCB* pcbListo, int quantum, int quantum_sleep){
	char* mensaje;
	char* quantum_char = toStringInt(quantum);
	char* quantum_sleep_char = toStringInt(quantum_sleep);
	char* pcb_char = toStringPCB(*pcbListo);
	mensaje = malloc((strlen(pcb_char)+10)*sizeof(char));
	sprintf(mensaje,"%s%s%s",quantum_char,quantum_sleep_char,pcb_char);
	free(quantum_char);
	free(quantum_char);
	free(pcb_char);

	return mensaje;
}
PCB* desSerializarMensajeCPU(char* char_pcb){
	PCB * pcbDevuelto = (PCB*) malloc(sizeof(PCB));
	//pcbDevuelto = fromStringPCB(char_pcb);
	free(char_pcb);

	return pcbDevuelto;
}
void enviarPCBaCPU(int cpu, char* pcbSerializado){
	char* mensaje = string_new();
	string_append(&mensaje, "1");
	agregarHeader(&pcbSerializado);
	string_append(&mensaje,pcbSerializado);
	send(cpu, mensaje, string_length(mensaje), 0);
}
