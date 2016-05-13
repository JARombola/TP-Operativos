/*
 * umc.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <commons/string.h>
#include <commons/config.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/collections/node.h>
#include <commons/collections/list.h>
#include <pthread.h>

#define PUERTO_SWAP 6660
#define PUERTO_UMC 6661
#define esIgual(a,b) string_equals_ignore_case(a,b)
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra)

typedef struct{
	int puerto;
	char* ip;				//PASAR A IP CON: inet_addr() / o inet_ntoa()
	int puerto_swap;
	int marcos;
	int marco_size;
	int marco_x_proc;
	int entradas_tlb;
	int retardo;
}datosConfiguracion;


/*	FALTAN CREAR "ESTRUCTURAS" PARA: - INDICE DE CODIGO
 *  							     - INDICE DE ETIQUETAS
 *            					     - INDICE DE STACK
 */

void leerConfiguracion(char*, datosConfiguracion*);
struct sockaddr_in crearDireccion(int);
int conectar(int);
int autentificar(int);
int comprobarCliente(int);
int recibirProtocolo(int);
void* recibirMensaje(int, int);
//COMPLETAR...........................................................
void comprobarOperacion(int);
void inicializarPrograma(int PID, int cantPaginas);
void enviarBytes(int pagina, int offset, int tamanio);
void almacenarBytes(int pagina, int offset, int tamanio, int buffer);
void finalizarPrograma(int);
void atenderNucleo(int);
void atenderCpu(int);

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char* argv[]) { //SOCKETS, CONEXION, BLA...
	int TAMPAGINA=ntohl(10);				//TEMPORAL PARA PROBAR NUCLEO
	pthread_attr_t attr;
	pthread_t thread;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

	datosConfiguracion* datosMemoria=malloc(sizeof(datosConfiguracion));
	char* comando;
	int velocidad;
//	leerConfiguracion(argv[1], datosMemoria);


	//-------------------------SOCKETS
	struct sockaddr_in direccionUMC=crearDireccion(PUERTO_UMC);					//Para el bind
	int umc_servidor = socket(AF_INET, SOCK_STREAM, 0); 						//creo el descriptor con esa direccion
	printf("UMC Creada. Conectando con la Swap...\n");
	int umc_cliente = conectar(PUERTO_SWAP),
			cliente_nucleo,											//Socket del nucleo para el accept
			activado=1,
			max_desc = 0,
			nuevo_cliente,											//Recibir conexiones
			sin_size = sizeof(struct sockaddr_in),
			i, nucleoOK=0,conexionSwap=0;
	//calloc(datosMemoria.marcos,datosMemoria.marco_size);
	if (!autentificar(umc_cliente)){printf("Falló el handshake\n");return -1;}
	printf("Conexion con la Swap Ok\n");
	setsockopt(umc_servidor, SOL_SOCKET, SO_REUSEADDR, &activado,sizeof(activado)); 				//para cerrar los binds al cerrar
	if (bind(umc_servidor, (void *) &direccionUMC, sizeof(direccionUMC)) != 0) {
		perror("Fallo el bind");
		return 1;
	}
	printf("Escuchando\n");
	listen(umc_servidor, 15);

	struct sockaddr_in direccionCliente; 							//direccion donde guarde el cliente

		fd_set descriptores;
		t_list* cpus;
		cpus = list_create();
/*	if (fork()==0){						//Para que reciba los mensajes x consola
		while (1) {
				comando = string_new(), scanf("%s", comando);
				if (esIgual(comando, "retardo")) {
					printf("velocidad nueva:");
					scanf("%d", &velocidad);
					printf("Velocidad actualizada:%d\n", velocidad);
				} else {
					if (esIgual(comando, "dump")) {
						printf("Estructuras de Memoria\n");
						printf("Datos de Memoria\n");
					} else {
						if (esIgual(comando, "tlb")) {
							printf("TLB Borrada :)\n");
						} else {
							if (esIgual(comando, "memoria")) {
								printf("Memoria Limpiada :)\n");
							}
						}
					}
				}
		}
	}else{*/
	while(1){
		FD_ZERO (&descriptores);
		FD_SET(umc_cliente,&descriptores);
		FD_SET(umc_servidor,&descriptores);
		max_desc=umc_cliente;
		for(i=0; i<list_size(cpus);i++){
			int cpuset = (int)list_get(cpus,i);
			FD_SET(cpuset,&descriptores);
			if(cpuset > max_desc){ max_desc = cpuset; }
		}
		if(nucleoOK){
			FD_SET(cliente_nucleo,&descriptores);
			if(cliente_nucleo > max_desc){ max_desc = cliente_nucleo; }
		}
		if (select (max_desc+1, &descriptores, NULL, NULL, NULL) < 0){
			 	perror("Select fail");
		}

		for(i=0; i<list_size(cpus);i++){
														//CPUs que se activaron
			int unCPU = (int) list_get(cpus, i);
				if (FD_ISSET(unCPU, &descriptores)) {
					int protocolo=recibirProtocolo(unCPU);
					if (protocolo==-1) {
						perror("el cpu se desconecto o algo. Se lo elimino\n");
						list_remove(cpus, i);
					} else {
						char* bufferCpu = malloc(protocolo + 1);
						int mensaje =(int) recibirMensaje(unCPU,protocolo);
						comprobarOperacion(mensaje);
						//mando el mensaje a la swap
							int longitud = htonl(string_length(bufferCpu));
							send(umc_cliente, &longitud, sizeof(int32_t), 0);
							send(umc_cliente, bufferCpu, strlen(bufferCpu), 0);
							free(bufferCpu);
					}
			 }
		  }
		if (FD_ISSET(cliente_nucleo,&descriptores)){
			//se activo el Nucleo, me esta mandando algo (nucleoOK=0 si se desconecto)
		}
		if (FD_ISSET(umc_cliente,&descriptores)){
			//se activo la swap, me esta mandando algo
		}
		if(FD_ISSET(umc_servidor,&descriptores)){			 //aceptar cliente
			nuevo_cliente = accept(umc_servidor, (void *) &direccionCliente, (void *)&sin_size);
				if (nuevo_cliente == -1){
					perror("Fallo el accept");
				}
				printf("Recibi una conexion en %d!!\n", nuevo_cliente);
				switch (comprobarCliente(nuevo_cliente)) {
							case 0:															//Error
								perror("No lo tengo que aceptar, fallo el handshake\n");
								close(nuevo_cliente);
								break;
							case 1:
								send(nuevo_cliente,"1",1,0);									//1=CPU
								list_add(cpus, (void *)nuevo_cliente);
								printf("acepte un nuevo cpu\n");
								pthread_create(&thread,&attr,atenderCpu,nuevo_cliente);
								break;
							case 2:
								send(nuevo_cliente,&TAMPAGINA,4,0);
								cliente_nucleo = nuevo_cliente;
								nucleoOK = 1;
								printf("acepte al nucleo\n");
								pthread_create(&thread,&attr,atenderNucleo,nuevo_cliente);
								break;
							}

			}
	}
	//free(datosConfiguracion);
	return 0;
}

