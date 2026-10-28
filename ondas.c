/* ============================================================================
 *  ondas.c  -  Formas de onda de las fuentes  (ver ondas.h)
 *
 *  Los instantes de los flancos salen de multiplicar (k·T, td + k·periodo) y
 *  el emulador llega a ellos sumando pasos: los dos pueden diferir en el
 *  último bit. Por eso las comparaciones llevan una tolerancia de 1e-9 del
 *  periodo: un instante que cae "casi en" un flanco cuenta como el flanco, y
 *  en el flanco ya vale el valor nuevo.
 * ========================================================================= */
#include <math.h>
#include "ondas.h"

#define PI_D 3.14159265358979323846
#define NUNCA 1e300
#define TOL 1e-9

/* Fracción del periodo en [0, 1), con el tramo final "casi 1" llevado a 0 */
static double fraccion(double t, double T, double *k)
{
    double u = t / T, n = floor(u + TOL), fr = u - n;
    if (fr < 0.0) fr = 0.0;
    if (k) *k = n;
    return fr;
}

static double ciclo01(double c)
{
    c *= 0.01;
    return c < 0.0 ? 0.0 : c > 1.0 ? 1.0 : c;
}

double onda_periodo(int forma, const double *p)
{
    switch (forma) {
    case ONDA_SENO: case ONDA_CUADRADA: case ONDA_TRIANGULAR:
        return p[1] > 0.0 ? 1.0 / p[1] : 0.0;
    case ONDA_PULSO:
        return (p[3] > 0.0 && p[4] > 0.0) ? p[4] : 0.0;
    default:
        return 0.0;
    }
}

double onda_valor(int forma, const double *p, double t)
{
    double T, fr, c;
    switch (forma) {
    case ONDA_SENO:
        if (!(p[1] > 0.0)) t = 0.0;
        return p[3] + p[0] * sin(2.0 * PI_D * p[1] * t + p[2] * (PI_D / 180.0));
    case ONDA_CUADRADA:
        if (!(p[1] > 0.0)) return p[3] + p[0];
        T = 1.0 / p[1];
        fr = fraccion(t, T, 0);
        c = ciclo01(p[2]);
        return (fr + TOL < c) ? p[3] + p[0] : p[3] - p[0];
    case ONDA_TRIANGULAR:
        if (!(p[1] > 0.0)) return p[3] - p[0];
        T = 1.0 / p[1];
        fr = fraccion(t, T, 0);
        c = ciclo01(p[2]);
        if (fr < c) return p[3] - p[0] + 2.0 * p[0] * fr / c;
        return p[3] + p[0] - 2.0 * p[0] * (fr - c) / (1.0 - c);
    case ONDA_PULSO: {
        double tt = t - p[2], ph, esc = fabs(p[2]) + fabs(p[3]) + fabs(p[4]) + 1e-30;
        if (tt + TOL * esc < 0.0) return p[0];
        if (!(p[3] > 0.0)) return p[1];                         /* escalón */
        ph = tt;
        if (p[4] > 0.0) ph = fraccion(tt < 0.0 ? 0.0 : tt, p[4], 0) * p[4];
        return (ph + TOL * esc < p[3]) ? p[1] : p[0];
    }
    default:
        return p[0];
    }
}

/* Escala de tiempo de la onda: lo que mide su tolerancia */
static double escala_t(int forma, const double *p)
{
    double T = onda_periodo(forma, p);
    if (T > 0.0) return T;
    if (forma == ONDA_PULSO) return fabs(p[2]) + fabs(p[3]) + fabs(p[4]) + 1e-30;
    return 1.0;
}

double onda_reposo(int forma, const double *p)
{
    switch (forma) {
    case ONDA_SENO:       return p[3];
    case ONDA_CUADRADA:
    case ONDA_TRIANGULAR: return p[3] - p[0];
    default:              return p[0];                  /* CC y PULSO */
    }
}

double onda_nivel_cc(int forma, const double *p)
{
    switch (forma) {
    case ONDA_SENO:
    case ONDA_TRIANGULAR: return p[3];
    case ONDA_CUADRADA:   return p[3] + p[0] * (p[2] / 50.0 - 1.0);
    default:              return p[0];                  /* CC y PULSO */
    }
}

