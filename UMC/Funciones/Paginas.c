/*
 * Paginas.c
 *
 *  Created on: 1/6/2016
 *      Author: utnso
 */

#include "Paginas.h"

int buscar(int proceso, int pag) {				//todo busqueda en la TLB
	int paginaBuscada(traductor_marco* fila) {
		if ((fila->proceso == proceso) && (fila->pagina == pag)) {
			return 1;}
		return 0;
	}
	int posicion;
	traductor_marco* encontrada;
	if(pag==13){
		int p=2;
	}
	if (list_any_satisfy(tabla_de_paginas,(void*)paginaBuscada)) {				//Esta "registrada" la pag (Existe)
		//Consultar tlb
		/*encontrada=list_find(tlb,(void*paginaBuscada);
			if (encontrada!=NULL){		//Esta en la tlb, entonces saco y vuelvo a poner

		}
		else{*/
		encontrada=list_find(tabla_de_paginas,(void*) paginaBuscada);
			//slep, consulta a la tabla de paginas
		if (encontrada->marco <0) {	//no está en memoria => peticion a swap
			void* datos = (void*) malloc(datosMemoria->marco_size);
			char* pedido = string_new();
			string_append(&pedido, "2");
			string_append(&pedido, (char*)header(proceso));
			string_append(&pedido, (char*)header(pag));
			string_append(&pedido, "\0");
			send(conexionSwap, pedido, string_length(pedido), 0);
			free(pedido);
			recv(conexionSwap, datos, datosMemoria->marco_size, 0);
			encontrada=guardarPagina(datos, proceso, pag);
		}
							//Actualizar tlb
		//}
		posicion=encontrada->marco * datosMemoria->marco_size;
		return posicion;								//devuelve la posicion dentro de la "memoria"
	}
	printf("No existe la pagina solicitada\n");
	return -1;
}

traductor_marco* actualizarTabla(int pag, int proceso, int marco){
	traductor_marco* traductorMarco=malloc(sizeof(traductor_marco));
	void eliminarAnterior(traductor_marco* marco){
		free(marco);
	}
	int existe(traductor_marco* entrada){
		return (entrada->pagina==pag && entrada->proceso==proceso);
	}
	if(list_any_satisfy(tabla_de_paginas,(void*)existe)){						//Si la pagina existe, la actualizo
		int i,encontrado=0;
		for (i=0;!encontrado;i++){
			traductorMarco=list_get(tabla_de_paginas,i);
			if (traductorMarco->pagina==pag && traductorMarco->proceso==proceso){
				encontrado=1;
				i--;
			}
		}
		traductorMarco->marco=marco;
		traductorMarco->modificada=0;
		list_replace(tabla_de_paginas,i,traductorMarco);}
	else{																//Sino la registro
	traductorMarco->pagina=pag;
	traductorMarco->proceso=proceso;
	traductorMarco->marco=marco;
	traductorMarco->modificada=0;
	//printf("Pag %d, proceso %d, marco %d\n",traductorMarco->pagina,traductorMarco->proceso,traductorMarco->marco);
	list_add(tabla_de_paginas,traductorMarco);}
	return traductorMarco;
}


traductor_marco* guardarPagina(void* datos,int proceso,int pag){
	int marco,tamMarco=datosMemoria->marco_size;
	marco = buscarMarcoLibre(proceso);
	if (marco==-1){								//no hay marcos para darle => hay que eliminar el proceso
		return -1;
	}
	memcpy(memoria + (marco * tamMarco), datos, tamMarco);//
	traductor_marco* datosPagina=actualizarTabla(pag, proceso, marco);
//	printf("Paginas Necesarias:%d , TotalMarcosGuardados: %d\n",paginasNecesarias,i);
//	printf("TablaDePaginas:%d\n",list_size(tabla_de_paginas));
	return datosPagina;
}

