/* ============================================================================
 *  archivo.c  -  Guardar y abrir circuitos  (ver archivo.h)
 *
 *  TODA escritura del programa (problemas, sesión, Controles, el grabador;
 *  borrar también) pasa por la puerta de abajo: abre_w y cierra_w,
 *  o quita. Con el turbo (turbo.h), el reloj vuelve a 396 MHz antes de tocar
 *  la NAND y sube después; si no pudo volver, no se escribe. Una escritura
 *  nueva, por la puerta.
 * ========================================================================= */
#include <stdio.h>
#include <string.h>
#include "archivo.h"
#include "turbo.h"

/* Formato CESP (CES = Circuit Emulator & Solver; P = problema), el de los
 * archivos ces_01.tns ... ces_20.tns. Todo en little-endian.
 *
 *   "CESP", versión (1 byte), columnas, filas, nombre (24 bytes; si es más
 *   largo, la cola va en la sección 3 del bloque global), segmentos h y v,
 *   bloque global: su longitud (1 byte) y tierra (x, y), terminales A y B
 *                  (x, y), convenio, frecuencia (magnitud),
 *   nel, y por cada elemento: su longitud (1 byte), tipo, xa ya xb yb,
 *                  etiqueta[6], val, V, I, aux (magnitudes), variante, ref.
 *
 *   Una magnitud son 18 bytes: módulo y ángulo (double) y sus dos estados.
 *   Las longitudes dejan que una versión futura añada campos al final de un
 *   bloque: esta los salta; un bloque más corto que lo que se sabe leer no
 *   se acepta.
 *
 *   Cada bloque crece por SECCIONES al final, que se escriben solo si hacen
 *   falta y se leen solo si caben enteras (una sección publicada no cambia
 *   de tamaño; quien necesite la n escribe también las anteriores):
 *     global, sección 2 (28 B): las dos sondas (tipo, x, y, el, sg) y t_fin
 *       y paso del emulador (magnitud corta: double y estado, 9 B);
 *     global, sección 3 (16 B): la cola del nombre, sus bytes 24 a 39
 *       (Circuito.nombre[40], rev8: los títulos de la biblioteca pasan de
 *       23), solo si el nombre pasa de 23; la cabecera lleva los 24
 *       primeros. Un lector de antes salta la sección y se queda con 23,
 *       como siempre: no pide versión nueva;
 *     elemento, sección 2 (54 B): par[0..5] del emulador (magnitudes cortas);
 *     elemento, sección 3 (2 B): xc, yc, solo en los de 3 patas.
 *
 *   La versión dice qué parte del modelo hace falta conocer para leerlo bien
 *   (version_de): 1 la CC; 2 lo que cambia con el tiempo (C, L, S, una E o J
 *   que no es continua); 3 la CA (alterna, Z, vatímetro, ángulos); 4 los
 *   semiconductores, el AO y las controladas. Este programa lee hasta la
 *   VERSION_MAX y rechaza lo más nuevo en vez de leerlo mal; lo que no pide
 *   una versión nueva (sondas, parámetros) va en secciones que un lector
 *   anterior salta.
 *
 *   Hasta la Etapa 4 este mismo formato llevaba la marca "EFK3" (y antes
 *   hubo un "EFK1"): con el nombre nuevo cambió la marca y esos archivos ya
 *   no se leen. Ver docs/DESIGN.md, «Nombre y formato». */
