/*
 ============================================================================
 Name        : Probando.c
 Author      :
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#include <sys/inotify.h>
#include "select.h"


#define PUERTOORQUESTADOR 15000
#define MAXIMO_CLIENTES 10      // El número máximo de conexiones permitidas  (máximo de clientes conectados simultáneamente a nuestro servidor).
#define MAXIMODENIVELES 50
#define AGRANDAMIENTO 10//modifico
////////////////////////////////////////////////////////


////////////////////// INOTIFY//////////////////////////
#define EVENT_SIZE  ( sizeof (struct inotify_event)+17 )
#define BUF_LEN     ( 10 * ( EVENT_SIZE) )
////////////////////////////////////////////////////////
//tipos


struct registro{
	char nombreNivel[16];
	char ip[16];
	int puerto;
} __attribute__((packed));

typedef struct registro reg;



//------------------------------------prototipos
int sockets_create_Server(int);
void *hiloPlanificador (void *);
int comparar(char*,const char*);
int esperarConexion(int socketEscucha,struct sockaddr* datosDelCliente);
void registrarNivelYlanzarHiloDePlanificador(int socketConexion);
char* obtenerIP(int unSocket);
void agrandarVectorPlataforma(pndata* vector,int nuevoTamanio);//modifico
void leerArchivoConfiguracion();
void lanzarHiloINOTIFY();
void* hiloInotify(void* p);

//----------------------modificacion lista sublista
void agregarRecurso(nodoRecursoLS** listaRecursos, char recurso);
nodoRecursoLS* crearNodoRecurso(char recurso);
void agregarNodoRecursoEnCola(nodoRecursoLS** listaRecursos,nodoRecursoLS* nodo);
//----------------------
pndata* crearNodoNivel(char* nombreNivel,char* ipnivel,int puertoNivel,int puertoPlanificador);
void agregarNodoNivelEnLista(pndata** listaNiveles,pndata* nodo);
pndata* buscarNivel(pndata* lista,char* nombreNivel);
int buscarLibreVectorPlanif();

//------------------------------------fin prototipos
//////////////////////// variables globales /////////////////////////////////

pndata* niveles;
int cantNivelesRegistrados=0;//modifico
char* miIP=NULL;
vectorPlanificador* vectorPlanif;
int quantum;
char* bufferLogeo;
int personajesPendientes=0;
int ejecutarKoopa=0;
double tiempoRetardo;
int puertoPlanificador=16000;
tipoVectorLlegada *vectorLlegada;
char* PathArchivoConfig=NULL;
char* ConfigKoopa=NULL;
char bufferInotify[100];
////////////////////////////////////////////////////////////////////////////

int main (int argc, char **argv){

	int socketEscucha,socketNuevaConexion;
	char buffer[10];
	miIP=(char*)malloc(sizeof(char)*16);
	bufferLogeo=malloc(TAM_BUFFER);
	vectorPlanif=calloc(MAXIMODENIVELES,sizeof(vectorPlanificador));
	vectorLlegada=malloc(MAXIMODENIVELES*sizeof(tipoVectorLlegada));
	PathArchivoConfig=calloc(100,1);
	ConfigKoopa=calloc(100,1);
	if(argc>2)
	{
		strcpy(PathArchivoConfig,argv[1]);
		strcpy(ConfigKoopa,argv[2]);
		strcpy(bufferInotify,"/home/utnso/tp-20131c-so-pa/Plataforma/");
		strcat(bufferInotify,PathArchivoConfig);
	}
	else if(argc>1)
	{
		printf("Koopa se ejecutara en modo de prueba, se considera la carpeta git para el INOTIFY\n");
		strcpy(PathArchivoConfig,argv[1]);
		strcpy(ConfigKoopa,"../../Koopa/archivo.lst");
		strcpy(bufferInotify,"/home/utnso/git/tp-20131c-so-pa/Plataforma/");
		strcat(bufferInotify,PathArchivoConfig);
	}
	else
	{
		strcpy(PathArchivoConfig,"plataforma");
		strcpy(ConfigKoopa,"../../Koopa/archivo.lst");
		strcpy(bufferInotify,"/home/utnso/git/tp-20131c-so-pa/Plataforma/Debug");
	}

	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Leyendo configuracion...",__LINE__);
	leerArchivoConfiguracion();

	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Iniciando hilo INOTIFY...",__LINE__);
	lanzarHiloINOTIFY();
	socketEscucha=sockets_create_Server(PUERTOORQUESTADOR);

	while(1)
	{
		socketNuevaConexion= esperarConexion(socketEscucha,NULL);
		miIP = obtenerIP(socketNuevaConexion);

		sprintf(bufferLogeo,"IP propia obtenida: %s",miIP);
		funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

		recv(socketNuevaConexion,buffer,5,0);

		if(comparar(buffer,"NIVEL"))
		{
			int cantidadCaracteres;
			pndata*aux;

			recv(socketNuevaConexion, &cantidadCaracteres, 4, 0);

			char* auxBuffer=malloc(cantidadCaracteres);

			recv(socketNuevaConexion, auxBuffer, cantidadCaracteres, 0);

			snprintf(bufferLogeo,TAM_BUFFER,"Se recibio solicitud de nivel: %s",auxBuffer);
			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			if((aux=buscarNivel(niveles,auxBuffer))!=0){
			send(socketNuevaConexion,aux, sizeof(pndata), 0);

			snprintf(bufferLogeo,TAM_BUFFER,"Se enviaron datos del nivel: %s",aux->nombreNivel);
			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

			}else {
				send(socketNuevaConexion,"WAIT", 4, 0);

				funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Se respondio WAIT",__LINE__);
			}

			close(socketNuevaConexion);

		} else if(comparar(buffer,"REGIS"))
		{

			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Registrando nivel. Lanzando hilo planificador",__LINE__);
			registrarNivelYlanzarHiloDePlanificador(socketNuevaConexion);

		}
		else if(comparar(buffer,"KOOPA"))
		{
			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"EL ORQUESTADOR RECIBIO KOOPA!!",__LINE__);
			if(ejecutarKoopa==1)
			{
				funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"EL ORQUESTADOR ANTES DEL BREAK KOOPA!!",__LINE__);
				break;
			}
		}

	}
	if(execl("../Koopa/koopa","./koopa",ConfigKoopa,NULL)<0)
	{
		funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"ERROR AL ABRIR KOOPA, REINTENTANDO OTRA RUTA DE ARCHIVO",__LINE__);
		execl("../../Koopa/koopa","./koopa","../../Koopa/archivo.lst",NULL);
	}

	pthread_exit(0);

}

/*void *hiloPlanificador(void *parametro)
{
	char variable[6];
	variable[5]='\0';
	reg estrucRegistro;

	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Nuevo hilo",__LINE__);

	recv((int)parametro, variable, 5, 0);

    while(1)
    {

		if (comparar(variable,"REGIS"))
		{
			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Recibiendo datos de registro.",__LINE__);
			recv((int)parametro, &estrucRegistro, sizeof(reg), 0);

			agregarNodoNivelEnLista(&niveles,crearNodoNivel(estrucRegistro.nombreNivel,estrucRegistro.ip,estrucRegistro.puerto,puertoPlanificador));
			puertoPlanificador++;

			send((int)parametro,"REGISTRADO", 11, 0);
			snprintf(bufferLogeo,TAM_BUFFER,"El nivel: %s se ha registrado exitosamente",estrucRegistro.nombreNivel);
			funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
		}
		recv((int)parametro, variable, 5, 0);
    }
    funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,variable,__LINE__);
    return NULL;
}*/


