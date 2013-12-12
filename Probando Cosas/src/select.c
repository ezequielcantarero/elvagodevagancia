/*
 * select.c
 *
 *  Created on: 12/12/2013
 *      Author: utnso
 */
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <syscall.h>
#include <sys/timeb.h>

#include "select.h"

#define AGRANDAMIENTO 10


typedef struct nodoLlegada{
	int socket;
	char simbolo;
}nodoLlegada;

typedef struct estructuraLiberar{
	int cantidad;
	char simbolo;
}estructuraLiberar;



int comparar(char*,const char*);
void crearNodo(nodo **lista,void *dato);
int sockets_create_Client(char *ip, int port);
int obtenerPosicion(char simboloPersonaje,int nroNivel);
int quitarDeColaReady(int nroNivel,int socket);
int quitarDeColaBloqueados(int nroNivel,int socket);
int quitarDeColaLlegada(nodo** lista,int socket);

//inicio listasublista prototipos
void ejecutarSiguienteColaReady(int nroNivel);
void logearColaReadyRunning(int nroNivel);
void liberNivel(int nroNivel, int socketNivel);
char* armarPayloadLiberar(int nroNivel);
int contarTipoRecursos(nodoRecursoLS* lista);
estructuraLiberar* obtenerRecursoPorIndice(nodoRecursoLS* lista,int indice);
int eliminarSocketListaPersonajeBloqueados(nodoPersonajeLS** listaPersonaje, int socket);
int buscarQuitarPersonajeColaBloqueadosPorRecurso(nodoRecursoLS* lista, int socket);
void resetearTodosLosRecursos(nodoRecursoLS* listaRecurso); //pone en 0 el contador de todos los recursos
void resetearRecurso(nodoRecursoLS* listaRecurso, char recurso); //pone en 0 el contador del recurso
void agregarRecurso(nodoRecursoLS** listaRecursos, char recurso);
nodoRecursoLS* crearNodoRecurso(char recurso);
void agregarNodoRecursoEnCola(nodoRecursoLS** listaRecursos,nodoRecursoLS* nodo);
nodoRecursoLS* buscarRecurso(nodoRecursoLS* lista,char recurso);
nodoPersonajeLS* crearNodoPersonaje(char personaje,int socket);
void agregarPersonajeEnColaDeRecurso(nodoRecursoLS* listaRecursos,char recurso,char personaje,int socketpersonaje);
void agregarNodoPersonajeEnCola(nodoPersonajeLS** listaPersonaje,nodoPersonajeLS* nodo);
void mostrarListaSubLista(nodoRecursoLS* lista);
void incrementarRecurso(nodoRecursoLS* listaRecursos, char recurso);
void decrementarRecurso(nodoRecursoLS* listaRecurso, char recurso);

void liberarTodosLosPersonajesDelRecurso(nodoRecursoLS* lista, char recurso);
int quitarPrimerNodoPersonaje(nodoPersonajeLS** listaPersonaje);
int quitarPrimerPersonajeDeColaBloqueadosPorRecurso(nodoRecursoLS* lista, char recurso);
//fin listasublista prototipos
int eliminarNodoNiveles(pndata** lista, char* nombreNivel);


/////////////////// Variables globales ///////////////////////
extern pndata* niveles;
extern vectorPlanificador* vectorPlanif;
extern int quantum;
extern char* bufferLogeo;
extern int personajesPendientes;
extern int ejecutarKoopa;
extern double tiempoRetardo;
pthread_mutex_t semaforito=PTHREAD_MUTEX_INITIALIZER;
extern tipoVectorLlegada *vectorLlegada;
/////////////////////////////////////////////////////////////
int sockets_create_Server(int port) {

	int socketFD;
	int backlog=10;
	struct sockaddr_in socketInfo;

	if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {

		  funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"Error al crear el Socket del server",__LINE__);
		   exit(-1);
	}

	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = INADDR_ANY;
	socketInfo.sin_port = htons(port);
	bzero(&(socketInfo.sin_zero),8);

	  funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Socket creado.",__LINE__);

	int on = 1;
	if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)

	  funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"setsockopt of SO_REUSEADDR error",__LINE__);

	// Asociar puerto
	bind(socketFD, (struct sockaddr*) &socketInfo, sizeof(socketInfo));

	if (listen(socketFD, backlog) == -1) {
		  funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"Error al escuchar por el puerto",__LINE__);
	}

	sprintf(bufferLogeo,"Escuchando conexiones entrantes en el puerto %d.",port);
	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	return socketFD;

}

void actualizarDescriptorMaximo(int socketEscucha, int *vectorclientesconectados, int tamaniovectorclientesconectados, int *descr_max){
	int i;
	int aux=0;
	snprintf(bufferLogeo,TAM_BUFFER,"TAMANIO VECTOR CLIENTES CONECTADOS: %d - DESCR MAX: %d",tamaniovectorclientesconectados,*descr_max);
	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	for(i=0;i<tamaniovectorclientesconectados;i++){//primero obtengo en aux el maximo del vector
		if (aux<vectorclientesconectados[i]){
			aux=vectorclientesconectados[i];
			snprintf(bufferLogeo,TAM_BUFFER,"DESCR MAX ACTUALIZADO A: %d",aux);
			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
		}
	}
	if (aux<socketEscucha){//obtengo el maximo entre el maximo del vector y el socketescucha
		aux=socketEscucha;
	}
	snprintf(bufferLogeo,TAM_BUFFER,"EL MAX AL FINAL TERMINA SIENDO: %d",aux);
	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	*descr_max=aux;//el resultado es el maximo total
}

void agregarNuevaConexionEnVectorClientesConectados(int socketNuevaConexion,int* vectorclientesconectados,int *tamaniovector){
	int i;
	for(i=0;i<*tamaniovector;i++){
		if(vectorclientesconectados[i]==0){
			vectorclientesconectados[i]=socketNuevaConexion;
			return;//si pude agregarlo salgo inmediatamente
		}
	}
	agrandarVectorSelect(vectorclientesconectados,*tamaniovector);
	//si no pude agregarlo es por que no tengo lugar en el vector; entonces agrando el vector y luego lo agrego

}

void cerrarConexionCliente(int socket,int* vectorclientesconectados,int tamaniovector){
	int i=0;
	int aux;
	for(i=0;i<tamaniovector;i++)
	{
		if (socket==vectorclientesconectados[i])
		{
			vectorclientesconectados[i]=0;//dejo libre la posicion del vector

			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"**ATENCION SE VA A CERRAR EL SOCKET DEL CLIENTE!!",__LINE__);

			snprintf(bufferLogeo,TAM_BUFFER,"Cerrando socket %d",socket);
			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			aux=close(socket);//cierro socket


			snprintf(bufferLogeo,TAM_BUFFER,"Resultado del close %d",aux);
			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

		}
	}
}

void inicializarVectorEn0(int* vectorclientesconectados,int tamaniovector){
	int i;
	for(i=0;i<tamaniovector;i++){
			vectorclientesconectados[i]=0;
	}
}

