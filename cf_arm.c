/* ============================================================================
 *  cf_arm.c  -  Coma flotante rápida de la calculadora: la división y la raíz
 *  cuadrada de double y sus comparaciones, con el resultado exacto de IEEE
 *  754 (el mismo que la libgcc y la newlib del SDK, bit a bit)
 *
 *  Solo para el ARM. El Makefile la enlaza con --wrap (CF_WRAP): todas las
 *  llamadas a esas rutinas, las del programa y las de la libm (exp, pow,
 *  hypot, sqrt...), pasan por las de aquí. El PC no la usa: su coma flotante
 *  es la del procesador.
 *
 *  En la CX II la división de libgcc va bit a bit (804 ciclos, el 13 % de un
 *  paso del emulador), la raíz de newlib también (2129) y cada comparación
 *  son tres llamadas anidadas (60-90 ciclos). Aquí:
 *  - división: el recíproco del divisor en 32 bits (tabla y dos pasos de
 *    Newton), el cociente en dos trozos de 26 bits, cada uno con su resto
 *    exacto, y el redondeo al par más cercano con el resto final; entre una
 *    potencia de 2 (x/2, las escalas del osciloscopio), solo el exponente,
 *    como el atajo de libgcc (sin él, esas saldrían 5 veces más caras);
 *  - raíz: 1/√m (tabla y dos pasos de Newton), √m con un paso más y la raíz
 *    entera con su resto exacto, que decide el redondeo;
 *  - comparaciones: con los bits, en una sola llamada.
 *  Las estimaciones pueden fallar en una o dos unidades: el resto exacto las
 *  corrige, así que el resultado es exacto por construcción. Lo raro de la
 *  división (0, subnormales, Inf, NaN, un cociente que no sale normal) va a
 *  la de libgcc (__real___aeabi_ddiv); el de la raíz (NaN, Inf, ±0 y los
 *  negativos) se hace como en newlib, con las mismas rutinas de libgcc: los
 *  mismos bits, también los de cada NaN. La raíz no llama a la de newlib: en
 *  el .tns, la libSDL de Ndless (que va antes que la libm al enlazar) tiene
 *  otra __ieee754_sqrt, y pedirla se traería esa y 190 KB de SDL.
 *
 *  Sin double en todo el archivo: cualquier cuenta de coma flotante aquí
 *  llamaría a las rutinas que este archivo sustituye. Un double llega (y
 *  sale) en dos registros, la palabra baja primero (AAPCS sin FPU).
 * ========================================================================= */
#include <stdint.h>

uint64_t __real___aeabi_ddiv(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);
/* las de libgcc que no se envuelven (lo raro de la raíz, como en newlib) */
uint64_t __aeabi_dmul(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);
uint64_t __aeabi_dadd(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);
uint64_t __aeabi_dsub(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);
uint64_t __wrap___aeabi_ddiv(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);
uint64_t __wrap___ieee754_sqrt(uint32_t xl, uint32_t xh);
int __wrap___aeabi_dcmpeq(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);
int __wrap___aeabi_dcmplt(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);
int __wrap___aeabi_dcmple(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);
int __wrap___aeabi_dcmpge(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);
int __wrap___aeabi_dcmpgt(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh);

/* --------------------------------------------------------------------------
 *  División
 * ----------------------------------------------------------------------- */
