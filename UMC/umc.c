/*
 * umc.c
 *
 *  Created on: 28/4/2016
 *      Author: utnso
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include <pthread.h>
#include <unistd.h>
#include "Funciones/Comunicacion.h"
#include "Funciones/Paginas.h"

#define esIgual(a,b) string_equals_ignore_case(a,b)
#define marcosTotal datosMemoria->marco_size*datosMemoria->marcos
#define INICIALIZAR 1
#define ENVIAR_BYTES 2
#define GUARDAR_BYTES 3
#define FINALIZAR 4


//COMPLETAR...........................................................

void consola();
void atenderNucleo(int);
void atenderCpu(int);
int esperarRespuestaSwap();

int inicializarPrograma(int);                    // a traves del socket recibe el PID + Cant de Paginas + Codigo
void* enviarBytes(int proceso,int pagina,int offset,int size, int operacion);
int almacenarBytes(int proceso,int pagina, int offset, int tamanio, int buffer);
int finalizarPrograma(int);
void dumpTabla(traductor_marco*);
void dumpDatos(traductor_marco*);

//-----MENSAJES----
void mostrarTablaPag(traductor_marco*);
void guardarDump(t_list* proceso);

//COMANDOS--------------

pthread_mutex_t mutexMarcos=PTHREAD_MUTEX_INITIALIZER,								// Para sincronizar busqueda de marcos libres
                mutexModificacion=PTHREAD_MUTEX_INITIALIZER,						// sincronizar comando memory
                mutexTlb=PTHREAD_MUTEX_INITIALIZER,									// Sincroniza TLB
				mutexTablaPaginas=PTHREAD_MUTEX_INITIALIZER;							// Sincroniza entradas a la tabla de paginas

t_list *tabla_de_paginas, *tablaClocks,*tlb;
int totalPaginas,conexionSwap,cantSt, *vectorMarcos;
void* memoria;
datosConfiguracion* datosMemoria;
t_log* archivoLog;
FILE* reporteDump;


int main(int argc, char* argv[]) {

    archivoLog = log_create("UMC.log", "UMC", true, log_level_from_string("INFO"));

    int nucleo,nuevo_cliente,sin_size = sizeof(struct sockaddr_in);
    tabla_de_paginas = list_create();
    pthread_attr_t attr;
    pthread_t thread;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

    datosMemoria=(datosConfiguracion*) malloc(sizeof(datosConfiguracion));
    if (!(leerConfiguracion("ConfigUMC", &datosMemoria) || leerConfiguracion("../ConfigUMC", &datosMemoria))){
        registrarError(archivoLog,"No se pudo leer archivo de Configuracion");return 1;}                                                                //El posta por parametro es: leerConfiguracion(argv[1], &datosMemoria)

    vectorMarcos=(int*) malloc(datosMemoria->marcos*sizeof(int*));
    memoria = (void*) malloc(marcosTotal);
    int j;
    for (j=0;j<datosMemoria->marcos;j++){
        vectorMarcos[j]=0;                                                            //Para la busqueda de marcos (como un bitMap)
    }
    tablaClocks=list_create();
    tlb=list_create();

    //----------------------------------------------------------------------------SOCKETS

    struct sockaddr_in direccionUMC = crearDireccion(datosMemoria->puerto_umc,datosMemoria->ip);
    struct sockaddr_in direccionCliente;
    int umc_servidor = socket(AF_INET, SOCK_STREAM, 0);

    registrarInfo(archivoLog,"UMC Creada. Conectando con la Swap...");
    conexionSwap = conectar(datosMemoria->puerto_swap, datosMemoria->ip_swap);

    //----------------------------------------------------------------SWAP

    if (!autentificar(conexionSwap)) {
        registrarError(archivoLog,"Falló el handShake");
        return -1;}

    registrarInfo(archivoLog,"Conexion con la Swap OK!");

    if (!bindear(umc_servidor, direccionUMC)) {
        registrarError(archivoLog,"Error en el bind, desaprobamos");
            return 1;
        }

    //-------------------------------------------------------------------------NUCLEO

    registrarTrace(archivoLog,"Esperando nucleo...");
    listen(umc_servidor, 1);
    nucleo=aceptarNucleo(umc_servidor,direccionCliente);

    //-------------------------------------------------------------------Funcionamiento de la UMC

    pthread_create(&thread, &attr, (void*) atenderNucleo,(void*) nucleo);                        //Hilo para atender al nucleo
    pthread_create(&thread, &attr, (void*) consola, NULL);                                        //Hilo para atender comandos
    listen(umc_servidor, 100);                                                                    //Para recibir conexiones (CPU's)
    int cpuRespuesta=htonl(datosMemoria->marco_size);
    while (1) {
        nuevo_cliente = accept(umc_servidor, (void *) &direccionCliente,(void *) &sin_size);
        if (nuevo_cliente == -1) {perror("Fallo el accept");}
        registrarTrace(archivoLog,"Conexion entrante");
        switch (comprobarCliente(nuevo_cliente)) {
        case 0:                                                            //Error
            perror("No lo tengo que aceptar, fallo el handshake");
            close(nuevo_cliente);
            break;
        case 1:
            send(nuevo_cliente, &cpuRespuesta, sizeof(int32_t), 0);                                //1=CPU
            pthread_create(&thread, &attr, (void*) atenderCpu,(void*) nuevo_cliente);
            break;
        }
    }
    free(datosMemoria);
    free(memoria);
    return 0;
}


//--------------------------------------------HILOS------------------------
void consola(){
    reporteDump=fopen("reporteDump","w");
    fclose(reporteDump);
    while (1) {
        char* comando;

        int nroProceso;
        comando = string_new(), scanf("%s", comando);
        if (esIgual(comando, "RETARDO")) {
            int velocidadNueva;
            scanf("%d", &velocidadNueva);
            char* mensaje=string_new();
            string_append(&mensaje,"Retardo nuevo:");
            string_append(&mensaje,(char*)velocidadNueva);
            registrarInfo(archivoLog,mensaje);
            //actualizar retardo en el config
            datosMemoria->retardo= velocidadNueva;
            free(mensaje);
        }
        else {
            if (esIgual(comando, "DUMP")) {
                scanf("%d",&nroProceso);
            //    int pos=buscar(6,nroProceso);

                int filtrarPorPid(traductor_marco* marco){
                    if(nroProceso==-1){return 1;}                            //Para que lo haga a todos los procesos
                    return (marco-> proceso == nroProceso);}

                // guardo en una lista nueva los que tengan el mismo pid
                t_list* nueva = list_filter(tabla_de_paginas,(void*) filtrarPorPid);
                reporteDump=fopen("reporteDump","a");

                pthread_mutex_lock(&mutexTablaPaginas);
                	guardarDump(nueva);
                pthread_mutex_lock(&mutexTablaPaginas);

                fclose(reporteDump);
                list_clean(nueva);
                printf("Dump generado\n");
            }
            else {
                if (esIgual(comando, "TLB")) {
                	pthread_mutex_lock(&mutexTlb);
                    list_clean(tlb);
                    pthread_mutex_unlock(&mutexTablaPaginas);
                    printf("TLB Limpia\n");
                }
                else {
                    if (esIgual(comando, "MODIFICADAS")) {
                       /* list_iterate(tabla_de_paginas,(void*)mostrarTablaPag);
                        finalizarPrograma(0);
                        list_iterate(tabla_de_paginas,(void*)mostrarTablaPag);*/
                        scanf("%d",&nroProceso);//todo
                        void comandoMemory(traductor_marco* pagina){
                        	if (nroProceso==-1){ pagina->modificada=1;}
                        	else{
                        		if(pagina->proceso==nroProceso)pagina->modificada=1;
                        	}
                        }
                        pthread_mutex_lock(&mutexTablaPaginas);							// Por las dudas que el nucleo justo las estuviera tocando
                        	pthread_mutex_lock(&mutexModificacion);
                        		list_iterate(tabla_de_paginas,(void*)comandoMemory);
                        	pthread_mutex_lock(&mutexTablaPaginas);
                        pthread_mutex_lock(&mutexModificacion);

                        printf("Paginas modificadas (Proceso: %d)\n",nroProceso);
                    }
                }
            }
        }
    }
}