void *threadPlanificador(void *parametro) {

		hplan* estructura=(hplan*)parametro;
		int tamanioVectorClientes=50;//seteo un valor inicial para tamanioVectorClientes
		int vectorClientesConectados[tamanioVectorClientes];//el vector puede crecer en tiempo de ejecucion, por lo que tamanioVectorClientes es un parametro inicial
	    fd_set readset, masterset;
	    int socketEscucha = sockets_create_Server(estructura->puerto);
	    int flagSalir=0;
	    int indiceVectorPlanif=estructura->indiceVectorPlanif;
	    char nombreNivel[16];
	    strcpy(nombreNivel,estructura->nombreNivel);
	    vectorLlegada[indiceVectorPlanif].lista=NULL;


	    //establecemos la conexion antes del while, esto es muy
	    //importante, por que es necesario agregar un descriptor al set
	    //de manera tal que detecte algo, sino queda bloqueado

		FD_ZERO(&readset);
		FD_ZERO(&masterset);//inicializamos en cero el set, esto hay que hacerlo 1 sola vez fuera del while. Te borra el contenido de los descriptores
					        //esto va dentro del while FD_SET(socketEscucha, &readset); //aca seteamos el descriptor de la conexion que aceptamos anteriormente  //********esto debe hacerse con el descriptor del socket que escucha, y guardar los descriptores de las nuevas conexiones en el set debe estar dentro del while

		inicializarVectorEn0(vectorClientesConectados,tamanioVectorClientes);
		int descr_max=socketEscucha; //descr max siempre tiene el nro mayor de socket (recordemos que el socket es un numero) dentro del masterset. Me da la impresion que el select internamente hace un for y por eso necesita que le pasemo el nro de socket maximo+1 por que es tipico en el for comparar de la manera i<max

		FD_SET(socketEscucha, &masterset);//fundamental para que el masterset arranque con el socket de escucha

		//agrego la conexion del nivel
		int socketNivel;
		socketNivel=estructura->conexionNivel;
		FD_SET(socketNivel,&masterset);
		agregarNuevaConexionEnVectorClientesConectados(socketNivel,vectorClientesConectados,&tamanioVectorClientes);
		actualizarDescriptorMaximo(socketEscucha,vectorClientesConectados,tamanioVectorClientes,&descr_max);
		//fin agrego conexion nivel

		//libero memoria de la estructura
		//free(estructura);

		struct timeval TimeoutSelect; /* Ultimo parametro de select  */
		TimeoutSelect.tv_sec = 0;
	    TimeoutSelect.tv_usec = 100;

	    snprintf(bufferLogeo,TAM_BUFFER,"HILO PLANIFICADOR NIVEL: %s",nombreNivel);
	    funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	    while(1){

	    	int result;

	    	while(1)
	    	{
	    		//usleep(100000);

	    	 	readset=masterset; //fundamental, para que el select pueda volver a monitorear todos los descriptores necesarios
	    		result= select(descr_max+1, &readset, NULL, NULL, NULL); // el cien es para evitar problemas. En varios codigos vi que siempre guardaban el valor del maximo
					   	   	   	   	   	   	   	   	   	   	   	   	       //  descriptor a medida que se iban generando las conexiones nuevas
				if ( !(result<0) || (result < 0 && errno !=EINTR) )
				{
					//si entra en este IF significa que NO hubo error. el break le va a permitir segui ejecutando

					break;
				}

				//si llego hasta aca es que HUBO error y entonces pega la vuelta en while y queda nuevamente en el select
				 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"ERROR EN EL SELECT",__LINE__);
			}

			if (result > 0) //si encontro algun cambio en los descriptores
			{

				 if (FD_ISSET(socketEscucha, &readset)) //me fijo si hay nuevas conexiones mirando el descriptor del server
				 {
					//se agregan nuevas conexiones
					// aca se que son conexiones entrantes por que el cambio se produce en el socketescucha
					 int socketNuevaConexion;
					 socketNuevaConexion=accept(socketEscucha,0,0);
					 agregarNuevaConexionEnVectorClientesConectados(socketNuevaConexion,vectorClientesConectados,&tamanioVectorClientes);
					 FD_SET(socketNuevaConexion, &masterset);
					 actualizarDescriptorMaximo(socketEscucha,vectorClientesConectados,tamanioVectorClientes,&descr_max);

					 funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Se registro una nueva conexion",__LINE__);

					 sprintf(bufferLogeo,"NUEVA CONEXION SE AGREGA A READY socket: %d",socketNuevaConexion);
					 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

					 crearNodo(&(vectorPlanif[indiceVectorPlanif].listaReady),(void*)socketNuevaConexion);

					 if((vectorPlanif[indiceVectorPlanif].running)==0)
					 {
						 ejecutar(indiceVectorPlanif,nombreNivel);
					 }
				 }

				int i;

		        for (i=0; i<tamanioVectorClientes; i++) //recorro todos los descriptores de los clientes
				{

			       if ((vectorClientesConectados[i]!=0) && FD_ISSET(vectorClientesConectados[i], &readset)) //me fijo si alguno tiene datos en el descriptor
				   {

						//recibo datos de ese socket. si recibo 0 bytes significa que el cliente cerro la conexion y tengo que sacarlo del vectorClientesConectados
			    	    //luego proceso lo recibido y respondo o puedo esperar de recibir todo y despues responder todo junto.
			    	   char*buffer=malloc(5);
			    	   int recibido;
			    	   if((recibido=recv(vectorClientesConectados[i],buffer,5,0))>0)
			    	   {
			    		   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"RECIBIDO",__LINE__);
			    		   if(comparar(buffer,"AUMOV"))
			    		   {
			    			   usleep(tiempoRetardo*1000000);
			    			   if (vectorPlanif[indiceVectorPlanif].running==vectorClientesConectados[i] && vectorPlanif[indiceVectorPlanif].quantum>0)
			    			   {
									snprintf(bufferLogeo,TAM_BUFFER,"Se autoriza el movimiento para el personaje: %d en el nivel %s",vectorClientesConectados[i],nombreNivel);
									funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
								   send(vectorClientesConectados[i],"OK",3,0); //devuelvo lo que recibi. hago un simple eco para probar la conexion
								   vectorPlanif[indiceVectorPlanif].quantum--;
								   snprintf(bufferLogeo,TAM_BUFFER,"el valor del quantum es: %d",vectorPlanif[indiceVectorPlanif].quantum);
								   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			    			   }else if((vectorPlanif[indiceVectorPlanif].quantum==0) && (vectorPlanif[indiceVectorPlanif].running==vectorClientesConectados[i]))
			    			   {
			    				   snprintf(bufferLogeo,TAM_BUFFER,"SE AGOTO QUANTUM para el personaje: %d en el nivel %s, indiceVectorPlanif %d",vectorClientesConectados[i],nombreNivel,indiceVectorPlanif);
			    				   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			    				   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"LO PASO A READY Y EJECUTO EL SIGUIENTE PERSONAJE",__LINE__);
			    				   crearNodo(&(vectorPlanif[indiceVectorPlanif].listaReady),(void*)vectorPlanif[indiceVectorPlanif].running);
			    				   ejecutar(indiceVectorPlanif,nombreNivel);

			    			   }else{

			    			   }
			    		   }
			    		   else if(comparar(buffer,"REGIS"))
			    		   {
			    			   char simbolo;
			    			   personajesPendientes++;
			    			   nodoLlegada* auxSimbolo;
			    			   auxSimbolo=malloc(sizeof(nodoLlegada));
			    			   recv(vectorClientesConectados[i],&simbolo,1,0);

			    			   snprintf(bufferLogeo,TAM_BUFFER,"REGIS- Se registra personaje: %c en nivel %s nroSocket %d",simbolo,nombreNivel,vectorClientesConectados[i]);
			    			   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			    			   auxSimbolo->simbolo=simbolo;
			    			   auxSimbolo->socket=vectorClientesConectados[i];
			    			   crearNodo(&(vectorLlegada[indiceVectorPlanif].lista),(void*)auxSimbolo);
			    		   }

			    		   else if(comparar(buffer,"REGLI"))
			    		   {
			    			   snprintf(bufferLogeo,TAM_BUFFER,"---EL VALOR DE PERSONAJES PENDIENTES ES: %d REGLI---",personajesPendientes);
			    			   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
			    		   }

			    		   else if(comparar(buffer,"SIGNL"))
			    		   {
							   personajesPendientes++;
			    			   //si esta en running ejecuto el siguiente y descarto al que estaba en running
			    			   if(vectorPlanif[indiceVectorPlanif].running==vectorClientesConectados[i]){
								   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"SE QUITO DE RUNNING al personaje que debe morir por SIGNAL",__LINE__);
			    				   ejecutar(indiceVectorPlanif,nombreNivel);
			    			   }
			    			   //si estaba en la cola de ready lo quito
			    			   if(quitarDeColaReady(indiceVectorPlanif,vectorClientesConectados[i])){
								   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"Se quito de la cola de ready al personaje que debe morir",__LINE__);
							   }else{
								   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"ADVERTENCIA No se pudo quitar de la cola de ready al personaje que debe morir. Posiblemente estaba en running",__LINE__);
							   }
							   //quitar de la cola de bloqueados a la espera de un recurso
							   if(quitarDeColaBloqueados(indiceVectorPlanif,vectorClientesConectados[i])){
								   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"Se quito de la cola de BLOQUEADOS al personaje que debe morir",__LINE__);
							   }else{
								   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"ADVERTENCIA No se pudo quitar de la cola de ready al personaje que debe morir. Posiblemente estaba en running",__LINE__);
							   }

							   //desregistrar al personaje
							   snprintf(bufferLogeo,TAM_BUFFER,"SE DESREGISTRA PERSONAJE por muerte a causa de senial NroSocket: %d Nivel: %s",vectorClientesConectados[i],nombreNivel);
							   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
							   quitarDeColaLlegada(&(vectorLlegada[indiceVectorPlanif].lista),vectorClientesConectados[i]);

							   personajesPendientes--;

			    		   }
			    		   else if(comparar(buffer,"TERMI"))
						   {
							   snprintf(bufferLogeo,TAM_BUFFER,"---EL VALOR DE PERSONAJES PENDIENTES ES: %d TERMI---",personajesPendientes);
							   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
							   if(personajesPendientes==0)
							   {
								   int socketOrquestador;
								   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"EJECUTAR KOOPA",__LINE__);

								   ejecutarKoopa=1;
								   socketOrquestador=sockets_create_Client("127.0.0.1",15000);
								   send(socketOrquestador,"KOOPA",5,0);
							   }

						   }
			    		   else if(comparar(buffer,"DESRE"))
						   {
			    			   //desregistra al personaje de la cola de llegada al nivel
			    			   snprintf(bufferLogeo,TAM_BUFFER,"DESRE- SE DESREGISTRA PERSONAJE por finalizacion de nivel NroSocket: %d Nivel: %s",vectorClientesConectados[i],nombreNivel);
			    			   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
			    			   quitarDeColaLlegada(&(vectorLlegada[indiceVectorPlanif].lista),vectorClientesConectados[i]);

						   }
			    		   else if(comparar(buffer,"CLOSE"))
						   {
//							   FD_CLR(vectorClientesConectados[i],&masterset);
//							   close(vectorClientesConectados[i]);
//							   actualizarDescriptorMaximo(socketEscucha,vectorClientesConectados,tamanioVectorClientes,&descr_max);
						   }
			    		   else if (comparar(buffer,"FINIV"))
			    		   {
			    			   //ojo que aca vectorClientesConectados[i] no estamos con el socket del personaje sino con el socket de la conexion con el nivel
			    			   //este es un mensaje que nivel nos manda para liberar recursos de un personaje en particular

							   int cantidadRecursos,j;
							   char recurso;
								 resetearTodosLosRecursos(vectorPlanif[indiceVectorPlanif].listaRecursos); //pongo los contadores de los recursos en 0


								 snprintf(bufferLogeo,TAM_BUFFER,"El nivel: %s informa que se liberaron recursos...",nombreNivel);
								 funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

							     recv(vectorClientesConectados[i],&cantidadRecursos,4,0);

								 snprintf(bufferLogeo,TAM_BUFFER,"RECIBI FINIV-Cantidad de recursos a liberar: %d del nivel: %s",cantidadRecursos,nombreNivel);
								 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
							   for(j=0;j<cantidadRecursos;j++)
							   {
								   recv(vectorClientesConectados[i],&recurso,1,0);

									 snprintf(bufferLogeo,TAM_BUFFER,"Liberando recurso: %c del nivel: %s",recurso,nombreNivel);
									 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

								   liberarRecurso(indiceVectorPlanif,recurso,nombreNivel);
							   }
							   send(estructura->conexionNivel,"OK",3,0);

							   //modificacion lista sublista

							    liberNivel(indiceVectorPlanif,estructura->conexionNivel); //envio a nivel los recursos que debe incrementar

							    logearColaReadyRunning(indiceVectorPlanif);

							  	ejecutarSiguienteColaReady(indiceVectorPlanif);


							   if(ejecutarKoopa==1)
							   {
								   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"EJECUTANDO KOOPA!!",__LINE__);
								   break;
							   }


			    	       }else if(comparar(buffer,"MORIR")){
			    	    	   //ojo que aca vectorClientesConectados[i] no estamos con el socket del personaje sino con el socket de la conexion con el nivel
							   //este es un mensaje que nivel nos manda para liberar recursos de un personaje en particular

							   int cantidadRecursos,j;
							   char recurso;

							   snprintf(bufferLogeo,TAM_BUFFER,"---EL VALOR DE PERSONAJES PENDIENTES ES: %d MORIR---",personajesPendientes);
							   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

							   resetearTodosLosRecursos(vectorPlanif[indiceVectorPlanif].listaRecursos); //pongo los contadores de los recursos en 0

								 snprintf(bufferLogeo,TAM_BUFFER,"El nivel: %s informa que se liberaron recursos...",vectorPlanif[indiceVectorPlanif].nombreNivel);
								 funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

								 recv(vectorClientesConectados[i],&cantidadRecursos,4,0);

								 snprintf(bufferLogeo,TAM_BUFFER,"RECIBI MORIR-Cantidad de recursos a liberar: %d del nivel: %s",cantidadRecursos,vectorPlanif[indiceVectorPlanif].nombreNivel);
								 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
							   for(j=0;j<cantidadRecursos;j++)
							   {
								   recv(vectorClientesConectados[i],&recurso,1,0);

									 snprintf(bufferLogeo,TAM_BUFFER,"Liberando recurso: %c del nivel: %s",recurso,vectorPlanif[indiceVectorPlanif].nombreNivel);
									 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

								   liberarRecurso(indiceVectorPlanif,recurso,nombreNivel);
							   }
							   send(estructura->conexionNivel,"OK",3,0);

							   liberNivel(indiceVectorPlanif,estructura->conexionNivel);

							   logearColaReadyRunning(indiceVectorPlanif);


							  }

			    		   else if(comparar(buffer,"DEADL"))
			    		   {
			    			   //recibo la lista de bloqueados
			    			   nodo *listaEnDeadlock=NULL;
			    			   nodo *aux=NULL;//Solo para logueo
			    			   char personaje;
			    			   int auxposicion;
			    			   int bytesRecibidos;
			    			   int posicionMin=0;
			    			   int contadorNodos,z;
			    			   char personajeParaMatar='\0';
			    			   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"SE RECIBIO NOTIFICACION DE DEADLOCK",__LINE__);
			    			   recv(vectorClientesConectados[i],&contadorNodos,4,0);//recibo la cantidad de nodos (cada nodo es un caracter que representa el simbolo de un personaje involucrado en el deadlock) y esa cantidad la uso en el for para iterar

			    			   snprintf(bufferLogeo,TAM_BUFFER,"DEADLOCK-HAY %d PERSONAJES INVOLUCRADOS",contadorNodos);
			    			   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			    			   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"A continuacion la lista de personajes involucrados:",__LINE__);

			    			   for(z=0;z<contadorNodos;z++) //muestro una lista con los personajes involucrados y mientras itero resuelvo cual matar
			    			   {
			    				   bytesRecibidos=recv(vectorClientesConectados[i],&personaje,1,0); //voy recibiendo SIMBOLO POR SIMBOLO los personajes involucrados en el deadlock

			    				   snprintf(bufferLogeo,TAM_BUFFER,"DEADLOCK-Personaje: %c",personaje);
			    				   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			    				   crearNodo(&listaEnDeadlock,(void*)personaje);

			    				   //ahora me voy guardar el simbolo del personaje que llego ultimo al  nivel en la variable personajeMatar

			    				   if((auxposicion=obtenerPosicion(personaje,indiceVectorPlanif))>=0){ //primero checkeo que no haya error al obtener posicion
			    					   if (auxposicion<=posicionMin){//me voy guardando el personaje que esta en la minima posicion
										   posicionMin=auxposicion;
										   personajeParaMatar=personaje;
									   }


			    				   }else{ //si hay error al querer obtener la posicion del personaje
			    					   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"DEADLOCK-ERROR ** No se pudo obtener la posicion del personaje",__LINE__);
			    				   }
			    			   }

			    			   sprintf(bufferLogeo,"DEADLOCK-MATAR PERSONAJE: %c",personajeParaMatar);
			    			   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			    			   //aca arranca el circuito de dar notificacion de muerte al personaje

			    			   nodo* auxPersonaje=vectorLlegada[indiceVectorPlanif].lista;
			    			   while(auxPersonaje!=NULL) //busco en la lista de vectorLlegada correspondiente al nroNivel el socket correspondiente al simbolo del personaje que debe morir(el simbolo lo tengo lo que necesito buscar en la lista es el socket)
			    			   {

			    				   if(((nodoLlegada*)(auxPersonaje->data))->simbolo==personajeParaMatar){
			    					   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Envio morir al ultimo personaje en llegar al nivel involucrado",__LINE__);
			    					   send(((nodoLlegada*)(auxPersonaje->data))->socket,"MORIR",5,0);
			    					   //flagMuerte=1;
			    					   //quitar de la cola de ready al personaje que acabo de matar
			    					   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"Intentando quitar de la cola de ready...",__LINE__);
			    					   if(quitarDeColaReady(indiceVectorPlanif,((nodoLlegada*)(auxPersonaje->data))->socket)){
			    						   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"Se quito de la cola de ready al personaje que debe morir",__LINE__);
			    					   }
			    					   //quitar de la cola de bloqueados a la espera de un recurso
			    					  snprintf(bufferLogeo,TAM_BUFFER,"El conflicto esta en: %d",((nodoLlegada*)(auxPersonaje->data))->socket) ;
		  	  	  	  	  	  	  	  funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
			    					   if(quitarDeColaBloqueados(indiceVectorPlanif,((nodoLlegada*)(auxPersonaje->data))->socket)){
										   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"Se quito de la cola de BLOQUEADOS al personaje que debe morir",__LINE__);
									   }else{
										   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"ADVERTENCIA No se pudo quitar de la cola de bloqueados al personaje que debe morir.",__LINE__);
									   }

			    					   //desregistrar al personaje
					    			   snprintf(bufferLogeo,TAM_BUFFER,"SE DESREGISTRA PERSONAJE por muerte a causa de deadlock NroSocket: %d Nivel: %s",((nodoLlegada*)(auxPersonaje->data))->socket,nombreNivel);
					    			   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
					    			   quitarDeColaLlegada(&vectorLlegada[indiceVectorPlanif].lista,((nodoLlegada*)(auxPersonaje->data))->socket);

			    					   break;
			    				   }

			    				   char buffer[6];
			    				   char simboloPj=((nodoLlegada*)(auxPersonaje->data))->simbolo;
			    				   memcpy(buffer,"DESBL",5);
			    				   memcpy(buffer+5,&simboloPj,1);
			    				   snprintf(bufferLogeo,TAM_BUFFER,"LE MANDO DESBL AL PERSONAJE %c",simboloPj);
			    				   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
			    				   send(estructura->conexionNivel,buffer,6,0);

			    				   auxPersonaje=auxPersonaje->next;
			    			   }

			    		   	}
			    		   else if(comparar(buffer,"BLOQE"))
			    		   {
			    			   char *bufferAux=malloc(2);

			    			   recv(vectorClientesConectados[i],bufferAux,2,0);

			    			   snprintf(bufferLogeo,TAM_BUFFER,"SE BLOQUEO PERSONAJE: %c SOCKET: %d POR RECURSO: %c NIVEL: %s",bufferAux[1],vectorClientesConectados[i],bufferAux[0],nombreNivel);
			    			   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			    			   agregarPersonajeEnColaDeRecurso(vectorPlanif[indiceVectorPlanif].listaRecursos,bufferAux[0],bufferAux[1],vectorClientesConectados[i]);


			    				funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Ejecutar proximo personaje en cola ready",__LINE__);
			    				   if(vectorPlanif[indiceVectorPlanif].listaReady!=NULL)
			    				   {
			    					   ejecutar(indiceVectorPlanif,nombreNivel);//ejecuto el siguiente de la cola de ready
			    				   }
			    				   else
			    				   {
										snprintf(bufferLogeo,TAM_BUFFER,"En el nivel %s cola ready vacia. Running quedo en 0",nombreNivel);
										funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
										vectorPlanif[indiceVectorPlanif].running=0;
			    				   }


								 free(bufferAux);
			    		   }

			    	   }else if(recibido==0){
			    		   //el cliente cerro la conexion

			    		   if(vectorClientesConectados[i]==socketNivel){
			    			   //aviso se cerro la conexion con el nivel, el planificador se cierra usando break y el flagSalir
								 snprintf(bufferLogeo,TAM_BUFFER,"EL NIVEL %s CERRO LA CONEXION",nombreNivel);
								 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

					    		   FD_CLR(vectorClientesConectados[i],&masterset);//saco del masterset el socket por que sino revienta el select
					    		   cerrarConexionCliente(vectorClientesConectados[i],vectorClientesConectados,tamanioVectorClientes);
					    	       actualizarDescriptorMaximo(socketEscucha,vectorClientesConectados,tamanioVectorClientes,&descr_max);
					    	       flagSalir=1;
					    	       break;
			    		   }else{
								   snprintf(bufferLogeo,TAM_BUFFER,"EL PERSONAJE SOCKET: %d CERRO LA CONEXION",vectorClientesConectados[i]);
								   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

					    		   FD_CLR(vectorClientesConectados[i],&masterset);//saco del masterset el socket por que sino revienta el select
					    		   cerrarConexionCliente(vectorClientesConectados[i],vectorClientesConectados,tamanioVectorClientes);
					    	       actualizarDescriptorMaximo(socketEscucha,vectorClientesConectados,tamanioVectorClientes,&descr_max);
			    		   }



			    	   }//cierro else if

			    	   free(buffer);
				   }//cierro if


				}//cierro for


			}//cierro if



	///aca puedo responder todo junto si acumule algo

			 if (flagSalir==1) break;

	  } // cierro while(1)
	    snprintf(bufferLogeo,TAM_BUFFER,"FINALIZANDO HILO PLANIFICADOR NIVEL %s",nombreNivel);
	    funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	    eliminarNodoNiveles(&niveles,nombreNivel);


  return NULL;

}

