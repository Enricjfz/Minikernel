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
#include <stdlib.h>




int acc_t_ejec = 0; //flag que indica si el proceso esta accediendo a la estructura tiempos_ejec
int num_int_total = 0; //entero que indica el numero de interrupciones totales en el sistema
int num_int_usuario = 0; //entero que indica el numero de interrupciones en las que el proceso estaba en modo usuario
int num_int_sistema = 0; //entero que indica el numero de interrupciones en las que el proceso estaba en modo sistema
int num_actual_mutex = 0; //entero que indica el numero de mutex abiertos en el sistema
int puntero_lectura = 0; //entero que indica la posición de lectura del buffer del terminal
int puntero_escritura = 0; //entero que indica la posición de escritura del buffer del terminal
int datos_buffer = 0; //entero que indica cuantos buffer hay en el buffer

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
	while (lista_listos.primero==NULL){ 
		espera_int();		/* No hay nada que hacer */

	}
	lista_listos.primero->ticks_round_robin = TICKS_POR_RODAJA;

	return lista_listos.primero;
}

//metodo auxiliar que desbloquea un proceso que esta bloqueado esperando abrir un mutex
static void desbloqueo_lista_abrir_mutex(){
   
   // se desbloquea si hay mutex bloqueado
   if(lista_bloqueados_abrir_mutex.primero != NULL){ 
   // desbloqueo  crear mutex
   BCP * primero;
   primero = lista_bloqueados_abrir_mutex.primero;
   int anterior = fijar_nivel_int(NIVEL_3);
   eliminar_elem(&lista_bloqueados_abrir_mutex,primero);
   primero->estado = LISTO;
   insertar_ultimo(&lista_listos,primero);
   fijar_nivel_int(anterior);
   } 
} 

//Funcion auxiliar que devuelve la posicion del mutex dado su nombre en la lista de mutex creados o negativos si no existe
static int buscar_indice(char *nombre){
    int i;
	for (i = 0; i < NUM_MUT; i++){
		if(vectorMutex[i] != NULL && strcmp(vectorMutex[i]->nombre,nombre)== 0){
			printk("->Se encuentra mutex en la pos %d\n",i);
			return i; //se encuentra el mutex en el vector de mutex
		} 
		
	} 
	printk("->No se encuentra mutex\n");
	return -1; //no se encuentra
} 

/** función que desbloquea a todos los procesos bloqueados por un mutex  **/

static void desbloqueo_cerrar_mutex(lista_BCPs * lista_bloqueados,int indice){
    
    printk("SE DESBLOQUEAN LOS PROCESOS DEL MUTEX: %s\n",vectorMutex[indice]->nombre);
	 for (int i = 0; i < MAX_PROC; i++){
	   //printk("Iter: %d\n",i);
       if(vectorMutex[indice]->proc_bloqueados[i] != NULL){
		   BCP * proc_bloqueado;
		   proc_bloqueado = vectorMutex[indice]->proc_bloqueados[i];
		   printk("SE DESBLOQUEA PROC %d\n",proc_bloqueado->id); 
		   proc_bloqueado->estado = LISTO;
		   int anterior = fijar_nivel_int(NIVEL_3);
		   eliminar_elem(lista_bloqueados,proc_bloqueado);
		   insertar_ultimo(&lista_listos,proc_bloqueado);
		   vectorMutex[indice]->proc_bloqueados[i] = NULL;
		   fijar_nivel_int(anterior);
		  
	   } 

   } 


} 