/* 2^16/(1 + (i + 1/2)/128): 1/D en el centro de cada 1/128 de [1, 2) */
static const uint16_t REC[128] = {
    0xFF01, 0xFD09, 0xFB19, 0xF930, 0xF74E, 0xF574, 0xF3A1, 0xF1D5, 0xF00F, 0xEE50, 0xEC98, 0xEAE5,
    0xE939, 0xE793, 0xE5F3, 0xE459, 0xE2C5, 0xE136, 0xDFAC, 0xDE28, 0xDCA9, 0xDB2F, 0xD9BA, 0xD84A,
    0xD6DF, 0xD579, 0xD417, 0xD2BA, 0xD161, 0xD00D, 0xCEBD, 0xCD71, 0xCC29, 0xCAE6, 0xC9A6, 0xC86A,
    0xC733, 0xC5FE, 0xC4CE, 0xC3A1, 0xC278, 0xC152, 0xC030, 0xBF11, 0xBDF6, 0xBCDD, 0xBBC8, 0xBAB6,
    0xB9A8, 0xB89C, 0xB793, 0xB68D, 0xB58A, 0xB48A, 0xB38D, 0xB292, 0xB19B, 0xB0A6, 0xAFB3, 0xAEC3,
    0xADD6, 0xACEB, 0xAC03, 0xAB1D, 0xAA39, 0xA958, 0xA879, 0xA79C, 0xA6C2, 0xA5EA, 0xA514, 0xA440,
    0xA36E, 0xA29F, 0xA1D1, 0xA106, 0xA03C, 0x9F74, 0x9EAF, 0x9DEB, 0x9D29, 0x9C69, 0x9BAB, 0x9AEE,
    0x9A34, 0x997B, 0x98C4, 0x980E, 0x975A, 0x96A8, 0x95F8, 0x9549, 0x949C, 0x93F0, 0x9346, 0x929D,
    0x91F6, 0x9150, 0x90AC, 0x9009, 0x8F68, 0x8EC8, 0x8E29, 0x8D8C, 0x8CF0, 0x8C56, 0x8BBC, 0x8B24,
    0x8A8E, 0x89F8, 0x8964, 0x88D2, 0x8840, 0x87AF, 0x8720, 0x8692, 0x8605, 0x8579, 0x84EF, 0x8465,
    0x83DD, 0x8356, 0x82CF, 0x824A, 0x81C6, 0x8143, 0x80C1, 0x8040,
};

/* x ≈ 2^32/D con D = d/2^31 en [1, 2): un paso de Newton, x·(2 − D·x). Da
 * siempre de menos (x·(2 − D·x) ≤ 1/D, y cada truncamiento resta): nunca
 * llega a 2^32 */
static uint32_t rec_paso(uint32_t d, uint32_t x)
{
    uint64_t t = 0u - (uint64_t)d * x;                    /* (2 − D·x)·2^63 */
    return (uint32_t)(((uint64_t)x * (uint32_t)(t >> 32)) >> 31);
}

/* Un cociente parcial q ≈ floor(N/B) y su resto R = N − q·B, que se sabe
 * pequeño (|R| < 2^62): se calcula en 64 bits, módulo 2^64, y se corrige q
 * hasta 0 <= R < B */
#define CORRIGE(q, R, B) do {                                          \
        while ((R) < 0) { (q)--; (R) += (int64_t)(B); }                \
        while ((R) >= (int64_t)(B)) { (q)++; (R) -= (int64_t)(B); }    \
    } while (0)