void agrandarVectorSelect(int* vector,int nuevoTamanio)//modifico
{
	//vector=NULL;
	vector=realloc(vector,nuevoTamanio*sizeof (int));
	nuevoTamanio=nuevoTamanio+AGRANDAMIENTO;
}

void crearNodo(nodo **lista,void *dato){
	//crea un nodo para la lista de lineas parseadas en clave, valor
	nodo *aux;
	if(*lista==NULL){
			*lista=malloc(sizeof(nodo));
			(*lista)->data=dato;
			(*lista)->next=NULL;
			funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"la lista estaba vacia creo el primer nodo",__LINE__);

	}else{
			aux=*lista;

			while(aux->next!=NULL){
				aux=aux->next;
			}

			(aux->next)=malloc(sizeof(nodo));
			(aux->next)->data=dato;
			(aux->next)->next=NULL;
			funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"la lista no estaba vacia, inserto un nuevo nodo",__LINE__);
	}
}

/*recurso obtenerRecurso(char recu,int nroNivel)
{
	recurso aux;
	switch(recu)
	{
		case 'M':
			return vectorPlanif[nroNivel].moneda;
			break;

		case 'H':
			return vectorPlanif[nroNivel].hongo;
			break;

		case 'F':
			return vectorPlanif[nroNivel].flor;
			break;
	}
	return aux;
}*/

