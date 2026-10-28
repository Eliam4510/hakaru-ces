/* ============================================================================
 *  capmotor.h  -  Capacitores equivalentes: sustituir un capacitor con los
 *  de la bodega
 *
 *  Taller > «Capacitores equivalentes» (capacitores/ESPEC_CAPEQ.md, la v3;
 *  se apoya en ESPEC_CAPMOTOR.md, la v2): el capacitor de arranque de un
 *  motor monofásico se quemó y en la bodega no hay uno igual. ¿Con cuáles se
 *  arma, con uno, dos o tres, en serie o en paralelo?
 *
 *    uno:            CT = c
 *    paralelo:       CT = a + b
 *    serie:          CT = a·b/(a + b)
 *    paralelo de 3:  CT = a + b + c
 *    serie de 3:     CT = a·b·c/(a·b + b·c + a·c)  (1/CT = 1/a + 1/b + 1/c)
 *    mixto 1:        (a ∥ b) en serie con c: CT = (a + b)·c/(a + b + c)
 *    mixto 2:        (a en serie con b) ∥ c: CT = a·b/(a + b) + c
 *
 *  La bodega admite REPETIDOS: cada vez que un valor aparece en la lista es
 *  un capacitor más de ese valor, y en cada arreglo cada capacitor se usa
 *  una sola vez (7 ∥ 7 solo si hay dos de 7).
 *
 *  Vale lo que cae en [C - t, C + t]; sin tolerancia (t = 0), lo exacto con
 *  el margen del redondeo a un decimal, ±0.05 MF (21·42/63 = 14 da «14 ✔»).
 *  Las comparaciones llevan un margen de 1e-9 (CPM_EPS): un CT en el borde
 *  exacto vale aunque la coma flotante lo deje un ulp fuera.
 *
 *  «Capacitores» (el modo): Auto (0) busca con 1, 2 y 3; 2 o 3, solo con
 *  ese número (lo pide el profe). El orden de la lista (§5.1): primero los
 *  de MENOS capacitores; dentro de cada número, |CT - C| (redondeada a 1e-9,
 *  para que dos cuentas iguales empaten aunque difieran en un ulp); luego el
 *  tipo, uno < paralelo < serie < paralelo de 3 < serie de 3 < mixto 1 <
 *  mixto 2; y luego los valores ascendentes. Es un orden total.
 *
 *  Sin gfx ni estado: funciones puras que prueba el arnés (test_capmotor.inc)
 *  y que compara el oráculo (fuerza bruta en Python) con el volcado. Los
 *  textos van en Latin-1 (las macros S_* de gfx.h), con PUNTO decimal como
 *  el PDF y el pizarrón (el resto de la app escribe coma), líneas de 52
 *  columnas como mucho y '\n' entre ellas. El dibujo, en capmotor_dib.c. El
 *  prefijo cpm_ viene de cuando se llamaba «Capacitor de motor».
 * ========================================================================= */
#ifndef CAPMOTOR_API_H
#define CAPMOTOR_API_H

#include <stddef.h>

#define CPM_MAX      100        /* capacitores de la bodega, contando repetidos   */
#define CPM_TMP      400        /* valores distintos que se juntan antes de quitar */
#define CPM_VER2     50         /* las de dos que se enseñan, como mucho           */
#define CPM_VER3     20         /* las de tres: las más cercanas                   */
#define CPM_VAL      (CPM_MAX + CPM_VER2 + CPM_VER3)
#define CPM_TXT      300        /* el texto de la bodega (sin el cero)             */
#define CPM_MARGEN   0.05       /* sin tolerancia: el redondeo a un decimal        */
#define CPM_EPS      1e-9
#define CPM_TOPE     100000.0   /* MF: más ya no es un capacitor de motor          */
#define CPM_VER_COLS 51         /* el ancho del veredicto (la pantalla: 52 - 1)    */

/* Las bodegas del pizarrón: la A (a la izquierda) y la B (del 15 al 42) */
extern const char CPM_BODEGA_A[];
extern const char CPM_BODEGA_B[];

