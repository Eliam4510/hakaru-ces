/* ============================================================================
 *  numfmt.c  -  Numeros a texto y construccion de cadenas, sin printf
 *
 *  Copiado de EstaticaCX (commit eca703d). Anade la lectura con exponente y
 *  prefijo, las cifras significativas y el rotulo corto del esquema (al
 *  final del archivo). El tope de decimales sube de 6 a 9 para las
 *  corrientes de microamperios.
 * ========================================================================= */
#include <math.h>
#include <string.h>
#include "numfmt.h"

#ifndef NDLESS
/* Para el arnés: 1 = pow, log10 y las cuentas de 64 bits en cada número,
 * como antes (test_opt_otros compara las dos maneras) */
static int lento;
void num_lento(int si) { lento = si; }
#else
#define lento 0
#endif

/* --------------------------------------------------------------------------
 *  Potencias de 10 y décadas, sin pow ni log10 en cada número
 * ----------------------------------------------------------------------- */
#define P10_MIN (-40)
#define P10_MAX 40
static double p10[P10_MAX - P10_MIN + 1];
static unsigned char p10_hecha[P10_MAX - P10_MIN + 1];

/* 10^k tal como lo da pow(10, k) de la libm, memorizado: la primera vez se
 * le pide a pow. NUNCA con literales: la newlib del SDK no redondea como el
 * compilador (en 12 de 81 potencias difiere en un ulp, rev9/perf_plan/
 * pow10.c) y los números cambiarían en la calculadora sin que el PC lo viera */
static double pot10(int k)
{
    int i = k - P10_MIN;
    if (lento || k < P10_MIN || k > P10_MAX) return pow(10.0, (double)k);
    if (!p10_hecha[i]) { p10[i] = pow(10.0, (double)k); p10_hecha[i] = 1; }
    return p10[i];
}

static unsigned long long bits_de(double x)
{
    unsigned long long u;
    memcpy(&u, &x, sizeof u);
    return u;
}

/* floor(log10(a)), exactamente lo que da la libm. Un double positivo se
 * ordena como sus bits: con los de las potencias de pot10 sale la década sin
 * log10. Solo a menos de 2^14 ulp (1,8e-12 relativo) de una potencia se le
 * pregunta a log10, que es quien decide ahí; lejos, log10(a) está a más de
 * 7,8e-13 de un entero y su error es de pocos ulp (7e-15 hasta 1e37): el
 * floor no puede cambiar. Con 0, negativos, NaN, Inf, subnormales o fuera de
 * 2^±120, log10 */
#define DEC_MARGEN 16384ull
static int dec_de(double a)
{
    unsigned long long u = bits_de(a);
    int e2 = (int)(u >> 52) - 1023, e;
    if (lento || e2 < -120 || e2 > 120) return (int)floor(log10(a));
    /* floor(e2·log10 2): la década de a o la de debajo (entre 2^±120 nunca
     * la de encima: comprobado exponente a exponente) */
    e = e2 >= 0 ? (e2 * 1233) >> 12 : -((-e2 * 1233 + 4095) >> 12);
    if (u >= bits_de(pot10(e + 1))) e++;
    if (u < bits_de(pot10(e)) + DEC_MARGEN || u > bits_de(pot10(e + 1)) - DEC_MARGEN)
        return (int)floor(log10(a));
    return e;
}

#ifndef NDLESS
int num_decada(double a) { return dec_de(a); }
#endif

/* floor(log10(a) / 3.0) * 3 con el tope de los prefijos, de -12 (p) a 9
 * (G), a partir de e = floor(log10(a)): dividir entre 3 en coma flotante no
 * pasa nunca de un múltiplo de 3 (ni por redondeo), así que da floor(e/3) */
static int e3_de(int e)
{
    static const signed char E3[21] = { -12, -12, -12, -9, -9, -9, -6, -6, -6, -3, -3, -3,
                                          0, 0, 0, 3, 3, 3, 6, 6, 6 };
    return e < -12 ? -12 : e > 8 ? 9 : E3[e + 12];
}

/* El múltiplo de 1000 de a > 0 para un prefijo del SI, de -12 (p) a 9 (G) */
static int e3_pref(double a)
{
    int e3;
    if (!lento) return e3_de(dec_de(a));
    e3 = (int)floor(log10(a) / 3.0) * 3;
    return e3 < -12 ? -12 : e3 > 9 ? 9 : e3;
}