void guardarDump(t_list* proceso){
    int i;
    int menorMayorMarco(traductor_marco* marco1, traductor_marco* marco2){
    	return (marco1->marco<marco2->marco);
    }
    list_sort(proceso,(void*)menorMayorMarco);

    fprintf(reporteDump,"___TABLA DE PAGINAS___\n");
    for(i=0;i<list_size(proceso);i++){
        traductor_marco* datosProceso=list_get(proceso,i);
        dumpTabla(datosProceso);
    }
    fprintf(reporteDump,"___Datos___\n");
    for(i=0;i<list_size(proceso);i++){
        traductor_marco* datosProceso=list_get(proceso,i);
        dumpDatos(datosProceso);
    }
}

void dumpTabla(traductor_marco* datosProceso){
    fprintf(reporteDump,"Proceso: %d    |    Pág: %d    |    Marco: %d\n",datosProceso->proceso,datosProceso->pagina,datosProceso->marco);
}

void dumpDatos(traductor_marco* datosProceso){
    if(datosProceso->marco!=-1){
    	int numeros(traductor_marco* traductor){
    		return (traductor->proceso==datosProceso->proceso);
    	}
    	if (datosProceso->pagina>=list_count_satisfying(tabla_de_paginas,(void*)numeros)-cantSt){
    		void* datos=malloc(sizeof(int));
    		int i=0;
    		fprintf(reporteDump,"Marco: %d - [",datosProceso->marco);

    		for(i=0;i<datosMemoria->marco_size%4;i++){
    			memcpy(&datos,memoria+datosProceso->marco*datosMemoria->marco_size+i*4,sizeof(int));
    			fprintf(reporteDump,"%d ",datos);}

    		fprintf(reporteDump,"]\n");
    	}else{
    	void* datos=malloc(datosMemoria->marco_size);
    	memcpy(datos,memoria+datosProceso->marco*datosMemoria->marco_size,datosMemoria->marco_size);
   	    fprintf(reporteDump,"Marco: %d",datosProceso->marco);
   	    fprintf(reporteDump,"[%s]\n",datos);}
    }
}

