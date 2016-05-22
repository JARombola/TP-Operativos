/*
 * ArchivosLogs.h
 *
 *  Created on: 21/5/2016
 *      Author: utnso
 */

#ifndef FUNCIONES_ARCHIVOSLOGS_H_
#define FUNCIONES_ARCHIVOSLOGS_H_
#include <commons/log.h>

void registrarError(t_log* archivo, char* mensaje);
void registrarInfo(t_log* archivo, char* mensaje);
void registrarTrace(t_log* archivo, char* mensaje);
void registrarDebug(t_log* archivo, char* mensaje);
void registrarWarning(t_log* archivo, char* mensaje);

#endif /* FUNCIONES_ARCHIVOSLOGS_H_ */