int esperarConexion(int socketEscucha,struct sockaddr* datosDelCliente)
{
	int socketNuevaConexion;

	socklen_t len= sizeof(struct sockaddr_in);
	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Esperando conexiones entrantes",__LINE__);
	if ((socketNuevaConexion = accept(socketEscucha, (struct sockaddr*)datosDelCliente, &len)) == -1) //se acepta la conexion
	{
		funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"Error al aceptar conexion.",__LINE__);
		return -1;
	}

	funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,"Conexion aceptada",__LINE__);
	return socketNuevaConexion;
}

void registrarNivelYlanzarHiloDePlanificador(int socketConexion)
{
	reg estructuraRegistro;
	int cantidadRecursos;
	recv(socketConexion,&estructuraRegistro,sizeof(reg),0);
	recv(socketConexion,&cantidadRecursos,4,0);
	char* bufferRecursos=malloc(cantidadRecursos);

	estructuraRegistro.puerto=puertoPlanificador+1000;
	snprintf(bufferLogeo,TAM_BUFFER,"Registrando nivel: %s Puerto Nivel: %d",estructuraRegistro.nombreNivel,estructuraRegistro.puerto);
	funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	//registroNivel
	agregarNodoNivelEnLista(&niveles,crearNodoNivel(estructuraRegistro.nombreNivel,estructuraRegistro.ip,estructuraRegistro.puerto,puertoPlanificador));
	puertoPlanificador++;
	int indiceVectorPlanif=buscarLibreVectorPlanif();
	strcpy(vectorPlanif[indiceVectorPlanif].nombreNivel,estructuraRegistro.nombreNivel);
	vectorPlanif[indiceVectorPlanif].listaReady=NULL;
	vectorPlanif[indiceVectorPlanif].listaRecursos=NULL;
	vectorPlanif[indiceVectorPlanif].running=0;
	vectorPlanif[indiceVectorPlanif].quantum=quantum;

	recv(socketConexion,bufferRecursos,cantidadRecursos,0);
	snprintf(bufferLogeo,TAM_BUFFER,"+++ SE RECIBEN LOS %d RECURSOS +++  %s",cantidadRecursos,bufferRecursos);
	funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

	int u;
	for(u=0;u<cantidadRecursos;u++)
	{
		agregarRecurso(&(vectorPlanif[indiceVectorPlanif].listaRecursos),bufferRecursos[u]);
		snprintf(bufferLogeo,TAM_BUFFER,"Se creo recurso: %c",bufferRecursos[u]);
		funcionLogueo(DEBUG,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	}

	//fin registro nivel
	cantNivelesRegistrados++;//modifico

	char buffer[14];
	memcpy(buffer,"REGISTRADO",10);
	memcpy(buffer+10,&estructuraRegistro.puerto,4);
	send((int)socketConexion,buffer, 14, 0);

	//levanto thread planificador y le paso el puerto de escucha
	pthread_t tid;
	hplan *estructura=malloc(sizeof(hplan));
	estructura->puerto=puertoPlanificador-1;
	estructura->conexionNivel=socketConexion;
	estructura->indiceVectorPlanif=indiceVectorPlanif;
	strcpy(estructura->nombreNivel,estructuraRegistro.nombreNivel);


	pthread_create(&tid,NULL,threadPlanificador,(void*)estructura);

}

int comparar(char *str1,const char *str2)
{
	if(str1!=NULL)
	{
	return !(strncmp(str1,str2,strlen(str2)));
	}else{
		//funcionLogueo(DEBUG,PathArchivoLogeo,"ERROR AL COMPARAR CONTRA ALGO NULO",__LINE__);
		return 0;
	}
}

char* obtenerIP(int unSocket)
{
	struct sockaddr_in addr;
	char* ip=malloc(16);
	socklen_t socklen = sizeof(addr);
	if (getpeername(unSocket, (struct sockaddr*) &addr, &socklen) < 0) {
			   funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"Error al retornar la IP del socket",__LINE__);
			}
			else {
			    //printf("IP: %s\n", inet_ntoa(addr.sin_addr));
			}
	strcpy(ip,inet_ntoa(addr.sin_addr));
	return ip;
	//inet_ntop( AF_INET, &(datosDelCliente.sin_addr.s_addr), ip, INET_ADDRSTRLEN );
}

