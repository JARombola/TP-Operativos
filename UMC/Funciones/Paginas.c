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
            printf("Estaba en la TLB ;)\n");
            list_remove_by_condition(tlb,(void*)paginaEncontrada);
            pthread_mutex_unlock(&mutexTlb);
        }
        else{
    pthread_mutex_unlock(&mutexTlb);

        paginaBuscada=list_find(tabla_de_paginas,(void*) paginaEncontrada);
            //slep, consulta a la tabla de paginas
        }
        if (paginaBuscada->marco <0) {    //no está en memoria => peticion a swap
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
        }

    pthread_mutex_lock(&mutexTlb);

        list_add(tlb, paginaBuscada);
        if(list_size(tlb)>=datosMemoria->entradas_tlb){                            //Actualizar tlb
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

    if(traductorMarco!=NULL){
    //    pthread_mutex_lock(&mutexTlb);            //Bloquear? No creo que sea necesario...
        traductorMarco->marco=marco;
        traductorMarco->modificada=0;
    //    pthread_mutex_unlock(&mutexTlb);
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
    marco = buscarMarcoLibre(proceso);
    if (marco==-1){                                //no hay marcos para darle => hay que eliminar el proceso
        traductor_marco* traductorErroneo=malloc(sizeof(traductor_marco));
        traductorErroneo->marco=-1;
        return traductorErroneo;
    }

    pthread_mutex_lock(&mutexReemplazo);
        traductor_marco* datosPagina=actualizarTabla(pag, proceso, marco);
        memcpy(memoria + (marco * tamMarco), datos, tamMarco);
    pthread_mutex_unlock(&mutexReemplazo);

    return datosPagina;
}


int buscarMarcoLibre(int pid) {
        int pos = 0, cantMarcos = marcosAsignados(pid, 1);
        int menorMayor(traductor_marco* marco1, traductor_marco* marco2){
            return marco1->marco<marco2->marco;}
        int marcoDelProceso(traductor_marco* marco) {
            return (marco->proceso == pid && marco->marco >= 0);}                                        //está en memoria
        int clockDelProceso(unClock* marco){
            return(marco->proceso==pid);}
        void eliminarClock(unClock* clock){
            free (clock);}


        if (cantMarcos < datosMemoria->marco_x_proc) {                                                //cantidad de marcos del proceso para ver si reemplazo, o asigno vacios

            pthread_mutex_lock(&mutexMarcos);                                                    //Para evitar 2 procesos el mismo marco
            for (pos = 0; pos < datosMemoria->marcos; pos++) {                                            //Se fija si hay marcos vacios
                if (!vectorMarcos[pos]) {
                    if (!cantMarcos){                                                                //Registro el clock del proceso (marco)
                        unClock* clockProceso=malloc(sizeof(unClock));
                        clockProceso->posClock=pos;
                        clockProceso->proceso=pid;
                        list_add(tablaClocks,clockProceso);}
                    vectorMarcos[pos] = 2;
                    pthread_mutex_unlock(&mutexMarcos);
                    return pos;
                }
            }
            pthread_mutex_unlock(&mutexMarcos);
        }

        if (cantMarcos) {                            //Si tiene marcos => hay que reemplazarle uno, SINO no hay espacio

            int i = 0,primeraVuelta=0,modificada=0;
            unClock* marcoClock=list_find(tablaClocks,(void*)clockDelProceso);
            pos=marcoClock->posClock;
            traductor_marco* datosMarco;// = malloc(sizeof(traductor_marco));

            t_list* listaFiltrada = list_filter(tabla_de_paginas,(void*) marcoDelProceso);                    //Filtro los marcos de ESE proceso

            if (datosMemoria->algoritmo) {
                primeraVuelta=1;}

            list_sort(listaFiltrada,(void*)menorMayor);                //CLOCK MEJORADO
            int encontrado=0;
            for (i = 0; !encontrado; i++) {
                datosMarco = list_get(listaFiltrada, i);
                if (datosMarco->marco == pos) {
                    encontrado = 1;
                    i--;}
            }

            int cont=0;

            do {//traductor_marco* datosMarco = malloc(sizeof(traductor_marco));
                if (i == list_size(listaFiltrada)) {                                                //Para que pegue la vuelta
                    i = 0;}

                if (cont==list_size(listaFiltrada)){primeraVuelta=0;}

                datosMarco = list_get(listaFiltrada, i);                                                //Chequeo c/u para ver cual sacar
                pos = datosMarco->marco;

                if (datosMemoria->algoritmo){
                    if (primeraVuelta){
                    //    pthread_mutex_lock(&mutexModificacion);                //Por el comando que marca todas como modificadas
                        modificada=datosMarco->modificada;}
                    //    pthread_mutex_unlock(&mutexModificacion);
                    else{modificada=0;}
                }

                if (vectorMarcos[pos] == 1 && !modificada) {                                                        //Se va de la UMC
                    vectorMarcos[pos] = 2;
                //    pthread_mutex_lock(&mutexModificacion);
                    if (datosMarco->modificada) {                                                        //Estaba modificada => se la mando a la swap
                //        pthread_mutex_unlock(&mutexModificacion);
                        enviarPaginaASwap(datosMarco);
                        char resp[3];
                        recv(conexionSwap,resp,2,MSG_WAITALL);                //"ok" o "no"                //todo
                /*        if (string_equals_ignore_case(resp,"ok")){
                            printf("TODO PEOLA\n");
                        }*/
                    }
                    //pthread_mutex_unlock(&mutexModificacion);
                    printf("Marco eliminado: %d\n", pos);

                    pthread_mutex_lock(&mutexReemplazo);
                    actualizarTabla(datosMarco->pagina,pid,-1);
                    pthread_mutex_unlock(&mutexReemplazo);

                    i++;
                    if (i >= list_size(listaFiltrada)) {i = 0;}                                                    //Para que pegue la vuelta
                    datosMarco=list_get(listaFiltrada,i);                        //Dejo el puntero apuntando al proximo marco
                    marcoClock->posClock=datosMarco->marco;
                    list_clean(listaFiltrada);

                    return pos;}                                        //La nueva posicion libre
                if(!primeraVuelta){
                vectorMarcos[pos]--;}
                cont++;
            } while (++i);
        }
        return -1;                                                                                        //No hay marcos para darle
    }





    /*    Con marcos en orden de llegada
    int marco = 0, cantMarcos = marcosAsignados(pid, 1);
    unClock* clockProceso=malloc(sizeof(unClock));
    int paginaDelMarco(traductor_marco* pagina) {                                                    //porque la pos que ocupa y el marco no son el mismo, necesito el marco con esa posicion
        return (pagina->marco == marco);
    }
    int clockDelProceso(unClock* marco){
        return(marco->proceso==pid);
    }
    if (cantMarcos < datosMemoria->marco_x_proc) {                                                //cantidad de marcos del proceso para ver si reemplazo, o asigno vacios
        for (marco = 0; marco < datosMemoria->marcos; marco++) {                                            //Se fija si hay marcos vacios
            if (!vectorMarcos[marco]) {
                if (!cantMarcos){                                        //Para el clock mejorado, registro el clock del proceso
                    clockProceso->colaMarcos=queue_create();
                    queue_push(clockProceso->colaMarcos,(int)marco);
                    clockProceso->proceso=pid;
                    list_add(tablaClocks,clockProceso);}
                else{
                    clockProceso=(unClock*)list_find(tablaClocks,(void*)clockDelProceso);
                    queue_push(clockProceso->colaMarcos,(int)marco);
                }
                vectorMarcos[marco] = 2;
                return marco;
            }
        }
    }
    if (cantMarcos) {        //Si tiene marcos => hay que reemplazarle uno, SINO no hay espacio
        int i = 0,primeraVuelta=0,modificada=0;
        clockProceso=(unClock*)list_find(tablaClocks,(void*)clockDelProceso);
        if (datosMemoria->algoritmo) {                            //Clock mejorado
            primeraVuelta=1;}                                    //Puede hacerse mas lindo con listas filtradas, pero KISS
        int cont=0;
        do {traductor_marco* datosMarco;
            marco=(int) queue_pop(clockProceso->colaMarcos);
            datosMarco=(traductor_marco*) list_find(tabla_de_paginas,(void*)paginaDelMarco);
            if (datosMemoria->algoritmo){
                if (primeraVuelta){modificada=datosMarco->modificada;}
                else{modificada=0;}
            }
            if (vectorMarcos[marco]== 1 && !modificada) {                                                        //Se va de la UMC
                if (datosMarco->modificada) {                                                        //Estaba modificada => se la mando a la swap
                    char* mje = string_new();
                    string_append(&mje, "1");
                    string_append(&mje, header(datosMarco->proceso));
                    string_append(&mje,header(0));
                    string_append(&mje,    memoria+ datosMarco->marco* datosMemoria->marco_size);
                    string_append(&mje, header(datosMarco->pagina));
                    string_append(&mje, header(0));
                    string_append(&mje, "\0");
                    send(conexionSwap, mje, string_length(mje), 0);
                    free(mje);
                }
                vectorMarcos[marco] = 2;
                printf("Marco eliminado: %d\n", marco);
                queue_push(clockProceso->colaMarcos,(int)marco);
                actualizarTabla(datosMarco->pagina,pid,-1);

            //    datosMarco=list_find(listaFiltrada,(void*)marcoDelProceso);

        //        datosMarco->marco = -1;
        //        list_replace(tabla_de_paginas, (int) marcoPosicion, datosMarco);
                return marco;}                                    //La nueva posicion libre
            queue_push(clockProceso->colaMarcos,(int)marco);
            if(!primeraVuelta){
            vectorMarcos[marco]--;}
            if (cont==queue_size(clockProceso->colaMarcos)-1){primeraVuelta=0;}
            else{cont++;}
        } while (++i);
    }
    return -1;                                                                                        //No hay marcos para darle
}*/

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

int marcosAsignados(int pid, int operacion){
    int marcosDelProceso(traductor_marco* marco){
        return (marco->proceso==pid && marco->marco>=0);}

 return (list_count_satisfying(tabla_de_paginas,(void*)marcosDelProceso));
}

int hayMarcosLibres(){
    int i;
    for (i=0;i<datosMemoria->marcos;i++){
        if (!vectorMarcos[i]){
            return 1;}
    }
    return 0;
}
