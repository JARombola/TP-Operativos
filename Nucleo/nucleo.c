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

typedef struct {
	int PID;
	int PC;
	int SP;
	t_intructions* indiceCodigo;
	char* etiquetas;
	t_size cantEtiquetas;
	int consola;
	int cantInstrucciones;
	int indiceStack;
} pcb;

typedef struct{
	pcb* pcb;
	int ut;
}pcbParaES;

t_list *cpus, *consolas;

//estructuras para planificacion
pthread_attr_t attr;
pthread_t thread;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_queue *colaListos, *colaExec, *colaBloq, *colaTerminados, *colaCPUs;
sem_t sem_Listos,sem_Terminado;

int autentificarUMC(int);
int leerConfiguracion(char*, datosConfiguracion**);
t_dictionary* crearDiccionarioGlobales(char** keys);
t_dictionary* crearDiccionarioSEMyES(char** keys, char** init, int esIO);
int comprobarCliente(int);
pcb* crearPCB(char*);
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
int revisarActividad(t_list*, fd_set*);


int ultimoPID=0,tamPagina=0;
datosConfiguracion* datosNucleo;
t_dictionary *globales,*semaforos,*dispositivosES;
int *dispositivosSleeps, *globalesValores;
sem_t *semaforosES,*semaforosGlobales;
t_queue **colasES,**colasSEM;