/** Funcion auxilar que elimina todos los mutex y por consiguiente a los procesos bloqueados, asociados al proceso a liberar **/
static void liberar_mutex(){

   for(int i = 0; i < NUM_MUT_PROC; i++){
      if(p_proc_actual->vectorMutexAbiertos[i] != NULL){
		if(p_proc_actual->vectorMutexAbiertos[i]->id_proc_actual == p_proc_actual->id){
			int indice = buscar_indice(p_proc_actual->vectorMutexAbiertos[i]->nombre);
			//printk("LLEGO LIBERAR 1\n");
			p_proc_actual->vectorMutexAbiertos[i]->id_proc_actual = -1;
		    desbloqueo_cerrar_mutex(&lista_bloqueados_mutex,indice);
			//printk("LLEGO LIBERAR 2\n");
			p_proc_actual->vectorMutexAbiertos[i]->n_veces_abierto--;
		   if(p_proc_actual->vectorMutexAbiertos[i]->n_veces_abierto <= 0){
			    printk("->SE VA A ELIMINAR EL MUTEX %s\n",p_proc_actual->vectorMutexAbiertos[i]->nombre);
				//int indice = buscar_indice(p_proc_actual->vectorMutexAbiertos[i]->nombre);
				num_actual_mutex--;
				vectorMutex[indice] = NULL; 
				desbloqueo_lista_abrir_mutex();
				printk("->SE ELIMINA UN MUTEX QUE NO ESTA ABIERTO POR NINGUN PROCESO\n");
		   }	
		   
		} 
		else{  
           p_proc_actual->vectorMutexAbiertos[i]->n_veces_abierto--;
		   if(p_proc_actual->vectorMutexAbiertos[i]->n_veces_abierto <= 0){
			printk("->SE VA A ELIMINAR EL MUTEX %s\n",p_proc_actual->vectorMutexAbiertos[i]->nombre);
			int indice = buscar_indice(p_proc_actual->vectorMutexAbiertos[i]->nombre);
	        num_actual_mutex--;
	        vectorMutex[indice] = NULL; 
			desbloqueo_lista_abrir_mutex();
			printk("->SE ELIMINA UN MUTEX QUE NO ESTA ABIERTO POR NINGUN PROCESO\n");
		   } 
		}	
	  } 


   }  


} 

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;
	//printk("LLEGA\n");
	liberar_mutex();
    //printk("SALE\n");
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

	if (!viene_de_modo_usuario()){ 
	    if(acc_t_ejec == 1){
           liberar_proceso(); //se aborta proceso
		} 
		else{ 
		panico("excepcion de memoria cuando estaba dentro del kernel");

		}	
	} 
	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/** Metodo de desbloqueo de procesos **/

static void desbloqueo_proceso (lista_BCPs * lista){
  
  if(lista->primero != NULL){
    //hay un proceso bloqueado, se procede a desbloquear
	int anterior = fijar_nivel_int(NIVEL_3);
	 BCP * proceso_bloqueado = lista->primero;
	 eliminar_elem(lista, proceso_bloqueado);
	 proceso_bloqueado->estado = LISTO;
	 insertar_ultimo(&lista_listos,proceso_bloqueado);
	 fijar_nivel_int(anterior);

  } 


} 

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	int nivel_anterior = fijar_nivel_int(NIVEL_2);
	char car;
	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);
	if(datos_buffer < TAM_BUF_TERM){
		//si hay espacio se introduce el caracter en el buffer
       buffer_terminal[puntero_escritura] = car;
	   datos_buffer++;
	   puntero_escritura = (puntero_escritura + 1)% TAM_BUF_TERM; 


	} 
	//se libera a un proceso bloqueado en el terminal
    desbloqueo_proceso(&lista_bloqueados_lectura_term);
    fijar_nivel_int(nivel_anterior);

        return;
}

