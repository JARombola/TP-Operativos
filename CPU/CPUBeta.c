#include "CPUBeta.h"
#include <pthread.h>
#include <commons/process.h>
#include <signal.h>

void crearHiloSignal();
void hiloSignal();
void funcionSenial(int n);
int enviarMjeFinANucleo(int);

int ejecutar=1,murio=0,errorAnsisop=0,bloqueado=0;

int main(){
	printf("CPU estable...[%d] \n",process_getpid());

	if(levantarArchivoDeConfiguracion()<0) return -1;

	crearHiloSignal();

	conectarseAlNucleo();
	if (nucleo < 0) return -1;

	conectarseALaUMC();
	if (umc < 0) return -1;

	if (procesarPeticion()<0) return -1;

	printf("Cerrando CPU.. \n");

	return 0;
}
void hiloSignal();
void crearHiloSignal(){
	pthread_t th_seniales;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	pthread_create(&th_seniales, NULL, (void*)hiloSignal, NULL);
}

void hiloSignal(){
		signal(SIGUSR1, funcionSenial);					//(SIGUSR1) ---------> Kill -10 (ProcessPid)
		signal(SIGINT,funcionSenial);					//(SIGINT) ---------> Ctrl+C
}

void funcionSenial(int n){
	int error;
	switch(n){
	case SIGUSR1:
		printf("Me mataron con SIGUSR1\n");
		sleep(2);
		printf("Adios mundo cruel\n");
		murio=1;
		return;
		break;

	case SIGINT:
		printf("Por que me cerraste? \n");
		sleep(2);
		printf("Mentira, bye\n");
		finalizado=0;						//Para que entre bien en enviarMjeFinNucleo
		enviarMjeFinANucleo(1);
		exit(0);
		break;
	}
}


int levantarArchivoDeConfiguracion(){
	FILE* archivoDeConfiguracion = fopen("../ArchivoDeConfiguracionCPU.txt","r");
	if (archivoDeConfiguracion==NULL){
		archivoDeConfiguracion = fopen("ArchivoDeConfiguracionCPU.txt","r");
		if (archivoDeConfiguracion==NULL){
		printf("Error: No se pudo abrir el archivo de configuracion, verifique su existencia en la ruta: %s \n", ARCHIVO_DE_CONFIGURACION);
		return -1;}
	}
	char* archivoJson =toJsonArchivo(archivoDeConfiguracion);
	char puertoDelNucleo [6];
	buscar(archivoJson,"PUERTO_NUCLEO", puertoDelNucleo);
	PUERTO_NUCLEO = atoi(puertoDelNucleo);
	if (PUERTO_NUCLEO == 0){
		printf("Error: No se ha encontrado el Puerto del Nucleo en el archivo de Configuracion \n");
		return -1;
	}

	buscar(archivoJson,"AUTENTIFICACION", AUTENTIFICACION);
	if (AUTENTIFICACION[0] =='\0'){
		printf("Error: No se ha encontrado la Autentificacion en el archivo de Configuracion \n");
		return -1;
	}
	buscar(archivoJson,"IP_NUCLEO", IP_NUCLEO);
	if (IP_NUCLEO[0] == '\0'){
		printf("Error: No se ha encontrado la IP del Nucleo en el archivo de Configuracion \n");
		return -1;
	}
	buscar(archivoJson,"IP_UMC", IP_UMC);
	if (IP_UMC[0] =='\0'){
		printf("Error: No se ha encontrado la IP de la UMC en el archivo de Configuracion \n");
		return -1;
	}

	char puertoDeLaUMC[6];
	buscar(archivoJson,"PUERTO_UMC", puertoDeLaUMC);
	PUERTO_UMC = atoi(puertoDeLaUMC);
	if (PUERTO_UMC == 0){
		printf("Error: No se ha encontrado el Puerto de la UMC en el archivo de Configuracion \n");
		return -1;
	}

	return 0;
}


void conectarseAlNucleo(){
	nucleo = conectar(PUERTO_NUCLEO,IP_NUCLEO);
	if (nucleo<0){
		printf("Error al conectarse con el nucelo \n");
		return;
	}
	autentificar(nucleo,AUTENTIFICACION);
	printf("Conexion con el nucleo OK... \n");
}

