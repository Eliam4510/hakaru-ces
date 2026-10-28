/* ============================================================================
 *  gfx.c  -  Capa grafica de EstaticaCX
 *
 *  Todo se dibuja en un backbuffer propio y se vuelca de una sola vez, asi
 *  que no hay parpadeo. Ni una sola operacion en coma flotante: el ARM926 de
 *  la CX II no tiene FPU y el soft-float dentro de un bucle de rasterizado
 *  costaria decimas de segundo por cuadro. Los angulos usan una tabla de seno
 *  en punto fijo (x1024).
 * ========================================================================= */
#include "gfx.h"
#include "font.h"

#ifdef NDLESS
#include <libndls.h>
#else
#include <stdio.h>
#include <string.h>
#include <time.h>
#endif

/* --------------------------------------------------------------------------
 *  Estado
 * ----------------------------------------------------------------------- */
/* 150 KB en .bss. La union lo alinea a 4 bytes: gfx_clear, gfx_hline y
 * gfx_fila_pares escriben de dos en dos píxeles, por palabras de 32 bits
 * (siempre por el miembro w)                                              */
static union { u16 h[SCR_W * SCR_H]; u32 w[SCR_W * SCR_H / 2]; } fbu;
#define fb (fbu.h)
static int cl_x0 = 0, cl_y0 = 0, cl_x1 = SCR_W - 1, cl_y1 = SCR_H - 1;

u16 *gfx_buffer(void) { return fb; }

void gfx_clip(int x, int y, int w, int h)
{
    cl_x0 = x < 0 ? 0 : x;
    cl_y0 = y < 0 ? 0 : y;
    cl_x1 = x + w - 1; if (cl_x1 > SCR_W - 1) cl_x1 = SCR_W - 1;
    cl_y1 = y + h - 1; if (cl_y1 > SCR_H - 1) cl_y1 = SCR_H - 1;
}

void gfx_clip_reset(void)
{
    cl_x0 = 0; cl_y0 = 0; cl_x1 = SCR_W - 1; cl_y1 = SCR_H - 1;
}

int gfx_clip_arriba(void) { return cl_y0; }

/* --------------------------------------------------------------------------
 *  Ciclo de vida
 * ----------------------------------------------------------------------- */
#ifdef NDLESS
/* Modo de pantalla que tenia el sistema al entrar. Esta version del SDK no
 * ofrece lcd_deinit(): la unica forma de devolver la pantalla al OS es volver
 * a llamar a lcd_init() con el modo original. Si no se hace, el sistema se
 * queda con el framebuffer en otro formato al salir de la app.             */
static scr_type_t s_tipo_orig = SCR_TYPE_INVALID;

void gfx_init(void)
{
    font_init();
    s_tipo_orig = lcd_type();
    /* La CX II no admite escritura directa al framebuffer como la CX vieja:
     * el camino valido es declarar el modo aqui y volcar con lcd_blit().   */
    lcd_init(SCR_320x240_565);
}

void gfx_deinit(void)
{
    if (s_tipo_orig != SCR_TYPE_INVALID) lcd_init(s_tipo_orig);
}

void gfx_flush(void)  { lcd_blit(fb, SCR_320x240_565); }
unsigned long gfx_reloj_ms(void) { return *(volatile unsigned *)0x90090000 * 1000ul; }
#else
void gfx_init(void)   { font_init(); }
void gfx_deinit(void) { }
void gfx_flush(void)  { }
unsigned long gfx_reloj_ms(void) { return (unsigned long)((double)clock() * 1000.0 / CLOCKS_PER_SEC); }
#endif

/* --------------------------------------------------------------------------
 *  Primitivas basicas
 * ----------------------------------------------------------------------- */
/* Dos píxeles por palabra y ocho palabras por vuelta (la pantalla entera
 * son 4800): una cuarta parte de las escrituras y casi nada de bucle */
void gfx_clear(u16 c)
{
    u32 v = (u32)c | (u32)c << 16, *p = fbu.w, *fin = fbu.w + SCR_W * SCR_H / 2;
    while (p < fin) {
        p[0] = v; p[1] = v; p[2] = v; p[3] = v;
        p[4] = v; p[5] = v; p[6] = v; p[7] = v;
        p += 8;
    }
}

void gfx_px(int x, int y, u16 c)
{
    if (x < cl_x0 || x > cl_x1 || y < cl_y0 || y > cl_y1) return;
    fb[y * SCR_W + x] = c;
}

/* Por palabras: los píxeles i..j del backbuffer; el de un borde que cae en
 * media palabra va solo, y los de en medio de dos en dos */
