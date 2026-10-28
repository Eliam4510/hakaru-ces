/* ============================================================================
 *  esquema.h  -  El unico archivo que dibuja el circuito
 *
 *  Como dcl.c en EstaticaCX: el editor, los resultados y cada paso del
 *  procedimiento son el MISMO dibujo con otra Vista. Un paso no es un dibujo
 *  nuevo: es una mascara de elementos a resaltar.
 *
 *  En 320x240 no caben R, V, I y P de diez elementos a la vez, asi que el
 *  rotulo de cada elemento depende de la CAPA que se este mirando.
 * ========================================================================= */
#ifndef ESQUEMA_H
#define ESQUEMA_H

#include "circuito.h"
#include "resuelve.h"
#include "gfx.h"

/* Donde cae la rejilla en pantalla: punto (0,0) y paso entre puntos */
typedef struct { int x0, y0, px, py; } Marco;

enum { CAPA_NOMBRES = 0, CAPA_VALORES, CAPA_CORRIENTES, CAPA_TENSIONES,
       CAPA_POTENCIAS, CAPA_POTENCIALES, CAPA_N };

typedef struct {
    unsigned int   resalta;    /* bit k = elemento k en naranja            */
    unsigned char  atenuar;    /* 1 = lo no resaltado en gris              */
    unsigned char  capa;
    unsigned char  rejilla;    /* dibujar los puntos de la rejilla         */
    const Valores *val;        /* NULL = sin resolver                      */
    const Red     *red;        /* para los potenciales de nodo; puede ser NULL */
    int            cur_x, cur_y;   /* cursor del editor; -1 = sin cursor  */
    int            sel;        /* elemento seleccionado; -1 = ninguno      */
    unsigned char  sondas;     /* dibujar las sondas del osciloscopio      */
    const double  *lectura;    /* V y A: lo que marcan tras emular, o NULL */
    /* Etapa 3 (esq_vista los deja a 0) */
    const struct CaRes *ca;    /* un resultado de CA: fasores; NULL = no   */
    unsigned char  conv, rect; /* su convenio y polar (0) o rectangular (1) */
    /* Etapa 4 (esq_vista los deja a NULL y -1) */
    const unsigned char *brillo;   /* LED: nivel 0..9 por elemento (esq_led_nivel); NULL = sin encender */
    int            marca_x, marca_y;   /* el punto que estorba al colocar (cuadro rojo); -1 = no */
    int            ctl_de;     /* la controlada cuya línea de control se dibuja; -1 = ninguna */
    /* Las letras de los nodos (esq_vista los deja a 0 y -1). La capa
     * Potenciales las pone siempre delante del valor («B 7,5V») */
    unsigned char  letras;     /* 1 = la letra de cada nodo sin su valor (los pasos de Kirchhoff; con red) */
    int            nodo_resalta;   /* el nodo del paso: su letra y sus puntos en C_RESALTA; -1 = ninguno */
} Vista;

/* El convenio con el que esq_valor y los rótulos sin resolver enseñan los
 * fasores guardados (fuera de esq_dibuja: la línea "aquí" del editor) */
void esq_convenio(int conv);

void esq_vista(Vista *v);                 /* vista neutra */

/* Marco del editor: la rejilla entera, fija. */
void esq_marco_editor(Marco *m);
/* Marco ajustado a lo que ocupa el circuito dentro de un rectangulo. */
void esq_marco_ajustado(const Circuito *c, int x, int y, int w, int h, Marco *m);

void esq_dibuja(const Circuito *c, const Marco *m, const Vista *v);

/* Las cajas de los rótulos de los elementos que pintó el último esq_dibuja
 * (x, y, ancho, alto y el elemento k): el arnés mira que no se pisen */
typedef struct { short x, y, w, h, k; } EsqCaja;
int esq_cajas(const EsqCaja **c);

/* Pantalla de un punto de la rejilla. */
void esq_pt(const Marco *m, int gx, int gy, int *sx, int *sy);

const char *esq_capa_nombre(int capa);

/* Texto del rotulo de la capa VALORES para la magnitud principal */
void esq_valor(const Elemento *e, const Valores *v, int k, char *buf, int cap);
/* Lo mismo con el circuito: una controlada nombra a su control («2·V(R2)»,
 * «1k·I(R1)»; con v, la ganancia de la solución). Los demás, esq_valor. */
void esq_valor_c(const Circuito *c, const Valores *v, int k, char *buf, int cap);

/* ---- Etapa 4 ---- */
/* Las regiones (REG_* de sim.h, REG_N nombres): "conduce", "corte",
 * "satura +"...; "" en REG_NADA. Su color: conduce C_OK, corte C_SUAVE, el
 * resto C_CURSOR (un número fuera de rango da "" y C_SUAVE). */
extern const char *const NOM_REG[];
const char *esq_region(int reg);
u16  esq_region_col(int reg);
/* El brillo de un LED (D21): 0 si I < If/100; 9 si I > Im; si no, 8·I/If
 * redondeado, entre 1 y 8 */
int  esq_led_nivel(double I, double If, double Im);
/* If e Im de un LED (vacíos: los de nolin.h) */
void esq_led_if_im(const Elemento *e, double *If, double *Im);
/* Los niveles de todos los LED de c con la corriente I[k] de cada elemento
 * (0 en los que no son LED): lo que va en Vista.brillo */
void esq_brillos(const Circuito *c, const double *I, unsigned char br[MAX_EL]);
/* (f·(256 - t) + c·t)/256 por canal (RGB565), t de 0 a 256 */
u16  esq_mezcla(u16 fondo, u16 col, int t256);

/* Iconos de la barra de herramientas del editor, de 20x14 con la esquina
 * superior izquierda en (x,y). Los dibuja este archivo para que el icono de
 * la resistencia y la fuente sean EXACTAMENTE los simbolos del esquema.
 * Los de la Etapa 4 no pintan nada fuera de x..x+19, y..y+13. */
enum { ICO_RES = 0, ICO_FUENTE, ICO_CABLE, ICO_BORRAR, ICO_EDITAR, ICO_RESOLVER, ICO_CORR,
       ICO_VOLT, ICO_AMP, ICO_TIERRA, ICO_TERM, ICO_PALETA,
       ICO_COND, ICO_BOB, ICO_INTER, ICO_SONDA, ICO_IMP, ICO_VATI,
       ICO_DIODO, ICO_ZENER, ICO_LED, ICO_EDEP, ICO_JDEP, ICO_AO, ICO_BJT, ICO_MOS };
void esq_icono(int ico, int x, int y, u16 col);

/* Cuatro flechitas alrededor de un punto: "elige una direccion" */
void esq_flechas_cursor(int sx, int sy, u16 col);
/* Las mismas al colocar un 3 patas: la de la dirección d (DIR_*) en col si
 * el bit d de mascara está (cabe hacia d), y en col_no si no */
void esq_flechas_cursor_m(int sx, int sy, unsigned mascara, u16 col, u16 col_no);

#endif /* ESQUEMA_H */