void mostrarTablaPag(traductor_marco* fila) {
    printf("Marco: %d, Pag: %d, Proc:%d", fila->marco, fila->pagina,fila->proceso);
    char* asd = malloc(datosMemoria->marco_size + 1);
    memcpy(asd, memoria + datosMemoria->marco_size * fila->pagina,datosMemoria->marco_size);//memcpy(asd, memoria + datosMemoria->marco_size * fila->marco,datosMemoria->marco_size);
    memcpy(asd + datosMemoria->marco_size , "\0", 1);
    printf("-[%s]\n", asd);
    free(asd);
}

void atenderCpu(int conexion){
	registrarTrace(archivoLog, "Nuevo CPU-");
	int salir = 0, operacion, proceso, pagina, offset, buffer, size,procesoAnterior=-1;
	int removerEntradasProcesoAnterior(traductor_marco* entradaTlb){
		return entradaTlb->proceso!=procesoAnterior;
	}
	void* datos;
	while (!salir) {
		operacion = atoi(recibirMensaje(conexion, 1));
		if (operacion) {

			proceso = recibirProtocolo(conexion);
			pagina = recibirProtocolo(conexion);
			offset = recibirProtocolo(conexion);
			size=recibirProtocolo(conexion);
			switch (operacion) {

			case ENVIAR_BYTES:													//2 = Enviar Bytes (busco pag, y devuelvo el valor)
				datos=enviarBytes(proceso,pagina,offset,size,operacion);
				if (string_equals_ignore_case(datos,"-1")){
					datos=string_repeat('@',size);}							//NO DEBERIA PASAR NUNCA ESTO
				send(conexion,datos,size,0);
				free(datos);
				break;
			case GUARDAR_BYTES:													//3 = Guardar Valor
				recv(conexion,&buffer,sizeof(int),MSG_WAITALL);
				buffer=ntohl(buffer);
				char* resp;
				if (almacenarBytes(proceso,pagina,offset,size,buffer)==-1){						//La pag no existe
					resp=header(0);
				}else{resp=header(1);}
					send(conexion,resp,string_length(resp),0);
				free(resp);
				break;
			}
			if (procesoAnterior!=proceso){							//cambió el proceso, limpio la tlb
				pthread_mutex_lock(&mutexTlb);
					tlb=list_filter(tlb,(void*)removerEntradasProcesoAnterior);					//filtra las que son DIFERENTES
				pthread_mutex_lock(&mutexTlb);
				procesoAnterior=proceso;
			}
		} else {salir = 1;}
	}
	registrarWarning(archivoLog, "Se desconectó una CPU\n");
}