enum { CPM_UNO = 0, CPM_PAR, CPM_SER, CPM_PAR3, CPM_SER3, CPM_MIX1, CPM_MIX2, CPM_NT };
#define CPM_MIX CPM_MIX1        /* el mixto de la v2 */
enum { CPM_AUTO = 0 };          /* el modo: 0 (Auto), 2 o 3 */

/* Una combinación: índices en los valores DISTINTOS de la bodega (v[],
 * ascendentes). Uno: a (b = c = a); paralelo y serie: a <= b (a = b si hay
 * dos de ese valor), c = b; paralelo y serie de tres: a <= b <= c; mixto 1:
 * (a ∥ b) en serie con c, a <= b; mixto 2: (a en serie con b) ∥ c, a <= b */
typedef struct {
    unsigned char tipo, a, b, c;
    double ct;
} CpmComb;

typedef struct CpmRes {
    double C, tol;              /* el pedido y su tolerancia (0: exacto)        */
    double lo, hi;              /* lo que vale: C ± t, o C ± 0.05               */
    int modo;                   /* 0 (Auto), 2 o 3                              */
    int n;                      /* valores distintos de la bodega               */
    int ncap;                   /* capacitores, contando repetidos              */
    double v[CPM_MAX];          /* los distintos, ascendentes                   */
    unsigned char q[CPM_MAX];   /* cuántos hay de cada uno                      */
    long tot[4];                /* las que valen con 1, 2 y 3 (todas)           */
    long ntot;                  /* las que valen en el modo                     */
    int nval;                   /* las que se enseñan, en el orden de la lista  */
    int nver[4];                /* de ellas, cuántas de 1, 2 y 3                */
    CpmComb val[CPM_VAL];
    int hay_cerca;              /* con ntot = 0: la más cercana del modo        */
    CpmComb cerca;
} CpmRes;

/* «Te faltaría»: con c1 de la bodega, el que falta va de lo a hi */
typedef struct { double c1, lo, hi; } CpmFalta;

/* ---- Leer lo escrito ---------------------------------------------------- */
enum {
    CPM_OK = 0,
    CPM_E_VALOR,                /* el valor pedido no es un número > 0          */
    CPM_E_TOL,                  /* la tolerancia: número >= 0 y menor que C     */
    CPM_E_NUMERO,               /* en la bodega, algo que no es un número       */
    CPM_E_CERO,                 /* un 0 en la bodega                            */
    CPM_E_RANGO,                /* un rango que no va de entero a entero        */
    CPM_E_MUCHOS,               /* más de CPM_MAX capacitores                   */
    CPM_E_GRANDE,               /* un número de más de CPM_TOPE (o muy largo)   */
    CPM_E_CHICO,                /* el valor pedido: > 0, pero no más que 0.05   */
    CPM_E_N
};
/* El valor pedido: número mayor que 0.05 (sin tolerancia vale C ± 0.05, que
 * tiene que quedar > 0), con punto o coma decimal */
int cpm_lee_valor(const char *s, double *v);
/* La tolerancia: vacía = 0 (exacto); si no, número >= 0 */
int cpm_lee_tol(const char *s, double *t);
/* La bodega: valores separados por espacios o comas («2.5 7.5»: el punto es
 * el decimal), rangos de enteros «15-42» (los dos incluidos, UNO de cada) y
 * lo que se quita: «-21-24», «-7» o todo lo que va tras «sin» («15-42 sin
 * 21-24»): quita TODOS los de esos valores, escritos donde sea. Cada
 * aparición de un valor es un capacitor («7 7» son dos de 7; un 20 suelto y
 * el rango 15-42 son dos de 20). Deja en v los capacitores, ascendentes y
 * con sus repetidos, y devuelve CPM_OK o el error (*n = 0) */
int cpm_lee_bodega(const char *s, double *v, int *n);
/* El aviso de un error, para la pantalla (<= 52 columnas) */
const char *cpm_error(int e);

/* ---- Resolver ----------------------------------------------------------- */
/* v (n <= CPM_MAX capacitores, > 0) en cualquier orden y con repetidos.
 * tol = 0: exacto (±0.05). modo: 0 (Auto), 2 o 3 (otro valor, Auto) */
