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
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <parser/metadata_program.h>
#include <semaphore.h>
#include <pthread.h>


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
struct sockaddr_in crearDireccion(int puerto, char* ip);
int conectarUMC(int, char* ip);
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
void enviarAnsisopAUMC(int, char*,int);
void maximoDescriptor(int* maximo, t_list* lista, fd_set *descriptores);
void atender_Listos();
void atender_Exec();
void atender_Bloq();
void atender_Terminados();
void  mandar_instruccion_a_CPU();
void procesar_respuesta();
int revisarActividad(t_list*, fd_set*);


int ultimoPID=0,tamPagina=0;
datosConfiguracion* datosNucleo;

int main(int argc, char* argv[]) {
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

	pthread_create(&hiloListos, &attr, (void*)atender_Listos, NULL);
	pthread_create(&hiloBloq, &attr, (void*)atender_Bloq, NULL);		//un hilo por cada e/s, y por parametro el sleep?
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
	struct sockaddr_in direccionNucleo = crearDireccion(datosNucleo->puerto_nucleo, datosNucleo->ip);
	printf("Nucleo creado, conectando con la UMC...\n");
	int conexionUMC = conectarUMC(datosNucleo->puerto_umc, datosNucleo->ip_umc);
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
		} else {
			if (revisarActividad(cpus, &descriptores)) {								//Reviso actividad en cpus
				printf("Se desconecto una CPU, eliminada\n");
				//sem_wait(&sem_cpuDisponible);
			} else {
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
//				sem_post(&sem_cpuDisponible);														//todo agregar semaforos cuando funcionen
//				pthread_create(&hiloExec, &attr, (void*)atender_Exec, (void*)nuevo_cliente);
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
struct sockaddr_in crearDireccion(int puerto, char* ip) {
	struct sockaddr_in direccion;
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr = inet_addr(ip);
	direccion.sin_port = htons(puerto);
	return direccion;
}

int conectarUMC(int puerto, char* ip) {
	struct sockaddr_in direccionUMC = crearDireccion(puerto, ip);
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
	int bufferHandshake=0;
	recv(nuevoCliente, &bufferHandshake, 4, 0);
	bufferHandshake=ntohl(bufferHandshake);
	return bufferHandshake;
}

int recibirProtocolo(int conexion){
	char* protocolo = malloc(5);
	int bytesRecibidos = recv(conexion, protocolo, sizeof(int32_t), 0);
	if (bytesRecibidos <= 0) {printf("Error al recibir protocolo\n");
		free(protocolo);
		return -1;}
	protocolo[4]='\0';
	return atoi(protocolo);}

char* recibirMensaje(int conexion, int tamanio){
	char* mensaje=(char*)malloc(tamanio+1);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, 0);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		free(mensaje);
		return "a";}
	mensaje[tamanio]='\0';
	return mensaje;
}