#define MAGIA        "CESP"
#define VERSION_MIN  1
#define VERSION_MAX  4
#define MAG_BYTES 18
#define MAGC_BYTES 9                                   /* magnitud corta */
#define GLOBAL_BYTES (6 + 1 + MAG_BYTES)
#define ELEM_BYTES (5 + 6 + 4 * MAG_BYTES + 2)
#define GLOBAL2_BYTES (2 * 5 + 2 * MAGC_BYTES)         /* 28 */
#define NOM_CAB 24                                     /* los caracteres del nombre en la cabecera */
#define GLOBAL3_BYTES 16                               /* la cola del nombre */
#define ELEM2_BYTES (NPAR * MAGC_BYTES)                /* 54 */
typedef char cabe_global[GLOBAL_BYTES + GLOBAL2_BYTES + GLOBAL3_BYTES <= 255 ? 1 : -1];
typedef char cabe_nombre[NOM_CAB + GLOBAL3_BYTES == sizeof ((Circuito *)0)->nombre ? 1 : -1];
typedef char cabe_elem[ELEM_BYTES + ELEM2_BYTES <= 255 ? 1 : -1];
#define ELEM3_BYTES 2          /* xc, yc: solo en los de 3 patas (Etapa 4)             */
typedef char cabe_elem3[ELEM_BYTES + ELEM2_BYTES + ELEM3_BYTES <= 255 ? 1 : -1];   /* 141 */

static char carpeta[200] = "/documents";

void arch_carpeta(const char *ruta)
{
    size_t n, corte = 0, i;
    if (!ruta || !*ruta) return;
    n = strlen(ruta);
    if (n >= sizeof carpeta) return;
    /* Si es la ruta de un archivo (termina en .tns), se queda con su carpeta */
    if (n > 4 && strcmp(ruta + n - 4, ".tns") == 0) {
        for (i = 0; i < n; i++) if (ruta[i] == '/') corte = i;
        if (corte == 0) return;
        memcpy(carpeta, ruta, corte);
        carpeta[corte] = 0;
    } else {
        memcpy(carpeta, ruta, n + 1);
    }
}

static void nombre(int hueco, char *buf, size_t cap)
{
    size_t n = strlen(carpeta);
    if (n + 20 >= cap) { buf[0] = 0; return; }
    memcpy(buf, carpeta, n);
    memcpy(buf + n, "/ces_00.tns", 12);             /* ces_01.tns ... ces_20.tns */
    buf[n + 5] = (char)('0' + (hueco + 1) / 10);
    buf[n + 6] = (char)('0' + (hueco + 1) % 10);
}

/* Un archivo de nombre fijo junto a los problemas, como "/ces_sesion.tns"
 * (con su barra) */
static void ruta_fija(const char *arch, char *buf, size_t cap)
{
    size_t n = strlen(carpeta), m = strlen(arch);
    if (n + m + 1 > cap) { buf[0] = 0; return; }
    memcpy(buf, carpeta, n);
    memcpy(buf + n, arch, m + 1);
}

/* ---- escritura y lectura campo a campo ---- */
static void w8(FILE *f, int v) { fputc(v & 0xFF, f); }

static void wdbl(FILE *f, double d)
{
    unsigned char b[8];
    int i;
    memcpy(b, &d, 8);                  /* ARM EABI y x86 son little-endian */
    for (i = 0; i < 8; i++) fputc(b[i], f);
}

static int r8(FILE *f, int *ok)
{
    int c = fgetc(f);
    if (c == EOF) { *ok = 0; return 0; }
    return c;
}

/* Lectura desde un bloque ya leído (con su longitud) */
typedef struct { const unsigned char *p; int n, i, ok; } Bloque;

static int b8(Bloque *b)
{
    if (b->i >= b->n) { b->ok = 0; return 0; }
    return b->p[b->i++];
}

static double bdbl(Bloque *b)
{
    unsigned char t[8];
    double d;
    int i;
    for (i = 0; i < 8; i++) t[i] = (unsigned char)b8(b);
    memcpy(&d, t, 8);
    return d;
}

static void wmag3(FILE *f, const Mag *m)
{
    wdbl(f, m->v); wdbl(f, m->ang); w8(f, m->est); w8(f, m->est_ang);
}

static void bmag3(Bloque *b, Mag *m)
{
    m->v = bdbl(b); m->ang = bdbl(b);
    m->est = (unsigned char)b8(b); m->est_ang = (unsigned char)b8(b);
    if (m->est > M_BUSCAR || m->est_ang > M_BUSCAR) b->ok = 0;
    if (!FINITO(m->v) || !FINITO(m->ang)) b->ok = 0;
}

