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
	int retardoAcceso;
	int retardoCompactacion;
}datosConfiguracion;

typedef struct{
	int proceso,inicio,paginas;
}traductor_marco;

struct sockaddr_in crearDireccion(int puerto, char* ip);
int comprobarCliente(int);
int leerConfiguracion(char*, datosConfiguracion**);
void* crearArchivoSwap();
int guardarDatos(int,int,int);
int buscarEspacioLibre(int);
void* buscar(int, int);
int compactar();
int eliminarProceso(int);
void verMarcos();

datosConfiguracion *datosSwap;
t_bitarray* bitArray;
int pagsLibres;
void* archivoSwap;
t_list* tablaPaginas;

int main(int argc, char* argv[]){

	int	conexionUmc, swap_servidor=socket(AF_INET, SOCK_STREAM, 0);

	datosSwap=malloc(sizeof(datosConfiguracion));

	if (!(leerConfiguracion("ConfigSwap", &datosSwap)|| leerConfiguracion("../ConfigSwap", &datosSwap))) {
		printf("Error archivo de configuracion\n FIN.");
		return 1;}


	if(!crearArchivoSwap()){
		printf("Error al crear archivo Swap\n");
	}
	verMarcos();
/*	memcpy(archivoSwap,"be",2);
	verMarcos();*/



	if (string_equals_ignore_case(archivoSwap,"Fuiste")){printf("Error al crear el archivo Swap\n");return 0;}

	//munmap(archivoSwap,sizeof(archivoSwap));

	struct sockaddr_in direccionSWAP=crearDireccion(datosSwap->puerto, datosSwap->ip);

	pagsLibres=30;//datosSwap->cantidadPaginas;					//todo

	printf("Swap OK - ");

	tablaPaginas=list_create();

	if (!bindear(swap_servidor, direccionSWAP)) {printf("Error en el bind, Adios!\n");
		return 1;
	}

	printf("Esperando UMC...\n");
	listen(swap_servidor, 5);

	//----------------------------creo cliente para umc

	struct sockaddr_in direccionCliente;
	int sin_size = sizeof(struct sockaddr_in);

	do{
		conexionUmc= accept(swap_servidor, (void *)&direccionCliente, (void *)&sin_size);
		if (conexionUmc == -1){perror("Una conexion rechazada\n");}
	}while(!comprobarCliente(conexionUmc));
	printf("UMC Conectada!\n");

	//----------------Recibo datos de la UMC
	int operacion=1, PID, cantPaginas, pagina;
	while (operacion){
		operacion = atoi((char*)recibirMensaje(conexionUmc,1));
		PID = recibirProtocolo(conexionUmc);
		switch (operacion){
		case 1:															//Almacenar codigo, PROTOCOLO: [1° PID, 2° Cant paginas (4 bytes c/u))]
				cantPaginas = recibirProtocolo(conexionUmc);
				if (pagsLibres>=cantPaginas){
					guardarDatos(conexionUmc,cantPaginas, PID);
					send(conexionUmc, "ok", 2, 0);

					printf("\n%s\n",(char*)archivoSwap);
			//		printf("\nPagsLibres:%d\n",pagsLibres);
			//		printf("Guardado!!\n");
					verMarcos();
				}
				else {
					send(conexionUmc,"no",2,0);
				}
				break;
		case 2:																//enviar pagina a la UMC
				pagina=recibirProtocolo(conexionUmc);
				void* datos=buscar(PID,pagina);
				send(conexionUmc,datos,datosSwap->tamPagina,0);
				break;

		case 3:																//eliminar ansisop
				eliminarProceso(PID);
				verMarcos();
		}
	}
	free(datosSwap);
	printf("Cayó la Umc, swap autodestruida!\n");
	return 0;
}


//-----------------------------------FUNCIONES-----------------------------------

int comprobarCliente(int cliente){
	char* bufferHandshake = malloc(10);
	int bytesRecibidosH = recv(cliente, bufferHandshake, 10, 0);				//lo paso a string para comparar
	bufferHandshake[bytesRecibidosH] = '\0';
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
		if (cantidadKeys < 7) {
			return 0;
		} else {
			(*datos)->puerto = buscarInt(archivoConfiguracion, "PUERTO");
			char* nombreSwap=string_new();
			string_append(&nombreSwap,config_get_string_value(archivoConfiguracion, "NOMBRE_SWAP"));
			(*datos)->nombre_swap =nombreSwap;
			(*datos)->cantidadPaginas = buscarInt(archivoConfiguracion, "CANTIDAD_PAGINAS");
			(*datos)->tamPagina = buscarInt(archivoConfiguracion, "TAM_PAGINA");
			(*datos)->retardoAcceso = buscarInt(archivoConfiguracion, "RETARDO_ACCESO");
			(*datos)->retardoCompactacion = buscarInt(archivoConfiguracion, "RETARDO_COMPACTACION");
			char* ip=string_new();
			string_append(&ip,config_get_string_value(archivoConfiguracion,"IP"));
			(*datos)->ip=ip;
			config_destroy(archivoConfiguracion);
			return 1;
		}
	}
}
void* crearArchivoSwap(){
	char* instruccion=string_from_format("dd if=/dev/zero of=%s count=%d bs=%d",datosSwap->nombre_swap,datosSwap->cantidadPaginas,datosSwap->tamPagina);
	system(instruccion);
	int fd_archivo=open(datosSwap->nombre_swap,O_RDWR);
	archivoSwap=(void*) mmap(NULL ,datosSwap->cantidadPaginas*datosSwap->tamPagina,PROT_READ|PROT_WRITE,MAP_PRIVATE,(int)fd_archivo,0);
	char* archivoBitArray=(char*) mmap(NULL ,datosSwap->cantidadPaginas*datosSwap->tamPagina,PROT_READ|PROT_WRITE,MAP_PRIVATE,(int)fd_archivo,0);
	bitArray=bitarray_create((char*)archivoBitArray,datosSwap->cantidadPaginas);			//El bitArray usa bits, y el resto usan Bytes
	if (archivoSwap==MAP_FAILED){
		return 0;}
	return 1;
}


