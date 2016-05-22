#include "ArchivosLogs.h"

void registrarError(t_log* archivo, char* mensaje) {
	log_error(archivo, mensaje);
}

void registrarInfo(t_log* archivo, char* mensaje) {
	log_info(archivo, mensaje);
}

void registrarTrace(t_log* archivo, char* mensaje) {
	log_trace(archivo, mensaje);
}

void registrarDebug(t_log* archivo, char* mensaje) {
	log_debug(archivo, mensaje);
}

void registrarWarning(t_log* archivo, char* mensaje) {
	log_warning(archivo, mensaje);
}