void conectarseALaUMC(){
	umc = conectar(PUERTO_UMC,IP_UMC);
	if (umc<0){
		printf("Error: No se ha logrado establecer la conexion con la UMC\n");
	}
	TAMANIO_PAGINA = autentificar(umc,AUTENTIFICACION);
	if (!TAMANIO_PAGINA){
		printf("Error: No se ha logrado establecer la conexion con la UMC\n");
		}
	printf("Conexion con la UMC OK...\n");
}

int procesarPeticion(){
	int quantum_sleep;
	char *pcbRecibido;
	while(!murio){
		quantum = recibirProtocolo(nucleo);
		quantum_sleep=recibirProtocolo(nucleo);
		pcbRecibido = esperarRespuesta(nucleo);
		if (quantum<=0){
			close(nucleo);
			close(umc);
				perror("Error: Error de conexion con el nucleo\n");
			return 0;
		}
		quantum = 10;
		printf("Eldel nucleo:%s\n",pcbRecibido);

		if (pcbRecibido[0] == '\0'){
			perror("Error: Error de conexion con el nucleo\n");
			return 0;}

		printf("Hardcodeado: %s\n",pcbRecibido);
		pcb = fromStringPCB(pcbRecibido);
		free(pcbRecibido);

		int terminado=procesarCodigo();
		enviarMjeFinANucleo(terminado);
	}
	return 0;
}

int procesarCodigo(){
	finalizado = 0;errorAnsisop=0;
	char* linea;
	printf("Iniciando Proceso de Codigo...\n");
	while ((quantum>0) && (!finalizado) && (!errorAnsisop) && (!bloqueado)){
		//sleep(quantum_sleep);
		linea = pedirLinea();
		printf("Recibi: %s \n", linea);
		if (linea[0] == '\0'){
			perror("Error: Error de conexion con la UMC \n");
			return -1;
		}
		parsear(linea);
		quantum--;
		pcb.pc++;
	}
	if(finalizado){
		printf("Finalizado el Proceso de Codigo\n");
		return 1;
	}
	if(errorAnsisop){
		printf("Error durante la ejecución, abortando...\n");
		return 1;
	}
	if(bloqueado){
		printf("Proceso Bloqueado\n");
		return 1;
	}
	return 0;
}

char* pedirLinea(){
	int start,
		pag,
		size_page,
		longitud,
		proceso = pcb.id;

	start = pcb.indices.instrucciones_serializado[pcb.pc].start;
	longitud = pcb.indices.instrucciones_serializado[pcb.pc].offset-1;
	pag = start / TAMANIO_PAGINA;
	int off = start%TAMANIO_PAGINA;

	size_page = longitud;

	char* respuestaFinal = string_new();
	while (longitud>0) {
		if (longitud > TAMANIO_PAGINA - off) {
			size_page = TAMANIO_PAGINA - off;
		} else {
			size_page = longitud;
		}
		enviarMensajeUMCConsulta(pag, off, size_page, proceso);
		char* respuesta=malloc(size_page+1);
		recv(umc, respuesta, size_page, 0);
		respuesta[size_page]='\0';
		string_append(&respuestaFinal, respuesta);
		respuesta='\0';
		free(respuesta);
		pag++;
		longitud -= size_page;
		off = 0;
	}
	string_append(&respuestaFinal, "\0");
	return respuestaFinal;
}


////////////////////////////////////----PARSER-------///////////////////////////////////////////////////

t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);
	Variable* var = crearVariable(variable);
	printf("Variable %c creada\n", var->id);
	sumarEnLasVariables(var);
	return  (int)var;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable) {
	int variableBuscada(Variable* var){
		return (var->id==variable);
	}
	printf("Obtener posicion de %c\n", variable);
	Stack* stack = obtenerStack();

	t_list* variables = stack->vars;
	Variable* var;
	var = (Variable*) list_find(variables,(void*)variableBuscada);
	if ( var!=NULL){
		return (int)&(var->pagina);
	}
	return -1;
}