static void copia(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (!cap) return;
    while (src[i] && i < cap - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void invierte(char *s, int n)
{
    int i;
    for (i = 0; i < n / 2; i++) {
        char t = s[i]; s[i] = s[n - 1 - i]; s[n - 1 - i] = t;
    }
}

static char *formatea(char *buf, size_t cap, double v, int dec, int recorta)
{
    /* 10^dec exacto (enteros de hasta 1e9: el literal es exacto, como el
     * (double) del entero de antes) */
    static const double DIEZ_A[10] = { 1.0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9 };
    char tmp[48];
    int n = 0, neg = 0, i, c32 = 0;
    long long esc = 1, ent, fra, val;
    unsigned int u = 0;
    double y;

    if (!cap) return buf;
    if (cap < 2) { buf[0] = 0; return buf; }

    if (dec < 0) dec = 0;
    if (dec > 9) dec = 9;

    /* NaN es el unico valor que no es igual a si mismo */
    if (v != v) { copia(buf, cap, "?"); return buf; }
    if (v < 0) { neg = 1; v = -v; }
    if (v > 1e15) { copia(buf, cap, neg ? "-inf" : "inf"); return buf; }

    y = v * DIEZ_A[dec] + 0.5;
    if (!lento && y < 4294967296.0) {
        /* Casi siempre cabe en 32 bits: sin las divisiones de 64 (en la
         * calculadora, una llamada a la biblioteca por cifra). Los decimales
         * son las dec cifras bajas del entero; lo que queda, la parte entera */
        u = (unsigned int)y;
        if (u == 0) neg = 0;                  /* sin «-0,00»: ver abajo */
        for (i = 0; i < dec; i++) { tmp[n++] = (char)('0' + (int)(u % 10u)); u /= 10u; }
        c32 = 1;
        ent = 0;
    } else {
        for (i = 0; i < dec; i++) esc *= 10;
        val = (long long)y;

        /* Un valor que redondea a cero no lleva signo. "-0,00" no significa nada
         * y en una reaccion sugiere un sentido que no existe. */
        if (val == 0) neg = 0;

        ent = val / esc;
        fra = val % esc;

        /* Decimales, del menos significativo al mas: luego se invierte todo. */
        for (i = 0; i < dec; i++) { tmp[n++] = (char)('0' + (int)(fra % 10)); fra /= 10; }
    }

    if (recorta) {
        while (n > 0 && tmp[0] == '0') {          /* ceros finales del decimal */
            int k;
            for (k = 0; k < n - 1; k++) tmp[k] = tmp[k + 1];
            n--;
        }
    }
    if (n > 0) tmp[n++] = ',';

    if (c32) {
        if (u == 0) tmp[n++] = '0';
        while (u > 0 && n < 44) { tmp[n++] = (char)('0' + (int)(u % 10u)); u /= 10u; }
    } else {
        if (ent == 0) tmp[n++] = '0';
        while (ent > 0 && n < 44) { tmp[n++] = (char)('0' + (int)(ent % 10)); ent /= 10; }
    }
    if (neg) tmp[n++] = '-';

    tmp[n] = 0;
    invierte(tmp, n);
    copia(buf, cap, tmp);
    return buf;
}

char *num_txt(char *buf, size_t cap, double v, int dec)
{ return formatea(buf, cap, v, dec, 0); }

char *num_auto(char *buf, size_t cap, double v, int dec)
{ return formatea(buf, cap, v, dec, 1); }

/* --------------------------------------------------------------------------
 *  Buf
 * ----------------------------------------------------------------------- */
void buf_init(Buf *b, char *dst, size_t cap)
{
    b->p = dst; b->cap = cap; b->n = 0; b->lleno = 0;
    if (cap) dst[0] = 0;
}

void buf_ch(Buf *b, char c)
{
    if (!b->cap || b->n + 1 >= b->cap) { b->lleno = 1; return; }
    b->p[b->n++] = c;
    b->p[b->n] = 0;
}

void buf_str(Buf *b, const char *s)
{
    if (!s) return;
    while (*s) {
        if (b->n + 1 >= b->cap) { b->lleno = 1; return; }
        b->p[b->n++] = *s++;
    }
    if (b->cap) b->p[b->n] = 0;
}

void buf_num(Buf *b, double v, int dec)
{
    char t[48];
    buf_str(b, num_txt(t, sizeof t, v, dec));
}

void buf_auto(Buf *b, double v, int dec)
{
    char t[48];
    buf_str(b, num_auto(t, sizeof t, v, dec));
}

void buf_int(Buf *b, long v)
{
    char t[24];
    int n = 0, neg = 0, i;
    unsigned long u;

    if (v < 0) { neg = 1; u = (unsigned long)(-(v + 1)) + 1u; }
    else        u = (unsigned long)v;

    if (u == 0) t[n++] = '0';
    while (u > 0 && n < 20) { t[n++] = (char)('0' + (int)(u % 10)); u /= 10; }
    if (neg) t[n++] = '-';
    t[n] = 0;
    for (i = 0; i < n / 2; i++) { char c = t[i]; t[i] = t[n-1-i]; t[n-1-i] = c; }
    buf_str(b, t);
}

/* --------------------------------------------------------------------------
 *  Lectura
 * ----------------------------------------------------------------------- */
double num_lee(const char *s)
{
    double ent = 0.0, fra = 0.0, div = 1.0;
    int neg = 0;

    if (!s) return 0.0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    while (*s >= '0' && *s <= '9') { ent = ent * 10.0 + (*s - '0'); s++; }

    if (*s == ',' || *s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            div *= 10.0;
            fra = fra * 10.0 + (*s - '0');
            s++;
        }
    }
    ent += fra / div;
    return neg ? -ent : ent;
}

/* --------------------------------------------------------------------------
 *  Propio de este programa
 * ----------------------------------------------------------------------- */
/* El número sin signo que empieza en s: cifras, coma (o punto), exponente y
 * un prefijo del SI. Devuelve el puntero detrás de él (y el valor en *pv), o
 * 0 si ahí no hay un número (ni una cifra, o una E sin cifras). Lo usan
 * num_lee_ing y las cuentas (num_eval): el mismo número da el mismo double */
static const char *lee_num(const char *s, double *pv)
{
    double v = 0.0, div = 1.0;
    int dig = 0, e = 0, eneg = 0, edig = 0;

    while (*s >= '0' && *s <= '9') { v = v * 10.0 + (*s - '0'); s++; dig++; }
    if (*s == ',' || *s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            div *= 10.0;
            v += (*s - '0') / div;
            s++; dig++;
        }
    }
    if (!dig) return 0;

    if (*s == 'E' || *s == 'e') {
        s++;
        if (*s == '-') { eneg = 1; s++; }
        else if (*s == '+') s++;
        while (*s >= '0' && *s <= '9') { if (e < 300) e = e * 10 + (*s - '0'); s++; edig++; }
        if (!edig) return 0;
        while (e-- > 0) v = eneg ? v / 10.0 : v * 10.0;
    }
    /* Prefijos del SI: p n µ m k M G (la µ es la de Latin-1, 0xB5) */
    switch (*s) {
    case 'p': v *= 1e-12; s++; break;
    case 'n': v *= 1e-9;  s++; break;
    case 'u': case (char)0xB5: v *= 1e-6; s++; break;
    case 'm': v *= 1e-3;  s++; break;
    case 'k': case 'K': v *= 1e3; s++; break;
    case 'M': v *= 1e6;   s++; break;
    case 'G': v *= 1e9;   s++; break;
    default: break;
    }
    *pv = v;
    return s;
}

