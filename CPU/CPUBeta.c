#include "parser.h"

int levantarArchivoDeConfiguracion();
void conectarseAlNucleo();
void conectarseALaUMC();
int procesarPeticion();
int procesarCodigo();

int PUERTO_NUCLEO;
char AUTENTIFICACION[100];
char ARCHIVO_DE_CONFIGURACION[60] = "ArchivoDeConfiguracionCPU.txt";
char IP_NUCLEO[50];
char IP_UMC[50];
int quantum = 1;


/*
 * Precondiciones el Quantum me tiene que venir junto con el PCB como protocolo;
 * Si el protocolo es 0 interpreto que debo finalizar;
 */

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
	buscar(archivoJson,"PURTO_NUCLEO", puertoDelNucleo);
	PUERTO_NUCLEO = atoi(puertoDelNucleo);
	if (PUERTO_NUCLEO == 0){
		printf("Error: No se ha encontrado el Puerto del Nucleo en el archivo de Configuracion \n");
		return -1;
	}
	buscar(archivoJson,"AUTENTIFICACION", AUTENTIFICACION);
	if (AUTENTIFICACION == NULL){
		printf("Error: No se ha encontrado la Autentificacion en el archivo de Configuracion \n");
		return -1;
	}
	buscar(archivoJson,"IP_NUCLEO", IP_NUCLEO);
	if (IP_NUCLEO == NULL){
		printf("Error: No se ha encontrado la IP del Nucleo en el archivo de Configuracion \n");
		return -1;
	}
	buscar(archivoJson,"IP_UMC", IP_UMC);
	if (IP_NUCLEO == NULL){
		printf("Error: No se ha encontvoid saltoDeLinea(int cantidad, char* nombre)rado la IP de la UMC en el archivo de Configuracion \n");
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
	char* puertoUMC = esperarRespuesta(nucleo);
	if (*puertoUMC == NULL){
		printf("Error: Conexion con el nucleo Interrumpida\n");
		umc = -1;
		return;
	}
	int puerto = atoi(puertoUMC);
	umc = conectar(puerto,IP_UMC,AUTENTIFICACION);
	if (umc<0){
		printf("Error: No se ha logrado establecer la conexion con la UMC\n");
	}
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
			if (*pcb_char == NULL){
				perror("Error: Error de conexion con el nucleo\n");
			}else{
				pcb = fromStringPCB(pcb_char);
				if (procesarCodigo()<0) return -1;
			}
			free(pcb_char);
	}
}

int procesarCodigo(){
	finalizado = 0;
	char* linea;
	printf("Iniciando Proceso de Codigo...\n");
	while ((quantum>0) && (!(finalizado))){
		linea = pedirLinea();
		printf("Recibi: %s \n", linea);
		if (*linea == NULL){
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
