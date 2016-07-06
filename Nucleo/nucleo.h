/*
 * nucleo.h
 *
 *  Created on: 2/7/2016
 *      Author: utnso
 */

#ifndef NUCLEO_H_
#define NUCLEO_H_

#include <sys/select.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/collections/list.h>
#include <commons/log.h>
#include <parser/metadata_program.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include "Funciones/Comunicacion.h"
#include "Funciones/json.h"

#define EVENT_SIZE  ( sizeof (struct inotify_event) + 24 )
#define BUF_LEN     ( 1024 * EVENT_SIZE )

typedef struct{
	char* pcb;
	int ut;
}pcbParaES;


t_list *cpus, *consolas, *listConsolasParaEliminarPCB, *cpusDisponibles;
t_log* archivoLog;
//estructuras para planificacion
pthread_attr_t attr;
pthread_t thread;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
t_queue *colaNuevos, *colaListos,*colaTerminados;
sem_t sem_Nuevos, sem_Listos,sem_Terminado;



t_dictionary* crearDiccionarioGlobales(char** keys);
t_dictionary* crearDiccionarioSEMyES(char** keys, char** init, int esIO);

char* crearPCB(char*, int);
int calcularPaginas(char*);
void enviarAnsisopAUMC(int, char*,int);
void maximoDescriptor(int* maximo, t_list* lista, fd_set *descriptores);
void atender_Nuevos();
void atender_Ejecuciones();
void atender_Bloq_ES(int posicion);
void atender_Bloq_SEM(int posicion);
void atender_Terminados();
void atenderOperacion(int op,int cpu);
void procesar_operacion_privilegiada(int operacion, int cpu);
void sacar_socket_de_lista(t_list* lista,int socket);
int esa_consola_existe(int consola);
int ese_PCB_hay_que_eliminarlo(int consola);
int revisarActividadConsolas(fd_set*);
int revisarActividadCPUs(fd_set*);
char* serializarMensajeCPU(char* pcbListo, int quantum, int quantum_sleep);
void enviarPCBaCPU(int, char*);
void finalizarProgramaUMC(int id);
void finalizarProgramaConsola(int consola, int codigo);
void enviarTextoConsola(int consola, char* texto);
void Modificacion_quantum();
int buscar_pcb_en_bloqueados(int pid);
int buscar_pcb_en_cola(t_queue* cola, int pid);
int obtenerPID(char* pcb);

datosConfiguracion* datosNucleo;
t_dictionary *globales,*semaforos,*dispositivosES;
int tamPagina=0,*dispositivosSleeps, *globalesValores, *contadorSemaforo, conexionUMC, cantidad_io, cantidad_sem;
sem_t *semaforosES,*semaforosGlobales;
t_queue **colasES,**colasSEM;


#endif /* NUCLEO_H_ */
