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
	char** sem_init;		//TRANSFORMAR CON (atoi)
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

typedef struct {
	char *nombre;
	int valor;
}variableGlobarl;

t_list *cpus, *consolas, *variablesGlobales;

//estructuras para planificacion
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_queue *colaListos, *colaExec, *colaBloq, *colaTerminados;
sem_t sem_Listos,sem_Exec, sem_Bloq,sem_Terminado,sem_cpuDisponible;

int autentificarUMC(int);
int leerConfiguracion(char*, datosConfiguracion**);
t_dictionary* crearDiccionario(char** keys);
int comprobarCliente(int);
pcb* crearPCB(char*);
t_list* crearIndiceDeCodigo(t_metadata_program*);
int cortarInstrucciones(t_metadata_program*);
int calcularPaginas(char*);
void mostrar(int*);
void enviarAnsisopAUMC(int, char*,int);
void maximoDescriptor(int* maximo, t_list* lista, fd_set *descriptores);
void atender_Listos();
void atender_Exec(int cpu);
void atender_Bloq();
void atender_Terminados();
int  mandar_instruccion_a_CPU(int cpu, pcb*pcb, int *todoSigueIgual);
int procesar_respuesta(int op,int cpu, pcb*pcb, int *todoSigueIgual);
int procesar_operacion_privilegiada(int operacion, int cpu, pcb*pcb, int *seBloqueo);
void eliminarCPU(int cpu);
int revisarActividad(t_list*, fd_set*);


int ultimoPID=0,tamPagina=0;
datosConfiguracion* datosNucleo;