double onda_valor_lado(int forma, const double *p, double t, int lado)
{
    double esc, dt;
    if (lado >= 0) return onda_valor(forma, p, t);
    esc = escala_t(forma, p);                   /* por la izquierda (su 1/f, solo aquí) */
    dt = 1e-6 * esc;
    if (t <= TOL * esc) return onda_reposo(forma, p);   /* antes de encenderse */
    if (forma == ONDA_CC || forma == ONDA_SENO) return onda_valor(forma, p, t);
    /* La triangular (con ciclo entre 0 y 100 %) es continua: en un vértice
     * solo cambia la pendiente y su límite por la izquierda es su valor. Con
     * el retroceso de abajo, un derivador (i = C·dv/dt) se llevaba 4e-6 V de
     * error en cada vértice. El diente de sierra (0 o 100 %) sí salta. */
    if (forma == ONDA_TRIANGULAR && p[2] > 0.0 && p[2] < 100.0) return onda_valor(forma, p, t);
    /* Un poco antes que la tolerancia de los flancos (1e-9 de la escala):
     * sale del flanco sin llegar al anterior. En un pulso (o un tramo de la
     * cuadrada) más estrecho que eso, el retroceso se queda en medio tramo:
     * si no, el pulso entero se perdería */
    if (forma == ONDA_PULSO) {
        if (p[3] > 0.0 && dt > 0.5 * p[3]) dt = 0.5 * p[3];
        if (p[4] > p[3] && dt > 0.5 * (p[4] - p[3])) dt = 0.5 * (p[4] - p[3]);
    } else if (forma == ONDA_CUADRADA || forma == ONDA_TRIANGULAR) {
        double T = onda_periodo(forma, p), c = p[2] * 0.01;
        if (c > 0.0 && c < 1.0) {
            double m = (c < 1.0 - c ? c : 1.0 - c) * T;
            if (dt > 0.5 * m) dt = 0.5 * m;
        }
    }
    return onda_valor(forma, p, t - dt);
}

/* El menor de los candidatos que queda después de t (con la tolerancia) */
static double despues(double t, double tol, const double *cand, int n)
{
    double m = NUNCA;
    int i;
    for (i = 0; i < n; i++) if (cand[i] > t + tol && cand[i] < m) m = cand[i];
    return m;
}

double onda_evento(int forma, const double *p, double t)
{
    double T, k, c, cand[4];
    switch (forma) {
    case ONDA_CUADRADA:
    case ONDA_TRIANGULAR:
        if (!(p[1] > 0.0)) return NUNCA;
        T = 1.0 / p[1];
        c = ciclo01(p[2]);
        (void)fraccion(t, T, &k);
        /* flancos (o vértices) en k·T y en (k + c)·T */
        cand[0] = k * T; cand[1] = (k + c) * T; cand[2] = (k + 1.0) * T; cand[3] = (k + 1.0 + c) * T;
        if (forma == ONDA_CUADRADA && (c <= 0.0 || c >= 1.0)) return NUNCA;   /* es constante */
        return despues(t, TOL * T, cand, 4);
    case ONDA_PULSO: {
        double esc = (fabs(p[2]) + fabs(p[3]) + fabs(p[4])) * TOL + 1e-30, base;
        if (t + esc < p[2]) return p[2];                         /* el primer flanco */
        if (!(p[3] > 0.0)) return NUNCA;                          /* escalón: ya pasó */
        if (!(p[4] > 0.0)) {                                      /* un solo pulso */
            cand[0] = p[2] + p[3];
            return despues(t, esc, cand, 1);
        }
        (void)fraccion(t - p[2], p[4], &k);
        base = p[2] + k * p[4];
        cand[0] = base; cand[1] = base + p[3]; cand[2] = base + p[4]; cand[3] = base + p[4] + p[3];
        if (p[3] >= p[4]) return NUNCA;                           /* nunca baja */
        return despues(t, esc, cand, 4);
    }
    default:
        return NUNCA;
    }
}

const char *onda_nombre(int forma)
{
    switch (forma) {
    case ONDA_SENO:       return "senoidal";
    case ONDA_CUADRADA:   return "cuadrada";
    case ONDA_TRIANGULAR: return "triangular";
    case ONDA_PULSO:      return "pulso";
    default:              return "continua";
    }
}