void insertarPrimeroEnLista(nodoPersonaje** lista, int dato)//dato es un socket
{
	// (*lista) apunta al primer nodo

		if(*lista ==NULL)
		{
			//la lista esta vacia
			(*lista)=malloc(sizeof(nodoPersonaje)); //linkeo el puntero a el primer nodo con la direccion de memoria del nuevo
			(*lista)->socketPersonaje=dato;
			(*lista)->next=NULL;
		}
		else
		{
			//la lista no esta vacia
			//inserto al principio
			nodoPersonaje *punteroAux;
			punteroAux=(*lista); //guardo la referencia al actual primer nodo de la lista, posteriormente sera el segundo.
			(*lista)=malloc(sizeof(nodoPersonaje)); //la lista apunta al nodo recien creado
			(*lista)->socketPersonaje=dato;
			(*lista)->next=punteroAux; //linkeo el nuevo nodo con el segundo de la lista, que antes era el primero
		}
}

int sacarDeListaDeReady(nodoPersonaje** lista,int unSocket)
{
		nodoPersonaje* fin=NULL;
		nodoPersonaje* aux=NULL;
		fin=(nodoPersonaje*)malloc(sizeof(nodoPersonaje));
		aux=(nodoPersonaje*)malloc(sizeof(nodoPersonaje));

		aux=(*lista);

		while(aux->next!=NULL)
		{
			aux=aux->next;
		}

		fin=aux;
		//free(aux);
		return (fin->socketPersonaje);
}