void cpm_resuelve_n(CpmRes *r, double C, double tol, int modo, const double *v, int n);
/* Lo mismo en Auto */
void cpm_resuelve(CpmRes *r, double C, double tol, const double *v, int n);
/* ¿Vale ese CT? */
int cpm_vale(const CpmRes *r, double ct);
/* Los capacitores de un tipo: 1, 2 o 3 */
int cpm_ncap(int tipo);
/* «Te faltaría» (para resolverlo con dos): el directo y, en el orden de la
 * bodega, cada valor distinto c1 < C con el que falta en paralelo y cada c1
 * > C + t con el que falta en serie. Devuelven cuántos */
void cpm_falta_directo(const CpmRes *r, double *lo, double *hi);
int  cpm_faltas(const CpmRes *r, int tipo, CpmFalta *f, int max);
/* ¿Lleva «te faltaría»? Si el modo no es 3 y no hay ninguna que valga de
 * las que pide: con «2», ninguna de dos (aunque uno solo valga: el profe
 * pide dos); en Auto, ninguna de uno ni de dos */
int  cpm_con_faltas(const CpmRes *r);
/* El orden de la lista: < 0 si x va antes que y */
int  cpm_compara(const CpmRes *r, const CpmComb *x, const CpmComb *y);
/* El orden de «lo más cercano»: |CT - C|, luego menos capacitores, el tipo
 * y los valores */
int  cpm_compara_cerca(const CpmRes *r, const CpmComb *x, const CpmComb *y);
/* TODAS las que valen con los números de capacitores de la máscara (bit 1:
 * uno; 2: dos; 4: tres), sin orden ni tope: f(ctx, k) por cada una (el
 * volcado del oráculo las junta y las ordena) */
typedef void (*CpmVisita)(void *ctx, const CpmComb *k);
void cpm_recorre(const CpmRes *r, int mascara, CpmVisita f, void *ctx);
/* La máscara del modo: Auto 7, 2 la de dos, 3 la de tres */
int  cpm_mascara(int modo);
/* El CT de una combinación (con los valores de r) */
double cpm_ct(const CpmRes *r, int tipo, int a, int b, int c);

/* ---- Textos ------------------------------------------------------------- */
/* v con dec decimales como mucho, sin ceros de cola y con PUNTO: 46.7, 14 */
char *cpm_num(char *buf, size_t cap, double v, int dec);
/* «10 ∥ 7 = 17 MF», «21 y 42 en serie = 14 MF», «17 MF solo», «10 ∥ 5 ∥ 2
 * = 17 MF», «20, 30 y 60 en serie = 10 MF», «(18 ∥ 42) y 36 en serie = 22.5
 * MF», «(20 y 30 en serie) ∥ 5 = 17 MF». El CT, si es lo pedido (a 1e-9),
 * como lo pedido (2.25, no 2.3); si no, con los decimales que lo distinguen
 * de lo pedido («27 y 29 en serie = 13.98 MF» con 14, no «= 14 MF») */
void cpm_titulo(const CpmRes *r, const CpmComb *k, char *buf, size_t cap);
/* La fila de la lista de opciones, sin el número: «paralelo: 10 ∥ 7 = 17
 * MF», «serie: 21 y 42 = 14 MF», «mixto: (20 y 30) ∥ 5 = 17 MF» (en la
 * lista, «y» es en serie y «∥» en paralelo) */
void cpm_fila(const CpmRes *r, const CpmComb *k, char *buf, size_t cap);
/* La diferencia CT - C de la lista y del texto: «exacto» si es 0 (a 1e-9,
 * como el orden) y si no «dif. +0.02», con los decimales que la distinguen
 * de 0: los suyos si es un decimal corto (dif. -0.05); si no, uno (dos por
 * debajo de 0.1), y no menos que los de C */
void cpm_dif(const CpmRes *r, double ct, char *buf, size_t cap);
/* La diferencia de la opción k (una de r->val, o lo más cercano): la de
 * cpm_dif, con más decimales si la de una vecina de su grupo (con otra
 * distancia) se escribiría igual: «dif. -0.018» frente a «dif. -0.02» (las
 * dos con los decimales que las distinguen) */