double num_lee_ing(const char *s, int *ok)
{
    double v = 0.0;
    int neg = 0;

    if (ok) *ok = 0;
    if (!s) return 0.0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;

    s = lee_num(s, &v);
    if (!s) return 0.0;
    while (*s == ' ') s++;
    if (*s) return 0.0;                      /* sobra algo: no es un numero */

    if (ok) *ok = 1;
    return neg ? -v : v;
}

/* --------------------------------------------------------------------------
 *  Cuentas (la ficha): descenso recursivo, con la precedencia de siempre
 *
 *      suma     = producto { (+ | -) producto }
 *      producto = unario { (× | ÷) unario }          (también * y /)
 *      unario   = - unario | potencia
 *      potencia = primario [ ^ unario ]              (a la derecha: 2^3^2 = 2^9)
 *      primario = número | ( suma )
 *
 *  Así -2^2 = -4 y 2^-1 = 0,5. Nada de coma flotante nueva: solo double.
 * ----------------------------------------------------------------------- */
#define ES_FINITO(x) ((x) - (x) == 0.0)      /* ni NaN ni ±Inf (como FINITO de circuito.h) */
#define EV_PROF 40                           /* paréntesis anidados, como mucho */

typedef struct { const char *p; int err, prof; } Ev;

static int es_por(char c)   { return c == '*' || c == (char)0xD7; }   /* × */
static int es_entre(char c) { return c == '/' || c == (char)0xF7; }   /* ÷ */
/* Lo que puede ir detrás de un número: un signo, un paréntesis o el final */
static int tras_num(char c)
{
    return c == 0 || c == '+' || c == '-' || c == '^' || c == '(' || c == ')' || es_por(c) || es_entre(c);
}