/* Magnitud corta: valor y estado (los parámetros del emulador: dato o vacío) */
static void wmagc(FILE *f, const Mag *m) { wdbl(f, m->v); w8(f, m->est); }

static void bmagc(Bloque *b, Mag *m)
{
    m->v = bdbl(b); m->est = (unsigned char)b8(b);
    m->ang = 0.0; m->est_ang = M_LIBRE;
    if (m->est > M_DATO || !FINITO(m->v)) b->ok = 0;
}

static void wsonda(FILE *f, const Sonda *s)
{
    w8(f, s->tipo); w8(f, s->x); w8(f, s->y); w8(f, s->el); w8(f, s->sg);
}

static void bsonda(Bloque *b, Sonda *s)
{
    s->tipo = (unsigned char)b8(b);
    s->x = (signed char)b8(b); s->y = (signed char)b8(b);
    s->el = (signed char)b8(b); s->sg = (signed char)b8(b);
}

/* ¿Hace falta la sección 2 del bloque global, o la del elemento? */
static int mag_usada(const Mag *m) { return m->est != M_LIBRE || m->v != 0.0; }

/* La sección 3: un nombre de más de 23 caracteres (sin ella, el lector
 * corta en 23: el byte 24 de la cabecera siempre fue su 0). Con ella, la 2 */
static int global3(const Circuito *c)
{
    return strlen(c->nombre) > NOM_CAB - 1;
}

static int global2(const Circuito *c)
{
    return c->sonda[0].tipo != SONDA_NO || c->sonda[1].tipo != SONDA_NO ||
           mag_usada(&c->t_fin) || mag_usada(&c->paso) || global3(c);
}

static int elem2(const Elemento *e)
{
    int i;
    for (i = 0; i < NPAR; i++) if (mag_usada(&e->par[i])) return 1;
    return 0;
}

/* Versión 3 si un lector que solo sepa hasta la 2 lo leería mal: una fuente
 * alterna, una Z o un vatímetro, el valor de una C o L en [?!], o un ángulo
 * en un campo que lo tiene. Versión 2 si lo leería mal uno que solo sepa la
 * 1: un tipo dinámico (C, L, S) o una fuente que no es continua */
static int version_de(const Circuito *c)
{
    int k, q;
    /* Versión 4 si hay algún tipo de la Etapa 4 (lo primero: un lector de la
     * versión 3 no los conoce) */
    if (circ_etapa4(c)) return 4;
    if (circ_alterna(c)) return 3;
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if (e->tipo >= EL_IMP) return 3;
        if ((e->tipo == EL_COND || e->tipo == EL_BOB) && e->val.est == M_BUSCAR) return 3;
        for (q = 0; q < Q_N; q++) {
            const CampoInfo *ci = circ_campo(e, q);
            if (ci && (ci->flags & CF_ANG) && circ_mag_c(e, q)->est_ang != M_LIBRE) return 3;
        }
    }
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if (tipo_info(e->tipo)->dinamico) return 2;
        if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante != 0) return 2;
    }
    return 1;
}

/* Lee un bloque precedido de su longitud; exige al menos `minimo` bytes */
static int lee_bloque(FILE *f, unsigned char *buf, int minimo, Bloque *b, int *ok)
{
    int n = r8(f, ok), i;
    for (i = 0; *ok && i < n; i++) buf[i] = (unsigned char)r8(f, ok);
    b->p = buf; b->n = n; b->i = 0; b->ok = *ok && n >= minimo;
    return b->ok;
}

