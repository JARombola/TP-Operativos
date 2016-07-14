#include "nucleo.h"

char* rutaConfig;

int main(int argc, char* argv[]) {			// 	!!!!!PARA EJECUTAR: 						./Nucleo ../Config1		(o el numero de configuracion que sea)
	archivoLog = log_create("Nucleo.log", "Nucleo", true, log_level_from_string("INFO"));
	rutaConfig=argv[1];
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
	if (!leerConfiguracion(rutaConfig, &datosNucleo)){
			log_error(archivoLog,"No se pudo leer archivo de Configuracion");
			return 1;}
	pthread_create(&thread, &attr, (void*)Modificacion_quantum, NULL);
//-------------------------------------------DICCIONARIOS---------------------------------------------------------------
	globales = crearDiccionarioGlobales(datosNucleo->shared_vars);
	semaforos = crearDiccionarioSEMyES(datosNucleo->sem_ids,datosNucleo->sem_init, 0);
	dispositivosES = crearDiccionarioSEMyES(datosNucleo->io_ids,datosNucleo->io_sleep,1);

	//---------------------------------PLANIFICACION PCB-----------------------------------

	sem_init(&sem_Nuevos, 0, 0);
	sem_init(&sem_Listos, 0, 0);
	sem_init(&sem_cpusDisponibles, 0, 0);

	cpusDisponibles=list_create();
	colaNuevos=queue_create();
	colaListos=queue_create();

	pthread_create(&thread, &attr, (void*)atender_Ejecuciones, NULL);
	pthread_create(&thread, &attr, (void*)atender_Nuevos, NULL);

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
	char* codigo;
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
			log_info(archivoLog,"[Desconexion] Consola %d, eliminada",socketARevisar);
			int estaBloqueada = buscar_pcb_en_bloqueados(socketARevisar);
			if(estaBloqueada){										//TODO REVISAR ESTOOOOOOOO
				log_info(archivoLog,"Proceso %d eliminado (estaba bloqueado)",socketARevisar);
			 	finalizarProgramaUMC(socketARevisar);
			}
		//	close(socketARevisar);
		}
		else {
			socketARevisar = revisarActividadCPUs(&descriptores);
			if (socketARevisar) {								//Reviso actividad en cpus
				log_info(archivoLog,"[Desconexion] CPU %d, eliminada",socketARevisar);
		//		close(socketARevisar);
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
						int tamanio;
						char* mjeConsola;
						switch (comprobarCliente(nuevo_cliente)) {

						case 0:										//ERROR!!
							log_error(archivoLog,"Falló el handshake\n");
							close(nuevo_cliente);
							break;

						case 1:											//CPU
							send(nuevo_cliente, &mjeCpu, 4, 0);
							log_info(archivoLog,"--NUEVO CPU: %d",nuevo_cliente);
							list_add(cpusDisponibles, (void *)nuevo_cliente);
							sem_post(&sem_cpusDisponibles);
							list_add(cpus, (void *) nuevo_cliente);
							break;

						case 2:						//CONSOLA, RECIBO EL CODIGO
							mjeConsola=toStringInt(nuevo_cliente);
							send(nuevo_cliente, mjeConsola, 4, 0);
							free(mjeConsola);
							log_info(archivoLog,"--NUEVA consola: %d",nuevo_cliente);
							tamanio = recibirProtocolo(nuevo_cliente);
							if (tamanio > 0) {
								codigo = recibirMensaje(nuevo_cliente, tamanio);
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
	char* pcbNuevo;
	if(!aceptado){													//consola rechazada
		log_warning(archivoLog,"Ansisop rechazado");
		send(consola,"0000",4,0);}
	else{
			send(consola,"0001",4,0);
			list_add(consolas,(void*)consola);
			char* prog=string_substring_from(codigo,4);

			pcbNuevo= crearPCB(prog,consola);
			free(prog);

	if(aceptado==1){
			log_info(archivoLog,"Ansisop aceptado por UMC");
			log_info(archivoLog,"Nuevo PCB en cola de LISTOS!");
			queue_push(colaListos, pcbNuevo);
			sem_post(&sem_Listos);
	}else{
			log_info(archivoLog,"Ansisop aceptado por UMC");
			log_info(archivoLog,"Nuevo PCB en cola de NUEVOS!");
			queue_push(colaNuevos, pcbNuevo);
			sem_post(&sem_Nuevos);
		}
	}
}


char* crearPCB(char* codigo, int pid) {
	PCB* pcb=malloc(sizeof(PCB));
	t_metadata_program* metadata = metadata_desde_literal(codigo);
	pcb->indices = metadata;
	pcb->paginas_codigo = calcularPaginas(codigo);
	pcb->pc = metadata->instruccion_inicio;
	pcb->stack = list_create();
	pcb->id=pid;
	char* pcbChar=toStringPCB(*pcb);
	list_destroy(pcb->stack);
	metadata_destruir(pcb->indices);
	free(pcb);
	return pcbChar;
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
		int componente = (int)list_get(consolas, i);
		if (FD_ISSET(componente, descriptores)) {
			int protocolo = recibirProtocolo(componente);
			if (protocolo == -1) {				//si murio de golpe, tengo que eliminar el pcb
				list_add(listConsolasParaEliminarPCB,(void *) componente);
				list_remove(consolas, i);
				return componente;
			}else{
				list_remove(consolas, i);		//sino, me manda un 1, que termino bien
				log_info(archivoLog,"[Desconexion] Consola: %d eliminada. (Terminó su programa)",componente);
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
	char* pcbNuevo;
	while(1){
		 sem_wait(&sem_Nuevos); //se libero un pcb en la umc
		 if(!queue_is_empty(colaNuevos)){ //si hay alguno
			 pcbNuevo = queue_pop(colaNuevos);
		 	 queue_push(colaListos,pcbNuevo); //entonces lo mando a Listos
			 log_info(archivoLog,"Proceso %d: [Nuevo] => [Listo]",obtenerPID(pcbNuevo));
		 	 sem_post(&sem_Listos);
		 }
	}
}

void atender_Ejecuciones(){
	char* mensajeCPU,*pcbListo;
	int cpu;
	 while(1){
		 sem_wait(&sem_Listos);
		 pcbListo= queue_pop(colaListos);
		 int pid=obtenerPID(pcbListo);
		 if(ese_PCB_hay_que_eliminarlo(pid)){
			 log_info(archivoLog,"Consola del Proceso %d no existe, se lo ELIMINARÁ",pid);
		 	 finalizarProgramaUMC(pid);
		 	 sem_post(&sem_Nuevos);
		 }else{
		 sem_wait(&sem_cpusDisponibles);
			cpu = (int) list_remove(cpusDisponibles, 0); //saco el socket de ese cpu disponible
		    pthread_mutex_lock(&mutexInotify);
			mensajeCPU = serializarMensajeCPU(pcbListo, datosNucleo->quantum, datosNucleo->quantum_sleep);		//todo TERMINAR ESTO!!! semaforo para inotify?
		    pthread_mutex_unlock(&mutexInotify);
			send(cpu,mensajeCPU,string_length(mensajeCPU),0);
			log_info(archivoLog,"Proceso %d: [Listo] => [Execute]",pid);
			free(mensajeCPU);
			free(pcbListo);
		 }
	 }
 }


 void atender_Bloq_ES(int posicion){
	 int miSLEEP = dispositivosSleeps[posicion];
	 pcbParaES* pcbBloqueando;
	 int pid;
	 while(1){
	 	 sem_wait(&semaforosES[posicion]);
	 	 pcbBloqueando = queue_pop(colasES[posicion]);
	 	 usleep(miSLEEP*pcbBloqueando->ut*1000);
		 queue_push(colaListos, pcbBloqueando->pcb);
		 pid=obtenerPID(pcbBloqueando->pcb);
		 log_info(archivoLog,"Proceso %d: [Bloqueado] (IO) => [Listo]",pid);
		 sem_post(&sem_Listos);
	//	 free(pcbBloqueando);						//TESTEADO, PARECE NO IR...
	 }
 }

 void atender_Bloq_SEM(int posicion){
	 char* pcbBloqueando;
	 while(1){
		 sem_wait(&semaforosGlobales[posicion]);
		 	 if(!queue_is_empty(colasSEM[posicion])){
		 		 pcbBloqueando = queue_pop(colasSEM[posicion]);
		 		 int pid=obtenerPID(pcbBloqueando);
		 		 queue_push(colaListos, pcbBloqueando);
		 		 log_info(archivoLog,"Proceso %d: [Bloqueado] (SEM) => [Listo]",pid);
		 	 	 sem_post(&sem_Listos);
		 	 }
	 }
}


void atenderOperacion(int op,int cpu){
#define ERROR 0
#define QUANTUM_OK 1
#define PRIVILEGIADA 2
#define FIN_ANSISOP 3
#define IMPRIMIR 4

	int tamanio, consola, operacion, pidMalo, sigueCPU, pid, cod_fin;
	char *texto, *pcb;

	switch (op){

	case ERROR:													//CPU Desconectado / error ansisop
		pidMalo = recibirProtocolo(cpu);
		if(pidMalo){
			operacion = 3;
		 	finalizarProgramaConsola(pidMalo, operacion);
		 	finalizarProgramaUMC(pidMalo);
		 	log_error(archivoLog,"Error en proceso %d, ELIMINADO",pidMalo);
		 	sem_post(&sem_Nuevos);
		}
		sigueCPU = recibirProtocolo(cpu);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void*)cpu);
			sem_post(&sem_cpusDisponibles);
 		}else{
 			sacar_socket_de_lista(cpus,cpu);
 			log_info(archivoLog,"[Desconexion] CPU %d: ELIMINADA (v2)",cpu);
 		}
		break;

	case QUANTUM_OK:
		tamanio = recibirProtocolo(cpu);
		pcb = recibirMensaje(cpu,tamanio);
	//	printf("PCB RECIBIDO: \n %s\n",pcb);
		pid=obtenerPID(pcb);
		sigueCPU = recibirProtocolo(cpu);
 		log_info(archivoLog,"Proceso %d: [Execute] => [Listo]",pid);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void*)cpu);
			sem_post(&sem_cpusDisponibles);
 		}
		queue_push(colaListos, pcb);
		sem_post(&sem_Listos);
		break;

	case PRIVILEGIADA:
		operacion = recibirProtocolo(cpu);
		procesar_operacion_privilegiada(operacion, cpu);
		break;

	case FIN_ANSISOP:
		cod_fin = 2;
		tamanio = recibirProtocolo(cpu);
		pcb = recibirMensaje(cpu,tamanio);
		sigueCPU = recibirProtocolo(cpu);
		pid=obtenerPID(pcb);
		log_info(archivoLog,"Proceso %d: [Execute] => [Terminado]",pid);
 		if(sigueCPU){
			list_add(cpusDisponibles, (void *)cpu);
			sem_post(&sem_cpusDisponibles);
 		}
	 	 finalizarProgramaConsola(pid, cod_fin);
	 	 finalizarProgramaUMC(pid);
	 	 log_info(archivoLog,"Proceso %d: TERMINADO\n",pid);
	 	 free(pcb);
	 	 sem_post(&sem_Nuevos);
		break;

	case IMPRIMIR:
		consola = recibirProtocolo(cpu);
		tamanio = recibirProtocolo(cpu);
		texto = recibirMensaje(cpu, tamanio);  					 //texto o valor
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

	int tamanioNombre, posicion, unidadestiempo,valor,tamanio,sigueCPU, pid;
	char *identificador, *valor_char, *ut;
	char *pcb;

	if (operacion){
		tamanioNombre = recibirProtocolo(cpu);
		identificador = recibirMensaje(cpu,tamanioNombre);
	}

	switch (operacion){
		case ERROR:							//no deberia pasar NUNCA
			perror("el cpu mando mal la operacion privilegiada, todo mal\n");
			break;

		case OBTENER_COMPARTIDA:						//envía el valor
			//printf("Me pidio una compartida\n");
			posicion = (int)dictionary_get(globales,identificador);
			valor = globalesValores[posicion];
			valor=htonl(valor);
			send(cpu,&valor,4,0);
			free(identificador);
			break;

		case GUARDAR_COMPARTIDA:							//guarda y devuelve el valor
			//printf("Me asigno una compartida\n");
			tamanio=recibirProtocolo(cpu);
			valor_char=recibirMensaje(cpu,tamanio);
			valor=atoi(valor_char);
			free(valor_char);
			posicion = (int)dictionary_get(globales,identificador);
			globalesValores[posicion] = valor;
			valor=htonl(valor);
			send(cpu,&valor,4,0);
			free(identificador);
			break;

		case WAIT:													//si no puede acceder al sem => se bloquea el proceso
			posicion = (int)dictionary_get(semaforos,identificador);
			contadorSemaforo[posicion]--;
			if(contadorSemaforo[posicion]<0){								//si es < a 0, se tiene que bloquear el pcb
				send(cpu, "no", 2, 0);										//=> Pido el pcb
				tamanio = recibirProtocolo(cpu); 							//tamaño del pcb
				pcb = recibirMensaje(cpu,tamanio);							//PCB
				pid=obtenerPID(pcb);
				sigueCPU = recibirProtocolo(cpu);
				queue_push(colasSEM[posicion], pcb); 						//mando el pcb a bloqueado
				log_info(archivoLog,"Proceso %d: [Execute] => [Bloqueado]",pid);
				if(sigueCPU){
					list_add(cpusDisponibles, (void *)cpu);
					sem_post(&sem_cpusDisponibles);
				}
			}else{											//si no, ok
				send(cpu, "ok", 2, 0);
			}
			free(identificador);
			break;

		case SIGNAL:											//post al semaforo
			posicion = (int)dictionary_get(semaforos,identificador);
			free(identificador);
			if(!queue_is_empty(colasSEM[posicion])){
				sem_post(&semaforosGlobales[posicion]); //si era < 0, tengo alguien que desbloquear
			}
			contadorSemaforo[posicion]++;
			send(cpu,"0001",4,0);
			break;

		case E_S:											//ansisop bloqueado => recibo pcb
			tamanio=recibirProtocolo(cpu);
			ut=recibirMensaje(cpu,tamanio);
			unidadestiempo = atoi(ut);
			free(ut);
			posicion = (int)dictionary_get(dispositivosES,identificador);
			free(identificador);
			tamanio = recibirProtocolo(cpu);
			pcb = recibirMensaje(cpu,tamanio);

			sigueCPU = recibirProtocolo(cpu);

			pcbParaES *pcbParaBloquear=malloc(sizeof(pcbParaES));

			pcbParaBloquear->pcb = pcb;
			pcbParaBloquear->ut = unidadestiempo;

			queue_push(colasES[posicion], pcbParaBloquear);

			pid=obtenerPID(pcb);

			log_info(archivoLog,"Proceso %d: [Execute] => [Bloqueado]",pid);
			sem_post(&semaforosES[posicion]);
			if(sigueCPU){
				list_add(cpusDisponibles, (void *)cpu);
				sem_post(&sem_cpusDisponibles);
			}
			break;
	}
}
void sacar_socket_de_lista(t_list* lista,int socket){
	int buscarIgual(int elemLista){
		return (socket==elemLista);}
	list_remove_by_condition(lista,(void*)buscarIgual);//entra por cpusDisponibles?
}