void liberarRecurso(int indiceVectorPlanif,char recurso,char* nombreNivel)
{
	 snprintf(bufferLogeo,TAM_BUFFER,"Se libera el recurso %c del nivel: %s",recurso,nombreNivel);
	 funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	 int aux;

	 if((aux=quitarPrimerPersonajeDeColaBloqueadosPorRecurso(vectorPlanif[indiceVectorPlanif].listaRecursos,recurso))>0)
	 {
		 //si habia personajes bloqueados meto el socket en la lista de Ready
		 crearNodo(&(vectorPlanif[indiceVectorPlanif].listaReady),(void*)aux);
		 snprintf(bufferLogeo,TAM_BUFFER,"DESBLOQUEO PERSONAJE - COLOCADO EN READY: %d  NRO NIVEL %s",aux,nombreNivel);
			 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	 }else{
		 //si no habia personajes bloqueados acumulo los recursos en un contador para luego informar al nivel cuanto tiene que liberar
		 incrementarRecurso(vectorPlanif[indiceVectorPlanif].listaRecursos,recurso);
	 }
	 logearColaReadyRunning(indiceVectorPlanif);

}

void desbloquearPersonaje(nodoPersonaje** lista,int nroNivel)
{

	 snprintf(bufferLogeo,TAM_BUFFER,"Se desbloquea personaje: %d en el nivel: %d - se coloca en la cola de ready",(*lista)->socketPersonaje,nroNivel);
	 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	 crearNodo(&(vectorPlanif[nroNivel].listaReady),(void*)((*lista)->socketPersonaje));
	 (*lista)=(*lista)->next;
}

void ejecutar(int indiceVectorPlanif,char *nombreNivel)
{
	if(vectorPlanif[indiceVectorPlanif].listaReady==NULL)
	{
		funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"Lista ready en NULL",__LINE__);
		   snprintf(bufferLogeo,TAM_BUFFER,"ESTADO DE Running antes de ejecutar: %d, indiceVectorPlanif %d", vectorPlanif[indiceVectorPlanif].running,indiceVectorPlanif);
		   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
		   vectorPlanif[indiceVectorPlanif].running=0;
		   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"RUNNING QUEDO EN 0",__LINE__);
		return;
	}
	   nodoPersonaje* auxN=vectorPlanif[indiceVectorPlanif].listaReady;
	   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Estado de la lista de READY antes de EJECUTAR",__LINE__);

	   while(auxN!=NULL)
	   {
		   snprintf(bufferLogeo,TAM_BUFFER,"Socket: %d", auxN->socketPersonaje);
		   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
		   auxN=auxN->next;
	   }
	   snprintf(bufferLogeo,TAM_BUFFER,"ESTADO DE Running: %d", vectorPlanif[indiceVectorPlanif].running);
	   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	vectorPlanif[indiceVectorPlanif].quantum=quantum - 1; //como el tipo va a ejecutar le resto ahora mismo el quantum para que no se arme quilombo
	nodoPersonaje* aux;
	aux = vectorPlanif[indiceVectorPlanif].listaReady;
	vectorPlanif[indiceVectorPlanif].running = vectorPlanif[indiceVectorPlanif].listaReady->socketPersonaje;
	vectorPlanif[indiceVectorPlanif].listaReady = vectorPlanif[indiceVectorPlanif].listaReady->next;

	snprintf(bufferLogeo,TAM_BUFFER,"Personaje en running socket: %d para el nivel: %s auxPersonaje %d",vectorPlanif[indiceVectorPlanif].running, nombreNivel,aux->socketPersonaje);
	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
