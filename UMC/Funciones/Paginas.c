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
	traductor_marco* encontrada = list_find(tabla_de_paginas,(void*) paginaBuscada);
	if (encontrada != NULL) {				//Esta "registrada" la pag (Existe)
		void* datos = (void*) malloc(datosMemoria->marco_size);
		if (encontrada->marco >= 0) {						//Está en memoria
			posicion=encontrada->marco * datosMemoria->marco_size;
		} else {					//todo no está en memoria => peticion a swap
			char* pedido = string_new();
			string_append(&pedido, "2");
			string_append(&pedido, (char*)header(proceso));
			string_append(&pedido, (char*)header(pag));
			string_append(&pedido, "\0");
			send(conexionSwap, pedido, string_length(pedido), 0);
			free(pedido);
			recv(conexionSwap, datos, datosMemoria->marco_size, 0);
			posicion=guardarPagina(datos, proceso, pag);
			posicion*=datosMemoria->marco_size;
		}
		return posicion;								//devuelve la posicion dentro de la "memoria"
	}
	printf("No existe la pagina solicitada\n");
	return -1;
}

void actualizarTabla(int pag, int proceso, int marco){
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
		if (datosMemoria->algoritmo){
		list_replace(tabla_de_paginas,i,traductorMarco);}
		else{
			list_remove(tabla_de_paginas,i);
			list_add(tabla_de_paginas,traductorMarco);
		}
	}
	else{																//Sino la registro
	traductorMarco->pagina=pag;
	traductorMarco->proceso=proceso;
	traductorMarco->marco=marco;
	traductorMarco->modificada=0;
	printf("Pag %d, proceso %d, marco %d\n",traductorMarco->pagina,traductorMarco->proceso,traductorMarco->marco);
	list_add(tabla_de_paginas,traductorMarco);}
}


int guardarPagina(void* datos,int proceso,int pag){
	int marco,tamMarco=datosMemoria->marco_size;
	marco = buscarMarcoLibre(proceso);
	if (marco==-1){								//no hay marcos para darle => hay que eliminar el proceso
		return -1;
	}
	memcpy(memoria + (marco * tamMarco), datos, tamMarco);//
	actualizarTabla(pag, proceso, marco);
//	printf("Paginas Necesarias:%d , TotalMarcosGuardados: %d\n",paginasNecesarias,i);
//	printf("TablaDePaginas:%d\n",list_size(tabla_de_paginas));
	return marco;
}

int buscarMarcoLibre(int pid) {
	int pos = 0, cantMarcos = marcosAsignados(pid, 1);
	int menorMayor(traductor_marco* marco1, traductor_marco* marco2){
		return marco1->marco<marco2->marco;
	}
	int marcoDelProceso(traductor_marco* marco) {
		return (marco->proceso == pid && marco->marco >= 0);										//está en memoria
	}
	int marcoPosicion(traductor_marco* marco) {													//porque la pos que ocupa y el marco no son el mismo, necesito el marco con esa posicion
		return (marco->marco == pos);
	}
	int marcoNuevo(traductor_marco* marco) {
		return (vectorMarcos[marco->marco] == 2);
	}
	int marcoViejo(traductor_marco* marco) {
		return (vectorMarcos[marco->marco] == 1);
	}
	void eliminarEntrada(traductor_marco* marco){
		free(marco);
	}

	if (cantMarcos < datosMemoria->marco_x_proc) {												//cantidad de marcos del proceso para ver si reemplazo, o asigno vacios
		for (pos = 0; pos < datosMemoria->marcos; pos++) {											//Se fija si hay marcos vacios
			if (!vectorMarcos[pos]) {
				vectorMarcos[pos] = 2;
				return pos;
			}
		}
	}
	if (cantMarcos) {																			//Si tiene marcos => hay que reemplazarle uno, SINO no hay espacio
		traductor_marco* datosMarco = malloc(sizeof(traductor_marco));
		t_list* listaFiltrada = list_filter(tabla_de_paginas,
				(void*) marcoDelProceso);														//Filtro los marcos de ESE proceso

		int i = 0,primeraVuelta=0,modificada=0;
		if (datosMemoria->algoritmo) {
			primeraVuelta=1;
			list_sort(listaFiltrada,(void*)menorMayor);				//CLOCK MEJORADO
			if (list_all_satisfy(listaFiltrada, (void*) marcoNuevo)) {								//Si son todas pags nuevas arranco x la primera (FIFO)
				i = 0;
			} else {
				int encontrado=0;
				datosMarco = list_find(listaFiltrada, (void*) marcoViejo);							//Sino, busco el primero disponible para sacar
				pos = datosMarco->marco;
				for (i = 0; !encontrado; i++) {
					datosMarco = list_get(listaFiltrada, i);
					if (datosMarco->marco == pos) {
						encontrado = 1;
						i--;
					}
				}
			}
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
					char* mje = string_new();
					string_append(&mje, "1");
					string_append(&mje, header(datosMarco->proceso));
					string_append(&mje,header(0));
					string_append(&mje,	memoria+ datosMarco->marco* datosMemoria->marco_size);
					string_append(&mje, header(datosMarco->pagina));
					string_append(&mje, header(0));
					string_append(&mje, "\0");
					send(conexionSwap, mje, string_length(mje), 0);
					free(mje);
				}
				vectorMarcos[pos] = 2;
				printf("Marco eliminado: %d\n", pos);
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