t_valor_variable dereferenciar(t_puntero pagina) {
	Pagina* pag = (Pagina*) pagina;
	enviarMensajeUMCConsulta(pag->pag,pag->off,pag->tamanio,pcb.id);
	int valor;
	int recibidos=recv(umc,&valor,sizeof(int),0);
	if (recibidos!=sizeof(int)){
		printf("___________________Cabum me exploto la UMC __________________\n");			//todo enviar mensaje al nucleo
		errorAnsisop=1;
	}
	printf("VALOR VARIABLE: %d \n",valor);
	return valor;
}

void asignar(t_puntero pagina, t_valor_variable valor) {
	printf("Asignar\n");
	Pagina* pag = (Pagina*) pagina;
	enviarMensajeUMCAsignacion(pag->pag,pag->off,pag->tamanio,pcb.id,valor);
}

t_valor_variable obtenerValorCompartida(t_nombre_compartida	variable){
	printf("Obtener Valor Compartido de: %s", variable);
 	enviarMensajeNucleoConsulta(variable);
 	int valor;
 	recv(nucleo,&valor,sizeof(int),0);
 	valor=ntohl(valor);
 	printf("--> Valor recibido:%d\n",valor);
 	return (valor);
}

t_valor_variable asignarValorCompartida(t_nombre_compartida	variable, t_valor_variable valor){
	printf("Asignar valor compatido \n");
	enviarMensajeNucleoAsignacion(variable,valor);
	int valor_nucleo;
	recv(nucleo,&valor_nucleo,4,0);
	return valor_nucleo;
}

t_puntero_instruccion irAlLabel(t_nombre_etiqueta etiqueta){
	return metadata_buscar_etiqueta(etiqueta,pcb.indices.etiquetas,pcb.indices.etiquetas_size);
}

void llamarConRetorno(t_nombre_etiqueta	etiqueta, t_puntero	donde_retornar){
	Stack* stack = malloc(sizeof(Stack));
	stack->retPos = donde_retornar;
	stack->vars = list_create();
	Pagina pag = obtenerPagDisponible();
	Pagina* pagina = malloc(sizeof(Pagina));
	pagina = &pag;
	list_add(stack->vars,pagina);
	list_add(pcb.stack,stack);
}

void entradaSalida(t_nombre_dispositivo dispositivo,int tiempo){
	printf("Entrada/Salida\n");
	char* mensaje=string_new();
	string_append(&mensaje,"00020005");
	char* tamanio=toStringInt(string_length(dispositivo));
	string_append(&mensaje,tamanio);
	free(tamanio);
	string_append(&mensaje,dispositivo);
	char* tiempoEspera=string_itoa(tiempo);
	int longitudTiempo=string_length(tiempoEspera);
	string_append(&mensaje,toStringInt(longitudTiempo));
	string_append(&mensaje,tiempoEspera);
	free(tiempoEspera);
	string_append(&mensaje,"\0");
	send(nucleo,mensaje,string_length(mensaje),0);
	free(mensaje);
	bloqueado=1;										//turbio, si
}

void wait(t_nombre_semaforo identificador_semaforo){
	printf("Wait: %s", identificador_semaforo);
	enviarMensajeConProtocolo(nucleo,identificador_semaforo,CODIGO_WAIT);
	char respuesta[3];
	recv(nucleo,respuesta,2,0);
	respuesta[2]='\0';
	int meBloquearon=string_equals_ignore_case(respuesta,"no");
	if(meBloquearon){
		bloqueado=1;							//turbio, si
	}
}

void signalHola(t_nombre_semaforo identificador_semaforo){
	printf("Signal: %s", identificador_semaforo);
	enviarMensajeConProtocolo(nucleo,identificador_semaforo,CODIGO_SIGNAL);
}


void imprimir(t_valor_variable valor){
	printf("Imprimir %d \n", valor);
	char* mensaje = string_new();
	string_append(&mensaje,"0004");
	string_append(&mensaje,toStringInt(pcb.id));
	string_append(&mensaje,"0004");
	string_append(&mensaje,toStringInt(valor));
	printf("Mensaje al Nucleo para imprimir: %s\n",mensaje);
	send(nucleo, mensaje,strlen(mensaje),0);
	free(mensaje);
	int verificador = recibirProtocolo(nucleo);
	if (verificador != 1){
		printf("Error: Algo fallo al enviar el mensaje para imprimir texto al nucleo, recibi: %d \n", verificador);
	}
}

