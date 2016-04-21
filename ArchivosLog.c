#include <commons/log.h>

void menuLogs() {
	int opcion;
	char *mensaje, *programa;
	mensaje = string_new();
	programa = string_new();
	printf("Mensaje: ");
	scanf("%d",&opcion);
	programa = string_new();
	programa = "Prueba";
	switch (opcion) {
	case 1:
		registrarError(programa, "Feo: ingresó un 1");
		break;
	case 2:
		registrarInfo(programa, "Ingresó un 2");
		break;
	case 3:
		registrarWarning(programa, "Epa... ingresó un 3");
	break;
}

}

void registrarError(char* programa, char* mensaje) {
t_log* archivoLog = log_create("Errores.log", programa, true,
		log_level_from_string("ERROR"));
log_error(archivoLog, mensaje);
log_destroy(archivoLog);
}

void registrarInfo(char* programa, char* mensaje) {
t_log* archivoLog = log_create("Info.log", programa, true,
		log_level_from_string("INFO"));
log_info(archivoLog, mensaje);
log_destroy(archivoLog);
}

void registrarTrace(char* programa, char* mensaje) {
t_log* archivoLog = log_create("Trace.log", programa, true,
		log_level_from_string("TRACE"));
log_info(archivoLog, mensaje);
log_destroy(archivoLog);
}

void registrarDebug(char* programa, char* mensaje) {
t_log* archivoLog = log_create("Debug.log", programa, true,
		log_level_from_string("DEBUG"));
log_info(archivoLog, mensaje);
log_destroy(archivoLog);
}

void registrarWarning(char* programa, char* mensaje) {
t_log* archivoLog = log_create("Warning.log", programa, true,
		log_level_from_string("WARNING"));
log_info(archivoLog, mensaje);
log_destroy(archivoLog);
}

