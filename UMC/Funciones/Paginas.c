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
    if (list_any_satisfy(tabla_de_paginas,(void*)paginaEncontrada)) {                //Esta "registrada" la pag (Existe)

    pthread_mutex_lock(&mutexTlb);

        paginaBuscada=list_find(tlb,(void*)paginaEncontrada);
        if (paginaBuscada!=NULL){                                                    //Esta en la tlb, entonces saco y vuelvo a poner más abajo
            printf("TLB HIT;)\n");
            list_remove_by_condition(tlb,(void*)paginaEncontrada);
            pthread_mutex_unlock(&mutexTlb);
        }
        else{
            printf("TLB MISS :(\n");
    pthread_mutex_unlock(&mutexTlb);

        paginaBuscada=list_find(tabla_de_paginas,(void*) paginaEncontrada);
            //slep, consulta a la tabla de paginas
        }
        if (paginaBuscada->marco <0) {    //no está en memoria => peticion a swap
            //printf("Pedido a swap - (Proceso: %d | Pag: %d)",proceso,pag);
            void* datos = (void*) malloc(datosMemoria->marco_size);
            char* pedido = string_new();
            string_append(&pedido, "2");
            string_append(&pedido, (char*)header(proceso));
            string_append(&pedido, (char*)header(pag));
            string_append(&pedido, "\0");
            send(conexionSwap, pedido, string_length(pedido), 0);
            free(pedido);
            recv(conexionSwap, datos, datosMemoria->marco_size, MSG_WAITALL);
            paginaBuscada=guardarPagina(datos, proceso, pag);
            free(datos);
        }else{
        //printf("Estaba en memoria - (Proceso: %d | Pag: %d)",proceso,pag);
            }
    pthread_mutex_lock(&mutexTlb);

        list_add(tlb, paginaBuscada);
        if(list_size(tlb)>datosMemoria->entradas_tlb){                            //Actualizar tlb
            list_remove(tlb,0);
        }

    pthread_mutex_unlock(&mutexTlb);
        posicion=paginaBuscada->marco * datosMemoria->marco_size;
        return posicion;                                //devuelve la posicion dentro de la "memoria"
    }
    printf("No existe la pagina solicitada\n");
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
        traductorErroneo->marco=-1;
        return traductorErroneo;
    }

    pthread_mutex_lock(&mutexTablaPaginas);
        traductor_marco* datosPagina=actualizarTabla(pag, proceso, marco);
        memcpy(memoria + (marco * tamMarco), datos, tamMarco);
    pthread_mutex_unlock(&mutexTablaPaginas);

    return datosPagina;
}


int buscarMarco(int pid){            //    Con marcos en orden de llegada
    int marco = 0, cantMarcos = marcosAsignados(pid);
    int paginaDelMarco(traductor_marco* pagina) {                                                    //porque la pos que ocupa y el marco no son el mismo, necesito el marco con esa posicion
        return (pagina->marco == marco);
    }
    int clockDelProceso(unClock* marco){
          return(marco->proceso==pid);
      }

    if (cantMarcos < datosMemoria->marco_x_proc) {                                                //cantidad de marcos del proceso para ver si reemplazo, o asigno vacios
        pthread_mutex_lock(&mutexMarcos);
        marco=buscarMarcoLibre(pid, cantMarcos);
        pthread_mutex_unlock(&mutexMarcos);
        if (marco!=-1){return marco;}                //encontró un marco libre
    }
    if (cantMarcos) {        //Si tiene marcos => hay que reemplazarle uno, SINO no hay espacio
        int i = 0,primeraVuelta=0,modificada=0;
        unClock* clockProceso=list_find(tablaClocks,(void*)clockDelProceso);
        if (datosMemoria->algoritmo) {                            //Clock mejorado
            primeraVuelta=1;}                                    //Puede hacerse mas lindo con listas filtradas, pero KISS
        int cont=0;
        traductor_marco* datosMarco;
        do {
            marco=(int) queue_pop(clockProceso->colaMarcos);
            datosMarco = list_find(tabla_de_paginas,(void*)paginaDelMarco);
            if (datosMemoria->algoritmo){
                if (primeraVuelta){
                    pthread_mutex_lock(&mutexModificacion);
                    	modificada=datosMarco->modificada;
                    pthread_mutex_unlock(&mutexModificacion);
                    }
                else{modificada=0;}
            }
            if (vectorMarcos[marco]== 1 && !modificada) {                                                        //Se va de la UMC
                pthread_mutex_lock(&mutexModificacion);
                if (datosMarco->modificada) {                                                        //Estaba modificada => se la mando a la swap
                    pthread_mutex_unlock(&mutexModificacion);
                    printf("(Proceso %d | Pag %d) Envío a swap (Estaba modificada)\n",datosMarco->proceso,datosMarco->pagina);
                    enviarPaginaASwap(datosMarco);
                    esperarRespuestaSwap();
                }else{
                    printf("(Proceso %d | Pag %d) No se envia a swap (no estaba modificada)\n",datosMarco->proceso,datosMarco->pagina);
                }
                pthread_mutex_unlock(&mutexModificacion);
                vectorMarcos[marco] = 2;
                queue_push(clockProceso->colaMarcos,(int)marco);
                printf("Marco eliminado: %d\n", marco);

                pthread_mutex_lock(&mutexTablaPaginas);
                actualizarTabla(datosMarco->pagina,pid,-1);
                pthread_mutex_unlock(&mutexTablaPaginas);

                return marco;}                                    //La nueva posicion libre

            queue_push(clockProceso->colaMarcos,(int)marco);
            if(!primeraVuelta){
            vectorMarcos[marco]--;}
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
                queue_push(clockProceso->colaMarcos, (int) marco);
                clockProceso->proceso = pid;
                list_add(tablaClocks, clockProceso);
            } else {
                unClock* clockProceso = list_find(tablaClocks,(void*) clockDelProceso);
                queue_push(clockProceso->colaMarcos, (int) marco);
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

int marcosAsignados(int pid){
    int marcosDelProceso(traductor_marco* marco){
        return (marco->proceso==pid && marco->marco>=0);}

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
