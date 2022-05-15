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

typedef struct tiempos_ejec {
    int usuario; //numero de interrupciones que ha recibido el proceso en modo usuario
    int sistema; //numero de interrupciones que ha recibido el proceso en modo sistema
};

/* Funcion de biblioteca */
int escribirf(const char *formato, ...);

/* Llamadas al sistema proporcionadas */
int crear_proceso(char *prog);
int terminar_proceso();
int escribir(char *texto, unsigned int longi);
int obtener_id_pr();

#endif /* SERVICIOS_H */

