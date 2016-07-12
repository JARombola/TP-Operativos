
#include "umc.h"

#define esIgual(a,b) string_equals_ignore_case(a,b)
#define marcosTotal datosMemoria->marco_size*datosMemoria->marcos
#define INICIALIZAR 1
#define ENVIAR_BYTES 2
#define GUARDAR_BYTES 3
#define FINALIZAR 4



int main(int argc, char* argv[]) {								//!!!!!	PARA EJECUTAR: 						./UMC ../Config1		(o el numero de configuracion que sea)

	archivoLog = log_create("UMC.log", "UMC", true, log_level_from_string("INFO"));

    int nucleo,nuevo_cliente,sin_size = sizeof(struct sockaddr_in);
    tabla_de_paginas = list_create();
    pthread_attr_t attr;
    pthread_t thread;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);


    datosMemoria=(datosConfiguracion*) malloc(sizeof(datosConfiguracion));
    if (!leerConfiguracion(argv[1], &datosMemoria)){
        log_error(archivoLog,"No se pudo leer archivo de Configuracion");
        return 1;}                                                                //El posta por parametro es: leerConfiguracion(argv[1], &datosMemoria)

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
    log_info(archivoLog,"--------------------------------------------------------------------------------------");
    log_info(archivoLog,"UMC Creada. Conectando con la Swap...");
    conexionSwap = conectar(datosMemoria->puerto_swap, datosMemoria->ip_swap);

    //----------------------------------------------------------------SWAP

    if (!autentificar(conexionSwap)) {
        log_error(archivoLog,"Falló el handShake");
        return -1;}

    log_info(archivoLog,"Conexion con la Swap OK!");

    if (!bindear(umc_servidor, direccionUMC)) {
        log_error(archivoLog,"Error en el bind\n");
            return 1;
        }

    //-------------------------------------------------------------------------NUCLEO

    listen(umc_servidor, 100);
    nucleo=aceptarNucleo(umc_servidor,direccionCliente);
    //-------------------------------------------------------------------Funcionamiento de la UMC
    procesos=dictionary_create();
//    procesosActivos=list_create();




    pthread_create(&thread, &attr, (void*) atenderNucleo,(void*) nucleo);                        //Hilo para atender al nucleo
    pthread_create(&thread, &attr, (void*) consola, NULL);                                        //Hilo para atender comandos
    listen(umc_servidor, 100);                                                                    //Para recibir conexiones (CPU's)

    int cpuRespuesta=htonl(datosMemoria->marco_size);
    while (1) {
        nuevo_cliente = accept(umc_servidor, (void *) &direccionCliente,(void *) &sin_size);
        switch (comprobarCliente(nuevo_cliente)) {
        case 0:                                                            //Error
            perror("No lo tengo que aceptar, fallo el handshake");
            close(nuevo_cliente);
            break;
        case 1:
            send(nuevo_cliente, &cpuRespuesta, sizeof(int32_t), 0);                                //1=CPU
            pthread_create(&thread, 0, (void*) atenderCpu,(void*) nuevo_cliente);
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
        comando=string_new();
        scanf("%s", comando);
        log_info(archivoLog,"[[[[[[[[[[[[[[[	COMANDO INTRODUCIDO: %s	]]]]]]]]]]]]]]]",comando);
        if (esIgual(comando, "RETARDO")) {
            int velocidadNueva;
            scanf("%d", &velocidadNueva);
            log_info(archivoLog,">>>>>>>>>> Retardo nuevo: %d\n",velocidadNueva);
            //actualizar retardo en el config
            datosMemoria->retardo= velocidadNueva;
        }
        else {
            if (esIgual(comando, "DUMP")) {
                scanf("%d",&nroProceso);
                int filtrarPorPid(traductor_marco* marco){
                    if(nroProceso==-1){return 1;}                            //Para que lo haga a todos los procesos
                    return (marco-> proceso == nroProceso);}

                // guardo en una lista nueva los que tengan el mismo pid
                t_list* nueva = list_filter(tabla_de_paginas,(void*) filtrarPorPid);
                reporteDump=fopen("reporteDump","a");

          //     pthread_mutex_lock(&mutexTablaPaginas);
                	guardarDump(nueva);
          //      pthread_mutex_unlock(&mutexTablaPaginas);

                fclose(reporteDump);
                list_clean(nueva);
                log_info(archivoLog,">>>>>>>>>> Reporte dump generado\n");
            }
            else {
                if (esIgual(comando, "TLB")) {
                	log_info(archivoLog,">>>>>>>>>> TLB Antes: %d",list_size(tlb));
                    list_clean(tlb);
                    log_info(archivoLog,">>>>>>>>>> TLB Después: %d\n",list_size(tlb));
                }
                else {
                    if (esIgual(comando, "MODIFICADAS")) {
                        scanf("%d",&nroProceso);
                        void marcarModificadas(traductor_marco* pagina){
                        	if (nroProceso==-1 && pagina->marco>=0){ pagina->modificada=1;}
                        	else{
                        		if(pagina->proceso==nroProceso && pagina->marco>=0)pagina->modificada=1;
                        	}
                        }
                        list_iterate(tabla_de_paginas,(void*)marcarModificadas);

                        log_info(archivoLog,">>>>>>>>>> (Proceso: %d) Páginas Modificadas\n",nroProceso);
                    }
                }
            }
        }

    }
}


