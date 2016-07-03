#include "CPUBeta.h"
#include <pthread.h>
#include <commons/process.h>
#include <signal.h>

int main(){

    archivoLog = log_create("CPU.log", "CPU", true, log_level_from_string("INFO"));

    log_info(archivoLog,"CPU estable...[%d] \n",process_getpid());

	if(levantarArchivoDeConfiguracion()<0) return -1;

	crearHiloSignal();

	conectarseAlNucleo();
	if (nucleo < 0) return -1;

	conectarseALaUMC();
	if (umc < 0) return -1;

	procesarPeticion();

	log_info(archivoLog,"Cerrando CPU.. \n");

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
		char* mensaje;
		switch(senial){
			case SIGUSR1:
				log_info(archivoLog,"Rayos Me mataron con SIGUSR1\n");
				status = 0;
				return;
			case SIGINT:
				mensaje = string_new();
				string_append(&mensaje,"0000");
				string_append(&mensaje,toStringInt(pcb.id));
				string_append(&mensaje,"0000");
				send(nucleo,mensaje,strlen(mensaje),0);
				close(nucleo);
				close(umc);
				exit(0);
				break;
		}
}


int levantarArchivoDeConfiguracion(){
	FILE* archivoDeConfiguracion = fopen("../ArchivoDeConfiguracionCPU.txt","r");
	if (archivoDeConfiguracion==NULL){
		archivoDeConfiguracion = fopen(ARCHIVO_DE_CONFIGURACION,"r");
		if (archivoDeConfiguracion==NULL){
		log_error(archivoLog,"Error: No se pudo abrir el archivo de configuracion, verifique su existencia en la ruta: %s \n", ARCHIVO_DE_CONFIGURACION);
		return -1;}
	}
	char* archivoJson =toJsonArchivo(archivoDeConfiguracion);
	char puertoDelNucleo [6];
	buscar(archivoJson,"PUERTO_NUCLEO", puertoDelNucleo);
	PUERTO_NUCLEO = atoi(puertoDelNucleo);
	if (PUERTO_NUCLEO == 0){
		log_error(archivoLog,"Error: No se ha encontrado el Puerto del Nucleo en el archivo de Configuracion \n");
		return -1;
	}

	buscar(archivoJson,"AUTENTIFICACION", AUTENTIFICACION);
	if (AUTENTIFICACION[0] =='\0'){
		log_error(archivoLog,"Error: No se ha encontrado la Autentificacion en el archivo de Configuracion \n");
		return -1;
	}
	buscar(archivoJson,"IP_NUCLEO", IP_NUCLEO);
	if (IP_NUCLEO[0] == '\0'){
		log_error(archivoLog,"Error: No se ha encontrado la IP del Nucleo en el archivo de Configuracion \n");
		return -1;
	}
	buscar(archivoJson,"IP_UMC", IP_UMC);
	if (IP_UMC[0] =='\0'){
		log_error(archivoLog,"Error: No se ha encontrado la IP de la UMC en el archivo de Configuracion \n");
		return -1;
	}

	char puertoDeLaUMC[6];
	buscar(archivoJson,"PUERTO_UMC", puertoDeLaUMC);
	PUERTO_UMC = atoi(puertoDeLaUMC);
	if (PUERTO_UMC == 0){
		log_error(archivoLog,"Error: No se ha encontrado el Puerto de la UMC en el archivo de Configuracion \n");
		return -1;
	}

	return 0;
}


void conectarseAlNucleo(){
	nucleo = conectar(PUERTO_NUCLEO,IP_NUCLEO);
	if (nucleo<0){
		log_error(archivoLog,"Error al conectarse con el nucelo \n");
		return;
	}
	autentificar(nucleo,AUTENTIFICACION);
	log_info(archivoLog,"Conexion con el nucleo OK... \n");
}

