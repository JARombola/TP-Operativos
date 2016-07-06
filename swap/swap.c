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
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "Funciones/Comunicacion.h"
#include <commons/log.h>

typedef struct{
	int proceso,inicio,paginas;
}traductor_marco;

#define GUARDAR 1
#define ENVIAR 2
#define ELIMINAR 3

int crearArchivoSwap();
int guardarDatos(int,int,int);
int buscarEspacioLibre(int);
void* buscar(int, int);
int compactar();
int eliminarProceso(int);
void verMarcos();


t_log* logs;
datosConfiguracion *datosSwap;
t_bitarray* bitArray;
int pagsLibres;
void* archivoSwap;
t_list* tablaPaginas;
t_log* logs;

int main(int argc, char* argv[]){							// 	PARA EJECUTAR: 						./Swap ../Config1		(o el numero de configuracion que sea)


	int	conexionUmc, swap_servidor=socket(AF_INET, SOCK_STREAM, 0);

	datosSwap=malloc(sizeof(datosConfiguracion));

	logs = log_create("Swap.log","Swap",true,log_level_from_string("INFO"));

	if (!leerConfiguracion(argv[1], &datosSwap)) {
		log_error(logs,"Error archivo de configuracion\n FIN.");
		return 1;}


	if(!crearArchivoSwap()){
		log_error(logs,"Error al crear archivo Swap\n");
	}

	if (string_equals_ignore_case(archivoSwap,"Fuiste")){log_error(logs,"Error al crear el archivo Swap\n");return 0;}

	struct sockaddr_in direccionSWAP=crearDireccion(datosSwap->puerto, datosSwap->ip);

	pagsLibres=datosSwap->cantidadPaginas;


	tablaPaginas=list_create();

	if (!bindear(swap_servidor, direccionSWAP)) {log_error(logs,"Error en el bind, Adios!\n");
		return 1;
	}

	log_info(logs,"Swap Funcionando - Esperando UMC...");
	listen(swap_servidor, 5);

	//----------------------------creo cliente para umc

	struct sockaddr_in direccionCliente;
	int sin_size = sizeof(struct sockaddr_in);

	conexionUmc= accept(swap_servidor, (void *)&direccionCliente, (void *)&sin_size);
	comprobarCliente(conexionUmc);

	log_info(logs,"UMC Conectada!\n");


	//----------------Recibo datos de la UMC

	int operacion=1, PID, cantPaginas, pagina;
	while (operacion){
		char* codOp=(char*)recibirMensaje(conexionUmc,1);
		operacion = atoi(codOp);
		free(codOp);
		if(operacion){
			PID = recibirProtocolo(conexionUmc);
			switch (operacion){

			case GUARDAR:															//Almacenar codigo, PROTOCOLO: [1° PID, 2° Cant paginas (4 bytes c/u))]
					cantPaginas = recibirProtocolo(conexionUmc);
					printf(">>>>>Cant Paginas:%d\n",cantPaginas);
					usleep(datosSwap->retardoAcceso*1000);
					if (pagsLibres>=cantPaginas){
						guardarDatos(conexionUmc,cantPaginas, PID);
						send(conexionUmc, "ok", 2, 0);
					verMarcos();}
					else {
						int tamanio = recibirProtocolo(conexionUmc);			//Recibo el programa, pero lo ignoro xq no tengo espacio
						void* datos = recibirMensaje(conexionUmc, tamanio);
						free(datos);
						send(conexionUmc,"no",2,0);
					}
					break;

			case ENVIAR:																//enviar pagina a la UMC
					usleep(datosSwap->retardoAcceso*1000);
					pagina=recibirProtocolo(conexionUmc);
					printf("ME PIDIO PROCESO:%d | Pag:%d\n",PID,pagina);
					void* datos=buscar(PID,pagina);
					if(datos==NULL){
						send(conexionUmc,"no",2,0);
					}else{
						send(conexionUmc,"ok",2,0);
						send(conexionUmc,datos,datosSwap->tamPagina,0);
						free(datos);
					}
					break;

			case ELIMINAR:																//eliminar ansisop
					printf("-----ELIMINANDO PROCESO :%d\n",PID);
					eliminarProceso(PID);
					verMarcos();
					break;
			}
		}
	}
	free(datosSwap);
	bitarray_destroy(bitArray);
	list_destroy_and_destroy_elements(tablaPaginas,free);
	munmap(archivoSwap,sizeof(archivoSwap));
	log_error(logs,"Cayó la Umc, swap autodestruida!\n");
	return 0;
}


