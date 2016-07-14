#!/bin/bash

cd
cd tp-2016-1c-CodeBreakers/swap

gcc swap.c Funciones/Comunicacion.c Funciones/Comunicacion.h -o swap -lcommons

./swap ./Config2
