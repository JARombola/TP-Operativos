#include "CPUBeta.h"
#include <pthread.h>
#include <commons/process.h>
#include <signal.h>

void crearHiloSignal();
void hiloSignal();
void funcionSenial(int n);

int ejecutar=1;

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
		quantum=0;
		return;
		break;

	case SIGINT:
		printf("Por que me cerraste? \n");
		sleep(2);
		printf("Mentira, bye\n");
		send(umc,"0",1,0);
	//	send(nucleo,error,4,0);
		exit(0);
		break;
	}
}


int levantarArchivoDeConfiguracion(){
	FILE* archivoDeConfiguracion = fopen("/home/utnso/tp-2016-1c-CodeBreakers/CPU/ArchivoDeConfiguracionCPU.txt","r");
	if (archivoDeConfiguracion==NULL){
		FILE* archivoDeConfiguracion = fopen("/home/utnso/tp-2016-1c-CodeBreakers/CPU/ArchivoDeConfiguracionCPU.txt","r");
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
	char* pcb_char;

	quantum = recibirProtocolo(nucleo);
	quantum_sleep=recibirProtocolo(nucleo);
	pcb_char = esperarRespuesta(nucleo);

	if (quantum<=0){
		close(nucleo);
		close(umc);
			perror("Error: Error de conexion con el nucleo\n");
		return 0;
	}
	quantum = 10;
	printf("Eldel nucleo:%s\n",pcb_char);
	//strcpy(pcb_char, "000600680000000600000000000000150006000400210004002500070029000400360004004000040000");

	if (pcb_char[0] == '\0'){
		perror("Error: Error de conexion con el nucleo\n");
		return 0;}

	printf("Hardcodeado: %s\n",pcb_char);
	pcb = fromStringPCB(pcb_char);

	procesarCodigo();
	free(pcb_char);
	return 0;
}

int procesarCodigo(){
	finalizado = 0;
	char* linea;
	printf("Iniciando Proceso de Codigo...\n");
	while ((quantum>0) && (!finalizado)){
		//sleep(3);
		linea = pedirLinea();
		printf("Recibi: %s \n", linea);
		if (linea[0] == '\0'){
			perror("Error: Error de conexion con la UMC \n");
			return -1;
		}
		parsear(linea);
		quantum--;
//		saltoDeLinea(1,NULL);
		pcb.pc++;
	}
	Stack* s = obtenerStack();
	t_list* vars = s->vars;
	Variable* a = list_get(vars,0);
	Variable* b = list_get(vars,1);
	printf("%c\n",a->id);
	printf("%c\n",b->id);
	printf("Finalizado el Proceso de Codigo...\n");
	return 0;
}

char* pedirLinea(){
	int start,
		pag,
		size_page,
		longitud,
		proceso = pcb.id;

	start = pcb.indices.instrucciones_serializado[pcb.pc].start-4;
	longitud = pcb.indices.instrucciones_serializado[pcb.pc].offset-1;//-1 para evitar el \n
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
		printf("Le pedi pag: %d, off: %d y size: %d y me respondio : %s \n", pag,off,size_page,respuesta);
		respuesta='\0';
		free(respuesta);
		pag++;
		longitud -= size_page;
		off = 0;
	}
	string_append(&respuestaFinal, "\0");
	return respuestaFinal;


	/*char* linea = string_new();
	switch (quantum){
	case 10: string_append(&linea,"variables a,b"); break;
	case 9: string_append(&linea,"a=3"); break;
	case 8: string_append(&linea,"b=5"); break;
	case 7: string_append(&linea,"a=12+b"); break;
	case 6: string_append(&linea,"end"); break;
	}
	return linea;*/

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
		return (int)&(var->pagina);				// POR QUE HAY UN "&" AHI!?!?!?!?  porq retorna un puntero  todo
	}
	return -1;
}

t_valor_variable dereferenciar(t_puntero pagina) {
	Pagina* pag = (Pagina*) pagina;
	enviarMensajeUMCConsulta(pag->pag-1,pag->off,pag->tamanio,pcb.id);			//1 = obtener valor, 0 = obtener linea
	int *p;
	recv(umc,&p,sizeof(int),0);
	printf("VALOR VARIABLE: %d \n",p);
	//if (!recibirProtocolo(umc)) printf("Cabum me exploto la UMC \n");
	//char* respuesta=esperarRespuesta(umc);
	//int resp=atoi(respuesta);
	//free(respuesta);
	return p;
}

void asignar(t_puntero pagina, t_valor_variable valor) {
	Pagina* pag = (Pagina*) pagina;
	enviarMensajeUMCAsignacion(pag->pag,pag->off,pag->tamanio,pcb.id,valor);
}

t_valor_variable obtenerValorCompartida(t_nombre_compartida	variable){
	enviarMensajeNucleoConsulta(variable);
	return atoi(esperarRespuesta(nucleo));
}

t_valor_variable asignarValorCompartida(t_nombre_compartida	variable, t_valor_variable valor){
	enviarMensajeNucleoAsignacion(variable,valor);
	return atoi(esperarRespuesta(nucleo));
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

void wait(t_nombre_semaforo identificador_semaforo){
	printf("Wait: %s", identificador_semaforo);
	enviarMensajeConProtocolo(nucleo,identificador_semaforo,CODIGO_WAIT);
}

void signalHola(t_nombre_semaforo identificador_semaforo){
	printf("Signal: %s", identificador_semaforo);
	enviarMensajeConProtocolo(nucleo,identificador_semaforo,CODIGO_SIGNAL);
}


void imprimir(t_valor_variable valor){
	printf("Imprimir %d \n", valor);
	char* valor_char = header(valor);
	char* protocolo = header(CODIGO_IMPRESION);
	char mensaje[10];
	sprintf(mensaje,"%s%s", protocolo, valor_char);
	send(nucleo, mensaje,strlen(mensaje),0);
}

void imprimirTexto(char* texto) {
	printf("ImprimirTexto: %s \n", texto);
	enviarMensajeConProtocolo(nucleo,texto, CODIGO_IMPRESION);
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
		pagina.pag = pcb.paginas_codigo+1;
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
	string_append_with_format(&mensaje,"3%s%s%s%s\0",toStringInt(proceso),toStringInt(pag-1),toStringInt(off),toStringInt(size));
	send(umc,mensaje,string_length(mensaje),0);
	send(umc,&valor,sizeof(int),0);
	char* resp=malloc(5);
	recv(umc,resp,4,0);
	if (atoi(resp)){
		printf("Asignacion ok\n");
	}else{
		printf("Cagamos\n");
	}
	free(mensaje);
}

void enviarMensajeNucleoConsulta(char* variable){

}
void enviarMensajeNucleoAsignacion(char* variable, int valor){

}


