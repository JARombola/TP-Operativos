/*
 * Paginas.c
 *
 *  Created on: 1/6/2016
 *      Author: utnso
 */

#include "Paginas.h"

int buscar(int proceso, int pag) {                //todo busqueda en la TLB
    int paginaEncontrada(traductor_marco* fila) {
        return ((fila->proceso == proceso) && (fila->pagina == pag));}

    int posicion;
    traductor_marco* paginaBuscada;

    if (list_any_satisfy(tabla_de_paginas,(void*)paginaEncontrada)) {                //Esta "registrada", la pag (Existe)

        paginaBuscada=list_find(tlb,(void*)paginaEncontrada);
        if(datosMemoria->entradas_tlb){
        	usleep(datosMemoria->retardo*1000);}											//Acceso a tlb

        if (paginaBuscada!=NULL){                                                    //Esta en la tlb, entonces saco y vuelvo a poner más abajo
            if (datosMemoria->entradas_tlb) log_info(archivoLog,"(Proceso %d | Pag %d) TLB HIT! ",proceso,pag);
            list_remove_by_condition(tlb,(void*)paginaEncontrada);
        }
        else{
        	if (datosMemoria->entradas_tlb) log_info(archivoLog,"(Proceso %d | Pag %d) TLB MISS! ",proceso,pag);
            paginaBuscada=list_find(tabla_de_paginas,(void*) paginaEncontrada);
            usleep(datosMemoria->retardo*1000);}										//Acceso a la tabla de paginas

        if (paginaBuscada->marco <0) {    //no está en memoria => peticion a swap
            log_info(archivoLog,"(Proceso %d | Pag %d) Pedido a Swap",proceso,pag);
        	paginaBuscada=solicitarPaginaASwap(proceso,pag);
        }else{
        	log_info(archivoLog,"(Proceso %d | Pag %d) Estaba en memoria",proceso,pag);
            }

        if(paginaBuscada->marco!=-666){
        	if (datosMemoria->entradas_tlb){
				list_add(tlb, paginaBuscada);
				if(list_size(tlb)>datosMemoria->entradas_tlb){                            //Actualizar tlb
					list_remove(tlb,0);
				}
        	}
			if(paginaBuscada!=NULL){
				posicion=paginaBuscada->marco * datosMemoria->marco_size;
				return posicion;}                                //devuelve la posicion dentro de la "memoria"
			else{
				return -1;
			}
        }
        else{
        	log_warning(archivoLog,"XX-No hay marcos disponibles-XX");
        	return -1;
       }
    }
    log_error(archivoLog,"(Proceso %d | Pag %d) PÁGINA INEXISTENTE!!!(Stack Overflow?)\n",proceso,pag);
    return -1;
}


traductor_marco* actualizarTabla(int pag, int proceso, int marco){
    void eliminarAnterior(traductor_marco* marco){
        free(marco);}
    int existe(traductor_marco* entrada){
        return (entrada->pagina==pag && entrada->proceso==proceso);}

    traductor_marco* traductorMarco=list_find(tabla_de_paginas,(void*)existe);

    if(traductorMarco!=NULL){											//Si la pag existe, la modifico
        traductorMarco->marco=marco;
        traductorMarco->modificada=0;
    }
    else{                                                                //Sino la registro
    traductor_marco* traductorMarco=malloc(sizeof(traductor_marco));
    traductorMarco->pagina=pag;
    traductorMarco->proceso=proceso;
    traductorMarco->marco=marco;
    traductorMarco->modificada=0;
    list_add(tabla_de_paginas,traductorMarco);}
    return traductorMarco;
}


traductor_marco* guardarPagina(void* datos,int proceso,int pag){
    int marco,tamMarco=datosMemoria->marco_size;
    marco = buscarMarco(proceso);
    if (marco==-1){                                //no hay marcos para darle => hay que eliminar el proceso
        traductor_marco* traductorErroneo=malloc(sizeof(traductor_marco));
        traductorErroneo->marco=-666;
        return traductorErroneo;
    }

        traductor_marco* datosPagina=actualizarTabla(pag, proceso, marco);
        memcpy(memoria + (marco * tamMarco), datos, tamMarco);

    return datosPagina;
}