static double ev_suma(Ev *e);

static double ev_prim(Ev *e)
{
    double v;
    const char *q;
    char c = *e->p;
    if (c == '(') {
        if (++e->prof > EV_PROF) { e->err = NE_NUMERO; return 0.0; }
        e->p++;
        v = ev_suma(e);
        if (e->err) return 0.0;
        if (*e->p != ')') {                   /* el final: sin cerrar; «(2(3))», falta un signo */
            e->err = *e->p ? NE_SIGNO : NE_PAR_ABRE;
            return 0.0;
        }
        e->p++;
        e->prof--;
        return v;
    }
    if (c == 0 || c == ')' || c == '+' || c == '^' || es_por(c) || es_entre(c)) { e->err = NE_FALTA; return 0.0; }
    q = lee_num(e->p, &v);
    if (!q || !tras_num(*q)) { e->err = NE_NUMERO; return 0.0; }   /* «4,5E», «4kk» */
    if (!ES_FINITO(v)) { e->err = NE_GRANDE; return 0.0; }          /* «8E505» */
    e->p = q;
    return v;
}

/* Cada paso de la cuenta, finito: un infinito a medias no se esconde luego
 * («(1E300×1E300)^0» no es 1, ni «1÷(1E300×1E300)» es 0) */
static double finito(Ev *e, double v)
{
    if (!e->err && !ES_FINITO(v)) e->err = v != v ? NE_REAL : NE_GRANDE;
    return e->err ? 0.0 : v;
}

static double ev_unario(Ev *e);

static double ev_pot(Ev *e)
{
    double b = ev_prim(e), x, r;
    if (e->err || *e->p != '^') return b;
    e->p++;
    x = ev_unario(e);
    if (e->err) return 0.0;
    if (b == 0.0 && x < 0.0) { e->err = NE_CERO; return 0.0; }      /* 0^-1 = 1/0 */
    r = pow(b, x);
    if (r != r) { e->err = NE_REAL; return 0.0; }                     /* (-8)^0,5 */
    return finito(e, r);
}

static double ev_unario(Ev *e)
{
    if (*e->p == '-') { e->p++; return -ev_unario(e); }
    return ev_pot(e);
}

static double ev_prod(Ev *e)
{
    double v = ev_unario(e), w;
    while (!e->err && (es_por(*e->p) || es_entre(*e->p))) {
        int div = es_entre(*e->p);
        e->p++;
        w = ev_unario(e);
        if (e->err) return 0.0;
        if (div && w == 0.0) { e->err = NE_CERO; return 0.0; }
        v = finito(e, div ? v / w : v * w);
        if (e->err) return 0.0;
    }
    return v;
}

static double ev_suma(Ev *e)
{
    double v = ev_prod(e), w;
    while (!e->err && (*e->p == '+' || *e->p == '-')) {
        int mas = *e->p == '+';
        e->p++;
        w = ev_prod(e);
        if (e->err) return 0.0;
        v = finito(e, mas ? v + w : v - w);
        if (e->err) return 0.0;
    }
    return v;
}

int num_es_cuenta(const char *s)
{
    size_t i;
    if (!s) return 0;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (c == '^' || c == '(' || c == ')' || es_por(c) || es_entre(c)) return 1;
        /* un + o un - al principio o tras la E es el signo del número o del
         * exponente; en otro sitio, una suma o una resta */
        if ((c == '+' || c == '-') && i > 0 && s[i - 1] != 'E' && s[i - 1] != 'e') return 1;
    }
    return 0;
}

