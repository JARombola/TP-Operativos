#include "CPUBeta.h"
#include <pthread.h>
#include <commons/process.h>
#include <signal.h>

int main(){
	printf("CPU estable...[%d] \n",process_getpid());

	if(levantarArchivoDeConfiguracion()<0) return -1;

	crearHiloSignal();

	conectarseAlNucleo();
	if (nucleo < 0) return -1;

	conectarseALaUMC();
	if (umc < 0) return -1;

	procesarPeticion();

	printf("Cerrando CPU.. \n");

	return 0;
}

void crearHiloSignal(){
	pthread_t th_seniales;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	pthread_create(&th_seniales, NULL, (void*)hiloSignal, NULL);
}

void hiloSignal(){
		signal(SIGUSR1, cerrarCPU);					//(SIGUSR1) ---------> Kill -10 (ProcessPid)
		signal(SIGINT,cerrarCPU);					//(SIGINT) ---------> Ctrl+C
}

void cerrarCPU(int senial){
		switch(senial){
			case SIGUSR1:
				printf("Rayos Me mataron con SIGUSR1\n");
				status = 1;
				break;
			case SIGINT:
				printf("Adios Mundo Cruel\n");
				close(nucleo);
				close(umc);
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
	char *pcbRecibido;
	int quantum = 0;
	int quantum_sleep = 0;

	while(!finalizado){

		quantum = recibirProtocolo(nucleo);
		printf("Peticion del Nucleo\n\n");
		quantum_sleep=recibirProtocolo(nucleo);
		pcbRecibido = esperarRespuesta(nucleo);

		if (quantum<=0){
			close(nucleo);
			close(umc);
				perror("Error: Error de conexion con el nucleo\n");
			return 0;
		}

		printf("Quantum recibido: %d\n",quantum);
		quantum = 100;printf("Quantum hardcodeado: %d\n",quantum);

		printf("Quantum Sleep recibido: %d\n",quantum_sleep);
		quantum_sleep = 0;printf("Quantum Sleep recibido: %d\n",quantum_sleep);

		if (pcbRecibido[0] == '\0'){
			perror("Error: Error de conexion con el nucleo\n");
			return 0;
		}

		pcb = fromStringPCB(pcbRecibido);
		free(pcbRecibido);

		procesarCodigo(quantum, quantum_sleep);
	}
	return 0;
}

void procesarCodigo(int quantum, int quantum_sleep){

	printf("Iniciando Proceso de Codigo...\n");
	char* linea;
	while (!finalizado){
		printf("Ronda: %d\n\n\n", pcb.pc);
		linea = pedirLinea();
		printf("Recibi: %s \n", linea);

		if (!finalizado){
			parsear(linea);
			printf("\nFin del parser\n");
			free(linea);
			quantum--;
			pcb.pc++;
			sleep(quantum_sleep);
		}

		if ((!finalizado) && (!quantum)){;
			finalizado = 4;
			char* mensaje = string_new();
			string_append(&mensaje,"0001");
			char* pcb_char = toStringPCB(pcb);
			string_append(&mensaje,toStringInt(strlen(pcb_char)));
			string_append(&mensaje,pcb_char);
			free(pcb_char);
			string_append(&mensaje,toStringInt(status));
			string_append(&mensaje,"\0");
			send(nucleo,mensaje,strlen(mensaje),0);
			free(mensaje);
			printf("Fin de Quantum \n");
		}
	}
	if (finalizado<0){
		switch(finalizado){
		case -1:
			close(nucleo);
			close(umc);
			break;
		case -2:
			close(umc);
			close(nucleo);
			break;
		case -9:
			send(nucleo,"0000",4,0);
			finalizado = 0;
			break;
		}
	}else{
		finalizado = 0;
	}
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
		int verificador = recv(umc, respuesta, size_page, 0);
		if (verificador <= 0){
			printf("Error : Fallor la conexion con la UMC\n");
			finalizado = -1;
			break;
		}
		respuesta[size_page]='\0';

		string_append(&respuestaFinal, respuesta);
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
	printf("Pag: %d Off: %d size: %d\n",var->pagina.pag,var->pagina.off,var->pagina.tamanio);
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
	printf("Error: No se encontro la variable\n");
	finalizado = -9; // Error turbio
	return -1;
}

t_valor_variable dereferenciar(t_puntero pagina) {
	Pagina* pag = (Pagina*) pagina;
	enviarMensajeUMCConsulta(pag->pag,pag->off,pag->tamanio,pcb.id);
	int valor;
	int recibidos=recv(umc,&valor,sizeof(int),0);
	if (recibidos<= 0){
		printf("Error: Fallo la conexion con la UMC\n");
		finalizado = -1;
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
 	int verificador = recv(nucleo,&valor,sizeof(int),0);

 	if (verificador <= 0){
 		finalizado = -2;
 		printf("Error: Fallo la conexion con el Nucleo\n");
 		return -1;
 	}

 	valor=ntohl(valor);
 	printf("--> Valor recibido:%d\n",valor);

 	return (valor);
}

t_valor_variable asignarValorCompartida(t_nombre_compartida	variable, t_valor_variable valor){
	printf("Asignar valor compatido \n");
	enviarMensajeNucleoAsignacion(variable,valor);
	int valor_nucleo;

 	int verificador = recv(nucleo,&valor,sizeof(int),0);

 	if (verificador <= 0){
 		finalizado = -2;
 		printf("Error: Fallo la conexion con el Nucleo\n");
 		return -1;
 	}

	return ntohl(valor_nucleo);
}

t_puntero_instruccion irAlLabel(t_nombre_etiqueta etiqueta){
	printf("Ir a Label: %s \n", etiqueta);
	return metadata_buscar_etiqueta(etiqueta,pcb.indices.etiquetas,pcb.indices.etiquetas_size);
}

void llamarConRetorno(t_nombre_etiqueta	etiqueta, t_puntero	donde_retornar){
	printf("Llamada con retorno a : %s \n", etiqueta);

	Stack* stack = malloc(sizeof(Stack));
	Pagina* paginaReturn = (Pagina*) donde_retornar;
	stack->retVar = *paginaReturn;
	stack->retPos = pcb.pc;
	stack->vars = list_create();
	Pagina pag = obtenerPagDisponible();
	Pagina* pagina = malloc(sizeof(Pagina));
	pagina = &pag;
	pcb.pc = metadata_buscar_etiqueta(etiqueta,pcb.indices.etiquetas,pcb.indices.etiquetas_size);
	printf("Salto a: %d\n", pcb.pc);

	list_add(stack->vars,pagina);
	list_add(pcb.stack,stack);
}

void entradaSalida(t_nombre_dispositivo dispositivo,int tiempo){
	printf("Entrada/Salida: %s tiempo: %d\n", dispositivo, tiempo);

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
	char* pcb_char = toStringPCB(pcb);
	string_append(&mensaje,toStringInt(strlen(pcb_char)));
	string_append(&mensaje,pcb_char);
	free(pcb_char);
	string_append(&mensaje,toStringInt(status));
	string_append(&mensaje,"\0");
	send(nucleo,mensaje,string_length(mensaje),0);
	free(mensaje);

	finalizado = 2;
}

void wait(t_nombre_semaforo identificador_semaforo){
	printf("Wait: %s", identificador_semaforo);
	char* mensaje = string_new();
	string_append(&mensaje,"00020003");
	string_append(&mensaje,toStringInt(strlen(identificador_semaforo)));
	string_append(&mensaje,identificador_semaforo);
	printf("Le mando el mensaje al nucleo: %s \n", mensaje);
	send(nucleo, mensaje,strlen(mensaje),0);
	free(mensaje);
	char respuesta[3];
 	int verificador = recv(nucleo,respuesta,2,0);

 	if (verificador <= 0){
 		finalizado = -2;
 		printf("Error: Fallo la conexion con el Nucleo\n");
 		return;
 	}
	respuesta[2]= '\0';

	if(respuesta[0]=='o'){
		printf("Wait ok sin problemas\n");
	}else if(respuesta[0]=='n'){
		printf("Semaforo bloqueante\n");
		finalizado = 3;
	}else printf("Error: Respuesta de el nucleo: %s\n",respuesta);
}

void post(t_nombre_semaforo identificador_semaforo){
	printf("Signal: %s\n", identificador_semaforo);
	char* mensaje = string_new();
	string_append(&mensaje,"00020004");
	string_append(&mensaje,toStringInt(strlen(identificador_semaforo)));
	string_append(&mensaje,identificador_semaforo);
	printf("Le mando el mensaje al nucleo: %s \n", mensaje);
	send(nucleo, mensaje,strlen(mensaje),0);
	free(mensaje);
	int verificador = recibirProtocolo(nucleo);
	printf("Recibi del nucleo %d\n", verificador);
	if (verificador != 1){
		finalizado = -2;
		printf("Error: Erro de conexion con el Nucleo \n");
		printf("Error: Algo fallo al enviar el mensaje para realizar un signal, recibi: %d \n", verificador);
	}
}


void imprimir(t_valor_variable valor){
	printf("Imprimir %d \n", valor);
	char* mensaje = string_new();
	string_append(&mensaje,"0004");
	string_append(&mensaje,toStringInt(pcb.id));
	char* valorOk=string_itoa(valor);
	char* tamanioValor=toStringInt(string_length(valorOk));
	string_append(&mensaje,tamanioValor);
	string_append(&mensaje,valorOk);
	free(valorOk);
	send(nucleo, mensaje,strlen(mensaje),0);
	free(mensaje);
	int verificador = recibirProtocolo(nucleo);
	if (verificador != 1){
		finalizado = -2;
		printf("Error: Erro de conexion con el Nucleo \n");
		printf("Error: Algo fallo al enviar el mensaje para imprimir texto al nucleo, recibi: %d \n", verificador);
	}
	free(tamanioValor);
}

void imprimirTexto(char* texto) {
	printf("ImprimirTexto: %s \n", texto);
	char* mensaje = string_new();
	string_append(&mensaje,"0004");
	string_append(&mensaje,toStringInt(pcb.id));
	string_append(&mensaje,toStringInt(strlen(texto)));
	string_append(&mensaje,texto);
	send(nucleo, mensaje,strlen(mensaje),0);
	free(mensaje);
	int verificador = recibirProtocolo(nucleo);
	if (verificador != 1){
		finalizado = -2;
		printf("Error: Erro de conexion con el Nucleo \n");
		printf("Error: Algo fallo al enviar el mensaje para imprimir texto al nucleo, recibi: %d \n", verificador);
	}
}

void finalizar() {
	printf("Finalizado \n");
	int tamanioStack = list_size(pcb.stack);
	printf("remueve: %d \n", tamanioStack-1);

	if (tamanioStack >1){
		Stack* stackActual = obtenerStack();
		pcb.pc = stackActual->retPos;
	}
	list_remove(pcb.stack,tamanioStack-1);
	if (tamanioStack == 1){
		char* mensaje = string_new();
		string_append(&mensaje,"0003");
		char* pcb_char = toStringPCB(pcb);
		string_append(&mensaje,toStringInt(strlen(pcb_char)));
		string_append(&mensaje,pcb_char);
		free(pcb_char);
		string_append(&mensaje,toStringInt(status));
		string_append(&mensaje,"\0");
		send(nucleo,mensaje,strlen(mensaje),0);
		free(mensaje);
		finalizado = 1;
	}
}

void retornar(t_valor_variable retorno){
	Stack* stackActual = obtenerStack();
	int puntero = (int)&(stackActual->retVar);
	asignar(puntero,retorno);
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

void saltoDeLinea(t_nombre_etiqueta t_nombre_etiqueta){
	char* nombre=string_substring_from(t_nombre_etiqueta,1);
	pcb.pc = metadata_buscar_etiqueta(nombre,pcb.indices.etiquetas,pcb.indices.etiquetas_size);
	pcb.pc--;
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
	send(umc,&valor,sizeof(int),0);  // AK HAY ALGO TURBIO
	free(mensaje);
	free(pid);
	free(pagina);
	free(offset);
	free(tam);
	char* resp=malloc(5);
	int verificador = recv(umc,resp,4,0);
	if (verificador <= 0){
		printf("Error: Fallo la conexion con la UMC\n");
		finalizado = -1;
	}
	free(resp);
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