void imprimirTexto(char* texto) {
	printf("ImprimirTexto: %s \n", texto);
	char* mensaje = string_new();
	string_append(&mensaje,"0004");
	string_append(&mensaje,toStringInt(pcb.id));
	string_append(&mensaje,toStringInt(strlen(texto)));
	string_append(&mensaje,texto);
	printf("Mensaje al Nucleo para imprimir Texto: %s\n",mensaje);
	send(nucleo, mensaje,strlen(mensaje),0);
	free(mensaje);
	int verificador = recibirProtocolo(nucleo);
	if (verificador != 1){
		printf("Error: Algo fallo al enviar el mensaje para imprimir texto al nucleo, recibi: %d \n", verificador);
	}
}

void finalizar() {
	printf("Finalizado \n");
	int tamanioStack = list_size(pcb.stack);
	list_remove(pcb.stack,tamanioStack-1);
	if (tamanioStack == 1){
		finalizado = 1;
	}
}

//-------------------------------------FUNCIONES AUXILIARES-------------------------------------------

Variable* crearVariable(char variable){
	Variable* var = malloc(sizeof(Variable));
	var->id = variable;
	var->pagina = obtenerPagDisponible();
	return var;
}

Pagina obtenerPagDisponible(){
	Stack* stackActual = obtenerStack();
	int cantidadDeVariables = list_size(stackActual->vars);
	Pagina pagina;
	if (cantidadDeVariables<=0){
		pagina.pag = pcb.paginas_codigo;
		pagina.off = 0;
	}else{
		Variable* ultimaVariable = list_get(stackActual->vars, cantidadDeVariables-1);
		if ((ultimaVariable->pagina.off+ultimaVariable->pagina.tamanio+4)<=TAMANIO_PAGINA){
			pagina.pag = ultimaVariable->pagina.pag;
			pagina.off = ultimaVariable->pagina.off+4;
		}else{
			pagina.pag = ultimaVariable->pagina.pag+1;
			pagina.off = 0;
		}
	}
	pagina.tamanio = 4;
	return pagina;
}

void sumarEnLasVariables(Variable* var){
	Stack* stackActual = obtenerStack();
	t_list* variables = stackActual->vars;
	printf("Agregando a la lista de variables: %c \n", var->id);
	list_add(variables,var);
}

Stack* obtenerStack(){
	int tamanioStack = list_size(pcb.stack);
	if (tamanioStack <= 0){
		Stack* stack = malloc(sizeof(Stack));
		stack->vars = list_create();
		Pagina pagina;
		stack->retVar = pagina;
		list_add(pcb.stack,stack);
		tamanioStack = 1;
	}
	return (list_get(pcb.stack,tamanioStack-1));
}


void parsear(char* instruccion){
	analizadorLinea(strdup(instruccion), &functions, &kernel_functions);
}

int tienePermiso(char* autentificacion){
	return 1;
}

void saltoDeLinea(int cantidad, void* funcion){
	if (cantidad == 0){
		pcb.pc = metadata_buscar_etiqueta(funcion,pcb.indices.etiquetas,pcb.indices.etiquetas_size);
		return;
	}
	pcb.pc++;
}

void enviarMensajeUMCConsulta(int pag, int off, int size, int proceso){
	char* mensaje = string_new();
	char* procesoMje=toStringInt(proceso);
	char* pagMje=toStringInt(pag);
	char* offMje=toStringInt(off);
	char* sizeMje=toStringInt(size);
	string_append(&mensaje,"2");
	string_append(&mensaje,procesoMje);
	string_append(&mensaje,pagMje);
	string_append(&mensaje,offMje);
	string_append(&mensaje,sizeMje);
	string_append(&mensaje,"\0");
	send(umc,mensaje,string_length(mensaje),0);
	free(procesoMje);
	free(pagMje);
	free(offMje);
	free(sizeMje);
	free(mensaje);
}