double num_eval(const char *s, int *err)
{
    Ev e;
    double v;
    int ok;
    *err = NE_OK;
    /* un número solo: exactamente lo de siempre (el mismo double y el mismo
     * «no es un número»); si no cabe en un double («8E505»), NE_GRANDE */
    v = num_lee_ing(s, &ok);
    if (ok && !ES_FINITO(v)) { *err = NE_GRANDE; return 0.0; }
    if (ok) return v;
    if (!num_es_cuenta(s)) { *err = NE_NUMERO; return 0.0; }
    e.p = s; e.err = NE_OK; e.prof = 0;
    v = ev_suma(&e);
    if (!e.err && *e.p) e.err = *e.p == ')' ? NE_PAR_CIERRA : NE_SIGNO;   /* «(1)(2)», «2)» */
    if (!e.err && !ES_FINITO(v)) e.err = v != v ? NE_REAL : NE_GRANDE;
    if (e.err) { *err = e.err; return 0.0; }
    return v == 0.0 ? 0.0 : v;                /* sin «-0» */
}

/* Decimales para cuatro cifras significativas, al menos dos, y sin pasarse
 * de lo que cabe en un long long al multiplicar por 10^dec. */
static int dec_sig(double v)
{
    double a = fabs(v);
    int e, dec;
    if (a == 0.0 || !(a < 1e300)) return 0;          /* 0, NaN o Inf */
    e = dec_de(a);
    dec = 4 - e;
    if (dec < 2) dec = 2;
    if (dec > 9) dec = 9;
    while (dec > 0 && a * pot10(dec) > 1e17) dec--;
    return dec;
}

int num_sig(char *buf, size_t cap, double v)
{
    int dec;
    double esc, r;
    if (fabs(v) >= 1e15 && fabs(v) < 1e300) {   /* no cabe en decimal: 1,5E21 */
        char t[40];
        Buf b;
        int e = dec_de(fabs(v));
        buf_init(&b, buf, cap);
        buf_str(&b, num_auto(t, sizeof t, v / pot10(e), 3));
        buf_ch(&b, 'E');
        buf_int(&b, e);
        return 0;
    }
    dec = dec_sig(v);
    esc = pot10(dec);
    num_auto(buf, cap, v, dec);
    r = floor(fabs(v) * esc + 0.5) / esc;
    return fabs(r - fabs(v)) <= 1e-9 * (fabs(v) > 1.0 ? fabs(v) : 1.0);
}

void buf_sig(Buf *b, double v)
{
    char t[48];
    num_sig(t, sizeof t, v);
    buf_str(b, t);
}

void buf_igual(Buf *b, double v)
{
    char t[48];
    int exacto = num_sig(t, sizeof t, v);
    buf_str(b, exacto ? " = " : " \x82 ");   /* \x82 = aproximado (gfx.h) */
    buf_str(b, t);
}

int num_fraccion(double v, long *p, long *q, long qmax)
{
    double x = fabs(v), resto;
    long h0 = 0, h1 = 1, k0 = 1, k1 = 0;        /* convergentes h/k */
    char t[48];
    int i;

    if (!(x > 0.0) || x > 1e9) return 0;
    if (num_sig(t, sizeof t, v)) return 0;      /* el decimal ya es exacto */
    resto = x;
    for (i = 0; i < 40; i++) {
        double a = floor(resto);
        long h2, k2;
        /* En la calculadora long es de 32 bits: se cuenta en double y solo se
         * aceptan fracciones de numerador corto, que son las que se leen. */
        if (a * (double)h1 + (double)h0 > 999999.0) break;
        if (a * (double)k1 + (double)k0 > (double)qmax) break;
        h2 = (long)a * h1 + h0;
        k2 = (long)a * k1 + k0;
        h0 = h1; h1 = h2; k0 = k1; k1 = k2;
        if (fabs((double)h1 / (double)k1 - x) <= 1e-10 * x) {
            if (k1 == 1) return 0;                  /* entero: sin fracción */
            *p = v < 0 ? -h1 : h1;
            *q = k1;
            return 1;
        }
        if (resto - a < 1e-12) break;
        resto = 1.0 / (resto - a);
    }
    return 0;
}

