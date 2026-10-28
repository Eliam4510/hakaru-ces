/* ============================================================================
 *  kanji_img.h  -  El kanji 測る («medir», hakaru) de la portada
 *
 *  Una imagen ya rasterizada (kanji_img.c, GENERADA con
 *  herramientas/gen_kanji.py desde Noto Sans JP): la Nspire no tiene
 *  fuentes japonesas. Cada píxel es RGB565 con el antialias ya mezclado
 *  contra C_FONDO, así que se dibuja copiando las filas (dib_kanji, ui.c);
 *  solo vale sobre ese fondo. Recortada a la tinta.
 * ========================================================================= */
/* OJO: la guarda NO puede llamarse KANJI_H, que es el alto de la imagen. */
#ifndef KANJI_IMG_API_H
#define KANJI_IMG_API_H

extern const int KANJI_W, KANJI_H;
extern const unsigned short KANJI_PIX[];   /* KANJI_W * KANJI_H, fila a fila */

#endif /* KANJI_IMG_API_H */