int retornoSend;
	if((retornoSend=send(aux->socketPersonaje,"OK",3,0))>0){  //autorizo movimiento
		 snprintf(bufferLogeo,TAM_BUFFER,"se mando bien el send");
			 funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	}
	snprintf(bufferLogeo,TAM_BUFFER,"el valor del send es %d",retornoSend);
				 funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	 snprintf(bufferLogeo,TAM_BUFFER,"Se autorizo movimiento al personaje: %d (RECIEN PUESTO EN RUNNING) para el nivel: %s",vectorPlanif[indiceVectorPlanif].running, nombreNivel);
	 funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	free(aux);
}

void *funcionLogueo(int nivelDeDetalle,char *archivo,char *string_A_Logear, int lineNumber)
{
	FILE *arch;
	time_t ticks;
	ticks = time(NULL);
	struct timeb tmili;
	ftime(&tmili);

	/////////////////////////////////////////////Hecho para no usar ctime, que no haya salto de linea, ctime tiene salto de linea y sino el logueo queda con ese salto
	char timestamp[1000];

	struct tm * p = localtime(&ticks);

	strftime(timestamp, 1000, "%H:%M:%S", p);
	/////////////////////////////////////////////

	//ingresando a la region critica!/////
	pthread_mutex_lock(&semaforito);

	arch=fopen(archivo,"a");
	//la fecha se graba al final porque ya tiene el \n
	switch (nivelDeDetalle) {
	 case 1:
		 fprintf(arch,"%d [INFO] %s:%u Plataforma / (%ld:%ld): %s\n",lineNumber,timestamp,tmili.millitm,(long int)getpid(),(long int)syscall(SYS_gettid),strdup(string_A_Logear));
		 break;
	 case 2:
		 fprintf(arch,"%d [WARN] %s:%u Plataforma / (%ld:%ld): %s\n",lineNumber,timestamp,tmili.millitm,(long int)getpid(),(long int)syscall(SYS_gettid),strdup(string_A_Logear));
		 break;
	 case 3:
		 fprintf(arch,"%d [ERROR] %s:%u Plataforma / (%ld:%ld): %s\n",lineNumber,timestamp,tmili.millitm,(long int)getpid(),(long int)syscall(SYS_gettid),strdup(string_A_Logear));
		 break;
	 case 4:
		 fprintf(arch,"%d [DEBUG] %s:%u Plataforma / (%ld:%ld): %s\n",lineNumber,timestamp,tmili.millitm,(long int)getpid(),(long int)syscall(SYS_gettid),strdup(string_A_Logear));
		 break;
	 default:
	 	 break;
	}
	fclose(arch);

	pthread_mutex_unlock(&semaforito);
	//saliendo de la region critica!/////

	return NULL;
}


int sockets_create_Client(char *ip, int port)
{

	int socketFD;
	struct sockaddr_in socketInfo;

	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Conectando...",__LINE__);

	if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		   funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"Error al crear el socket.",__LINE__);
		   exit(-1);
	}

	socketInfo.sin_family = AF_INET;
	socketInfo.sin_addr.s_addr = inet_addr(ip);
	socketInfo.sin_port = htons(port);
	bzero(&(socketInfo.sin_zero),8);
	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Socket creado.",__LINE__);

	while(connect(socketFD, (struct sockaddr*) &socketInfo, sizeof(socketInfo))==-1)
	{
		funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"Falló la conexión, esperará 1 seg. y se intentará nuevamente",__LINE__);
		sleep(1);
		funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Intentando reconectar...",__LINE__);
	}

	return socketFD;
}

void desregistrar(int socket,int indiceVectorPlanif)
{
	nodo* aux;
	aux=vectorLlegada[indiceVectorPlanif].lista;
	nodo* nodoAnterior=aux;

	while(aux!=NULL)
	{
		if(((nodoLlegada*)(vectorLlegada[indiceVectorPlanif].lista->data))->socket==socket)
		{
			vectorLlegada[indiceVectorPlanif].lista=vectorLlegada[indiceVectorPlanif].lista->next;
			free(aux->data);
			free(aux);
		}
		else if(((nodoLlegada*)(aux->data))->socket==socket)
		{
			nodoAnterior->next=aux->next;
			free(aux->data);
			free(aux);
		}
		nodoAnterior=aux;
		aux=aux->next;
	}
}
int obtenerPosicion(char simboloPersonaje,int indiceVectorPlanif)
{
	int i=0;
	nodo* aux=vectorLlegada[indiceVectorPlanif].lista;

	while (aux!=NULL){
		if(((nodoLlegada*)(aux->data))->simbolo==simboloPersonaje)
		{
			return i;
		}
		i++;
		aux=aux->next;
	}

	return -1;
}

int quitarDeColaReady(int indiceVectorPlanif,int socket)//retorno 1 si encontre y elimine OK. Retorno 0 en caso de error
{
	nodoPersonaje* aux=vectorPlanif[indiceVectorPlanif].listaReady;
	nodoPersonaje* auxAnterior=aux;
	if(aux!=NULL){
		   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Habia datos en ready",__LINE__);
		   snprintf(bufferLogeo,TAM_BUFFER,"comparando %d %d",aux->socketPersonaje,socket);
		   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
		if(aux->socketPersonaje==socket){ //en caso que sea el primer nodo
			vectorPlanif[indiceVectorPlanif].listaReady=aux->next;
			free(aux);
			return 1;
		}

		while(aux!=NULL)
		{
			   snprintf(bufferLogeo,TAM_BUFFER,"comparando %d %d",aux->socketPersonaje,socket);
			   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
			if(aux->socketPersonaje==socket)
			{
				auxAnterior->next=aux->next;
				free(aux);
				return 1;
			}
			auxAnterior=aux;
			aux=aux->next;
		}
		 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"ADVERTENCIA Quiso quitarlo de la cola de ready pero no lo encontro",__LINE__);
	}
	 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"Cola READY VACIA",__LINE__);
	return 0;
}

int quitarDeColaBloqueados(int indiceVectorPlanif,int socket)//retorno 1 si encontre y elimine OK. Retorno 0 en caso de error
{

	funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"ENTRO",__LINE__);
return buscarQuitarPersonajeColaBloqueadosPorRecurso(vectorPlanif[indiceVectorPlanif].listaRecursos,socket);

}

int quitarDeColaLlegada(nodo** lista, int socket)//retorno 1 si encontre y elimine OK. Retorno 0 en caso de error
{
	nodo* aux=*lista;
	nodo* auxAnterior=aux;
	if(aux!=NULL){
		snprintf(bufferLogeo,TAM_BUFFER,"A CONTINUACION LISTA DE REGISTRADOS ANTES DE QUITAR PERSONAJE CON SOCKET: %d",socket);
		funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
		while(aux!=NULL)
		{
			snprintf(bufferLogeo,TAM_BUFFER,"PERSONAJE: %c SOCKET: %d",((nodoLlegada*)(aux->data))->simbolo,((nodoLlegada*)(aux->data))->socket);
			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			aux=aux->next;
		}

		aux=*lista;

		if(((nodoLlegada*)(aux->data))->socket == socket)
		{ //en caso que sea el primer nodo
		*lista=aux->next;
			free(aux);
			 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"DESREGISTRO AL PERSONAJE EXITOSAMENTE",__LINE__);
			return 1;
		}

		while(aux!=NULL)
		{

			if(((nodoLlegada*)(aux->data))->socket==socket)
			{
				auxAnterior->next=aux->next;
				free(aux);
				 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"DESREGISTRO AL PERSONAJE EXITOSAMENTE",__LINE__);
				return 1;
			}
			auxAnterior=aux;
			aux=aux->next;
		}
		 funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"FALLO AL DESREGISTRAR NO SE ENCONTRO EL NRO PARA DESREGISTRAR",__LINE__);
		 return 0;
	}else{
		funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"FALLO AL DESREGISTRAR LISTA VACIA",__LINE__);
		return 0;
	}
}