uint64_t __wrap___aeabi_ddiv(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh)
{
    uint32_t ea = (ah >> 20) & 0x7FFu, eb = (bh >> 20) & 0x7FFu, d, r, q;
    uint64_t A, B, Q;
    int64_t R;
    int32_t e;

    if (ea - 1u >= 0x7FEu || eb - 1u >= 0x7FEu)          /* 0, subnormal, Inf o NaN */
        return __real___aeabi_ddiv(al, ah, bl, bh);
    A = (uint64_t)((ah & 0xFFFFFu) | 0x100000u) << 32 | al;   /* [2^52, 2^53) */
    B = (uint64_t)((bh & 0xFFFFFu) | 0x100000u) << 32 | bl;
    e = (int32_t)ea - (int32_t)eb + 1023;
    if (((bh & 0xFFFFFu) | bl) == 0u) {                  /* B = ±2^k: solo el exponente */
        if (e <= 0 || e >= 0x7FF) return __real___aeabi_ddiv(al, ah, bl, bh);
        return (uint64_t)(((ah ^ bh) & 0x80000000u) | ((uint32_t)e << 20) | (ah & 0xFFFFFu)) << 32 | al;
    }
    if (A < B) { A <<= 1; e--; }                          /* A/B en [1, 2) */
    if (e <= 0 || e >= 0x7FF)                            /* subnormal, 0 o Inf */
        return __real___aeabi_ddiv(al, ah, bl, bh);

    /* r ≈ 2^63/d por debajo, d = los 32 bits altos de B (error < 2^-29) */
    d = (uint32_t)(B >> 21);
    r = rec_paso(d, rec_paso(d, (uint32_t)REC[(d >> 24) & 127u] << 16));
    /* Q = floor(A·2^52/B) en dos trozos de 26 bits: primero A·2^26/B (de
     * 2^26 a 2^27), luego R·2^26/B con su resto; cada estimación falla como
     * mucho en una unidad */
    q = (uint32_t)(((uint64_t)(uint32_t)(A >> 22) * r) >> 36);
    R = (int64_t)((A << 26) - (uint64_t)q * B);
    CORRIGE(q, R, B);
    Q = (uint64_t)q << 26;
    q = (uint32_t)(((uint64_t)(uint32_t)((uint64_t)R >> 21) * r) >> 37);
    R = (int64_t)(((uint64_t)R << 26) - (uint64_t)q * B);
    CORRIGE(q, R, B);
    Q += q;
    /* Al par más cercano: A/B = (Q + R/B)·2^-52. A medio camino exacto no
     * se llega en una división, pero la regla está */
    if ((uint64_t)R * 2u > B || ((uint64_t)R * 2u == B && (Q & 1u))) Q++;
    if (Q >> 53) {                                       /* 1,111...1 + 1 ulp = 2 */
        Q >>= 1;
        if (++e >= 0x7FF) return __real___aeabi_ddiv(al, ah, bl, bh);
    }
    return (uint64_t)(((ah ^ bh) & 0x80000000u) | ((uint32_t)e << 20) | ((uint32_t)(Q >> 32) & 0xFFFFFu)) << 32 |
           (uint32_t)Q;
}

/* --------------------------------------------------------------------------
 *  Raíz cuadrada (la de newlib, __ieee754_sqrt: también la de sqrt, hypot,
 *  pow, asin y acos)
 * ----------------------------------------------------------------------- */
/* 2^16/√m en el centro de cada trozo: m en [1, 2) en 64 de 1/64 (i < 64) y
 * en [2, 4) en 64 de 1/32 (i >= 64) */
static const uint16_t RSQ[128] = {
    0xFF01, 0xFD0D, 0xFB24, 0xF946, 0xF773, 0xF5A9, 0xF3EA, 0xF234, 0xF087, 0xEEE2, 0xED46, 0xEBB3,
    0xEA27, 0xE8A3, 0xE727, 0xE5B1, 0xE443, 0xE2DB, 0xE17A, 0xE020, 0xDECB, 0xDD7C, 0xDC34, 0xDAF1,
    0xD9B3, 0xD87B, 0xD748, 0xD61A, 0xD4F1, 0xD3CD, 0xD2AD, 0xD192, 0xD07B, 0xCF69, 0xCE5A, 0xCD50,
    0xCC4A, 0xCB48, 0xCA49, 0xC94F, 0xC858, 0xC764, 0xC674, 0xC587, 0xC49D, 0xC3B7, 0xC2D4, 0xC1F4,
    0xC116, 0xC03C, 0xBF65, 0xBE90, 0xBDBE, 0xBCEF, 0xBC23, 0xBB59, 0xBA91, 0xB9CC, 0xB90A, 0xB84A,
    0xB78C, 0xB6D0, 0xB617, 0xB560, 0xB451, 0xB2EF, 0xB196, 0xB044, 0xAEF9, 0xADB6, 0xAC79, 0xAB43,
    0xAA14, 0xA8EB, 0xA7C7, 0xA6AA, 0xA592, 0xA480, 0xA373, 0xA26B, 0xA168, 0xA069, 0x9F70, 0x9E7B,
    0x9D8A, 0x9C9D, 0x9BB5, 0x9AD0, 0x99F0, 0x9913, 0x983A, 0x9764, 0x9692, 0x95C4, 0x94F8, 0x9430,
    0x936B, 0x92A9, 0x91EA, 0x912E, 0x9074, 0x8FBE, 0x8F0A, 0x8E59, 0x8DAA, 0x8CFD, 0x8C54, 0x8BAC,
    0x8B07, 0x8A64, 0x89C3, 0x8925, 0x8889, 0x87EE, 0x8756, 0x86C0, 0x862B, 0x8599, 0x8508, 0x8479,
    0x83EC, 0x8361, 0x82D8, 0x8250, 0x81C9, 0x8145, 0x80C2, 0x8040,
};