char* header(int numero){										//Recibe numero de bytes, y lo devuelve en 4 bytes (Ej. recibe "2" y devuelve "0002")
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

void enviarAnsisopAUMC(int conexionUMC, char* codigo,int consola){
	int paginasNecesarias=calcularPaginas(codigo);
	char* mensajeInicial = string_new();
	pcb* pcbNuevo = crearPCB(codigo);
	pcbNuevo->consola=consola;
	int i;
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
		queue_push(colaListos, pcbNuevo);								//todo
//		sem_post(&sem_Listos);
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
	printf("***CODIGO:%s\n", codigo);
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
	int offset,acum=0,cantMarcos,totalMarcos=0;
	do {offset=0;
		for (offset = 0; codigo[acum] != '\n'; offset++, acum++) {
//			printf("%c", codigo[acum]);											//Para controlar corte del codigo
		}
		cantMarcos = offset / tamPagina;
		if (offset % tamPagina || !cantMarcos)	cantMarcos++;
		totalMarcos += cantMarcos;
//		printf("	-Cant marcos: %d | Total %d\n", cantMarcos, totalMarcos);
		if(codigo[acum]=='\n') acum++;
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
	//	list_iterate(lineas,(void*)mostrar);
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
	 sem_wait(&sem_Listos);
	 pcb* pcbListo = malloc(sizeof(pcb));					//todo ?????????????
	 pcbListo = queue_pop(colaListos);
	 int paso=1;
	 while(paso){
		 if(&sem_cpuDisponible>0 ){
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
	 for(i=0; i<datosNucleo->quantum; i++){
		 mandar_instruccion_a_CPU(cpu,pcbExec,&todoSigueIgual);
		 pcbExec->PC++;
		 sleep(datosNucleo->quantum_sleep);
		 if(!todoSigueIgual){
			 i = datosNucleo->quantum; //el pcb paso a otro estado
		 }
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
	 pcb* pcbTerminado = (pcb*) malloc(sizeof(pcb));
	 pcbTerminado = queue_pop(colaTerminados);
	//todo avisar umc y consola que termino el programa
	 free(pcbTerminado);
 }

void mandar_instruccion_a_CPU(int cpu, pcb*pcb, int igual){
	char* Instruccion = string_new();
	string_append(&Instruccion, "1");
	string_append(&Instruccion, header(pcb->PID));
	string_append(&Instruccion, header(pcb->PC));
	string_append(&Instruccion, header(pcb->SP));
	string_append(&Instruccion, "\0");
	send(cpu, Instruccion, string_length(Instruccion), 0);
	free(Instruccion);

	int operacion = atoi(recibirMensaje(cpu,1));
	procesar_respuesta(operacion, cpu,  pcb, &igual);
}
void procesar_respuesta(int op,int cpu, pcb*pcb, int todoSigueIgual){
		int mostrar, tamanio, seBloqueo=0,operacion;
		char* texto;
	switch (op){
	case 1:
		//termino bien la instruccion, no necesita nada
		break;
	case 2:
		operacion = atoi(recibirMensaje(cpu,1));
		procesar_operacion_privilegiada(operacion, cpu, pcb, &seBloqueo);
		if(seBloqueo){
			printf("el proceso %d paso de Execute a Bloqueado",pcb->PID);
			todoSigueIgual=0;
			queue_push(colaBloq, pcb);
			sem_post(&sem_Bloq);
		}
		break;
	case 3:
		//termino el ansisop, va a listos
		printf("el proceso %d paso de Execute a Terminado",pcb->PID);
		todoSigueIgual=0;
		queue_push(colaTerminados, pcb);
		sem_post(&sem_Terminado);
		break;
	case 4:
		//imprimir
		//agregar header cod_op
		mostrar = recibirProtocolo(cpu);
		send(pcb->consola, mostrar, 4, 0);
		break;
	case 5:
		//imprimirTexto
		//agregar header cod_op
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu, tamanio);
		//send(pcb->consola, concatentar headers,(tamanio+4);
		break;

	}

}
void procesar_operacion_privilegiada(int operacion, int cpu, pcb*pcb, int seBloqueo){
	int tamanioNombre, valorAGuardar, unidadestiempo,valor;
	char *nombreVar, id_semaforo, id_es;

	switch (operacion){
	case 1:
		//obtener valor de variable compartida
		//recibo nombre de variable compartida, devuelvo su valor
		tamanioNombre = recibirProtocolo(cpu);
		nombreVar = recibirMensaje(cpu,tamanioNombre);
//		valor = valor_de_esta_variable_compartida(nombreVar); //todo funcion de comparar variables
		send(cpu, valor, 4, 0);
		break;
	case 2:
		//grabar valor en variable compartida
		//recibo el nombre de una variable, y un valor - guardo
		tamanioNombre = recibirProtocolo(cpu);
		nombreVar = recibirMensaje(cpu,tamanioNombre);
		valorAGuardar = recibirProtocolo(cpu);
	//	guardar_valor_en_variable_compartida(nombreVar);
		break;
	case 3:
		//wait a un semaforo, si no puiede acceder, se bloquea
		//recibo el identificador del semaforo
		tamanioNombre = recibirProtocolo(cpu);
		id_semaforo = recibirMensaje(cpu,tamanioNombre);
		//ver como esta el semaforo asociado
		//si esta ok => bloqueo el semaforo
		//si esta block => bloqueo el pcb, hasta que otro proceso desbloquee el semaforo
		break;
	case 4:
		//signal a un semaforo, post
		//recibo el identificador del semaforo
		tamanioNombre = recibirProtocolo(cpu);
		id_semaforo = recibirMensaje(cpu,tamanioNombre);
		//post(id_semaforo);
		break;
	case 5:
		//pedido E/S, va a bloqueado
		//recibo nombre de dispositivo, y unidades de tiempo a utilizar
		tamanioNombre = recibirProtocolo(cpu);
		id_es = recibirMensaje(cpu,tamanioNombre);
		unidadestiempo = recibirProtocolo(cpu);
		//mandar pcb a lista bloqueo de esa E/S, con la ut
		seBloqueo=1;
		break;
	}
}