void guardarDump(t_list* proceso){
    int i;
    int menorMayorMarco(traductor_marco* marco1, traductor_marco* marco2){
    	return (marco1->marco<=marco2->marco);
    }
    list_sort(proceso,(void*)menorMayorMarco);

    fprintf(reporteDump,"//////////// -- TABLA DE PAGINAS -- \\\\\\\\\\\\\\\\\\\\\n");
    for(i=0;i<list_size(proceso);i++){
        traductor_marco* datosProceso=list_get(proceso,i);
        dumpTabla(datosProceso);
    }
    fprintf(reporteDump,"////////////-- DATOS --\\\\\\\\\\\\\\\\\\\\\n");
    for(i=0;i<list_size(proceso);i++){
        traductor_marco* datosProceso=list_get(proceso,i);
        dumpDatos(datosProceso);
    }
}

void dumpTabla(traductor_marco* datosProceso){
    fprintf(reporteDump,"Proceso: %d		|		Pág: %d		|		Marco: %d		(modif=%d)\n",datosProceso->proceso,datosProceso->pagina,datosProceso->marco,datosProceso->modificada);
}

void dumpDatos(traductor_marco* datosProceso){
    if(datosProceso->marco!=-1){
    	int numeros(traductor_marco* traductor){
    		return (traductor->proceso==datosProceso->proceso);
    	}
    	if (datosProceso->pagina>=list_count_satisfying(tabla_de_paginas,(void*)numeros)-cantSt){
    		void* datos=malloc(sizeof(int));
    		int i=0;
    		fprintf(reporteDump,"Marco: %d [",datosProceso->marco);

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

int atenderCpu(int conexion){
	log_info(archivoLog, "Nuevo CPU Conectado!");
	int salir = 0, operacion, proceso, pagina, offset, buffer, size,procesoAnterior=-1;
	int removerEntradasProcesoAnterior(traductor_marco* entradaTlb){
		return entradaTlb->proceso!=procesoAnterior;
	}
	void* datos;
	char *cod_op, *resp,*pid;
	int bytesRecibidos, bytesEnviados, *hilo;
	while (!salir) {
		cod_op=recibirMensaje(conexion, 1);
		operacion = atoi(cod_op);
		free(cod_op);
		bytesEnviados=0;
		bytesRecibidos=0;
		if (operacion) {

				proceso = recibirProtocolo(conexion);
				//list_add(procesosActivos,proceso);
				pid=string_itoa(proceso);
				hilo=malloc(4);
				hilo=pthread_self();
				dictionary_put(procesos,pid,hilo);

				pagina = recibirProtocolo(conexion);
				offset = recibirProtocolo(conexion);
				size=recibirProtocolo(conexion);
				printf("Recibi: %d %d %d %d\n",proceso,pagina,offset,size);
				void* x;
				switch (operacion) {

				case ENVIAR_BYTES:													//2 = Enviar Bytes (busco pag, y devuelvo el valor)
					datos=enviarBytes(proceso,pagina,offset,size);

					if (string_equals_ignore_case(datos,"@")){
						bytesEnviados=send(conexion,"no",2,MSG_NOSIGNAL);}											//NO hay marcos/No existe la pág :/
					else{
						bytesEnviados=send(conexion,"ok",2,MSG_NOSIGNAL);
						bytesEnviados=send(conexion,datos,size,MSG_NOSIGNAL);}

					x=malloc(size+1);
					memcpy(x,datos,size);
					memcpy(x+size,"\0",1);
					printf("ENVIE:%s\n",x);
					free(x);
					free(datos);
					break;

				case GUARDAR_BYTES:													//3 = Guardar Valor

					bytesRecibidos=recv(conexion,&buffer,sizeof(int),MSG_WAITALL);
					if(bytesRecibidos){
						buffer=ntohl(buffer);
						if (almacenarBytes(proceso,pagina,offset,size,buffer)==-1){						//La pag no existe
							resp=header(0);
						}else{resp=header(1);}
						bytesEnviados=send(conexion,resp,string_length(resp),MSG_NOSIGNAL);
						free(resp);}
					break;
				}

				if (procesoAnterior!=proceso){													//cambió el proceso => limpio la tlb
					tlb=list_filter(tlb,(void*)removerEntradasProcesoAnterior);					//filtra las que son DIFERENTES
					procesoAnterior=proceso;}

		/*		int termino(int p){
					return p==proceso;
				}
			list_remove_by_condition(procesosActivos,termino);*/
				dictionary_remove(procesos,pid);
				free(pid);
				if(bytesEnviados==-1 || bytesRecibidos==-1){					//murió la cpu mientras ejecutaba
					salir=1;}

		} else {salir = 1;}
	}
	log_warning(archivoLog, "Se desconectó una CPU");
//	pthread_mutex_unlock(&mutexFinProceso);
	close(conexion);
	return 1;
}


void atenderNucleo(int nucleo){
        int salir=0,guardar,procesoEliminar;
        char* cod_op;
        while (!salir) {
            cod_op=recibirMensaje(nucleo,1);
        	int operacion = atoi(cod_op);
            free(cod_op);
        	if (operacion) {
                    switch (operacion) {

                    case INICIALIZAR:                                                //inicializar programa
                        guardar=inicializarPrograma(nucleo);
                        if (!guardar){                            //1 = hay marcos (cola ready), 2 = no hay marcos (cola new)
                            log_warning(archivoLog,"Ansisop rechazado, memoria insuficiente");
                            perror("cagamos");}
                        guardar=htonl(guardar);
                        send(nucleo, &guardar,sizeof(int),0);
                    break;

                    case FINALIZAR:                                                //Finalizar programa
                    	procesoEliminar=recibirProtocolo(nucleo);
                    	printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>EL NUCLEO ME PIDE ELIMINAR A: %d\n",procesoEliminar);
                        finalizarPrograma(procesoEliminar);
                    break;
                    }
                }else{salir=1;}
        }
        log_warning(archivoLog,"Nucleo desconectado\n");
        log_info(archivoLog, "Desconectando UMC...\n");
        list_destroy_and_destroy_elements(tabla_de_paginas,free);

        void eliminarClock(unClock* clock){
        	queue_clean(clock->colaMarcos);
        	free(clock);
        }

        list_destroy_and_destroy_elements(tablaClocks,(void*)eliminarClock);
        free(datosMemoria);
        close(nucleo);
        close(conexionSwap);
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
    char* pid_char=header(PID);
    string_append(&programa, pid_char);
    free(pid_char);
    char* pags=header(paginasNecesarias);
    string_append(&programa, pags);
    free(pags);
    string_append(&programa, codigo);
    string_append(&programa, "\0");
    free(codigo);
    send(conexionSwap, programa, string_length(programa), 0);
    free(programa);
    int aceptadoSwap= esperarRespuestaSwap();
    if (!aceptadoSwap){
        return 0;
    }
    log_info(archivoLog,"ANSISOP %d GUARDADO\n",PID);
    int i;

   // pthread_mutex_lock(&mutexTablaPaginas);
    	usleep(datosMemoria->retardo*1000);							//todo 1 por cada acceso, o por cada escritura? :/
    	for (i = 0; i < paginasNecesarias; i++) {				//Registro el programa en la tabla, marco -1 porque está en Swap
			  actualizarTabla(i, PID, -1);
		}
    //pthread_mutex_unlock(&mutexTablaPaginas);

    if (hayMarcosLibres()){
        return 1;
    }
    return 2;
}


void* enviarBytes(int proceso,int pagina,int offset,int size){
   	int posicion=buscar(proceso, pagina);

    if (posicion!=-1){
        void* datos=(void*) malloc(size);
        memcpy(datos,memoria+posicion+offset,size);
        return datos;
    }

    char* mje=string_new();
    string_append(&mje,"@\0");
    return mje;                //No existe la pag
}


int almacenarBytes(int proceso, int pagina, int offset, int size, int buffer){
    int buscarMarco(traductor_marco* fila){
            return (fila->proceso==proceso && fila->pagina==pagina);}

    int posicion=buscar(proceso,pagina);
    if(posicion!=-1){                                //no existe la pagina

    posicion+=offset;

  //  pthread_mutex_lock(&mutexTablaPaginas);
		memcpy(memoria+posicion,&buffer,size);
		traductor_marco* datosTabla=list_find(tabla_de_paginas,(void*)buscarMarco);
		datosTabla->modificada=1;
		log_info(archivoLog,"(Proceso %d - Pag %d) Modificada\n",proceso,pagina);
//    pthread_mutex_unlock(&mutexTablaPaginas);
    return posicion;}
    else{
    	return -1;
    }
}


int finalizarPrograma(int procesoEliminar){
    int paginasDelProceso(traductor_marco* entradaTabla){
        return (entradaTabla->proceso==procesoEliminar);}

    void limpiarMarcos(traductor_marco* marco){
      	if((marco->marco!=-1) && (paginasDelProceso(marco))){
     //  	printf("-----Limpio marco: %d\n",marco->marco);
       	vectorMarcos[marco->marco]=0;}
    }
    	printf("ESTOY ESPERANDO...\n");
    	usleep(datosMemoria->retardo*1000);
   /* 	int estaEjecutando(int p){
    		return p==procesoEliminar;
    	}
    	while(list_any_satisfy(procesosActivos,(void*)estaEjecutando));*/

    char* pro=string_itoa(procesoEliminar);
    if(dictionary_has_key(procesos,pro)){
    	int* c=dictionary_get(procesos,pro);
    	pthread_join(*c,NULL);
    }
    free(pro);

    printf("AL FIN!!\n");
    void limpiar(traductor_marco* marco){
        list_remove_and_destroy_by_condition(tabla_de_paginas,(void*)paginasDelProceso,(void*)free);}

    int clockDelProceso(unClock* clockDelProceso){
        return(clockDelProceso->proceso==procesoEliminar);}

    void limpiarTLB(traductor_marco* marco){
           list_remove_by_condition(tlb,(void*)paginasDelProceso);
    }

   // pthread_mutex_lock(&mutexFinProceso);
		list_iterate(tlb,(void*)limpiarTLB);

		//pthread_mutex_lock(&mutexTablaPaginas);								//Voy a eliminar entradas de la tabla
			pthread_mutex_lock(&mutexMarcos);									//Porque quizá algunos quedan libres ahora
				log_info(archivoLog,"-----FINALIZA PROCESO: %d",procesoEliminar);
				log_info(archivoLog,"[Antes] Paginas: %d | Clocks: %d | TLB:%d\n",list_size(tabla_de_paginas),list_size(tablaClocks),list_size(tlb));

				list_iterate(tabla_de_paginas,(void*)limpiarMarcos);
				list_iterate(tabla_de_paginas,(void*)limpiar);
			pthread_mutex_unlock(&mutexMarcos);
		//pthread_mutex_unlock(&mutexTablaPaginas);

		unClock* clockProceso=list_find(tablaClocks,(void*)clockDelProceso);
		list_remove_by_condition(tablaClocks,(void*)clockDelProceso);
		if (clockProceso!=NULL){
			queue_clean(clockProceso->colaMarcos);
			free(clockProceso);
		}

		log_info(archivoLog,"[Despues] Paginas: %d | Clocks: %d | TLB: %d\n",list_size(tabla_de_paginas),list_size(tablaClocks),list_size(tlb));

	char* mensajeEliminar=string_new();
    string_append(&mensajeEliminar,"3");
    char* proceso=header(procesoEliminar);
    string_append(&mensajeEliminar,proceso);
    string_append(&mensajeEliminar,"\0");
    send(conexionSwap,mensajeEliminar,string_length(mensajeEliminar),0);
    free(mensajeEliminar);
    free(proceso);

    return 1;
}


int esperarRespuestaSwap(){

	pthread_mutex_lock(&mutexSwap);
    	char *respuesta = malloc(3);
    	recv(conexionSwap, respuesta, 2, MSG_WAITALL);
    pthread_mutex_unlock(&mutexSwap);

    respuesta[2] = '\0';
    printf("_____________________________SWAP CONTESTÓ:%s\n",respuesta);
    int aceptado = esIgual(respuesta, "ok");
    free(respuesta);
    return aceptado;
}