//LISTA SUBLISTA INICIO

void agregarRecurso(nodoRecursoLS** listaRecursos, char recurso){
	nodoRecursoLS* nodo=crearNodoRecurso(recurso);
	agregarNodoRecursoEnCola(listaRecursos,nodo);
}

void agregarPersonajeEnColaDeRecurso(nodoRecursoLS* listaRecursos,char recurso,char personaje,int socketPersonaje){
	nodoRecursoLS *aux;
	snprintf(bufferLogeo,TAM_BUFFER,"SE AGREGA A COLA DE BLOQUEADOS POR %cPERSONAJE: %c SOCKET: %d",recurso,personaje,socketPersonaje);
	funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	if((aux=buscarRecurso(listaRecursos,recurso))!=NULL)
	{
		agregarNodoPersonajeEnCola(&(aux->colaPersonajes),crearNodoPersonaje(personaje,socketPersonaje));
	}
}

void incrementarRecurso(nodoRecursoLS* listaRecurso, char recurso)
{
	nodoRecursoLS *aux;
	if((aux=buscarRecurso(listaRecurso,recurso))!=NULL)
	{
		aux->cantidad++;
	}
}

void resetearRecurso(nodoRecursoLS* listaRecurso, char recurso) //pone en 0 el contador del recurso
{
	nodoRecursoLS *aux;
	if((aux=buscarRecurso(listaRecurso,recurso))!=NULL)
	{
		aux->cantidad=0;
	}
}

void resetearTodosLosRecursos(nodoRecursoLS* listaRecurso) //pone en 0 el contador de todos los recursos
{
	nodoRecursoLS* aux=listaRecurso;
	while(aux!=NULL)
	{
		aux->cantidad=0;
		aux=aux->next;
	}
}

void decrementarRecurso(nodoRecursoLS* listaRecurso, char recurso)
{
	nodoRecursoLS *aux;
	if((aux=buscarRecurso(listaRecurso,recurso))!=NULL)
	{
		aux->cantidad--;
	}
}

nodoRecursoLS* crearNodoRecurso(char recurso)
{
	nodoRecursoLS* nodo=(nodoRecursoLS*)malloc(sizeof(nodoRecursoLS));
	nodo->recurso=recurso;
	nodo->cantidad=0;
	nodo->colaPersonajes=NULL;
	nodo->next=NULL;
	return nodo;
}

nodoPersonajeLS* crearNodoPersonaje(char personaje, int socketPersonaje)
{
	nodoPersonajeLS* nodo=(nodoPersonajeLS*)malloc(sizeof(nodoPersonajeLS));
	nodo->personaje=personaje;
	nodo->socketPersonaje=socketPersonaje;
	nodo->next=NULL;
	return nodo;
}

void agregarNodoRecursoEnCola(nodoRecursoLS** listaRecursos,nodoRecursoLS* nodo)
{
	if (*listaRecursos==NULL) //si la lista esta vacia lo agrego al principio
	{
		*listaRecursos=nodo;
	}else{ //sino lo agrego al final

		nodoRecursoLS *aux=*listaRecursos;
		while(aux->next!=NULL)//avanzo hasta estar parado en el ultimo nodo
		{
			aux=aux->next;
		}

		aux->next=nodo;
	}
}

void agregarNodoPersonajeEnCola(nodoPersonajeLS** listaPersonaje,nodoPersonajeLS* nodo)
{
	if (*listaPersonaje==NULL) //si la lista esta vacia lo agrego al principio
	{
		*listaPersonaje=nodo;
	}else{ //sino lo agrego al final

		nodoPersonajeLS *aux=*listaPersonaje;
		while(aux->next!=NULL)//avanzo hasta estar parado en el ultimo nodo
		{
			aux=aux->next;
		}

		aux->next=nodo;
	}
}

nodoRecursoLS* buscarRecurso(nodoRecursoLS* lista,char recurso)
{
	nodoRecursoLS* aux=lista;
	while(aux!=NULL && aux->recurso!=recurso)
	{
		aux=aux->next;
	}
	return aux;
}

void mostrarListaSubLista(nodoRecursoLS* lista)
{
	nodoPersonajeLS *aux=NULL;

	while(lista!=NULL)
	{
		printf("MOSTRANDO LISTA DE RECURSO %c CANTIDAD %d\n",lista->recurso,lista->cantidad);
		aux=lista->colaPersonajes;

		while(aux!=NULL)
		{
			printf("  PERSONAJE %c\n",aux->personaje);
			aux=aux->next;
		}

		lista=lista->next;
	}
}

int quitarPrimerPersonajeDeColaBloqueadosPorRecurso(nodoRecursoLS* lista, char recurso)
//esta funcion quita el primer personaje de la cola de bloqueados por recurso y devuelve su nro de socket,
{
	nodoRecursoLS *aux=NULL;
	if((aux=buscarRecurso(lista,recurso))!=NULL)
	{
		int socketPersonaje;
		socketPersonaje=quitarPrimerNodoPersonaje(&(aux->colaPersonajes));

		if(socketPersonaje>0)
		{
			return socketPersonaje; //retorno el socket del personaje quitado de la cola
		}else{
			return 0; //error no se quito correctamente el primer personaje o este tenia en nro de socket el valor invalido 0
		}

	}
	return 0; //error al buscar recurso
}

int buscarQuitarPersonajeColaBloqueadosPorRecurso(nodoRecursoLS* lista, int socket)
//esta funcion quita el primer personaje de la cola de bloqueados por recurso y devuelve su nro de socket,
{
	snprintf(bufferLogeo,TAM_BUFFER,"lista: %p socket: %d",lista,socket) ;
	funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	nodoRecursoLS *aux=lista;
	while(aux!=NULL)
	{//voy recorriendo recursos
		snprintf(bufferLogeo,TAM_BUFFER,"auxColaPersonajes: %p socket: %d",&(aux->colaPersonajes),socket) ;
		funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
		if(aux->colaPersonajes!=NULL && eliminarSocketListaPersonajeBloqueados(&(aux->colaPersonajes),socket)==1)
		{
			funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"SALIO POR 1",__LINE__);
			return 1; //devuelvo 1 por que se pudo encontrar y eliminar ese socket de alguna lista
		}
		aux=aux->next;
	}
	funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"SALIO POR 0",__LINE__);
	return 0; //devuelvo 0 por que no encontre al socket buscado en ninguna lista
}

