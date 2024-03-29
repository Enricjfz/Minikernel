/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

#define NO_RECURSIVO 0
#define RECURSIVO 1

/** Definición de la estructura Mutex, que almacena la información asociada a un Mutex **/

typedef struct Mutex_t {
   char nombre [MAX_NOM_MUT];  //nombre del struct
   int tipo;   //tipo del struct 0/1
   int id_proc_actual;  //id del proceso que tiene el struct
   struct BCP_t * proc_bloqueados[MAX_PROC]; //procesos solicitantes del struct
   int n_locks; // solo si es recursivo, numero de veces que se ha hecho lock
   int n_veces_abierto; //indica cuantas veces ha sido abierto este mutex

} Mutex;

/** Definición del vector de structs, que almacena el numero de structs que hay en el sistema **/

struct Mutex_t * vectorMutex[NUM_MUT];


/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */

typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
        int id;				/* ident. del proceso */
        int estado;			/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
        contexto_t contexto_regs;	/* copia de regs. de UCP */
        void * pila;			/* dir. inicial de la pila */
	BCPptr siguiente;		/* puntero a otro BCP */
	void *info_mem;			/* descriptor del mapa de memoria */
	int interrupciones;    /* entero que indica cuantas interrupciones quedan hasta que se active el evento */
	struct Mutex_t * vectorMutexAbiertos[NUM_MUT_PROC]; /*vector de mutex abiertos por el proceso */
	int num_mutex_abiertos; /*manejo vertor mutex*/
    int ticks_round_robin;  /* rodaja para cada procesador */

} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;


/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};

/*
 * Variable global que representa la cola de procesos dormidos 
 */

lista_BCPs lista_dormidos = {NULL, NULL}; 

/** lista de procesos bloqueados a la espera de un mutex **/

lista_BCPs lista_bloqueados_mutex = {NULL, NULL};

/** lista bloqueados abrir_mutex **/

lista_BCPs lista_bloqueados_abrir_mutex ={NULL, NULL}; 

/** lista bloqueados lectura_terminal **/
lista_BCPs lista_bloqueados_lectura_term ={NULL,NULL}; 



/*
*
* Definición de la estructura tiempos_ejec que se utiliza en la funcionalidad
* de tiempos_proceso
*
*/

typedef struct tiempos_ejec {
    int usuario; //numero de interrupciones que ha recibido el proceso en modo usuario
    int sistema; //numero de interrupciones que ha recibido el proceso en modo sistema
};



/*
 *
 * Definicion del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;

/** Buffer de caracteres del terminal **/

char buffer_terminal[TAM_BUF_TERM]; 


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
int sis_obtener_id_pr();
int sis_dormir();
int sis_tiempos_proceso();
int sis_crear_mutex();
int sis_abrir_mutex();
int sis_lock();
int sis_unlock();
int sis_cerrar_mutex();
int sis_leer_caracter();

/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
				    {sis_obtener_id_pr},
                    {sis_dormir},
                    {sis_tiempos_proceso},
                    {sis_crear_mutex},
                    {sis_abrir_mutex},
                    {sis_lock},
                    {sis_unlock},
                    {sis_cerrar_mutex},
                    {sis_leer_caracter}};

#endif /* _KERNEL_H */