void conectarseALaUMC(){
	umc = conectar(PUERTO_UMC,IP_UMC);
	if (umc<0){
		log_error(archivoLog,"Error: No se ha logrado establecer la conexion con la UMC\n");
	}
	TAMANIO_PAGINA = autentificar(umc,AUTENTIFICACION);
	if (!TAMANIO_PAGINA){
		log_error(archivoLog,"Error: No se ha logrado establecer la conexion con la UMC\n");
	}
	log_info(archivoLog,"Conexion con la UMC OK...\n");
	log_info(archivoLog,"Tamanio de la Pagina : %d", TAMANIO_PAGINA);
}

int procesarPeticion(){
	char *pcbRecibido;
	int quantum = 0;
	int quantum_sleep = 0;
	while ((!finalizado) && (status)){

		log_info(archivoLog,"\n\nPeticion del Nucleo\n\n");

		quantum = recibirProtocolo(nucleo);
		quantum_sleep=recibirProtocolo(nucleo);
		pcbRecibido = esperarRespuesta(nucleo);

		if ((quantum<=0)||(quantum_sleep<=0)||pcbRecibido[0]=='\0'){
			close(nucleo);
			close(umc);
			log_error(archivoLog,"Error: Error de conexion con el nucleo\n");
			return 0;
		}

		log_info(archivoLog,"Quantum recibido: %d\n",quantum);

		log_info(archivoLog,"Quantum Sleep recibido: %d\n",quantum_sleep);

		if (pcbRecibido[0] == '\0'){
			log_error(archivoLog,"Error: Error de conexion con el nucleo\n");
			return 0;
		}
		log_info(archivoLog,"\n Recibi del Nucleo: %s\n", pcbRecibido);
		pcb = fromStringPCB(pcbRecibido);
		free(pcbRecibido);

		log_info(archivoLog,"\n Ejecutar Proceso: %d\n", pcb.id);
		procesarCodigo(quantum, quantum_sleep);

		log_info(archivoLog,"Fin del Proceso %d %d\n", finalizado, status);
	}
	return 0;
}