int eliminarSocketListaPersonajeBloqueados(nodoPersonajeLS** listaPersonaje, int socket)
{
	nodoPersonajeLS *aux=*listaPersonaje;
	nodoPersonajeLS *auxAnterior=aux;
	if(aux->socketPersonaje!=NULL)
	{
		if(aux->socketPersonaje==socket)
		{
			funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"ENTRO PRIMER NODO",__LINE__);
			*listaPersonaje=aux->next;
			free(aux);
			return 1;
		}

		while(aux!=NULL)
		{
			if(aux->socketPersonaje==socket)
			{
				funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"AVANZO",__LINE__);
				auxAnterior->next=aux->next;
				free(aux);
				return 1;
			}

			auxAnterior=aux;
			aux=aux->next;
		}
	}

	return 0;
}

int quitarPrimerNodoPersonaje(nodoPersonajeLS** listaPersonaje)
{
	if(*listaPersonaje!=NULL)
	{
		nodoPersonajeLS* aux=*listaPersonaje;//me guardo una referencia al primer nodo
		*listaPersonaje=(*listaPersonaje)->next;//quito de la cadena al primer nodo

		int socketPersonaje=aux->socketPersonaje;//me guardo en una variable auxiliar el socket del personaje antes de hacer free
		free(aux);//libero la memoria del primer nodo

		return socketPersonaje;//retorno el dato del socket del personaje
	}
	return 0;
}

void liberarTodosLosPersonajesDelRecurso(nodoRecursoLS* lista, char recurso)
//esta es una funcion para test. lo que hace es liberar secuencialmente todos los personajes encolados por un recurso
{
	int aux;

	printf("SE VAN A DESBLOQUEAR TODOS LOS PERSONAJES DEL RECURSO: %c\n",recurso);

	while((aux=quitarPrimerPersonajeDeColaBloqueadosPorRecurso(lista,recurso)))
	{
		printf("  PERSONAJE DESBLOQUEADO: %d\n",aux);
	}
}

int contarTipoRecursos(nodoRecursoLS* lista)
{
	nodoRecursoLS *aux=lista;
	int cantidad=0;
	while(aux!=NULL)
	{
		cantidad++;
		aux=aux->next;
	}
	return cantidad;
}

estructuraLiberar* obtenerRecursoPorIndice(nodoRecursoLS* lista,int indice)
{
	nodoRecursoLS *aux=lista;
	int cantidad=0;
	while(aux!=NULL)
	{
		if(cantidad==indice)
		{
			estructuraLiberar *estructura=malloc(sizeof(estructuraLiberar));
			estructura->cantidad=aux->cantidad;
			estructura->simbolo=aux->recurso;
			return estructura;
		}
		cantidad++;
		aux=aux->next;
	}
	return 0;
}

char* armarPayloadLiberar(int indiceVectorPlanif)
{
	   estructuraLiberar *auxEstrLiber;//puntero auxiliar
	   int u;
	   int cantidadAux=contarTipoRecursos(vectorPlanif[indiceVectorPlanif].listaRecursos);//recupero el numero de tipo de recursos que tengo
	   char* bufferSendLiberar=malloc(cantidadAux*sizeof(estructuraLiberar)); //armo un buffer auxiliar

		snprintf(bufferLogeo,TAM_BUFFER,"CANTIDAD DE DE TIPOS DE RECURSOS: %d",cantidadAux);
		funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	   for(u=0;u<cantidadAux;u++)
	   {

		   auxEstrLiber=obtenerRecursoPorIndice(vectorPlanif[indiceVectorPlanif].listaRecursos,u);
		   snprintf(bufferLogeo,TAM_BUFFER,"ESTRLIBER CANTIDAD: %d SIMBOLO: %c",auxEstrLiber->cantidad,auxEstrLiber->simbolo);
		   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
		   memcpy(bufferSendLiberar+(u*(sizeof(estructuraLiberar))),auxEstrLiber,sizeof(estructuraLiberar));
		   free(auxEstrLiber);//libero por que ya lo concatene a buffersendliberar
	   }
	   char* bufferSendLiberar2=malloc(4+cantidadAux*sizeof(estructuraLiberar));

	   //concateno en otro buffer el int y buffersendliberar
	   memcpy(bufferSendLiberar2,&cantidadAux,4);
	   memcpy(bufferSendLiberar2+4,bufferSendLiberar,cantidadAux*sizeof(estructuraLiberar));
	   free(bufferSendLiberar);//libero por que ya lo copie en buffersendliberar2
	   return bufferSendLiberar2;
}

void liberNivel(int indiceVectorPlanif, int socketNivel)
{
	   int cantidadAux=contarTipoRecursos(vectorPlanif[indiceVectorPlanif].listaRecursos);
	   int tambuff=4+cantidadAux*sizeof(estructuraLiberar);
	   char* bufferSendLiberar2=armarPayloadLiberar(indiceVectorPlanif);
	   int resSend=send(socketNivel,"LIBER",5,0);
	   personajesPendientes--;
	   snprintf(bufferLogeo,TAM_BUFFER,"---EL VALOR DE PERSONAJES PENDIENTES ES: %d LIBER---",personajesPendientes);
	   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	   snprintf(bufferLogeo,TAM_BUFFER,"Le digo al nivel: %s que libere los recursos. Resultado del send: %d",vectorPlanif[indiceVectorPlanif].nombreNivel,resSend);
	   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	   resSend=send(socketNivel,bufferSendLiberar2,tambuff,0);

	   snprintf(bufferLogeo,TAM_BUFFER,"Mando estructura recursos. Resultado del send: %d", resSend);
	   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

}

void logearColaReadyRunning(int indiceVectorPlanif)
{
	   nodoPersonaje* auxN=vectorPlanif[indiceVectorPlanif].listaReady;
	   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"Estado de la lista de READY al momento de LIBERAR",__LINE__);

	   while(auxN!=NULL)
	   {
		   snprintf(bufferLogeo,TAM_BUFFER,"Socket: %d", auxN->socketPersonaje);
		   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
		   auxN=auxN->next;
	   }
	   snprintf(bufferLogeo,TAM_BUFFER,"En running socket: %d", vectorPlanif[indiceVectorPlanif].running);
	   funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
}

void ejecutarSiguienteColaReady(int indiceVectorPlanif)
{
	   funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"EJECUTAR SIGUIENTE PERSONAJE EN COLA DE READY",__LINE__);
	   if(vectorPlanif[indiceVectorPlanif].listaReady!=NULL)
	   {
		   ejecutar(indiceVectorPlanif,vectorPlanif[indiceVectorPlanif].nombreNivel);
	   }
	   else
	   {
			sprintf(bufferLogeo,"En el nivel %s cola ready vacia. Running quedo en 0",vectorPlanif[indiceVectorPlanif].nombreNivel);
			funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
			vectorPlanif[indiceVectorPlanif].running=0;
	   }
}

//LISTA SUBLISTA FIN

int eliminarNodoNiveles(pndata** lista, char* nombreNivel)
{
	pndata *aux=*lista;
	pndata *auxAnterior=aux;
	if(aux!=NULL)
	{
		if(aux->nombreNivel==nombreNivel)
		{
			funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"ENTRO PRIMER NODO",__LINE__);
			*lista=aux->next;
			free(aux);
			return 1;
		}

		while(aux!=NULL)
		{
			if(aux->nombreNivel==nombreNivel)
			{
				funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,"AVANZO",__LINE__);
				auxAnterior->next=aux->next;
				free(aux);
				return 1;
			}

			auxAnterior=aux;
			aux=aux->next;
		}
	}

	return 0;
}


