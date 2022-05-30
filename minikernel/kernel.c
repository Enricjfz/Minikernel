/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */
#include <string.h>




int acc_t_ejec = 0; //flag que indica si el proceso esta accediendo a la estructura tiempos_ejec
int num_int_total = 0; //entero que indica el numero de interrupciones totales en el sistema
int num_int_anterior = 0; //entero que indica el numero de interrupciones en la llamada de tiempos_proc
int num_int_usuario = 0; //entero que indica el numero de interrupciones en las que el proceso estaba en modo usuario
int num_int_sistema = 0; //entero que indica el numero de interrupciones en las que el proceso estaba en modo sistema
int num_actual_mutex = 0; //entero que indica el numero de mutex abiertos en el sistema
int puntero_lectura = 0; //entero que indica la posición de lectura del buffer del terminal
int puntero_escritura = 0; //entero que indica la posición de escritura del buffer del terminal

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	while (lista_listos.primero==NULL)
		espera_int();		/* No hay nada que hacer */
	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/** Metodo de desbloqueo de procesos **/

static void desbloqueo_proceso (lista_BCPs * lista){
  
  if(lista->primero != NULL){
    //hay un proceso bloqueado, se procede a desbloquear
	 BCP * proceso_bloqueado = lista->primero;
	 eliminar_elem(&lista, proceso_bloqueado);
	 proceso_bloqueado->estado = LISTO;
	 insertar_ultimo(&lista_listos,proceso_bloqueado);

  } 


} 

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);
	if(puntero_escritura < TAM_BUF_TERM){
		//si hay espacio se introduce el caracter en el buffer
       int nivel_anterior = fijar_nivel_int(NIVEL_3);
       buffer_terminal[puntero_escritura] = car;
	   puntero_escritura = (puntero_escritura + 1)% TAM_BUF_TERM; 
	   fijar_nivel_int(nivel_anterior);



	} 
	//se libera a un proceso bloqueado en el terminal



        return;
}