/* y ≈ 2^31/√m (UQ1.31) con m = u/2^30: un paso de Newton, y·(3 − m·y²)/2 */
static uint32_t rsq_paso(uint32_t u, uint32_t y)
{
    uint32_t y2 = (uint32_t)(((uint64_t)y * y) >> 31);             /* y²·2^31 */
    uint64_t t = 0x6000000000000000ull - (uint64_t)u * y2;         /* (3 − m·y²)·2^61 */
    return (uint32_t)(((uint64_t)y * (uint32_t)(t >> 32)) >> 30);
}

#define PALABRAS(u) (uint32_t)(u), (uint32_t)((u) >> 32)

uint64_t __wrap___ieee754_sqrt(uint32_t xl, uint32_t xh)
{
    uint32_t u, y, s, ez;
    uint64_t M, S, t;
    int64_t D, R, dS;
    int32_t e;
    uint32_t impar;

    /* Lo raro, como __ieee754_sqrt de newlib (e_sqrt.c): NaN e Inf, x·x + x;
     * ±0, él mismo; un negativo, (x − x)/(x − x) (el NaN de 0/0) */
    if ((xh & 0x7FF00000u) == 0x7FF00000u) {
        t = __aeabi_dmul(xl, xh, xl, xh);
        return __aeabi_dadd(PALABRAS(t), xl, xh);
    }
    if ((int32_t)xh <= 0) {
        if (((xh & 0x7FFFFFFFu) | xl) == 0u) return (uint64_t)xh << 32 | xl;
        if ((int32_t)xh < 0) {
            t = __aeabi_dsub(xl, xh, xl, xh);
            return __real___aeabi_ddiv(PALABRAS(t), PALABRAS(t));
        }
    }
    /* m·2^52 con m en [1, 2) y el exponente sin sesgo; un subnormal se
     * normaliza (su raíz es normal) */
    M = (uint64_t)(xh & 0xFFFFFu) << 32 | xl;
    if (xh >> 20) { M |= 1ull << 52; e = (int32_t)(xh >> 20) - 1023; }
    else {
        int k = M >> 32 ? __builtin_clz((uint32_t)(M >> 32)) - 11 : __builtin_clz((uint32_t)M) + 21;
        M <<= k;
        e = -1022 - k;
    }
    u = (uint32_t)(M >> 46) & 63u;                        /* los 6 bits de la tabla */
    impar = (uint32_t)e & 1u;
    if (impar) M <<= 1;                                  /* m en [2, 4) */
    ez = (uint32_t)(e - (int32_t)impar + 2046) >> 1;      /* el de la raíz, con sesgo */

    /* 1/√m con 29 bits, √m con 29 bits (s = √m·2^31) */
    y = (uint32_t)RSQ[(impar << 6) | u] << 15;
    u = (uint32_t)(M >> 22);                              /* m·2^30 */
    y = rsq_paso(u, rsq_paso(u, y));
    t = ((uint64_t)u * y) >> 30;
    s = t > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)t;
    /* Un paso más con el resto exacto (m·2^62 − s², pequeño): S ≈ √(m·2^104)
     * con un error de pocas unidades; S = s·2^21 + D·2^20/s, con 1/s por y */
    D = (int64_t)((M << 10) - (uint64_t)s * s);
    if (D >= 0) dS = (int64_t)(((uint64_t)D * (y >> 8)) >> 34);
    else dS = -(int64_t)(((uint64_t)-D * (y >> 8)) >> 34);
    S = ((uint64_t)s << 21) + (uint64_t)dS;
    /* La raíz entera de N = m·2^104 con su resto R = N − S² (módulo 2^64:
     * pequeño), hasta 0 <= R <= 2S; y al más cercano: arriba si R > S (a
     * medio camino no se llega nunca: N sería un cuadrado más un cuarto) */
    R = (int64_t)((M << 52) - S * S);
    while (R < 0) { R += (int64_t)(2u * S - 1u); S--; }
    while (R > (int64_t)(2u * S)) { S++; R -= (int64_t)(2u * S - 1u); }
    if (R > (int64_t)S) S++;
    if (S >> 53) { S >>= 1; ez++; }                      /* 1,111...1 + 1 ulp = 2 */
    return (uint64_t)((ez << 20) | ((uint32_t)(S >> 32) & 0xFFFFFu)) << 32 | (uint32_t)S;
}