void gfx_hline(int x, int y, int w, u16 c)
{
    int x2 = x + w - 1, i, j;
    if (y < cl_y0 || y > cl_y1) return;
    if (x  < cl_x0) x  = cl_x0;
    if (x2 > cl_x1) x2 = cl_x1;
    if (x > x2) return;
    i = y * SCR_W + x;
    j = y * SCR_W + x2;
    if (i & 1) fb[i++] = c;
    if (!(j & 1)) fb[j--] = c;
    if (i < j) {
        u32 v = (u32)c | (u32)c << 16, *p = fbu.w + (i >> 1), *fin = fbu.w + (j >> 1);
        while (p <= fin) *p++ = v;
    }
}

/* Una palabra (dos píxeles) por byte de f, sacada de su tabla: la fila
 * entera de una imagen de 4 bits por píxel sin tocar cada píxel. Sin
 * recorte, pero nunca fuera de la pantalla: una fila fuera no escribe nada,
 * y de una fila no se pasa (160 palabras) */
void gfx_fila_pares(int y, const unsigned char *f, int n, const u32 *pares)
{
    u32 *d;
    int i;
    if ((unsigned)y >= (unsigned)SCR_H) return;
    if (n > SCR_W / 2) n = SCR_W / 2;
    d = fbu.w + y * (SCR_W / 2);
    for (i = 0; i < n; i++) d[i] = pares[f[i]];
}

void gfx_vline(int x, int y, int h, u16 c)
{
    int y2 = y + h - 1;
    u16 *p;
    if (x < cl_x0 || x > cl_x1) return;
    if (y  < cl_y0) y  = cl_y0;
    if (y2 > cl_y1) y2 = cl_y1;
    if (y > y2) return;
    p = fb + y * SCR_W + x;
    while (y++ <= y2) { *p = c; p += SCR_W; }
}

void gfx_fill(int x, int y, int w, int h, u16 c)
{
    int j;
    for (j = 0; j < h; j++) gfx_hline(x, y + j, w, c);
}

int gfx_ocupados(int x, int y, int w, int h, u16 fondo)
{
    int i, j, n = 0;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            if (i < cl_x0 || i > cl_x1 || j < cl_y0 || j > cl_y1 || fb[j * SCR_W + i] != fondo) n++;
    return n;
}

void gfx_rect(int x, int y, int w, int h, u16 c)
{
    if (w <= 0 || h <= 0) return;
    gfx_hline(x, y,         w, c);
    gfx_hline(x, y + h - 1, w, c);
    gfx_vline(x,         y, h, c);
    gfx_vline(x + w - 1, y, h, c);
}

/* --------------------------------------------------------------------------
 *  Lineas (Bresenham entero)
 * ----------------------------------------------------------------------- */