void leerConfiguracion(char *ruta, datosConfiguracion *datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		perror("FIN PROGRAMA");
		exit(0);
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys != 8) {
			perror("ERROR CANTIDAD DATOS DE CONFIGURACION");
		} else {
			datos->puerto = buscarInt(archivoConfiguracion, "PUERTO");
			datos->puerto_swap = buscarInt(archivoConfiguracion, "PUERTO_SWAP");
			datos->marcos = buscarInt(archivoConfiguracion, "MARCOS");
			datos->marco_size = buscarInt(archivoConfiguracion, "MARCO_SIZE");
			datos->marco_x_proc = buscarInt(archivoConfiguracion, "MARCO_X_PROC");
			datos->entradas_tlb = buscarInt(archivoConfiguracion, "ENTRADAS_TLB");
			datos->retardo = buscarInt(archivoConfiguracion, "RETARDO");
			struct sockaddr_in ipLinda;			//recurso TURBIO para guardar la ip :/
			char *direccion;
			inet_aton(config_get_string_value(archivoConfiguracion,"IP"), &ipLinda.sin_addr); //
			direccion = inet_ntoa(ipLinda.sin_addr);
			datos->ip=direccion;
			config_destroy(archivoConfiguracion);
		}
	}
}

struct sockaddr_in crearDireccion(int puerto){
	struct sockaddr_in direccion;
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr = INADDR_ANY;
	direccion.sin_port = htons(puerto);
	return direccion;
}

