/* ============================================================================
 *  archivo.h  -  "Mis problemas": guardar y abrir circuitos
 *
 *  Cada problema es un archivo pequeno en la MISMA carpeta que el programa
 *  (asi no hace falta crear carpetas en la calculadora):
 *
 *      /documents/<carpeta>/ces_01.tns ... ces_20.tns
 *
 *  (y ces_sesion.tns, la sesión, y ces_controles.tns, los Controles).
 *
 *  La extension .tns es la que deja pasar el sistema de archivos de la Nspire
 *  al copiar con TI Connect, asi que sirven tambien de copia de seguridad.
 *
 *  El formato (marca "CESP" y version; ver archivo.c y docs/DESIGN.md, «Nombre y
 *  formato») se escribe campo a campo (no se vuelca la estructura), para que
 *  un archivo guardado en el PC se lea igual en la calculadora.
 * ========================================================================= */
#ifndef ARCHIVO_H
#define ARCHIVO_H

#include "circuito.h"

#define ARCH_HUECOS 20

/* Carpeta a partir de la ruta del programa (argv[0] en Ndless) o de una
 * carpeta directa en el PC. */
void arch_carpeta(const char *ruta);

int  arch_existe(int hueco);                 /* 1 si hay problema guardado  */
int  arch_guardar(int hueco, const Circuito *c);   /* 1 si se pudo      */
int  arch_cargar(int hueco, Circuito *c);          /* 1 si se pudo      */
int  arch_borrar(int hueco);
int  arch_libre(void);                       /* primer hueco libre, o -1    */

#if ESPECIAL
#include "especial_api.h"              /* la versión especial (no se publica) */
#endif

/* La sesión (ces_sesion.tns, junto a los problemas): el circuito del editor
 * al salir, su hueco de Mis problemas (-1 si ninguno) y el cursor, para
 * seguir con él al volver a abrir el programa. El circuito va en CESP y se
 * valida como un problema: un archivo roto no se carga (0) y no toca c. El
 * hueco se lee solo si su archivo es el mismo que al guardar la sesión (con
 * otro, o sin él, -1: Guardar no pisa un problema distinto). */
int  arch_sesion_guardar(const Circuito *c, int hueco, int cx, int cy);
int  arch_sesion_cargar(Circuito *c, int *hueco, int *cx, int *cy);
int  arch_sesion_borrar(void);

/* Un archivo de bytes de nombre fijo junto a los problemas («/ces_...tns»,
 * con su barra): el panel Controles (ces_controles.tns, 8 bytes, o 12 con
 * la sección del turbo: ui.c, controles_guarda) y el
 * grabador de la capa de entrada (ces_muestras.tns). Guardar: 1 si se pudo
 * escribir entero. Leer: los bytes leídos (como mucho cap), o -1 si no hay
 * archivo. Borrar: 1 si se borró. */
int  arch_bytes_guardar(const char *nombre, const unsigned char *d, int n);
int  arch_bytes_leer(const char *nombre, unsigned char *d, int cap);
int  arch_bytes_borrar(const char *nombre);

#endif /* ARCHIVO_H */