int arch_existe(int hueco)
{
    char ruta[240];
    FILE *f;
    if (hueco < 0 || hueco >= ARCH_HUECOS) return 0;
    nombre(hueco, ruta, sizeof ruta);
    f = fopen(ruta, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* El circuito en CESP, en un archivo ya abierto (el de un hueco o la sesión) */
static void escribe(FILE *f, const Circuito *c)
{
    int x, y, k, i;

    fwrite(MAGIA, 1, 4, f);
    w8(f, version_de(c));
    w8(f, REJ_COLS); w8(f, REJ_FILAS);
    for (i = 0; i < NOM_CAB; i++) w8(f, global3(c) || i < NOM_CAB - 1 ? c->nombre[i] : 0);
    for (y = 0; y < REJ_FILAS; y++) for (x = 0; x < REJ_COLS - 1; x++) w8(f, c->h[y][x]);
    for (y = 0; y < REJ_FILAS - 1; y++) for (x = 0; x < REJ_COLS; x++) w8(f, c->v[y][x]);

    w8(f, GLOBAL_BYTES + (global2(c) ? GLOBAL2_BYTES : 0) + (global3(c) ? GLOBAL3_BYTES : 0));
    w8(f, c->tierra_x); w8(f, c->tierra_y);
    w8(f, c->term_x[0]); w8(f, c->term_y[0]);
    w8(f, c->term_x[1]); w8(f, c->term_y[1]);
    w8(f, c->conv);
    wmag3(f, &c->frec);
    if (global2(c)) {
        wsonda(f, &c->sonda[0]); wsonda(f, &c->sonda[1]);
        wmagc(f, &c->t_fin); wmagc(f, &c->paso);
    }
    if (global3(c)) for (i = NOM_CAB; i < NOM_CAB + GLOBAL3_BYTES; i++) w8(f, c->nombre[i]);   /* sección 3 */

    w8(f, c->nel);
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        /* las secciones son acumulativas: un 3 patas escribe también la 2 */
        int tres = circ_npatas(e) == 3;
        w8(f, ELEM_BYTES + (elem2(e) || tres ? ELEM2_BYTES : 0) + (tres ? ELEM3_BYTES : 0));
        w8(f, e->tipo); w8(f, e->xa); w8(f, e->ya); w8(f, e->xb); w8(f, e->yb);
        for (i = 0; i < 6; i++) w8(f, e->etiq[i]);
        wmag3(f, &e->val); wmag3(f, &e->V); wmag3(f, &e->I); wmag3(f, &e->aux);
        w8(f, e->variante);
        w8(f, e->ref);
        if (elem2(e) || tres) for (i = 0; i < NPAR; i++) wmagc(f, &e->par[i]);
        if (tres) { w8(f, e->xc); w8(f, e->yc); }       /* sección 3: la pata c */
    }
}

/* Cierra lo escrito: 1 si todo llegó al archivo */
static int cierra(FILE *f)
{
    int i = ferror(f);
    if (fclose(f) != 0) i = 1;
    return !i;
}

/* ---- La puerta: turbo_pausa antes (a 396; 0 si el reloj no pudo volver:
 *      entonces no se escribe) y turbo_sigue después, siempre en pareja.
 *      abre_w abre para escribir (si no se abre, ya ha seguido); cierra_w
 *      cierra lo que abrió (1 si todo llegó); quita borra (1 si se borró) ---- */
static FILE *abre_w(const char *ruta)
{
    FILE *f = 0;
    if (turbo_pausa()) { TURBO_TESTIGO(ruta); f = fopen(ruta, "wb"); }
    if (!f) turbo_sigue();
    return f;
}

static int cierra_w(FILE *f)
{
    int ok = cierra(f);
    turbo_sigue();
    return ok;
}

static int quita(const char *ruta)
{
    int r = -1;
    if (turbo_pausa()) { TURBO_TESTIGO(ruta); r = remove(ruta); }
    turbo_sigue();
    return r == 0;
}

int arch_guardar(int hueco, const Circuito *c)
{
    char ruta[240];
    FILE *f;

    if (hueco < 0 || hueco >= ARCH_HUECOS) return 0;
    nombre(hueco, ruta, sizeof ruta);
    f = abre_w(ruta);
    if (!f) return 0;
    escribe(f, c);
    return cierra_w(f);
}

/* La rejilla y el nombre (su byte 24, aparte: vale si luego viene la
 * sección 3 del bloque global; si no, el nombre acaba en el 23) */
static void lee_rejilla(FILE *f, Circuito *t, int *ok, char *nom24)
{
    int x, y, i;
    if (*ok && (r8(f, ok) != REJ_COLS || r8(f, ok) != REJ_FILAS)) *ok = 0;
    for (i = 0; *ok && i < NOM_CAB; i++) t->nombre[i] = (char)r8(f, ok);
    *nom24 = t->nombre[NOM_CAB - 1];
    t->nombre[NOM_CAB - 1] = 0;
    for (y = 0; *ok && y < REJ_FILAS; y++) for (x = 0; x < REJ_COLS - 1; x++) t->h[y][x] = (unsigned char)r8(f, ok);
    for (y = 0; *ok && y < REJ_FILAS - 1; y++) for (x = 0; x < REJ_COLS; x++) t->v[y][x] = (unsigned char)r8(f, ok);
}

static int elemento_en_rango(const Elemento *e)
{
    return e->tipo < EL_N && e->xa < REJ_COLS && e->xb < REJ_COLS &&
           e->ya < REJ_FILAS && e->yb < REJ_FILAS && e->xc < REJ_COLS && e->yc < REJ_FILAS;
}

/* Lee un circuito CESP de un archivo abierto (no lo cierra: detrás puede
 * venir la cola de la sesión). 1 si es válido; entonces, y solo entonces,
 * lo copia en c */
static int lee(FILE *f, Circuito *c)
{
    static Circuito t;             /* se valida entero antes de tocar c */
    static unsigned char buf[256];
    char mg[4], nom24 = 0;
    int x, y, k, i, ok = 1, max = SEG_EL0;

    circ_init(&t, "");
    if (fread(mg, 1, 4, f) != 4 || memcmp(mg, MAGIA, 4) != 0) ok = 0;
    if (ok) {                     /* una versión más nueva no se lee a medias */
        int ver = r8(f, &ok);
        if (ver < VERSION_MIN || ver > VERSION_MAX) ok = 0;
    }
    lee_rejilla(f, &t, &ok, &nom24);

    if (ok) {                                      /* bloque global */
        Bloque b;
        if (lee_bloque(f, buf, GLOBAL_BYTES, &b, &ok)) {
            t.tierra_x = (signed char)b8(&b); t.tierra_y = (signed char)b8(&b);
            t.term_x[0] = (signed char)b8(&b); t.term_y[0] = (signed char)b8(&b);
            t.term_x[1] = (signed char)b8(&b); t.term_y[1] = (signed char)b8(&b);
            t.conv = (unsigned char)b8(&b);
            bmag3(&b, &t.frec);
            if (b.n - b.i >= GLOBAL2_BYTES) {         /* sección 2: sondas, tiempo y paso */
                bsonda(&b, &t.sonda[0]); bsonda(&b, &t.sonda[1]);
                bmagc(&b, &t.t_fin); bmagc(&b, &t.paso);
                if (b.n - b.i >= GLOBAL3_BYTES) {     /* sección 3: la cola del nombre */
                    t.nombre[NOM_CAB - 1] = nom24;
                    for (i = NOM_CAB; i < NOM_CAB + GLOBAL3_BYTES; i++) t.nombre[i] = (char)b8(&b);
                    t.nombre[sizeof t.nombre - 1] = 0;
                }
            }
        }
        if (!b.ok) ok = 0;
    }

    if (ok) t.nel = r8(f, &ok);
    if (t.nel > MAX_EL) ok = 0;
    for (k = 0; ok && k < t.nel; k++) {
        Elemento *e = &t.el[k];
        Bloque b;
        if (lee_bloque(f, buf, ELEM_BYTES, &b, &ok)) {
            e->tipo = (unsigned char)b8(&b);
            e->xa = (unsigned char)b8(&b); e->ya = (unsigned char)b8(&b);
            e->xb = (unsigned char)b8(&b); e->yb = (unsigned char)b8(&b);
            for (i = 0; i < 6; i++) e->etiq[i] = (char)b8(&b);
            bmag3(&b, &e->val); bmag3(&b, &e->V); bmag3(&b, &e->I); bmag3(&b, &e->aux);
            e->variante = (unsigned char)b8(&b);
            e->ref = (signed char)b8(&b);
            if (b.n - b.i >= ELEM2_BYTES)               /* sección 2: parámetros */
                for (i = 0; i < NPAR; i++) bmagc(&b, &e->par[i]);
            /* sección 3: la pata c, solo en los de 3 patas (en los demás,
             * lo que sobra es de una versión futura y no se lee) */
            if (e->tipo < EL_N && tipo_info(e->tipo)->huella && b.n - b.i >= ELEM3_BYTES) {
                e->xc = (unsigned char)b8(&b); e->yc = (unsigned char)b8(&b);
            }
        }
        if (!b.ok) ok = 0;
        e->etiq[5] = 0;
        if (!elemento_en_rango(e)) ok = 0;
    }

    /* Cada segmento tiene que apuntar a un elemento que exista, y cada
     * elemento estar en su segmento: un archivo tocado a mano o cortado no
     * puede colar un circuito incoherente. */
    for (y = 0; ok && y < REJ_FILAS; y++)
        for (x = 0; x < REJ_COLS; x++) {
            if (x < REJ_COLS - 1 && t.h[y][x] >= max + t.nel) ok = 0;
            if (y < REJ_FILAS - 1 && t.v[y][x] >= max + t.nel) ok = 0;
        }
    if (!ok || !circ_valido(&t)) return 0;
    *c = t;
    return 1;
}

int arch_cargar(int hueco, Circuito *c)
{
    char ruta[240];
    FILE *f;
    int ok;

    if (hueco < 0 || hueco >= ARCH_HUECOS) return 0;
    nombre(hueco, ruta, sizeof ruta);
    f = fopen(ruta, "rb");
    if (!f) return 0;
    ok = lee(f, c);
    fclose(f);
    return ok;
}

int arch_borrar(int hueco)
{
    char ruta[240];
    if (hueco < 0 || hueco >= ARCH_HUECOS) return 0;
    nombre(hueco, ruta, sizeof ruta);
    return quita(ruta);
}

int arch_libre(void)
{
    int i;
    for (i = 0; i < ARCH_HUECOS; i++) if (!arch_existe(i)) return i;
    return -1;
}

/* ---- La sesión, en ces_sesion.tns: el circuito del editor en CESP (el
 *      mismo escritor y el mismo lector que un problema, con circ_valido) y
 *      detrás una cola de 8 bytes: 'S', hueco + 1 (0 = ninguno), el cursor
 *      (x, y) y la huella del archivo de ese hueco al escribir la sesión (4
 *      bytes, little-endian). Al leerla, el hueco vale solo si su archivo
 *      sigue teniendo esa huella: si otro problema ocupó el hueco (se borró
 *      y se guardó otro, o se copió otro con TI Connect), Guardar no lo pisa
 *      y va a un hueco nuevo. Un lector de problemas no mira lo que va detrás
 *      del último elemento, así que la cola no cambia el formato. Sin la
 *      cola (o con otra), el circuito vale igual: hueco -1 y el cursor en
 *      (0, 0). ---- */
#define SES_COLA 'S'

static void ruta_sesion(char *buf, size_t cap) { ruta_fija("/ces_sesion.tns", buf, cap); }

/* La huella del problema de un hueco: FNV-1a de 32 bits de todos sus bytes.
 * 0 si el hueco no tiene archivo o no se puede leer entero */
static int huella(int hueco, unsigned long *h)
{
    char ruta[240];
    FILE *f;
    unsigned long x = 2166136261UL;
    int c;
    if (hueco < 0 || hueco >= ARCH_HUECOS) return 0;
    nombre(hueco, ruta, sizeof ruta);
    f = fopen(ruta, "rb");
    if (!f) return 0;
    while ((c = fgetc(f)) != EOF) x = ((x ^ (unsigned long)c) * 16777619UL) & 0xFFFFFFFFUL;
    c = ferror(f);
    fclose(f);
    *h = x;
    return !c;
}

int arch_sesion_guardar(const Circuito *c, int hueco, int cx, int cy)
{
    char ruta[240];
    FILE *f;
    unsigned long h = 0;
    int i;
    if (!huella(hueco, &h)) hueco = -1;            /* sin su archivo, ninguno */
    ruta_sesion(ruta, sizeof ruta);
    f = abre_w(ruta);
    if (!f) return 0;
    escribe(f, c);
    w8(f, SES_COLA);
    w8(f, hueco + 1);
    w8(f, cx); w8(f, cy);
    for (i = 0; i < 4; i++) w8(f, (int)((h >> (8 * i)) & 0xFF));
    return cierra_w(f);
}

int arch_sesion_cargar(Circuito *c, int *hueco, int *cx, int *cy)
{
    char ruta[240];
    FILE *f;
    int ok, h, x, y, i;
    *hueco = -1; *cx = 0; *cy = 0;
    ruta_sesion(ruta, sizeof ruta);
    f = fopen(ruta, "rb");
    if (!f) return 0;
    ok = lee(f, c);
    if (ok) {
        int bien = 1;
        unsigned long antes = 0, ahora;
        if (r8(f, &bien) == SES_COLA && bien) {
            h = r8(f, &bien); x = r8(f, &bien); y = r8(f, &bien);
            if (bien && x < REJ_COLS && y < REJ_FILAS) { *cx = x; *cy = y; }
            for (i = 0; i < 4; i++) antes |= (unsigned long)r8(f, &bien) << (8 * i);
            /* el hueco, si su archivo es el mismo que cuando se escribió la sesión */
            if (bien && h >= 1 && h <= ARCH_HUECOS && huella(h - 1, &ahora) && ahora == antes) *hueco = h - 1;
        }
    }
    fclose(f);
    return ok;
}

int arch_sesion_borrar(void)
{
    char ruta[240];
    ruta_sesion(ruta, sizeof ruta);
    return quita(ruta);
}

#if ESPECIAL
#include "archivo_esp.inc"          /* la versión especial (no se publica) */
#endif

/* ---- Archivos de bytes de nombre fijo: el panel Controles y el grabador.
 *      Su formato lo decide quien los escribe (ui.c y entrada.c). ---- */
int arch_bytes_guardar(const char *nombre, const unsigned char *d, int n)
{
    char ruta[240];
    FILE *f;
    ruta_fija(nombre, ruta, sizeof ruta);
    if (!ruta[0] || n < 0) return 0;
    f = abre_w(ruta);
    if (!f) return 0;
    if (n > 0 && fwrite(d, 1, (size_t)n, f) != (size_t)n) { cierra_w(f); return 0; }
    return cierra_w(f);
}

int arch_bytes_leer(const char *nombre, unsigned char *d, int cap)
{
    char ruta[240];
    FILE *f;
    size_t n;
    ruta_fija(nombre, ruta, sizeof ruta);
    if (!ruta[0] || cap < 0) return -1;
    f = fopen(ruta, "rb");
    if (!f) return -1;
    n = fread(d, 1, (size_t)cap, f);
    fclose(f);
    return (int)n;
}

int arch_bytes_borrar(const char *nombre)
{
    char ruta[240];
    ruta_fija(nombre, ruta, sizeof ruta);
    return ruta[0] && quita(ruta);
}