void agrandarVectorPlataforma(pndata* vector,int nuevoTamanio)//modifico
{
	//vector=NULL;
	niveles=realloc(niveles,nuevoTamanio*sizeof (pndata));
	nuevoTamanio=nuevoTamanio+AGRANDAMIENTO;
}

void leerArchivoConfiguracion()
{
  FILE* arch;
  arch = fopen(PathArchivoConfig,"r");
  if(arch==NULL)
  {
		funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"Error al querer abrir el archivo de configuracion",__LINE__);
		exit(0);
  }

  char* clave=NULL;
  char* valor=NULL;
  char str[100];
  clave=(char*)malloc(sizeof(char)*25);
  valor=(char*)malloc(sizeof(char)*25);

  while(!feof(arch))
  {
	  str[0]='\0';
	  fgets(str,100,arch);
	  clave=strtok(str,":");
	  valor = strtok(NULL,"\n");

	  sprintf(bufferLogeo,"Leyendo parametro del archivo de configuracion: %s",clave);
	  funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);
	  if(comparar(clave,"Quantum"))
	  {
		  quantum = atoi(valor);
	  }else if(comparar(clave,"Tiempo de retardo de quantum"))
	  {
		  tiempoRetardo=atof(valor);
		  //printf("Tiempo retardo %f\n",tiempoRetardo);
	  }
  }


  sprintf(bufferLogeo,"Archivo de Configuración Quantum: %d",quantum);
  funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

}

