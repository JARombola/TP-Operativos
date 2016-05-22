#include <commons/log.h>


void registrarError(char* programa, char* mensaje) {
t_log* archivoLog = log_create("Errores.log", programa, true,
		log_level_from_string("ERROR"));
log_error(archivoLog, mensaje);
log_destroy(archivoLog);
}

void registrarInfo(t_log* archivo, char* mensaje) {
log_info(archivo, mensaje);
}

void registrarTrace(char* programa, char* mensaje) {
t_log* archivoLog = log_create("Trace.log", programa, true, log_level_from_string("TRACE"));
log_trace(archivoLog, mensaje);
log_destroy(archivoLog);
}

void registrarDebug(char* programa, char* mensaje) {
t_log* archivoLog = log_create("Debug.log", programa, true,
		log_level_from_string("DEBUG"));
log_debug(archivoLog, mensaje);
log_destroy(archivoLog);
}

void registrarWarning(char* programa, char* mensaje) {
t_log* archivoLog = log_create("Warning.log", programa, true,
		log_level_from_string("WARNING"));
log_warning(archivoLog, mensaje);
log_destroy(archivoLog);
}