int esa_consola_existe(int consola){
	int buscarIgual(int elemLista){
		return (consola==elemLista);}
	return(list_any_satisfy(consolas,(void*)buscarIgual));
}

int ese_PCB_hay_que_eliminarlo(int consola){ //devuelve si esa consola esta en la lista de eliminadas
	int buscarIgual(int elemLista){
		return (consola==elemLista);}

	if(list_any_satisfy(listConsolasParaEliminarPCB,(void*)buscarIgual)){
		list_remove_by_condition(listConsolasParaEliminarPCB,(void*)buscarIgual);
		return 1;}
	return 0;
}

char* serializarMensajeCPU(char* pcbListo, int quantum, int quantum_sleep){
	char* mensaje=string_new();
	char* quantum_char = toStringInt(quantum);
	char* quantum_sleep_char = toStringInt(quantum_sleep);
	string_append(&mensaje,quantum_char);
	string_append(&mensaje,quantum_sleep_char);
	agregarHeader(&pcbListo);
	string_append(&mensaje,pcbListo);
	string_append(&mensaje,"\0");
//	printf("Mensaje: %s\n",mensaje); //pcb serializado
	free(quantum_char);
	free(quantum_sleep_char);
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
	t_config* archivoConfiguracion;
	if (fd_config < 0) {
		perror("inotify_init");
	}

	int watch_descriptor = inotify_add_watch(fd_config, rutaConfig, IN_CLOSE_WRITE);//IN_CLOSE_WRITE);IN_MODIFY

	while(watch_descriptor){
		int length = read(fd_config, buffer, BUF_LEN);
		if (length < 0) {
			perror("read");
		}
		do{
			archivoConfiguracion = config_create(rutaConfig);
		}while(archivoConfiguracion == NULL);
	    	pthread_mutex_lock(&mutexInotify);
			datosNucleo->quantum = config_get_int_value(archivoConfiguracion, "QUANTUM");
			datosNucleo->quantum_sleep = config_get_int_value(archivoConfiguracion,"QUANTUM_SLEEP");
		    pthread_mutex_unlock(&mutexInotify);
			log_info(archivoLog,"---QUANTUM nuevo: %d |	QUANTUM_SLEEP: %d",(datosNucleo)->quantum, (datosNucleo)->quantum_sleep);

		config_destroy(archivoConfiguracion);
	}
}

int buscar_pcb_en_bloqueados(int pid){
	//busco entre las de ES y despues las de SEM
	//semaforo para bloquearo los hilos de las colas? no hizo falta
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
	void eliminar(char* pcb){
		free(pcb);
	}
	int buscarIgual(char* elemLista){
		int idEliminar=obtenerPID(elemLista);
		return (pid==idEliminar);}

	int encontrado=list_any_satisfy(cola->elements,(void*)buscarIgual);

	list_remove_and_destroy_by_condition(cola->elements,(void*)buscarIgual,(void*)eliminar);

	return encontrado;
}

int obtenerPID(char* pcb){
	char* id=string_substring(pcb,0,4);
	int pid=atoi(id);
	free(id);
	return pid;
}