void lanzarHiloINOTIFY()
{
	pthread_t hilo;
	pthread_create(&hilo,NULL,hiloInotify,NULL);
}

void* hiloInotify(void* p)
{
	int length;

		  int fd;
		  int wd;
		  char buffer[BUF_LEN];

		  fd = inotify_init();

		  if ( fd < 0 ) {
		  //  perror( "inotify_init" );
			  funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"Error en inotify_init",__LINE__);
		  }

		  wd = inotify_add_watch( fd, bufferInotify,
		                         IN_MODIFY);
		  if (wd<=0){

			  funcionLogueo(ERROR,PATH_ARCHIVO_LOGUEO,"Error al poner el watch, verificar que el archivo exista",__LINE__);
		  }

		 while(1){
		  int i=0;
		  length = read( fd, buffer, BUF_LEN );

		  while ( i < length ) {
		    struct inotify_event *event = ( struct inotify_event * ) &buffer[ i ];

		     if ( comparar(event->name,"plataforma") )
		     {
		    		 sprintf( bufferLogeo,"INOTIFY - The file was modified. %s",event->name);
					  funcionLogueo(INFO,PATH_ARCHIVO_LOGUEO,bufferLogeo,__LINE__);

		    		 leerArchivoConfiguracion();
		     }

		    i += (EVENT_SIZE+(event->len)) ;
		  }

		 }
	return NULL;
}
pndata* crearNodoNivel(char* nombreNivel,char* ipnivel,int puertoNivel,int puertoPlanificador)
{
	pndata* nodo=(pndata*)malloc(sizeof(pndata));
	strcpy(nodo->ipNivel,ipnivel);
	strcpy(nodo->nombreNivel,nombreNivel);
	nodo->puertoNIvel=puertoNivel;
	nodo->puertoPlanificador=puertoPlanificador;
	nodo->next=NULL;
	return nodo;
}

void agregarNodoNivelEnLista(pndata** listaNiveles,pndata* nodo)
{
	if (*listaNiveles==NULL) //si la lista esta vacia lo agrego al principio
	{
		*listaNiveles=nodo;
	}else{ //sino lo agrego al final

		pndata *aux=*listaNiveles;
		while(aux->next!=NULL)//avanzo hasta estar parado en el ultimo nodo
		{
			aux=aux->next;
		}

		aux->next=nodo;
	}
}

pndata* buscarNivel(pndata* lista,char* nombreNivel)
{
	pndata* aux=lista;
		while(aux!=NULL && !comparar(aux->nombreNivel,nombreNivel))
		{
			aux=aux->next;
		}
		return aux;
}

int buscarLibreVectorPlanif()
{
	int i;

	for(i=0;i<MAXIMODENIVELES;i++)
	{
		if(vectorPlanif[i].nombreNivel[0]=='\0')
		{
			return i;
		}
	}
	return -1;
}

int buscarIndiceVectorPlanif(char* nombreNivel)
{
	int i;

	for(i=0;i<MAXIMODENIVELES;i++)
	{
		if(comparar(vectorPlanif[i].nombreNivel,nombreNivel))
		{
			return i;
		}
	}
	return -1;
}