int guardarDatos(int conexionUmc,int cantPaginas, int PID){
	int existeProceso(traductor_marco* marco){
		return (marco->proceso==PID);
	}
	int posicion,size;
	traductor_marco* nuevaFila = malloc(sizeof(traductor_marco));
	void* datos;
	traductor_marco* proceso=list_find(tablaPaginas,(void*)existeProceso);
	if (proceso==NULL){								//No existe el proceso => guardo el ansisop
		int tamanio = recibirProtocolo(conexionUmc);
		datos = recibirMensaje(conexionUmc, tamanio);
		posicion = buscarEspacioLibre(cantPaginas);
		posicion *= datosSwap->tamPagina;
		nuevaFila->inicio = posicion;
		nuevaFila->paginas = cantPaginas;
		nuevaFila->proceso = PID;
		size=tamanio;
		list_add(tablaPaginas, nuevaFila);
		pagsLibres-=cantPaginas;
		printf("Nuevo ansisop\n");
	}
	else{												//actualizar pagina
		datos = recibirMensaje(conexionUmc, datosSwap->tamPagina);
		size=datosSwap->tamPagina;
		posicion=recibirProtocolo(conexionUmc);
		posicion*=datosSwap->tamPagina;
		posicion+=proceso->inicio;					//donde arranca el proceso + pag
		printf("Pagina modificada\n");
	}
	memcpy(archivoSwap + posicion * datosSwap->tamPagina, datos, size);
	free(datos);
	return 1;
}


int buscarEspacioLibre(int cantPaginas){							//todo debe buscar espacios CONTIGUOS o compactar
	int pos=0, i,contador=0,compactado=0;
	do{
	for(i=0;i<datosSwap->cantidadPaginas && contador<cantPaginas;i++){
		if (!bitarray_test_bit(bitArray,i)){
			contador++;
		}
		else{
			contador=0;
			pos=i;
		}
	}
	if (contador<cantPaginas){
		compactar();							//todo agregar semaforo
		compactado=0;
	}else{compactado=1;}
	}while(!compactado);

	for(i=pos;i<pos+contador;i++){
		bitarray_set_bit(bitArray,i);
		//printf("Pos %d - Ocupado: %d\n",i,bitarray_test_bit(bitArray,i));
	}
	//printf("----\n");
	return pos;
}

int compactar(){
	int pos=0, i,contador=0,compactado=0,inicio=0;
	int inicioMenorMayor(traductor_marco* marco1, traductor_marco* marco2){
		return (marco1->inicio<marco2->inicio);
	}
	int proximoProceso(traductor_marco* proceso){
		if (proceso->inicio>=i*datosSwap->tamPagina){
			return 1;
		}
		return 0;
	}
	list_sort(tablaPaginas,inicioMenorMayor);
	for(i=0;i<datosSwap->cantidadPaginas;i++){
		if (!bitarray_test_bit(bitArray,i)){
				traductor_marco* procesoAMover=list_find(tablaPaginas,proximoProceso);
		}
	}
	return 1;
}

	/*	int pos,a=1;
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
}*/

void* buscar(int pid, int pag){
	int proceso(traductor_marco* fila){
		return (fila->proceso==pid);
	}
	traductor_marco* datosProceso=list_find(tablaPaginas,(void*)proceso);
	void* pagina=(void*)malloc(datosSwap->tamPagina);
	int pos=datosProceso->inicio+(datosSwap->tamPagina*pag);
	memcpy(pagina,archivoSwap+pos,datosSwap->tamPagina);
	memcpy(pagina+datosSwap->tamPagina,"\0",1);
	printf("PAG %d - Datos:  %s-\n",pos/datosSwap->tamPagina,(char*)pagina);
	return pagina;
}

int eliminarProceso(int pid){
	int entradaDelProceso(traductor_marco* entrada){
		return (entrada->proceso==pid);
	}
	void eliminarEntrada(traductor_marco* entrada){
		free(entrada);
	}
	traductor_marco* datosProceso=list_find(tablaPaginas,(void*)entradaDelProceso);
	int posicion=datosProceso->inicio/datosSwap->tamPagina;
	int i;
	for(i=0;i<datosProceso->paginas;i++){
		bitarray_clean_bit(bitArray,posicion);
		posicion++;
	}
	free(datosProceso);
	list_remove_and_destroy_by_condition(tablaPaginas,(void*)entradaDelProceso,(void*)eliminarEntrada);
	return 1;
}
void verMarcos(){
	int i;
	printf("Estado de los marcos: \n");
	for(i=0;i<datosSwap->cantidadPaginas;i++){
		printf("Pos %d | Ocupado:%d\n",i,bitarray_test_bit(bitArray,i));
	}
}