int buscarMarcoLibre(int pid) {
		int pos = 0, cantMarcos = marcosAsignados(pid, 1),a=0;
		int menorMayor(traductor_marco* marco1, traductor_marco* marco2){
			return marco1->marco<marco2->marco;
		}
		int marcoDelProceso(traductor_marco* marco) {
			return (marco->proceso == pid && marco->marco >= 0);										//está en memoria
		}
		int marcoPosicion(traductor_marco* marco) {													//porque la pos que ocupa y el marco no son el mismo, necesito el marco con esa posicion
			return (marco->marco == pos);
		}
		int clockDelProceso(unClock* marco){
			return(marco->proceso==pid);
		}
		void eliminarClock(unClock* clock){
			free (clock);
		}

		if (cantMarcos < datosMemoria->marco_x_proc) {												//cantidad de marcos del proceso para ver si reemplazo, o asigno vacios
			for (pos = 0; pos < datosMemoria->marcos; pos++) {											//Se fija si hay marcos vacios
				if (!vectorMarcos[pos]) {
					if (!cantMarcos){										//Para el clock mejorado, registro el clock del proceso
						unClock* clockProceso=malloc(sizeof(unClock));
						clockProceso->posClock=pos;
						clockProceso->proceso=pid;
						list_add(tablaClocks,clockProceso);}
					vectorMarcos[pos] = 2;
					return pos;
				}
			}
		}
		if (cantMarcos) {		//Si tiene marcos => hay que reemplazarle uno, SINO no hay espacio

			int i = 0,primeraVuelta=0,modificada=0;
			unClock* marcoClock =(unClock*)malloc(sizeof(unClock));
			marcoClock=(unClock*)list_find(tablaClocks,(void*)clockDelProceso);
			pos=marcoClock->posClock;
			traductor_marco* datosMarco = malloc(sizeof(traductor_marco));
			t_list* listaFiltrada = list_filter(tabla_de_paginas,(void*) marcoDelProceso);														//Filtro los marcos de ESE proceso

			if (datosMemoria->algoritmo) {
				primeraVuelta=1;}
			list_sort(listaFiltrada,(void*)menorMayor);				//CLOCK MEJORADO
			int encontrado=0;
			for (i = 0; !encontrado; i++) {
				datosMarco = list_get(listaFiltrada, i);
				if (datosMarco->marco == pos) {
					encontrado = 1;
					i--;}
			}
			int cont=0;
			do {traductor_marco* datosMarco = malloc(sizeof(traductor_marco));
				if (i == list_size(listaFiltrada)) {													//Para que pegue la vuelta
					i = 0;
				}
				if (cont==list_size(listaFiltrada)){primeraVuelta=0;}
				datosMarco = list_get(listaFiltrada, i);												//Chequeo c/u para ver cual sacar
				pos = datosMarco->marco;
				if (datosMemoria->algoritmo){
					if (primeraVuelta){modificada=datosMarco->modificada;}
					else{modificada=0;}
				}
				if (vectorMarcos[pos] == 1 && !modificada) {														//Se va de la UMC
					if (datosMarco->modificada) {														//Estaba modificada => se la mando a la swap
						char* mje1 = string_new();
						string_append(&mje1, "1");
						string_append(&mje1, header(datosMarco->proceso));
						string_append(&mje1,header(0));											//Cantidad de paginas nuevas... 0, no necesito agregar ninguna, solo modifico
						string_append(&mje1,"\0");
						char* mje2 = string_new();
						string_append(&mje2, header(datosMarco->pagina));
						string_append(&mje2, "\0");
						int tamanioDatos=string_length(mje1)+datosMemoria->marco_size+string_length(mje2);
						void* datos=malloc(tamanioDatos);
						memcpy(datos,mje1,string_length(mje1));
						memcpy(datos+string_length(mje1),memoria+datosMarco->marco*datosMemoria->marco_size,datosMemoria->marco_size);
						memcpy(datos+string_length(mje1)+datosMemoria->marco_size,mje2,string_length(mje2));
						free(mje1);
						free(mje2);
						send(conexionSwap, datos, tamanioDatos, 0);
					}
					vectorMarcos[pos] = 2;
					printf("Marco eliminado: %d\n", pos);
					i++;
					if (i >= list_size(listaFiltrada)) {													//Para que pegue la vuelta
						i = 0;
					}
					datosMarco=list_get(listaFiltrada,i);						//Dejo el puntero apuntando al proximo marco
				//	datosMarco=list_find(listaFiltrada,(void*)marcoDelProceso);
					unClock* clockNuevo =(unClock*)malloc(sizeof(unClock));
					clockNuevo->proceso=pid;
					clockNuevo->posClock=datosMarco->marco;
					list_remove_and_destroy_by_condition(tablaClocks,(void*)clockDelProceso,(void*)eliminarClock);
					list_add(tablaClocks,clockNuevo);
					a=datosMarco->marco;										//todo actualizar tlb, saqué uno :/
					datosMarco = list_find(tabla_de_paginas, (void*) marcoPosicion);
					actualizarTabla(datosMarco->pagina,pid,-1);


			/*		datosMarco->marco = -1;
					list_replace(tabla_de_paginas, (int) marcoPosicion, datosMarco);*/
					list_clean(listaFiltrada);
					return pos;}																					//La nueva posicion libre
				if(!primeraVuelta){
				vectorMarcos[pos]--;}
				cont++;
			} while (++i);
		}
		return -1;																						//No hay marcos para darle
	}





	/*	Con marcos en orden de llegada
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
int marcosAsignados(int pid, int operacion){
	int marcosDelProceso(traductor_marco* marco){
		return (marco->proceso==pid && marco->marco>=0);
	}
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
