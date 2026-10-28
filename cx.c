/* ============================================================================
 *  cx.c  -  Números complejos (ver cx.h)
 * ========================================================================= */
#include <string.h>
#include "cx.h"

#define PI_CX   3.14159265358979323846
#define DOSPI_CX 6.28318530717958647692

Cx cx_div(Cx a, Cx b)
{
    double r, d;
    if (b.im == 0.0) return cx(a.re / b.re, a.im / b.re);      /* exacto: como la LU real */
    if (fabs(b.re) >= fabs(b.im)) {
        r = b.im / b.re;
        d = b.re + b.im * r;
        return cx((a.re + a.im * r) / d, (a.im - a.re * r) / d);
    }
    r = b.re / b.im;
    d = b.re * r + b.im;
    return cx((a.re * r + a.im) / d, (a.im * r - a.re) / d);
}

/* cx_div(1, a) sin lo que el 1 + 0j deja en nada: 1 + 0·r es 1, 1·r es r
 * y 0·r - 1 es -1 (con r finito), y 0/re es un cero con el signo de re (re
 * finito y no nulo). Las operaciones que quedan son las mismas: los mismos
 * bits. Los módulos se comparan por los bits (sin los signos, el orden de los
 * finitos es el de los enteros). Con infinitos, NaN o a = 0, cx_div tal
 * cual. En la LU compleja (dinv) del Bode de un RLC, 3 de cada 4 pivotes
 * son reales: una división en vez de dos */
Cx cx_inv(Cx a)
{
    unsigned long long ur, ui;
    double r, d;
    memcpy(&ur, &a.re, sizeof ur);
    memcpy(&ui, &a.im, sizeof ui);
    ur &= 0x7FFFFFFFFFFFFFFFull;                          /* |re| y |im| */
    ui &= 0x7FFFFFFFFFFFFFFFull;
    if (ur >= 0x7FF0000000000000ull || ui >= 0x7FF0000000000000ull || (ui == 0 && ur == 0))
        return cx_div(cx(1.0, 0.0), a);
    if (ui == 0) return cx(1.0 / a.re, signbit(a.re) ? -0.0 : 0.0);
    if (ur >= ui) {
        r = a.im / a.re;
        d = a.re + a.im * r;
        return cx(1.0 / d, (0.0 - r) / d);
    }
    r = a.re / a.im;
    d = a.re * r + a.im;
    return cx((r + 0.0) / d, -1.0 / d);
}

double cx_abs(Cx a) { return hypot(a.re, a.im); }

double cx_arg(Cx a)
{
    double t;
    if (a.re == 0.0 && a.im == 0.0) return 0.0;
    t = atan2(a.im, a.re);
    return t <= -PI_CX ? PI_CX : t;                  /* atan2(-0, -1) = -π: sale π */
}

Cx cx_polar(double m, double rad) { return cx(m * cos(rad), m * sin(rad)); }

/* A (-π, π]. Con un margen de 1e-15 en los dos bordes: 3π da π y no -π por
 * el redondeo de la reducción. */
double cx_envuelve(double x)
{
    if (!(fabs(x) < 1e300)) return x;
    x = fmod(x, DOSPI_CX);                        /* (-2π, 2π) */
    if (x > PI_CX + 1e-15) x -= DOSPI_CX;
    else if (x <= -PI_CX + 1e-15) x += DOSPI_CX;
    return x;
}
