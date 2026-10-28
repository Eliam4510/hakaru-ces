/* ============================================================================
 *  pasos.c  -  Procedimiento paso a paso  (ver pasos.h)
 * ========================================================================= */
#include <string.h>
#include "pasos.h"

static void copia(char *dst, size_t cap, const char *src)
{
    size_t i = 0;
    if (!cap) return;
    if (!src) { dst[0] = 0; return; }
    while (src[i] && i < cap - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

void pasos_init(Pasos *p)
{
    p->n = 0;
    p->desborde = 0;
}

Paso *pasos_nuevo(Pasos *p, const char *titulo, unsigned int resalta)
{
    Paso *s;
    if (p->n >= PASO_MAX) { p->desborde = 1; return 0; }
    s = &p->items[p->n++];
    copia(s->titulo, PASO_TIT, titulo);
    s->detalle[0] = 0;
    s->resultado[0] = 0;
    s->resalta = resalta;
    s->nodo = -1;
    return s;
}

void pasos_add(Pasos *p, const char *titulo, const char *detalle,
               const char *resultado, unsigned int resalta)
{
    Paso *s = pasos_nuevo(p, titulo, resalta);
    if (!s) return;
    copia(s->detalle, PASO_TXT, detalle);
    copia(s->resultado, PASO_TXT, resultado);
}