static void desbloqueo_reloj(){
    if(lista_dormidos.primero != NULL){
      BCP * aux;

	  aux = lista_dormidos.primero;
	  while(aux != NULL)
	 {
       aux->interrupciones--;
	   if(aux->interrupciones == 0)
	  {
		  //Hay un proceso que se puede desbloquear
		  eliminar_elem(&lista_dormidos, aux);
	      aux->estado = LISTO;
	      insertar_ultimo(&lista_listos,aux);
		  
	  } 
	  aux = aux->siguiente;
	 }  
  


	} 
	





} 

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){

	printk("-> TRATANDO INT. DE RELOJ\n");
	

    num_int_total++;	

	if(viene_de_modo_usuario()){
      num_int_usuario++;

	} 
	else{
     num_int_sistema++;

	} 

    //no hace falta cotrol de sincro ya que la interrupción de reloj es la de más alto nivel
    desbloqueo_reloj();

        return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw(){

	printk("-> TRATANDO INT. SW\n");

	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;
		p_proc->num_mutex_abiertos = 0;

		/* lo inserta al final de cola de listos */
		insertar_ultimo(&lista_listos, p_proc);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){

	printk("-> FIN PROCESO %d\n", p_proc_actual->id);

	liberar_proceso();

        return 0; /* no deber�a llegar aqui */
}

/*
* Devuelve el identificador del proceso actual
*
*/

int obtener_id_pr(){

   return p_proc_actual->id;
} 


/*
 * Función auxiliar de dormir que modifica la BCP del proceso, añade el proceso a la lista de BCPs dormidos, configura la interrupción
 * de reloj y llama al planificador
 *
 */

static void aux_dormir(unsigned int segundos, lista_BCPs * lista_dormidos){
   p_proc_actual->interrupciones = segundos * TICK;
   eliminar_elem(&lista_listos,p_proc_actual);
   insertar_ultimo(lista_dormidos, p_proc_actual);
   p_proc_actual->estado = BLOQUEADO;
   BCP * p_proc_anterior = p_proc_actual;
   // falta programar la interrupción
   p_proc_actual = planificador();
   cambio_contexto(&(p_proc_anterior->contexto_regs), &(p_proc_actual->contexto_regs));

}

/*
 * Duerme al proceso actual durante los segundos especificados en el argumento
 * de entrada. Se realiza un cambio de proceso a uno que este libre.
 * 
 * 
*/

int dormir(unsigned int segundos){
   aux_dormir(segundos, &lista_dormidos);
   return 1;

}

int tiempos_proceso(struct tiempos_ejec *t_ejec){
   if(t_ejec != NULL)
  {
	  acc_t_ejec = 1;
      //accedemos a memoria de proceso
	  t_ejec->sistema = num_int_sistema;
	  t_ejec->usuario = num_int_usuario; 

	  acc_t_ejec = 0;

  } 
  if(num_int_anterior == 0)
 {
	 //primera llamada
	 num_int_anterior = num_int_total;
 } 
 else{
	 //llamadas sucesivas 

	 num_int_anterior = num_int_total - num_int_anterior;
 } 

 return num_int_anterior/TICK;
	 

}


static void aux_bloqueo(lista_BCPs * lista_bloqueados_mutex){
   eliminar_elem(&lista_listos,p_proc_actual);
   insertar_ultimo(lista_bloqueados_mutex, p_proc_actual);
   p_proc_actual->estado = BLOQUEADO;
   BCP * p_proc_anterior = p_proc_actual;
   // falta programar la interrupción
   p_proc_actual = planificador();
   cambio_contexto(&(p_proc_anterior->contexto_regs), &(p_proc_actual->contexto_regs));

}


int abrir_mutex(char *nombre){

	for (int i = 0; i <= NUM_MUT; i++){
		if(strcmp(vectorMutex[i]->nombre,nombre)== 0){
			return i; //se encuentra el mutex en el vector de mutex
		} 
		
	} 
	return -1; //no se encuentra


} 

static int id_vector_mutex(struct Mutex_t *vector[],int length){
	int sol = 0;
	for (int i = 0; i < length; i++){
		if(vector[i] == NULL){
            //primer espacio libre del vector de mutex que esta libre
			sol = i;
			break;
		} 
	} 
	return sol;

} 

int crear_mutex(char *nombre, int tipo){
	if(tipo != NO_RECURSIVO && tipo != RECURSIVO){
		return -1; //no existe ese tipo de mutex
	} 
	int length = strlen(nombre);
	if(length > MAX_NOM_MUT){
		return -2; //el nombre del mutex es mayor que máximo
	}  

	while(num_actual_mutex >= NUM_MUT){
        //el proceso se bloquea y se produce un cambio de contexto
       aux_bloqueo(&lista_bloqueados_abrir_mutex);

		//return -3; //se ha excedido el numero máximo de mutex en el sistema
	} 

	if(abrir_mutex(nombre) != -1){
		return -4; //existe un mutex con ese nombre
	} 
    
	Mutex newMutex;
	newMutex.id_proc_actual = -1; // no hay proceso asociado al mutex
	newMutex.nombre = nombre;
	newMutex.tipo = tipo;
	newMutex.n_locks = 0;
	int mutexid = id_vector_mutex(&vectorMutex,NUM_MUT);
	//posible inhibicion de interrupciones
	vectorMutex[mutexid] = &newMutex; 
	num_actual_mutex++;
    //mutex creado y guardado
	return num_actual_mutex;

}




int lock(unsigned int mutexid){
	if(vectorMutex[mutexid]->id_proc_actual != -1 && vectorMutex[mutexid]->id_proc_actual != p_proc_actual){
		//se encuentra usado por un proceso distinto al actual
		vectorMutex[mutexid]->proc_bloqueados[p_proc_actual->id] = p_proc_actual; 
		//bloqueo del proceso y cambio de contexto 
		aux_bloqueo(&lista_bloqueados_mutex);
		return -1;
	} 
	else if(vectorMutex[mutexid]->id_proc_actual == p_proc_actual){
		//se hace lock a un mutex que tiene el proceso
        if(vectorMutex[mutexid]->tipo == RECURSIVO){
			vectorMutex[mutexid]->n_locks++; //mutex recursivo
			
		} 
		else{
			return -2; //no se puede hacer mas de un lock si este no es recursivo
		} 
	} 
	else{
       //el mutex esta libre
	   vectorMutex[mutexid]->id_proc_actual = p_proc_actual->id;
	   int pos = id_vector_mutex(p_proc_actual->vectorMutexAbiertos,p_proc_actual->num_mutex_abiertos);
	   p_proc_actual->vectorMutexAbiertos[pos] = vectorMutex[mutexid]; 
	   p_proc_actual->num_mutex_abiertos++; 
	   
	} 

   return 1;
   
} 

//método auxiliar que desbloquea un proceso asociado a un mutex y lo inserta en la lista de procesos listos

static void desbloqueoProceso(lista_BCPs lista_bloqueados, Mutex  * mutex){
   
   for (int i = 0; i < MAX_PROC; i++){
       if(mutex->proc_bloqueados[i] != NULL){
		   BCP * proc_bloqueado;
		   proc_bloqueado = mutex->proc_bloqueados[i]; 
		   proc_bloqueado->estado = LISTO;
		   eliminar_elem(&lista_bloqueados,proc_bloqueado);
		   insertar_ultimo(&lista_listos,proc_bloqueado);
		   break;
	   } 

   } 


} 

//metood auxilar que devuelve la posición del mutex en la lista de mutex que tiene un proceso

static int get_id_proc(int pos_vector_mutex){
  
  if(vectorMutex[pos_vector_mutex]->id_proc_actual != p_proc_actual->id){
     //el proceso no tiene ese mutex
	 return -1;
  } 


  for (int i = 0; i <= NUM_MUT_PROC; i++){
		if(strcmp(p_proc_actual->vectorMutexAbiertos[i]->nombre ,vectorMutex[pos_vector_mutex]->nombre)== 0){
			return i; //se encuentra el mutex en el vector de mutex
		} 
		
	} 
	return -1; //no se encuentra




} 


int unlock(unsigned int mutexid){
	int pos = get_id_proc(mutexid);
	if(pos == -1){
		return -1; //el proceso no tiene ese mutex abierto
	} 
	if(p_proc_actual->vectorMutexAbiertos[pos]->tipo == RECURSIVO){
		p_proc_actual->vectorMutexAbiertos[pos]->n_locks--;
		if(p_proc_actual->vectorMutexAbiertos[pos]->n_locks == 0){
			//se libera mutex y desbloquea proceso
			p_proc_actual->vectorMutexAbiertos[pos] = NULL;
			p_proc_actual->num_mutex_abiertos--;
			vectorMutex[mutexid]->id_proc_actual = -1; //no tiene asociado un proceso
			//se desbloquea el primer proceso bloqueado
			desbloqueoProceso(lista_bloqueados_mutex,vectorMutex[mutexid]);
			return 1;
		} 

	} 
	else{
		    // mutex no recursivo, se libera
			p_proc_actual->vectorMutexAbiertos[pos] = NULL;
			p_proc_actual->num_mutex_abiertos--;
			vectorMutex[mutexid]->id_proc_actual = -1; //no tiene asociado un proceso
			//se desbloquea el primer proceso bloqueado
			desbloqueoProceso(lista_bloqueados_mutex,vectorMutex[mutexid]);
			return 1;

	} 


}

static void desbloqueo_cerrar_mutex(lista_BCPs lista_bloqueados, Mutex * mutex){
    //función que desbloquea a todos los procesos bloqueados por un mutex

	 for (int i = 0; i < MAX_PROC; i++){
       if(mutex->proc_bloqueados[i] != NULL){
		   BCP * proc_bloqueado;
		   proc_bloqueado = mutex->proc_bloqueados[i]; 
		   proc_bloqueado->estado = LISTO;
		   eliminar_elem(&lista_bloqueados,proc_bloqueado);
		   insertar_ultimo(&lista_listos,proc_bloqueado);
	   } 

   } 


} 

static void desbloqueo_lista_abrir_mutex(){
   
   // se desbloquea si hay mutex bloqueado
   if(lista_bloqueados_abrir_mutex.primero != NULL){ 
   // desbloqueo  crear mutex
   BCP * primero;
   primero = lista_bloqueados_abrir_mutex.primero;
   eliminar_elem(&lista_bloqueados_abrir_mutex,primero);
   primero->estado = LISTO;
   insertar_ultimo(&lista_listos,primero);
   } 
} 




int cerrar_mutex(unsigned int mutexid){
	int pos = get_id_proc(mutexid);
	if(pos == -1){
		return -1; //el proceso no tiene ese mutex abierto
	}
	//se desbloquean todos los procesos asociados a ese mutex y se borra de la lista de mutex del sistema
	p_proc_actual->vectorMutexAbiertos[mutexid] = NULL;
	p_proc_actual->num_mutex_abiertos--; 
	desbloqueo_cerrar_mutex(lista_bloqueados_mutex,vectorMutex[mutexid]);
	vectorMutex[mutexid] = NULL; 
	desbloqueo_lista_abrir_mutex();
	
} 

int leer_caracter(){
    while(puntero_escritura == puntero_lectura){
		//no hay caracteres en el buffer nuevos, se bloquea al proceso y se produce un cambio de contexto
        aux_bloqueo(&lista_bloqueados_lectura_term);
	} 
	//se lee caracter
	int c;
	c = buffer_terminal[puntero_lectura];
	puntero_lectura = (puntero_lectura + 1)%TAM_BUF_TERM;
	desbloqueo_proceso(&lista_bloqueados_lectura_term);
	return c; 
} 






/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
