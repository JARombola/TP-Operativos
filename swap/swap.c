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

typedef struct{
	int proceso,inicio,paginas;
}traductor_marco;


int crearArchivoSwap();
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
	//verMarcos();
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
///////////////////////////////
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
		char* codOp=(char*)recibirMensaje(conexionUmc,1);
		operacion = atoi(codOp);
		free(codOp);
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
				free(datos);
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

int crearArchivoSwap(){				//todo modificar tamaños
	char* instruccion=string_from_format("dd if=/dev/zero of=%s count=100 bs=100",datosSwap->nombre_swap,datosSwap->cantidadPaginas,datosSwap->tamPagina);
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
		posicion=recibirProtocolo(conexionUmc);
		posicion+=proceso->inicio;					//donde arranca el proceso + pag
		printf("Pagina modificada\n");
		size=datosSwap->tamPagina;
	}
	//memcpy(donde voy a guardar, que es lo que voy a guardar, tamanio)
	memcpy(archivoSwap + posicion*datosSwap->tamPagina, datos, size);
	/*void* p=malloc(size+1);				//Para comprobar lo que guardó
	memcpy(p,archivoSwap+posicion*datosSwap->tamPagina,size);
	memcpy(p+size,"\0",1);
	printf("%s\n",p);
	free(p);*/
	free(datos);
	return 1;
}


int buscarEspacioLibre(int cantPaginas){							//todo debe buscar espacios CONTIGUOS o compactar
	int pos=0, i,contador=0,compactado=0;
	do{
	for(i=0;i<datosSwap->cantidadPaginas && contador<cantPaginas;i++){
		if (!bitarray_test_bit(bitArray,i)){
			contador++;}
		else{
			contador=0;
			pos=i+1;}
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
	int i,inicioAnterior;
	traductor_marco* procesoAMover;
	int libre;
	int cont;
	int inicioMenorMayor(traductor_marco* marco1, traductor_marco* marco2){
		return (marco1->inicio<marco2->inicio);
	}
	int proximoProceso(traductor_marco* proceso){
		return (proceso->inicio>=i);
	}

	list_sort(tablaPaginas,(void*)inicioMenorMayor);

	usleep(datosSwap->retardoCompactacion*1000);
	for(i=0;i<datosSwap->cantidadPaginas;i++){
		if (!bitarray_test_bit(bitArray,i)){
				libre = i;
				//printf("libre %d\n", libre);
				// busque el proximo proceso ocupado a mover
				procesoAMover=list_find(tablaPaginas,(void*)proximoProceso);
				if (procesoAMover!=NULL){
					inicioAnterior=procesoAMover->inicio;
					for (cont=0;cont<procesoAMover->paginas;cont++){
								bitarray_clean_bit(bitArray,inicioAnterior+cont);				//porque ahora va a estar vacia
								bitarray_set_bit(bitArray,libre+cont);									//Ahora va a estar ocupada
								//printf("modifique el bitarray\n");
						}
			memcpy(archivoSwap+ libre * datosSwap->tamPagina, archivoSwap+inicioAnterior * datosSwap->tamPagina, procesoAMover->paginas * datosSwap->tamPagina);		//Modifique los marcos, ahora copio los datos
			// el que estaba ocupado ahora va a empezar a partir del que estaba libre para compactarlo
			// como la tablaPaginas esta compuesta por procesos, aca la estaria actualizando
			procesoAMover->inicio = libre;}
			else{i=datosSwap->cantidadPaginas;}							//No hay mas procesos para mover => salgo del for
		}
	}
	return 1;
}

void* buscar(int pid, int pag){
	int proceso(traductor_marco* fila){
		return (fila->proceso==pid);
	}
	traductor_marco* datosProceso=list_find(tablaPaginas,(void*)proceso);
	void* pagina=(void*)malloc(datosSwap->tamPagina);
	int pos=(datosProceso->inicio+pag)*datosSwap->tamPagina;
	memcpy(pagina,archivoSwap+pos,datosSwap->tamPagina);
	memcpy(pagina+datosSwap->tamPagina,"\0",1);
	printf("PAG %d - Datos:  %s-\n",pos/datosSwap->tamPagina,(char*)pagina);
	return pagina;
}

int eliminarProceso(int pid){
	int entradaDelProceso(traductor_marco* entrada){
//		printf("----%d\n",entrada->proceso);
		return (entrada->proceso==pid);}
	void eliminarEntrada(traductor_marco* entrada){
//		printf("eliminada\n");
		free(entrada);
	}
	traductor_marco* datosProceso=list_find(tablaPaginas,(void*)entradaDelProceso);
	int posicion=datosProceso->inicio;
	int i;
	for(i=0;i<datosProceso->paginas;i++){
		bitarray_clean_bit(bitArray,posicion);
		posicion++;
	}
	//free(datosProceso);
	pagsLibres+=datosProceso->paginas;
	printf("Lista antes: %d\n",list_size(tablaPaginas));
	list_remove_and_destroy_by_condition(tablaPaginas,(void*)entradaDelProceso,(void*)eliminarEntrada);
	printf("Lista despues: %d\n",list_size(tablaPaginas));
	return 1;
}

void verMarcos(){
	int i;
	printf("Estado de los marcos: \n");
	for(i=0;i<datosSwap->cantidadPaginas;i++){
		printf("Pos %d | Ocupado:%d\n",i,bitarray_test_bit(bitArray,i));
	}
}