/* --------------------------------------------------------------------------
 *  Comparaciones: 1 o 0 como las de libgcc (con un NaN, siempre 0)
 * ----------------------------------------------------------------------- */
/* NaN: el exponente todo unos y la mantisa no nula */
#define ES_NAN(l, h) (((h) << 1) > 0xFFE00000u || (((h) << 1) == 0xFFE00000u && (l) != 0u))

/* ¿Alguno es NaN? Casi nunca: dos comparaciones dicen que los dos son
 * finitos, y solo si no se mira la mantisa */
static inline __attribute__((always_inline)) int hay_nan(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh)
{
    if ((ah << 1) < 0xFFE00000u && (bh << 1) < 0xFFE00000u) return 0;
    return ES_NAN(al, ah) || ES_NAN(bl, bh);
}

/* a < b sin NaN (±Inf se ordenan como los demás; +0 y -0 son iguales): con
 * signos distintos, a es el negativo y no son los dos 0; con el mismo, la
 * magnitud, al revés si son negativos. Dentro de cada comparación (con -Os,
 * gcc la llamaría: una llamada más) */
static inline __attribute__((always_inline)) int menor(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh)
{
    if ((int32_t)(ah ^ bh) < 0) return (int32_t)ah < 0 && ((((ah | bh) << 1) | al | bl) != 0u);
    if ((int32_t)ah < 0) return ah > bh || (ah == bh && al > bl);
    return ah < bh || (ah == bh && al < bl);
}

int __wrap___aeabi_dcmpeq(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh)
{
    if (hay_nan(al, ah, bl, bh)) return 0;
    return (al == bl && ah == bh) || (((ah | bh) << 1) | al | bl) == 0u;
}

int __wrap___aeabi_dcmplt(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh)
{
    return !hay_nan(al, ah, bl, bh) && menor(al, ah, bl, bh);
}

int __wrap___aeabi_dcmple(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh)
{
    return !hay_nan(al, ah, bl, bh) && !menor(bl, bh, al, ah);
}

int __wrap___aeabi_dcmpge(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh)
{
    return !hay_nan(al, ah, bl, bh) && !menor(al, ah, bl, bh);
}

int __wrap___aeabi_dcmpgt(uint32_t al, uint32_t ah, uint32_t bl, uint32_t bh)
{
    return !hay_nan(al, ah, bl, bh) && menor(bl, bh, al, ah);
}
