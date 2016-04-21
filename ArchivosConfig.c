#include <stdlib.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/txt.h>
#include <stdio.h>

#define AGREGARBN(palabra) strcat(palabra,"\n")				//char* le agrega \n

int menuConfig() {
	int opc;
	char *ruta;
	printf("1=Crear config\n2=Leer Config\n");
	scanf("%d", &opc);
	ruta = string_new();
	ruta = malloc(100);
	printf("Ingresar direccion: ");
	scanf("%s", ruta);
	switch (opc) {
	case 1:
		crearArchivoTexto(ruta);
		break;
	case 2:
		leerConfig(ruta);
		break;
	default:
		printf("Bye...");
	}
	return 0;
}

void crearArchivoTexto(char *ruta) {
	char* texto,*buffer;
	strcat(ruta,"/config.txt");
	FILE* archivoTxt = txt_open_for_append(ruta);
	int i;
	for (i = 0; i <= 2; i++) {
		printf("Datos a escribir: ");
		texto = string_new();
		scanf("%s", texto);
		AGREGARBN(texto);
		txt_write_in_file(archivoTxt, texto);
	}
	rewind(archivoTxt);
	for (i=0;i<=2;i++){
		fread(buffer,30,1,archivoTxt);
		txt_write_in_stdout(buffer);
	}
	txt_close_file(archivoTxt);

	printf("Guardado en: %s\n",ruta);
}

void leerConfig(char *ruta) {
	int cantidadKeys;
	t_config *archivoConfiguracion = config_create(ruta);//Crea struct de configuracion
	cantidadKeys = config_keys_amount(archivoConfiguracion);
	buscarKeyEspecifica(archivoConfiguracion);
	/*	/if (cantidadKeys != 9) {
		perror("ERROR CANTIDAD DATOS DE CONFIGURACION");
	}
	{
		//*******---------------LO QUE SE HAGA CON LAS KEYS----------------*****
	}*/
	config_destroy(archivoConfiguracion);		//Borra struct (limpia memoria)
//	buscarKeyEspecifica(archivoConfiguracion);
}

void buscarKeyEspecifica(t_config* archivoConfiguracion) {
	int valorKey;
	char* buscar = string_new();
	printf("Palabra a buscar: ");
	scanf("%s", buscar);
	if (config_has_property(archivoConfiguracion, buscar)) {
		valorKey = config_get_int_value(archivoConfiguracion, buscar);
		printf("Valor: %d \n", valorKey);
	} else {
		printf("Key no encontrada\n");
	}
}

