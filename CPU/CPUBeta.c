#include "sockets.h"
#include "parser.c"

int levantarArchivoDeConfiguracion();
int conectarseAlNucleo();
int conectarseALaUMC();
int procesarPeticion();
int esperarQuantum();
int procesarCodigo();

int PUERTO_NUCLEO;
char AUTENTIFICACION[100];

struct PCB{

};

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
	char* archivoJson =toJson(archivoDeConfiguracion);
	char puertoDelNucleo [6];
	buscar(archivoJson,"PN", puertoDelNucleo);
	PUERTO_NUCLEO = atoi(puertoDelNucleo);
	if (PUERTO_NUCLEO == 0){
		printf("Error: No se ha encontrado el Puerto del Nucleo en el archivo de Configuracion \n");
		return -1;
	}
	buscar(archivoJson,"AT", AUTENTIFICACION);

	return 0;
}


void conectarseAlNucleo(){
	printf("Conectandose al nucleo....\n");
	nucleo = conectar(PUERTO_NUCLEO, AUTENTIFICACION);
	if (nucleo<0){
		printf("Error al conectarse con el nucelo \n");
		return -1;
	}
	printf("Conexion con el nucleo OK... \n");
}

void conectarseALaUMC(){
	printf("Conectandose a la UMC....\n");
	char* puertoUMC = esperarRespuesta(nucleo);
	if (*puertoUMC == NULL){
		printf("Error: Conexion con el nucleo Interrumpida\n");
		umc = -1;
		return;
	}
	int puerto = atoi(puertoUMC);
	umc = conectar(puerto,AUTENTIFICACION);
	if (umc<0){
		printf("Error: No se ha logrado establecer la conexion con la UMC\n");
	}
	printf("Conexion con la UMC OK...\n");
}

int procesarPeticion(){
	int quantum
	while(1){
		pcb = esperarPCB(nucleo);
		quantum = esperarQuantum(nucleo);
		if(quantum<0) return -1;
		if (procesarCodigo(nucleo, umc,pcb)<0) return -1;		
	}
}

int esperarQuantum(){
	char* resp = esperarRespuesta(nucleo);
	if (resp == NULL){
		printf("Error: Se ha desconectado el Nucleo \n");
		return -1;
	}
	return (atoi(resp));
}

int procesarCodigo(){
	finalizado = 0;
	char* linea;
	printf("Iniciando Proceso de Codigo...\n");
	while ((quantum>0) && (!(finalizado))){
		linea = pedirLinea(umc,pcb);
		printf("Recibi: %s \n", linea);
		if (*linea == NULL) return -1;
		parsear(linea);
	}
	printf("Finalizado el Proceso de Codigo...\n");
}