//-----------------------------------FUNCIONES-----------------------------------

int crearArchivoSwap(){				//todo modificar tamaños
	char* instruccion=string_from_format("dd if=/dev/zero of=%s count=%d bs=%d",datosSwap->nombre_swap,datosSwap->cantidadPaginas,datosSwap->tamPagina);
	system(instruccion);
	int fd_archivo=open(datosSwap->nombre_swap,O_RDWR);
	archivoSwap=(void*) mmap(NULL ,datosSwap->cantidadPaginas*datosSwap->tamPagina,PROT_READ|PROT_WRITE,MAP_PRIVATE,(int)fd_archivo,0);
	char* archivoBitArray=(char*) mmap(NULL ,datosSwap->cantidadPaginas*datosSwap->tamPagina,PROT_READ|PROT_WRITE,MAP_PRIVATE,(int)fd_archivo,0);
	bitArray=bitarray_create((char*)archivoBitArray,datosSwap->cantidadPaginas/8);			//El bitArray usa bits, y el resto usan Bytes => /8
	if (archivoSwap==MAP_FAILED){
		return 0;}
	return 1;
}


int guardarDatos(int conexionUmc,int cantPaginas, int PID){
	int existeProceso(traductor_marco* marco){
		return (marco->proceso==PID);}

	int posicion,size;
	void* datos;

	traductor_marco* proceso=list_find(tablaPaginas,(void*)existeProceso);
	if (proceso==NULL){														//No existe el proceso => guardo el ansisop y lo registro
		traductor_marco* nuevaFila = malloc(sizeof(traductor_marco));
		int tamanio = recibirProtocolo(conexionUmc);
		datos = recibirMensaje(conexionUmc, tamanio);
		posicion = buscarEspacioLibre(cantPaginas);
		nuevaFila->inicio = posicion;
		nuevaFila->paginas = cantPaginas;
		nuevaFila->proceso = PID;
		size=tamanio;
		list_add(tablaPaginas, nuevaFila);
		pagsLibres-=cantPaginas;
		log_info(logs,"Nuevo ansisop\n");
	}

	else{												//actualizar pagina
		datos = recibirMensaje(conexionUmc, datosSwap->tamPagina);
		posicion=recibirProtocolo(conexionUmc);
		posicion+=proceso->inicio;					//donde arranca el proceso + pag
		log_info(logs,"Proceso %d | Pagina %d modificada\n",PID,posicion);
		size=datosSwap->tamPagina;
	}

	memcpy(archivoSwap + posicion*datosSwap->tamPagina, datos, size);
	free(datos);
	return 1;
}


int buscarEspacioLibre(int paginasNecesarias){							//todo debe buscar espacios CONTIGUOS o compactar
	int pos=0, i,contador=0,compactado=0;
	do{
	for(i=0;i<datosSwap->cantidadPaginas && contador<paginasNecesarias;i++){
		if (!bitarray_test_bit(bitArray,i)){
			contador++;}
		else{
			contador=0;
			pos=i+1;}
	}
	if (contador<paginasNecesarias){
		compactar();
		compactado=0;
	}else{compactado=1;}
	}while(!compactado);

	for(i=pos;i<pos+contador;i++){
		bitarray_set_bit(bitArray,i);
	}
	return pos;
}