char *num_corto(char *buf, size_t cap, double v)
{
    double a = fabs(v);
    const char *pre = "";
    int e, dec;
    char t[40];
    Buf b;

    /* 1000 ohm se lee mejor como 1k, y cabe mejor entre dos columnas */
    if (a >= 1e9)      { v /= 1e9; pre = "G"; }
    else if (a >= 1e6) { v /= 1e6; pre = "M"; }
    else if (a >= 1e3) { v /= 1e3; pre = "k"; }
    /* y por debajo de 0,01 los ceros no se leen: 5m, 43n */
    else if (a > 0.0 && a < 1e-2) {
        if (a >= 1e-3)      { v *= 1e3; pre = "m"; }
        else if (a >= 1e-6) { v *= 1e6; pre = "\xB5"; }   /* micro (S_MU en gfx.h) */
        else                { v *= 1e9; pre = "n"; }
    }
    a = fabs(v);
    if (a == 0.0) dec = 0;
    else {
        e = dec_de(a);
        dec = 2 - e;
        if (dec < 0) dec = 0;
        if (dec > 6) dec = 6;
    }
    num_auto(t, sizeof t, v, dec);
    buf_init(&b, buf, cap);
    buf_str(&b, t);
    buf_str(&b, pre);
    return buf;
}

/* Cifras significativas con prefijo del SI, de p a G: "6,32V", "-500mA",
 * "1,59kHz". Si la unidad empieza por un espacio, el espacio va antes del
 * prefijo: " s" da "2 ms". */
char *num_pref(char *buf, size_t cap, double v, int cifras, const char *unidad)
{
    static const char *const PRE[8] = { "p", "n", "\xB5", "m", "", "k", "M", "G" };
    double a = fabs(v), m;
    int e3 = 0, dec, ent;
    char t[40];
    Buf b;

    buf_init(&b, buf, cap);
    if (cifras < 1) cifras = 1;
    if (cifras > 9) cifras = 9;
    if (!(a < 1e300)) { buf_str(&b, "?"); return buf; }       /* NaN o infinito */
    if (a > 0.0) e3 = e3_pref(a);
    m = v / pot10(e3);
    /* redondeo a las cifras pedidas; 999,6 puede pasar a 1000 */
    ent = fabs(m) >= 1.0 ? dec_de(fabs(m)) + 1 : 1;
    dec = cifras - ent;
    if (dec < 0) dec = 0;
    if (fabs(m) * pot10(dec) + 0.5 >= 1000.0 * pot10(dec) && e3 < 9) {
        e3 += 3; m /= 1000.0;
        dec = cifras - 1;
    }
    num_auto(t, sizeof t, m, dec);
    buf_str(&b, t);
    if (unidad && unidad[0] == ' ') { buf_ch(&b, ' '); unidad++; }
    buf_str(&b, PRE[e3 / 3 + 4]);
    if (unidad) buf_str(&b, unidad);
    return buf;
}

/* --------------------------------------------------------------------------
 *  Fasores (Etapa 3)
 * ----------------------------------------------------------------------- */
double num_ang(double g)
{
    if (!(fabs(g) < 1e300)) return g;
    g = fmod(g, 360.0);                          /* (-360, 360) */
    if (g > 180.0 + 1e-12) g -= 360.0;
    else if (g <= -180.0 + 1e-12) g += 360.0;    /* -180 sale 180 */
    return g;
}

/* "30+j40", "30-j40", "j40", "1,5k+j2m", "4,7E3-j1E-3", o un real solo. La
 * parte imaginaria no lleva signo propio detrás de la j, y la real no puede
 * faltar si hay signo ("+j40" no vale). */
double num_lee_cx(const char *s, double *im, int *ok)
{
    char t[48];
    double re = 0.0, b;
    int p, k1, k2, i;

    if (ok) *ok = 0;
    if (im) *im = 0.0;
    if (!s) return 0.0;
    for (p = 0; s[p] && s[p] != 'j'; p++) {}
    if (!s[p]) return num_lee_ing(s, ok);               /* sin j: un real */
    if (!s[p + 1] || s[p + 1] == '+' || s[p + 1] == '-') return 0.0;
    b = num_lee_ing(s + p + 1, &k2);
    if (!k2) return 0.0;
    if (p > 0) {
        char sg = s[p - 1];
        if (sg != '+' && sg != '-') return 0.0;
        if (p - 1 == 0 || p - 1 >= (int)sizeof t) return 0.0;  /* "+j40": falta la real */
        for (i = 0; i < p - 1; i++) t[i] = s[i];
        t[p - 1] = 0;
        re = num_lee_ing(t, &k1);
        if (!k1) return 0.0;
        if (sg == '-') b = -b;
    }
    if (im) *im = b;
    if (ok) *ok = 1;
    return re;
}