void enviarMensajeUMCAsignacion(int pag, int off, int size, int proceso, int valor){
	char* mensaje = string_new();
	valor=htonl(valor);
	char* pid=toStringInt(proceso);
	char* pagina=toStringInt(pag);
	char* offset=toStringInt(off);
	char* tam=toStringInt(size);
	string_append_with_format(&mensaje,"3%s%s%s%s\0",pid,pagina,offset,tam);
	send(umc,mensaje,string_length(mensaje),0);
	send(umc,&valor,sizeof(int),0);
	free(mensaje);
	free(pid);
	free(pagina);
	free(offset);
	free(tam);
	char* resp=malloc(5);
	recv(umc,resp,4,0);
	if (atoi(resp)){
		printf("Asignacion ok\n");
	}else{
	//	errorAnsisop=1;
		printf("Cagamos\n");			//todo murió el proceso, abortar quantum y avisar al núcleo
	}
	free(resp);
}

int enviarMjeFinANucleo(int terminado){
	char* pcbEnvio;
	char* mensajeParaNucleo=string_new();
	char* codOperacion;
	if(!terminado || bloqueado){							//Terminó el quantum
		if(!terminado){						//si se bloqueo, no le tengo que mandar este codigo xq el nucleo esta esperando directamente el pcb
		codOperacion=toStringInt(1);
		string_append(&mensajeParaNucleo,codOperacion);}
		pcbEnvio=toStringPCB(pcb);
		string_append(&mensajeParaNucleo,toStringInt(string_length(pcbEnvio)));
		string_append(&mensajeParaNucleo,pcbEnvio);
		free(pcbEnvio);
	}
	else{
		if(finalizado){								//Terminó el ansisop
			codOperacion=toStringInt(3);
			string_append(&mensajeParaNucleo,codOperacion);
			pcbEnvio=toStringPCB(pcb);
			char* tamPCB=toStringInt(string_length(pcbEnvio));
			string_append_with_format(&mensajeParaNucleo,"%s%s",tamPCB,pcbEnvio);
			free(pcbEnvio);
			free(tamPCB);
		}
		else{												//error ansisop
			codOperacion=toStringInt(0);
			char* id=string_itoa(pcb.id);
			string_append(&mensajeParaNucleo,codOperacion);
			string_append(&mensajeParaNucleo,id);
			free(id);
			}
	}
	free(codOperacion);
	char* estadoCPU;
	if(murio){									//Murió la CPU
		estadoCPU=toStringInt(0);
		string_append(&mensajeParaNucleo,estadoCPU);
	}else{
		estadoCPU=toStringInt(1);
		string_append(&mensajeParaNucleo,estadoCPU);
	}
	free(estadoCPU);
	string_append(&mensajeParaNucleo,"\0");
	sleep(10);
	send(nucleo,mensajeParaNucleo,string_length(mensajeParaNucleo),0);
	terminado=0;
	bloqueado=0;
	finalizado=0;
	errorAnsisop=0;
	return 1;
}


void enviarMensajeNucleoConsulta(char* variable){
	char* mensaje = string_new();
	string_append(&mensaje, "00020001");
	char* tamVariable=toStringInt(strlen(variable)+1);
	string_append(&mensaje, tamVariable);
	free(tamVariable);
	string_append(&mensaje, "!");
	string_append(&mensaje, variable);
	string_append(&mensaje, "\0");
	send(nucleo, mensaje,strlen(mensaje),0);
	free(mensaje);
}

void enviarMensajeNucleoAsignacion(char* variable, int valor){
	char* mje=string_new();
	string_append(&mje,"00020002");
	string_append(&mje,toStringInt(string_length(variable)+1));
	string_append(&mje,"!");
	string_append(&mje,variable);
	char* numeroChar=string_new();
	string_append(&numeroChar,string_itoa(valor));
	string_append(&numeroChar,"\0");
	string_append(&mje,toStringInt(string_length(numeroChar)));
	string_append(&mje,numeroChar);
	string_append(&mje,"\0");
	free(numeroChar);
	send(nucleo,mje,string_length(mje),0);
	free(mje);
}
