/*
 * swap.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <commons/config.h>
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "Funciones/Comunicacion.h"


#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

typedef struct{
	int puerto;
	char* ip;
	char* nombre_swap;
	int cantidadPaginas;
	int tamPagina;
	int retardoCompactacion;
}datosConfiguracion;

typedef struct{
	int proceso,inicio,offset;
}traductor_marco;

struct sockaddr_in crearDireccion(int puerto, char* ip);
int comprobarCliente(int);
int leerConfiguracion(char*, datosConfiguracion**);
char* crearArchivoSwap();
int guardarCodigo(char*);
int buscarEspacioLibre(int);

datosConfiguracion *datosSwap;
t_bitarray* bitArray;
int pagsLibres;
char* archivoSwap;
t_list* paginas;

int main(int argc, char* argv[]){
	int	conexionUmc, swap_servidor=socket(AF_INET, SOCK_STREAM, 0);

	datosSwap=malloc(sizeof(datosConfiguracion));

	if (!(leerConfiguracion("ConfigSwap", &datosSwap)|| leerConfiguracion("../ConfigSwap", &datosSwap))) {
		printf("Error archivo de configuracion\n FIN.");
		return 1;}

	archivoSwap=crearArchivoSwap();				//Devuelve el mmap del archivo iniciado con \0

	printf("%s\n",archivoSwap);
	bitArray=bitarray_create(archivoSwap,datosSwap->cantidadPaginas);			//El bitArray usa bits, y el resto usan Bytes
	int i;
	for(i=0;i<datosSwap->cantidadPaginas;i++,bitarray_clean_bit(bitArray,i));				//Inicializa el bitArray
	if (string_equals_ignore_case(archivoSwap,"Fuiste")){printf("Error al crear el archivo Swap\n");return 0;}

	//munmap(archivoSwap,sizeof(archivoSwap));

	struct sockaddr_in direccionSWAP=crearDireccion(datosSwap->puerto, datosSwap->ip);

	pagsLibres=30;//datosSwap->cantidadPaginas;					//todo

	printf("Swap OK - ");

	paginas=list_create();

	if (!bindear(swap_servidor, direccionSWAP)) {printf("Error en el bind, Adios!\n");
		return 1;
	}

	printf("Esperando UMC...\n");
	listen(swap_servidor,1);

	//----------------------------creo cliente para umc

	struct sockaddr_in direccionCliente;
	int sin_size = sizeof(struct sockaddr_in);

	do{
		conexionUmc= accept(swap_servidor, (void *) &direccionCliente, (void *) &sin_size);
			if (conexionUmc == -1){perror("Una conexion rechazada\n");}
	}while(!comprobarCliente(conexionUmc));						//Mientras el que se conecta no es la UMC
	printf("UMC Conectada!\n");

	//----------------Recibo datos de la UMC

	int operacion=1, cantPaginas, PID,tamCodigo;
	char* codigo;
	int posicion;
	while (operacion){
		operacion = atoi(recibirMensaje(conexionUmc,1));
		switch (operacion){
		case 1:															//Almacenar codigo, PROTOCOLO: [1° PID, 2° Cant paginas (4 bytes c/u))]
				PID = recibirProtocolo(conexionUmc);
				cantPaginas = recibirProtocolo(conexionUmc);
				if (pagsLibres>=cantPaginas){
					traductor_marco* nuevaFila;
					send(conexionUmc,"ok",2,0);
					tamCodigo=recibirProtocolo(conexionUmc);
					codigo=recibirMensaje(conexionUmc,tamCodigo);
					posicion=buscarEspacioLibre(cantPaginas);
					memcpy(archivoSwap+posicion*datosSwap->tamPagina,codigo,tamCodigo);
					nuevaFila->inicio=posicion;
					nuevaFila->offset=tamCodigo;
					nuevaFila->proceso=PID;
					list_add(paginas,nuevaFila);
			//		printf("\n%s",archivoSwap);
					printf("\nPagsLibres:%d\n",pagsLibres);
					printf("Guardado!!\n");
					free(codigo);
				}

				break;}

		//char* codigo=malloc(tamCodigo);
	}
	free(datosSwap);
	printf("Cayó la Umc, swap autodestruida!\n");
	return 0;
}


//-----------------------------------FUNCIONES-----------------------------------

int comprobarCliente(int cliente){
	char* bufferHandshake = malloc(10);
	int bytesRecibidosH = recv(cliente, bufferHandshake, 10, 0);				//lo paso a string para comparar
	if (string_equals_ignore_case("soy_la_umc", bufferHandshake)) {
		free(bufferHandshake);
		send(cliente, "Hola_umc", 8, 0);
		return 1;}
	free(bufferHandshake);
	return 0;													//No era la UMC :/
}

int leerConfiguracion(char *ruta, datosConfiguracion **datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		return 0;
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys < 6) {
			return 0;
		} else {
			(*datos)->puerto = buscarInt(archivoConfiguracion, "PUERTO");
			char* nombreSwap=string_new();
			string_append(&nombreSwap,config_get_string_value(archivoConfiguracion, "NOMBRE_SWAP"));
			(*datos)->nombre_swap =nombreSwap;
			(*datos)->cantidadPaginas = buscarInt(archivoConfiguracion, "CANTIDAD_PAGINAS");
			(*datos)->tamPagina = buscarInt(archivoConfiguracion, "TAM_PAGINA");
			(*datos)->retardoCompactacion = buscarInt(archivoConfiguracion, "RETARDO_COMPACTACION");
			char* ip=string_new();
			string_append(&ip,config_get_string_value(archivoConfiguracion,"IP"));
			(*datos)->ip=ip;
			config_destroy(archivoConfiguracion);
			return 1;
		}
	}
}
char* crearArchivoSwap(){
	char* instruccion=string_from_format("dd if=/dev/zero of=%s count=1 bs=100",datosSwap->nombre_swap,datosSwap->cantidadPaginas,datosSwap->tamPagina);
	system(instruccion);
	char* nombreArchivo=string_new();
	string_append(&nombreArchivo,datosSwap->nombre_swap);
	int fd_archivo=open(nombreArchivo,O_RDWR);
	if (fd_archivo!=-1) {
	char* archivo=(char*) mmap(NULL ,datosSwap->cantidadPaginas*datosSwap->tamPagina,PROT_READ|PROT_WRITE,MAP_SHARED,(int)fd_archivo,0);
	if (archivo!=MAP_FAILED){
		return archivo;}
		}
	return "Fuiste";
}

int buscarEspacioLibre(int cantPaginas){
	int pos,a=1;
	for (pos = 0 ; (pos<datosSwap->cantidadPaginas) && a ;pos++){
		if (!bitarray_test_bit(bitArray,pos)){a=0;}
	}
	for(a=0;a<cantPaginas;a++){					//Marca las paginas como ocupadas
		//printf("%d",bitarray_test_bit(bitArray,pos+a));
		bitarray_set_bit(bitArray,pos+a);
		//printf("%d\n",bitarray_test_bit(bitArray,pos+a));
	}
	pagsLibres-=a;
	return (pos-1);
}

int guardarCodigo(char* codigo){
	int pos,a=1;
	printf("%s\n",codigo);
	for (pos = -1 ; a ;pos++){
		if (!bitarray_test_bit(bitArray,pos)){a=0;}}
	int longitud=string_length(codigo);
	int asd=pos*datosSwap->tamPagina;
	memcpy(archivoSwap+asd,"ASDASDASD",(int)9);
	int fin=string_length(codigo)/datosSwap->tamPagina;
	if (string_length(codigo)%datosSwap->tamPagina) fin++;
	for(;pos<=fin;pos++){
		bitarray_set_bit(bitArray,pos);
	}
	printf("%s %d\n",archivoSwap,string_length(codigo));
	return 1;
}