void atenderNucleo(int nucleo){
    registrarInfo(archivoLog,"Hilo de Nucleo creado");
        int salir=0,guardar,procesoEliminar;
        while (!salir) {
            int operacion = atoi(recibirMensaje(nucleo,1));
                if (operacion) {
                    switch (operacion) {

                    case INICIALIZAR:                                                //inicializar programa
                        guardar=inicializarPrograma(nucleo);
                        if (!guardar){                            //1 = hay marcos (cola ready), 2 = no hay marcos (cola new)
                            registrarWarning(archivoLog,"Ansisop rechazado, memoria insuficiente");}
                            guardar=htonl(guardar);
                        send(nucleo, &guardar,sizeof(int),0);
                    break;

                    case FINALIZAR:                                                //Finalizar programa
                        procesoEliminar=recibirProtocolo(nucleo);
                        if(finalizarPrograma(procesoEliminar)){
                        printf("Proceso %d eliminado\n",procesoEliminar);}
                    break;
                    }
                }else{salir=1;}
        }
        registrarWarning(archivoLog,"Nucleo desconectado\n");
        registrarWarning(archivoLog, "Desconectando UMC...\n");
        list_destroy_and_destroy_elements(tabla_de_paginas,free);
        void eliminarClock(unClock* clock){
        	queue_clean(clock->colaMarcos);
        	free(clock);
        }
        list_destroy_and_destroy_elements(tablaClocks,(void*)eliminarClock);
        free(datosMemoria);
        exit(0);
}





//-----------------------------------------------OPERACIONES UMC-------------------------------------------------

int inicializarPrograma(int conexion) {
    int PID=recibirProtocolo(conexion);                            //PID + PaginasNecesarias
    int paginasNecesarias=recibirProtocolo(conexion);
    int espacio_del_codigo = recibirProtocolo(conexion);
    char* codigo = recibirMensaje(conexion, espacio_del_codigo);            //CODIGO
    //printf("Codigo: %s-\n",codigo);
    agregarHeader(&codigo);
    char* programa = string_new();
    string_append(&programa, "1");
    string_append(&programa, header(PID));
    string_append(&programa, header(paginasNecesarias));
    string_append(&programa, codigo);
    string_append(&programa, "\0");
    free(codigo);
    send(conexionSwap, programa, string_length(programa), 0);
    free(programa);
    int aceptadoSwap= esperarRespuestaSwap();
    if (!aceptadoSwap){
        return 0;
    }
    printf("Ansisop guardado\n");
    int i;
    pthread_mutex_lock(&mutexTablaPaginas);
    for (i = 0; i < paginasNecesarias; i++) {				//Entradas en la tabla, marco -1 (=> está en Swap)
          actualizarTabla(i, PID, -1);
    }
    pthread_mutex_unlock(&mutexTablaPaginas);
    if (hayMarcosLibres()){
        return 1;
    }
    return 2;
}