//metodo auxiliar de interrupcion de reloj que reduce en uno
//los contadores de reloj de todos los procesos y si alcanzan un valor igual a 0 se debloquea
//el proceso asociado
static void desbloqueo_reloj(){
    if(lista_dormidos.primero != NULL){
      BCP * aux;

	  aux = lista_dormidos.primero;
	  while(aux != NULL)
	 {
       aux->interrupciones--;
	   //printk("PROCESO %d INTERRUPCIONES RESTANTES: %d\n",aux->id,aux->interrupciones);
	   if(aux->interrupciones <= 0)
	  {
          BCP * aux_sig;
		  aux_sig = aux->siguiente;
		  //Hay un proceso que se puede desbloquear
		  eliminar_elem(&lista_dormidos, aux);
	      aux->estado = LISTO;
	      insertar_ultimo(&lista_listos,aux);
		  aux = aux_sig;
		  
	  } 
	  else{ 
	  aux = aux->siguiente;
	  } 
	 }  
  


	} 
	





} 

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){

	printk("-> TRATANDO INT. DE RELOJ\n");
	//printk("INTERRUPCIONES RESTANTES: %d\n",p_proc_actual->interrupciones);

    num_int_total++;	

	if(viene_de_modo_usuario() && lista_listos.primero != NULL){
      num_int_usuario++;

	} 
	if(!viene_de_modo_usuario() && lista_listos.primero != NULL){ 
     num_int_sistema++;

	} 

    //no hace falta control de sincro ya que la interrupción de reloj es la de más alto nivel
    desbloqueo_reloj();
    if(lista_listos.primero != NULL)
   {
	  p_proc_actual->ticks_round_robin--; //se reduce la rodaja del proceso (siempre que no sea el proceso nulo)
	  //printk("ROUND ROBIN: %d\n",p_proc_actual->ticks_round_robin);
	  if(p_proc_actual->ticks_round_robin == 0){
		//printk("ROUND ROBIN LLEGA A CERO\n");
       //se le ha acabado el tiempo al proceso, se programa una interrupcion software
	   activar_int_SW();
	} 
   } 	

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
 * Tratamiento de interrupciones software
 */
static void int_sw(){

	printk("-> TRATANDO INT. SW\n");
	//se produce un cambio de contexto, se inserta al proceso anterior en la cola de procesos listos y se asigna una nueva rodaja
	if(p_proc_actual->ticks_round_robin == 0){
      //se comprueba que de verdad el proceso se le ha acabado la rodaja
	  int anterior = fijar_nivel_int(NIVEL_3);
	  eliminar_elem(&lista_listos,p_proc_actual);
      insertar_ultimo(&lista_listos, p_proc_actual);
	  fijar_nivel_int(anterior);
      BCP * p_proc_anterior = p_proc_actual;
      p_proc_actual = planificador();
      cambio_contexto(&(p_proc_anterior->contexto_regs), &(p_proc_actual->contexto_regs));



	} 

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
        p_proc->ticks_round_robin = TICKS_POR_RODAJA;
		/* lo inserta al final de cola de listos */
		int anterior = fijar_nivel_int(NIVEL_3);
		insertar_ultimo(&lista_listos, p_proc);
		fijar_nivel_int(anterior);
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

//resto de llamadas al sistema al final del fichero (antes del main)


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
static void aux_dormir(lista_BCPs * lista_dormidos){
   int anterior = fijar_nivel_int(NIVEL_3);
   eliminar_elem(&lista_listos,p_proc_actual);
   p_proc_actual->estado = BLOQUEADO;
   insertar_ultimo(lista_dormidos, p_proc_actual);
   fijar_nivel_int(anterior);
   BCP * p_proc_anterior = p_proc_actual;
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
   if(segundos == 0){
     printk("SLEEP 0 SEGUNDOS\n");
   } else{
     p_proc_actual->interrupciones = segundos * TICK;
	 printk ("SEGUNDOS: %d INTERRUPCIONES INICIALES: %d\n",segundos,p_proc_actual->interrupciones);
     aux_dormir(&lista_dormidos);
    
   }
   return 1;
}

//Funcion del kernel que devuelve el numero de interrupciones ocurridas desde el arranque, si el argumento 
//de entrada es distinto de null, devuelve el numero de interrupciones de sistema y de usuario
int tiempos_proceso(struct tiempos_ejec *t_ejec){
	//printk("LLEGA a tiempos proceso\n");
   if(t_ejec != NULL)
  {
	  acc_t_ejec = 1;
      //accedemos a memoria de proceso
	  t_ejec->sistema = num_int_sistema;
	  t_ejec->usuario = num_int_usuario; 

	  acc_t_ejec = 0;

  } 
 return num_int_total;
	 

}

//funcion auxiliar que bloquea al proceso en ejecucion en la lista de mutex bloqueados
static void aux_bloqueo(lista_BCPs * lista_bloqueados_mutex){
   int anterior = fijar_nivel_int(NIVEL_3);
   eliminar_elem(&lista_listos,p_proc_actual);
   insertar_ultimo(lista_bloqueados_mutex, p_proc_actual);
   p_proc_actual->estado = BLOQUEADO;
   fijar_nivel_int(anterior);
   BCP * p_proc_anterior = p_proc_actual;
   // falta programar la interrupción
   p_proc_actual = planificador();
   cambio_contexto(&(p_proc_anterior->contexto_regs), &(p_proc_actual->contexto_regs));

}

//Funcion auxiliar que devuelve la primera posicion libre de una lista dada su longitud
static int id_vector_mutex(struct Mutex_t *vector[],int length){
	int sol = -1;
	for (int i = 0; i < length; i++){
		if(vector[i] == NULL){
            //primer espacio libre del vector de mutex que esta libre
			sol = i;
			break;
		} 
	} 
	return sol; 

} 

//Funcion que abre el mutex especificado por su nombre y asigna ese mutex a la lista de mutex abierto por el proceso, devuelve negativo si
//se ha sobrepasado el numero de mutex abierto por un proceso o no existe ese mutex
int abrir_mutex(char *nombre){
    printk("Nombre: %s\n", nombre);
	if(p_proc_actual->num_mutex_abiertos >= NUM_MUT_PROC){
		printk("-> SE TIENEN ABIERTOS EL NUMERO MAXIMO DE DESCRIPTORES\n");
		return -2;
	} 
	//Se busca al mutex
	int i;
	for (i = 0; i < NUM_MUT; i++){
		if(vectorMutex[i] != NULL && strcmp(vectorMutex[i]->nombre,nombre)== 0){
			printk("Se encuentra mutex\n");
			printk("->Descriptor %d\n",i);
			int pos = id_vector_mutex(p_proc_actual->vectorMutexAbiertos,NUM_MUT_PROC);
			if(pos == -1){
				//no deberia de llegar aqui
				printk("-> NO HAY POSICION LIBRE");
				break;
			}   	
			p_proc_actual->vectorMutexAbiertos[pos] = vectorMutex[i]; 
	        p_proc_actual->num_mutex_abiertos++; 
			vectorMutex[i]->n_veces_abierto++; 
			//Se ha guardado el mutex
			return i; //se encuentra el mutex en el vector de mutex
		} 
		
	} 
	printk("->NO SE ENCUENTRA EL MUTEX ESPEFICICADO\n");
	return -1; //no se encuentra


} 


//funcion auxiliar que inicializa a null el vector de punteros de procesos del mutex
static void inicializar_vector(Mutex * mutex){
int i;
for (i = 0; i < MAX_PROC; i++){
  mutex->proc_bloqueados[i] = NULL; 
  
} 


} 

 

//funcion que crea un mutex dado los argumentos y se comprueba si se han pasado bien los argumentos o se ha excedido el numero
//maximo de mutex. Tambien se asocia al proceso.
int crear_mutex(char *nombre, int tipo){
	//printk("Nombre: %s\n", nombre);
	//printk("Llega 1 \n");
	if(tipo != NO_RECURSIVO && tipo != RECURSIVO){
		printk("->SE HA INTRODUCIDO UN TIPO QUE NO EXISTE\n");
		return -1; //no existe ese tipo de mutex
	} 
	int length = strlen(nombre);
	//printk("Llega 2 \n");
	if(length > MAX_NOM_MUT){
	    printk("->SE HA SUPERADO EL TAMANO MAXIMO DE DESCRIPTORES\n");
		return -2; //el nombre del mutex es mayor que máximo
	}  
	//printk("Llega 3 \n");
	if(buscar_indice(nombre) != -1){
		return -4; //existe un mutex con ese nombre
	} 
    //printk("Llega 4 \n");
	while(num_actual_mutex >= NUM_MUT){
        //el proceso se bloquea y se produce un cambio de contexto
	   printk("->SE HA SUPERADO EL NUMERO MAXIMO DE DESCRIPTORES\n");
	   printk("->PROCESO %d SE BLOQUEA\n",p_proc_actual->id);
       aux_bloqueo(&lista_bloqueados_abrir_mutex);
	   if(buscar_indice(nombre) != -1){
		//se vuelve a comprobar que no se haya creado un mutex con ese nombre--> competicion por mutex entre procesos
		return -4; //existe un mutex con ese nombre
	   } 

		
	} 

    //printk("Llega 5 \n");
	Mutex * newMutex;
	newMutex = malloc(sizeof(Mutex));
	newMutex->id_proc_actual = -1; // no hay proceso asociado al mutex
	newMutex->n_veces_abierto = 1; //se asocia directamente al proceso
	strcpy(newMutex->nombre,nombre);
	newMutex->tipo = tipo;
	newMutex->n_locks = 0;
	inicializar_vector(newMutex);
	int mutexid = id_vector_mutex(vectorMutex,NUM_MUT);
	vectorMutex[mutexid] = newMutex; 
	num_actual_mutex++;
	//printk("Llega 6 \n");
	int pos = id_vector_mutex(p_proc_actual->vectorMutexAbiertos,NUM_MUT_PROC);
	if(pos == -1){
        printk("->NO SE PUEDE ASOCIAR EL MUTEX CON EL PROCESO\n"); //creado pero no asociado 
		vectorMutex[mutexid]->n_veces_abierto = 0;
		return mutexid;
	} 
	p_proc_actual->vectorMutexAbiertos[pos] = newMutex;
	p_proc_actual->num_mutex_abiertos++; 
	//printk("Llega 7 \n");
    //mutex creado y guardado
	return mutexid;

}

//metood auxilar que devuelve la posición del mutex en la lista de mutex que tiene un proceso, negativo si no lo encuentra
static int get_pos_proc(int pos_vector_mutex){
  int sol = -1;
  for (int i = 0; i <= NUM_MUT_PROC; i++){
		if(p_proc_actual->vectorMutexAbiertos[i] != NULL && strcmp(p_proc_actual->vectorMutexAbiertos[i]->nombre ,vectorMutex[pos_vector_mutex]->nombre)== 0){
			sol = i; //se encuentra el mutex en el vector de mutex
			break;
		} 
		
	} 
	return sol; //no se encuentra

}



//metodo del kernel que usa el mutex por parte del proceso, devuelve negativo si el mutex esta en posesion de otro proceso o si es no recursivo
//y se hace mas de un lock en el.
int lock(unsigned int mutexid){
    //comprobar que existe el mutex
	if(vectorMutex[mutexid] == NULL){
		printk("->LOCK A UN PROCESO QUE NO EXISTE\n");
		return -3;
	} 
	//comprobar que el proceso tiene el mutex
	int pos = get_pos_proc(mutexid);
	if(pos < 0){
		printk("->LOCK A UN PROCESO QUE NO ESTA ABIERTO POR EL USUARIO\n");
		return -4; //el proceso no tiene ese mutex abierto
	}
	while(vectorMutex[mutexid]->id_proc_actual != -1 && vectorMutex[mutexid]->id_proc_actual != p_proc_actual->id){
		//se encuentra usado por un proceso distinto al actual, se queda bloqueado en un loop
		printk("PROCESO %d SE BLOQUEA EN EL MUTEX %s AL HACER UN LOCK A UN MUTEX NO LIBRE\n",p_proc_actual->id,vectorMutex[mutexid]->nombre);
		vectorMutex[mutexid]->proc_bloqueados[p_proc_actual->id] = p_proc_actual; 
		//bloqueo del proceso y cambio de contexto 
		aux_bloqueo(&lista_bloqueados_mutex);
		printk("PROCESO %d SE DESBLOQUEA EN EL MUTEX %s AL HACER UN LOCK A UN MUTEX NO LIBRE\n",p_proc_actual->id,vectorMutex[mutexid]->nombre);
		
	} 
	if(vectorMutex[mutexid]->id_proc_actual == p_proc_actual->id){
		//se hace lock a un mutex que tiene el proceso
        if(vectorMutex[mutexid]->tipo == RECURSIVO){
			printk("SE HACE LOCK A UN MUTEX RECURSIVO\n");
			vectorMutex[mutexid]->n_locks++; //mutex recursivo
			
		} 
		else{
			printk("SE HACE LOCK A UN MUTEX NO RECURSIVO\n");
			return -2; //no se puede hacer mas de un lock si este no es recursivo
		} 
	} 
     if(vectorMutex[mutexid]->id_proc_actual == -1){
       //el mutex esta libre
	   printk("PROCESO %d ADQUIERE MUTEX\n ",p_proc_actual->id);
	   vectorMutex[mutexid]->id_proc_actual = p_proc_actual->id;
	   vectorMutex[mutexid]->n_locks++;
	   
	} 
   printk("->LOCK HECHO\n");
   return 1;
   
} 

//método auxiliar que desbloquea un proceso asociado a un mutex y lo inserta en la lista de procesos listos
static void desbloqueoProceso(lista_BCPs * lista_bloqueados, int pos){
   
   for (int i = 0; i < MAX_PROC; i++){
       if(vectorMutex[pos]->proc_bloqueados[i] != NULL){
		   BCP * proc_bloqueado;
		   proc_bloqueado = vectorMutex[pos]->proc_bloqueados[i]; 
		   proc_bloqueado->estado = LISTO;
		   int anterior = fijar_nivel_int(NIVEL_3);
		   eliminar_elem(lista_bloqueados,proc_bloqueado);
		   insertar_ultimo(&lista_listos,proc_bloqueado);
		   vectorMutex[pos]->proc_bloqueados[i] = NULL;
		   fijar_nivel_int(anterior);
		   printk("SE DESBLOQUEA EL PROCESO %d\n",proc_bloqueado->id);
		   if(vectorMutex[pos]->proc_bloqueados[i] == NULL){
			   printk("Deberia de salir esto al desbloquear\n");
		   } 
		   break;
	   } 

   } 


} 

//metodo auxilar que devuelve la posición del mutex en la lista de mutex que tiene un proceso, negativo si no lo encuentra o no esta usando el mutex
static int get_id_proc(int pos_vector_mutex){
  
  if(vectorMutex[pos_vector_mutex]->id_proc_actual != p_proc_actual->id){
     //el proceso no esta usando ese mutex
	 printk("->SE ESTA HACIENDO UNLOCK A UN MUTEX QUE NO ESTA USANDO EL PROCESO\n");
	 return -2;
  } 


  for (int i = 0; i <= NUM_MUT_PROC; i++){
		if(p_proc_actual->vectorMutexAbiertos[i] != NULL && strcmp(p_proc_actual->vectorMutexAbiertos[i]->nombre ,vectorMutex[pos_vector_mutex]->nombre)== 0){
			return i; //se encuentra el mutex en el vector de mutex
		} 
		
	} 
	return -1; //no se encuentra

} 

//metodo del kernel que desbloquea a un mutex de la posesion de un proceso o reduce el numero de locks en caso de mutex recursivo,
//devuelve negativo en caso de no posesion o no existencia del mutex
int unlock(unsigned int mutexid){
	//comprobar que existe el mutex
	if(vectorMutex[mutexid] == NULL){
		printk("->UNLOCK A UN PROCESO QUE NO EXISTE\n");
		return -3;
	} 
	int pos = get_id_proc(mutexid);
	if(pos < 0){
		printk("->SE ESTA HACIENDO UNLOCK A UN MUTEX QUE NO ESTA ABIERTO AL PROCESO\n");
		return -1; //el proceso no tiene ese mutex abierto o no lo esta usando
	}
	if(p_proc_actual->vectorMutexAbiertos[pos]->tipo == RECURSIVO){
		p_proc_actual->vectorMutexAbiertos[pos]->n_locks--;
		printk("->UNLOCK RECURSIVO\n");
		if(p_proc_actual->vectorMutexAbiertos[pos]->n_locks == 0){
			vectorMutex[mutexid]->id_proc_actual = -1; //no tiene asociado un proceso
			//se desbloquea el primer proceso bloqueado
			desbloqueoProceso(&lista_bloqueados_mutex,mutexid);
			printk("->UNLOCK RECURSIVO Y SE QUITA POSESION\n");
			
		} 
	  }	
	else{
		    printk("->UNLOCK NO RECURSIVO Y SE QUITA POSESION\n");
		    // mutex no recursivo, se libera
			vectorMutex[mutexid]->id_proc_actual = -1; //no tiene asociado un proceso
			//se desbloquea el primer proceso bloqueado
			desbloqueoProceso(&lista_bloqueados_mutex,mutexid);

	} 
    return 1;

}


//Metodo del Kernel que cierra un mutex asociado a un proceso y si no hay mas procesos que lo hayan abierto
//se elimina ese mutex
int cerrar_mutex(unsigned int mutexid){
	int pos = get_pos_proc(mutexid);
	if(pos < 0){
		return -1; //el proceso no tiene ese mutex abierto
	}
	//se desbloquean todos los procesos asociados a ese mutex y se borra de la lista de mutex del sistema
	if(p_proc_actual->id == vectorMutex[mutexid]->id_proc_actual){
		//el proceso esta usando el mutex
		vectorMutex[mutexid]->id_proc_actual = -1; //se deja de asociar el mutex con el proceso
		vectorMutex[mutexid]->n_locks = 0; //se eliminan el numero de locks independientemente del tipo de mutex
		p_proc_actual->vectorMutexAbiertos[pos] = NULL; //se elimina el mutex de la lista de mutex abiertos por el proceso
	    p_proc_actual->num_mutex_abiertos--; //se reduce el numero de mutex abiertos por el procesos
		//se debe comprobar cuantas veces ha sido abierto y se desbloquean procesos
		desbloqueo_cerrar_mutex(&lista_bloqueados_mutex,mutexid);
		vectorMutex[mutexid]->n_veces_abierto--;
		printk("->SE HA CERRADO UN MUTEX QUE ESTABA USANDO EL PROCESO\n");
		if(vectorMutex[mutexid]->n_veces_abierto <= 0){
			//Se elimina mutex
			printk("->SE VA A ELIMINAR EL MUTEX %s\n",vectorMutex[mutexid]->nombre);
	        num_actual_mutex--;
	        vectorMutex[mutexid] = NULL; 
			desbloqueo_lista_abrir_mutex();
			printk("->SE ELIMINA UN MUTEX QUE NO ESTA ABIERTO POR NINGUN PROCESO\n");
		} 		 
	} 
	else{ 
	p_proc_actual->vectorMutexAbiertos[pos] = NULL;
	p_proc_actual->num_mutex_abiertos--; 
    vectorMutex[mutexid]->n_veces_abierto--;
	printk("->SE HA CERRADO UN MUTEX QUE ESTABA ABIERTO POR EL PROCESO\n");
	if(vectorMutex[mutexid]->n_veces_abierto <= 0){ 
		printk("->SE VA A ELIMINAR EL MUTEX %s\n",vectorMutex[mutexid]->nombre);
		num_actual_mutex--;
		vectorMutex[mutexid] = NULL; 
		desbloqueo_lista_abrir_mutex();
		printk("->SE ELIMINA UN MUTEX QUE NO ESTA ABIERTO POR NINGUN PROCESO\n");

		
	}	

   }
   return 1;	
	
} 

int leer_caracter() {
	int anterior = fijar_nivel_int(NIVEL_2);
    while(datos_buffer == 0){
		//no hay caracteres en el buffer nuevos, se bloquea al proceso y se produce un cambio de contexto
        aux_bloqueo(&lista_bloqueados_lectura_term);
	} 
	//se lee caracter
	int c;
	c = buffer_terminal[puntero_lectura];
	datos_buffer--;
	puntero_lectura = (puntero_lectura + 1)%TAM_BUF_TERM;
	//desbloqueo_proceso(&lista_bloqueados_lectura_term);
	fijar_nivel_int(anterior);
	return c; 
} 

static void inicializar_vector_mutex(){
int i;
for (i = 0; i < NUM_MUT; i++){
	vectorMutex[i] = NULL; 
} 

} 

/*
 * Tratamiento de llamada al sistema obtener_id_pr
 * 
 */

int sis_obtener_id_pr(){
    
	return obtener_id_pr();

} 


/*
 * Tratamiento de llamada al sistema dormir, recibe los segundos.
 */
int sis_dormir(){
    unsigned int segundos;
	segundos = (unsigned int) leer_registro(1);
	return dormir(segundos);

} 

/*
 * Tratamiento de llamada al sistema tiempos_proceso, puede o no recibir el struct tiempos_ejec
 */
int sis_tiempos_proceso(){
    
	struct tiempos_ejec *t_ejec;
	printk("LLEGA a sys tiempos proceso\n");
	t_ejec = (struct tiempos_ejec *) leer_registro(1);
	return tiempos_proceso(t_ejec);

} 

/*
 * Tratamiento de llamada al sistema crear_mutex.
 * 
 */
int sis_crear_mutex (){
   char * nombre;
   int tipo;
   nombre = (char *)leer_registro(1);
   tipo = (int)leer_registro(2);
   return crear_mutex(nombre,tipo);



} 

/*
 * Tratamiento de llamada al sistema abrir_mutex
 * 
 */
int sis_abrir_mutex (){
   char * nombre;
   nombre = (char *)leer_registro(1);
   return abrir_mutex(nombre);	
} 

/*
 * Tratamiento de llamada al sistema lock
 * 
 */
int sis_lock (){
   unsigned int mutexid;
   mutexid = (unsigned int) leer_registro(1);
   return lock(mutexid);	
} 

/*
 * Tratamiento de llamada al sistema unlock
 * 
 */
int sis_unlock (){
   unsigned int mutexid;
   mutexid = (unsigned int) leer_registro(1);
   return unlock(mutexid);	
	
} 

/*
 * Tratamiento de llamada al sistema cerrar_mutex
 * 
 */
int sis_cerrar_mutex (){
   unsigned int mutexid;
   mutexid = (unsigned int) leer_registro(1);
   return cerrar_mutex(mutexid);	
}

/*
 * Tratamiento de llamada al sistema leer_caracter
 * 
 */
int sis_leer_caracter (){
    return leer_caracter();

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
	inicializar_vector_mutex(); /* inicia el vector de mutex */
   
	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	//proceso inicial rodaja inf
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