int main(int argc, char* argv[]) {
	fd_set descriptores;
	cpus = list_create();
	consolas = list_create();
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
	//printf("%d\n",(int)dictionary_get(globales,"!UnaVar"));	//EJEMPLO DE BUSQUEDA

	//---------------------------------PLANIFICACION PCB-----------------------------------

	sem_init(&sem_Listos, 0, 0);
	sem_init(&sem_Terminado, 0, 0);

	colaListos=queue_create();
	colaTerminados=queue_create();
	colaCPUs=queue_create();

	pthread_create(&thread, &attr, (void*)atender_Ejecuciones, NULL);

	//-----------------------------------pcb para probar bloqueo de E/S
	pcb*pcbprueba=malloc(sizeof(pcb));
	pcbprueba->PID=5;
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

	//****_____________________________________________________________________________________________________________*
	//****--------------------------------------------ACA Arranca la Magia---------------------------------------------*
	//****_____________________________________________________________________________________________________________*
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
			printf("Se desconecto una consola, eliminada\n");
			close(socketARevisar);
			//todo eliminar pcb
		}
		else {
			socketARevisar = revisarActividad(cpus, &descriptores);
			if (socketARevisar) {								//Reviso actividad en cpus
				printf("Se desconecto una CPU, eliminada\n");
				close(socketARevisar);
				//todo eliminar pcb si no termino bien el quantum

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

						case 0:											//ERROR!!
							perror("Falló el handshake\n");
							close(nuevo_cliente);
							break;

						case 1:												//CPU
							send(nuevo_cliente, &tamPagParaCpu, 4, 0);
							printf("Acepté un nuevo cpu\n");
							queue_push(colaCPUs,&nuevo_cliente);
							break;

						case 2:							//CONSOLA, RECIBO EL CODIGO
							send(nuevo_cliente, "1", 1, 0);
							list_add(consolas, (void *) nuevo_cliente);
							printf("Acepté una nueva consola\n");
							int tamanio = recibirProtocolo(nuevo_cliente);
							if (tamanio > 0) {
								char* codigo = (char*) recibirMensaje(nuevo_cliente,
										tamanio);//printf("--Codigo:%s--\n",codigo);
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
	char* mensajeInicial = string_new();
	pcb* pcbNuevo = crearPCB(codigo);
	pcbNuevo->consola=consola;
	pcbNuevo->SP = 2; 											//todo numero para probar
	printf("\n");
	string_append(&mensajeInicial, "1");
	string_append(&mensajeInicial, header(pcbNuevo->PID));
	string_append(&mensajeInicial, header((paginasNecesarias)));
	string_append(&mensajeInicial, "\0");
	//printf("%s, Long:%d\n", mensajeInicial, string_length(mensajeInicial));
	send(conexionUMC, mensajeInicial, string_length(mensajeInicial), 0);
	free(mensajeInicial);
	char* resp = malloc(2);
	recv(conexionUMC, resp, 1, 0);
	resp[1] = '\0';
	if (!strcmp(resp, "1")) {
		agregarHeader(&codigo);
		send(conexionUMC, codigo, string_length(codigo), 0);
		free(codigo);
		printf("Código enviado a la UMC\nNuevo PCB en cola de listos!\n");
		queue_push(colaListos, pcbNuevo);
		sem_post(&sem_Listos);
	} else {
		printf("Ansisop rechazado\n");
		free(pcbNuevo);
		ultimoPID--;
	}
	free(resp);
	//list_iterate(pcbNuevo->indiceCodigo, (void*) mostrar);		//Ver inicio y offset de cada sentencia
}


pcb* crearPCB(char* codigo) {
	pcb* pcbProceso=(pcb*)malloc(sizeof(pcb));
	//printf("***CODIGO:%s\n", codigo);
	t_metadata_program *metadata = metadata_desde_literal(codigo);
	pcbProceso->PID=ultimoPID++;
	pcbProceso->PC = metadata->instruccion_inicio;								//Pos de la primer instruccion
	pcbProceso->indiceCodigo=metadata->instrucciones_serializado;
	pcbProceso->cantEtiquetas=metadata->etiquetas_size;
	pcbProceso->etiquetas=metadata->etiquetas;
	pcbProceso->cantInstrucciones=metadata->instrucciones_size;
	pcbProceso->indiceStack=string_length(codigo)+1;
	return pcbProceso;
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
			if (protocolo <= 0) {							//Se desconecto o algo, saque (== -1) porque el cpu me mandaba 0 al desconectarar
				list_remove(lista, i);
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
	 printf("Se creo el hilo para ejecutar programas, esperando..\n");
	 while(1){
		 sem_wait(&sem_Listos);
		 printf("se activo el semaforo listo y lo frene, voy a ver los cpus disponibles\n");
		 pcb* pcbListo = malloc(sizeof(pcb));
		 pcbListo = queue_pop(colaListos);
		 int paso=1;
		 while(paso){
			if(!queue_is_empty(colaCPUs)){
				printf("el proceso %d paso de Listo a Execute\n",pcbListo->PID);
				int cpu = queue_pop(colaCPUs); //saco el socket de ese cpu disponible
			 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 	 //todo serializar pcbListo (agregar quantum y quantum_sleep)
				//send(cpu, pcbSerializado, tamanioSerializado, 0);
				paso=0;
			}
		 }
	 }
	 //free(pcbListo);
 }
 void atender_Bloq_ES(int posicion){
	 printf("se creo el hilo %d de E/S\n",posicion);
	 int miSLEEP = dispositivosSleeps[posicion];
	 while(1){
	 	 sem_wait(&semaforosES[posicion]);
	 	 pcbParaES* pcbBloqueando = queue_pop(colasES[posicion]);
	 	 printf("saque el pcb nro %d y va a esperar %d ut\n", pcbBloqueando->pcb->PID,pcbBloqueando->ut);
	 	 usleep(miSLEEP*pcbBloqueando->ut);
	 	 sleep(5);
		 queue_push(colaListos, pcbBloqueando->pcb);
		 sem_post(&sem_Listos);
	 }
 }
 void atender_Bloq_SEM(int posicion){
	 printf("se creo el hilo %d de Semaforos de variables globales\n",posicion);
	 while(1){
		 sem_wait(&semaforosGlobales[posicion]);
		 if(!queue_is_empty(colaCPUs)){
			 pcb* pcbBloqueando = queue_pop(colasSEM[posicion]);
		 	 queue_push(colaListos, pcbBloqueando);
		 	 sem_post(&sem_Listos);
		 	 printf("el proceso %d paso de Bloqueado a Listo\n",pcbBloqueando->PID);
		 }else{
			 printf("no hay ningun pcb para desbloquear\n");
		 }
	 }

 }
 void atender_Terminados(){
	 while(1){
		 sem_wait(&sem_Terminado);
	 	 pcb* pcbTerminado = (pcb*) malloc(sizeof(pcb));
	 	 pcbTerminado = queue_pop(colaTerminados);
	 	 //todo avisar umc y consola que termino el programa
	 	 free(pcbTerminado);
	 }
 }


void atenderOperacion(int op,int cpu){
		int tamanio, consola, operacion;
		char* texto;
		pcb*pcbDesSerializado; //lo pongo aca para tenerlo de prueba
	switch (op){
	case 1:
		//termino bien el quantum, no necesita nada
		//pcb*pcb = desSerializar();
		printf("el cpu termino su quantum, no necesita nada\n");
 		printf("el proceso %d paso de Execute a Listo\n",pcbDesSerializado->PID);
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
		//pcb*pcb = desserializar();
		printf("el proceso %d paso de Execute a Terminado\n",pcbDesSerializado->PID);
		queue_push(colaCPUs, &cpu);
		queue_push(colaTerminados, pcbDesSerializado);
		sem_post(&sem_Terminado);

		break;
	case 4:
		//imprimir o imprimirTexto
		consola = recibirProtocolo(cpu);
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu, tamanio);   //texto o valor
		send(consola, texto,(tamanio+4),0); //agregar header a texto
		break;

	}

}
void procesar_operacion_privilegiada(int operacion, int cpu){
	int tamanioNombre, posicion, unidadestiempo,valor;
	char *identificador;
	pcb*pcbDesSerializado; //lo pongo aca para tenerlo de prueba

	switch (operacion){
	case 0:
		printf("el cpu mando mal la operacion privilegiada, todo mal\n");

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
		sem_getvalue(&semaforosGlobales[posicion],valor);
		if(valor){					//si es mas de 0, semaforo libre
			send(cpu, "ok", 2, 0);
			sem_wait(&semaforosGlobales[posicion]);
		}else{						//si es 0, semaforo bloqueado
			send(cpu, "no", 2, 0);
			//pcb*pcb = desserializar();
			queue_push(colasSEM[posicion], pcbDesSerializado);
			sem_post(&semaforosGlobales[posicion]);
			queue_push(colaCPUs, &cpu);
		}
		break;
	case 4:
		//signal a un semaforo, post
		//recibo el identificador del semaforo
		tamanioNombre = recibirProtocolo(cpu);
		identificador = recibirMensaje(cpu,tamanioNombre);
		posicion = (int)dictionary_get(semaforos,identificador);
		sem_post(&semaforosGlobales[posicion]);
		//post(id_semaforo);
		break;
	case 5:
		//pedido E/S, va a bloqueado
		//recibo nombre de dispositivo, y unidades de tiempo a utilizar
		tamanioNombre = recibirProtocolo(cpu);
		identificador = recibirMensaje(cpu,tamanioNombre);
		unidadestiempo = recibirProtocolo(cpu);
		posicion = (int)dictionary_get(dispositivosES,identificador);
		//pcb*pcb = desserializar();
		pcbParaES*pcbParaBloquear=malloc(sizeof(pcbParaES));
		pcbParaBloquear->pcb = pcbDesSerializado;
		pcbParaBloquear->ut = unidadestiempo;
		queue_push(colasES[posicion], pcbParaBloquear);
		sem_post(&semaforosES[posicion]);
		queue_push(colaCPUs, &cpu);
		break;
	}
}
