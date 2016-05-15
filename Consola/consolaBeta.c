/*
 * Pre condiciones:
 * 	Archivo de configuracion debe tener PN: Puerto del nucleo
 * 										AT: Nombre con el cual se autentifica
 *	Nombre del Archivo de configuracion: ArchivoDeConfiguracionConsola.txt
 *
 *	Los archivos sockets.h y json.h deben estar al mismo nivel que el archivo consola.c // Espero hacer esto temporal
 */

#include "sockets.h"
#include "json.h"

int validarEntrada(int parametros);
int levantarArchivoDeConfiguracion();
int conectarseAlNucleo();
int enviarAlNucleo(int nucleo , char* archivoAnsisop);
int imprimirMensajesDelNucleo(int nucleo);

const char* ARCHIVO_DE_CONFIGURACION = "ArchivoDeConfiguracionConsola.txt";
int PUERTO_NUCLEO;
char* AUTENTIFICACION;

int main (int argc, char* argv[]){
	printf("Consola estable \n");

	if (validarEntrada(argc)<0) return -1;

	if(levantarArchivoDeConfiguracion()<0) return -1;

	int nucleo = conectarseAlNucleo();
	if (nucleo < 0) return -1;

	if (enviarAlNucleo(nucleo , argv[1])<0) return -1;

	if (imprimirMensajesDelNucleo(nucleo)) return -1;

	printf("Cerrando Consola.. \n");

	return 0;
}


int validarEntrada(int parametros){
	if (parametros == 1){
		printf("Error : No se ha ingresado el archivo Ansisop \n");
		return -1;
	}
	if (parametros != 2){
		printf("Error: Se ha ingresado parametros errorneos, recuerda que solo debe ingresar la direccion del archivo Ansisop \n");
		return -1;
	}
	return 0;
}
int levantarArchivoDeConfiguracion(){
	FILE* archivoDeConfiguracion = fopen(ARCHIVO_DE_CONFIGURACION,"r");
	if (archivoDeConfiguracion==NULL){
		printf("Error: No se pudo abrir el archivo de configuracion, verifique su existencia en la ruta: %s \n", ARCHIVO_DE_CONFIGURACION);
		return -1;
	}
	char* archivoJson = toJson(archivoDeConfiguracion);
	char* puertoDelNucleo = buscar(archivoJson,"PN");
	PUERTO_NUCLEO = atoi(puertoDelNucleo);
	if (PUERTO_NUCLEO == 0){
		printf("Error: No se ha encontrado el Puerto del Nucleo en el archivo de Configuracion \n");
		return -1;
	}
	AUTENTIFICACION = buscar(archivoJson,"AT");

	return 0;
}
int conectarseAlNucleo(){
	printf("Conectandose con el Nucleo...\n");
	int nucleo = conectar(PUERTO_NUCLEO,AUTENTIFICACION);
	if (nucleo < 0){
		printf("Error: No se ha logrado establecer la conexion con el Nucleos\n");
		return -1;
	}
	printf("conexion con el nucleo OK...\n");

	return nucleo;
}
int enviarAlNucleo(int nucleo , char* archivoAnsisop){

	FILE* archivo = fopen(archivoAnsisop,"r");

	if (archivo == NULL){
		printf("Error: No se ha logrado abrir el archivo Ansisop verifique su existencia en la direccion: %s \n", archivoAnsisop);
		return -1;
	}

	char* ansisop = toJson(archivo);
	enviarMensaje(nucleo, ansisop);

	return 0;
}
int imprimirMensajesDelNucleo(int nucleo){
	printf("Esperando al Nucleo ...\n");
	char* respuesta;
	while (1){
		respuesta = esperarRespueta(nucleo);
		if (respuesta == NULL){
			printf("Conexion con el Nucleo finalizada...\n");
			return 0;
		}
		printf("-%s\n",respuesta);
	}
}


int conectarseAlNucleo(){
	printf("Conectandose al nucleo....\n");
	int nucleo = conectar(PUERTO_NUCLEO, AUTENTIFICACION);
	if (nucleo<0){
		printf("Error al conectarse con el nucelo \n");
		return -1;
	}
	printf("Conexion con el nucleo OK... \n");
	return nucleo;
}

