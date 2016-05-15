#include "sockets.h"

int main(){

  printf("Empezo\n");
  int servidor = crearServidor(6040);

	printf("servidor afuera: %d \n", servidor);

	int cliente = esperarConexion(&servidor);



  if (&cliente > 0){
	  printf("recibi un cliente ok \n");
  }
  else{
	  printf("FAIL CLIENTES \n");
  }
  return 0;
}