int main(int argc, char* argv[]) {
	fd_set descriptores;
	cpus = list_create();
	consolas = list_create();
	int max_desc, nuevo_cliente,sin_size = sizeof(struct sockaddr_in) ;
	struct sockaddr_in direccionCliente;
	//--------------------------------CONFIGURACION-----------------------------

	datosNucleo=malloc(sizeof(datosConfiguracion));
	if (!(leerConfiguracion("ConfigNucleo", &datosNucleo) || leerConfiguracion("../ConfigNucleo", &datosNucleo))){
			printf("Error archivo de configuracion\n FIN.");return 1;}
//-------------------------------------------DICCIONARIOS---------------------------------------------------------------
	t_dictionary *globales=crearDiccionario(datosNucleo->shared_vars);
	t_dictionary *semaforos=crearDiccionario(datosNucleo->sem_ids);
	t_dictionary *dispositivosES=crearDiccionario(datosNucleo->io_ids);
	//printf("%d\n",(int)dictionary_get(globales,"!UnaVar"));	//EJEMPLO DE BUSQUEDA
//-----------------------------------------------------------------------------------------------------------------
	/*t_queue* dispositivos[datosNucleo.io_ids.CANTIDAD];
	for(i=0;i<=cant;i++){
	dispositivos[i]=queue_create();}

	variablesGlobales = list_create();
	for(int i=0;i<=sizeof(*datosMemoria->shared_vars);i++){
		variableGlobarl vg;
		vg->nombre = datosMemoria->shared_vars[i];
		vg->valor = 0;
		list_add(variablesGlobales, vg);
	}
	*/
	//---------------------------------PLANIFICACION PCB-----------------------------------
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);//todo agregar el destroy
	pthread_t hiloListos, hiloExec, hiloBloq, hiloTerminados;

	sem_init(&sem_Listos, 0, 0);
	sem_init(&sem_Exec, 0, 0);
	sem_init(&sem_Bloq, 0, 0);
	sem_init(&sem_Terminado, 0, 0);
	sem_init(&sem_cpuDisponible, 0, 0);

	colaListos=queue_create();
	colaExec=queue_create();
	colaBloq=queue_create();
	colaTerminados=queue_create();

	pthread_create(&hiloListos, &attr, (void*)atender_Listos, NULL);
	pthread_create(&hiloBloq, &attr, (void*)atender_Bloq, NULL);		//un hilo por cada e/s, y por parametro el sleep?
	pthread_create(&hiloTerminados, &attr, (void*)atender_Terminados, NULL);



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
		if (revisarActividad(consolas, &descriptores)) {								//Reviso actividad en consolas
			printf("Se desconecto una consola, eliminada\n");
		}
																						//saque la actividad de los cpus, porque se comunica con los hilos de planificacion
		else {
				if (FD_ISSET(conexionUMC, &descriptores)) {								//Me mando algo la UMC
					if (recibirProtocolo(conexionUMC) == -1) {
						printf("Murio la UMC, bye\n");
						return 0;
					}
				}else{
				if (FD_ISSET(nucleo_servidor, &descriptores)) { //aceptar cliente
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
						list_add(cpus, (void *) nuevo_cliente);
						sem_post(&sem_cpuDisponible);
						pthread_create(&hiloExec, &attr, (void*)atender_Exec, (void*)nuevo_cliente);
						printf("Acepté un nuevo cpu\n");
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
t_dictionary* crearDiccionario(char** keys){
	int i=0;
	t_dictionary* diccionario=dictionary_create();
	while(keys[i]!=NULL){
	//	printf("%s\n",keys[i]);
		dictionary_put(diccionario,keys[i],(int*) i);
		i++;
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
			if (protocolo == -1) {							//Se desconecto o algo
				list_remove(lista, i);
				close(componente);
				return 1;
			} else {							//Me mandaron un mje
				char* bufferConsola = malloc(protocolo + 1);
				char* mensaje = recibirMensaje(componente, protocolo);
				free(bufferConsola);
				printf("mensaje de consola: %s", mensaje);
				free(mensaje);
			}
		}
	}
	return 0;
}
//--------------------------------------------PLANIFICACION----------------------------------------------------
 void atender_Listos(){
	 while(1){
	 printf("entro al hilo para listos, esperando..\n");
	 sem_wait(&sem_Listos);
	 printf("se activo el semaforo listo y lo frene, voy a ver los cpus\n");
	 pcb* pcbListo = malloc(sizeof(pcb));
	 pcbListo = queue_pop(colaListos);
	 int paso=1;
	 while(paso){
		 int value;
		 sem_getvalue(&sem_cpuDisponible, &value);
		 if(value>0 ){
			 printf("el proceso %d paso de Listo a Execute\n",pcbListo->PID);
			 queue_push(colaExec, pcbListo);
			 sem_post(&sem_Exec);
			 paso=0;
		 }
	 }
	 }
	 //free(pcbListo);
 }
 void atender_Exec(int cpu){
	 while(1){
	 sem_wait(&sem_Exec);
	 sem_wait(&sem_cpuDisponible);
	 pcb* pcbExec = malloc(sizeof(pcb));
	 pcbExec = queue_pop(colaExec);
	 printf("se activo el sem execute, el proceso %d se va a ejecutar\n",pcbExec->PID);
	 int i,todoSigueIgual=1;
	 for(i=0; i < datosNucleo->quantum; i++){
		 printf("%d. le voy a mandar una instruccion al cpu\n",i);
		 int mandoBien = mandar_instruccion_a_CPU(cpu,pcbExec,&todoSigueIgual);
		 	 if(!mandoBien){
		 		 printf("hubo un error en la ejecucion, no se completo la instruccion y desaparecio el cpu\n");
		 		 printf("el proceso %d paso de Execute a Listo\n",pcbExec->PID);
		 		 queue_push(colaListos, pcbExec);
		 		 sem_post(&sem_Listos);
		 		 i = datosNucleo->quantum;
		 		 todoSigueIgual=0;
		 	 }else{
		 		 printf("%d. le mande una instruccion al cpu\n",i);
		 		 pcbExec->PC++;
		 		 usleep(datosNucleo->quantum_sleep);
		 		 if(!todoSigueIgual){						//todo esto no esta funcionando
		 			 i = datosNucleo->quantum; //el pcb paso a otro estado
		 	 	 	 sem_post(&sem_cpuDisponible);
		 		 }
		 	 }
	  }
		 	if(todoSigueIgual){
		 		printf("el proceso %d paso de Execute a Listo\n",pcbExec->PID);
	 		 	queue_push(colaListos, pcbExec);
	 		 	sem_post(&sem_Listos);
	 	 	 	sem_post(&sem_cpuDisponible);
		 	}


	 }
	// free(pcbExec);

 }
 void atender_Bloq(){
	 //varias colas?... cola para wait y para las e/s
	 /*obtener_valor [identificador de variable compartida]
		grabar_valor [identificador de variable compartida] [valor a grabar]
		wait [identificador de semáforo]
		signal [identificador de semáforo]
		entrada_salida [identificador de dispositivo] [unidades de tiempo a utilizar]
	  */
 }
 void atender_Terminados(){
	 sem_wait(&sem_Terminado);
	 pcb* pcbTerminado = (pcb*) malloc(sizeof(pcb));
	 pcbTerminado = queue_pop(colaTerminados);
	//todo avisar umc y consola que termino el programa
	 free(pcbTerminado);
 }

int mandar_instruccion_a_CPU(int cpu, pcb *pcb, int *todoSigueIgual){
	char* Instruccion = string_new();
	string_append(&Instruccion, "1");
	string_append(&Instruccion, header(pcb->PID));
	string_append(&Instruccion, header(pcb->PC));
	string_append(&Instruccion, header(pcb->SP));
	string_append(&Instruccion, "\0");
	int enviados = send(cpu, Instruccion, string_length(Instruccion), 0);
	free(Instruccion);
	if(enviados==-1){
		printf("error del send\n");
	 	sem_post(&sem_cpuDisponible);
		eliminarCPU(cpu);
		todoSigueIgual=0;
		return 0;
	}else{
		printf("mande %d bytes de instruccion, esperando respuesta..\n",enviados);


		int operacion=0;
		int recibidos = recv(cpu, &operacion, 4, 0);
		if(recibidos == -1){
			printf("error en el recv\n");
 	 	 	sem_post(&sem_cpuDisponible);
			eliminarCPU(cpu);
			todoSigueIgual=0;
			return 0;
		}else{
		operacion=ntohl(operacion);

		printf("recibi la respuesta codigo: %d\n",operacion);
		int procesoBien = procesar_respuesta(operacion, cpu, pcb, todoSigueIgual);
			if (!procesoBien){
				return 0;
			}else{
				return 1;
			}
		}
	}
}
int procesar_respuesta(int op,int cpu, pcb*pcb, int *todoSigueIgual){
		int mostrar, tamanio, seBloqueo=0,operacion;
		char* texto;
	switch (op){
	case 0:
		printf("el cpu me mando algo mal, se rompio algo(?\n");
		todoSigueIgual=0;
	 	sem_post(&sem_cpuDisponible);
		eliminarCPU(cpu);
		return 0;
		break;
	case 1:
		//termino bien la instruccion, no necesita nada
		printf("el cpu me devolvio la instruccion, no necesita nada\n");
		return 1;
		break;
	case 2:
		//me pide una operacion privilegiada
		operacion = atoi(recibirMensaje(cpu,1));
		int procesoBien = procesar_operacion_privilegiada(operacion, cpu, pcb, &seBloqueo);
		if(!procesoBien){
			todoSigueIgual=0;
 	 	 	sem_post(&sem_cpuDisponible);
			eliminarCPU(cpu);//todo este hara falta?
			return 0;
		}else{
			if(seBloqueo){
				printf("el proceso %d paso de Execute a Bloqueado\n",pcb->PID);
				todoSigueIgual=0;
				queue_push(colaBloq, pcb);
				sem_post(&sem_Bloq);
			}
			return 1;
		}
		break;
	case 3:
		//termino el ansisop, va a listos
		printf("el proceso %d paso de Execute a Terminado\n",pcb->PID);
		todoSigueIgual=0;
		queue_push(colaTerminados, pcb);
		sem_post(&sem_Terminado);
		return 1;
		break;
	case 4:
		//imprimir
		//agregar header cod_op
		mostrar = recibirProtocolo(cpu);
		send(pcb->consola, mostrar, 4, 0);
		return 1;
		break;
	case 5:
		//imprimirTexto
		//agregar header cod_op
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu, tamanio);
		//send(pcb->consola, concatentar headers,(tamanio+4);
		return 1;
		break;

	}

}
int procesar_operacion_privilegiada(int operacion, int cpu, pcb*pcb, int *seBloqueo){
	int tamanioNombre, valorAGuardar, unidadestiempo,valor;
	char *nombreVar, id_semaforo, id_es;

	switch (operacion){
	case 0:
		printf("el cpu mando mal la operacion privilegiada, todo mal\n");
		return 0;
		break;
	case 1:
		//obtener valor de variable compartida
		//recibo nombre de variable compartida, devuelvo su valor
		tamanioNombre = recibirProtocolo(cpu);
		nombreVar = recibirMensaje(cpu,tamanioNombre);
//		valor = valor_de_esta_variable_compartida(nombreVar); //todo funcion de comparar variables
		send(cpu, valor, 4, 0);
		return 1;
		break;
	case 2:
		//grabar valor en variable compartida
		//recibo el nombre de una variable, y un valor - guardo
		tamanioNombre = recibirProtocolo(cpu);
		nombreVar = recibirMensaje(cpu,tamanioNombre);
		valorAGuardar = recibirProtocolo(cpu);
	//	guardar_valor_en_variable_compartida(nombreVar);
		return 1;
		break;
	case 3:
		//wait a un semaforo, si no puiede acceder, se bloquea
		//recibo el identificador del semaforo
		tamanioNombre = recibirProtocolo(cpu);
		id_semaforo = recibirMensaje(cpu,tamanioNombre);
		//ver como esta el semaforo asociado
		//si esta ok => bloqueo el semaforo
		//si esta block => bloqueo el pcb, hasta que otro proceso desbloquee el semaforo
		return 1;
		break;
	case 4:
		//signal a un semaforo, post
		//recibo el identificador del semaforo
		tamanioNombre = recibirProtocolo(cpu);
		id_semaforo = recibirMensaje(cpu,tamanioNombre);
		//post(id_semaforo);
		return 1;
		break;
	case 5:
		//pedido E/S, va a bloqueado
		//recibo nombre de dispositivo, y unidades de tiempo a utilizar
		tamanioNombre = recibirProtocolo(cpu);
		id_es = recibirMensaje(cpu,tamanioNombre);
		unidadestiempo = recibirProtocolo(cpu);
		//mandar pcb a lista bloqueo de esa E/S, con la ut
		seBloqueo=1;
		return 1;
		break;
	}
}

void eliminarCPU(int cpu){
	printf("el cpu %d no responde o algo, se lo elimino.\n",cpu);
	list_remove(cpus, 0); //todo diccionario para ver el index de los cpus? o recorrer lista y ver cual eliminar
	close(cpu);
	sem_wait(&sem_cpuDisponible);
}
