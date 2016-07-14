#!/bin/bash

cd
cd workspace/tp-2016-1c-CodeBreakers/swap

gcc swap.c Funciones/Comunicacion.c Funciones/Comunicacion.h -o swap -lcommons -lpthread

./swap ./Config1