void procesarCodigo(int quantum, int quantum_sleep){

	log_info(archivoLog,"Iniciando Proceso de Codigo...\n");
	char* linea;
	while (!finalizado){
		log_info(archivoLog,"Ronda: %d\n\n\n", pcb.pc);
		linea = pedirLinea();
		if (linea == NULL){
			finalizado = 0;
			return;
		}
		log_info(archivoLog,"Recibi: %s \n", linea);

		if (!finalizado){
			parsear(linea);
			log_info(archivoLog,"\nFin del parser\n");
			free(linea);
			quantum--;
			pcb.pc++;
			usleep(quantum_sleep*1000);
		}

		if ((!finalizado) && (!quantum)){
			log_info(archivoLog,"Fin de Quantum \n");
			finalizado = 4;
			char* mensaje = string_new();
			string_append(&mensaje,"0001");
			char* pcb_char = toStringPCB(pcb);
			liberarPCB(pcb);
			printf("Liberado \n");
			string_append(&mensaje,toStringInt(strlen(pcb_char)));
			string_append(&mensaje,pcb_char);
			string_append(&mensaje,toStringInt(status));
			string_append(&mensaje,"\0");
			printf("Pre envio\n");
			send(nucleo,mensaje,strlen(mensaje),0);
			log_info(archivoLog,"\n\nLe mande al nucleo el PCB: %s \n\n", pcb_char);
			log_info(archivoLog,"\n\nLe mande al nucleo el PCB: %s\n\n", toStringPCB(fromStringPCB(pcb_char)));
			free(pcb_char);
			free(mensaje);
			log_info(archivoLog,"Ansisop Enviado a Nucleo \n");
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
		char* respuesta=malloc((size_page+1)*sizeof(char));
		char* estado = malloc(3*sizeof(char));
		recv(umc,estado,2,MSG_WAITALL);
		if (estado[0]!='o'){
			estado[2] = '\0';
			log_warning(archivoLog,"Respuesta UMC: %s\n", estado);
			free(respuesta);
			free(estado);
			log_warning(archivoLog,"La UMC rechazo el pedido, Eliminar Ansisop\n");
			char *mensaje = string_new();
			string_append(&mensaje,"0000");
			string_append(&mensaje,toStringInt(pcb.id));
			string_append(&mensaje,"0001");
			send(nucleo,mensaje,strlen(mensaje),0);
			free(mensaje);
			return NULL;
		}
		int verificador = recv(umc, respuesta, size_page, MSG_WAITALL);
		if (verificador <= 0){
			log_error(archivoLog,"Error : Fallor la conexion con la UMC\n");
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
	log_info(archivoLog,"definir la variable %c\n", variable);
	Variable* var = crearVariable(variable);
	log_info(archivoLog,"Variable %c creada\n", var->id);
	if ((variable>='0') && (variable <='9')){
		int tamanioStack = list_size(pcb.stack);
		Stack* stackActual = list_get(pcb.stack,tamanioStack-1);
		if (stackActual->args == NULL){
			stackActual->args = list_create();
		}
		list_add(stackActual->args,var);
	}else{
		sumarEnLasVariables(var);
	}
	log_info(archivoLog,"Pag: %d Off: %d size: %d\n",var->pagina.pag,var->pagina.off,var->pagina.tamanio);
	log_info(archivoLog,"retorno : %d", (int)&var->pagina);
	return  (int)&var->pagina;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable) {
	int variableBuscada(Variable* var){
		return (var->id==variable);
	}
	Variable* var;
	log_info(archivoLog,"Obtener posicion de %c\n", variable);
	if ((variable>='0') && (variable <='9')){
		int tamanioStack = list_size(pcb.stack);
		Stack* stackActual = list_get(pcb.stack,tamanioStack-1);
		int i;
		for(i=0; i<list_size(stackActual->args);i++){
			var = list_get(stackActual->args,i);
			if (variable == var->id){
				log_info(archivoLog,"La posicion de %c es: %d %d %d\n", variable, var->pagina.pag, var->pagina.off, var->pagina.tamanio);
				log_info(archivoLog,"retorno : %d\n", (int)&var->pagina);
				return (int)&var->pagina;
			}
		}
	}else{
		Stack* stack = obtenerStack();
		t_list* variables = stack->vars;
		var = (Variable*) list_find(variables,(void*)variableBuscada);
		if ( var!=NULL){
			log_info(archivoLog,"La posicion de %c es: %d %d %d\n", variable, var->pagina.pag, var->pagina.off, var->pagina.tamanio);
			log_info(archivoLog,"retorno : %d\n", (int)&var->pagina);
			return (int)&(var->pagina);
		}
	}
	log_error(archivoLog,"Error: No se encontro la variable\n");
	finalizado = -9; // Error turbio
	return -1;
}

t_valor_variable dereferenciar(t_puntero pagina) {
	log_info(archivoLog,"Me llega : %d", (int) pagina);
	Pagina*  pag = (Pagina*) pagina;
	log_info(archivoLog,"Dereferenciar %d %d %d",pag->pag,pag->off,pag->tamanio);
	enviarMensajeUMCConsulta(pag->pag,pag->off,pag->tamanio,pcb.id);
	char resp[2];
	recv(umc,resp,2,MSG_WAITALL);
	if(resp[0]=='o'){
		int valor;
		int recibidos=recv(umc,&valor,sizeof(int),MSG_WAITALL);
		if (recibidos<= 0){
			log_error(archivoLog,"Error: Fallo la conexion con la UMC\n");
			finalizado = -1;}
		log_info(archivoLog,"VALOR VARIABLE: %d \n",valor);
		return valor;
	}
	log_warning(archivoLog,"La UMC rechazo el pedido, Eliminar Ansisop\n");
	log_warning(archivoLog, "Sobrepase el limite del stack");
	char* mensaje = string_new();
	string_append(&mensaje,"0000");
	char * char_id_pcb = toStringInt(pcb.id);
	string_append(&mensaje,char_id_pcb);
	send(nucleo,mensaje,strlen(mensaje),0);
	return -1;
}

void asignar(t_puntero pagina, t_valor_variable valor) {
	Pagina* pag = (Pagina*) pagina;
	log_info(archivoLog,"Asignar %d -> %d %d %d\n",valor,pag->pag,pag->off,pag->tamanio);
	enviarMensajeUMCAsignacion(pag->pag,pag->off,pag->tamanio,pcb.id,valor);
}

t_valor_variable obtenerValorCompartida(t_nombre_compartida	variable){
	log_info(archivoLog,"Obtener Valor Compartido de: %s\n", variable);
 	enviarMensajeNucleoConsulta(variable);
 	int valor;
 	int verificador = recv(nucleo,&valor,sizeof(int),MSG_WAITALL);

 	if (verificador <= 0){
 		finalizado = -2;
 		log_error(archivoLog,"Error: Fallo la conexion con el Nucleo\n");
 		return -1;
 	}

 	valor=ntohl(valor);
 	log_info(archivoLog,"--> Valor recibido:%d\n",valor);

 	return (valor);
}

t_valor_variable asignarValorCompartida(t_nombre_compartida	variable, t_valor_variable valor){
	log_info(archivoLog,"Asignar valor compatido \n");
	enviarMensajeNucleoAsignacion(variable,valor);
	int valor_nucleo;

 	int verificador = recv(nucleo,&valor_nucleo,sizeof(int),MSG_WAITALL);
 	log_info(archivoLog,"Valor de %s : %d",variable, verificador);
 	if (verificador <= 0){
 		finalizado = -2;
 		log_error(archivoLog,"Error: Fallo la conexion con el Nucleo\n");
 		return -1;
 	}
	return ntohl(valor_nucleo);
}

void irAlLabel(t_nombre_etiqueta etiqueta){
	log_info(archivoLog,"Ir a Label: %s \n", etiqueta);
	pcb.pc =  metadata_buscar_etiqueta(etiqueta,pcb.indices.etiquetas,pcb.indices.etiquetas_size);
	log_info(archivoLog,"Salto a: %d", pcb.pc);
	pcb.pc--;
}

void llamarConRetorno(t_nombre_etiqueta	etiqueta, t_puntero	donde_retornar){
	log_info(archivoLog,"Llamada con retorno a : %s \n", etiqueta);

	Stack* stack = malloc(sizeof(Stack));
	Pagina* paginaReturn = (Pagina*) donde_retornar;
	stack->retVar = *paginaReturn;
	stack->retPos = pcb.pc;
	stack->vars = list_create();
	stack->args = list_create();


	pcb.pc = metadata_buscar_etiqueta(etiqueta,pcb.indices.etiquetas,pcb.indices.etiquetas_size);
	log_info(archivoLog,"Salto a: %d\n", pcb.pc);
	pcb.pc--;

	list_add(pcb.stack,stack);
}

void entradaSalida(t_nombre_dispositivo dispositivo,int tiempo){
	log_info(archivoLog,"Entrada/Salida: %s tiempo: %d\n", dispositivo, tiempo);

	char* mensaje=string_new();
	string_append(&mensaje,"00020005");
	pcb.pc++;
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
	liberarPCB(pcb);
	string_append(&mensaje,toStringInt(strlen(pcb_char)));
	string_append(&mensaje,pcb_char);
	string_append(&mensaje,toStringInt(status));
	string_append(&mensaje,"\0");
	send(nucleo,mensaje,string_length(mensaje),0);
	log_info(archivoLog,"\n\nLe mande al nucleo el PCB: %s\n\n", pcb_char);
	log_info(archivoLog,"\n\nLe mande al nucleo el PCB: %s\n\n", toStringPCB(fromStringPCB(pcb_char)));
	free(pcb_char);
	free(mensaje);

	finalizado = 2;
}

void wait(t_nombre_semaforo identificador_semaforo){
	log_info(archivoLog,"Wait: %s", identificador_semaforo);
	char* mensaje = string_new();
	string_append(&mensaje,"00020003");
	string_append(&mensaje,toStringInt(strlen(identificador_semaforo)));
	string_append(&mensaje,identificador_semaforo);
	log_info(archivoLog,"Le mando el mensaje al nucleo: %s \n", mensaje);
	send(nucleo, mensaje,strlen(mensaje),0);
	free(mensaje);
	char respuesta[3];
 	int verificador = recv(nucleo,respuesta,2,MSG_WAITALL);

 	if (verificador <= 0){
 		finalizado = -2;
 		log_error(archivoLog,"Error: Fallo la conexion con el Nucleo\n");
 		 return;
 	}
	respuesta[2]= '\0';

	if(strcmp(respuesta,"ok")==0){
		log_info(archivoLog,"Wait ok sin problemas\n");
	}else if(strcmp(respuesta,"no")==0){
		log_info(archivoLog,"Semaforo bloqueante\n");
		pcb.pc++;
		char* mensaje = string_new();
		char* char_pcb = toStringPCB(pcb);
		liberarPCB(pcb);
		string_append(&mensaje,toStringInt(strlen(char_pcb)));
		string_append(&mensaje,char_pcb);
		string_append(&mensaje,toStringInt(status));
		send(nucleo,mensaje,strlen(mensaje),0);
		log_info(archivoLog,"\n\nLe mande al nucleo el PCB: %s \n\n", char_pcb);
		free(mensaje);
		free(char_pcb);
		finalizado = 3;
	}else log_error(archivoLog,"Error: Respuesta de el nucleo: %s\n",respuesta);
}

void post(t_nombre_semaforo identificador_semaforo){
	log_info(archivoLog,"Signal: %s\n", identificador_semaforo);
	char* mensaje = string_new();
	string_append(&mensaje,"00020004");
	string_append(&mensaje,toStringInt(strlen(identificador_semaforo)));
	string_append(&mensaje,identificador_semaforo);
	log_info(archivoLog,"Le mando el mensaje al nucleo: %s \n", mensaje);
	send(nucleo, mensaje,strlen(mensaje),0);
	free(mensaje);
	int verificador = recibirProtocolo(nucleo);
	log_info(archivoLog,"Recibi del nucleo %d\n", verificador);
	if (verificador != 1){
		finalizado = -2;
		log_error(archivoLog,"Error: Erro de conexion con el Nucleo \n");
		log_error(archivoLog,"Error: Algo fallo al enviar el mensaje para realizar un signal, recibi: %d \n", verificador);
	}
}


void imprimir(t_valor_variable valor){
	log_info(archivoLog,"Imprimir %d \n", valor);
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
		log_error(archivoLog,"Error: Erro de conexion con el Nucleo \n");
		log_error(archivoLog,"Error: Algo fallo al enviar el mensaje para imprimir texto al nucleo, recibi: %d \n", verificador);
	}
	free(tamanioValor);
}

void imprimirTexto(char* texto) {
	log_info(archivoLog,"ImprimirTexto: %s \n", texto);
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
		log_error(archivoLog,"Error: Erro de conexion con el Nucleo \n");
		log_error(archivoLog,"Error: Algo fallo al enviar el mensaje para imprimir texto al nucleo, recibi: %d \n", verificador);
	}
}

void finalizar() {
	log_info(archivoLog,"Finalizado \n");
	int tamanioStack = list_size(pcb.stack);
	log_info(archivoLog,"remueve: %d \n", tamanioStack-1);

	if (tamanioStack >1){
		Stack* stackActual = obtenerStack();
		pcb.pc = stackActual->retPos;
	}
	list_remove(pcb.stack,tamanioStack-1);
	if (tamanioStack == 1){
		char* mensaje = string_new();
		string_append(&mensaje,"0003");
		pcb.stack = list_create();
		char* pcb_char = toStringPCB(pcb);
		liberarPCB(pcb);
		string_append(&mensaje,toStringInt(strlen(pcb_char)));
		string_append(&mensaje,pcb_char);
		log_info(archivoLog,"\n\nLe mande al nucleo el PCB: %s\n\n", toStringPCB(fromStringPCB(pcb_char)));
		log_info(archivoLog,"\n\nLe mande al nucleo el PCB: %s\n\n", toStringPCB(fromStringPCB(pcb_char)));

		free(pcb_char);
		string_append(&mensaje,toStringInt(status));
		string_append(&mensaje,"\0");
		send(nucleo,mensaje,strlen(mensaje),0);
		free(mensaje);
		finalizado = 1;
		log_info(archivoLog,"PCB enviado al Nucleo sin problemas \n");
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
	Stack* stack = obtenerStack();
	var->pagina = obtenerPagDisponible();
	if (list_size(stack->args)>0){
		Pagina pagArgs = ((Variable*)list_get(stack->args,list_size(stack->args)-1))->pagina;
		if (numeroPagina(var->pagina)<=numeroPagina(pagArgs)){
			if ((pagArgs.off+pagArgs.tamanio+4)<=TAMANIO_PAGINA){
				var->pagina.pag = pagArgs.pag;
				var->pagina.off = pagArgs.off+4;
			}else{
				var->pagina.pag = pagArgs.pag+1;
				var->pagina.off = 0;
			}
		}
	}
	return var;
}

int numeroPagina(Pagina pag){
	char* resultado = string_new();
	string_append(&resultado,toStringInt(pag.pag));
	string_append(&resultado,toStringInt(pag.off));
	string_append(&resultado,toStringInt(pag.tamanio));
	int result = atoi(resultado);
	free(resultado);
	return result;
}

Pagina obtenerPagDisponible(){
	Stack* stackActual = obtenerStack();
	int cantidadDeVariables = list_size(stackActual->vars);
	Pagina pagina;
	if ((cantidadDeVariables<=0)&&(list_size(pcb.stack)==1)){
		pagina.pag = pcb.paginas_codigo;
		pagina.off = 0;
	}else{
		if (cantidadDeVariables <= 0){
			stackActual = anteUltimoStack();
			if (stackActual == NULL){
				pagina.pag = pcb.paginas_codigo;
			    pagina.off = 0;
			    pagina.tamanio = 4;
			    return pagina;
			}
			cantidadDeVariables = list_size(stackActual->vars);
		}
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
	log_info(archivoLog,"Agregando a la lista de variables: %c \n", var->id);
	if (('0'>=var->id)&&(var->id<='9')){
		list_add(stackActual->args,var);
	}else{
		t_list* variables = stackActual->vars;
		list_add(variables,var);
	}
}

Stack* obtenerStack(){
	int tamanioStack = list_size(pcb.stack);
	if (tamanioStack <= 0){
		Stack* stack = malloc(sizeof(Stack));
		stack->vars = list_create();
		Pagina pagina;
		pagina.off = 0;
		pagina.pag = 0;
		pagina.tamanio = 0;
		stack->retVar = pagina;
		stack->args = list_create();
		list_add(pcb.stack,stack);
		tamanioStack = 1;
	}
	return (list_get(pcb.stack,tamanioStack-1));
}
Stack* anteUltimoStack(){
	int cantidadDeStacks = list_size(pcb.stack);
	Stack* stackDisponible;
	int i;
	for (i = 1; i<= cantidadDeStacks;i++){
		stackDisponible = list_get(pcb.stack,cantidadDeStacks-i);
		if (list_size(stackDisponible->vars) > 0){
			return stackDisponible;
		}
	}
	return NULL;
}


void parsear(char* instruccion){
	analizadorLinea(strdup(instruccion), &functions, &kernel_functions);
}

int tienePermiso(char* autentificacion){
	return 1;
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
	log_info(archivoLog,"Le envio a la UMC: %s", mensaje);
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
	int verificador = recv(umc,resp,4,MSG_WAITALL);
	if (verificador <= 0){
		log_error(archivoLog,"Error: Fallo la conexion con la UMC\n");
		finalizado = -1;
	}else{
		if (!atoi(resp)){
			log_error(archivoLog,"Pagina inexistente\n");
			finalizado = -1;
		}
	}
	free(resp);
}

void enviarMensajeNucleoConsulta(char* variable){
	char* mensaje = string_new();
	string_append(&mensaje, "00020001");
	char* tamVariable=toStringInt(strlen(variable));
	string_append(&mensaje, tamVariable);
	free(tamVariable);
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