int compactar(){
	int i,inicioAnterior, libre, cont;
	traductor_marco* procesoAMover;

	int ordenarPorInicioDeMenorAMayor(traductor_marco* marco1, traductor_marco* marco2){
		return (marco1->inicio<marco2->inicio);
	}
	int proximoProceso(traductor_marco* proceso){
		return (proceso->inicio>=i);
	}

	list_sort(tablaPaginas,(void*)ordenarPorInicioDeMenorAMayor);
	log_info(logs,"INICIANDO COMPACTACION...");
	usleep(datosSwap->retardoCompactacion*1000);
	for(i=0;i<datosSwap->cantidadPaginas;i++){
		if (!bitarray_test_bit(bitArray,i)){
				libre = i;
				// busque el proximo proceso ocupado a mover
				procesoAMover=list_find(tablaPaginas,(void*)proximoProceso);
				if (procesoAMover!=NULL){
				log_info(logs, ">>>El proceso a mover es: %d<<<", procesoAMover);
					inicioAnterior=procesoAMover->inicio;
					for (cont=0;cont<procesoAMover->paginas;cont++){
								bitarray_clean_bit(bitArray,inicioAnterior+cont);				//porque ahora va a estar vacia
								bitarray_set_bit(bitArray,libre+cont);									//Ahora va a estar ocupada
						}
			memcpy(archivoSwap+ libre * datosSwap->tamPagina, archivoSwap+inicioAnterior * datosSwap->tamPagina, procesoAMover->paginas * datosSwap->tamPagina);		//Modifique los marcos, ahora copio los datos
			// el que estaba ocupado ahora va a empezar a partir del que estaba libre para compactarlo
			// como la tablaPaginas esta compuesta por procesos, aca la estaria actualizando
			procesoAMover->inicio = libre;}
			else{i=datosSwap->cantidadPaginas;}							//No hay mas procesos para mover => salgo del ciclo, no necesito buscar mas
		}
	}
	log_info(logs,"Compactacion realizada con exito!!\n");
	return 1;
}

void* buscar(int pid, int pag){
	int proceso(traductor_marco* fila){
		return (fila->proceso==pid);
	}
	traductor_marco* datosProceso=list_find(tablaPaginas,(void*)proceso);
	if (datosProceso==NULL){
		return NULL;
	}
	void* pagina=(void*)malloc(datosSwap->tamPagina);
	int pos=(datosProceso->inicio+pag)*datosSwap->tamPagina;
	memcpy(pagina,archivoSwap+pos,datosSwap->tamPagina);
	memcpy(pagina+datosSwap->tamPagina,"\0",1);
	//printf("PAG %d - Datos:  %s-\n",pos/datosSwap->tamPagina,(char*)pagina);
	return pagina;
}

int eliminarProceso(int pid){
	int entradaDelProceso(traductor_marco* entrada){
		return (entrada->proceso==pid);}

	traductor_marco* datosProceso=list_find(tablaPaginas,(void*)entradaDelProceso);
	if(datosProceso!=NULL){
		int posicion=datosProceso->inicio;
		int i;

		for(i=0;i<datosProceso->paginas;i++){					//Marco los marcos del proceso como vacíos
			bitarray_clean_bit(bitArray,posicion+i);
		//	log_info(logs,"Cambie el bit a 0 de la posicion nro %d", posicion);
		}

		pagsLibres+=datosProceso->paginas;
		log_info(logs,"Lista antes: %d\n",list_size(tablaPaginas));
		list_remove_and_destroy_by_condition(tablaPaginas,(void*)entradaDelProceso,(void*)free);			//Elimino las entradas de la tabla
		log_info(logs, "Lista despues: %d\n",list_size(tablaPaginas));}
	return 1;
}

void verMarcos(){
	int i;
	char* marcos=string_new();
	log_info(logs,"Estado de los marcos:");
	for(i=0;i<datosSwap->cantidadPaginas;i++){
		string_append_with_format(&marcos,"%d", bitarray_test_bit(bitArray,i));
	}
		log_info(logs,"%s",marcos);
	free(marcos);
}
