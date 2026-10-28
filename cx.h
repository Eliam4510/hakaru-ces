/* ============================================================================
 *  cx.h  -  Números complejos para la corriente alterna
 *
 *  Un tipo propio y no el _Complex de C99: GCC multiplica los complejos de
 *  C99 con __muldc3 (que vigila infinitos y NaN), y sin FPU cada llamada
 *  cuesta varias veces lo que cuatro productos y dos sumas. Estas funciones
 *  son static inline: en la calculadora se quedan en su sitio.
 *
 *  La división usa el método de Smith (sin desbordes con módulos de 1e300) y,
 *  con la parte imaginaria del divisor nula, da exactamente a.re/b.re: así la
 *  LU compleja con im = 0 hace las mismas operaciones que la real (luc.c).
 * ========================================================================= */
#ifndef CX_H
#define CX_H

#include <math.h>

typedef struct { double re, im; } Cx;

static inline Cx     cx(double re, double im)   { Cx z; z.re = re; z.im = im; return z; }
static inline Cx     cx_mas(Cx a, Cx b)         { return cx(a.re + b.re, a.im + b.im); }
static inline Cx     cx_menos(Cx a, Cx b)       { return cx(a.re - b.re, a.im - b.im); }
static inline Cx     cx_por(Cx a, Cx b)         { return cx(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re); }
static inline Cx     cx_esc(Cx a, double k)     { return cx(a.re * k, a.im * k); }
static inline Cx     cx_conj(Cx a)              { return cx(a.re, -a.im); }
static inline Cx     cx_j(Cx a)                 { return cx(-a.im, a.re); }          /* j·a            */
static inline double cx_abs1(Cx a)              { return fabs(a.re) + fabs(a.im); } /* pivotes         */
static inline int    cx_nulo(Cx a)              { return a.re == 0.0 && a.im == 0.0; }

Cx     cx_div(Cx a, Cx b);          /* Smith: sin desbordes; con im = 0 da a.re/b.re exacto */
Cx     cx_inv(Cx a);                /* 1/a por Smith                                        */
double cx_abs(Cx a);                /* hypot                                                */
double cx_arg(Cx a);                /* atan2 en (-π, π]; 0 si a = 0                         */
Cx     cx_polar(double m, double rad);
double cx_envuelve(double rad);     /* a (-π, π]                                            */

#endif /* CX_H */
