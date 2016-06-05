#include "CPUBeta.h"

int main(){
	printf("CPU estable...\n");

	if(levantarArchivoDeConfiguracion()<0) return -1;

	conectarseAlNucleo();
	if (nucleo < 0) return -1;

	conectarseALaUMC();
	if (umc < 0) return -1;

	if (procesarPeticion()<0) return -1;

	printf("Cerrando CPU.. \n");

	return 0;
}

int levantarArchivoDeConfiguracion(){
	FILE* archivoDeConfiguracion = fopen(ARCHIVO_DE_CONFIGURACION,"r");
	if (archivoDeConfiguracion==NULL){
		printf("Error: No se pudo abrir el archivo de configuracion, verifique su existencia en la ruta: %s \n", ARCHIVO_DE_CONFIGURACION);
		return -1;
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
	printf("Conectandose al nucleo....\n");
	nucleo = conectar(PUERTO_NUCLEO,IP_NUCLEO, AUTENTIFICACION);
	if (nucleo<0){
		printf("Error al conectarse con el nucelo \n");
		return;
	}
	printf("Conexion con el nucleo OK... \n");
}

void conectarseALaUMC(){
	printf("Conectandose a la UMC....\n");

	umc = conectar(PUERTO_UMC,IP_UMC,AUTENTIFICACION);
	if (umc<0){
		printf("Error: No se ha logrado establecer la conexion con la UMC\n");
	}
	char* tamanioPagina = esperarRespuesta(umc);
	if (tamanioPagina[0] == '\0'){
		printf("Error: Se ha perdido la coneccion con la UMC\n");
		close(umc);
	}
	TAMANIO_PAGINA = atoi(tamanioPagina);
	printf("Conexion con la UMC OK...\n");
}

int procesarPeticion(){
	int quantum;
	char* pcb_char;

	while(1){
		quantum = recibirProtocolo(nucleo);
		if (quantum <= 0){
			if (quantum == 0){
				close(nucleo);
				close(umc);
				return 0;
			}
			perror("Error: Error de conexion con el nucleo\n");
		}else{
			pcb_char = esperarRespuesta(nucleo);
			if (pcb_char[0] == '\0'){
				perror("Error: Error de conexion con el nucleo\n");
			}else{
				pcb = fromStringPCB(pcb_char);
				if (procesarCodigo()<0) return -1;
			}
			free(pcb_char);
		}
	}
	return 0;
}

int procesarCodigo(){
	finalizado = 0;
	char* linea;
	printf("Iniciando Proceso de Codigo...\n");
	while ((quantum>0) && (!(finalizado))){
		linea = pedirLinea();
		printf("Recibi: %s \n", linea);
		if (linea[0] == '\0'){
			perror("Error: Error de conexion con la UMC \n");
			return -1;
		}
		parsear(linea);
		quantum--;
		saltoDeLinea(1,NULL);
	}
	printf("Finalizado el Proceso de Codigo...\n");
	return 0;
}

char* pedirLinea(){
	int pag = pcb.indices.instrucciones_serializado[pcb.pc].start/TAMANIO_PAGINA;
	int off = pcb.indices.instrucciones_serializado[pcb.pc].start-TAMANIO_PAGINA*pag;
	int size = pcb.indices.instrucciones_serializado[pcb.pc].offset;
	int size_page = size;
	int proceso = pcb.id;
	char* respuesta;
	char* respuestaFinal = malloc(sizeof(char));
	int repeticiones = 0;
	while(size >0){
		if (size > TAMANIO_PAGINA-off){
			size_page = TAMANIO_PAGINA-off;
		}else{
			size_page = size;
		}
		enviarMensajeUMCConsulta(pag,off,size_page,proceso);
		respuesta = esperarRespuesta(umc);
		respuestaFinal = realloc(respuestaFinal,(strlen(respuesta)+ strlen(respuestaFinal)+1)*sizeof(char));
		repeticiones++;
		pag++;
		size = size - size_page;
		off = 0;
		if (respuesta[0] == '\0'){
			printf("Error: No se ha logrado conectarse a la UMC\n");
			break;
		}
	}

	return respuestaFinal;
}


////////////////////////////////////----PARSER-------///////////////////////////////////////////////////

t_puntero definirVariable(t_nombre_variable variable) {
	printf("definir la variable %c\n", variable);
	Variable* var = crearVariable(variable);
	sumarEnLasVariables(var);
	return  (int)var;
}

t_puntero obtenerPosicionVariable(t_nombre_variable variable) {
	printf("Obtener posicion de %c\n", variable);
	Stack* stack = obtenerStack();
	t_list* variables = stack->vars;
	Variable* var;
	int i;
	for(i = 0; i< list_size(variables); i++){
		var = (Variable*) list_get(variables,i);
		if ( var->id == variable  ){
			return (int)&(var->pagina);
		}
	}
	return -1;
}

t_valor_variable dereferenciar(t_puntero pagina) {
	Pagina* pag = (Pagina*) pagina;
	enviarMensajeUMCConsulta(pag->pag,pag->off,pag->tamanio,pcb.id);
	return atoi(esperarRespuesta(umc));
}

void asignar(t_puntero pagina, t_valor_variable valor) {
	Pagina* pag = (Pagina*) pagina;
	enviarMensajeUMCAsignacion(pag->pag,pag->off,pag->tamanio,pcb.id,valor);
}
char* pedirLinea();
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

void signal(t_nombre_semaforo identificador_semaforo){
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
	int cantidadDeVariables = list_size(stackActual->vars)-1;
	Pagina pagina;
	if (cantidadDeVariables == 0){
		pagina.pag = pcb.paginas_codigo+1;
		pagina.off = 0;
	}else{
		Variable* ultimaVariable = list_get(stackActual->vars, cantidadDeVariables);
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
	list_add(variables,var);
}

Stack* obtenerStack(){
	int tamanioStack = list_size(pcb.stack)-1;
	return (list_get(pcb.stack,tamanioStack));
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
	char* mensaje = malloc(18*sizeof(char));
	sprintf(mensaje,"2%s%s%s%s",toStringInt(proceso),toStringInt(pag),toStringInt(off),toStringInt(size));
	send(umc,mensaje,strlen(mensaje),0);
	free(mensaje);
}

void enviarMensajeUMCAsignacion(int pag, int off, int size, int proceso, int valor){
	char* mensaje = malloc(22*sizeof(char));
	sprintf(mensaje,"3%s%s%s%s%s",toStringInt(proceso),toStringInt(pag),toStringInt(off),toStringInt(size),toStringInt(valor));
	send(umc,mensaje,strlen(mensaje),0);void parsear(char* instruccion);
	free(mensaje);
}

void enviarMensajeNucleoConsulta(char* variable){

}
void enviarMensajeNucleoAsignacion(char* variable, int valor){

}