static const char *const PRE_F[8] = { "p", "n", "\xB5", "m", "", "k", "M", "G" };

/* Múltiplo de 1000 (e3, de -12 a 9), mantisa y decimales fijos para que a >= 0
 * salga con 'cifras' significativas, como num_pref (999,96 pasa a 1,000k) */
static int pref_de(double a, int cifras, double *m, int *dec)
{
    int e3 = 0, ent;
    if (a > 0.0 && a < 1e300) e3 = e3_pref(a);
    *m = a / pot10(e3);
    ent = (*m >= 1.0 && *m < 1e300) ? dec_de(*m) + 1 : 1;
    *dec = cifras - ent;
    if (*dec < 0) *dec = 0;
    if (*m * pot10(*dec) + 0.5 >= 1000.0 * pot10(*dec) && e3 < 9) {
        e3 += 3; *m /= 1000.0;
        *dec = cifras - 1;
    }
    return e3;
}

static void fasor_de(Buf *b, double mod, double grados, int rect, int corto, int cifras, const char *unidad)
{
    char t[48];
    double m;
    int dec, e3;

    mod = fabs(mod);
    if (!rect) {
        double g = num_ang(grados);
        e3 = pref_de(mod, cifras, &m, &dec);
        buf_str(b, num_txt(t, sizeof t, m, dec));
        buf_str(b, "\x7F");                                  /* ∠ (S_ANG) */
        if (!corto && fabs(g) < 99.95) buf_str(b, num_txt(t, sizeof t, g, 1));
        else buf_str(b, num_txt(t, sizeof t, g, 0));
        buf_str(b, "\xB0");                                  /* ° (S_DEG) */
    } else {
        double rad = grados * 3.14159265358979323846 / 180.0;
        double re = mod * cos(rad), im = mod * sin(rad), esc, mr, mi;
        e3 = pref_de(fabs(re) > fabs(im) ? fabs(re) : fabs(im), cifras, &m, &dec);
        esc = pot10(e3);
        mr = re / esc; mi = im / esc;
        buf_str(b, num_txt(t, sizeof t, mr, dec));
        /* el signo de la j: el de lo que se escribe (-0,001 con 2 decimales es +j0,00) */
        if (mi < 0.0 && floor(-mi * pot10(dec) + 0.5) > 0.0) buf_str(b, "-j");
        else buf_str(b, "+j");
        buf_str(b, num_txt(t, sizeof t, fabs(mi), dec));
    }
    if (unidad && (unidad[0] || PRE_F[e3 / 3 + 4][0])) {
        if (!corto) buf_ch(b, ' ');
        buf_str(b, PRE_F[e3 / 3 + 4]);
        buf_str(b, unidad);
    }
}

/* Fasor en el convenio: polar "84,85∠-45,0° V" o rectangular "7,55+j5,80 A"
 * (con corto, "9,52∠38°A" y "7,55+j5,80A"). Si no cabe en 15 columnas (12
 * con corto), una cifra menos. */
char *num_fasor(char *buf, size_t cap, double mod, double grados, int rect, int corto, const char *unidad)
{
    int cifras = corto ? 3 : (rect ? 3 : 4), tope = corto ? 12 : 15;
    Buf b;
    for (;;) {
        buf_init(&b, buf, cap);
        fasor_de(&b, mod, grados, rect, corto, cifras, unidad);
        if ((int)b.n <= tope || cifras <= 1) break;
        cifras--;
    }
    return buf;
}

char *num_pot(char *buf, size_t cap, double v, const char *uni)
{
    double a = fabs(v);
    char t[40], u[12];
    Buf b;
    if (a >= 999.5 && a < 9999.5) {
        buf_init(&b, buf, cap);
        buf_str(&b, num_txt(t, sizeof t, v, 0));
        buf_ch(&b, ' '); buf_str(&b, uni);
        return buf;
    }
    buf_init(&b, u, sizeof u);
    buf_ch(&b, ' '); buf_str(&b, uni);
    return num_pref(buf, cap, v, 4, u);
}
