/*
 * select.h
 *
 *  Created on: 12/12/2013
 *      Author: utnso
 */

#ifndef SELECT_H_
#define SELECT_H_

#define PUERTOBASE_PLANIFICADOR 16000 //usamos el 15000. Hay que tener cuidado de no usar un puerto usado por otro proceso (ojo que el sistema operativo usa muchos).

#define CARACTER_PARSEADO ","
#define PATH_ARCHIVO_LOGUEO "./logPlataforma"
#define INFO 1
#define WARN 2
#define ERROR 3
#define DEBUG 4

#define TAM_BUFFER 200

typedef struct pndata{
	char nombreNivel[16];
	int puertoPlanificador;
	char ipNivel[16];
	int puertoNIvel;
	struct pndata* next;
} pndata;

typedef struct nodo{
	void *data;
	struct nodo *next;
} nodo;

typedef struct vecllegada{
	nodo* lista;
	char nombreNivel[16];
} tipoVectorLlegada;

//inicio lista sublista tipos

typedef struct nodoPersonajeLS{
	char personaje;
	int socketPersonaje;
	struct nodoPersonajeLS* next;
} nodoPersonajeLS;

typedef struct nodoRecursoLS{
	char recurso;
	int cantidad;
	nodoPersonajeLS* colaPersonajes;
	struct nodoRecursoLS* next;
} nodoRecursoLS;
//fin lista sublista tipos

typedef struct nodoPersonaje{
	int socketPersonaje;
	struct nodoPersonaje *next;
} nodoPersonaje;

typedef struct recurso{
	int cantidad;
	nodoPersonaje* nodo;//lista de personajes esperando por el recurso
} recurso;

typedef struct vectorPlanificador{
	char nombreNivel[16];
	int running;
	int quantum;
	nodoRecursoLS* listaRecursos;
	nodoPersonaje* listaReady;
} vectorPlanificador;

typedef struct recursos{
	int hongo;
	int moneda;
	int flor;
} recursos;

typedef struct hiloplanificador{
	int puerto;
	int conexionNivel;
	int indiceVectorPlanif;
	char nombreNivel[16];
} hplan;

int sockets_create_Server(int port);
void actualizarDescriptorMaximo(int socketEscucha, int *vectorclientesconectados, int tamaniovectorclientesconectados, int *descr_max);
void agregarNuevaConexionEnVectorClientesConectados(int socketNuevaConexion,int* vectorclientesconectados,int *tamaniovector);
void inicializarVectorEn0(int* vectorclientesconectados,int tamaniovector);
void *threadPlanificador(void *parametro);
void agrandarVectorSelect(int* vector,int nuevoTamanio);
recurso obtenerRecurso(char recu,int nroNivel);
void insertarPrimeroEnLista(nodoPersonaje** lista, int dato);
int sacarDeListaDeReady(nodoPersonaje** lista,int unSocket);
void liberarRecurso(int nroNivel,char recurso,char* nombreNivel);
void desbloquearPersonaje(nodoPersonaje** lista,int nroNivel);
void *funcionLogueo(int nivelDeDetalle,char *archivo,char *string_A_Logear, int lineNumber);
int buscarIndiceVectorPlanif(char* nombreNivel);


#endif /* SELECT_H_ */
