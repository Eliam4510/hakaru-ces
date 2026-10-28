/* ============================================================================
 *  fasorial.h  -  Diagrama fasorial y triángulo de potencias (P_FASORIAL)
 *
 *  Como osc.h: funciones puras (dónde cae la punta de un fasor, la escala
 *  automática, dónde acaba la cadena) que el arnés prueba sin mirar píxeles,
 *  y la pantalla entera (fz_dibuja) con sus teclas (fz_tecla).
 *
 *  Vista de fasores. El dibujo va de x = 0 a 207 e y = 16 a 226, con el
 *  origen en (FZ_CX, FZ_CY) y el borde en la circunferencia de radio FZ_R.
 *  La lista tiene la V y la I de cada elemento (sin las que son nulas, como
 *  la I de un voltímetro ideal), en el orden del circuito. Dos familias,
 *  tensiones (0) y corrientes (1), cada una con su escala del borde: un
 *  índice de ESCALA (osc.h), 1-2-5, en las unidades que se ven (el convenio).
 *  Las V con trazo grueso; las I, discontinuo.
 *    - Referencia (ref): todo gira -θ_ref; los ángulos de la leyenda también.
 *    - Cadena ([S]): los visibles de una familia que no son fuentes, uno tras
 *      otro desde el origen, y «Σ» punteado del origen al final. Las fuentes
 *      se quedan en el origen: en serie la cadena de las V acaba en la punta
 *      de E; en paralelo, la de las I en la de I(E).
 *
 *  Vista de potencias. El triángulo P (horizontal), Q (vertical: arriba si
 *  es inductiva) y S (hipotenusa) de lo que entregan las fuentes o del
 *  elemento elegido, con el origen en (20, 121) y k = mín(170/|P|, 95/|Q|)
 *  px por W. Con Boucherot ([S]), las (P, Q) de cada carga una tras otra, que
 *  acaban en la S total.
 *
 *  Convenios (D4): los fasores internos (pico, coseno) se enseñan con
 *  ca_a_conv(conv, ...). Las escalas automáticas se eligen con c->conv
 *  (fz_nuevo, fz_tecla): ui.c pasa a fz_dibuja ese mismo conv. fz_vector lo
 *  recibe; fz_punta (sin conv) usa el convenio 0 (eficaces, coseno), el de
 *  las pruebas. [.] (polar o rectangular) es de la sesión: lo atiende ui.c
 *  y llega en el argumento rect de fz_dibuja (la forma de arriba en la
 *  leyenda; la otra va debajo, en gris).
 * ========================================================================= */
#ifndef FASORIAL_H
#define FASORIAL_H

#include "ca.h"

#define FZ_CX 104
#define FZ_CY 121
#define FZ_R 100
#define FZ_MAX 64
enum { FZ_FASORES = 0, FZ_POTENCIAS };  enum { FZ_NADA = 0, FZ_SALIR };

/* z->cadena: bits 0-1 la familia encadenada (0 = ninguna, 1 = las V,
 * 2 = las I); FZ_BOUCHEROT, la cadena de la vista de potencias */
#define FZ_BOUCHEROT 4

typedef struct Fasorial {
    int n; signed char el[FZ_MAX]; unsigned char que[FZ_MAX], ver[FZ_MAX];  /* que: 'V' o 'I' */
    int sel, ini, ref;                 /* ref = índice en la lista, o -1 (ángulos absolutos) */
    unsigned char vista, cadena;
    signed char iesc[2]; unsigned char ajustado[2];   /* escala del borde por familia (ESCALA) */
    int selp, inip;                    /* potencias: 0 = total; 1.. = elementos con S ≠ 0 */
} Fasorial;

void fz_nuevo(Fasorial *z, const Circuito *c, const CaRes *s);
void fz_dibuja(const Fasorial *z, const Circuito *c, const CaRes *s, int conv, int rect);
int  fz_tecla(Fasorial *z, const Circuito *c, const CaRes *s, int t);
void fz_punta(const Fasorial *z, const CaRes *s, int i, double ox, double oy, int *x, int *y);  /* pura */

/* ---- Más funciones puras (para el arnés) ---- */
/* El fasor i en píxeles (y hacia abajo), en el convenio conv, sin redondear */
void   fz_vector(const Fasorial *z, const CaRes *s, int i, int conv, double *dx, double *dy);
/* Índice en la lista de la V ('V') o la I ('I') del elemento k, o -1 */
int    fz_busca(const Fasorial *z, int k, int que);
/* Escala automática de las dos familias: la menor de ESCALA con el punto
 * dibujado más lejano (punta o eslabón de la cadena) al 90 % de FZ_R como
 * mucho. Deja ajustado a 0 */
void   fz_autoescala(Fasorial *z, const Circuito *c, const CaRes *s, int conv);
/* Final de la cadena como se dibuja (redondeado una sola vez). Devuelve los
 * eslabones; 0 sin cadena (y *x, *y en el origen) */
int    fz_cadena_fin(const Fasorial *z, const Circuito *c, const CaRes *s, int conv, int *x, int *y);
/* Elementos con S ≠ 0 (la lista de potencias sin el total) y el k-ésimo (1..) */
int    fz_npot(const Circuito *c, const CaRes *s);
int    fz_pot_el(const Circuito *c, const CaRes *s, int j);
/* Escala del triángulo (px por W, var o VA) como se dibuja; 0 = nada */
double fz_escala_pot(const Fasorial *z, const Circuito *c, const CaRes *s);

#endif /* FASORIAL_H */
