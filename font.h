/* ============================================================================
 *  font.h  -  Acceso a la fuente bitmap 6x8
 * ========================================================================= */
#ifndef FONT_API_H
#define FONT_API_H

#include "gfx.h"

/* Compila la tabla ASCII-art a bits. Idempotente; gfx_init() ya la llama. */
void font_init(void);

/* Devuelve las 8 filas del glifo (bit x = columna x). Nunca devuelve NULL:
 * un codigo sin glifo da un recuadro visible para delatar el fallo.        */
const u8 *font_glifo(unsigned c);

/* Fuente del texto: Terminus 6x12 (font6x12.c). 12 filas, 6 columnas. */
const u8 *font_glifo12(unsigned c);

#endif /* FONT_API_H */