void gfx_line(int x0, int y0, int x1, int y1, u16 c)
{
    int dx =  (x1 > x0 ? x1 - x0 : x0 - x1);
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;) {
        gfx_px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Grosor barato pero suficiente: se repite la linea desplazada en el eje
 * perpendicular al dominante. Para vectores de fuerza (grosor 2) es exacto
 * a la vista y cuesta lo mismo que dos lineas.                              */
void gfx_line_w(int x0, int y0, int x1, int y1, u16 c, int grosor)
{
    int adx = (x1 > x0 ? x1 - x0 : x0 - x1);
    int ady = (y1 > y0 ? y1 - y0 : y0 - y1);
    int i, d;

    if (grosor < 1) grosor = 1;
    if (grosor > 4) grosor = 4;

    for (i = 0; i < grosor; i++) {
        /* 0, +1, -1, +2 : crece centrado alrededor del trazo original */
        d = (i + 1) / 2;
        if (i & 1) d = -d;
        if (adx >= ady) gfx_line(x0, y0 + d, x1, y1 + d, c);
        else            gfx_line(x0 + d, y0, x1 + d, y1, c);
    }
}

/* Trazo discontinuo: en toda la suite significa "todavia es incognita". */
void gfx_line_dash(int x0, int y0, int x1, int y1, u16 c, int on, int off)
{
    int dx =  (x1 > x0 ? x1 - x0 : x0 - x1);
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    int n = 0, ciclo;

    if (on  < 1) on  = 1;
    if (off < 1) off = 1;
    ciclo = on + off;

    for (;;) {
        if (n % ciclo < on) gfx_px(x0, y0, c);
        n++;
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* --------------------------------------------------------------------------
 *  Circulos (punto medio)
 * ----------------------------------------------------------------------- */
void gfx_circle(int cx, int cy, int r, u16 c, int relleno)
{
    int x = r, y = 0, err = 1 - r;

    if (r < 0) return;
    if (r == 0) { gfx_px(cx, cy, c); return; }

    while (x >= y) {
        if (relleno) {
            gfx_hline(cx - x, cy + y, 2 * x + 1, c);
            gfx_hline(cx - x, cy - y, 2 * x + 1, c);
            gfx_hline(cx - y, cy + x, 2 * y + 1, c);
            gfx_hline(cx - y, cy - x, 2 * y + 1, c);
        } else {
            gfx_px(cx + x, cy + y, c); gfx_px(cx - x, cy + y, c);
            gfx_px(cx + x, cy - y, c); gfx_px(cx - x, cy - y, c);
            gfx_px(cx + y, cy + x, c); gfx_px(cx - y, cy + x, c);
            gfx_px(cx + y, cy - x, c); gfx_px(cx - y, cy - x, c);
        }
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

/* --------------------------------------------------------------------------
 *  Triangulo relleno  (puntas de flecha, apoyos fijos)
 *  Barrido por lineas: para cada y se cortan las tres aristas y se rellena
 *  entre el minimo y el maximo. Simple y robusto frente a casos degenerados.
 * ----------------------------------------------------------------------- */
static void corte(int xa, int ya, int xb, int yb, int y, int *mn, int *mx)
{
    int x, ylo, yhi;

    ylo = ya < yb ? ya : yb;
    yhi = ya < yb ? yb : ya;
    if (y < ylo || y > yhi) return;

    if (ya == yb) {                     /* arista horizontal: cuenta entera */
        if (xa < *mn) *mn = xa;
        if (xa > *mx) *mx = xa;
        if (xb < *mn) *mn = xb;
        if (xb > *mx) *mx = xb;
        return;
    }
    x = xa + (int)(((long)(xb - xa) * (y - ya)) / (yb - ya));
    if (x < *mn) *mn = x;
    if (x > *mx) *mx = x;
}

void gfx_tri(int x0, int y0, int x1, int y1, int x2, int y2, u16 c)
{
    int ymin = y0, ymax = y0, y, mn, mx;

    if (y1 < ymin) ymin = y1;
    if (y2 < ymin) ymin = y2;
    if (y1 > ymax) ymax = y1;
    if (y2 > ymax) ymax = y2;

    if (ymin < cl_y0) ymin = cl_y0;
    if (ymax > cl_y1) ymax = cl_y1;

    for (y = ymin; y <= ymax; y++) {
        mn = 0x7FFFFFF; mx = -0x7FFFFFF;
        corte(x0, y0, x1, y1, y, &mn, &mx);
        corte(x1, y1, x2, y2, y, &mn, &mx);
        corte(x2, y2, x0, y0, y, &mn, &mx);
        if (mn <= mx) gfx_hline(mn, y, mx - mn + 1, c);
    }
}

/* --------------------------------------------------------------------------
 *  Trigonometria en punto fijo (x1024)
 * ----------------------------------------------------------------------- */
static const short SENO[91] = {
       0,  18,  36,  54,  71,  89, 107, 125, 143, 160,
     178, 195, 213, 230, 248, 265, 282, 299, 316, 333,
     350, 367, 384, 400, 417, 433, 449, 465, 481, 496,
     512, 527, 543, 558, 573, 587, 602, 616, 630, 644,
     658, 672, 685, 698, 711, 724, 737, 749, 761, 773,
     784, 796, 807, 818, 828, 839, 849, 859, 868, 878,
     887, 896, 904, 912, 920, 928, 935, 943, 950, 956,
     962, 968, 974, 979, 984, 989, 993, 997,1001,1005,
    1008,1011,1014,1016,1018,1020,1022,1023,1023,1024,
    1024
};

int gfx_isin(int grados)
{
    int d = grados;                     /* el % (una división de software) solo fuera de [0, 720) */
    if ((unsigned)d >= 360u) {          /* sin signo: los negativos también */
        if ((unsigned)d < 720u) d -= 360;
        else { d %= 360; if (d < 0) d += 360; }
    }
    if (d <=  90) return  SENO[d];
    if (d <= 180) return  SENO[180 - d];
    if (d <= 270) return -SENO[d - 180];
    return -SENO[360 - d];
}

int gfx_icos(int grados) { return gfx_isin(grados + 90); }

/* --------------------------------------------------------------------------
 *  Arco de circunferencia
 *  Se muestrea grado a grado y se unen los puntos con segmentos, de modo que
 *  el trazo queda continuo para cualquier radio. Angulos en grados con el
 *  criterio fisico: 0 = eje +x, y creciendo hacia ARRIBA.
 * ----------------------------------------------------------------------- */
void gfx_arc(int cx, int cy, int r, int a0, int a1, u16 c)
{
    int a, px = 0, py = 0, x, y, primero = 1, paso;

    if (r <= 0) return;
    if (a1 < a0) { int t = a0; a0 = a1; a1 = t; }
    paso = (r > 40) ? 1 : (r > 12 ? 2 : 3);

    for (a = a0; ; a += paso) {
        if (a > a1) a = a1;
        x = cx + (r * gfx_icos(a) >> 10);
        y = cy - (r * gfx_isin(a) >> 10);   /* y de pantalla crece hacia abajo */
        if (primero) { primero = 0; }
        else gfx_line(px, py, x, y, c);
        px = x; py = y;
        if (a >= a1) break;
    }
}

/* --------------------------------------------------------------------------
 *  Rayado a 45 grados: suelo, muros, empotramientos
 * ----------------------------------------------------------------------- */
void gfx_hatch(int x, int y, int w, int h, u16 c, int paso)
{
    int sx0 = cl_x0, sy0 = cl_y0, sx1 = cl_x1, sy1 = cl_y1;
    int i;

    if (paso < 2) paso = 2;

    /* Recorte temporal al rectangulo pedido, interseccion con el vigente */
    gfx_clip(x > sx0 ? x : sx0,
             y > sy0 ? y : sy0,
             ((x + w - 1 < sx1 ? x + w - 1 : sx1) - (x > sx0 ? x : sx0)) + 1,
             ((y + h - 1 < sy1 ? y + h - 1 : sy1) - (y > sy0 ? y : sy0)) + 1);

    for (i = -h; i < w; i += paso)
        gfx_line(x + i, y + h - 1, x + i + h, y - 1, c);

    cl_x0 = sx0; cl_y0 = sy0; cl_x1 = sx1; cl_y1 = sy1;
}

/* --------------------------------------------------------------------------
 *  Texto
 * ----------------------------------------------------------------------- */
int gfx_text_w(const char *s, int escala)
{
    int n = 0;
    if (escala < 1) escala = 1;
    while (*s++) n++;
    return n * FONT_W * escala;
}

static int s_desbordes = 0;

static int s_silencio = 0;

int  gfx_desbordes(void)         { return s_desbordes; }
void gfx_desbordes_reset(void)   { s_desbordes = 0; }
void gfx_desbordes_silencio(int s){ s_silencio = s; }

#ifndef NDLESS
/* Registro de los textos dibujados, para que el arnés compruebe lo que se ve */
static char s_textos[32768];
static size_t s_ntextos = 0;

void gfx_textos_reset(void) { s_ntextos = 0; s_textos[0] = 0; }
int  gfx_texto_visto(const char *t) { return strstr(s_textos, t) != 0; }

static void anota_texto(const char *s)
{
    size_t n = strlen(s);
    if (s_ntextos + n + 2 >= sizeof s_textos) return;
    memcpy(s_textos + s_ntextos, s, n);
    s_ntextos += n;
    s_textos[s_ntextos++] = '\n';
    s_textos[s_ntextos] = 0;
}
#endif

static void cuenta_desborde(int x, const char *s, int escala)
{
#ifndef NDLESS
    anota_texto(s);
#endif
    if (x >= 0 && x + gfx_text_w(s, escala) > SCR_W) {
        s_desbordes++;
#ifndef NDLESS
        if (!s_silencio && s_desbordes <= 24)
            printf("    [desborde] x=%d ancho=%d  \"%s\"\n",
                   x, gfx_text_w(s, escala), s);
#endif
    }
}

/* Motor comun: 'alto' filas de glifo, 'ancho' columnas utiles. El glifo que
 * cae entero dentro del recorte va directo al backbuffer (sin gfx_px ni
 * gfx_fill por punto): los mismos píxeles, con la máscara recortada a su
 * ancho */
static int texto(int x, int y, const char *s, u16 fg, int escala,
                 const u8 *(*glifo)(unsigned), int alto, int ancho)
{
    int px = x, gy, gx, filas = y >= cl_y0 && y + alto * escala - 1 <= cl_y1;
    const u8 *g;
    u8 m;

    while (*s) {
        g = glifo((unsigned char)*s);
        if (filas && px >= cl_x0 && px + ancho * escala - 1 <= cl_x1) {
            u16 *f = fb + y * SCR_W + px;
            for (gy = 0; gy < alto; gy++, f += escala * SCR_W)
                for (gx = 0, m = (u8)(g[gy] & ((1u << ancho) - 1u)); m; gx++, m = (u8)(m >> 1)) {
                    u16 *b = f + gx * escala;
                    int i, j;
                    if (!(m & 1u)) continue;
                    if (escala == 1) { *b = fg; continue; }
                    for (j = 0; j < escala; j++, b += SCR_W)
                        for (i = 0; i < escala; i++) b[i] = fg;
                }
            px += FONT_W * escala;
            s++;
            continue;
        }
        for (gy = 0; gy < alto; gy++) {
            m = g[gy];
            if (!m) continue;
            for (gx = 0; gx < ancho; gx++) {
                if (!(m & (1u << gx))) continue;
                if (escala == 1) gfx_px(px + gx, y + gy, fg);
                else gfx_fill(px + gx * escala, y + gy * escala, escala, escala, fg);
            }
        }
        px += FONT_W * escala;
        s++;
    }
    return px - x;
}

int gfx_text(int x, int y, const char *s, u16 fg, int escala)
{
    if (escala < 1) escala = 1;
    cuenta_desborde(x, s, escala);
    return texto(x, y, s, fg, escala, font_glifo12, FONT_H, 6);
}

int gfx_text_bg(int x, int y, const char *s, u16 fg, u16 bg, int escala)
{
    int w = gfx_text_w(s, escala);
    if (escala < 1) escala = 1;
    gfx_fill(x, y, w, FONT_H * escala, bg);
    return gfx_text(x, y, s, fg, escala);
}

int gfx_text_p(int x, int y, const char *s, u16 fg)
{
    cuenta_desborde(x, s, 1);
    return texto(x, y, s, fg, 1, font_glifo, FONT_H_P, 5);
}

int gfx_text_p_bg(int x, int y, const char *s, u16 fg, u16 bg)
{
    gfx_fill(x, y, gfx_text_w(s, 1), FONT_H_P, bg);
    return gfx_text_p(x, y, s, fg);
}

int gfx_text_c(int cx, int y, const char *s, u16 fg, int escala)
{
    return gfx_text(cx - gfx_text_w(s, escala) / 2, y, s, fg, escala);
}

/* --------------------------------------------------------------------------
 *  Volcado a BMP (solo PC)
 *  Es lo que permite revisar cada diagrama en el PC sin pasar por la
 *  calculadora: el renderizador completo se depura con MSVC.
 * ----------------------------------------------------------------------- */
#ifndef NDLESS

static void w16(FILE *f, unsigned v)
{
    fputc((int)( v        & 0xFF), f);
    fputc((int)((v >>  8) & 0xFF), f);
}

static void w32(FILE *f, unsigned v)
{
    fputc((int)( v        & 0xFF), f);
    fputc((int)((v >>  8) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 24) & 0xFF), f);
}

static int dump(const char *ruta, int zoom)
{
    FILE *f;
    int ancho, alto, relleno, x, y, zx, zy;
    unsigned tam;

    if (zoom < 1) zoom = 1;
    ancho = SCR_W * zoom;
    alto  = SCR_H * zoom;
    relleno = (4 - (ancho * 3) % 4) % 4;
    tam = 54u + (unsigned)((ancho * 3 + relleno) * alto);

    f = fopen(ruta, "wb");
    if (!f) return 1;

    fputc('B', f); fputc('M', f);
    w32(f, tam); w32(f, 0); w32(f, 54);
    w32(f, 40); w32(f, (unsigned)ancho); w32(f, (unsigned)alto);
    w16(f, 1); w16(f, 24); w32(f, 0);
    w32(f, tam - 54u); w32(f, 2835); w32(f, 2835); w32(f, 0); w32(f, 0);

    /* BMP se guarda de abajo hacia arriba */
    for (y = SCR_H - 1; y >= 0; y--) {
        for (zy = 0; zy < zoom; zy++) {
            for (x = 0; x < SCR_W; x++) {
                u16 c = fb[y * SCR_W + x];
                int r5 = (c >> 11) & 0x1F, g6 = (c >> 5) & 0x3F, b5 = c & 0x1F;
                int r8 = (r5 << 3) | (r5 >> 2);
                int g8 = (g6 << 2) | (g6 >> 4);
                int b8 = (b5 << 3) | (b5 >> 2);
                for (zx = 0; zx < zoom; zx++) {
                    fputc(b8, f); fputc(g8, f); fputc(r8, f);
                }
            }
            for (x = 0; x < relleno; x++) fputc(0, f);
        }
    }
    fclose(f);
    return 0;
}

int gfx_dump_bmp(const char *ruta)             { return dump(ruta, 1); }
int gfx_dump_bmp_zoom(const char *ruta, int z) { return dump(ruta, z); }

#endif /* !NDLESS */