void* enviarBytes(int proceso,int pagina,int offset,int size,int op){
    int posicion=buscar(proceso, pagina);
    if (posicion!=-1){
        void* datos=(void*) malloc(size);
        memcpy(datos,memoria+posicion+offset,size);
        void* a=(void*)malloc(size+1);
        memcpy(a,datos,size);
        memcpy(a+size,"\0",1);
        printf("Pag: %d -> Envio: %s\n",pagina,a);					//todo esto se va
        free(a);
        return datos;//}
    }
    char* mje=string_new();
    string_append(&mje,"-1\0");
    return mje;                //No existe la pag
}


int almacenarBytes(int proceso, int pagina, int offset, int size, int buffer){
    int buscarMarco(traductor_marco* fila){
            return (fila->proceso==proceso && fila->pagina==pagina);}

    int posicion=buscar(proceso,pagina);
    if(posicion==-1){                                //no existe la pagina
        return -1;
    }

    posicion+=offset;

    pthread_mutex_lock(&mutexTablaPaginas);
		memcpy(memoria+posicion,&buffer,size);
		traductor_marco* datosTabla=list_find(tabla_de_paginas,(void*)buscarMarco);
		datosTabla->modificada=1;
		printf("Pagina modificada\n");
    pthread_mutex_unlock(&mutexTablaPaginas);

    void* a=malloc(4);							//todo esto se va
    memcpy(&a,memoria+posicion,4);
    printf("Guardé: %d\n",a);

    return posicion;
}


int finalizarPrograma(int procesoEliminar){
    int paginasDelProceso(traductor_marco* entradaTabla){
        return (entradaTabla->proceso==procesoEliminar);}

    void limpiar(traductor_marco* marco){
    	if(marco->marco!=-1){
       	vectorMarcos[marco->marco]=0;}
        list_remove_and_destroy_by_condition(tabla_de_paginas,(void*)paginasDelProceso,free);}

    int clockDelProceso(unClock* clockDelProceso){
        return(clockDelProceso->proceso==procesoEliminar);}

    printf("[Antes] Paginas: %d | Clocks: %d | TLB:%d\n",list_size(tabla_de_paginas),list_size(tablaClocks),list_size(tlb));

    pthread_mutex_lock(&mutexTlb);
    	list_remove_by_condition(tlb,(void*)paginasDelProceso);
    pthread_mutex_unlock(&mutexTlb);

    pthread_mutex_lock(&mutexMarcos);									//Porque quizá algunos quedan libres ahora
    	pthread_mutex_lock(&mutexTablaPaginas);								//Voy a eliminar entradas de la tabla
    		list_iterate(tabla_de_paginas,(void*)limpiar);
    	pthread_mutex_unlock(&mutexTablaPaginas);
    pthread_mutex_unlock(&mutexMarcos);

    unClock* clockProceso=list_remove_by_condition(tablaClocks,(void*)clockDelProceso);
    if (clockProceso!=NULL){
        queue_clean(clockProceso->colaMarcos);
        free(clockProceso);
    }

    printf("[Despues] Paginas: %d | Clocks: %d | TLB: %d\n",list_size(tabla_de_paginas),list_size(tablaClocks),list_size(tlb));

    char* mensajeEliminar=string_new();
    string_append(&mensajeEliminar,"3");
    string_append(&mensajeEliminar,header(procesoEliminar));
    string_append(&mensajeEliminar,"\0");
    send(conexionSwap,mensajeEliminar,string_length(mensajeEliminar),0);
    free(mensajeEliminar);

    return 1;
}


int esperarRespuestaSwap(){
    char *respuesta = malloc(3);
    recv(conexionSwap, respuesta, 2, MSG_WAITALL);
    respuesta[2] = '\0';
    int aceptado = esIgual(respuesta, "ok");
    free(respuesta);
    return aceptado;
}
