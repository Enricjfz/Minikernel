/*
 *  usuario/include/servicios.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene los prototipos de funciones de
 * biblioteca que proporcionan la interfaz de llamadas al sistema.
 *
 *      SE DEBE MODIFICAR AL INCLUIR NUEVAS LLAMADAS
 *
 */

#ifndef SERVICIOS_H
#define SERVICIOS_H

/* Evita el uso del printf de la bilioteca est�ndar */
#define printf escribirf

/*
*
* Definición de la estructura tiempos_ejec que se utiliza en la funcionalidad
* de tiempos_proceso
*
*/

    

/*Variables para mutex */


#define NO_RECURSIVO 0
#define RECURSIVO 1

/* Funcion de biblioteca */
int escribirf(const char *formato, ...);
/* Struct tiempos eject **/
struct tiempos_ejec {
    int usuario; //numero de interrupciones que ha recibido el proceso en modo usuario
    int sistema; //numero de interrupciones que ha recibido el proceso en modo sistema
};

/* Llamadas al sistema proporcionadas */
int crear_proceso(char *prog);
int terminar_proceso();
int escribir(char *texto, unsigned int longi);
int obtener_id_pr();
int dormir(unsigned int segundos);
int tiempos_proceso(struct tiempos_ejec *t_ejec);
int crear_mutex(char *nombre, int tipo);
int abrir_mutex(char *nombre);
int lock(unsigned int mutexid);
int unlock(unsigned int mutexid);
int cerrar_mutex(unsigned int mutexid);
int leer_caracter();


#endif /* SERVICIOS_H */

