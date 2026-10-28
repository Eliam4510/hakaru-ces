/* ============================================================================
 *  numfmt.h  -  Numeros a texto y construccion de cadenas, sin printf
 *
 *  El printf de newlib con soporte de coma flotante anade decenas de KB al
 *  .tns, y aqui solo hacen falta numeros decimales normales y concatenar
 *  trozos. Con esto el binario se queda pequeno y el formato es el que se
 *  espera en espanol: coma decimal.
 * ========================================================================= */
#ifndef NUMFMT_H
#define NUMFMT_H

#include <stddef.h>

/* Escribe v con exactamente 'dec' decimales.  1234.5 -> "1234,50"          */
char *num_txt(char *buf, size_t cap, double v, int dec);
/* Igual, pero quita los ceros finales y la coma si sobra. 30.0 -> "30"     */
char *num_auto(char *buf, size_t cap, double v, int dec);

/* --------------------------------------------------------------------------
 *  Constructor de cadenas con tope. Nunca desborda y siempre deja el cero
 *  final; si no cabe algo, se trunca y se marca.
 * ----------------------------------------------------------------------- */
typedef struct {
    char  *p;
    size_t cap;
    size_t n;
    int    lleno;
} Buf;

void buf_init(Buf *b, char *dst, size_t cap);
void buf_str(Buf *b, const char *s);
void buf_ch(Buf *b, char c);
void buf_num(Buf *b, double v, int dec);    /* decimales fijos */
void buf_auto(Buf *b, double v, int dec);   /* sin ceros finales */
void buf_int(Buf *b, long v);

/* Lee un numero de texto. Acepta coma o punto decimal, porque el teclado de
 * la calculadora escribe punto y la pantalla muestra coma. */
double num_lee(const char *s);

/* --------------------------------------------------------------------------
 *  Propio de este programa
 * ----------------------------------------------------------------------- */
/* Lectura con exponente (tecla EE: "4,7E3") y prefijo final k o M
 * ("4,7k" = 4700). *ok = 0 si el texto no es un numero. */
double num_lee_ing(const char *s, int *ok);

/* Cuentas en la ficha: «220^2/100», «1/(1/4k+1/6k)», «4,7k+1k». Los números
 * son los de num_lee_ing (coma, [EE] y prefijo pegado a cada uno); los signos,
 * + - × ÷ ^ (también * y /) y paréntesis, con la precedencia de siempre (^ a
 * la derecha y antes que el - de delante: -2^2 = -4). Un número solo da
 * exactamente lo de num_lee_ing. *err = NE_OK, o por qué no vale (y 0). */
enum {
    NE_OK = 0,
    NE_NUMERO,       /* un número mal escrito («4,5E») o nada que leer      */
    NE_PAR_ABRE,     /* falta cerrar un paréntesis                           */
    NE_PAR_CIERRA,   /* sobra un «)»                                         */
    NE_FALTA,        /* a medias: falta un número («2+», «()»)               */
    NE_SIGNO,        /* dos números sin signo entre ellos («(1)(2)», «2(3)») */
    NE_CERO,         /* división entre cero (y 0^-1)                         */
    NE_REAL,         /* no da un número real ((-8)^0,5)                      */
    NE_GRANDE,       /* da infinito (1E300×1E300)                            */
    NE_N
};
double num_eval(const char *s, int *err);
/* ¿Es una cuenta (tiene +, ×, ÷, ^, paréntesis o una resta) y no un número? */
int  num_es_cuenta(const char *s);

/* Cuatro cifras significativas y al menos dos decimales, sin ceros de cola:
 * 146,6667 -> "146,67"   0,00012 -> "0,00012"   12288 -> "12288".
 * Devuelve 1 si el texto es el valor EXACTO y 0 si se ha redondeado, para
 * escribir "=" o "aprox." delante. */
int  num_sig(char *buf, size_t cap, double v);
void buf_sig(Buf *b, double v);
/* " = 12,5" o " ~ 12,67" segun num_sig */
void buf_igual(Buf *b, double v);

/* Rotulo corto para el esquema: tres cifras y prefijo k o M a partir de
 * 10 000, para que quepa en el hueco entre dos columnas de la rejilla. */
char *num_corto(char *buf, size_t cap, double v);

/* Cifras significativas con prefijo del SI (p..G) pegado a la unidad:
 * "6,32V", "-500mA", "1,59kHz". Con la unidad " s" sale "2 ms". Ancho:
 * como mucho cifras + 3 + strlen(unidad). */
char *num_pref(char *buf, size_t cap, double v, int cifras, const char *unidad);
/* Una potencia de la CA: 4 cifras con prefijo, pero de 1000 a 9999 sin él
 * ("1142 VA", "-2403 var"; "906,1 W", "12,50 kvar"). uni sin el espacio */
char *num_pot(char *buf, size_t cap, double v, const char *uni);

/* Fracción p/q (q <= qmax) que reproduce v hasta 1e-10 relativo, por
 * fracciones continuas. Devuelve 1 solo si es útil mostrarla: no es un
 * entero y su decimal no es exacto (440/7 sí; 0,75 no, ya es exacto). */
int  num_fraccion(double v, long *p, long *q, long qmax);

/* --------------------------------------------------------------------------
 *  Fasores (Etapa 3). El ángulo es "\x7F" (S_ANG) y el grado "\xB0" (S_DEG).
 * ----------------------------------------------------------------------- */
/* Grados a (-180, 180]; -180 sale 180 */
double num_ang(double grados);
/* "30+j40", "30-j40", "j40", "1,5k+j2m", "4,7E3-j1E-3" (o un real solo):
 * devuelve la parte real y deja la imaginaria en *im; *ok = 0 si no lo es */
double num_lee_cx(const char *s, double *im, int *ok);
/* Fasor ya en el convenio (módulo y grados). corto = 0 (tablas): polar
 * "84,85∠-45,0° V" (4 cifras; el ángulo con un decimal si |θ| < 99,95, si no
 * entero); rect "7,55+j5,80 A" (3 cifras, prefijo común). Como mucho 15
 * columnas con una unidad de 1 carácter. corto = 1 (esquema): 3 cifras,
 * grados enteros y sin espacio, "9,52∠38°A": como mucho 12. */
char *num_fasor(char *buf, size_t cap, double mod, double grados, int rect, int corto, const char *unidad);

#ifndef NDLESS
/* Para el arnés: 1 = pow(10, k) y log10 en cada número y las cuentas de 64
 * bits en formatea, como antes (las potencias memorizadas, la década por los
 * bits y los 32 bits tienen que dar las mismas cadenas) */
void num_lento(int si);
/* floor(log10(a)) por los bits (o por log10 donde hace falta) */
int  num_decada(double a);
#endif

#endif /* NUMFMT_H */