int conectar(int puerto){   							//Con la swap
	struct sockaddr_in direccion=crearDireccion(puerto);
	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccion, sizeof(direccion)));
	return conexion;
}

int autentificar(int conexion) {
	send(conexion, "soy_la_umc", 10, 0);
	char* bufferHandshakeSwap = malloc(10);
	int bytesRecibidosH = recv(conexion, bufferHandshakeSwap, 10, 0);
	if (bytesRecibidosH <= 0) {
		printf("Error al conectarse con Swap");
		free (bufferHandshakeSwap);
		return 0;
	}
	free (bufferHandshakeSwap);
	return 1;
}

int comprobarCliente(int nuevoCliente) {
	char* bufferHandshake = malloc(15);
	int bytesRecibidosHs = recv(nuevoCliente, bufferHandshake, 15, 0);
	bufferHandshake[bytesRecibidosHs] = '\0'; //lo paso a string para comparar
	if (strcmp("soy_un_cpu", bufferHandshake) == 0) {
		free(bufferHandshake);
		return 1;
	} else if (strcmp("soy_el_nucleo", bufferHandshake) == 0) {
		free(bufferHandshake);
		return 2;
	}
	free(bufferHandshake);
	return 0;
}

int recibirProtocolo(int conexion){
	char* protocolo = malloc(4);
	int bytesRecibidos = recv(conexion, protocolo, sizeof(int32_t), 0);
	if (bytesRecibidos <= 0) {	printf("Error al recibir protocolo\n");
	return -1;
	}
	return atoi(protocolo);}

void* recibirMensaje(int conexion, int tamanio){
	int mensaje=malloc(tamanio);
	int bytesRecibidos = recv(conexion, mensaje, tamanio, 0);
	if (bytesRecibidos != tamanio) {
		perror("Error al recibir el mensaje\n");
		return -1;}
	return mensaje;
}

void comprobarOperacion(int codigoOperacion){				//Recibe el 1er byte y lo manda acá. En cada funcion deberá recibir el resto de bytes
	switch(codigoOperacion){
	case 1:							//inicializarPrograma(); 		HACER LOS RECV NECESARIOS!
		break;
	case 2:							//enviarBytes();
		break;
	case 3:							//almacenarBytes();
		break;
	case 4:							//finalizarPrograma();
		break;
	}
}


//-----------------------------------------------OPERACIONES UMC-------------------------------------------------
void inicializarPrograma(int PID, int cantPaginas){

}

void enviarBytes(int pagina, int offset, int tamanio){

}

void almacenarBytes(int pagina, int offset, int tamanio, int buffer){

}

void finalizarPrograma(int PID){

}
//--------------------------------------------HILOS------------------------

void atenderCpu(int conexion){
	printf("Hilo de CPU creado\n");
	int i;
	for (i=0;i<2000;i++){
		usleep(1*1000);
	printf("%d\n",conexion);}
}

void atenderNucleo(int conexion){
	printf("Hilo de Nucleo creado\n");
}
