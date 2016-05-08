/*
 * nucleo.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <commons/string.h>
#include <commons/config.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <commons/collections/list.h>
#include <parser/metadata_program.h>

#define PUERTO_UMC 6661
#define PUERTO_NUCLEO 6662
#define buscarInt(archivo,palabra) config_get_int_value(archivo, palabra) 	//MACRO

typedef struct {
	int puerto_prog;
	int puerto_cpu;
	int quantum;
	int quantum_sleep;
	char** sem_ids;
	char** sem_init;		//TRANSFORMAR CON (atoi)
	char** io_ids;
	char** io_sleep;		//LO MISMO
	char** shared_vars;
} datosConfiguracion;

typedef struct {
	int PID;
	int PC;
	int SP;
	int pagsCodigo;
	int indiceCodigo;
	int indiceEtiquetas;
} pcb;

typedef struct {
	int inicio;
	int offset;
} sentencia;

int autentificar(int);
void leerConfiguracion(char*, datosConfiguracion*);
struct sockaddr_in crearDireccion(int puerto);
int conectarUMC(int);
int comprobarCliente(int);
void manejarPCB(char*);
char* armarLiteral(FILE*);
int instruccion(char*);
t_list* crearIndiceDeCodigo(t_metadata_program*);
void mostrar(int*);

int main(int argc, char* argv[]) {
	char* literal;
//	datosConfiguracion datosMemoria=malloc(sizeof(datosConfiguracion));
//	leerConfiguracion(argv[0], &datosMemoria);
//	manejarPCB("/home/utnso/tp-2016-1c-CodeBreakers/Consola/Nuevo");
	int nucleo_servidor = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in direccionNucleo = crearDireccion(PUERTO_NUCLEO);
	printf("Nucleo creado, conectando con la UMC...\n");
	int conexionUMC = conectarUMC(PUERTO_UMC);
	if (!autentificar(conexionUMC)) {
		printf("Falló el handshake\n");
		return -1;
	}
	printf("Aceptados por la umc\n");

	int activado = 1;
	setsockopt(nucleo_servidor, SOL_SOCKET, SO_REUSEADDR, &activado,
			sizeof(activado));
	if (bind(nucleo_servidor, (void *) &direccionNucleo,
			sizeof(direccionNucleo))) {
		perror("Fallo el bind");
		return 1;
	}

	printf("Estoy escuchando\n");
	listen(nucleo_servidor, 15);

	//ahora creo el select
	fd_set descriptores;
	int nuevo_cliente;
	t_list* cpus, *consolas;
	cpus = list_create();
	consolas = list_create();
	int max_desc = conexionUMC;
	struct sockaddr_in direccionCliente;	//direccion donde guarde el cliente
	int sin_size = sizeof(struct sockaddr_in);
	int i;

	while (1) {
		FD_ZERO(&descriptores);
		FD_SET(nucleo_servidor, &descriptores);
		FD_SET(conexionUMC, &descriptores);
		max_desc = conexionUMC;
		for (i = 0; i < list_size(consolas); i++) {
			int conset = (int) list_get(consolas, i); //conset = consola para setear
			FD_SET(conset, &descriptores);
			if (conset > max_desc) {
				max_desc = conset;
			}
		}
		for (i = 0; i < list_size(cpus); i++) {
			int cpuset = (int) list_get(cpus, i);
			FD_SET(cpuset, &descriptores);
			if (cpuset > max_desc) {
				max_desc = cpuset;
			}
		}

		if (select(max_desc + 1, &descriptores, NULL, NULL, NULL) < 0) {
			perror("Error en el select");
			exit(EXIT_FAILURE);
		}
		for (i = 0; i < list_size(consolas); i++) {
			//entro si una consola me mando algo
			int unaConsola = (int) list_get(consolas, i);
			if (FD_ISSET(unaConsola, &descriptores)) {
				printf("se activo la consola %d\n", unaConsola);
				int protocoloC = 0; //donde quiero recibir y cantidad que puedo recibir
				int bytesRecibidosC = recv(unaConsola, &protocoloC,
						sizeof(int32_t), 0);
				protocoloC = ntohl(protocoloC);
				if (bytesRecibidosC <= 0) {
					perror("la consola se desconecto o algo. Se la elimino\n");
					list_remove(consolas, i);
				} else {
					char* bufferC = malloc(protocoloC * sizeof(char) + 1);
					bytesRecibidosC = recv(unaConsola, bufferC, protocoloC, 0);
					bufferC[protocoloC + 1] = '\0'; //para pasarlo a string (era un stream)
					printf("cliente: %d, me llegaron %d bytes con %s\n",
							unaConsola, bytesRecibidosC, bufferC);
					//mando mensaje a los CPUs
					for (i = 0; i < list_size(cpus); i++) {
						//ver los clientes que recibieron informacion
						int unCPU = list_get(cpus, i);
						int longitud = htonl(string_length(bufferC));
						send(unCPU, &longitud, sizeof(int32_t), 0);
						send(unCPU, bufferC, strlen(bufferC), 0);
					}
					free(bufferC);
				}
			}
		}
		for (i = 0; i < list_size(cpus); i++) {
			//que cpu me mando informacion
			int unCPU = (int) list_get(cpus, i);
			if (FD_ISSET(unCPU, &descriptores)) {
				printf("se activo el cpu %d\n", unCPU);
				int protocoloCPU = 0; //donde quiero recibir y cantidad que puedo recibir
				int bytesRecibidosCpu = recv(unCPU, &protocoloCPU,
						sizeof(int32_t), 0);
				protocoloCPU = ntohl(protocoloCPU);
				if (bytesRecibidosCpu <= 0) {
					perror("el cpu se desconecto o algo. Se lo elimino\n");
					list_remove(cpus, i); //no entendi de la funcion de commons: list_remove_and_destroy_element, el parametro: void(*element_destroyer)(void*)
				} else {
					char* bufferCpu = malloc(protocoloCPU * sizeof(char) + 1);
					bytesRecibidosCpu = recv(unCPU, bufferCpu, protocoloCPU, 0);
					bufferCpu[protocoloCPU + 1] = '\0'; //para pasarlo a string (era un stream)
					printf("cliente: %d, me llegaron %d bytes con %s\n", unCPU,
							bytesRecibidosCpu, bufferCpu);
					free(bufferCpu);
				}
			}
		}

		if (FD_ISSET(conexionUMC, &descriptores)) {
			//se activo la UMC, me esta mandando algo
		}

		if (FD_ISSET(nucleo_servidor, &descriptores)) { //aceptar cliente
			nuevo_cliente = accept(nucleo_servidor, (void *) &direccionCliente,
					(void *) &sin_size);
			if (nuevo_cliente == -1) {
				perror("Fallo el accept");
			}
			printf("Recibi una conexion en %d!!\n", nuevo_cliente);
			int puertoumc;
			switch (comprobarCliente(nuevo_cliente)) {
			case 0:														//Error
				perror("No lo tengo que aceptar, fallo el handshake\n");
				close(nuevo_cliente);
				break;
			case 1:
				send(nuevo_cliente, "1", 1, 0);
				list_add(cpus, (void *) nuevo_cliente);
				printf("acepte un nuevo cpu\n");
				break;
			case 2:
				send(nuevo_cliente, "1", 1, 0);
				list_add(consolas, (void *) nuevo_cliente);			//consola
				printf("acepte una nueva consola\n");
				break;
			}
		}
	}
	//free(datosMemoria);
	return 0;
}

//--------------------------------------LECTURA CONFIGURACION

void leerConfiguracion(char *ruta, datosConfiguracion *datos) {
	t_config* archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	if (archivoConfiguracion == NULL) {
		perror("Faltó Ruta CONFIGURACION");
		exit(0);
	} else {
		int cantidadKeys = config_keys_amount(archivoConfiguracion);
		if (cantidadKeys != 9) {
			perror("ERROR CANTIDAD DATOS DE CONFIGURACION");
		} else {
			datos->puerto_prog = buscarInt(archivoConfiguracion, "PUERTO_PROG");
			datos->puerto_cpu = buscarInt(archivoConfiguracion, "PUERTO_CPU");
			datos->quantum = buscarInt(archivoConfiguracion, "QUANTUM");
			datos->quantum_sleep = buscarInt(archivoConfiguracion,
					"QUANTUM_SLEEP");
			datos->sem_ids = config_get_array_value(archivoConfiguracion,
					"SEM_ID");
			datos->sem_init = config_get_array_value(archivoConfiguracion,
					"SEM_INIT");
			datos->io_ids = config_get_array_value(archivoConfiguracion,
					"IO_ID");
			datos->io_sleep = config_get_array_value(archivoConfiguracion,
					"IO_SLEEP");
			datos->shared_vars = config_get_array_value(archivoConfiguracion,
					"SHARED_VARS");
			config_destroy(archivoConfiguracion);
		}
	}
}
struct sockaddr_in crearDireccion(int puerto) {
	struct sockaddr_in direccion;
	direccion.sin_family = AF_INET;
	direccion.sin_addr.s_addr = INADDR_ANY;
	direccion.sin_port = htons(puerto);
	return direccion;
}

int conectarUMC(int puerto) {
	struct sockaddr_in direccionUMC = crearDireccion(puerto);
	int conexion = socket(AF_INET, SOCK_STREAM, 0);
	while (connect(conexion, (void*) &direccionUMC, sizeof(direccionUMC)))
		;
	return conexion;
}

int autentificar(int conexion) {
	send(conexion, "soy_el_nucleo", 13, 0);
	char* bufferHandshakeCli = malloc(8);
	int bytesRecibidosH = recv(conexion, bufferHandshakeCli, 8, 0);
	if (bytesRecibidosH <= 0) {
		printf("Rechazado por la UMC\n");
		free(bufferHandshakeCli);
		return 0;
	}
	free(bufferHandshakeCli);
	return 1;
}

int comprobarCliente(int nuevoCliente) {
	char* bufferHandshake = malloc(16);
	int bytesRecibidosHs = recv(nuevoCliente, bufferHandshake, 15, 0);
	bufferHandshake[bytesRecibidosHs] = '\0'; //lo paso a string para comparar
	if (!strcmp("soy_un_cpu", bufferHandshake)) {
		free(bufferHandshake);
		return 1;
	} else if (!strcmp("soy_una_consola", bufferHandshake)) {
		free(bufferHandshake);
		return 2;
	} else {
		free(bufferHandshake);
		return 0;
	}
}
//----------------------------------------PCB------------------------------------------------------
void manejarPCB(char* ruta) {
	pcb pcbProceso;
	FILE* archivo = fopen(ruta, "r");
	char* codigo = armarLiteral(archivo);				//El codigo del programa
	printf("%s", codigo);
	t_metadata_program *metadata = metadata_desde_literal(codigo);
	pcbProceso.PC = metadata->instruccion_inicio;
	t_list* indiceCodigo = crearIndiceDeCodigo(metadata);
	list_iterate(indiceCodigo, (void*) mostrar);//Ver inicio y offset de cada sentencia
	pcbProceso.pagsCodigo = metadata->instrucciones_size;//!!!!!!!!!!!!!!!!!!!Hay que dividir por cantidad de paginas!!!!!!

}

char* armarLiteral(FILE* archivoCodigo) {		//Copia el codigo ansisop
	char unaLinea[200], *codigoTotal;
	codigoTotal = string_new();
	while (!feof(archivoCodigo)) {
		fgets(unaLinea, 200, archivoCodigo);
		string_append(&codigoTotal, unaLinea);
	}
	return codigoTotal;
}

int instruccion(char* linea) {//*******Podria usarse la funcion del Parser: LlamasSinRetorno
	if ((strcmp(_string_trim(linea), "begin") == 0)
			|| (strcmp(_string_trim(linea), "begin\n") == 0)) {	//No se si tiene que ignorar el begin... :/
		return 0;
	}
	if (_string_trim(linea)[0] == '#') {
		return 0;
	}
	return 1;
}

t_list* crearIndiceDeCodigo(t_metadata_program* meta) {
	t_list* lineas = list_create();
	t_intructions* unaInstruccion = meta->instrucciones_serializado;
	printf("\ninstrucciones:%d\n", meta->instrucciones_size); //=5
	int i;
	for (i = 0; i < (meta->instrucciones_size); i++, unaInstruccion++) {
		int* unaLinea = malloc(sizeof(int*));				//0=inicio, 1=offset
		unaLinea[0] = unaInstruccion->start;
		unaLinea[1] = unaInstruccion->offset;
		list_add(lineas, (int*) unaLinea);			//Guarda el PUNTERO al int
	}
	return lineas;
	list_clean(lineas);
	list_destroy(lineas);
}

void mostrar(int* sentencia) {
	printf("Inicio:%d | Offset:%d\n", sentencia[0], sentencia[1]);
}