int buscarMarco(int pid){
    int marco = 0, cantMarcos = marcosAsignados(pid);
    int paginaDelMarco(traductor_marco* pagina) {                                                    //porque la pos que ocupa y el marco no son el mismo, necesito el marco con esa posicion
        return (pagina->marco == marco);
    }
    int clockDelProceso(unClock* marco){
          return(marco->proceso==pid);
    }

    if (cantMarcos < datosMemoria->marco_x_proc) {                                                //cantidad de marcos del proceso para ver si reemplazo, o asigno vacios
        pthread_mutex_lock(&mutexMarcos);
        if(hayMarcosLibres()){
        	marco=buscarMarcoLibre(pid, cantMarcos);
        	log_info(archivoLog,"(Proceso %d) ==> Nuevo Marco: %d",pid,marco);
        	pthread_mutex_unlock(&mutexMarcos);
        	return marco;														//encontró un marco libre
        }
        pthread_mutex_unlock(&mutexMarcos);
    }
    if (cantMarcos) {        													//Si tiene marcos => hay que reemplazarle uno, SINO no hay espacio
        int i = 0,primeraVuelta=0,modificada=0;
        unClock* clockProceso=list_find(tablaClocks,(void*)clockDelProceso);
        if (datosMemoria->algoritmo) {                            				//Clock mejorado
            primeraVuelta=1;}
        int cont=0;
        traductor_marco* datosMarco;

        do {
            marco=(int) queue_pop(clockProceso->colaMarcos);
            datosMarco = list_find(tabla_de_paginas,(void*)paginaDelMarco);

            if (datosMemoria->algoritmo){
                if (primeraVuelta){
                    	modificada=datosMarco->modificada;
                    }
                else{modificada=0;}
            }

            if (vectorMarcos[marco]== 1 && !modificada) {                                                        //Se va de la UMC
            	if (datosMarco->modificada) {                                                        //Estaba modificada => se la mando a la swap
                    log_info(archivoLog,"(Proceso %d | Pag %d) Envío a swap (Estaba modificada)\n",datosMarco->proceso,datosMarco->pagina);
                    enviarPaginaASwap(datosMarco);
                }else{
                    log_info(archivoLog,"(Proceso %d | Pag %d) No se envia a swap (no estaba modificada)\n",datosMarco->proceso,datosMarco->pagina);
                }
                vectorMarcos[marco] = 2;
                queue_push(clockProceso->colaMarcos,(void*)marco);
                log_info(archivoLog,"(Proceso %d) ===> Marco Reemplazado: %d",pid, marco);

                actualizarTabla(datosMarco->pagina,pid,-1);

                return marco;}                                    //La nueva posicion libre

            queue_push(clockProceso->colaMarcos,(void*)marco);
            if(!primeraVuelta){
            vectorMarcos[marco]=1;}
            if (cont==queue_size(clockProceso->colaMarcos)-1){primeraVuelta=0;}
            else{cont++;}
        } while (++i);
    }
    return -1;                                                                                        //No hay marcos para darle ni reemplazar
}

int buscarMarcoLibre(int pid, int cantMarcos){
    int clockDelProceso(unClock* marco){
          return(marco->proceso==pid);
     }
    int marco;
    for (marco = 0; marco < datosMemoria->marcos; marco++) { //Se fija si hay marcos vacios
        if (!vectorMarcos[marco]) {
            if (!cantMarcos) { 							//Para el clock mejorado, registro el clock del proceso
                unClock* clockProceso = malloc(sizeof(unClock));
                clockProceso->colaMarcos = queue_create();
                queue_push(clockProceso->colaMarcos,(void*) marco);
                clockProceso->proceso = pid;
                list_add(tablaClocks, clockProceso);
            } else {
                unClock* clockProceso = list_find(tablaClocks,(void*) clockDelProceso);
                queue_push(clockProceso->colaMarcos, (void*)marco);
            }
            vectorMarcos[marco] = 2;
            return marco;
        }
    }
    return -1;
}

void enviarPaginaASwap(traductor_marco* datosMarco){
    char* mje1 = string_new();
    string_append(&mje1, "1");
    string_append(&mje1, header(datosMarco->proceso));
    string_append(&mje1,header(0));                                            //Cantidad de paginas nuevas... 0, no necesito agregar ninguna, solo modifico
    string_append(&mje1,"\0");
    char* mje2 = string_new();
    string_append(&mje2, header(datosMarco->pagina));
    string_append(&mje2, "\0");
    int tamanioDatos=string_length(mje1)+datosMemoria->marco_size+string_length(mje2);
    void* datos=malloc(tamanioDatos);
    memcpy(datos,mje1,string_length(mje1));                        //Datos de protocolo
    memcpy(datos+string_length(mje1),memoria+datosMarco->marco*datosMemoria->marco_size,datosMemoria->marco_size);        //COPIO LOS DATOS QUE CONTIENE ESA PAG
    memcpy(datos+string_length(mje1)+datosMemoria->marco_size,mje2,string_length(mje2));    //Datos de protocolo
    free(mje1);
    free(mje2);
    send(conexionSwap, datos, tamanioDatos, 0);
    free(datos);
}

traductor_marco* solicitarPaginaASwap(int proceso, int pagina){
	 char* pedido = string_new();
	 char* proceso_char=header(proceso);
	 char* pag= header(pagina);
	 string_append(&pedido, "2");
	 string_append(&pedido, proceso_char);
	 string_append(&pedido, pag);
	 string_append(&pedido, "\0");
	 send(conexionSwap, pedido, string_length(pedido), 0);
	 free(pedido);
	 free(proceso_char);
	 free(pag);


	 pthread_mutex_lock(&mutexSwap);
	 	 void* datos = (void*) malloc(datosMemoria->marco_size);
	 	 recv(conexionSwap, datos, datosMemoria->marco_size, MSG_WAITALL);
	 pthread_mutex_unlock(&mutexSwap);

	 traductor_marco* paginaBuscada=guardarPagina(datos, proceso, pagina);
	 free(datos);

	 return paginaBuscada;
}


int marcosAsignados(int pid){
    int marcosDelProceso(traductor_marco* marco){
        return ((marco->proceso==pid) && (marco->marco>=0));}

 return (list_count_satisfying(tabla_de_paginas,(void*)marcosDelProceso));
}

int hayMarcosLibres(){						//La usá en UMC para ver si manda a NEW o READY al proceso
    int i;
    for (i=0;i<datosMemoria->marcos;i++){
        if (!vectorMarcos[i]){
            return 1;}
    }
    return 0;
}