void cpm_dif_op(const CpmRes *r, const CpmComb *k, char *buf, size_t cap);
/* La fila de la lista de opciones tal como se ve: «12. mixto: (40 ∥ 42) y 31
 * = 22.496 MF (dif. -0.004)», en maxch columnas como mucho. Si no cabe,
 * pierde por este orden « MF», el tipo («mixto: »), «dif. » y los espacios
 * de ∥; el CT y la diferencia se quedan. Solo si ni así cabe se corta con
 * «...». num: el número de la fila */
void cpm_fila_lista(const CpmRes *r, const CpmComb *k, int num, int maxch, char *buf, size_t cap);
/* El veredicto, en líneas ('\n' entre ellas) de CPM_VER_COLS columnas como
 * mucho: «Con 2: 10 ∥ 7 = 17 MF ✔», «Con dos no se puede (¿el profe lo puso
 * mal?);\ncon tres sí: ...», «IRRESOLUBLE con uno, dos o tres.\nLo más
 * cercano: ...». Un título largo pierde «en serie», « MF» y «dif. »; si ni
 * así cabe, pasa a la línea siguiente */
void cpm_veredicto(const CpmRes *r, char *buf, size_t cap);
/* El procedimiento (§4 y §5.4) de la opción S (0: la primera de la lista;
 * si no hay, el del irresoluble: lo más cercano y «te faltaría»), con
 * palabras y con las cuentas del profe: líneas con '\n'. Devuelve el número
 * del paso siguiente */
int cpm_proced(const CpmRes *r, const CpmComb *S, char *txt, size_t cap);
/* La leyenda del dibujo de k (<= 52 columnas): «Cada capacitor toca A y T.» */
const char *cpm_leyenda(const CpmRes *r, const CpmComb *k);
/* Corta un texto de cpm_proced en líneas de pantalla de maxch columnas (por
 * palabras; '\n' corta): cada una es «sangria» espacios y txt[ini, ini +
 * len). La continuación de una línea lleva la sangría de esa línea más 2 (3
 * si no tenía: «1. ...»). Devuelve cuántas (como mucho max) */
typedef struct { unsigned short ini; unsigned char len, sangria; } CpmLinea;
int cpm_envuelve(const char *txt, int maxch, CpmLinea *l, int max);

/* ---- La edición con cursor (los campos de los datos) -------------------- */
/* Un campo de texto en líneas de w columnas: por palabras, sin sangría (el
 * espacio del corte se queda al final de su línea; una palabra más larga
 * que w se parte). ini[i] es donde empieza la línea i; devuelve cuántas
 * (como mucho max, y al menos 1) */
int cpm_lineas_campo(const char *s, int w, int *ini, int max);
/* La línea (y en *col la columna) del cursor pos */
int cpm_linea_de(const int *ini, int nl, int len, int pos, int *col);
/* El cursor tras saltar de valor: al principio del valor siguiente (dir >
 * 0) o del anterior (dir < 0); los valores van entre espacios */
int cpm_salta_valor(const char *s, int pos, int dir);

/* ---- El dibujo (capmotor_dib.c) ----------------------------------------- */
/* El cableado de k al estilo del profe (figuras 2-4 del PDF) entre y0 e y1:
 * la clavija, la punteada a C, la sólida al borne que va a T, el opuesto a
 * A, y los capacitores como óvalos con su valor. no_entra: la leyenda en
 * rojo (lo más cercano de un irresoluble) */
void cpm_dibuja(const CpmRes *r, const CpmComb *k, int y0, int y1, int no_entra);
#ifndef NDLESS
/* Para el arnés: los rectángulos de los rótulos y de los óvalos del último
 * dibujo (que no se pisen) */
typedef struct { int x, y, w, h, ocupados; } CpmCaja;   /* ocupados: lo que había debajo del rótulo; -1 en un óvalo */
int cpm_cajas(CpmCaja *c, int max);
#endif

#endif /* CAPMOTOR_API_H */
