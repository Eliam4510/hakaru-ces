/* ============================================================================
 *  ui.c  -  Interfaz de Hakaru CES
 *
 *  Tres partes, desde el menú principal:
 *
 *    1. TALLER   dibuja tu circuito y lo emula, lo analiza (CC, CA, Bode)
 *                o despeja lo que buscas. Editor de rejilla con barra de
 *                herramientas, opciones con [menu], deshacer y rehacer,
 *                copiar y pegar, cuentas en la ficha, "Mis problemas"
 *                para guardar (con nombre) y la sesión (el circuito
 *                de la última vez, que main.c abre y guarda).
 *    2. APRENDE  trece lecciones: unas páginas de teoría y sus ejemplos
 *                resueltos paso a paso (1 a 9; la 9, los problemas del
 *                pizarrón) o circuitos modelo para emular (10 a 13).
 *    3. AYUDA    cómo se usa.
 *
 *  Resultado, tabla y procedimiento son comunes: se llega desde el Taller o
 *  desde un ejemplo, y [esc] vuelve a donde se estaba.
 *
 *  El texto usa Terminus 6x12 (53 columnas, 13 px por línea). Presupuesto:
 *      0..15    barra de título
 *      16..226  contenido
 *      227..239 pie con las teclas
 * ========================================================================= */
#include <string.h>
#include <math.h>
#include "ui.h"
#include "gfx.h"
#include "numfmt.h"
#include "circuito.h"
#include "red.h"
#include "resuelve.h"
#include "proced.h"
#include "esquema.h"
#include "ejemplos.h"
#include "lecciones.h"
#include "archivo.h"
#if ESPECIAL
#include "especial.h"               /* la versión especial (no se publica) */
#else
/* La versión pública: los ganchos de la especial, vacíos */
#define ESP_AYUDA
#define ESP_SE_BORRAN
#define ESP_DIBUJA
#define ESP_ANIMA
#define ESP_TICK
#define ESP_SIN_SHIFT
#define ESP_PORTADA(t)
#define ESP_TECLA(t)
#define ESP_FN_JUEGA
#define ESP_FIRMA(f)
#define ESP_REPITE(k, vert)
#endif
#include "banco.h"
#include "sim.h"
#include "nolin.h"
#include "osc.h"
#include "biblioteca.h"
#include "ca.h"
#include "bode.h"
#include "trazador.h"
#include "calca.h"
#include "fasorial.h"
#include "curva.h"
#include "app.h"
#include "kanji_img.h"
#include "entrada.h"
#include "turbo.h"
#include "capmotor.h"
#include "capeq_texto.h"

#define BARRA_H   16
#define PIE_Y    227
#define LIN_H     13
#define COLS      52

#define ED_HERR  188          /* barra de herramientas del editor     */
#define ED_AQUI  209          /* línea "Aquí:"                        */
#define ED_ESTADO 223         /* línea de estado del problema         */
#define FO_PANEL 146          /* formulario de un elemento            */
#define RS_PANEL 158          /* panel de resultados                  */
#define PS_PANEL 104          /* texto de la pantalla de pasos       */
#define TE_TEXTO (BARRA_H + 4 + FIG_ALTO + 2)   /* texto bajo la figura */

enum { ACC_NADA = 0, ACC_PONER, ACC_CABLE, ACC_BORRAR, ACC_EDITAR, ACC_SONDA };
enum { AV_NADA = 0, AV_R, AV_NUMERO, AV_RANGO, AV_FUERA, AV_ANG, AV_VPVN,     /* avisos del formulario */
       AV_CUENTA, AV_NEGATIVO };                                                /* las cuentas */

#define HIST 12               /* niveles de deshacer                  */

/* --------------------------------------------------------------------------
 *  Estado
 * ----------------------------------------------------------------------- */
static int pant, origen;              /* origen: a dónde vuelve el resultado */
static int sel_menu, sel_taller, scroll;

static Circuito  circ;                /* el que se está editando            */
static Circuito  cres;                /* el que se resuelve y se enseña     */
static Red       red;
static Solucion  sol;
static Pasos     pasos;
static int       metodo;
static int       hueco;               /* de "Mis problemas", o -1           */

/* editor */
static int cur_x, cur_y, accion, sel_el;
static int poner_tipo;                /* con ACC_PONER: el tipo que se coloca */
static int marca_x = -1, marca_y = -1;/* el punto que estorba a un 3 patas (-1: ninguno) */
static int paleta_abierta, sel_pal;   /* la paleta de componentes         */
static void dib_paleta(void);
static double ultimo_R;
static int    hay_ultimo_R;
static int    ctx_abierto, sel_ctx;
static char   aviso[128];           /* dos líneas de COLS: un enunciado traído */
static u16    aviso_col;
/* Una segunda línea del aviso, en la de estado y mientras dure el aviso (se
 * va con él): al traer un ejemplo, su enunciado arriba y esto debajo. Un
 * aviso que no cabe en una línea ocupa las dos (y la de estado vuelve con la
 * tecla siguiente); entonces lo de abajo espera a esa tecla (aviso_luego) */
static const char *aviso_bajo, *aviso_luego;
/* Deshacer y rehacer: cada nivel es el circuito con su hueco y su documento
 * (doc: un número por problema abierto, nuevo o traído; al guardar en un hueco
 * nuevo, los niveles del mismo documento pasan a ese hueco y ese nombre, y así
 * deshacer tras abrir otro problema no hace que Guardar pise el que se abrió) */
typedef struct { Circuito c; int hueco, doc; } Nivel;
static Nivel hist[HIST], reh[HIST];
static int   nhist, nreh, doc_act, doc_n;
/* [tab] (o volver de un error) eligió sel_el: el [enter] siguiente abre su
 * ficha sin preguntar la flecha; elegido_q, la fila (Q_*) en que se abre, o -1 */
static int elegido, elegido_q = -1;
/* La ficha de un elemento que ya estaba: el circuito al abrirla. Solo gasta un
 * nivel de deshacer al salir si algo cambió */
static Circuito ficha_antes;
static int      ficha_pend;
static int      ses_aviso;            /* la sesión se recuperó: se dice al entrar */
/* Volver de un error al elemento que lo causa (ver culpable) */
static void pie_error(const char *sin);
static void a_culpable(void);

/* formulario */
static int  campo, campo_ini, editando, aviso_r;
static int  sub;                      /* CA: 0 = caja del módulo, 1 = la del ángulo */
static char edit[32];                 /* un número, o una cuenta en la ficha (31) */
#define EDIT_OTROS 19                 /* las demás cajas (los ajustes, la parte j): las de siempre */
static int  aviso_cuenta;             /* con AV_CUENTA: por qué no vale (NE_*) */
static int  cuentas_aqui(void);

/* El portapapeles del editor ([ctrl][C], [ctrl][X] y [ctrl][V]): la «ficha
 * física» de un componente (tipo, variante, valor, aux y par[]), sin los datos
 * de V e I, los [?!], las sondas ni la etiqueta. No se vacía al abrir otro
 * problema: se copia de uno y se pega en otro */
static struct {
    int      hay;
    Elemento e;                       /* su geometría y su etiqueta no cuentan */
    char     de[6];                   /* la etiqueta del original             */
    char     ctl[6];                  /* la del que manda una controlada, o "" */
    int      espejo;                  /* un 3 patas con [F]                   */
    Mag      frec;                    /* la f del circuito de donde vino      */
    char     pegar[64];               /* «Pegar R2 (4,7 kΩ): una flecha. [esc] no» */
} clip;
static int pegando;                   /* ACC_PONER pone la copia de clip      */
static void aplica_clip(int k);
#define COPIA_PUESTA "La copia, con sus valores: rev" S_ii "salos."   /* en su ficha */

/* Poner nombre: en Mis problemas ([N], el elegido) y en el editor ([ctrl][N],
 * y lo guarda). La línea se escribe con las letras, las cifras y el espacio */
enum { NOM_NO = 0, NOM_MIS, NOM_EDITOR };
static int  nombrando;
static char nom_edit[24];
static const char *nom_aviso;         /* por qué no se pudo (en rojo), o 0 */
/* ¿Se escribe un nombre en la pantalla que se ve? ([ctrl][menu] deja el
 * nombre a medias y al volver sigue) */
static int escribe_nombre(void)
{
    return (nombrando == NOM_MIS && pant == P_MIS) || (nombrando == NOM_EDITOR && pant == P_EDITOR);
}

/* resultados */
static int capa, isol, paso_i;

/* la alterna: capa, origen, polar o rectangular, página de la tabla */
static int ca_capa, ca_origen, ca_rect, ca_pag;
/* la calculadora de CA: el problema como se planteó (con sus [?!]); cres
 * lleva la solución ca_isol ya puesta como datos */
static Circuito ca_pose;
static int ca_calc, ca_isol;
static void calcula_ca_de(void);
/* el diagrama fasorial */
static Fasorial fz;
/* P_EMULANDO sirve para cuatro trabajos: emular, barrer el Bode, calcular en
 * CA y el barrido de CC */
enum { TR_EMULAR = 0, TR_BODE, TR_CALCA, TR_BARRIDO };
static int trabajo;
/* el Bode: la pantalla, su rango (de la sesión) y de dónde se vino */
static Trazador traz;
static BodeAjustes bode_aj;
static char bode_ent_etiq[6];         /* la etiqueta de bode_aj.ent (la de [F]) */
static int bode_origen;
static Circuito bode_c;
static Red bode_red;
static void bode_de(int desde, const Circuito *c);
static void fin_bode(void);
/* Etapa 4: el punto de operación (P_RESULTADO con res_nl) y el barrido de CC */
static int res_nl, op_modo_cc;        /* P_RESULTADO viene de punto_op; con [P]  */
static SimDiag op_d;
static SimNewton op_nw;
static char res_aviso[64];            /* P_RESULTADO: el aviso de la última tecla */
static char res_titulo[64];           /* «En el cursor: R1 = 360,0 Ω» (del barrido) */
static Curva curv;
static int barr_origen, barr_curva_k = -1;   /* el trazador: el componente; -1 = el circuito */
static int rebarre;                   /* [=] en P_BARRIDO: la misma pantalla, otra ventana */
static unsigned barr_aj_firma;        /* de qué circuito es barr_aj: con otro, se olvida */
/* La P_RESULTADO de la que salió un barrido ([I]): [tab] en el cursor la pisa
 * con la del cursor, y [esc] en P_BARRIDO vuelve a ella, no a la del cursor */
static struct {
    int       hay;
    Circuito  c;
    Solucion  s;
    SimDiag   d;
    SimNewton nw;
    int       res_nl, op_modo_cc, origen, isol, capa, practica;
    char      titulo[64];
} res_guarda;
static Circuito barr_c0;              /* el circuito que se barre              */
static Red barr_red;
static BarrAjustes barr_aj;           /* el de la sesión, como el rango del Bode */
static void punto_op(int desde, int modo_cc);
static void barrido_de(int desde, const Circuito *c);
static void curva_de(int desde, const Circuito *c, int k);
static void fin_barrido(void);
static void barre_modelo(int m);
static void dib_resultado_nl(void);
static void tecla_resultado_nl(int t);
static const unsigned char *brillo_de(const Circuito *c, const double *I);
static void dib_led_linea(int y, const char *pre, const Circuito *c, const double *I);
static const char *reg_texto(int r);
static u16 reg_color(int r);
static int es_nl_ui(int tipo);
static int hay_suave_ui(const Circuito *c);
static int barrible_ui(int tipo);

/* modo práctica (ver practica_empieza) */
#define MAX_PREG 16
enum { PRAC_NO = 0, PRAC_TAPADO, PRAC_HECHO };
enum { PR_SIN = 0, PR_BIEN, PR_MAL, PR_VISTO };
typedef struct { unsigned char k, que, estado; double resp; } Pregunta;

static Pregunta preg[MAX_PREG];
static int  practica, npreg, sel_preg, pasos_vistos, resp_aviso;
static unsigned sol_mask;             /* soluciones que cuadran con TODO lo contestado */
static void practica_empieza(void);
static char resp[20];

/* aprende */
static int sel_lec, pag, sel_ejl, ej_actual;
static int lista_practica;            /* [?!] en los ejemplos: notas tapadas */

/* mis problemas */
static int  mis_hueco[ARCH_HUECOS], nmis, sel_mis, borrar_confirma;
static unsigned char mis_malo[ARCH_HUECOS];   /* existe pero no se puede abrir */
static char mis_nombre[ARCH_HUECOS][sizeof circ.nombre];

static int volver_ayuda;
static int mod_pend;                  /* shift/ctrl pulsados: van con la siguiente */
static int mods;                      /* los de la tecla que se está procesando */
static int previa = -1;               /* en la portada: la cifra de antes */
static unsigned vueltas;              /* teclas desde el arranque: semilla */

/* La capa de entrada (entrada.c): una ráfaga es una tecla mantenida (sus
 * T_REPE) o un gesto del dedo (sus pasos T_TACTIL), y acaba con T_SUELTA */
static int rep_act;                   /* T_REPE / T_TACTIL de la tecla que se procesa */
static int rep_ant;                   /* los de la anterior (0 tras T_SUELTA): un paso del dedo
                                         tras algo que no lo era empieza otro gesto */
static int mods_rafaga;               /* los modificadores del primer evento de la ráfaga */
static int raf_modo, raf_ok;          /* el modo tras el primer evento, y si no lo cambió */
static int hist_rafaga;               /* la ráfaga (o el gesto) ya gastó su nivel de deshacer */
static unsigned long ops_tick = SIM_OPS_TICK;   /* el trabajo de un ui_tick */
static int ctl_origen, ctl_campo;     /* el panel Controles: de dónde se vino y la fila */
static void cotas_vista(void);
static void dib_controles(void);
static void tecla_controles(int t);
static int  tb_probando(void);        /* la prueba del turbo, en marcha (Controles) */
static void tb_tick(void);

/* Capacitores equivalentes (Taller, 4.ª opción; capmotor.c): lo escrito en
 * los tres campos de texto (el valor, la tolerancia y la bodega; se recuerda
 * mientras dure la sesión) y el modo («Capacitores»: Auto, 2 o 3), el campo
 * elegido con su cursor, el último resultado con el paso a paso de la opción
 * elegida ya cortado en líneas de pantalla, la pestaña, y lo elegido en su
 * menú, en la Teoría y en los Ejemplos */
enum { CPM_F_VALOR = 0, CPM_F_TOL, CPM_F_BODEGA, CPM_F_NCAP, CPM_NF };   /* el orden de las flechas */
enum { CPM_AV_NADA = 0, CPM_AV_A, CPM_AV_B, CPM_AV_LLENO, CPM_AV_EJ, CPM_AV_MIO };
#define CPM_LIN  900                  /* líneas de pantalla del paso a paso   */
static char cpm_txt[3][CPM_TXT + 1];  /* el valor, la tolerancia y la bodega  */
static int  cpm_campo, cpm_nuevo;     /* nuevo: la primera tecla sustituye (valor y tolerancia) */
static int  cpm_pos, cpm_btop;        /* el cursor en el campo, y la primera línea de la caja de la bodega */
static int  cpm_modo;                 /* «Capacitores»: 0 (Auto), 2 o 3       */
static int  cpm_err, cpm_av;          /* el error de la última [enter] (CPM_E_*) y un aviso (CPM_AV_*) */
static CpmRes cpm_r;
static char cpm_proc[24576];          /* con 100 valores y sin solución, unos 12 KB */
static CpmLinea cpm_lin[CPM_LIN];
static int  cpm_nlin;                 /* sus líneas                           */
static int  cpm_sel;                  /* la opción elegida (la que se dibuja) */
static int  cpm_tab;                  /* la pestaña: 0 Opciones, 1 Paso a paso */
static int  cpm_lsc;                  /* la primera fila a la vista de la lista de opciones */
static int  cpm_msel, cpm_tsel, cpm_esel;   /* el menú, la página de la Teoría y el ejemplo */
static int  cpm_bpos;                 /* el cursor de la bodega al salir de ella: al volver, ahí */
static char cpm_mio[3][CPM_TXT + 1];  /* lo del usuario mientras están los datos de un ejemplo */
static int  cpm_mio_modo, cpm_hay_mio;   /* ...su modo, y si hay que devolvérselo (Resolver) */
#ifndef NDLESS
static int  cpm_oculto;               /* el arnés: las pruebas de antes de rev10 no lo ven (ui.h) */
#else
#define cpm_oculto 0
#endif
static void dib_cpm(void);
static void dib_cpm_res(void);
static void dib_cpm_dib(void);
static void dib_cpm_menu(void);
static void dib_cpm_teo(void);
static void dib_cpm_pag(void);
static void dib_cpm_ej(void);
static int  cpm_nopc(void);
static int  cpm_max(void);
static int  cpm_teo_max(void);
static void cpm_lista_vista(void);

/* --------------------------------------------------------------------------
 *  Utilidades de dibujo
 * ----------------------------------------------------------------------- */
/* Las marcas de arriba a la derecha, que se pintan tras la pantalla, de
 * derecha a izquierda: el turbo (su frecuencia, mientras está puesto), shift
 * o ctrl esperando tecla y el grabador fuera de su panel. Con dibuja = 0
 * solo mide: la x de la de más a la izquierda (SCR_W si no hay ninguna) */
static int marcas(int dibuja)
{
    int x = SCR_W - 2, izq = SCR_W, ms, m = turbo_mhz_marca();
    char t[16];
    Buf b;
    if (m) {                                /* el turbo: siempre a la vista (atascado, lo de verdad) */
        int w;
        buf_init(&b, t, sizeof t);
        buf_int(&b, m); buf_str(&b, " MHz");
        w = gfx_text_w(t, 1) + 6;
        izq = x -= w;
        if (dibuja) { gfx_fill(x, 1, w, 14, C_POTENCIA); gfx_text(x + 3, 2, t, C_FONDO, 1); }
        x -= 2;
    }
    if (mod_pend) {                         /* shift o ctrl esperando tecla */
        const char *s = mod_pend == T_CON_SHIFT ? "shift" : mod_pend == T_CON_CTRL ? "ctrl" : "shift+ctrl";
        int w = gfx_text_w(s, 1) + 6;
        izq = x -= w;
        if (dibuja) { gfx_fill(x, 1, w, 14, C_CURSOR); gfx_text(x + 3, 2, s, C_FONDO, 1); }
        x -= 2;
    }
    if (pant != P_CONTROLES && ent_grabacion(0, &ms) == GRAB_GRABANDO) {
        /* el grabador (Controles > [G]) sigue en cualquier pantalla: que
         * se vea, con los segundos que le quedan (el panel lo dice entero) */
        int w;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "grabando "); buf_int(&b, (ms + 999) / 1000); buf_str(&b, " s");
        w = gfx_text_w(t, 1) + 6;
        izq = x -= w;
        if (dibuja) { gfx_fill(x, 1, w, 14, C_ERROR); gfx_text(x + 3, 2, t, C_TEXTO, 1); }
    }
    return izq;
}

/* La barra de título. El título (el nombre de un modelo llega a 39) no pisa
 * lo de la derecha ni las marcas: si no cabe, se corta en una palabra y
 * lleva «...». Lo de la derecha va a la izquierda de las marcas */
static void barra(const char *txt, const char *dcha)
{
    int izq = marcas(0), der = izq < SCR_W ? izq - 4 : SCR_W - 5, wd = dcha ? gfx_text_w(dcha, 1) : 0;
    int cab = ((dcha ? der - wd - 6 : der) - 5) / FONT_W, n = (int)strlen(txt);
    gfx_fill(0, 0, SCR_W, BARRA_H, C_BARRA);
    if (n <= cab) gfx_text(5, 2, txt, C_TEXTO, 1);
    else {
        char t[64];
        int k = cab - 3 < (int)sizeof t - 4 ? cab - 3 : (int)sizeof t - 4, i;
        for (i = k; i > k / 2 && txt[i] != ' '; i--) {}   /* en un espacio, si lo hay cerca */
        if (i > k / 2) k = i;
        while (k > 0 && (txt[k - 1] == ' ' || txt[k - 1] == ':' || txt[k - 1] == ',')) k--;
        if (k < 0) k = 0;
        memcpy(t, txt, (size_t)k);
        memcpy(t + k, "...", 4);
        gfx_text(5, 2, t, C_TEXTO, 1);
    }
    if (dcha) gfx_text(der - wd, 2, dcha, C_CURSOR, 1);
}

static void pie(const char *txt)
{
    gfx_fill(0, PIE_Y, SCR_W, SCR_H - PIE_Y, C_PANEL);
    gfx_text(5, PIE_Y + 1, txt, C_SUAVE, 1);
}

/* La primera línea de s (hasta maxch, cortada en un espacio o en '\n') en
 * lin (64); devuelve lo que sigue, sin los espacios del corte */
static const char *primera_linea(const char *s, int maxch, char *lin)
{
    int n = 0, corte = 0;
    if (maxch > 62) maxch = 62;
    while (s[n] && s[n] != '\n' && n < maxch) { if (s[n] == ' ') corte = n; n++; }
    if (s[n] && s[n] != '\n' && corte > 0) n = corte;
    memcpy(lin, s, (size_t)n);
    lin[n] = 0;
    s += n;
    if (*s == '\n') s++;
    else while (*s == ' ') s++;
    return s;
}

/* Texto con corte por palabras y saltos de línea explícitos. Devuelve las
 * líneas dibujadas. El texto está en Latin-1: un byte, una columna. */
static int envuelve(int x, int y, int maxch, const char *s, u16 c, int maxlin)
{
    char lin[64];
    int nl = 0;

    if (!s || !*s) return 0;
    while (*s && nl < maxlin) {
        s = primera_linea(s, maxch, lin);
        gfx_text(x, y + nl * LIN_H, lin, c, 1);
        nl++;
    }
    return nl;
}

/* Las líneas que ocuparía s con envuelve, sin tope */
static int lineas_envuelve(const char *s, int maxch)
{
    char lin[64];
    int nl = 0;
    while (s && *s) { s = primera_linea(s, maxch, lin); nl++; }
    return nl;
}

/* Lista con selección; 'desp' es la primera visible */
static void lista(int y0, int visibles, const char *const *items, int n, int sel, int desp)
{
    int i;
    for (i = 0; i < visibles && desp + i < n; i++) {
        int k = desp + i, y = y0 + i * LIN_H;
        if (k == sel) {
            gfx_fill(0, y - 1, SCR_W, LIN_H, C_BARRA);
            gfx_text(4, y, S_RARR, C_CURSOR, 1);
        }
        gfx_text(16, y, items[k], k == sel ? C_TEXTO : C_COTA, 1);
    }
    if (n > visibles) {                       /* hay más: barrita a la derecha */
        int alto = visibles * LIN_H;
        int h = alto * visibles / n, p = alto * desp / n;
        gfx_fill(SCR_W - 3, y0 + p, 2, h < 6 ? 6 : h, C_SUAVE);
    }
}

static int desplaza(int sel, int desp, int visibles)
{
    if (sel < desp) desp = sel;
    if (sel >= desp + visibles) desp = sel - visibles + 1;
    return desp < 0 ? 0 : desp;
}

static void separador(int y) { gfx_fill(0, y, SCR_W, 1, C_TENUE); }

/* La unidad del valor: la fila de su variante (β de una CCCS sin unidad, r de
 * una CCVS en Ω); en los tipos de una sola fila, la de siempre */
static const char *unidad_val(const Elemento *e)
{
    const CampoInfo *ci = tipo_campo_v(e->tipo, e->variante, Q_VAL);
    return ci ? ci->unidad : tipo_uni_val(e->tipo);
}

static void pon_aviso(const char *s, u16 col)
{
    Buf b;
    buf_init(&b, aviso, sizeof aviso);
    buf_str(&b, s);
    aviso_col = col;
    aviso_bajo = 0;
}

/* Un circuito en miniatura dentro de un rectángulo */
static void miniatura(const Circuito *c, int x, int y, int w, int h)
{
    Marco m;
    Vista v;
    esq_convenio(c->conv);
    esq_marco_ajustado(c, x, y, w, h, &m);
    esq_vista(&v);
    gfx_clip(x, y, w, h);
    esq_dibuja(c, &m, &v);
    gfx_clip_reset();
}

/* --------------------------------------------------------------------------
 *  Figuras de la teoría
 * ----------------------------------------------------------------------- */
/* Triángulo con una magnitud arriba y dos abajo, y las tres fórmulas */
static void fig_triangulo(int y, const char *arr, const char *izq, const char *dch,
                          const char *f1, const char *f2, const char *f3)
{
    int ax = 84, ay = y + 4, by = y + 80, bx0 = 30, bx1 = 138;
    int my = (ay + by) / 2, hw = (bx1 - bx0) / 2 * (my - ay) / (by - ay);
    gfx_line_w(ax, ay, bx0, by, C_TITULO, 2);
    gfx_line_w(ax, ay, bx1, by, C_TITULO, 2);
    gfx_line_w(bx0, by, bx1, by, C_TITULO, 2);
    gfx_hline(ax - hw, my, 2 * hw + 1, C_TITULO);
    gfx_vline(ax, my, by - my, C_TITULO);
    gfx_text_c(ax + 1, my - 26, arr, C_FUENTE, 2);
    gfx_text_c(ax - 22, my + 8, izq, C_CORRIENTE, 2);
    gfx_text_c(ax + 24, my + 8, dch, C_RESIST, 2);
    gfx_text(176, y + 14, f1, C_TEXTO, 1);
    gfx_text(176, y + 36, f2, C_TEXTO, 1);
    gfx_text(176, y + 58, f3, C_TEXTO, 1);
}

static void flecha_fig(int x0, int y0, int x1, int y1, u16 c)
{
    int dx = x1 - x0, dy = y1 - y0;
    int l = (int)sqrt((double)(dx * dx + dy * dy));
    int ux, uy;
    if (l == 0) return;
    ux = dx * 7 / l; uy = dy * 7 / l;
    gfx_line_w(x0, y0, x1, y1, c, 2);
    gfx_tri(x1, y1, x1 - ux - uy / 2, y1 - uy + ux / 2, x1 - ux + uy / 2, y1 - uy - ux / 2, c);
}

static void fig_nodo(int y)
{
    int nx = 104, ny = y + 44;
    flecha_fig(30, ny, nx - 6, ny, C_CORRIENTE);
    flecha_fig(nx + 5, ny - 4, 150, y + 12, C_CORRIENTE);
    flecha_fig(nx + 5, ny + 4, 150, y + 76, C_CORRIENTE);
    gfx_circle(nx, ny, 4, C_TEXTO, 1);
    gfx_text(40, ny - 16, "I" S_SUB1, C_CORRIENTE, 1);
    gfx_text(154, y + 6, "I" S_SUB2, C_CORRIENTE, 1);
    gfx_text(154, y + 70, "I" S_SUB3, C_CORRIENTE, 1);
    gfx_text(186, y + 30, "I" S_SUB1 " = I" S_SUB2 " + I" S_SUB3, C_TEXTO, 1);
    gfx_text(186, y + 48, "entra = sale", C_SUAVE, 1);
}

static void dib_figura(const Pagina *p)
{
    int y = BARRA_H + 4;
    switch (p->fig) {
    case FIG_TRIANGULO:
        fig_triangulo(y, "V", "I", "R", "V = I " S_CDOT " R", "I = V / R", "R = V / I");
        break;
    case FIG_POTENCIA:
        fig_triangulo(y, "P", "V", "I", "P = V " S_CDOT " I", "V = P / I", "I = P / V");
        break;
    case FIG_NODO:
        fig_nodo(y);
        break;
    case FIG_CIRCUITO:
        if (p->fig_ej >= 0) {
            static Circuito c;
            ejemplo_cargar(p->fig_ej, &c);
            miniatura(&c, 0, y, SCR_W, FIG_ALTO);
        }
        break;
    }
    if (p->fig != FIG_NADA) separador(y + FIG_ALTO);
}

/* --------------------------------------------------------------------------
 *  Deshacer
 * ----------------------------------------------------------------------- */
/* Un nivel más en una pila; llena, se pierde el más viejo */
static void apila(Nivel *p, int *n, const Circuito *c, int h, int doc)
{
    int i;
    if (*n == HIST) { for (i = 1; i < HIST; i++) p[i - 1] = p[i]; (*n)--; }
    p[*n].c = *c;
    p[*n].hueco = h;
    p[*n].doc = doc;
    (*n)++;
}

/* Un cambio nuevo: el circuito de antes (c) se puede deshacer, y lo que se
 * había deshecho ya no se puede rehacer */
static void guarda_hist_de(const Circuito *c)
{
    apila(hist, &nhist, c, hueco, doc_act);
    nreh = 0;
}

static void guarda_hist(void) { guarda_hist_de(&circ); }

/* Vuelve al nivel de arriba de la pila p; el circuito de ahora pasa a la
 * otra (o) */
static void recupera(Nivel *p, int *n, Nivel *o, int *no)
{
    apila(o, no, &circ, hueco, doc_act);
    (*n)--;
    circ = p[*n].c;
    hueco = p[*n].hueco;
    doc_act = p[*n].doc;
    sel_el = -1;              /* los índices pueden haber cambiado */
}

static void deshacer(void)
{
    char t[48];
    Buf b;
    if (nhist == 0) { pon_aviso("No hay nada que deshacer.", C_SUAVE); return; }
    recupera(hist, &nhist, reh, &nreh);
    buf_init(&b, t, sizeof t);                /* «Deshecho (quedan 5). [ctrl][Y] lo rehace.» */
    buf_str(&b, nhist == 1 ? "Deshecho (queda " : "Deshecho (quedan ");
    buf_int(&b, nhist);
    buf_str(&b, "). [ctrl][Y] lo rehace.");
    pon_aviso(t, C_OK);
}

static void rehacer(void)
{
    if (nreh == 0) { pon_aviso("Nada que rehacer.", C_SUAVE); return; }
    recupera(reh, &nreh, hist, &nhist);
    pon_aviso("Rehecho.", C_OK);
}

/* guardar() dio al documento un hueco (y quizá un nombre): sus niveles de
 * deshacer y rehacer los toman también. Si no, deshacer volvería a «Problema
 * nuevo» sin hueco y el siguiente Guardar crearía otro archivo */
static void doc_renombra(void)
{
    int i;
    for (i = 0; i < nhist; i++)
        if (hist[i].doc == doc_act) { hist[i].hueco = hueco; memcpy(hist[i].c.nombre, circ.nombre, sizeof circ.nombre); }
    for (i = 0; i < nreh; i++)
        if (reh[i].doc == doc_act) { reh[i].hueco = hueco; memcpy(reh[i].c.nombre, circ.nombre, sizeof circ.nombre); }
}

/* Se borró en Mis problemas el problema del hueco h: nadie lo tiene ya, ni el
 * circuito del editor ni sus niveles de deshacer y rehacer. Si no, otro
 * problema que ocupara después ese hueco quedaría pisado al deshacer hasta el
 * de antes y pulsar Guardar (que irá, en cambio, a un hueco nuevo) */
static void olvida_hueco(int h)
{
    int i;
    if (hueco == h) hueco = -1;
    for (i = 0; i < nhist; i++) if (hist[i].hueco == h) hist[i].hueco = -1;
    for (i = 0; i < nreh; i++) if (reh[i].hueco == h) reh[i].hueco = -1;
}

/* Se sale de la ficha (al editor, a la de otro elemento, a [=] o a deshacer).
 * Si era la de un elemento que ya estaba y algo cambió, el circuito de antes
 * de abrirla se puede deshacer; si solo se miró, no se gasta un nivel */
static void sal_ficha(void)
{
    if (ficha_pend && !circ_igual(&circ, &ficha_antes)) guarda_hist_de(&ficha_antes);
    ficha_pend = 0;
}

/* --------------------------------------------------------------------------
 *  Estado del problema: qué se pide y si los datos alcanzan
 * ----------------------------------------------------------------------- */
static void nombre_mag(Buf *b, const Elemento *e, int que)
{
    /* el valor de R, E o J se nombra con la etiqueta; el de un instrumento
     * es su R interna, y "V1" se leería como una tensión; la ganancia de una
     * controlada, con el nombre de su fila: μ(E2), r(E2), g(J2), β(J2) */
    if (que == Q_VAL && tipo_info(e->tipo)->ley_cc == LEY_DEP) {
        const CampoInfo *ci = tipo_campo_v(e->tipo, e->variante, Q_VAL);
        buf_str(b, ci ? ci->nombre : "g"); buf_ch(b, '('); buf_str(b, e->etiq); buf_ch(b, ')');
        return;
    }
    if (que == Q_VAL && !tipo_info(e->tipo)->ideal && e->tipo != EL_VATI) { buf_str(b, e->etiq); return; }
    if (e->tipo == EL_VATI) buf_str(b, que == Q_AUX ? "fp(" : "P(");
    else buf_str(b, que == Q_VAL ? "R(" : que == Q_V ? "V(" : que == Q_I ? "I(" : "r(");
    buf_str(b, e->etiq);
    buf_ch(b, ')');
}

static const Mag *mag_de(const Elemento *e, int que)
{
    return circ_mag_c(e, que);
}

/* Unidad de una magnitud: la de su campo en la tabla de tipos */
static const char *unidad_q(const Elemento *e, int que)
{
    /* la fila de su variante (la ganancia de una CCVS es en ohmios); en los
     * tipos de antes es la misma que la primera */
    const CampoInfo *ci = tipo_campo_v(e->tipo, e->variante, que);
    if (!ci) ci = tipo_campo(e->tipo, que);
    if (ci) return ci->unidad;
    return que == Q_VAL ? unidad_val(e) : que == Q_V ? "V" : que == Q_I ? "A" : S_OHM;
}

/* La magnitud que de un elemento en una solución */
static double valor_de(const Valores *v, int k, int que)
{
    return que == Q_VAL ? v->val[k] : que == Q_V ? v->V[k] : que == Q_I ? v->I[k] : v->aux[k];
}

/* El valor de una fuente lleva signo (una E negativa tiene el + al otro
 * lado del dibujado); lo demás se da en magnitud */
static int con_signo(const Elemento *e, int que)
{
    const CampoInfo *ci = tipo_campo_v(e->tipo, e->variante, que);
    return (que == Q_VAL && tipo_info(e->tipo)->val_min < 0.0) || (ci && (ci->flags & CF_SIGNO));
}

/* Lo que se pregunta de un elemento: una magnitud que está en su ficha y
 * marcada con [?!] (la V de una fuente ideal es su E: no va aparte). */
static int pregunta(const Elemento *e, int que)
{
    return mag_de(e, que)->est == M_BUSCAR && tipo_campo(e->tipo, que) != 0;
}

/* La línea de estado de la Etapa 4, delante de la de hoy (§4.1): lo que
 * decide igual() con un no lineal y los controles de las controladas. 0 si no
 * tiene nada que decir (un circuito lineal con sus controles bien sigue por la
 * de hoy); si no, 1 y el color en *col. Los textos caben en 52 columnas con
 * nombres de 3 caracteres. */
static int estado_e4(Buf *b, u16 *col)
{
    int k;
    *col = C_ERROR;
    if (circ_no_lineal(&circ) && !circ_alterna(&circ) && circ_busca_valor(&circ)) {
        /* un [?!] en un valor: la calculadora es lineal (SOL_NO_LINEAL). Con
         * una fuente alterna, igual() emula: lo dice sim_valida, más abajo */
        int j;
        for (k = 0; k < circ.nel - 1 && tipo_info(circ.el[k].tipo)->ley_cc != LEY_NL; k++) {}
        buf_str(b, "Con "); buf_str(b, circ.el[k].etiq); buf_str(b, " no se despeja: ");
        /* [I] halla lo que se barre (como el pie de P_RESULTADO); un [?!] en
         * otra cosa (una C, la r de una fuente) solo se puede quitar */
        for (j = 0; j < circ.nel && !(barrible_ui(circ.el[j].tipo) && circ.el[j].val.est == M_BUSCAR); j++) ;
        if (j < circ.nel) { buf_str(b, "[I] barre y [N] lee"); return 1; }
        for (j = 0; j < circ.nel - 1 && circ.el[j].val.est != M_BUSCAR && circ.el[j].aux.est != M_BUSCAR; j++) ;
        buf_str(b, "quita el [?!] de "); buf_str(b, circ.el[j].etiq);
        return 1;
    }
    for (k = 0; k < circ.nel; k++) {
        int m;
        if (tipo_info(circ.el[k].tipo)->ley_cc != LEY_DEP || (m = circ_ctl_motivo(&circ, k)) == CTL_OK) continue;
        if (m == CTL_FALTA) { buf_str(b, circ.el[k].etiq); buf_str(b, " no tiene control: el" S_ii "gelo en su ficha."); }
        else { buf_str(b, "El control de "); buf_str(b, circ.el[k].etiq); buf_str(b, " no vale: mira su ficha."); }
        return 1;
    }
    if (!circ_no_lineal(&circ)) return 0;
    {
        /* la salida de un AO se mide desde la tierra o el - de una fuente, y
         * no puede ser ese mismo nodo (AOR_CORTO, como sim_red_de) */
        static Red r;
        red_extraer(&circ, &r);
        for (k = 0; k < circ.nel; k++) {
            if (circ.el[k].tipo != EL_AO) continue;
            if (!red_ref_propia(&circ, &r, r.c[k])) {
                /* con la tierra en otra parte, [G] la quitaría de allí: que lo una */
                buf_str(b, circ.el[k].etiq);
                buf_str(b, circ.tierra_x >= 0 ? " no llega a la tierra ni a una fuente: " S_uu "nelo." :
                                                " sin referencia: pon la tierra con [G].");
                return 1;
            }
            if (red_ref_de(&r, r.c[k]) == r.c[k]) {
                buf_str(b, "La salida de "); buf_str(b, circ.el[k].etiq);
                buf_str(b, " est" S_aa " en su referencia: mu" S_ee "vela.");
                return 1;
            }
        }
    }
    {
        /* [=] emula (dinámico o alterno) o da el punto de operación */
        SimDiag d;
        int emula = circ_dinamico(&circ) || circ_alterna(&circ);
        if (sim_valida(&circ, &d) != SIM_OK && d.elem >= 0 && d.elem < circ.nel) {
            const Elemento *e = &circ.el[d.elem];
            if (d.estado == SIM_FALTA) {
                const CampoInfo *ci = tipo_campo_v(e->tipo, e->variante, d.campo);
                buf_str(b, emula ? "Para emular falta " : "Para el punto de operaci" S_oo "n falta ");
                buf_str(b, ci ? ci->nombre : "un valor");
                buf_str(b, " en "); buf_str(b, e->etiq); buf_ch(b, '.');
            } else if (d.estado == SIM_MODELO) {
                buf_str(b, e->etiq); buf_str(b, ": su modelo no cuadra: [=] dice por qu" S_ee ".");
            } else { buf_str(b, "Algo no cuadra en "); buf_str(b, e->etiq); buf_str(b, ": [=] dice qu" S_ee "."); }
            return 1;
        }
        *col = C_OK;
        if (!emula) { buf_str(b, "Listo: [=] da el punto de operaci" S_oo "n"); return 1; }
        buf_str(b, "Listo: pulsa [=] para emular");
        if (circ.sonda[0].tipo == SONDA_NO && circ.sonda[1].tipo == SONDA_NO) buf_str(b, " (canales auto)");
        return 1;
    }
}

/* Devuelve el color: verde si está bien planteado, rojo si faltan datos,
 * amarillo si sobran o no se pide nada. */
static u16 estado_problema(char *buf, size_t cap)
{
    Buf b;
    int k, que, ndesc, ndatos = 0, npide = 0, nf = 0;
    u16 c4;

    buf_init(&b, buf, cap);
    ndesc = sol_desconocidos(&circ, 0);     /* la misma regla que el motor */
    if (circ.nel == 0) {
        buf_str(&b, "Vacío: empieza con [R], [E] o [J] y una flecha.");
        return C_SUAVE;
    }
    if (circ_etapa4(&circ) && estado_e4(&b, &c4)) return c4;
    if (circ_alterna(&circ) && !circ_pide(&circ)) {
        /* la alterna: ¿se puede analizar? */
        SimDiag d;
        double f;
        char u[24];
        if (ca_valida(&circ, &d, &f) != SIM_OK) {
            const Elemento *e = (d.elem >= 0 && d.elem < circ.nel) ? &circ.el[d.elem] : 0;
            if (d.estado == SIM_FALTA && d.campo == Q_FCIRC && e) {
                buf_str(&b, "Para CA falta f: ponla en la ficha de "); buf_str(&b, e->etiq); buf_ch(&b, '.');
            } else if (d.estado == SIM_FALTA && e) {
                const CampoInfo *ci = tipo_campo_v(e->tipo, e->variante, d.campo);
                buf_str(&b, "Falta "); buf_str(&b, d.campo == Q_VAL ? "el valor" : ci ? ci->nombre : "un valor");
                buf_str(&b, " de "); buf_str(&b, e->etiq); buf_ch(&b, '.');
            } else if (d.estado == SIM_CA_FORMA && e) {
                buf_str(&b, e->etiq); buf_str(&b, " es "); buf_str(&b, NOM_FORMA[e->variante < FORMA_N ? e->variante : 0]);
                buf_str(&b, ": en CA solo senoidales.");
            } else if (d.estado == SIM_CA_FRECS) buf_str(&b, "Hay fuentes de frecuencias distintas: en CA, una.");
            else if (d.estado == SIM_CA_SIN_F) buf_str(&b, "Para CA falta f: ponla en [menu] > Ajustes de CA.");
            else buf_str(&b, "Para CA falta un dato.");
            return C_ERROR;
        }
        buf_str(&b, "Listo: [=] analiza en CA a ");
        buf_str(&b, num_pref(u, sizeof u, f, 4, " Hz"));
        return C_OK;
    }
    if (circ_alterna(&circ)) {
        /* la calculadora de CA: incógnitas y ecuaciones reales */
        static CalRes pl;
        int st = cal_plantea(&circ, &pl), corto = 0;
        const Elemento *e = (pl.elem >= 0 && pl.elem < circ.nel) ? &circ.el[pl.elem] : 0;
        if (st == CAL_SIM) {
            if (pl.d.estado == SIM_FALTA && pl.d.campo == Q_FCIRC) buf_str(&b, "Para CA falta f: ponla, o b" S_uu "scala con [?!].");
            else if (pl.d.estado == SIM_CA_FORMA && pl.d.elem >= 0 && pl.d.elem < circ.nel) {
                buf_str(&b, circ.el[pl.d.elem].etiq); buf_str(&b, " no es senoidal: en CA solo senoidales.");
            } else if (pl.d.estado == SIM_CA_FRECS) buf_str(&b, "Hay fuentes de frecuencias distintas: en CA, una.");
            else if (pl.d.estado == SIM_FALTA && pl.d.elem >= 0 && pl.d.elem < circ.nel) {
                buf_str(&b, "Falta un dato en "); buf_str(&b, circ.el[pl.d.elem].etiq); buf_ch(&b, '.');
            } else buf_str(&b, "Algo no cuadra: [=] dice qu" S_ee " (CA).");
            return C_ERROR;
        }
        if (st == CAL_NO_CA) {
            if (e && (e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante == ONDA_SENO) {
                buf_str(&b, "La f la fija "); buf_str(&b, e->etiq); buf_str(&b, ": no se busca.");
            } else if (e && (e->tipo == EL_EDEP || e->tipo == EL_JDEP)) {
                buf_str(&b, "En CA no se despeja la ganancia de "); buf_str(&b, e->etiq); buf_ch(&b, '.');
            } else { buf_str(&b, "En CA no se despeja la r o la R de "); buf_str(&b, e ? e->etiq : "eso"); buf_ch(&b, '.'); }
            return C_ERROR;
        }
        {
            char suf[40], nom[16];
            Buf bs;
            u16 col;
            int max;
            buf_init(&bs, suf, sizeof suf);
            if (st == CAL_MUCHOS) {
                buf_str(&bs, pl.ninc > CAL_MAX_INC ? "m" S_aa "s de 4 inc" S_oo "gnitas" : "m" S_aa "s de 8 datos");
                col = C_ERROR;
            } else if (st == CAL_FALTAN) {
                int n = pl.ninc - pl.necu;
                if (n == 1) buf_str(&bs, "falta 1 dato (CA)");
                else { buf_str(&bs, "faltan "); buf_int(&bs, n); buf_str(&bs, " datos (CA)"); }
                col = C_ERROR;
            } else if (pl.necu > pl.ninc) {
                int n = pl.necu - pl.ninc;
                if (pl.ninc == 0) buf_str(&bs, "solo se comprueba");
                else if (n == 1) buf_str(&bs, "sobra 1 (se comprueba)");
                else { buf_str(&bs, "sobran "); buf_int(&bs, n); buf_str(&bs, " (se comprueban)"); }
                col = C_CURSOR;
            } else { buf_str(&bs, "listo " S_CHECK " pulsa [=] (CA)"); col = C_OK; }
            /* «Pides » + lista + «: » + cola en 52 columnas; «...» si no cabe */
            max = COLS - 6 - 2 - (int)bs.n - 3;
            buf_str(&b, "Pides ");
            for (k = 0; k <= circ.nel; k++)
                for (que = 0; que < Q_N; que++) {
                    Buf bn;
                    if (k == circ.nel) {                       /* la f del circuito, al final */
                        if (que > 0 || circ.frec.est != M_BUSCAR) continue;
                        buf_init(&bn, nom, sizeof nom); buf_ch(&bn, 'f');
                    } else {
                        const Mag *m = mag_de(&circ.el[k], que);
                        if (m->est != M_BUSCAR && m->est_ang != M_BUSCAR) continue;
                        buf_init(&bn, nom, sizeof nom);
                        if (m->est != M_BUSCAR) buf_str(&bn, S_ANG);
                        nombre_mag(&bn, &circ.el[k], que);
                    }
                    npide++;
                    if (corto) continue;
                    if ((int)(b.n - 6 + (npide > 1) + bn.n) > max) { buf_str(&b, "..."); corto = 1; continue; }
                    if (npide > 1) buf_ch(&b, ',');
                    buf_str(&b, nom);
                }
            buf_str(&b, ": ");
            buf_str(&b, suf);
            return col;
        }
    }
    if (circ_pide(&circ)) {
        /* la calculadora es de CC: con [?!], las fuentes tienen que ser continuas */
        for (k = 0; k < circ.nel; k++) {
            const Elemento *e = &circ.el[k];
            if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante != ONDA_CC) {
                buf_str(&b, e->etiq);
                buf_str(&b, " no es continua: quita los [?!] para emular.");
                return C_ERROR;
            }
        }
    } else if (circ_dinamico(&circ)) {
        /* se emula: ¿tiene todo lo que hace falta? */
        SimDiag d;
        if (sim_valida(&circ, &d) != SIM_OK && d.elem >= 0 && d.elem < circ.nel) {
            const Elemento *e = &circ.el[d.elem];
            const CampoInfo *ci = tipo_campo_v(e->tipo, e->variante, d.campo);
            buf_str(&b, "Para emular falta ");
            buf_str(&b, ci ? ci->nombre : "un valor");
            buf_str(&b, " en ");
            buf_str(&b, e->etiq);
            buf_ch(&b, '.');
            return C_ERROR;
        }
        buf_str(&b, "Listo: pulsa [=] para emular");
        if (circ.sonda[0].tipo == SONDA_NO && circ.sonda[1].tipo == SONDA_NO) buf_str(&b, " (canales auto)");
        return C_OK;
    }
    for (k = 0; k < circ.nel; k++) {
        const Elemento *e = &circ.el[k];
        if (tipo_info(e->tipo)->fuente) nf++;
        if (e->V.est == M_DATO) ndatos++;
        if (e->I.est == M_DATO) ndatos++;
    }
    if (nf == 0) { buf_str(&b, "Falta una fuente: ponla con [E] o [J]."); return C_ERROR; }

    buf_str(&b, "Pides ");
    {
        int corto = 0;                  /* la lista se corta a los 16 caracteres */
        for (k = 0; k < circ.nel; k++)
            for (que = 0; que < Q_N; que++) {
                if (mag_de(&circ.el[k], que)->est != M_BUSCAR) continue;
                npide++;
                if (corto) continue;
                if (b.n >= 16) { buf_str(&b, "..."); corto = 1; continue; }
                if (npide > 1) buf_ch(&b, ',');
                nombre_mag(&b, &circ.el[k], que);
            }
    }
    if (npide == 0) {
        buf_init(&b, buf, cap);
        buf_str(&b, "Marca con [?!] en la ficha lo que buscas.");
        return C_CURSOR;
    }
    buf_str(&b, ": ");

    /* La regla: tantos datos de V o I como valores de R o E desconocidos */
    if (ndatos < ndesc) {
        if (ndesc - ndatos == 1) buf_str(&b, "falta 1 dato de V o I");
        else { buf_str(&b, "faltan "); buf_int(&b, ndesc - ndatos); buf_str(&b, " datos de V o I"); }
        return C_ERROR;
    }
    if (ndatos > ndesc) {
        buf_str(&b, ndatos - ndesc == 1 ? "sobra 1 dato" : "sobran datos");
        return C_CURSOR;
    }
    buf_str(&b, "listo " S_CHECK " pulsa [=]");
    return C_OK;
}

/* Resumen de datos e incógnitas, para "Mis problemas" */
static void resumen(const Circuito *c, char *buf, size_t cap)
{
    Buf b;
    int k, que, n = 0, ca = circ_alterna(c);
    buf_init(&b, buf, cap);
    if (circ_no_lineal(c)) {                  /* lo que hace igual() con un no lineal */
        if (ca || (circ_dinamico(c) && !circ_busca_valor(c))) buf_str(&b, "Se emula. ");
        else if (!circ_busca_valor(c)) buf_str(&b, "Se da el punto de operaci" S_oo "n. ");
    }
    else if (ca && !circ_pide(c)) buf_str(&b, "Se analiza en CA. ");
    else if (circ_dinamico(c) && !circ_pide(c)) buf_str(&b, "Se emula. ");
    buf_str(&b, "Datos: ");
    for (k = 0; k < c->nel; k++)
        for (que = 0; que < Q_N; que++) {
            const Mag *m = mag_de(&c->el[k], que);
            const CampoInfo *ci = circ_campo(&c->el[k], que);
            if (m->est != M_DATO) continue;
            if (n++) buf_str(&b, ", ");
            nombre_mag(&b, &c->el[k], que);
            buf_str(&b, " = ");
            if (ca && ci && (ci->flags & CF_ANG) && m->est_ang == M_DATO) {
                /* un fasor, en el convenio del circuito: "V(R1) = 72∠30° V" */
                double mo = m->v, an = m->ang;
                if (ci->flags & CF_AMPL) ca_canon_a_conv(c->conv, m->v, m->ang, &mo, &an);
                buf_sig(&b, mo); buf_str(&b, S_ANG); buf_sig(&b, an); buf_str(&b, S_DEG);
            } else buf_sig(&b, con_signo(&c->el[k], que) ? m->v : fabs(m->v));
            buf_ch(&b, ' ');
            buf_str(&b, unidad_q(&c->el[k], que));
        }
    if (ca && c->frec.est == M_DATO) {
        if (n++) buf_str(&b, ", ");
        buf_str(&b, "f = "); buf_sig(&b, c->frec.v); buf_str(&b, " Hz");
    }
    if (!n) buf_str(&b, "ninguno");
    buf_str(&b, ".\nPides: ");
    n = 0;
    for (k = 0; k < c->nel; k++)
        for (que = 0; que < Q_N; que++) {
            const Mag *m = mag_de(&c->el[k], que);
            if (m->est != M_BUSCAR && !(ca && m->est_ang == M_BUSCAR)) continue;
            if (n++) buf_str(&b, ", ");
            if (m->est != M_BUSCAR) buf_str(&b, S_ANG);            /* solo el ángulo */
            nombre_mag(&b, &c->el[k], que);
        }
    if (ca && c->frec.est == M_BUSCAR) { if (n++) buf_str(&b, ", "); buf_ch(&b, 'f'); }
    if (!n) buf_str(&b, "todo");
    buf_ch(&b, '.');
}

/* --------------------------------------------------------------------------
 *  Portada y menú principal
 * ----------------------------------------------------------------------- */
/* El kanji 測る de la portada: una imagen RGB565 ya mezclada con C_FONDO
 * (kanji_img.c, de herramientas/gen_kanji.py). Basta con copiar las filas. */
static void dib_kanji(int x, int y)
{
    u16 *fb = gfx_buffer();
    int j;
    if (x < 0 || y < 0 || x + KANJI_W > SCR_W || y + KANJI_H > SCR_H) return;
    for (j = 0; j < KANJI_H; j++)
        memcpy(fb + (y + j) * SCR_W + x, KANJI_PIX + j * KANJI_W, (size_t)KANJI_W * sizeof *fb);
}

#define TIT_Y 12                        /* el título x3: mayúsculas de y+6 a y+29 */

static void dib_inicio(void)
{
    static Circuito muestra;
    static int listo = 0;
    /* el título sin la columna vacía de su última letra, un hueco y el kanji,
     * centrado a la altura de las mayúsculas */
    int tw = gfx_text_w(APP_TITULO, 3) - 3, x = (SCR_W - tw - 10 - KANJI_W) / 2;

    if (!listo) {            /* el emisor común de la biblioteca: lo que la app es hoy */
        static const char pref[] = "Emisor com";
        int i;
        for (i = 0; i < N_MODELOS; i++)
            if (!strncmp(MODELOS[i].titulo, pref, sizeof pref - 1)) break;
        if (i < N_MODELOS) modelo_cargar(i, &muestra);
        else circ_init(&muestra, "");          /* si no estuviera: sin miniatura */
        listo = 1;
    }

    gfx_text(x, TIT_Y, APP_TITULO, C_FUENTE, 3);
    dib_kanji(x + tw + 10, TIT_Y + 6 + (24 - KANJI_H) / 2);
    gfx_text_c(SCR_W / 2, 50, APP_SIGLA " " S_CDOT " " APP_SIGLA_L, C_TITULO, 1);
    gfx_text_c(SCR_W / 2, 64, "Emulador y calculadora de circuitos de CC y CA", C_COTA, 1);
    if (muestra.nel > 0) miniatura(&muestra, 40, 80, 240, 102);
    gfx_text_c(SCR_W / 2, 188, "Dibuja, emula y mide como en el laboratorio;", C_TEXTO, 1);
    gfx_text_c(SCR_W / 2, 202, "despeja lo que buscas, paso a paso.", C_TEXTO, 1);
    pie("[enter] empezar                        [esc] salir");
}

static const char *const TARJ_T[3] = { "Taller", "Aprende", "Ayuda" };
static const char *const TARJ_S[3] = {
    "Dibuja, emula, mide y despeja",
    "Lecciones y circuitos modelo para emular",
    "Cómo se usa " APP_NOMBRE
};

static void icono_tarjeta(int i, int x, int y, u16 c)
{
    if (i == 0) {                                    /* un circuito mínimo */
        int k;
        gfx_hline(x + 4, y + 4, 24, c);              /* lazo */
        gfx_hline(x + 4, y + 30, 24, c);
        gfx_vline(x + 4, y + 4, 8, c);
        gfx_vline(x + 4, y + 21, 10, c);
        gfx_vline(x + 28, y + 4, 6, c);
        gfx_vline(x + 28, y + 25, 6, c);
        gfx_hline(x - 2, y + 13, 13, C_FUENTE);      /* pila: placa larga + */
        gfx_hline(x + 1, y + 18, 7, C_FUENTE);       /* y placa corta - */
        gfx_hline(x + 1, y + 19, 7, C_FUENTE);
        for (k = 0; k < 5; k++)                      /* resistencia en zigzag */
            gfx_line(x + 28 + ((k & 1) ? 4 : -4), y + 10 + k * 3,
                     x + 28 + ((k & 1) ? -4 : 4), y + 13 + k * 3, C_RESIST);
    } else if (i == 1) {                             /* un libro abierto */
        gfx_rect(x + 2, y + 6, 14, 22, c);
        gfx_rect(x + 16, y + 6, 14, 22, c);
        gfx_hline(x + 5, y + 12, 8, c); gfx_hline(x + 5, y + 16, 8, c); gfx_hline(x + 5, y + 20, 6, c);
        gfx_hline(x + 19, y + 12, 8, c); gfx_hline(x + 19, y + 16, 8, c); gfx_hline(x + 19, y + 20, 6, c);
    } else {
        gfx_text(x + 6, y + 4, "?", c, 2);
    }
}

static void dib_menu(void)
{
    int i;
    barra(APP_NOMBRE, 0);
    for (i = 0; i < 3; i++) {
        int y = BARRA_H + 8 + i * 66, sel = (i == sel_menu);
        gfx_fill(8, y, SCR_W - 16, 60, sel ? C_BARRA : C_PANEL);
        gfx_rect(8, y, SCR_W - 16, 60, sel ? C_CURSOR : C_TENUE);
        icono_tarjeta(i, 20, y + 12, sel ? C_CURSOR : C_COTA);
        gfx_text(64, y + 8, TARJ_T[i], sel ? C_CURSOR : C_TEXTO, 2);
        gfx_text(64, y + 38, TARJ_S[i], sel ? C_TEXTO : C_SUAVE, 1);
    }
    pie("[arriba/abajo] elegir  [enter] entrar  [esc] salir");
}

/* --------------------------------------------------------------------------
 *  1. Taller: menú, Mis problemas, editor, formulario
 * ----------------------------------------------------------------------- */
#define N_TALLER 4
#define TA_TXT (BARRA_H + 8 + N_TALLER * LIN_H + 9)   /* el texto bajo la lista */
/* Las filas que cuentan para las teclas: las pruebas de antes de rev10 no
 * ven la 4.ª (ui_cpm_oculto) */
static int n_taller(void) { return N_TALLER - (cpm_oculto ? 1 : 0); }
static void dib_taller(void)
{
    static const char *const items[N_TALLER] = {
        "Problema nuevo", "Seguir con el problema actual", "Mis problemas guardados",
        "Capacitores equivalentes"
    };
    barra("Taller: dibuja, emula y calcula", 0);
    lista(BARRA_H + 8, N_TALLER, items, N_TALLER, sel_taller, 0);
    separador(BARRA_H + 8 + N_TALLER * LIN_H + 4);
    if (sel_taller == 1 && circ.nel > 0) {
        gfx_text(8, TA_TXT, circ.nombre, C_TITULO, 1);
        miniatura(&circ, 0, TA_TXT + 14, SCR_W, PIE_Y - TA_TXT - 16);
    } else if (sel_taller == 2) {
        envuelve(8, TA_TXT, COLS,
                 "Los problemas que guardas desde el editor\n"
                 "([ctrl][S] o [menu] > Guardar) quedan aquí para\n"
                 "abrirlos otro día. Se guardan en la misma\n"
                 "carpeta que el programa.", C_SUAVE, 6);
    } else if (sel_taller == 3) {
        envuelve(8, TA_TXT, COLS,
                 "Sustituye un capacitor (el de arranque de un\n"
                 "motor monofásico) con los de la bodega: uno, dos o\n"
                 "tres, en serie, en paralelo o mixtos (los\n"
                 "repetidos cuentan). Da las opciones (primero las\n"
                 "de menos capacitores), el paso a paso con palabras\n"
                 "y el dibujo del cableado (la clavija, C, A y T).\n"
                 "Con su Teoría y sus Ejemplos. Si no se puede, lo\n"
                 "más cercano y qué capacitor te faltaría.",
                 C_SUAVE, 9);
    } else {
        /* con semiconductores o AO un [?!] no se despeja (la Ayuda, «[=] CON
         * SEMICONDUCTORES Y AO»): el barrido de [I] lo halla */
        envuelve(8, TA_TXT, COLS,
                 "Dibuja el circuito: una tecla y luego una flecha.\n"
                 "[catálogo] (el libro): los componentes y su tecla.\n"
                 "Sin [?!], [=] lo analiza o lo emula: punto de\n"
                 "operación, osciloscopio o fasores. [B] el Bode.\n"
                 "Marca con [?!] lo que buscas y [=] lo despeja en\n"
                 "CC y CA; con semiconductores o AO, barre con [I].\n"
                 "\n"
                 "La línea de abajo del editor dice qué hará [=].\n"
                 "[menu] abre las opciones.",
                 C_SUAVE, 9);
    }
    pie("[arriba/abajo] elegir   [enter] entrar   [esc]");
}

static void carga_lista_mis(void)
{
    static Circuito t;
    int i;
    nmis = 0;
    for (i = 0; i < ARCH_HUECOS; i++)
        if (arch_cargar(i, &t)) {
            mis_hueco[nmis] = i;
            mis_malo[nmis] = 0;
            memcpy(mis_nombre[nmis], t.nombre, sizeof mis_nombre[0]);
            mis_nombre[nmis][sizeof mis_nombre[0] - 1] = 0;
            nmis++;
        } else if (arch_existe(i)) {
            /* Dañado o de otra versión: se ve, para poder borrarlo */
            mis_hueco[nmis] = i;
            mis_malo[nmis] = 1;
            strcpy(mis_nombre[nmis], "(no se puede abrir)");
            nmis++;
        }
    if (sel_mis >= nmis) sel_mis = nmis - 1;
    if (sel_mis < 0) sel_mis = 0;
    borrar_confirma = 0;
}

#define MIS_VIS 5                     /* las filas de la lista de Mis problemas */
static void dib_mis(void)
{
    static const char *items[ARCH_HUECOS];
    static Circuito t;
    char r[200];
    int i, nvis = MIS_VIS, y;

    barra("Mis problemas", 0);
    if (nmis == 0) {
        envuelve(8, BARRA_H + 10, COLS,
                 "Todavía no has guardado ningún problema.\n\n"
                 "En el editor, pulsa [ctrl][S] (o [menu] y\n"
                 "elige \"Guardar\").", C_SUAVE, 8);
        pie("[esc] volver");
        return;
    }
    for (i = 0; i < nmis; i++) items[i] = mis_nombre[i];
    scroll = desplaza(sel_mis, scroll, nvis);
    lista(BARRA_H + 6, nvis, items, nmis, sel_mis, scroll);
    y = BARRA_H + 6 + nvis * LIN_H + 2;
    separador(y);
    if (nombrando == NOM_MIS) {
        /* el nombre que se escribe (en lugar del resumen) y debajo, qué
         * teclas escriben o por qué no se pudo */
        Buf b;
        buf_init(&b, r, sizeof r);
        buf_str(&b, "Nombre: "); buf_str(&b, nom_edit); buf_ch(&b, '_');
        gfx_text(8, y + 4, r, C_CURSOR, 1);
        gfx_text(8, y + 4 + LIN_H, nom_aviso ? nom_aviso : "Letras, cifras y [espacio]; [shift] may" S_uu "scula.",
                 nom_aviso ? C_ERROR : C_SUAVE, 1);
        if (arch_cargar(mis_hueco[sel_mis], &t)) miniatura(&t, 0, y + 46, SCR_W, PIE_Y - y - 48);
    }
    else if (mis_malo[sel_mis])
        envuelve(8, y + 4, COLS, "Este archivo está dañado o no es de esta "
                 "versión. [del] lo borra.", C_ERROR, 3);
    else if (arch_cargar(mis_hueco[sel_mis], &t)) {
        resumen(&t, r, sizeof r);
        envuelve(8, y + 4, COLS, r, C_TEXTO, 3);
        miniatura(&t, 0, y + 46, SCR_W, PIE_Y - y - 48);
    }
    if (nombrando == NOM_MIS) pie("[letras] nombre [del] borra [enter] guarda [esc] no");
    else if (borrar_confirma) pie("[del] otra vez para BORRARLO   [esc] no");
    else pie("[enter] abrir [N] nombre [del] borrar [esc] volver");
}

/* ---- Editor ---- */
static void vista_editor(Vista *v)
{
    esq_vista(v);
    v->rejilla = 1;
    v->cur_x = cur_x;
    v->cur_y = cur_y;
    v->sel = sel_el;
    v->sondas = 1;
    v->marca_x = marca_x;
    v->marca_y = marca_y;
    /* una controlada elegida (en el editor y en su ficha): quién la manda, en
     * naranja sin atenuar lo demás, y la línea de control */
    if (sel_el >= 0 && sel_el < circ.nel && tipo_info(circ.el[sel_el].tipo)->ley_cc == LEY_DEP) {
        int r = circ.el[sel_el].ref;
        if (r >= 0 && r < circ.nel && r != sel_el) { v->resalta = 1u << r; v->ctl_de = sel_el; }
    }
}

/* La barra de herramientas: una casilla por herramienta, con su tecla */
#define N_HERR 8
static const struct { const char *tecla; int ico, acc, tipo; } HERR[N_HERR] = {
    { "R",   ICO_RES,      ACC_PONER,  EL_RES },
    { "E",   ICO_FUENTE,   ACC_PONER,  EL_FUENTE },
    { "J",   ICO_CORR,     ACC_PONER,  EL_CORR },
    { "W",   ICO_CABLE,    ACC_CABLE,  -1 },
    { "del", ICO_BORRAR,   ACC_BORRAR, -1 },
    { "ent", ICO_EDITAR,   ACC_EDITAR, -1 },
    { "=",   ICO_RESOLVER, -1,         -1 },
    { "cat", ICO_PALETA,   -1,         -1 },
};

static void dib_herramientas(void)
{
    int i, w = SCR_W / N_HERR;
    gfx_fill(0, ED_HERR - 1, SCR_W, SCR_H - ED_HERR + 1, C_PANEL);
    separador(ED_HERR - 1);
    for (i = 0; i < N_HERR; i++) {
        int x = i * w, activo = HERR[i].acc == accion && accion != ACC_NADA &&
                                (accion != ACC_PONER || HERR[i].tipo == poner_tipo);
        u16 col = activo ? C_CURSOR : HERR[i].tipo >= 0 ? tipo_info(HERR[i].tipo)->color : C_COTA;
        if (activo) {
            gfx_fill(x + 1, ED_HERR + 1, w - 2, 18, C_BARRA);
            gfx_rect(x + 1, ED_HERR + 1, w - 2, 18, C_CURSOR);
        }
        esq_icono(HERR[i].ico, x + 1, ED_HERR + 3, col);
        gfx_text(x + 22, ED_HERR + 4, HERR[i].tecla, activo ? C_CURSOR : C_SUAVE, 1);
    }
}

static int elem_en(int x, int y, int d)
{
    int s = circ_seg(&circ, x, y, d);
    return s >= SEG_EL0 ? s - SEG_EL0 : -1;
}

/* "Aquí: ->R2 3 ohm  abajo cable ..." */
static void linea_aqui(char *buf, size_t cap)
{
    static const char *const FL[4] = { S_RARR, S_DARR, S_LARR, S_UARR };
    Buf b;
    int d, k, hay = 0;
    buf_init(&b, buf, cap);
    buf_str(&b, "Aquí:");
    if ((k = circ_interior(&circ, cur_x, cur_y)) >= 0) {
        buf_str(&b, " dentro de "); buf_str(&b, circ.el[k].etiq); buf_str(&b, " (no es una pata)");
        return;
    }
    for (k = 0; k < circ.nel; k++) {
        /* la pata de un 3 patas: su nombre, "U1 (-)", y no los segmentos de la caja */
        int p = circ_npatas(&circ.el[k]) == 3 ? circ_pata_en(&circ.el[k], cur_x, cur_y) : -1;
        if (p < 0) continue;
        hay = 1;
        buf_ch(&b, ' '); buf_str(&b, circ.el[k].etiq);
        buf_str(&b, " ("); buf_str(&b, tipo_info(circ.el[k].tipo)->huella->nom[p]); buf_ch(&b, ')');
    }
    for (d = 0; d < 4; d++) {
        int s = circ_seg(&circ, cur_x, cur_y, d);
        if (s <= 0) continue;
        if (s >= SEG_EL0 && circ_npatas(&circ.el[s - SEG_EL0]) == 3) continue;
        hay = 1;
        buf_ch(&b, ' ');
        buf_str(&b, FL[d]);
        if (s == SEG_CABLE) buf_str(&b, "cable");
        else {
            const Elemento *e = &circ.el[s - SEG_EL0];
            char v[16];
            buf_str(&b, e->etiq);
            esq_valor(e, 0, s - SEG_EL0, v, sizeof v);
            if (v[0]) { buf_ch(&b, ' '); buf_str(&b, v); }
        }
    }
    if (!hay) buf_str(&b, " nada (punto vacío)");
}

static const char *instruccion(void)
{
    switch (accion) {
    case ACC_PONER:  return pegando ? clip.pegar : tipo_info(poner_tipo)->al_poner;
    case ACC_CABLE:  return "Cable: cada flecha, un tramo. [esc] fin";
    case ACC_BORRAR: return "Borrar: cada flecha, un tramo. [esc] fin";
    case ACC_EDITAR: return "Editar: flecha hacia el elemento";
    case ACC_SONDA:  return "Sonda: [enter] tensión aquí " S_CDOT " flecha: I o V";
    default:         return "";
    }
}

/* Los índices 0 a 9 no se mueven (las pruebas cuentan flechas) y «Salir» va
 * la última: lo nuevo entra justo antes de ella */
#define N_CTX 12
static const char *const CTX[N_CTX] = {
    "Resolver o emular            [=]",
    "Practicar: solución tapada   [?!]",
    "Deshacer                [ctrl Z]",
    "Guardar                 [ctrl S]",
    "Borrar todo el circuito",
    "Ayuda",
    "Emulación: tiempo y paso",
    "Bode: barrido de frecuencia  [B]",
    "Barrido de CC: curvas        [I]",
    "Punto de operaci" S_oo "n de CC     [P]",
    "Guardar como copia (otro hueco)",
    "Salir al menú principal"
};

/* La caja acaba justo encima de la línea de avisos (ED_AQUI), que sigue a la
 * vista: con 12 opciones va de y = 27 a 208 */
static void dib_ctx(void)
{
    int w = 252, h = N_CTX * LIN_H + 26, x = 34, y = ED_AQUI - h, i;
    gfx_fill(x, y, w, h, C_PANEL);
    gfx_rect(x, y, w, h, C_CURSOR);
    gfx_text(x + 8, y + 5, "Opciones", C_CURSOR, 1);
    for (i = 0; i < N_CTX; i++) {
        int yy = y + 21 + i * LIN_H;
        const char *txt = (i == 6 && circ_alterna(&circ)) ? "Ajustes de CA: f y convenio" : CTX[i];
        if (i == sel_ctx) gfx_fill(x + 2, yy - 1, w - 4, LIN_H, C_BARRA);
        gfx_text(x + 10, yy, txt, i == sel_ctx ? C_TEXTO : C_COTA, 1);
    }
}

static void dib_editor(void)
{
    Marco m;
    Vista v;
    char t[96];
    u16 col;
    int dos = 0;                                /* el aviso ocupa también la línea de estado */

    barra(circ.nombre[0] ? circ.nombre : "Problema", "[menu] opciones");

    esq_marco_editor(&m);
    vista_editor(&v);
    gfx_clip(0, BARRA_H, SCR_W, ED_HERR - 1 - BARRA_H);
    esq_dibuja(&circ, &m, &v);
    if (accion != ACC_NADA) {
        int sx, sy;
        esq_pt(&m, cur_x, cur_y, &sx, &sy);
        if (accion == ACC_PONER && tipo_info(poner_tipo)->huella) {
            /* un 3 patas: las flechas que caben, en C_CURSOR; las demás, tenues */
            unsigned caben = 0;
            int d;
            for (d = 0; d < 4; d++) if (circ_cabe(&circ, poner_tipo, cur_x, cur_y, d, 0) == CM_OK) caben |= 1u << d;
            esq_flechas_cursor_m(sx, sy, caben, C_CURSOR, C_TENUE);
        } else
            esq_flechas_cursor(sx, sy, C_CURSOR);
    }
    gfx_clip_reset();

    dib_herramientas();

    if (nombrando == NOM_EDITOR) {
        /* [ctrl][N]: el nombre que se escribe y, debajo, las teclas (o por
         * qué no se pudo guardar) */
        Buf b;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Nombre: "); buf_str(&b, nom_edit); buf_ch(&b, '_');
        gfx_text(6, ED_AQUI, t, C_CURSOR, 1);
        gfx_text(6, ED_ESTADO, nom_aviso ? nom_aviso : "[letras] nombre [del] borra [enter] guarda [esc] no",
                 nom_aviso ? C_ERROR : C_SUAVE, 1);
        return;
    }
    if (accion != ACC_NADA && !aviso[0]) {
        gfx_text(6, ED_AQUI, instruccion(), C_CURSOR, 1);
    } else if (aviso[0]) {
        const char *resto = primera_linea(aviso, COLS, t);
        gfx_text(6, ED_AQUI, t, aviso_col, 1);
        /* lo que no cabe, en la línea de estado (el aviso dura una tecla) */
        if (*resto && !aviso_bajo) { primera_linea(resto, COLS, t); gfx_text(6, ED_ESTADO, t, aviso_col, 1); dos = 1; }
    } else {
        linea_aqui(t, sizeof t);
        envuelve(6, ED_AQUI, COLS, t, C_TEXTO, 1);
    }
    col = estado_problema(t, sizeof t);
    if (aviso[0] && aviso_bajo) gfx_text(6, ED_ESTADO, aviso_bajo, C_CURSOR, 1);   /* una tecla */
    else if (!dos) gfx_text(6, ED_ESTADO, t, col, 1);

    if (ctx_abierto) dib_ctx();
    if (paleta_abierta) dib_paleta();
}

/* ---- Formulario de un elemento: sus campos salen de la tabla de tipos ----
 * Solo los que se ven con su variante (la forma de una fuente, el modo de un
 * instrumento, el estado de un interruptor): vis[] son sus índices. */
static int vis_campo[MAX_CAMPOS], nvis;
static const char *aviso_nb;           /* aviso informativo de la ficha (no bloquea) */
static int aviso_nb_mal;               /* 1: ese aviso va en C_ERROR (si no, C_CURSOR) */

static void calcula_vis(void)
{
    const Elemento *e = &circ.el[sel_el];
    const TipoInfo *t = tipo_info(e->tipo);
    int i;
    nvis = 0;
    for (i = 0; i < t->ncampos; i++) {
        if (!(t->campos[i].vars == 0 || (t->campos[i].vars & VM(e->variante)))) continue;
        if ((t->campos[i].flags & CF_VER_CA) && !circ_alterna(&circ)) continue;   /* solo en alterna */
        vis_campo[nvis++] = i;
    }
}
static const CampoInfo *campo_info(int i)
{
    calcula_vis();
    if (i >= nvis) i = nvis - 1;
    if (i < 0) i = 0;
    return &tipo_info(circ.el[sel_el].tipo)->campos[vis_campo[i]];
}
static int ncampos(void) { calcula_vis(); return nvis; }

/* Los 3 campos que se ven: la primera fila (campo_ini) sigue al cursor */
static void acota_campos(void)
{
    int nc = ncampos();
    if (campo < campo_ini) campo_ini = campo;
    if (campo >= campo_ini + 3) campo_ini = campo - 2;
    if (campo_ini > nc - 3) campo_ini = nc - 3;
    if (campo_ini < 0) campo_ini = 0;
}

/* Nombre de la variante que se elige con [izq/der]: forma, modo o estado */
static const char *que_variante(int tipo)
{
    if (tipo == EL_FUENTE || tipo == EL_CORR) return "forma";
    if (tipo == EL_INTER) return "estado";
    if (tipo == EL_DIODO || tipo == EL_ZENER || tipo == EL_AO) return "modelo";
    if (tipo == EL_LED) return "color";
    if (tipo == EL_EDEP || tipo == EL_JDEP || tipo == EL_BJT) return "tipo";
    if (tipo == EL_MOS) return "canal";
    return "modo";
}
static Mag *mag_campo(int i)
{
    /* la fila x de una controlada es Elemento.ref, no una magnitud: nunca pasa
     * por circ_mag (escribiría en aux). Un relleno vacío, que nadie guarda */
    static Mag relleno;
    int q = campo_info(i)->q;
    if (q == Q_FCIRC) return &circ.frec;                  /* la f de todo el circuito */
    if (q == Q_CTRL) { memset(&relleno, 0, sizeof relleno); return &relleno; }
    return (Mag *)mag_de(&circ.el[sel_el], q);
}
/* ¿La fila i lleva caja de ángulo? Solo en un circuito de alterna */
static int fila_ang(int i) { return circ_alterna(&circ) && (campo_info(i)->flags & CF_ANG); }
static const char *nom_campo(int i) { return campo_info(i)->nombre; }
static const char *uni_campo(int i) { return campo_info(i)->unidad; }
static const char *ayuda_campo(int i) { return campo_info(i)->ayuda; }

/* ---- Cuentas en la ficha: «220^2/100», «1/(1/4k+1/6k)» ----
 * En las cajas de valor (no en la del ángulo ni en la fila x de una
 * controlada) y, en CA, no en las de parte j: ahí [+] y [-] son +j y -j */
static int cuentas_aqui(void)
{
    const CampoInfo *ci;
    if (pant != P_ELEMENTO || sel_el < 0 || sel_el >= circ.nel) return 0;
    ci = campo_info(campo);
    if (ci->q == Q_CTRL || sub == 1) return 0;
    return !(circ_alterna(&circ) && (ci->flags & CF_CX));
}

/* Lo escrito en la caja de valor, número o cuenta (num_eval). Un número solo
 * que no cabe en un double («8E505», «9E300G») es, como siempre, infinito:
 * el eco dice «inf» y al confirmar lo para el rango con su aviso (que dice
 * el máximo). num_eval ya no lo da por bueno: aquí se decide */
static double valor_edit(int *err)
{
    int ok;
    double v = num_eval(edit, err);
    if (*err == NE_GRANDE && !num_es_cuenta(edit)) {
        v = num_lee_ing(edit, &ok);
        if (ok) *err = NE_OK;
    }
    return v;
}

/* Por qué no vale una cuenta (num_eval): el aviso rojo de la ficha */
static const char *texto_cuenta(int err)
{
    switch (err) {
    case NE_PAR_ABRE:   return "Falta cerrar un par" S_ee "ntesis.";
    case NE_PAR_CIERRA: return "Sobra un par" S_ee "ntesis de cierre.";
    case NE_FALTA:      return "La cuenta est" S_aa " a medias: falta un n" S_uu "mero.";
    case NE_SIGNO:      return "Falta un signo entre dos n" S_uu "meros: + - " S_TIMES " " S_DIVIDE " ^";
    case NE_CERO:       return "Divisi" S_oo "n entre cero.";
    case NE_REAL:       return "La cuenta no da un n" S_uu "mero real.";
    case NE_GRANDE:     return "La cuenta da un n" S_uu "mero demasiado grande.";
    default:            return "Un n" S_uu "mero de la cuenta est" S_aa " mal escrito.";
    }
}

/* Lo que acaba un operando: una cifra, la coma, un prefijo o «)» */
static int fin_operando(char c)
{
    return (c >= '0' && c <= '9') || c == ',' || c == ')' || (c && strchr("pnmkMG\xB5", c));
}

/* ¿Puede empezar un operando detrás de edit[0..n)? Al principio, o detrás de
 * un signo o de «(» (el - del exponente, «2E-», no cuenta) */
static int abre_operando(size_t n)
{
    char c;
    if (n == 0) return 1;
    c = edit[n - 1];
    if (c == '-') return !(n >= 2 && edit[n - 2] == 'E');
    return c == '+' || c == '^' || c == '(' || c == (char)0xD7 || c == (char)0xF7;
}

/* Paréntesis abiertos sin cerrar */
static int abiertos(void)
{
    int n = 0;
    const char *p;
    for (p = edit; *p; p++) n += *p == '(' ? 1 : *p == ')' ? -1 : 0;
    return n;
}

/* Dónde empieza el número que se escribe: en una cuenta, detrás del último
 * signo o paréntesis (el - del principio y el del exponente son del número);
 * en una caja con parte j, en la j (como siempre) */
static const char *num_actual(void)
{
    const char *p = edit;
    size_t i;
    if (!cuentas_aqui()) { const char *j = strchr(edit, 'j'); return j ? j : edit; }
    for (i = 0; edit[i]; i++) {
        char c = edit[i];
        if (c == '+' || c == '^' || c == '(' || c == ')' || c == (char)0xD7 || c == (char)0xF7 ||
            (c == '-' && i > 0 && edit[i - 1] != 'E')) p = edit + i + 1;
    }
    return p;
}

/* El número de una caja de la ficha y de su eco: el de num_sig, salvo que sus
 * 9 decimales se coman cifras (el Is de un diodo: 1E-14 saldría «0» y 2,52 nA
 * «0,000000003»); entonces con exponente, como se escribe con [EE]: «1E-14»,
 * «2,52E-9». Lo que ya salía exacto (0,0000001) sale igual */
static void buf_dato(Buf *b, double v)
{
    double a = fabs(v), m;
    int e;
    char t[24];
    if (!(a > 0.0 && a < 1e-5) || fabs(floor(a * 1e9 + 0.5) * 1e-9 - a) <= 1e-9 * a) { buf_sig(b, v); return; }
    e = (int)floor(log10(a));
    m = a / pow(10.0, (double)e);
    if (m < 1.0) { m *= 10.0; e--; }                  /* el redondeo de log10 */
    if (m >= 9.9995) { m /= 10.0; e++; }              /* 9,9996E-9 sale 1E-8, no 10E-9 */
    buf_str(b, num_auto(t, sizeof t, v < 0.0 ? -m : m, 3));
    buf_ch(b, 'E');
    buf_int(b, e);
}

/* Un extremo del rango en el aviso: el rótulo corto («1m», «1M»); por debajo
 * del nano (el Is de un diodo, la C), con exponente: num_corto daría «0n» */
static void buf_limite(Buf *b, double v)
{
    char t[24];
    if (v != 0.0 && fabs(v) < 1e-9) buf_dato(b, v);
    else buf_str(b, num_corto(t, sizeof t, v));
}

/* ---- La ficha en un circuito de alterna ----
 * Cada fila: nombre, caja del módulo (24 a 95), ∠, caja del ángulo (106 a
 * 149), °, unidad (160) y estado (190). Lo que se ve está en el convenio del
 * circuito (conv); lo guardado es canónico (eficaz, coseno y grados). */
static void ver_en_conv(const Mag *mg, int flags, double *mod, double *ang)
{
    double a = mg->est_ang == M_DATO ? mg->ang : 0.0;
    if (flags & CF_AMPL) ca_canon_a_conv(circ.conv, mg->v, a, mod, ang);
    else { *mod = mg->v; *ang = a; }
}

/* El texto de un campo de la ficha, arrimado a la derecha en xd, en una
 * caja cuyo primer píxel útil es x0: si no cabe entero, «« y su final por
 * caracteres enteros (nunca medio glifo pegado al borde: se leía
 * «-1+1+...» con media «+» delante) */
static void texto_campo(int x0, int xd, int y, const char *t, u16 col)
{
    int cab = (xd - x0) / FONT_W, n = (int)strlen(t);
    if (n <= cab) { gfx_text(xd - FONT_W * n, y, t, col, 1); return; }
    gfx_text(xd - FONT_W * cab, y, S_LAQUO, C_SUAVE, 1);
    gfx_text(xd - FONT_W * (cab - 1), y, t + n - (cab - 1), col, 1);
}

static void fila_ca(int i, int y, int selec)
{
    const Mag *mg = mag_campo(i);
    const CampoInfo *ci = campo_info(i);
    const Elemento *e = &circ.el[sel_el];
    int ang = fila_ang(i), fuente_z = ang && ci->q == Q_VAL;
    double mod, an;
    const char *estado;
    u16 ce;
    char t[40];                                  /* una cuenta de 31 y el cursor */
    Buf b;

    ver_en_conv(mg, ci->flags, &mod, &an);
    /* el módulo */
    buf_init(&b, t, sizeof t);
    if (selec && editando && sub == 0) { buf_str(&b, edit); buf_ch(&b, '_'); }
    else if (mg->est == M_DATO) buf_dato(&b, ang ? fabs(mod) : (con_signo(e, ci->q) ? mod : fabs(mod)));
    else if (mg->est == M_BUSCAR) buf_ch(&b, '?');
    else buf_str(&b, S_RAYA);
    gfx_fill(24, y - 1, 72, 14, selec ? C_PANEL : C_FONDO);
    gfx_rect(24, y - 1, 72, 14, selec && sub == 0 ? C_CURSOR : C_TENUE);
    gfx_clip(25, y - 1, 70, 14);                 /* lo que se escribe no pisa la otra caja */
    texto_campo(26, 92, y, t, mg->est == M_BUSCAR ? C_INCOG : (selec ? C_CURSOR : C_TEXTO));
    gfx_clip_reset();
    /* el ángulo */
    if (ang) {
        u16 ca = C_TEXTO;
        gfx_text(98, y, S_ANG, C_SUAVE, 1);
        buf_init(&b, t, sizeof t);
        if (selec && editando && sub == 1) { buf_str(&b, edit); buf_ch(&b, '_'); }
        else if (mg->est_ang == M_DATO) buf_sig(&b, an);
        else if (mg->est_ang == M_BUSCAR) { buf_ch(&b, '?'); ca = C_INCOG; }
        else if (fuente_z) { buf_sig(&b, an); ca = C_TENUE; }    /* vacío = 0° (en su convenio) */
        else { buf_str(&b, S_RAYA); ca = C_TENUE; }
        gfx_fill(106, y - 1, 44, 14, selec ? C_PANEL : C_FONDO);
        gfx_rect(106, y - 1, 44, 14, selec && sub == 1 ? C_CURSOR : C_TENUE);
        gfx_clip(107, y - 1, 42, 14);
        texto_campo(108, 146, y, t, selec && ca == C_TEXTO ? C_CURSOR : ca);
        gfx_clip_reset();
        gfx_text(151, y, S_DEG, C_SUAVE, 1);
    }
    gfx_text(160, y, uni_campo(i), C_SUAVE, 1);
    /* el estado de la fila */
    if (!ang) {
        if (mg->est == M_DATO || (selec && editando && edit[0])) { estado = "dato"; ce = C_OK; }
        else if (mg->est == M_BUSCAR) { estado = "lo que buscas"; ce = C_INCOG; }
        else if (ci->vacio) { estado = ci->vacio; ce = strcmp(estado, "falta") == 0 ? C_ERROR : C_SUAVE; }
        else if (mg->est == M_LIBRE && (ci->q == Q_AUX || (ci->q == Q_VAL && tipo_info(e->tipo)->ideal))) { estado = "ideal"; ce = C_SUAVE; }
        else { estado = "no se sabe"; ce = C_SUAVE; }
    } else if (mg->est == M_DATO && mg->est_ang == M_DATO) { estado = "dato"; ce = C_OK; }
    else if (mg->est == M_DATO && mg->est_ang == M_LIBRE) {
        static char dz[24];
        Buf bz;
        buf_init(&bz, dz, sizeof dz);
        buf_str(&bz, "dato (" S_ANG); buf_sig(&bz, an); buf_ch(&bz, ')');
        estado = fuente_z ? dz : "solo m" S_oo "dulo"; ce = C_OK;
    }
    else if (mg->est == M_DATO)                           { estado = "buscas el " S_aa "ngulo"; ce = C_INCOG; }
    else if (mg->est == M_BUSCAR && mg->est_ang == M_BUSCAR) { estado = "lo que buscas"; ce = C_INCOG; }
    else if (mg->est == M_BUSCAR)                         { estado = "buscas el m" S_oo "dulo"; ce = C_INCOG; }
    else if (mg->est_ang == M_DATO)                       { estado = "solo el " S_aa "ngulo"; ce = C_SUAVE; }
    else if (mg->est_ang == M_BUSCAR)                     { estado = "buscas el " S_aa "ngulo"; ce = C_INCOG; }
    else if (ci->vacio) { estado = ci->vacio; ce = strcmp(estado, "falta") == 0 ? C_ERROR : C_SUAVE; }
    else { estado = "no se sabe"; ce = C_SUAVE; }
    if (selec && editando && edit[0]) { estado = "dato"; ce = C_OK; }
    gfx_text(190, y, estado, ce, 1);
}

/* El eco en CA: lo que se escribe en forma compleja, la otra forma de un
 * fasor dado, o R y X de una Z. 1 si escribió algo */
static int eco_ca(int y)
{
    const CampoInfo *ci = campo_info(campo);
    const Mag *mg = mag_campo(campo);
    char t[64], u[32];
    Buf b;
    buf_init(&b, t, sizeof t);
    if (editando && edit[0] && sub == 0 && strchr(edit, 'j')) {
        double im, re;
        int ok;
        re = num_lee_cx(edit, &im, &ok);
        if (!ok) { gfx_text(8, y, "...", C_SUAVE, 1); return 1; }
        buf_str(&b, "= ");
        buf_str(&b, edit);
        buf_str(&b, " = ");
        buf_str(&b, num_fasor(u, sizeof u, hypot(re, im), atan2(im, re) * 180.0 / 3.14159265358979323846, 0, 0, ci->unidad));
        gfx_text(8, y, t, C_CURSOR, 1);
        return 1;
    }
    if (editando && sub == 1 && edit[0]) {          /* el ángulo que se escribe: grados */
        int ok;
        double v = num_lee_ing(edit, &ok);
        if (!ok) { gfx_text(8, y, "...", C_SUAVE, 1); return 1; }
        buf_str(&b, "= "); buf_sig(&b, v); buf_str(&b, S_DEG);
        gfx_text(8, y, t, C_CURSOR, 1);
        return 1;
    }
    if (editando) return 0;
    if (!fila_ang(campo) || mg->est != M_DATO) return 0;
    if (ci->q == Q_VAL && circ.el[sel_el].tipo == EL_IMP) {
        /* la Z: R y X, y a qué L o C equivale a la f del circuito */
        double ang = mg->est_ang == M_DATO ? mg->ang * 3.14159265358979323846 / 180.0 : 0.0;
        double R = fabs(mg->ang) == 90.0 && ang != 0.0 ? 0.0 : mg->v * cos(ang), X = mg->v * sin(ang);
        buf_str(&b, "R = "); buf_str(&b, num_pref(u, sizeof u, R, 4, S_OHM));
        buf_str(&b, ", X = "); buf_str(&b, num_pref(u, sizeof u, X, 4, S_OHM));
        if (circ.frec.est == M_DATO && fabs(X) > 0.0) {
            double w = 2.0 * 3.14159265358979323846 * circ.frec.v;
            buf_str(&b, X > 0.0 ? " (L = " : " (C = ");
            buf_str(&b, X > 0.0 ? num_pref(u, sizeof u, X / w, 4, "H") : num_pref(u, sizeof u, -1.0 / (w * X), 4, "F"));
            buf_ch(&b, ')');
        }
        gfx_text(8, y, t, C_SUAVE, 1);
        return 1;
    }
    if (mg->est_ang != M_DATO && !(ci->q == Q_VAL)) return 0;
    {
        double mod, an;
        ver_en_conv(mg, ci->flags, &mod, &an);
        buf_str(&b, "= ");
        buf_str(&b, num_fasor(u, sizeof u, mod, an, 1, 0, ci->unidad));
        gfx_text(8, y, t, C_SUAVE, 1);
    }
    return 1;
}

/* La fila x de una controlada: quién la manda, «V(R2)» o «I(R1)» (la variante
 * dice cuál), o «—». Se elige con [+][-], no se escribe; el estado sale de
 * circ_ctl_motivo, la misma regla que la tabla y el emulador */
static void fila_ctrl(int y, int selec)
{
    const Elemento *e = &circ.el[sel_el];
    int m = circ_ctl_motivo(&circ, sel_el);
    char t[16];
    Buf b;
    buf_init(&b, t, sizeof t);
    if (e->ref >= 0 && e->ref < circ.nel) {
        buf_str(&b, e->variante == DEP_I ? "I(" : "V(");
        buf_str(&b, circ.el[e->ref].etiq);
        buf_ch(&b, ')');
    } else buf_str(&b, S_RAYA);
    gfx_fill(24, y - 1, 104, 14, selec ? C_PANEL : C_FONDO);
    gfx_rect(24, y - 1, 104, 14, selec ? C_CURSOR : C_TENUE);
    gfx_text(124 - gfx_text_w(t, 1), y, t, selec ? C_CURSOR : C_TEXTO, 1);
    gfx_text(circ_alterna(&circ) ? 190 : 160, y, m == CTL_OK ? "control" : m == CTL_FALTA ? "falta" : "no vale",
             m == CTL_OK ? C_OK : C_ERROR, 1);
}

/* El eco de la Etapa 4 sin escribir (y = 200): lo que toma el vacío o el
 * catálogo del modelo elegido; en la fila x de una controlada cuyo control no
 * vale, la primera línea del porqué (el texto de SIM_CTL_MAL). Un valor que es
 * dato manda: entonces se ve su ayuda. 1 si escribió algo */
static int eco_nl(int y)
{
    const Elemento *e = &circ.el[sel_el];
    char t[64], u[24];
    Buf b;
    if (campo_info(campo)->q == Q_CTRL) {
        int m = circ_ctl_motivo(&circ, sel_el);
        SimDiag d;
        char msg[200];
        if (m == CTL_OK || m == CTL_FALTA) return 0;
        memset(&d, 0, sizeof d);
        d.estado = SIM_CTL_MAL; d.elem = sel_el; d.nodo = e->ref; d.campo = m;
        sim_mensaje_circ(&circ, 0, &d, msg, (int)sizeof msg);
        envuelve(8, y, COLS, msg, C_ERROR, 1);
        return 1;
    }
    if (mag_campo(campo)->est == M_DATO) return 0;
    buf_init(&b, t, sizeof t);
    switch (e->tipo) {
    case EL_DIODO:
        if (e->variante == DIO_1N4148)
            buf_str(&b, "1N4148: Is 2,52 nA " S_CDOT " n 1,752 " S_CDOT " Rs 0,568 " S_OHM);
        else if (e->variante == DIO_1N4007)
            buf_str(&b, "1N4007: Is 7,03 nA " S_CDOT " n 1,808 " S_CDOT " Rs 34 m" S_OHM);
        break;
    case EL_ZENER:
        if (e->variante == ZEN_1N4733A) buf_str(&b, "1N4733A: Vz 5,1 V a 49 mA " S_CDOT " rz 7 " S_OHM);
        break;
    case EL_LED:                        /* «Rojo: Vf 1,8 V a 20 mA (n 2, Rs 5 Ω)» */
        if (e->variante < LED_N) {
            const NlColor *nc = &NL_COLOR[e->variante];
            const Mag *vf = &e->par[QL_VF - Q_P1], *fi = &e->par[QL_IF - Q_P1];
            buf_ch(&b, (char)(NOM_LED[e->variante][0] - 'a' + 'A'));
            buf_str(&b, NOM_LED[e->variante] + 1);
            buf_str(&b, ": Vf "); buf_sig(&b, vf->est == M_DATO ? vf->v : nc->Vf);
            buf_str(&b, " V a "); buf_str(&b, num_pref(u, sizeof u, fi->est == M_DATO ? fi->v : NL_IF, 3, " A"));
            buf_str(&b, " (n "); buf_sig(&b, nc->n);
            buf_str(&b, ", Rs "); buf_sig(&b, nc->Rs); buf_str(&b, " " S_OHM ")");
        }
        break;
    case EL_AO:
        if (e->variante == AO_IDEAL) buf_str(&b, "Ideal: sin corriente de entrada; en lineal Vd = 0");
        break;
    case EL_BJT:                        /* «NPN: β 100 · Is 1 fA · sin Early» */
        if (e->variante < BJT_N) {
            const Mag *bf = &e->par[QB_BF - Q_P1], *is = &e->par[QB_IS - Q_P1], *va = &e->par[QB_VA - Q_P1];
            double isv = is->est == M_DATO ? is->v : NL_IS_Q;
            buf_str(&b, NOM_BJT[e->variante]);
            buf_str(&b, ": " S_BETA " "); buf_sig(&b, bf->est == M_DATO ? bf->v : NL_BF);
            buf_str(&b, " " S_CDOT " Is ");
            /* num_pref llega a los pA: la de omisión, 1 fA, saldría «0 pA» */
            if (isv > 0.0 && isv < 1e-12) { buf_sig(&b, isv * 1e15); buf_str(&b, " fA"); }
            else buf_str(&b, num_pref(u, sizeof u, isv, 3, " A"));
            buf_str(&b, " " S_CDOT " ");
            if (va->est == M_DATO) { buf_str(&b, "VA "); buf_str(&b, num_pref(u, sizeof u, va->v, 3, " V")); }
            else buf_str(&b, "sin Early");
        }
        break;
    case EL_MOS:                        /* «Canal N: Vt 2 V · K 1 mA/V²» */
        if (e->variante < MOS_N) {
            const Mag *vt = &e->par[QM_VT - Q_P1], *kk = &e->par[QM_K - Q_P1];
            buf_ch(&b, (char)(NOM_MOS[e->variante][0] - 'a' + 'A'));
            buf_str(&b, NOM_MOS[e->variante] + 1);
            buf_str(&b, ": Vt "); buf_sig(&b, vt->est == M_DATO ? vt->v : NL_VT_M);
            buf_str(&b, " V " S_CDOT " K "); buf_str(&b, num_pref(u, sizeof u, kk->est == M_DATO ? kk->v : NL_K, 3, " A/V" S_SUP2));
        }
        break;
    default: break;
    }
    if (!t[0]) return 0;
    gfx_text(8, y, t, C_SUAVE, 1);
    return 1;
}

/* «Espejo: + y - de U1 cambian de sitio.» (las patas a y b de su huella) */
static const char *texto_espejo(const Elemento *e)
{
    static char t[48];
    const Huella *h = tipo_info(e->tipo)->huella;
    Buf b;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Espejo: "); buf_str(&b, h ? h->nom[0] : "a"); buf_str(&b, " y "); buf_str(&b, h ? h->nom[1] : "b");
    buf_str(&b, " de "); buf_str(&b, e->etiq); buf_str(&b, " cambian de sitio.");
    return t;
}

static void dib_elemento(void)
{
    Marco m;
    Vista v;
    char t[48];
    Buf b;
    int i, y;
    const Elemento *e = &circ.el[sel_el];

    buf_init(&b, t, sizeof t);
    buf_str(&b, e->etiq);
    buf_str(&b, "  ");
    buf_str(&b, tipo_info(e->tipo)->nombre);
    if (tipo_info(e->tipo)->nvar > 0 && e->variante < tipo_info(e->tipo)->nvar) {
        char dv[40];
        Buf bd;
        buf_init(&bd, dv, sizeof dv);
        buf_str(&bd, S_LARR " ");
        buf_str(&bd, tipo_info(e->tipo)->nom_var[e->variante]);
        buf_str(&bd, " " S_RARR);
        barra(t, dv);
    } else
        barra(t, 0);

    esq_marco_editor(&m);
    vista_editor(&v);
    v.cur_x = -1;
    v.marca_x = v.marca_y = -1;
    gfx_clip(0, BARRA_H, SCR_W, FO_PANEL - BARRA_H);
    esq_dibuja(&circ, &m, &v);
    gfx_clip_reset();

    gfx_fill(0, FO_PANEL, SCR_W, PIE_Y - FO_PANEL, C_PANEL);
    separador(FO_PANEL);

    {
        /* Se ven 3 campos; con más, la lista se desplaza con el cursor (y
         * también tras cada tecla: cotas_vista) */
        acota_campos();
        if (campo_ini > 0) gfx_text(SCR_W - 10, FO_PANEL + 5, S_UARR, C_CURSOR, 1);
        if (campo_ini + 3 < ncampos()) gfx_text(SCR_W - 10, FO_PANEL + 5 + 2 * 15, S_DARR, C_CURSOR, 1);
    }
    for (i = campo_ini; i < ncampos() && i < campo_ini + 3; i++) {
        const Mag *mg = mag_campo(i);
        int selec = (i == campo);
        const char *estado;
        u16 ce;
        y = FO_PANEL + 5 + (i - campo_ini) * 15;
        if (selec) gfx_fill(0, y - 1, SCR_W, 14, C_BARRA);
        gfx_text(8, y, nom_campo(i), selec ? C_TEXTO : C_COTA, 1);
        if (campo_info(i)->q == Q_CTRL) { fila_ctrl(y, selec); continue; }
        if (circ_alterna(&circ)) { fila_ca(i, y, selec); continue; }

        buf_init(&b, t, sizeof t);
        if (selec && editando) { buf_str(&b, edit); buf_ch(&b, '_'); }
        else if (mg->est == M_DATO) buf_dato(&b, con_signo(e, campo_info(i)->q) ? mg->v : fabs(mg->v));
        else if (mg->est == M_BUSCAR) buf_ch(&b, '?');
        else buf_str(&b, S_RAYA);
        gfx_fill(24, y - 1, 104, 14, selec ? C_PANEL : C_FONDO);
        gfx_rect(24, y - 1, 104, 14, selec ? C_CURSOR : C_TENUE);
        gfx_clip(25, y - 1, 102, 14);            /* una cuenta larga: se ve su final, tras un «« */
        texto_campo(26, 124, y, t, mg->est == M_BUSCAR ? C_INCOG : (selec ? C_CURSOR : C_TEXTO));
        gfx_clip_reset();
        gfx_text(134, y, uni_campo(i), C_SUAVE, 1);

        if (mg->est == M_DATO || (selec && editando && edit[0]))
                                      { estado = "dato";          ce = C_OK; }
        else if (mg->est == M_LIBRE && campo_info(i)->vacio) {
            /* el rótulo del vacío: "de CC", "nunca", "falta"... */
            estado = campo_info(i)->vacio;
            ce = strcmp(estado, "falta") == 0 ? C_ERROR : C_SUAVE;
        }
        else if (mg->est == M_LIBRE && (campo_info(i)->q == Q_AUX ||
                 (campo_info(i)->q == Q_VAL && tipo_info(e->tipo)->ideal)))
                                      { estado = "ideal";         ce = C_SUAVE; }
        else if (mg->est == M_BUSCAR) { estado = "lo que buscas"; ce = C_INCOG; }
        else                          { estado = "no se sabe";    ce = C_SUAVE; }
        gfx_text(160, y, estado, ce, 1);
    }
    y = FO_PANEL + 5 + 3 * 15 + 4;
    if (circ_alterna(&circ) && eco_ca(y)) {}
    else if (editando && edit[0]) {
        /* Eco: cuánto vale lo que se está escribiendo ("= 0,025 V"), número o
         * cuenta; a medias, «...» */
        int err, ok;
        double vale = valor_edit(&err);
        ok = err == NE_OK;
        buf_init(&b, t, sizeof t);
        if (ok) { buf_str(&b, "= "); buf_dato(&b, vale); buf_ch(&b, ' '); buf_str(&b, uni_campo(campo)); }
        else buf_str(&b, "...");
        gfx_text(8, y, t, ok ? C_CURSOR : C_SUAVE, 1);
    } else if (e->tipo >= EL_DIODO && eco_nl(y)) {}
    else
        gfx_text(8, y, ayuda_campo(campo), C_COTA, 1);
    y += LIN_H + 2;
    if (aviso_r == AV_R) {
        char mn[24];
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Demasiado pequeño: el mínimo es ");
        buf_str(&b, num_corto(mn, sizeof mn, tipo_info(e->tipo)->val_min));
        buf_str(&b, campo_info(campo)->unidad);
        buf_ch(&b, '.');
        gfx_text(8, y, t, C_ERROR, 1);
    }
    else if (aviso_r == AV_NUMERO) gfx_text(8, y, "Eso no es un número: escríbelo otra vez.", C_ERROR, 1);
    else if (aviso_r == AV_RANGO) gfx_text(8, y, "Demasiado grande (máximo 1 000 000 000).", C_ERROR, 1);
    else if (aviso_r == AV_FUERA) {
        double lo, hi;
        tipo_rango(e->tipo, campo_info(campo)->q, &lo, &hi);
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Fuera de rango: entre ");
        buf_limite(&b, lo); buf_str(&b, " y ");
        buf_limite(&b, hi); buf_ch(&b, ' ');
        buf_str(&b, campo_info(campo)->unidad); buf_ch(&b, '.');
        gfx_text(8, y, t, C_ERROR, 1);
    }
    else if (aviso_r == AV_VPVN) gfx_text(8, y, "V+ tiene que ser mayor que V-.", C_ERROR, 1);
    else if (aviso_r == AV_CUENTA) gfx_text(8, y, texto_cuenta(aviso_cuenta), C_ERROR, 1);
    else if (aviso_r == AV_NEGATIVO) gfx_text(8, y, "La cuenta da negativo y aqu" S_ii " no lleva signo.", C_ERROR, 1);
    else if (aviso_r == AV_ANG)
        gfx_text(8, y, (campo_info(campo)->flags & CF_ANG90) ? "Fuera de rango: el " S_aa "ngulo va de -90 a 90" S_DEG "." :
                                                           "Fuera de rango: el " S_aa "ngulo va de -360 a 360" S_DEG ".", C_ERROR, 1);
    else if (aviso_nb) gfx_text(8, y, aviso_nb, aviso_nb_mal ? C_ERROR : C_CURSOR, 1);
    else if (editando && cuentas_aqui())        /* se está escribiendo: también cuentas */
        gfx_text(8, y, "Cuentas: + - " S_TIMES " " S_DIVIDE " ^ ( ) x" S_SUP2 "   [enter] la guarda", C_SUAVE, 1);
    else if (campo_info(campo)->q == Q_CTRL)
        gfx_text(8, y, "[+][-] elige  [del] ninguno  [cat" S_aa "logo]=modelo", C_SUAVE, 1);
    else if (circ_alterna(&circ) && (campo_info(campo)->flags & CF_CX))
        gfx_text(8, y, "número=dato [+][J]=parte j [tab]=" S_ANG " [del]=vacío", C_SUAVE, 1);
    else if (fila_ang(campo))
        gfx_text(8, y, "número=dato [tab]=" S_ANG " [?!]=buscas [del]=vacío", C_SUAVE, 1);
    else if ((campo_info(campo)->flags & CF_NO_BUSCAR) && !(circ_alterna(&circ) && (campo_info(campo)->flags & CF_BUSCA_CA)))
        gfx_text(8, y, "número=dato  [del]=vacío  [catálogo]=modelo", C_SUAVE, 1);
    else gfx_text(8, y, "número=dato  [?!]=lo buscas  [del]=no se sabe", C_SUAVE, 1);
    if (campo_info(campo)->q == Q_CTRL)
        pie("[arr/ab] campo [+][-] control [izq/der] tipo [=]");
    else if (tipo_info(e->tipo)->nvar > 0) {
        char pp[56];
        Buf bp;
        buf_init(&bp, pp, sizeof pp);
        buf_str(&bp, "[arr/ab] campo [izq/der] ");
        buf_str(&bp, que_variante(e->tipo));
        /* en CC, [tab] pasa a la ficha del siguiente (en CA es el ángulo): se
         * dice donde cabe, en lugar de «[enter] listo». Con [F] no cabe (51 a
         * 57 columnas): lo dice la Ayuda */
        buf_str(&bp, circ_npatas(e) == 3 ? " [F] espejo [=]" :
                     tipo_info(e->tipo)->polar ? " [F] girar [=]" :
                     circ_alterna(&circ) ? "  [enter] listo [=]" : " [tab] siguiente [=]");
        pie(pp);
    } else if (tipo_info(e->tipo)->polar)
        pie(circ_alterna(&circ) ? "[arr/ab] campo [F] girar [enter] listo [=]"
                                : "[arr/ab] campo [F] girar [tab] siguiente [=]");
    else
        pie(circ_alterna(&circ) ? "[arriba/abajo] campo   [enter] listo   [=]"
                                : "[arr/ab] campo [tab] siguiente [enter] listo [=]");
}

/* Un valor que no vale: el aviso av (con AV_CUENTA, err dice por qué) y el
 * campo como estaba. Un número se tira (se escribe otra vez, como siempre);
 * una cuenta se queda escrita, para corregirla ([del], el paréntesis que
 * falta) sin teclearla entera. confirma_campo ya apagó editando */
static void rechaza(int av, int err)
{
    aviso_r = av;
    aviso_cuenta = err;
    if (num_es_cuenta(edit)) editando = 1;
    else edit[0] = 0;
}

/* Lo escrito en una caja de valor: número o cuenta. 1 si vale (en *v); si
 * no, el aviso (rechaza) */
static int lee_valor(double *v)
{
    int err;
    *v = valor_edit(&err);
    if (err == NE_OK) return 1;
    rechaza(err == NE_NUMERO && !num_es_cuenta(edit) ? AV_NUMERO : AV_CUENTA, err);
    return 0;
}

/* Confirma lo escrito en CA: el ángulo, una Z en forma compleja, o un
 * módulo (en el convenio: con pico se guarda el eficaz). 1 si lo atendió */
static int confirma_ca(Mag *mg)
{
    const CampoInfo *ci = campo_info(campo);
    int ok, tipo = circ.el[sel_el].tipo;
    double v, lo, hi;
    if (!fila_ang(campo)) return 0;
    if (sub == 1) {
        double lim = (ci->flags & CF_ANG90) ? 90.0 : 360.0;
        v = num_lee_ing(edit, &ok);
        edit[0] = 0;
        if (!ok) { aviso_r = AV_NUMERO; return 1; }
        if (!(fabs(v) <= lim)) { aviso_r = AV_ANG; return 1; }
        if ((ci->flags & CF_AMPL) && (circ.conv & CONV_SENO)) v -= 90.0;    /* se guarda en coseno */
        mg->ang = num_ang(v);
        mg->est_ang = M_DATO;
        return 1;
    }
    if (strchr(edit, 'j')) {                              /* R + jX */
        double im, re = num_lee_cx(edit, &im, &ok);
        edit[0] = 0;
        if (!ok) { aviso_r = AV_NUMERO; return 1; }
        if (re < 0.0) { aviso_r = AV_FUERA; return 1; }
        v = hypot(re, im);
        tipo_rango(tipo, ci->q, &lo, &hi);
        if (!(v >= lo && v <= hi)) { aviso_r = AV_FUERA; return 1; }
        mg->v = v; mg->est = M_DATO;
        mg->ang = atan2(im, re) * 180.0 / 3.14159265358979323846;
        mg->est_ang = M_DATO;
        return 1;
    }
    if (!lee_valor(&v)) return 1;                         /* el módulo: número o cuenta */
    if (ci->q == Q_VAL) { tipo_rango(tipo, Q_VAL, &lo, &hi); if (lo < 0.0) lo = 0.0; }
    else { lo = 0.0; hi = 1e9; }
    if (!(v >= lo && v <= hi)) { rechaza(AV_FUERA, 0); return 1; }
    edit[0] = 0;
    if ((ci->flags & CF_AMPL) && (circ.conv & CONV_PICO)) v /= sqrt(2.0);  /* se guarda el eficaz */
    mg->v = v; mg->est = M_DATO;
    return 1;
}

/* El AO: V+ > V- (con los vacíos, 15 y -15), §2.2. 1 si vale con la
 * magnitud q del elemento e puesta a (est, v) */
static int ao_vp_vn_vale(const Elemento *e, int q, int est, double v)
{
    double vp = e->par[QA_VP - Q_P1].est == M_DATO ? e->par[QA_VP - Q_P1].v : NL_VSAT;
    double vn = e->par[QA_VN - Q_P1].est == M_DATO ? e->par[QA_VN - Q_P1].v : -NL_VSAT;
    if (e->tipo != EL_AO || (q != QA_VP && q != QA_VN)) return 1;
    if (q == QA_VP) vp = est == M_DATO ? v : NL_VSAT;
    else vn = est == M_DATO ? v : -NL_VSAT;
    return vp > vn;
}

static void confirma_campo(void)
{
    Mag *mg;
    double v;

    if (!editando) return;
    editando = 0;
    mg = mag_campo(campo);
    if (!edit[0]) {
        if (!ao_vp_vn_vale(&circ.el[sel_el], campo_info(campo)->q, M_LIBRE, 0.0)) { aviso_r = AV_VPVN; return; }
        if (fila_ang(campo) && sub == 1) mg->est_ang = M_LIBRE;
        else mg->est = M_LIBRE;
        return;
    }
    if (confirma_ca(mg)) return;
    /* Un número mal escrito ("4,5E", "1,2,") o una cuenta que no vale no se
     * descartan en silencio: se avisa y el campo se queda como estaba. */
    if (!lee_valor(&v)) return;
    {
        /* El rango: el de la tabla de tipos. Los campos de las Etapas 0 y 1
         * conservan sus avisos; los nuevos dicen el rango entero. Una cuenta
         * que da negativo donde no se escribe signo, no vale */
        int tipo = circ.el[sel_el].tipo, q = campo_info(campo)->q;
        double lo, hi;
        if (v < 0.0 && !con_signo(&circ.el[sel_el], q)) { rechaza(AV_NEGATIVO, 0); return; }
        tipo_rango(tipo, q, &lo, &hi);
        if (q >= Q_P1 || (q == Q_VAL && tipo >= EL_COND) || (q == Q_AUX && tipo == EL_VATI)) {
            if (!(v >= lo && v <= hi)) { rechaza(AV_FUERA, 0); return; }
            if (!ao_vp_vn_vale(&circ.el[sel_el], q, M_DATO, v)) { rechaza(AV_VPVN, 0); return; }
        } else {
            if (!(fabs(v) <= (q == Q_VAL ? tipo_info(tipo)->val_max : 1e9))) { rechaza(AV_RANGO, 0); return; }
            if (q == Q_VAL && !(v >= tipo_info(tipo)->val_min)) { rechaza(AV_R, 0); return; }
        }
    }
    edit[0] = 0;
    if (campo == 0 && circ.el[sel_el].tipo == EL_RES) {
        ultimo_R = v;
        hay_ultimo_R = 1;
    }
    mg->v = v;
    mg->est = M_DATO;
}

/* Prefijo del SI que escribe una tecla en un campo, o 0. [M] es mili y
 * [shift][M] mega (confundirlos es un error de mil millones: por eso el
 * campo enseña debajo cuánto vale lo escrito). */
static char tecla_prefijo(int t)
{
    if (t == T_LETRA('P')) return 'p';
    if (t == T_LETRA('N')) return 'n';
    if (t == T_LETRA('U')) return (char)0xB5;              /* µ */
    if (t == T_LETRA('M')) return (mods & T_CON_SHIFT) ? 'M' : 'm';
    if (t == T_LETRA('K')) return 'k';
    if (t == T_LETRA('G')) return 'G';
    return 0;
}

/* Se puede poner un prefijo: detrás de una cifra (o coma) y solo uno */
static int admite_prefijo(const char *e)
{
    size_t n;
    const char *q;
    if (strchr(e, 'j')) e = strchr(e, 'j') + 1;        /* R + jX: el de la parte imaginaria */
    n = strlen(e);
    if (n == 0) return 0;
    for (q = "pnmkMG\xB5"; *q; q++) if (strchr(e, *q)) return 0;
    return (e[n - 1] >= '0' && e[n - 1] <= '9') || e[n - 1] == ',';
}

/* Lo que cabe en la caja: una cuenta, 31; lo demás, 19 (como siempre) */
static size_t edit_max(void) { return cuentas_aqui() ? sizeof edit - 1 : EDIT_OTROS; }

static void mete_char(char ch)
{
    size_t n;
    aviso_r = 0;
    if (!editando) { editando = 1; edit[0] = 0; }
    n = strlen(edit);
    if (n >= edit_max()) return;
    if (sub == 1 && n >= 7) return;               /* la caja del ángulo */
    edit[n] = ch;
    edit[n + 1] = 0;
}

/* Los signos de una cuenta (cuentas_aqui): + - × ÷ ^ x² ( ). Cada uno entra
 * solo donde tiene sentido (un signo detrás de un número o de «)», «(» al
 * principio o detrás de un signo, «)» si hay uno abierto): lo escrito es
 * siempre el principio de una cuenta. El - al principio y tras la E sigue
 * sus reglas de siempre (tecla_elemento). 1 si atendió t */
static int mete_cuenta(int t)
{
    size_t n;
    char u;
    int fin;
    if (!cuentas_aqui()) return 0;
    n = editando ? strlen(edit) : 0;
    u = n ? edit[n - 1] : 0;
    fin = n > 0 && fin_operando(u);
    switch (t) {
    case T_MAS:   if (fin) mete_char('+'); return 1;
    case T_POR:   if (fin) mete_char((char)0xD7); return 1;          /* × */
    case T_ENTRE: if (fin) mete_char((char)0xF7); return 1;          /* ÷ */
    case T_POT:   if (fin) mete_char('^'); return 1;
    case T_CUAD:  if (fin && n + 2 <= edit_max()) { mete_char('^'); mete_char('2'); } return 1;
    case T_PAR_A: if (abre_operando(n)) mete_char('('); return 1;
    case T_PAR_C: if (fin && abiertos() > 0) mete_char(')'); return 1;
    case T_MENOS: case T_SIGNO:
        if (n == 0 || u == 'E') return 0;         /* el del número o el del exponente */
        /* detrás de un número, la resta; detrás de «(», × ÷ o ^, el signo de
         * lo que sigue (tras + o -, nada: «2+-3» no) */
        if (fin || u == '(' || u == (char)0xD7 || u == (char)0xF7 || u == '^') mete_char('-');
        return 1;
    default: return 0;
    }
}

/* --------------------------------------------------------------------------
 *  2. Aprende: lecciones, teoría, ejemplos
 * ----------------------------------------------------------------------- */
static void dib_lecciones(void)
{
    static const char *items[16];
    const Leccion *l = &LECCIONES[sel_lec];
    char t[80];
    Buf b;
    int i, y = BARRA_H + 6 + N_LECCIONES * LIN_H + 4;

    for (i = 0; i < N_LECCIONES && i < 16; i++) items[i] = LECCIONES[i].titulo;
    barra("Aprende: teoría y ejemplos", 0);
    lista(BARRA_H + 6, N_LECCIONES, items, N_LECCIONES, sel_lec, 0);
    separador(y);
    gfx_text(8, y + 6, l->resumen, C_TEXTO, 1);
    buf_init(&b, t, sizeof t);
    if (l->npag) {
        buf_int(&b, l->npag);
        buf_str(&b, l->npag == 1 ? " página de teoría y " : " páginas de teoría y ");
    }
    if (l->nmod > 0) {                          /* las de circuitos modelo (10 a 13) */
        buf_int(&b, l->nmod);
        buf_str(&b, l->nmod == 1 ? " circuito modelo." : " circuitos modelo.");
    } else {
        buf_int(&b, l->nej);
        buf_str(&b, l->nej == 1 ? " ejemplo resuelto." : " ejemplos resueltos.");
    }
    gfx_text(8, y + 6 + LIN_H, t, C_SUAVE, 1);
    pie("[arriba/abajo] elegir   [enter] entrar   [esc]");
}

static void dib_teoria(void)
{
    const Leccion *l = &LECCIONES[sel_lec];
    const Pagina *p = &l->pag[pag];
    char t[16];
    Buf b;
    int y;

    buf_init(&b, t, sizeof t);
    buf_int(&b, pag + 1); buf_ch(&b, '/'); buf_int(&b, l->npag);
    barra(l->titulo, t);
    dib_figura(p);
    y = (p->fig != FIG_NADA) ? TE_TEXTO : BARRA_H + 6;
    envuelve(8, y, COLS, p->texto, C_TEXTO, (PIE_Y - y) / LIN_H);
    pie(pag < l->npag - 1 ? "[izq/der] página   [enter] ejemplos   [esc]"
                          : "[izq] página  [der/enter] a los ejemplos  [esc]");
}

#define EJ_VIS 5
/* La lista de una lección de circuitos modelo: se emulan */
static void dib_modelos(const Leccion *l)
{
    static const char *items[16];
    static Circuito c;
    const CircModelo *md;
    char t[80];
    Buf b;
    int i, y;

    for (i = 0; i < l->nmod && i < 16; i++) items[i] = MODELOS[(int)l->mod[i]].titulo;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Modelos: ");
    buf_str(&b, l->titulo + 4);
    barra(t, 0);
    scroll = desplaza(sel_ejl, scroll, EJ_VIS);
    lista(BARRA_H + 5, EJ_VIS, items, l->nmod, sel_ejl, scroll);
    y = BARRA_H + 5 + EJ_VIS * LIN_H + 2;
    separador(y);
    md = &MODELOS[(int)l->mod[sel_ejl]];
    y += 4;
    y += envuelve(8, y, COLS, md->enunciado, C_TEXTO, 3) * LIN_H;
    y += envuelve(8, y, COLS, md->que_ver, C_TITULO, 2) * LIN_H;
    modelo_cargar(l->mod[sel_ejl], &c);
    miniatura(&c, 0, y, SCR_W, PIE_Y - y);
    pie(md->ca == 3 ? "[enter] calcular   [tab] al Taller   [esc]" :
        md->ca == 4 ? "[enter] barrer   [tab] al Taller   [esc]" :
        md->ca ? "[enter] analizar   [tab] al Taller   [esc]" : "[enter] emular   [tab] al Taller   [esc]");
}

static void dib_ejemplos(void)
{
    static const char *items[16];
    static Circuito c;
    const Leccion *l = &LECCIONES[sel_lec];
    const Ejemplo *e;
    char t[80];
    Buf b;
    int i, y;

    if (l->nmod > 0) { dib_modelos(l); return; }

    for (i = 0; i < l->nej && i < 16; i++) items[i] = EJEMPLOS[(int)l->ej[i]].titulo;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Ejemplos: ");
    buf_str(&b, l->titulo + 3);
    barra(t, lista_practica ? "Práctica" : 0);
    scroll = desplaza(sel_ejl, scroll, EJ_VIS);
    lista(BARRA_H + 5, EJ_VIS, items, l->nej, sel_ejl, scroll);
    y = BARRA_H + 5 + EJ_VIS * LIN_H + 2;
    separador(y);

    e = &EJEMPLOS[(int)l->ej[sel_ejl]];
    y += 4;
    y += envuelve(8, y, COLS, e->enunciado, C_TEXTO, 3) * LIN_H;
    if (lista_practica)                   /* la nota dice la solución */
        y += envuelve(8, y, COLS, "(La explicación, al terminar.)", C_SUAVE, 1) * LIN_H;
    else
        y += envuelve(8, y, COLS, e->nota, C_TITULO, 2) * LIN_H;
    ejemplo_cargar(l->ej[sel_ejl], &c);
    miniatura(&c, 0, y, SCR_W, PIE_Y - y);
    pie(lista_practica ? "[enter] practicar [?!] ver soluciones [esc]"
                       : "[enter] resolver [?!] practicar [tab] Taller [esc]");
}

/* --------------------------------------------------------------------------
 *  3. Ayuda
 * ----------------------------------------------------------------------- */
/* La sección de los capacitores equivalentes (rev10; v3 en rev12), al
 * final, tras ACERCA DE: las pruebas de antes no cuentan sus líneas
 * (ayuda_max, ui_cpm_oculto) y ven la Ayuda de siempre, hasta el mismo
 * final. Cabe entera en una pantalla */
#define AYUDA_CPM \
    "\n" \
    "\n" \
    "CAPACITORES EQUIVALENTES (TALLER, CUARTA OPCIÓN)\n" \
    "Sustituye un capacitor (el de un motor) con los de\n" \
    "la bodega: uno, dos o tres, en serie o paralelo. Su\n" \
    "menú: Resolver, Teoría y Ejemplos. En Resolver: el\n" \
    "valor en MF (" S_MU "F), la tolerancia (vacía: exacto a un\n" \
    "decimal), la bodega ([espacio] separa y 7 7 son\n" \
    "dos; 15-42 es un rango y sin 21-24 los quita; [A]\n" \
    "y [B], las del pizarrón) y Capacitores (Auto, o 2\n" \
    "o 3 si el profe lo pide). [izq/der] mueven el\n" \
    "cursor, [shift] salta de valor, [ctrl] a un extremo.\n" \
    "[enter]: las pestañas Opciones y Paso a paso\n" \
    "([izq/der] o [tab]); [arr/ab] elige y [enter]\n" \
    "dibuja. Ejemplo: 17 MF con la bodega A da\n" \
    "10 " S_PAR " 7 = 17 " S_CHECK "."
static const char *const AYUDA =
    "EL PROGRAMA TIENE TRES PARTES\n"
    "Taller: dibujas tu circuito; lo emula o despeja.\n"
    "Aprende: lecciones y circuitos modelo.\n"
    "Ayuda: esta página.\n"
    "\n"
    "DIBUJAR EN EL TALLER\n"
    "1. Mueve el cursor (cuadrito amarillo) con las\n"
    "   flechas.\n"
    "2. Pulsa una herramienta y después una flecha:\n"
    "   [R] resistencia   [E] fuente (+ hacia la flecha)\n"
    "   [J] fuente de corriente (la flecha, su sentido)\n"
    "   [V] voltímetro    [A] amperímetro\n"
    "   [W] cable: tira tramos hasta que pulses [esc]\n"
    "   [del] borrar: igual, hasta [esc]\n"
    "   [C] condensador  [L] bobina  [S] interruptor\n"
    "   [O] sonda del osciloscopio\n"
    "   [Z] impedancia    vatímetro en la paleta\n"
    "   [D] diodo  [U] amp. op.  [Q] BJT  [M] MOSFET\n"
    "   zener, LED y fuentes controladas, en la paleta\n"
    "   [catálogo] (el libro): todo, con su tecla.\n"
    "3. Al poner un elemento se abre su ficha:\n"
    "   un número = dato;  [?!] = lo que buscas;\n"
    "   [del] = no se sabe. En la fuente, I es la\n"
    "   corriente total que entrega y r su resistencia\n"
    "   interna (vacía = ideal). En un instrumento,\n"
    "   R vacía = ideal.\n"
    "4. Para cambiar un elemento: [enter] junto a él\n"
    "   (y una flecha si hay varios), o [tab] hasta\n"
    "   elegirlo y [enter]. En la ficha de CC, [tab]\n"
    "   pasa a la del siguiente ([shift][tab], a la del\n"
    "   anterior) y [menu] abre las opciones.\n"
    "5. [=]: con [?!] despeja; sin [?!], calcula, emula\n"
    "   o analiza en fasores (ver abajo). Si falta algo,\n"
    "   [esc] vuelve con el cursor en el elemento que\n"
    "   falla: [enter] abre su ficha en esa fila.\n"
    "\n"
    "EMULAR O CALCULAR: LA TECLA [=]\n"
    "Si marcaste algo con [?!], [=] CALCULA en CC y\n"
    "despeja lo que buscas (lo de siempre). Si no, y\n"
    "el circuito cambia con el tiempo (C, L, S, una\n"
    "fuente que no es continua, una sonda o un\n"
    "multímetro en CA), [=] lo EMULA: lo deja correr\n"
    "como en el laboratorio y enseña el osciloscopio.\n"
    "Un circuito de CC sin [?!] se calcula entero.\n"
    "\n"
    "COMPONENTES QUE CAMBIAN CON EL TIEMPO\n"
    "C: V0 es su tensión al empezar, con el + en el\n"
    "   borne donde empezaste a ponerlo. Nuevo: V0 = 0.\n"
    "   [del] en V0: la de CC (la que tendría ya).\n"
    "L: I0 es su corriente al empezar, de a hacia b.\n"
    "   Nueva: I0 = 0; [del]: la de CC.\n"
    "S: [izq/der] en la ficha: abierto o cerrado al\n"
    "   empezar. t1: cuándo cambia; t2: cuándo vuelve.\n"
    "E y J: [izq/der] en la ficha elige la forma:\n"
    "   continua, senoidal, cuadrada, triangular,\n"
    "   pulso o alterna (el fasor de la CA). Antes de\n"
    "   t = 0 están en reposo (el nivel de CC, o V1):\n"
    "   el generador se enciende en 0.\n"
    "[catálogo] en la ficha: las ecuaciones que usa el\n"
    "emulador. En la calculadora (CC), un C es un\n"
    "abierto, una L un cable y un S lo que sea al\n"
    "empezar.\n"
    "\n"
    "SONDAS\n"
    "[O] y luego [enter]: la tensión de ese punto\n"
    "(respecto a la tierra). [O] y una flecha: la\n"
    "corriente del elemento que hay en esa dirección;\n"
    "otra vez, su tensión; otra vez, se quita. Hay dos\n"
    "canales; una tercera sonda sustituye al 2. Sin\n"
    "sondas, el emulador elige: la V del primer C, o\n"
    "la I de la primera L, y la fuente que varía.\n"
    "\n"
    "EL OSCILOSCOPIO\n"
    "[+][-] tiempo por división; [arr][ab] voltios por\n"
    "división del canal activo ([shift]: su posición);\n"
    "[C] cambia de canal y [del] lo oculta.\n"
    "[izq/der] mueve la ventana ([shift]: diez veces).\n"
    "[X] cursores: [izq/der] los mueven, [enter] cambia\n"
    "de T1 a T2; escribe un tiempo (1 [M] = 1 ms) y\n"
    "[enter] lleva T1 ahí. [P] el siguiente pico\n"
    "([shift][P], el mínimo). [M] medidas o cursores;\n"
    "con cursores, las medidas son entre T1 y T2.\n"
    "[U] el último periodo (el régimen permanente):\n"
    "T1 y T2 en sus bordes, y sus medidas.\n"
    "[D] disparo: subida, bajada o libre, al nivel\n"
    "medio de lo que se ve. [A] autoescala. [=] emula\n"
    "hasta donde miras, o más fino (en T1-T2 si hay\n"
    "cursores). [tab] la tabla; [menu] tiempo y paso.\n"
    "\n"
    "MULTÍMETRO\n"
    "Un V o un A marca, según su modo ([izq/der] en\n"
    "la ficha): CC, la media; CA, el eficaz de la parte\n"
    "alterna (como Multisim); CA+CC, el eficaz total.\n"
    "Se mide en los últimos periodos enteros de la\n"
    "ventana (o en el último 10 % si nada se repite).\n"
    "\n"
    "TABLA DE LA EMULACIÓN\n"
    "Cinco páginas ([izq/der]): los valores al final,\n"
    "los máximos, los eficaces con la P media, la\n"
    "energía de cada C y L con el balance (entregada,\n"
    "disipada, almacenada y la diferencia: saltos y\n"
    "método), y la respuesta natural: s, " S_TAU ", fd y " S_ZETA ".\n"
    "\n"
    "CORRIENTE ALTERNA: LA FUENTE\n"
    "En la ficha de una E o una J, [izq] elige la\n"
    "forma \"alterna (fasor)\": E es su fasor, módulo y\n"
    "ángulo. Escribe el módulo, [tab] y el ángulo en\n"
    "grados: 120 [tab] 30 es 120" S_ANG "30°. Sin ángulo, 0°.\n"
    "Al empezar son eficaces con coseno:\n"
    "e(t) = " S_SQRT "2·120·cos(" S_OMEGA "t + 30°). La f es la de todo\n"
    "el circuito (su fila en la ficha; 60 Hz al\n"
    "empezar). [menu] > Ajustes de CA: la f y el\n"
    "convenio (pico o eficaz, coseno o seno); lo\n"
    "guardado no cambia. La forma \"senoidal\" es otra\n"
    "cosa: el generador del emulador, en pico y seno.\n"
    "\n"
    "QUÉ HACE [=]\n"
    "Con una fuente alterna o una Z, [=] analiza en CA\n"
    "(fasores, régimen permanente) y, con [?!], la\n"
    "calculadora de CA despeja. Sin alterna: con [?!]\n"
    "calcula en CC y, si el circuito cambia con el\n"
    "tiempo, lo emula. La línea de abajo del editor\n"
    "dice qué hará. [B] traza el Bode. En el resultado\n"
    "de CA, [T] da Thévenin y [O] lo emula.\n"
    "Una E continua en un circuito de CA cuenta\n"
    "apagada: las dos juntas se ven emulando ([O]).\n"
    "\n"
    "CALCULADORA DE CA\n"
    "Con [?!] despeja hasta 4 valores: R, L o C; |Z|\n"
    "o su ángulo; |E| (|J|) o su ángulo; la f ([?!]\n"
    "en su fila). Datos: V e I de R, C, L, Z y\n"
    "fuentes, en fasor (cuenta 2) o solo el módulo\n"
    "(cuenta 1); lo que marca un V o un A (1); la P y\n"
    "el fp de un vatímetro (1 y 1). Un módulo 0 cuenta\n"
    "2: el equilibrio de un puente. [?!] en una V o\n"
    "una I no es incógnita: es lo que quieres ver.\n"
    "En la caja del módulo, [?!] pide el fasor entero;\n"
    "[tab] y [?!], solo el ángulo. Sin datos de fase,\n"
    "el ángulo de la fuente vale 0°.\n"
    "Enseña todas las soluciones ([arr/ab]) y, si no\n"
    "hay, dice por qué: faltan datos, no se alcanzan\n"
    "(y qué rango sí), solo en el borde...\n"
    "Corregir el fp: la P y el fp que quieres en el W\n"
    "de la línea, y [?!] en la C en paralelo.\n"
    "\n"
    "RESULTADOS DE CA\n"
    "[izq/der] capa: valores, corrientes, tensiones,\n"
    "potencias, potenciales (con la letra del nodo) o\n"
    "nombres. [.] polar o rectangular. [tab] tabla:\n"
    "fasores, potencias, totales (Boucherot y\n"
    "Tellegen), nodos y en el tiempo: v(t) e i(t) de\n"
    "pico. [enter] diagrama fasorial. [T] Thévenin\n"
    "(con A y B). [B] Bode. [O] lo emula en el tiempo:\n"
    "el régimen permanente es el fasor. [menu] f y\n"
    "convenio. [=] vuelve a analizar (o a calcular).\n"
    "\n"
    "DIAGRAMA FASORIAL Y POTENCIAS\n"
    "[arr/ab] elige un fasor; [enter] lo enseña u\n"
    "oculta. [R] lo toma de referencia (0°); otra vez,\n"
    "se quita. [S] suma los de su familia uno tras\n"
    "otro (Kirchhoff a la vista): en serie, las V\n"
    "acaban en la E. [+][-] escala de su familia; [A]\n"
    "automática. [izq/der] el triángulo de potencias:\n"
    "P, Q, S y " S_PHI "; [S], el de Boucherot (cada carga,\n"
    "una tras otra). [.] polar o rectangular.\n"
    "\n"
    "TRAZADOR DE BODE\n"
    "[B] en el editor o en el resultado de CA, o\n"
    "[menu] > Bode. |H| en dB y su fase, con f en\n"
    "escala logarítmica; H = salida/entrada. Entrada:\n"
    "una E o una J ([F] la siguiente). Salida: sonda\n"
    "1, sonda 2, V(AB) o la automática ([C] cambia).\n"
    "[izq/der] mueve el cursor ([shift]: una década);\n"
    "un número y [enter] lo llevan a esa f. [X] otro\n"
    "cursor ([enter] pasa de uno a otro). [M] las\n"
    "marcas (cortes, f0); [P] el pico ([shift]: el\n"
    "valle). [arr/ab] dB por división; [+][-] acerca\n"
    "o aleja; [A] todo; [=] barre otra vez lo que se\n"
    "ve. [tab] los fasores a la f del cursor. [menu]\n"
    "el rango (vacío = automático).\n"
    "\n"
    "DIODOS, ZENER Y LED\n"
    "[D] y una flecha: el diodo, del ánodo (donde\n"
    "empiezas) al cátodo (la raya). En la ficha,\n"
    "[izq/der] elige su modelo: ideal (un\n"
    "interruptor), con V" S_GAMMA " (el del libro: V" S_GAMMA " y rd),\n"
    "exponencial (Shockley: Is, n y Rs), 1N4148 o\n"
    "1N4007. El zener (Vz a Iz y rz) y el LED (su\n"
    "color, Vf a If e Im) están en la paleta. El LED\n"
    "se enciende en el resultado y al emular: brilla\n"
    "con la corriente media; con más de Im, se\n"
    "quemaría. [catálogo] en la ficha: su modelo; [I]\n"
    "ahí: su curva I-V.\n"
    "\n"
    "[=] CON SEMICONDUCTORES Y AO\n"
    "Sin [?!], [=] da el punto de operación de CC\n"
    "(Newton), con cada no lineal en su región:\n"
    "conduce, corte, ruptura, lineal o satura. Si el\n"
    "circuito cambia con el tiempo, lo emula (un\n"
    "rectificador, un astable). Un [?!] en un valor\n"
    "no se despeja: barre con [I] y lee con [N]. [P]\n"
    "da el punto de operación de CC de cualquier\n"
    "circuito (C abiertos, L cables). [B]: el Bode de\n"
    "pequeña señal en ese punto.\n"
    "\n"
    "BARRIDO DE CC\n"
    "[I] (o [menu] > Barrido) cambia una E, una J, una\n"
    "R o una ganancia y dibuja sus canales: las\n"
    "sondas, o la I y la V del primer diodo, o la\n"
    "salida del AO. [izq/der] el cursor; [N], un\n"
    "nivel y [enter]: el x donde se cruza (con un\n"
    "dato, el cursor ya está ahí). [X] dos cursores;\n"
    "[Y] X-Y; [L] escala log; [+][-] acerca o aleja;\n"
    "[=] barre lo que se ve; [tab] el circuito en el\n"
    "cursor; [menu] qué se barre, el rango y la ida y\n"
    "vuelta (la histéresis de un Schmitt).\n"
    "\n"
    "FUENTES CONTROLADAS\n"
    "La E y la J controladas (el rombo) están en la\n"
    "paleta. Su tensión o su corriente la manda otro\n"
    "elemento de dos bornes: en su ficha, fila x,\n"
    "[+][-] lo elige e [izq/der] el tipo: e = " S_MU S_CDOT "V(x),\n"
    "e = r" S_CDOT "I(x), j = g" S_CDOT "V(x) o j = " S_BETA S_CDOT "I(x). Son lineales:\n"
    "la calculadora despeja su ganancia con [?!].\n"
    "\n"
    "AMPLIFICADOR OPERACIONAL\n"
    "[U] y una flecha: el AO, con la flecha hacia la\n"
    "salida y + y - detrás ([F] los cambia). Ideal\n"
    "(sin corriente de entrada; en lineal, v+ = v-) o\n"
    "real (A, Ri y Ro). Satura en V+ y V- (" S_PM "15 V si\n"
    "no se dice). Su salida se mide desde la tierra:\n"
    "pon [G]. Con realimentación negativa trabaja en\n"
    "lineal; con positiva salta a un raíl\n"
    "(comparador, Schmitt, astable).\n"
    "\n"
    "TRANSISTORES\n"
    "[Q] y una flecha: el BJT, de la base hacia C y\n"
    "E ([F] los cambia). En la ficha, [izq/der]: NPN\n"
    "o PNP; " S_BETA ", Is, VA (Early; vacía, sin él) y " S_BETA "r.\n"
    "[M] y una flecha: el MOSFET, de la puerta G hacia\n"
    "D y S; canal N o P, Vt, K y VA. Es simétrico:\n"
    "con VDS < 0, D y S cambian.\n"
    "La ficha da V e I: VCE e IC (VDS e ID); la tabla\n"
    "del resultado, además VBE e IB (VGS) y la región:\n"
    "activa, corte, saturación, inversa u óhmica.\n"
    "[catálogo] en la ficha: su modelo; [I] ahí: su\n"
    "familia de 6 curvas, IC(VCE) con IB en pasos o\n"
    "ID(VDS) con VGS en pasos. [B]: el Bode de\n"
    "pequeña señal en el punto de operación.\n"
    "\n"
    "VATÍMETRO Y OHMÍMETRO\n"
    "El vatímetro está en la paleta ([catálogo]), con\n"
    "la flecha hacia la carga. Su bobina de corriente\n"
    "va en serie (un cable) y la de tensión, de su\n"
    "salida a la tierra ([G]; sin tierra, al - de la\n"
    "primera fuente). Marca P = Re(V·I*) y el fp: +\n"
    "inductivo, - capacitivo. Sus datos son de la CA.\n"
    "Para medir la Z de una red sin fuentes, pon una J\n"
    "alterna de 1 A entre A y B: V(AB) vale Z(AB).\n"
    "Un V o un A nuevo en CA marca el eficaz.\n"
    "\n"
    "THÉVENIN Y NORTON\n"
    "Pon A y B con [T] en el editor. En el resultado\n"
    "de CC o de CA, [T] da Vth, Zth e IN, los dos\n"
    "equivalentes y la carga de máxima potencia:\n"
    "ZL = Zth* (en CC, RL = Rth), y la mejor si solo\n"
    "puede ser una R.\n"
    "\n"
    "IMPEDANCIA Z\n"
    "[Z] y una flecha ponen una Z = R + jX: 30 [+] 40\n"
    "es 30+j40 ([-] da -j; [J] 40, j40), o el módulo,\n"
    "[tab] y su ángulo (50 [tab] 36,87). Es la Z a la\n"
    "f del circuito; en el Bode, X cambia como una L\n"
    "(X > 0) o una C (X < 0). No existe en el tiempo:\n"
    "para emularla, dibújala como R y L (o R y C).\n"
    "\n"
    "LA REGLA\n"
    "Hacen falta tantos datos de V o I como valores\n"
    "desconocidos (R, E, J o r). La línea de abajo\n"
    "del editor lo cuenta: en verde, el problema\n"
    "está listo para resolver.\n"
    "\n"
    "OTRAS TECLAS DEL TALLER\n"
    "[ctrl][Z] deshacer   [ctrl][Y] rehacer\n"
    "[ctrl][S] guardar    [F] girar la fuente elegida\n"
    "[ctrl][C] copia el elegido con sus valores y\n"
    "[ctrl][X] lo corta; [ctrl][V] y una flecha ponen\n"
    "la copia (con otra etiqueta) y abren su ficha.\n"
    "[G] tierra (0 V) en el cursor; [T] terminal A,\n"
    "otra vez en otro punto, el B: da V(AB).\n"
    "[B] traza el Bode.\n"
    "[I] barrido de CC  [P] punto de operación\n"
    "[menu] opciones: resolver o emular, practicar,\n"
    "deshacer, guardar (o una copia), borrar todo,\n"
    "ayuda, el tiempo y el paso de la emulación (en\n"
    "CA, la f y el convenio) y el Bode.\n"
    "[shift] y [ctrl] van con la tecla: a la vez, o\n"
    "antes (quedan armados para la siguiente).\n"
    "\n"
    "CONTROLES\n"
    "Cada tecla actúa al bajarla. Las flechas, [del] y\n"
    "las escalas se repiten si las mantienes; [esc],\n"
    "[enter], [tab], [menu], [=] y las letras, nunca.\n"
    "[shift] y una flecha: un salto (una página, 28\n"
    "puntos, hasta el siguiente borne); [ctrl]: hasta\n"
    "el extremo (el principio, el final, el borde).\n"
    "En el Bode y el barrido, mientras el cursor corre\n"
    "la lectura sale con " S_APROX "; al soltar, la exacta.\n"
    "Touchpad: un clic en un borde es su flecha (en\n"
    "una esquina, la del lado que manda) y en el\n"
    "centro, [enter]. Deslizar el dedo sin hundirlo\n"
    "mueve el cursor, las listas y la Ayuda.\n"
    "[ctrl][menu]: el panel Controles (deslizar, la\n"
    "repetición y el clic central).\n"
    "\n"
    "DESHACER, GUARDAR Y SEGUIR OTRO DÍA\n"
    "Mirar una ficha sin cambiar nada no gasta un\n"
    "deshacer. Poner una pieza o un cable encima de\n"
    "un elemento lo sustituye, y lo dice: [ctrl][Z]\n"
    "lo recupera. Problema nuevo, abrir otro o traer\n"
    "un ejemplo tampoco pierden el de antes. En la\n"
    "ficha o con la paleta abierta, [ctrl] con Z, Y,\n"
    "S, C, X, V o N vale igual que en el editor.\n"
    "Guardar: un ejemplo traído conserva su nombre;\n"
    "[ctrl][N] le pone uno y lo guarda; en Mis\n"
    "problemas, [N] cambia el del elegido.\n"
    "[menu] > Guardar como copia lo pone en otro\n"
    "hueco y sigues con la copia. Al salir del\n"
    "programa, el circuito se guarda solo: Taller >\n"
    "Seguir lo recupera la próxima vez.\n"
    "\n"
    "VALORES\n"
    "Prefijos detrás del número: [P] pico, [N] nano,\n"
    "[U] micro, [M] mili, [K] kilo, [shift][M] mega,\n"
    "[G] giga. 4.7 y [K] = 4,7 k" S_OHM ". [EE] = exponente.\n"
    "Debajo del campo se ve cuánto vale lo escrito.\n"
    "Cuentas: + - " S_TIMES " " S_DIVIDE " ^ ( ) y x" S_SUP2 ". 220^2/100 da 484 y\n"
    "1/(1/4k+1/6k), 2,4 k: se guarda el resultado.\n"
    "\n"
    "EN EL RESULTADO\n"
    "[izq/der] cambia lo que se ve en el dibujo:\n"
    "valores, corrientes, tensiones, potencias,\n"
    "potenciales de nodo (con su letra: B 7,5V) o\n"
    "nombres. [tab] tabla completa: P abs es la que\n"
    "absorbe cada uno (negativa si entrega; en una\n"
    "fuente real, la de bornes y no la de la fem), y\n"
    "abajo el balance: entregan = absorben.\n"
    "[T] Thévenin (con A y B). [enter] el\n"
    "procedimiento paso a paso, con el dibujo\n"
    "resaltando lo que usa cada paso (en Kirchhoff,\n"
    "las letras de los nodos). Si el resultado es\n"
    "una fracción exacta, se muestra: 440/7.\n"
    "\n"
    "PRACTICAR\n"
    "En el editor, [?!] (o [menu] > Practicar) resuelve\n"
    "con las respuestas tapadas: escribe tu número y\n"
    "[enter]; te dice si está bien. En los ejemplos,\n"
    "[?!] tapa las explicaciones de la lista. En el\n"
    "paso a paso, cada paso se destapa con [enter].\n"
    "\n"
    "ACERCA DE\n"
    APP_NOMBRE ", versión " APP_VERSION ", para TI-Nspire CX II\n"
    "con Ndless. " APP_SIGLA ": " APP_SIGLA_L ".\n"
    "Hakaru (el kanji de la portada) quiere decir\n"
    "\"medir\" en japonés.\n"
    "Calculadora de CC y CA; emulador en el tiempo y\n"
    "en frecuencia; semiconductores, AO y barrido de CC.\n"
    "Autor: Eliam Mojardin Ibarra. Licencia MIT.\n"
    "Letra: Terminus Font, (c) 2010 Dimitar Toshkov\n"
    "Zhekov, licencia SIL Open Font License 1.1.\n"
    "Kanji: Noto Sans JP, (c) 2014-2021 Adobe, con\n"
    "la misma licencia."
    ESP_AYUDA
    AYUDA_CPM;

static int lineas_de(const char *s)
{
    int n = 1;
    while (*s) if (*s++ == '\n') n++;
    return n;
}

/* Las líneas que se ven de la Ayuda (15) y hasta dónde baja el scroll: las
 * cotas son de las teclas (así no dependen de si se dibujó entre una y otra) */
#define AYUDA_VIS ((PIE_Y - BARRA_H - 6) / LIN_H)
static int ayuda_max(void)
{
    int m = lineas_de(AYUDA) - (cpm_oculto ? lineas_de(AYUDA_CPM) - 1 : 0) - AYUDA_VIS;
    return m > 0 ? m : 0;
}

static void dib_ayuda(void)
{
    const char *s = AYUDA;
    int i, vis = AYUDA_VIS, tot = lineas_de(AYUDA);
    if (scroll > tot - vis) scroll = tot - vis;
    if (scroll < 0) scroll = 0;
    for (i = 0; i < scroll; i++) { while (*s && *s != '\n') s++; if (*s) s++; }
    barra("Ayuda", 0);
    envuelve(8, BARRA_H + 4, COLS, s, C_TEXTO, vis);
    pie("[arriba/abajo] desplazar            [esc] volver");
}

/* --------------------------------------------------------------------------
 *  Resolver y enseñar el resultado (común a Taller y Aprende)
 * ----------------------------------------------------------------------- */
static void resolver_y_mostrar(int desde)
{
    practica = PRAC_NO;
    res_nl = 0;
    red_extraer(&cres, &red);
    resolver(&cres, &red, &sol);
    isol = 0;
    paso_i = 0;
    capa = CAPA_VALORES;
    metodo = proced_generar(&cres, &red, &sol, 0, &pasos, 0);
    scroll = 0;
    origen = desde;
    pant = P_RESULTADO;
}

static void calcular(void)
{
    cres = circ;
    resolver_y_mostrar(P_EDITOR);
}

/* [?!] en el editor o la opción del menú: si no se pregunta nada no hay
 * qué tapar, y enseñar la solución entera sería justo lo contrario. */
static void practicar(void)
{
    int k, que, n = 0;
    for (k = 0; k < circ.nel; k++)
        for (que = 0; que < Q_N; que++)
            if (pregunta(&circ.el[k], que)) n++;
    if (circ_no_lineal(&circ)) { pon_aviso("La pr" S_aa "ctica tapada es de CC lineal.", C_CURSOR); return; }
    if (circ_alterna(&circ)) { pon_aviso("La pr" S_aa "ctica tapada es de CC.", C_CURSOR); return; }
    if (n == 0) { pon_aviso("Para practicar, marca en la ficha lo que buscas.", C_CURSOR); return; }
    calcular();
    practica_empieza();
}

/* Un valor como se escribe a mano: "= 4", "~ 1,5" o, si hay una fracción
 * corta que lo da exacto, "= 440/7 ~ 62,857". */
static void igual_exacto(Buf *b, double x, int fraccion)
{
    long p, q;
    if (fraccion && num_fraccion(x, &p, &q, 999)) {
        buf_str(b, " = ");
        buf_int(b, p); buf_ch(b, '/'); buf_int(b, q);
    }
    buf_igual(b, x);
}

/* "R4 = 4 ohm" o "I(E1) = 220/3 ~ 73,333 A" */
static void resultado_de(Buf *b, int k, int que, int fraccion)
{
    const Elemento *e = &cres.el[k];
    const Valores *v = &sol.s[isol];
    double x = valor_de(v, k, que);
    nombre_mag(b, e, que);
    igual_exacto(b, con_signo(e, que) ? x : fabs(x), fraccion);
    buf_ch(b, ' ');
    buf_str(b, unidad_q(e, que));
}

/* Cuántas cosas se preguntan (para decidir una o dos columnas) */
static int n_pedidas(void)
{
    int k, que, n = 0;
    for (k = 0; k < cres.nel; k++)
        for (que = 0; que < Q_N; que++)
            if (pregunta(&cres.el[k], que)) n++;
    return n;
}

/* --------------------------------------------------------------------------
 *  Modo práctica: el resultado se abre con las respuestas tapadas. Para cada
 *  cosa que se pregunta el alumno escribe su número; al contestar se destapa
 *  esa, con el visto bueno o con lo que puso. En el paso a paso, el
 *  resultado de cada paso se destapa con [enter]. [tab] lo destapa todo.
 * ----------------------------------------------------------------------- */
static const char *unidad_de(const Elemento *e, int que) { return unidad_q(e, que); }

static void practica_empieza(void)
{
    int k, que;
    practica = PRAC_NO; npreg = 0; sel_preg = 0; pasos_vistos = 0;
    resp[0] = 0; resp_aviso = 0;
    if (sol.estado != SOL_OK) return;
    sol_mask = (1u << sol.nsol) - 1u;
    for (k = 0; k < cres.nel; k++)
        for (que = 0; que < Q_N; que++)
            if (pregunta(&cres.el[k], que) && npreg < MAX_PREG) {
                preg[npreg].k = (unsigned char)k;
                preg[npreg].que = (unsigned char)que;
                preg[npreg].estado = PR_SIN;
                preg[npreg].resp = 0.0;
                npreg++;
            }
    if (npreg > 0) practica = PRAC_TAPADO;
}

static int practica_cuenta(int estado)
{
    int i, n = 0;
    for (i = 0; i < npreg; i++) if (preg[i].estado == estado) n++;
    return n;
}

/* Soluciones con las que coincide la respuesta (al 1 %). Si hay varias,
 * vale cualquiera, pero TODAS las respuestas tienen que salir de la misma:
 * sol_mask se va quedando con las que cuadran con todo lo contestado. */
static unsigned coincide(const Pregunta *p, double v)
{
    unsigned m = 0;
    int i;
    for (i = 0; i < sol.nsol; i++) {
        const Valores *vv = &sol.s[i];
        double real = fabs(valor_de(vv, p->k, p->que));
        if (fabs(fabs(v) - real) <= 0.01 * real + 1e-12) m |= 1u << i;
    }
    return m;
}

/* Al terminar, se enseña una solución que cuadre con lo contestado */
static void practica_termina(void)
{
    int i;
    practica = PRAC_HECHO;
    sel_preg = 0;
    for (i = 0; i < sol.nsol; i++)
        if (sol_mask & (1u << i)) {
            if (i != isol) {
                isol = i;
                metodo = proced_generar(&cres, &red, &sol, isol, &pasos, 0);
            }
            break;
        }
}

static void practica_siguiente(void)
{
    int i;
    for (i = 1; i <= npreg; i++) {
        int j = (sel_preg + i) % npreg;
        if (preg[j].estado == PR_SIN) { sel_preg = j; return; }
    }
    practica_termina();                         /* ya contestó todo */
}

static void practica_responde(void)
{
    Pregunta *p = &preg[sel_preg];
    int ok;
    double v = num_lee_ing(resp, &ok);
    resp[0] = 0;
    if (!ok) { resp_aviso = 1; return; }
    p->resp = fabs(v);
    if (coincide(p, v) & sol_mask) { sol_mask &= coincide(p, v); p->estado = PR_BIEN; }
    else p->estado = PR_MAL;
    practica_siguiente();
}

static void practica_rinde(void)
{
    int i;
    for (i = 0; i < npreg; i++) if (preg[i].estado == PR_SIN) preg[i].estado = PR_VISTO;
    resp[0] = 0;
    practica_termina();
}

static void resp_char(char ch)
{
    size_t n = strlen(resp);
    resp_aviso = 0;
    if (preg[sel_preg].estado != PR_SIN || n + 1 >= sizeof resp) return;
    resp[n] = ch;
    resp[n + 1] = 0;
}

/* Teclas del resultado mientras las respuestas están tapadas. Devuelve 0 si
 * la tecla sigue su camino normal ([enter] sin número: los pasos; [esc]). */
static int tecla_practica(int t)
{
    size_t n = strlen(resp);
    if (t >= T_DIG(0) && t <= T_DIG(9)) resp_char((char)('0' + (t - T_DIG0)));
    else if (t == T_PUNTO) { if (!strchr(resp, ',') && !strchr(resp, 'E')) resp_char(','); }
    else if (t == T_EE)    { if (n && !strchr(resp, 'E')) resp_char('E'); }
    else if (tecla_prefijo(t)) { if (admite_prefijo(resp)) resp_char(tecla_prefijo(t)); }
    else if (t == T_SIGNO || t == T_MENOS) { if (n && resp[n - 1] == 'E') resp_char('-'); }
    else if (t == T_BORRAR) { if (n) resp[n - 1] = 0; resp_aviso = 0; }
    else if (t == T_ARRIBA) { if (sel_preg > 0) sel_preg--; resp[0] = 0; resp_aviso = 0; }
    else if (t == T_ABAJO)  { if (sel_preg < npreg - 1) sel_preg++; resp[0] = 0; resp_aviso = 0; }
    else if (t == T_ENTER && resp[0]) practica_responde();
    else if (t == T_TAB) practica_rinde();
    else if (t == T_IZQ || t == T_DER) ;         /* las capas destaparían todo */
    else return 0;
    return 1;
}

static const char *practica_rotulo(void)
{
    static char t[24];
    Buf b;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Práctica");
    if (practica == PRAC_HECHO) {
        buf_ch(&b, ' ');
        buf_int(&b, practica_cuenta(PR_BIEN));
        buf_ch(&b, '/');
        buf_int(&b, npreg);
    }
    return t;
}

/* Las preguntas, una por línea: tapadas, con el visto bueno o con lo que
 * puso el alumno. Devuelve cuántas líneas usó. */
static int dib_preguntas(int y, int vis)
{
    char t[96];
    Buf b;
    int i, ini = 0, nl = 0;
    if (sel_preg >= vis) ini = sel_preg - vis + 1;
    for (i = ini; i < npreg && i < ini + vis; i++, y += LIN_H, nl++) {
        const Pregunta *p = &preg[i];
        const Elemento *e = &cres.el[p->k];
        u16 c = C_INCOG;
        buf_init(&b, t, sizeof t);
        if (p->estado == PR_BIEN)     { buf_str(&b, S_CHECK " "); c = C_OK; }
        else if (p->estado == PR_MAL) { buf_str(&b, S_TIMES " "); c = C_ERROR; }
        else if (p->estado == PR_VISTO) c = C_SUAVE;
        if (p->estado == PR_SIN) {
            nombre_mag(&b, e, p->que);
            buf_str(&b, " = ");
            if (i == sel_preg && practica == PRAC_TAPADO) { buf_str(&b, resp); buf_ch(&b, '_'); }
            else buf_ch(&b, '?');
            buf_ch(&b, ' ');
            buf_str(&b, unidad_de(e, p->que));
        } else {
            resultado_de(&b, p->k, p->que, 0);
            if (p->estado == PR_MAL) {
                size_t antes = b.n;
                buf_str(&b, "  (pusiste ");
                buf_sig(&b, p->resp);
                buf_ch(&b, ')');
                if (b.n > COLS) { t[antes] = 0; b.n = antes; }
            }
        }
        if (strlen(t) > COLS - 1) t[COLS - 1] = 0;
        if (i == sel_preg && (practica == PRAC_TAPADO || npreg > vis))
            gfx_fill(0, y - 1, SCR_W, LIN_H, C_BARRA);
        gfx_text(6, y, t, c, 1);
        /* hay más arriba o más abajo: [arr/ab] */
        if ((i == ini && ini > 0) || (i == ini + vis - 1 && i < npreg - 1))
            gfx_text(SCR_W - 9, y, i == ini && ini > 0 ? S_UARR : S_DARR, C_CURSOR, 1);
    }
    return nl;
}

static void dib_resultado(void)
{
    Marco m;
    Vista v;
    char t[212];
    Buf b;
    int y, k, col = 0;

    if (res_nl) { dib_resultado_nl(); return; }
    buf_init(&b, t, sizeof t);
    buf_str(&b, cres.nombre);
    barra(t, sol.estado != SOL_OK ? 0 : practica ? practica_rotulo() : esq_capa_nombre(capa));

    esq_vista(&v);
    v.capa = (unsigned char)(practica == PRAC_TAPADO ? CAPA_VALORES : capa);
    if (sol.estado == SOL_OK && practica != PRAC_TAPADO) v.val = &sol.s[isol];
    v.red = &red;
    esq_marco_ajustado(&cres, 0, BARRA_H, SCR_W, RS_PANEL - BARRA_H, &m);
    gfx_clip(0, BARRA_H, SCR_W, RS_PANEL - BARRA_H);
    esq_dibuja(&cres, &m, &v);
    gfx_clip_reset();

    gfx_fill(0, RS_PANEL, SCR_W, PIE_Y - RS_PANEL, C_PANEL);
    separador(RS_PANEL);
    y = RS_PANEL + 4;

    if (sol.estado != SOL_OK) {
        sol_mensaje(&cres, &red, &sol, t, sizeof t);
        envuelve(6, y, COLS, t, C_ERROR, 4);
        if (sol.estado == SOL_NO_LINEAL) {
            /* con un no lineal y un [?!]: el barrido lo halla (§4.5), si es
             * de los que se barren (como en barrido_de) */
            for (k = 0; k < cres.nel && !(barrible_ui(cres.el[k].tipo) && cres.el[k].val.est == M_BUSCAR); k++) ;
            buf_init(&b, t, sizeof t);
            if (k < cres.nel) { buf_str(&b, "[I] barrer "); buf_str(&b, cres.el[k].etiq); buf_str(&b, "   "); }
            buf_str(&b, "[esc] volver y corregirlo");
            pie(t);
            return;
        }
        if (origen == P_EDITOR) pie_error("[esc] volver al editor y corregirlo");
        else pie("[esc] volver");
        return;
    }

    /* Práctica: las preguntas, tapadas o ya contestadas */
    if (practica == PRAC_TAPADO) {
        y += dib_preguntas(y, 4) * LIN_H;
        gfx_text(6, PIE_Y - LIN_H, resp_aviso ? "Eso no es un número: escríbelo otra vez."
                                              : "[enter] sin número: los pasos, uno a uno",
                 resp_aviso ? C_ERROR : C_SUAVE, 1);
        pie("[núm][enter] responder [arr/ab] otra [tab] ver todo");
        return;
    }
    if (practica == PRAC_HECHO) y += dib_preguntas(y, 3) * LIN_H;

    /* Lo que se preguntó: una por línea, con su fracción exacta si la hay;
     * si se preguntan muchas cosas, en dos columnas y solo en decimal. */
    if (practica == PRAC_NO) {
        int una = n_pedidas() <= 3;
        for (k = 0; k < cres.nel && y < PIE_Y - 2 * LIN_H; k++) {
            const Elemento *e = &cres.el[k];
            int que;
            for (que = 0; que < Q_N; que++) {
                if (!pregunta(e, que)) continue;
                if (y >= PIE_Y - 2 * LIN_H) break;  /* el resto, en la tabla */
                buf_init(&b, t, sizeof t);
                resultado_de(&b, k, que, una);
                gfx_text(6 + col * 160, y, t, C_INCOG, 1);
                if (una || ++col == 2) { col = 0; y += LIN_H; }
            }
        }
        if (col) y += LIN_H;
    }

    if (sol.fuente >= 0) {
        const Valores *vv = &sol.s[isol];
        int f = sol.fuente;
        buf_init(&b, t, sizeof t);
        if (sol.hay_req) { buf_str(&b, "Req"); buf_igual(&b, fabs(sol.req)); buf_str(&b, " " S_OHM "  "); }
        buf_str(&b, "I"); buf_igual(&b, fabs(vv->I[f])); buf_str(&b, " A  ");
        buf_str(&b, "P"); buf_igual(&b, fabs(vv->P[f])); buf_str(&b, " W");
        if (b.n > COLS && sol.hay_req) {         /* números largos: Req aparte */
            buf_init(&b, t, sizeof t);
            buf_str(&b, "Req"); buf_igual(&b, fabs(sol.req)); buf_str(&b, " " S_OHM);
            gfx_text(6, y, t, C_TEXTO, 1);
            y += LIN_H;
            buf_init(&b, t, sizeof t);
            buf_str(&b, "I"); buf_igual(&b, fabs(vv->I[f])); buf_str(&b, " A  ");
            buf_str(&b, "P"); buf_igual(&b, fabs(vv->P[f])); buf_str(&b, " W");
        }
        gfx_text(6, y, t, C_TEXTO, 1);
        y += LIN_H;
    }
    /* Terminales A y B: la tensión entre ellos (antes que la línea de la
     * fuente: es para lo que se pusieron). Los nodos van con su letra, la
     * misma que en los pasos. */
    if (cres.term_x[0] >= 0 && cres.term_x[1] >= 0 && y + LIN_H <= PIE_Y) {
        double vab;
        int na, nb;
        buf_init(&b, t, sizeof t);
        switch (sol_vab(&cres, &red, &sol.s[isol], &vab, &na, &nb)) {
        case 1:
            buf_str(&b, "V(AB) = V"); buf_ch(&b, red_letra(&red, na));
            buf_str(&b, " - V"); buf_ch(&b, red_letra(&red, nb));
            buf_igual(&b, vab);
            buf_str(&b, " V");
            if (practica == PRAC_NO && gfx_text_w(t, 1) + 6 + 16 * 6 <= SCR_W - 12)
                gfx_text(6 + gfx_text_w(t, 1) + 12, y, "[T] Th" S_ee "venin", C_SUAVE, 1);
            break;
        case 0:  buf_str(&b, "A o B no están en un punto del circuito."); break;
        default: buf_str(&b, "A y B están en circuitos separados: no hay V(AB)."); break;
        }
        gfx_text(6, y, t, C_TENSION, 1);
        y += LIN_H;
    }
    if (sol.fuente >= 0 && cres.el[sol.fuente].val.est == M_LIBRE && y + LIN_H <= PIE_Y) {
        /* el valor que no se sabía (si se pidió, ya salió arriba) */
        int f = sol.fuente;
        buf_init(&b, t, sizeof t);
        buf_str(&b, tipo_nom_val(cres.el[f].tipo));
        buf_igual(&b, sol.s[isol].val[f]);
        buf_ch(&b, ' '); buf_str(&b, tipo_uni_val(cres.el[f].tipo));
        gfx_text(6, y, t, C_TEXTO, 1);
        y += LIN_H;
    }
    if (y == RS_PANEL + 4) {
        /* nada pedido, sin una fuente sola (Req) ni terminales: el panel
         * quedaba vacío. El balance, como al pie de la tabla */
        double ent, ab, res = sol_balance(&cres, &sol.s[isol], &ent, &ab);
        int bien = ui_fila_balance(t, (int)sizeof t, ent, ab, res);
        gfx_text(6, y, t, bien ? C_TEXTO : C_ERROR, 1);
        y += LIN_H;
    }
    if (sol.nsol > 1 && y + LIN_H <= PIE_Y) {
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Hay ");
        buf_int(&b, sol.nsol);
        buf_str(&b, " soluciones; esta es la ");
        buf_int(&b, isol + 1);
        buf_str(&b, " [arr/ab]");
        gfx_text(6, y, t, C_CURSOR, 1);
    }
    pie("[enter] paso a paso [izq/der] ver [tab] tabla");
}

/* ---- Tabla de todos los valores ---- */
/* Cada columna tiene unos 70 px: un número de más de 8 caracteres, o por
 * debajo de 0,1, se escribe con prefijo (918 µW, 3,588 MW, 13,251 mW: no
 * «0,013251 W» junto a «2,8877 mW», y un «-0,032028 W» ya no llega al borde)
 * y, fuera de p..T, con exponente (1,2E21 W). */
#define CELDA_MAX 8
static char *celda_txt(char *t, size_t cap, double v, const char *u, int signo)
{
    static const char *const pre[] = { "p", "n", S_MU, "m", "", "k", "M", "G", "T" };
    char n[48];
    Buf b;
    double a = fabs(v);
    int i = 4;

    num_sig(n, sizeof n, a);
    if (((int)strlen(n) > CELDA_MAX || a >= 1e15 || a < 0.1) && a > 0.0) {
        while (a >= 1000.0 && i < 8) { a /= 1000.0; i++; }
        while (a < 1.0 && i > 0)     { a *= 1000.0; i--; }
        num_sig(n, sizeof n, a);
        if ((int)strlen(n) > CELDA_MAX || a >= 1000.0) {   /* fuera de p..T */
            int e = (int)floor(log10(fabs(v)));
            char m[24];
            Buf bn;
            buf_init(&bn, n, sizeof n);
            buf_str(&bn, num_auto(m, sizeof m, fabs(v) / pow(10.0, e), 2));
            buf_ch(&bn, 'E');
            buf_int(&bn, e);
            i = 4;
        }
    }
    buf_init(&b, t, cap);
    if (signo && v < 0.0 && strcmp(n, "0") != 0) buf_ch(&b, '-');     /* lo que redondea a 0 no lleva signo */
    buf_str(&b, n);
    buf_ch(&b, ' ');
    buf_str(&b, pre[i]);
    buf_str(&b, u);
    return t;
}

static void celda_g(int x, int y, double v, const char *u, u16 c, int signo)
{
    char t[32];
    gfx_text(x, y, celda_txt(t, sizeof t, v, u, signo), c, 1);
}

/* En magnitud (la tabla de CC) y con su signo (la de la emulación) */
static void celda(int x, int y, double v, const char *u, u16 c) { celda_g(x, y, v, u, c, 0); }
static void celda_s(int x, int y, double v, const char *u, u16 c) { celda_g(x, y, v, u, c, 1); }

/* La fila de un vatímetro en las tablas: lo que marca, con su signo, en la
 * columna del valor («marca -9 W»; si no caben las 12 columnas, solo la
 * lectura) y en «P abs» su 0 W, como un amperímetro: no absorbe y no cuenta
 * en el balance (antes la lectura iba bajo «P abs» y la columna no sumaba) */
static void fila_vatimetro(int y, double V, double I, double P)
{
    char t[40], u[32];
    Buf b;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "marca ");
    buf_str(&b, celda_txt(u, sizeof u, P, "W", 1));
    gfx_text(40, y, strlen(t) <= 12 ? t : u, C_INSTR, 1);
    celda_s(114, y, V, "V", C_TENSION);
    celda_s(184, y, I, "A", C_CORRIENTE);
    celda_s(254, y, 0.0, "W", C_POTENCIA);
}

/* Una potencia del balance de la tabla de CC: como en la CA («36 W»,
 * «906,1 W») y, fuera de p..G, con exponente («1E21 W»: los extremos) */
static char *pot_txt(char *buf, size_t cap, double v)
{
    double a = fabs(v);
    if (a > 0.0 && (a < 1e-12 || a >= 999.95e9)) {
        char m[24];
        Buf b;
        int e = (int)floor(log10(a));
        buf_init(&b, buf, cap);
        buf_str(&b, num_auto(m, sizeof m, a / pow(10.0, (double)e), 3));
        buf_ch(&b, 'E'); buf_int(&b, e);
        buf_str(&b, " W");
        return buf;
    }
    return num_pot(buf, cap, v, "W");
}

/* La fila del balance de la tabla de CC: «Entregan 36 W · absorben 36 W ✓»
 * o, si no cuadra, el residuo relativo: «(no cuadra: 3E-5)»; si con eso
 * pasa de COLS, «(no cuadra)» o «(!)». 1 si cuadra (res <= 1e-9) */
int ui_fila_balance(char *buf, int cap, double ent, double ab, double res)
{
    char u[24];
    Buf b;
    int bien = res <= 1e-9;
    buf_init(&b, buf, (size_t)cap);
    buf_str(&b, "Entregan "); buf_str(&b, pot_txt(u, sizeof u, ent));
    buf_str(&b, " " S_CDOT " absorben "); buf_str(&b, pot_txt(u, sizeof u, ab));
    if (bien) buf_str(&b, " " S_CHECK);
    else {
        size_t n0 = b.n;
        if (res < 1e300) {
            int ex = (int)floor(log10(res));
            buf_str(&b, " (no cuadra: ");
            buf_str(&b, num_auto(u, sizeof u, res / pow(10.0, (double)ex), 1));
            buf_ch(&b, 'E'); buf_int(&b, ex);
            buf_ch(&b, ')');
        }
        if (b.n > COLS || !(res < 1e300)) {          /* con números largos (o un NaN), sin la cifra */
            b.n = n0; buf[n0] = 0;
            buf_str(&b, n0 + 12 <= COLS ? " (no cuadra)" : " (!)");
        }
    }
    return bien;
}

static void dib_tabla_nl(void);

/* Las filas de la tabla (las de dib_tabla y dib_tabla_nl: los elementos,
 * la carga o el flujo de cada C o L de dato, los segundos puertos con el
 * punto de operación y el balance) y las que se ven: las cotas del scroll
 * son de las teclas */
#define TB_VIS ((PIE_Y - BARRA_H - 20) / 14)
static int tabla_filas(void)
{
    int k, n = cres.nel + 1;
    for (k = 0; k < cres.nel; k++) {
        int tp = cres.el[k].tipo;
        if (res_nl && circ_npatas(&cres.el[k]) == 3) n++;
        if ((tp == EL_COND || tp == EL_BOB) && cres.el[k].val.est == M_DATO) n++;
    }
    return n;
}
static int tabla_max(void) { int m = tabla_filas() - TB_VIS; return m > 0 ? m : 0; }

static void dib_tabla(void)
{
    const Valores *v = &sol.s[isol];
    int k, fila, y = BARRA_H + 20, filas_vis = (PIE_Y - y) / 14, nCL = 0, hay_dyn = 0, ntot;
    int cl[MAX_EL];
    double ent, ab, res;

    if (res_nl) { dib_tabla_nl(); return; }
    res = sol_balance(&cres, v, &ent, &ab);
    /* Tras los elementos, una fila por cada C o L: su carga o flujo y su
     * energía; y al final, el balance de potencias */
    for (k = 0; k < cres.nel; k++) {
        int t = cres.el[k].tipo;
        if (t == EL_COND || t == EL_BOB || t == EL_INTER) hay_dyn = 1;
        if ((t == EL_COND || t == EL_BOB) && cres.el[k].val.est == M_DATO) cl[nCL++] = k;
    }
    ntot = cres.nel + nCL + 1;
    barra("Tabla de valores", 0);
    gfx_text(6, BARRA_H + 4, "Elem", C_SUAVE, 1);
    gfx_text(40, BARRA_H + 4, hay_dyn ? "Valor" : "R / E", C_SUAVE, 1);
    gfx_text(114, BARRA_H + 4, "V", C_SUAVE, 1);
    gfx_text(184, BARRA_H + 4, "I", C_SUAVE, 1);
    gfx_text(254, BARRA_H + 4, "P abs", C_SUAVE, 1);    /* absorbida: negativa si entrega */
    separador(BARRA_H + 17);

    if (scroll > ntot - filas_vis) scroll = ntot - filas_vis;
    if (scroll < 0) scroll = 0;
    for (fila = scroll; fila < ntot && fila < scroll + filas_vis; fila++, y += 14) {
        const Elemento *e;
        if (fila == ntot - 1) {                  /* el balance: ΣP abs = 0 */
            char t[80];
            int bien = ui_fila_balance(t, sizeof t, ent, ab, res);
            gfx_text(6, y, t, bien ? C_SUAVE : C_ERROR, 1);
            continue;
        }
        if (fila >= cres.nel) {                  /* carga o flujo, y energía */
            double x, W;
            k = cl[fila - cres.nel];
            e = &cres.el[k];
            gfx_text(6, y, e->etiq, tipo_info(e->tipo)->color, 1);
            if (e->tipo == EL_COND) {
                x = e->val.v * v->V[k]; W = 0.5 * e->val.v * v->V[k] * v->V[k];
                gfx_text(40, y, "Q", C_SUAVE, 1); celda(52, y, x, "C", C_TEXTO);
            } else {
                x = e->val.v * v->I[k]; W = 0.5 * e->val.v * v->I[k] * v->I[k];
                gfx_text(40, y, S_PHI, C_SUAVE, 1); celda(52, y, x, "Wb", C_TEXTO);
            }
            gfx_text(172, y, "W", C_SUAVE, 1); celda(184, y, W, "J", C_POTENCIA);
            continue;
        }
        k = fila;
        e = &cres.el[k];
        gfx_text(6, y, e->etiq, tipo_info(e->tipo)->color, 1);
        if (e->tipo == EL_VATI) {                /* lo que marca, no lo que absorbe (nada) */
            fila_vatimetro(y, fabs(v->V[k]), fabs(v->I[k]), v->P[k]);
            continue;
        }
        if (e->tipo == EL_INTER) gfx_text(40, y, e->variante == INTER_CERRADO ? "cerrado" : "abierto", C_SUAVE, 1);
        else if (ES_IDEAL(e)) gfx_text(40, y, e->tipo == EL_VOLT ? "ideal (" S_INF ")" : "ideal (0)", C_SUAVE, 1);
        else celda(40, y, v->val[k], unidad_val(e), e->val.est == M_BUSCAR ? C_INCOG : C_TEXTO);
        celda(114, y, v->V[k], "V", e->V.est == M_BUSCAR ? C_INCOG : C_TENSION);
        celda(184, y, v->I[k], "A", e->I.est == M_BUSCAR ? C_INCOG : C_CORRIENTE);
        {                                        /* con signo: - entrega; el ruido de un cero, 0 */
            double p = sol_p_abs(&cres, v, k);
            celda_s(254, y, fabs(p) <= 1e-12 * (ent + ab) ? 0.0 : p, "W", C_POTENCIA);
        }
    }
    pie("[arriba/abajo] desplazar   [esc] volver");
}

/* ---- Página Modelo: las ecuaciones que usa el emulador para ese tipo ---- */
/* Los que tienen curva I-V en el trazador (el BJT y el MOSFET, su familia) */
static int tiene_curva(int tipo)
{
    return tipo == EL_DIODO || tipo == EL_ZENER || tipo == EL_LED || tipo == EL_BJT || tipo == EL_MOS;
}

static void dib_modelo(void)
{
    const Elemento *e = &circ.el[sel_el];
    const TipoInfo *ti = tipo_info(e->tipo);
    char t[48];
    Buf b;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Modelo: ");
    buf_str(&b, ti->nombre);
    barra(t, 0);
    {
        /* En E y J, la ecuación de su forma debajo */
        static char txt[900];
        Buf bt;
        buf_init(&bt, txt, sizeof txt);
        buf_str(&bt, ti->modelo ? ti->modelo : "");
        if ((e->tipo == EL_FUENTE || e->tipo == EL_CORR) && e->variante < FORMA_N) buf_str(&bt, FORMA_MODELO[e->variante]);
        if (ti->modelo_var && e->variante < ti->nvar && ti->modelo_var[e->variante]) {
            buf_ch(&bt, '\n');
            buf_str(&bt, ti->modelo_var[e->variante]);
        }
        envuelve(8, BARRA_H + 6, COLS, txt, C_TEXTO, (PIE_Y - BARRA_H - 6) / LIN_H);
    }
    pie(tiene_curva(e->tipo) ? "[I] su curva I-V              [esc] a la ficha" : "[esc] a la ficha");
}

/* ---- Pasos: el esquema arriba con lo que usa cada paso resaltado ---- */
static void dib_pasos(void)
{
    Marco m;
    Vista v;
    const Paso *s;
    char t[40];
    Buf b;
    int y, i;

    barra(metodo == PROC_ARBOL ? "Paso a paso" : "Paso a paso: Kirchhoff", 0);
    if (pasos.n == 0) {
        gfx_text(6, 40, "No hay procedimiento para este resultado.", C_SUAVE, 1);
        pie("[esc] volver");
        return;
    }
    if (paso_i >= pasos.n) paso_i = pasos.n - 1;
    if (paso_i < 0) paso_i = 0;
    s = &pasos.items[paso_i];

    esq_vista(&v);
    v.capa = CAPA_NOMBRES;
    v.resalta = s->resalta;
    v.atenuar = s->resalta != 0;
    if (metodo == PROC_KIRCHHOFF) {           /* las ecuaciones hablan de VA, VB...: sus letras */
        v.red = &red;
        v.letras = 1;
        v.nodo_resalta = s->nodo;
    }
    esq_marco_ajustado(&cres, 0, BARRA_H, SCR_W, PS_PANEL - BARRA_H - 4, &m);
    gfx_clip(0, BARRA_H, SCR_W, PS_PANEL - BARRA_H);
    esq_dibuja(&cres, &m, &v);
    gfx_clip_reset();

    /* avance: un tramo por paso */
    for (i = 0; i < pasos.n; i++) {
        int x0 = 4 + i * 312 / pasos.n, x1 = 4 + (i + 1) * 312 / pasos.n - 1;
        gfx_fill(x0, PS_PANEL - 3, x1 - x0 > 1 ? x1 - x0 : 1, 2, i <= paso_i ? C_OK : C_TENUE);
    }

    gfx_fill(0, PS_PANEL, SCR_W, PIE_Y - PS_PANEL, C_PANEL);
    separador(PS_PANEL);

    buf_init(&b, t, sizeof t);
    buf_int(&b, paso_i + 1); buf_ch(&b, '/'); buf_int(&b, pasos.n);
    gfx_text(SCR_W - gfx_text_w(t, 1) - 5, PS_PANEL + 3, t, C_SUAVE, 1);
    gfx_text(6, PS_PANEL + 3, s->titulo, C_OK, 1);

    y = PS_PANEL + 17;
    if (practica == PRAC_TAPADO && paso_i >= pasos_vistos) {
        /* En práctica, el paso entero: el detalle también lleva números */
        gfx_text(6, y, "Piénsalo y pulsa [enter] para ver este paso.", C_SUAVE, 1);
        pie("[enter] destapar  [izq/der] paso  [esc] volver");
        return;
    }
    y += envuelve(6, y, COLS, s->detalle, C_TEXTO, 6) * LIN_H;
    if (s->resultado[0])
        envuelve(6, y, COLS, s->resultado, C_CORRIENTE, (PIE_Y - y) / LIN_H);

    pie("[izq/der] paso              [esc] al resultado");
}

/* --------------------------------------------------------------------------
 *  El emulador: [=] sin [?!], la marcha, el osciloscopio, la tabla, el
 *  esquema emulado y los ajustes de tiempo y paso
 * ----------------------------------------------------------------------- */
static Osc osc;
static int emu_origen = P_EDITOR;     /* a dónde vuelve [esc]               */
static int emu_pag, emu_capa;
static unsigned char osc_tipo_ant[2]; /* magnitudes de la emulación anterior */
static int aj_origen, aj_campo, aj_aviso;
static Mag aj_mag[2];                 /* t_fin y paso que se están editando  */
/* Contexto de los ajustes: el tiempo (emulador) o la alterna (f y convenio) */
enum { AJ_TIEMPO = 0, AJ_CA, AJ_BODE, AJ_BARRIDO };
static int aj_bel, aj_blog, aj_bvuelta, ab_aviso;   /* el barrido: qué se barre, escala, sentido y su aviso */
static int aj_ctx, aj_conv;           /* CA: aj_mag[0] es la f; aj_conv, el convenio */
static void reanaliza_ca(void);
static void analizar_ca(int desde);

static int reemula_cur;               /* [=] en el osciloscopio con cursores: siguen */
static void emula_cres(int desde)
{
    reemula_cur = 0;
    red_extraer(&cres, &red);
    emu_origen = desde;
    emu_pag = 0;
    emu_capa = CAPA_VALORES;
    scroll = 0;
    if (sim_iniciar(&cres, &red) != SIM_OK) { pant = P_EMU_ESQ; return; }
    trabajo = TR_EMULAR;
    pant = P_EMULANDO;
}

static void emular(int desde)
{
    cres = circ;
    emula_cres(desde);
}

/* [I] y la opción 8 del menú: el barrido de CC del circuito del editor */
static void barre_editor(void)
{
    if (circ.nel == 0) pon_aviso("Nada que barrer: pon una E, una J o una R.", C_SUAVE);
    else barrido_de(P_EDITOR, &circ);
}

/* [=]: con una fuente alterna o una Z, la CA (fasores); si no, con [?!]
 * calcula y, si el circuito cambia con el tiempo, emula */
static void analizar_ca(int desde);
static void calcular_ca(void);
static void igual(void)
{
    if (circ_no_lineal(&circ)) {
        if (circ_alterna(&circ)) emular(P_EDITOR);          /* un rectificador en fasores no tiene sentido */
        else if (circ_busca_valor(&circ)) calcular();       /* SOL_NO_LINEAL: el mensaje lleva a [I]       */
        else if (circ_dinamico(&circ)) emular(P_EDITOR);
        else punto_op(P_EDITOR, 0);                         /* P_RESULTADO con res_nl                      */
        return;
    }
    /* lo de hoy: las controladas son lineales y las resuelven la tabla, la
     * CA, la calculadora de CA y el emulador */
    if (circ_alterna(&circ) && !circ_pide(&circ)) analizar_ca(P_EDITOR);
    else if (circ_alterna(&circ)) calcular_ca();
    else if (circ_pide(&circ)) calcular();
    else if (circ_dinamico(&circ)) emular(P_EDITOR);
    else calcular();
}

static int hay_canal_emu(const SimRes *s) { return s->canal[0].tipo != CANAL_NADA || s->canal[1].tipo != CANAL_NADA; }

/* Al acabar (o al cortar): al osciloscopio si hay algo que ver */
static void fin_emulacion(void)
{
    const SimRes *s = sim_res();
    int k, cambia = 0, cur = reemula_cur;
    double tc0 = osc.tc[0], tc1 = osc.tc[1], tizq = osc.tizq;
    unsigned char ccur = osc.cur, panel = osc.panel;
    signed char itdiv = osc.itdiv;
    reemula_cur = 0;
    if (s->fin == FIN_ERROR || sim_reg()->n < 2 || !hay_canal_emu(s)) { pant = P_EMU_ESQ; return; }
    for (k = 0; k < 2; k++) if (s->canal[k].tipo != osc_tipo_ant[k]) cambia = 1;
    osc_nuevo(&osc, sim_reg(), s, !osc.ajustado || cambia);
    if (cur && !cambia) {
        /* el [=] de «T1-T2: pocas muestras [=]»: los cursores y la ventana
         * siguen donde estaban (la autoescala los habría quitado) */
        osc.cursores = 1; osc.cur = ccur; osc.panel = panel;
        osc.tc[0] = tc0 < s->t_fin ? tc0 : s->t_fin;
        osc.tc[1] = tc1 < s->t_fin ? tc1 : s->t_fin;
        osc.itdiv = itdiv;
        osc.tizq = tizq <= s->t_fin ? tizq : 0.0;
        osc.c_gen = 0;
    }
    for (k = 0; k < 2; k++) osc_tipo_ant[k] = s->canal[k].tipo;
    if (s->fin == FIN_CORTADO || s->fin == FIN_TOPE) {
        char t[24];
        Buf b;
        buf_init(&b, osc.aviso, sizeof osc.aviso);
        buf_str(&b, s->fin == FIN_TOPE ? "Tope de pasos en t = " : "Cortado en t = ");
        buf_str(&b, num_pref(t, sizeof t, sim_reg()->t[sim_reg()->n - 1], 3, " s"));
        osc.aviso_col = C_CURSOR;
    }
    pant = P_OSC;
}

static void dib_barriendo(void)
{
    const BodeRes *r = bode_res();
    char t[64], u[24];
    Buf b;
    double f = bode_progreso();
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    barra(bode_c.nombre[0] ? bode_c.nombre : "Bode", 0);
    gfx_fill(40, 92, 240, 56, C_PANEL);
    gfx_rect(40, 92, 240, 56, C_CURSOR);
    buf_init(&b, t, sizeof t);
    if (r->auto_f && bode_reg()->n == 0) buf_str(&b, "Buscando el rango (1 mHz a 1 GHz)...");
    else {
        buf_str(&b, "Barriendo de "); buf_str(&b, num_pref(u, sizeof u, r->f_ini, 3, " Hz"));
        buf_str(&b, " a "); buf_str(&b, num_pref(u, sizeof u, r->f_fin, 3, " Hz")); buf_str(&b, "...");
    }
    gfx_text(50, 97, t, C_TEXTO, 1);
    gfx_rect(60, 115, 200, 8, C_SUAVE);
    gfx_fill(60, 115, (int)(200.0 * f), 8, C_OK);
    gfx_text(50, 131, "[esc] cancela", C_SUAVE, 1);
}

static void dib_calculando(void)
{
    const CalRes *s = cal_res();
    char t[64];
    Buf b;
    double f = cal_progreso();
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    barra(ca_pose.nombre[0] ? ca_pose.nombre : "Calculadora de CA", 0);
    gfx_fill(40, 92, 240, 56, C_PANEL);
    gfx_rect(40, 92, 240, 56, C_CURSOR);
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Calculando en CA");
    if (s->ninc > 0) {
        buf_str(&b, " ("); buf_int(&b, s->ninc);
        buf_str(&b, s->ninc == 1 ? " inc" S_oo "gnita)..." : " inc" S_oo "gnitas)...");
    } else buf_str(&b, "...");
    gfx_text(50, 97, t, C_TEXTO, 1);
    gfx_rect(60, 115, 200, 8, C_SUAVE);
    gfx_fill(60, 115, (int)(200.0 * f), 8, C_OK);
    buf_init(&b, t, sizeof t);
    if (s->nsol == 1) buf_str(&b, "1 soluci" S_oo "n  " S_CDOT "  ");
    else if (s->nsol > 1) { buf_int(&b, s->nsol); buf_str(&b, " soluciones  " S_CDOT "  "); }
    buf_str(&b, "[esc] corta");
    gfx_text(50, 131, t, C_SUAVE, 1);
}

/* El barrido de CC en marcha: «Barriendo R1 de 10 Ω a 10 kΩ...» y el x del
 * último punto */
static void x_con_unidad(Buf *b, double x)
{
    char u[24], e[8];
    Buf be;
    const BarrRes *r = barr_res();
    buf_init(&be, e, sizeof e);
    if (r->uni_x[0]) { buf_ch(&be, ' '); buf_str(&be, r->uni_x); }
    buf_str(b, cv_fija(u, sizeof u, x, 4, e));
}

static void dib_barriendo_cc(void)
{
    const BarrRes *r = barr_res();
    char t[64];
    Buf b;
    double f = barr_progreso();
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    barra(barr_c0.nombre[0] ? barr_c0.nombre : "Barrido de CC", 0);
    gfx_fill(40, 92, 240, 56, C_PANEL);
    gfx_rect(40, 92, 240, 56, C_CURSOR);
    buf_init(&b, t, sizeof t);
    if (r->modo == BARR_CURVA && barr_curva_k >= 0) {
        buf_str(&b, "Curva I-V de "); buf_str(&b, barr_c0.el[barr_curva_k].etiq); buf_str(&b, "...");
    } else if (r->modo == BARR_FAMILIA && barr_curva_k >= 0) {
        buf_str(&b, "Las 6 curvas de "); buf_str(&b, barr_c0.el[barr_curva_k].etiq); buf_str(&b, "...");
    } else {
        buf_str(&b, "Barriendo "); buf_str(&b, r->nom_x); buf_str(&b, " de ");
        x_con_unidad(&b, r->x0); buf_str(&b, " a "); x_con_unidad(&b, r->x1); buf_str(&b, "...");
    }
    gfx_text(50, 97, t, C_TEXTO, 1);
    gfx_rect(60, 115, 200, 8, C_SUAVE);
    gfx_fill(60, 115, (int)(200.0 * f), 8, C_OK);
    buf_init(&b, t, sizeof t);
    buf_str(&b, r->nom_x); buf_str(&b, " = "); x_con_unidad(&b, barr_x_hecho());
    buf_str(&b, "  " S_CDOT "  [esc] corta");
    gfx_text(50, 131, t, C_SUAVE, 1);
}

static void dib_emulando(void)
{
    const SimRes *s = sim_res();
    char t[64], u[24];
    Buf b;
    double f = sim_progreso();
    if (trabajo == TR_BODE) { dib_barriendo(); return; }
    if (trabajo == TR_CALCA) { dib_calculando(); return; }
    if (trabajo == TR_BARRIDO) { dib_barriendo_cc(); return; }
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    barra(cres.nombre[0] ? cres.nombre : "Emulando", 0);
    gfx_fill(40, 92, 240, 56, C_PANEL);
    gfx_rect(40, 92, 240, 56, C_CURSOR);
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Emulando de 0 a ");
    buf_str(&b, num_pref(u, sizeof u, s->t_fin, 3, " s"));
    buf_str(&b, "...");
    gfx_text(50, 97, t, C_TEXTO, 1);
    gfx_rect(60, 115, 200, 8, C_SUAVE);
    gfx_fill(60, 115, (int)(200.0 * f), 8, C_OK);
    buf_init(&b, t, sizeof t);
    buf_str(&b, "t = ");
    buf_str(&b, num_pref(u, sizeof u, f * s->t_fin, 3, " s"));
    buf_str(&b, "  " S_CDOT "  [esc] cancela");
    gfx_text(50, 131, t, C_SUAVE, 1);
}

static void fin_calca(void);
static void tecla_emulando(int t)
{
    if (t != T_ESC) return;
    if (trabajo == TR_BARRIDO) {
        double x = barr_x_hecho();
        barr_cortar();
        if (barr_hechos() >= 2) {
            Buf b;
            fin_barrido();
            buf_init(&b, curv.aviso, sizeof curv.aviso);
            buf_str(&b, "Cortado en "); buf_str(&b, barr_res()->nom_x); buf_str(&b, " = ");
            x_con_unidad(&b, x); buf_ch(&b, '.');
            curv.aviso_col = C_CURSOR;
        } else { pant = barr_origen; pon_aviso("Barrido cancelado.", C_CURSOR); }
        return;
    }
    if (trabajo == TR_CALCA) {
        cal_cortar();
        if (cal_res()->nsol > 0) fin_calca();
        else { pant = ca_origen; pon_aviso("C" S_aa "lculo cortado: a" S_uu "n no hab" S_ii "a soluci" S_oo "n.", C_CURSOR); }
        return;
    }
    if (trabajo == TR_BODE) {
        bode_cortar();
        if (bode_reg()->n >= 2) {
            char m[64], u[24];
            Buf b;
            fin_bode();
            buf_init(&b, m, sizeof m);
            buf_str(&b, "Cortado en f = ");
            buf_str(&b, num_pref(u, sizeof u, bode_reg()->f[bode_reg()->n - 1], 3, " Hz"));
            buf_init(&b, traz.aviso, sizeof traz.aviso);
            buf_str(&b, m);
            traz.aviso_col = C_CURSOR;
        } else { pant = bode_origen; pon_aviso("Barrido cancelado.", C_CURSOR); }
        return;
    }
    sim_cortar();
    if (sim_reg()->n > 1 && hay_canal_emu(sim_res())) fin_emulacion();
    else { pant = emu_origen; pon_aviso("Emulación cancelada.", C_CURSOR); }
}

/* ---- Ajustes: tiempo total y paso máximo ---- */
static void abre_ajustes(int desde)
{
    const Circuito *c = desde == P_EDITOR ? &circ : &cres;
    aj_origen = desde;
    aj_ctx = (desde == P_CA || (desde == P_EDITOR && circ_alterna(&circ))) ? AJ_CA : AJ_TIEMPO;
    aj_mag[0] = aj_ctx == AJ_CA ? c->frec : c->t_fin;
    aj_mag[1] = c->paso;
    if (desde == P_BARRIDO) {                       /* el barrido de CC: qué, dónde y cómo */
        const BarrRes *r = barr_res();
        aj_ctx = AJ_BARRIDO;
        aj_bel = r->el;
        aj_blog = r->log_x;
        aj_bvuelta = r->vuelta;
        ab_aviso = 0;
        aj_mag[0].v = barr_aj.x0; aj_mag[1].v = barr_aj.x1;
        aj_mag[0].est = aj_mag[1].est = (unsigned char)(barr_aj.el == aj_bel && !(barr_aj.x0 == 0.0 && barr_aj.x1 == 0.0) ? M_DATO : M_LIBRE);
    }
    if (desde == P_BODE) {                          /* el rango del barrido (de la sesión) */
        aj_ctx = AJ_BODE;
        aj_mag[0].v = bode_aj.f_ini; aj_mag[0].est = (unsigned char)(bode_aj.f_ini > 0.0 ? M_DATO : M_LIBRE);
        aj_mag[1].v = bode_aj.f_fin; aj_mag[1].est = (unsigned char)(bode_aj.f_fin > 0.0 ? M_DATO : M_LIBRE);
    }
    aj_conv = c->conv;
    aj_campo = 0;
    aj_aviso = 0;
    editando = 0;
    edit[0] = 0;
    pant = P_AJUSTES;
}

/* 1 si lo escrito vale (o no hay nada escrito) */
static int aj_confirma(void)
{
    static const double LO[2] = { 1e-9, 1e-12 }, HI[2] = { 1e6, 1.0 };
    double v, lo = aj_ctx != AJ_TIEMPO ? 1e-3 : LO[aj_campo], hi = aj_ctx != AJ_TIEMPO ? 1e9 : HI[aj_campo];
    int ok;
    if (!editando) return 1;
    editando = 0;
    if (!edit[0]) { aj_mag[aj_campo].est = M_LIBRE; return 1; }
    v = num_lee_ing(edit, &ok);
    edit[0] = 0;
    if (!ok) { aj_aviso = AV_NUMERO; return 0; }
    if (!(v >= lo && v <= hi)) { aj_aviso = AV_FUERA; return 0; }
    aj_mag[aj_campo].v = v;
    aj_mag[aj_campo].est = M_DATO;
    return 1;
}

static void aj_aplica(void)
{
    if (aj_ctx == AJ_BODE) {
        bode_aj.f_ini = aj_mag[0].est == M_DATO ? aj_mag[0].v : 0.0;
        bode_aj.f_fin = aj_mag[1].est == M_DATO ? aj_mag[1].v : 0.0;
        if (!(bode_aj.f_ini > 0.0 && bode_aj.f_fin > bode_aj.f_ini)) bode_aj.f_ini = bode_aj.f_fin = 0.0;
        return;
    }
    if (aj_ctx == AJ_CA) {
        int cambia_f;
        if (aj_mag[0].est == M_DATO && !(aj_mag[0].v >= 1e-3 && aj_mag[0].v <= 1e9)) aj_mag[0].est = M_LIBRE;
        /* con la calculadora, cres lleva la solución: la f cuenta solo si se tocó
         * (si no, una f hallada se volvería dato en el problema) */
        cambia_f = !(aj_origen == P_CA && ca_calc) || aj_mag[0].est != cres.frec.est || aj_mag[0].v != cres.frec.v;
        if (aj_origen == P_EDITOR || ca_origen == P_EDITOR) {
            guarda_hist();
            if (cambia_f && (circ.frec.est != M_BUSCAR || aj_mag[0].est != M_LIBRE)) circ.frec = aj_mag[0];
            circ.conv = (unsigned char)aj_conv;
        }
        if (aj_origen == P_CA && ca_calc) {
            ca_pose.conv = (unsigned char)aj_conv;
            if (cambia_f) ca_pose.frec = aj_mag[0];
        }
        if (cambia_f && (cres.frec.est != M_BUSCAR || aj_mag[0].est != M_LIBRE)) cres.frec = aj_mag[0];
        cres.conv = (unsigned char)aj_conv;
        return;
    }
    if (aj_mag[0].est == M_DATO && !(aj_mag[0].v >= 1e-9 && aj_mag[0].v <= 1e6)) aj_mag[0].est = M_LIBRE;
    if (aj_mag[1].est == M_DATO && !(aj_mag[1].v >= 1e-12 && aj_mag[1].v <= 1.0)) aj_mag[1].est = M_LIBRE;
    if (aj_origen == P_EDITOR || emu_origen == P_EDITOR) {
        guarda_hist();
        circ.t_fin = aj_mag[0];
        circ.paso = aj_mag[1];
    }
    cres.t_fin = aj_mag[0];
    cres.paso = aj_mag[1];
}

/* Los ajustes de la alterna: la f del circuito y el convenio */
static void dib_ajustes_ca(void)
{
    static const char *const NOM[3] = { "Frecuencia", "Fasores", "Referencia" };
    char t[64], u[24];
    Buf b;
    int i, y = BARRA_H + 12;
    barra("Ajustes de CA: frecuencia y convenio", 0);
    for (i = 0; i < 3; i++, y += 26) {
        u16 col = i == aj_campo ? C_CURSOR : C_COTA;
        gfx_text(8, y, NOM[i], col, 1);
        if (i == aj_campo) gfx_rect(120, y - 3, 190, 17, C_CURSOR);
        buf_init(&b, t, sizeof t);
        if (i == 0) {
            if (aj_campo == 0 && editando) { buf_str(&b, edit); buf_ch(&b, '_'); }
            else if (aj_mag[0].est == M_DATO) buf_str(&b, num_pref(u, sizeof u, aj_mag[0].v, 4, " Hz"));
            else if (aj_mag[0].est == M_BUSCAR) buf_str(&b, "? (la busca la calculadora)");
            else buf_str(&b, "falta");
        } else if (i == 1) { buf_str(&b, S_LARR " "); buf_str(&b, (aj_conv & CONV_PICO) ? "de pico" : "eficaces"); buf_str(&b, " " S_RARR); }
        else { buf_str(&b, S_LARR " "); buf_str(&b, (aj_conv & CONV_SENO) ? "seno" : "coseno"); buf_str(&b, " " S_RARR); }
        gfx_text(126, y, t, i == 0 && aj_mag[0].est != M_DATO && !editando ? C_SUAVE : C_TEXTO, 1);
    }
    y += 4;
    y += envuelve(8, y, COLS, "Los fasores y las tablas salen en este convenio; lo guardado no cambia. "
                  "Los instrumentos marcan el eficaz y el generador senoidal va en pico y seno.", C_SUAVE, 5) * LIN_H;
    if (aj_aviso)
        envuelve(8, y + 4, COLS, aj_aviso == AV_NUMERO ? "Eso no es un n" S_uu "mero." : "Fuera de rango: entre 1 mHz y 1 GHz.", C_ERROR, 2);
    pie("[arr/ab] campo [izq/der] opci" S_oo "n [enter] listo [=]");
}

/* Los ajustes del Bode: el rango de f; vacío = automático */
static void dib_ajustes_bode(void)
{
    static const char *const NOM[2] = { "Desde", "Hasta" };
    const BodeRes *r = bode_res();
    char t[64], u[24];
    Buf b;
    int i, y = BARRA_H + 12;
    barra("Bode: rango de frecuencias", 0);
    for (i = 0; i < 2; i++, y += 30) {
        const Mag *m = &aj_mag[i];
        gfx_text(8, y, NOM[i], i == aj_campo ? C_CURSOR : C_COTA, 1);
        if (i == aj_campo) gfx_rect(120, y - 3, 190, 17, C_CURSOR);
        buf_init(&b, t, sizeof t);
        if (i == aj_campo && editando) { buf_str(&b, edit); buf_ch(&b, '_'); }
        else if (m->est == M_DATO) buf_str(&b, num_pref(u, sizeof u, m->v, 4, " Hz"));
        else {
            double ult = i == 0 ? r->f_ini : r->f_fin;
            buf_str(&b, "auto");
            if (ult > 0.0) { buf_str(&b, " (" S_uu "ltimo: "); buf_str(&b, num_pref(u, sizeof u, ult, 3, " Hz")); buf_ch(&b, ')'); }
        }
        gfx_text(126, y, t, m->est == M_DATO || editando ? C_TEXTO : C_SUAVE, 1);
    }
    y += 6;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Entrada: "); buf_str(&b, r->nom_ent); buf_str(&b, "  ([F] en el Bode la cambia)");
    gfx_text(8, y, t, C_SUAVE, 1); y += LIN_H + 2;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Salida: "); buf_str(&b, r->nom_sal); buf_str(&b, "  ([C] la cambia)");
    gfx_text(8, y, t, C_SUAVE, 1); y += LIN_H + 6;
    y += envuelve(8, y, COLS, "Vac" S_ii "o = autom" S_aa "tico: un barrido grueso de 1 mHz a 1 GHz "
                  "busca donde cambian la fase y la ganancia. El rango se olvida con otro circuito.", C_SUAVE, 4) * LIN_H;
    if (aj_aviso)
        envuelve(8, y + 4, COLS, aj_aviso == AV_NUMERO ? "Eso no es un n" S_uu "mero." : "Fuera de rango: entre 1 mHz y 1 GHz.", C_ERROR, 2);
    pie("[arr/ab] campo [del] auto [enter] listo [=] barrer");
}

/* ---- Los ajustes del barrido de CC (§4.9) ---- */
static int barrible_ui(int tipo)
{
    return tipo == EL_FUENTE || tipo == EL_CORR || tipo == EL_RES || tipo == EL_EDEP || tipo == EL_JDEP;
}

/* La firma de un circuito para los ajustes del barrido de la sesión: sus
 * elementos (tipo, sitio y etiqueta), no sus valores. Otro circuito (un
 * modelo, otro que se abre, o este con un elemento de más o de menos) da otra */
static unsigned firma_mas(unsigned h, unsigned x) { return (h ^ x) * 16777619u; }
static unsigned firma_circ(const Circuito *c)
{
    unsigned h = 2166136261u;
    int k;
    const char *q;
    h = firma_mas(h, (unsigned)c->nel);
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        h = firma_mas(h, e->tipo);
        h = firma_mas(h, (unsigned)e->xa | (unsigned)e->ya << 8 | (unsigned)e->xb << 16 | (unsigned)e->yb << 24);
        h = firma_mas(h, (unsigned)e->xc | (unsigned)e->yc << 8);
        for (q = e->etiq; *q; q++) h = firma_mas(h, (unsigned char)*q);
    }
    return h;
}

/* La unidad de lo que se barre (de su campo Q_VAL, con la fila de su variante) */
static const char *uni_barre(int k) { return unidad_q(&barr_c0.el[k], Q_VAL); }

/* El rango de los límites de lo que se barre */
static void rango_barre(int k, double *lo, double *hi)
{
    int t = barr_c0.el[k].tipo;
    if (t == EL_FUENTE) { *lo = -1000.0; *hi = 1000.0; }
    else if (t == EL_CORR) { *lo = -100.0; *hi = 100.0; }
    else if (t == EL_RES) { *lo = 1e-6; *hi = 1e9; }
    else { *lo = -1e9; *hi = 1e9; }
}

enum { AB_NADA = 0, AB_NUMERO, AB_FUERA, AB_ORDEN, AB_LOG };


static void dib_ajustes_barrido(void)
{
    static const char *const NOM[5] = { "Barre", "Desde", "Hasta", "Escala", "Sentido" };
    const Elemento *e = &barr_c0.el[aj_bel];
    char t[80], u[24];
    Buf b;
    int i, y = 28;
    double a0, a1;
    int lg;
    barr_rango_auto(&barr_c0, aj_bel, &a0, &a1, &lg);
    barra("Barrido de CC: qu" S_ee " se barre y d" S_oo "nde", 0);
    for (i = 0; i < 5; i++, y += 26) {
        u16 col = i == aj_campo ? C_CURSOR : C_COTA;
        gfx_text(8, y, NOM[i], col, 1);
        if (i == aj_campo) gfx_rect(120, y - 3, 190, 17, C_CURSOR);
        buf_init(&b, t, sizeof t);
        if (i == 0) {
            buf_str(&b, S_LARR " "); buf_str(&b, e->etiq);
            if (e->val.est == M_DATO) { buf_str(&b, " ("); buf_str(&b, num_pref(u, sizeof u, e->val.v, 3, uni_barre(aj_bel))); buf_ch(&b, ')'); }
            buf_str(&b, " " S_RARR);
        } else if (i <= 2) {
            const Mag *m = &aj_mag[i - 1];
            if (i - 1 == aj_campo - 1 && editando) { buf_str(&b, edit); buf_ch(&b, '_'); }
            else if (m->est == M_DATO) buf_str(&b, num_pref(u, sizeof u, m->v, 4, uni_barre(aj_bel)));
            else { buf_str(&b, "auto ("); buf_str(&b, num_pref(u, sizeof u, i == 1 ? a0 : a1, 3, uni_barre(aj_bel))); buf_ch(&b, ')'); }
        } else if (i == 3) { buf_str(&b, S_LARR " "); buf_str(&b, aj_blog ? "logar" S_ii "tmica" : "lineal"); buf_str(&b, " " S_RARR); }
        else { buf_str(&b, S_LARR " "); buf_str(&b, aj_bvuelta ? "ida y vuelta" : "ida"); buf_str(&b, " " S_RARR); }
        gfx_text(126, y, t, (i == 1 || i == 2) && aj_mag[i - 1].est != M_DATO && !(editando && aj_campo == i) ? C_SUAVE : C_TEXTO, 1);
    }
    /* los canales: las sondas o los automáticos */
    {
        const BarrRes *r = barr_res();
        int sondas = barr_c0.sonda[0].tipo != SONDA_NO || barr_c0.sonda[1].tipo != SONDA_NO;
        buf_init(&b, t, sizeof t);
        if (sondas) {
            buf_str(&b, "Canal 1 = "); buf_str(&b, r->canal[0].nombre);
            if (r->canal[1].tipo != CANAL_NADA) { buf_str(&b, ", canal 2 = "); buf_str(&b, r->canal[1].nombre); }
            buf_str(&b, ": las sondas.");
        } else {
            buf_str(&b, "Sin sondas: "); buf_str(&b, r->canal[0].nombre);
            if (r->canal[1].tipo != CANAL_NADA) { buf_str(&b, " y "); buf_str(&b, r->canal[1].nombre); }
            buf_ch(&b, '.');
        }
        envuelve(8, 160, COLS, t, C_SUAVE, 2);
    }
    if (ab_aviso) {
        double lo, hi;
        rango_barre(aj_bel, &lo, &hi);
        buf_init(&b, t, sizeof t);
        if (ab_aviso == AB_NUMERO) buf_str(&b, "Eso no es un n" S_uu "mero.");
        else if (ab_aviso == AB_ORDEN) buf_str(&b, "Desde tiene que ser menor que Hasta.");
        else if (ab_aviso == AB_LOG) buf_str(&b, "La escala logar" S_ii "tmica pide Desde > 0.");
        else {
            buf_str(&b, "Fuera de rango: entre "); buf_str(&b, num_pref(u, sizeof u, lo, 3, ""));
            buf_str(&b, " y "); buf_str(&b, num_pref(u, sizeof u, hi, 3, uni_barre(aj_bel))); buf_ch(&b, '.');
        }
        gfx_text(8, 188, t, C_ERROR, 1);
    }
    pie("[arr/ab] campo [izq/der] opci" S_oo "n [enter] listo [=]");
}

/* 1 si lo escrito en Desde o Hasta vale (o no hay nada escrito) */
static int ab_confirma(void)
{
    double v, lo, hi;
    int ok, q = aj_campo - 1;
    if (!editando) return 1;
    editando = 0;
    if (!edit[0]) { aj_mag[q].est = M_LIBRE; return 1; }
    v = num_lee_ing(edit, &ok);
    edit[0] = 0;
    if (!ok) { ab_aviso = AB_NUMERO; return 0; }
    rango_barre(aj_bel, &lo, &hi);
    if (!(v >= lo && v <= hi)) { ab_aviso = AB_FUERA; return 0; }
    aj_mag[q].v = v;
    aj_mag[q].est = M_DATO;
    return 1;
}

/* Aplica los ajustes al barrido de la sesión: 0 si no valen (con su aviso) */
static int ab_aplica(void)
{
    double a0, a1, x0, x1;
    int lg;
    barr_rango_auto(&barr_c0, aj_bel, &a0, &a1, &lg);
    x0 = aj_mag[0].est == M_DATO ? aj_mag[0].v : a0;
    x1 = aj_mag[1].est == M_DATO ? aj_mag[1].v : a1;
    if (!(x0 < x1)) { ab_aviso = AB_ORDEN; return 0; }
    if (aj_blog && !(x0 > 0.0)) { ab_aviso = AB_LOG; return 0; }
    barr_aj.el = (signed char)aj_bel;
    barr_aj.log_x = (unsigned char)aj_blog;
    barr_aj.vuelta = (unsigned char)aj_bvuelta;
    if (aj_mag[0].est != M_DATO && aj_mag[1].est != M_DATO && aj_blog == lg) barr_aj.x0 = barr_aj.x1 = 0.0;
    else { barr_aj.x0 = x0; barr_aj.x1 = x1; }
    barr_aj_firma = firma_circ(&barr_c0);
    return 1;
}

static void tecla_ajustes_barrido(int t)
{
    if (t == T_ARRIBA || t == T_ABAJO) {
        if (!ab_confirma()) return;
        aj_campo = (aj_campo + (t == T_ABAJO ? 1 : 4)) % 5;
        return;
    }
    if ((t == T_IZQ || t == T_DER) && (aj_campo == 0 || aj_campo >= 3)) {
        ab_aviso = 0;
        if (aj_campo == 0) {                        /* otro elemento: su rango, automático */
            int i, n = barr_c0.nel;
            for (i = 1; i <= n; i++) {
                int q = (aj_bel + (t == T_DER ? i : n - i)) % n;
                if (barrible_ui(barr_c0.el[q].tipo)) { aj_bel = q; break; }
            }
            aj_mag[0].est = aj_mag[1].est = M_LIBRE;
            { double a0, a1; barr_rango_auto(&barr_c0, aj_bel, &a0, &a1, &aj_blog); }
        } else if (aj_campo == 3) aj_blog = !aj_blog;
        else aj_bvuelta = !aj_bvuelta;
        return;
    }
    if (aj_campo == 1 || aj_campo == 2) {
        if (t >= T_DIG(0) && t <= T_DIG(9)) { ab_aviso = 0; mete_char((char)('0' + (t - T_DIG0))); return; }
        if (t == T_PUNTO) { if (!strchr(edit, ',') && !strchr(edit, 'E')) { ab_aviso = 0; mete_char(','); } return; }
        if (t == T_EE) { if (editando && edit[0] && !strchr(edit, 'E')) mete_char('E'); return; }
        if (tecla_prefijo(t)) { if (editando && admite_prefijo(edit)) mete_char(tecla_prefijo(t)); return; }
        if (t == T_SIGNO || t == T_MENOS) {
            size_t n = strlen(edit);
            if (n == 0 || edit[n - 1] == 'E') { ab_aviso = 0; mete_char('-'); }
            return;
        }
        if (t == T_BORRAR) {
            if (editando && edit[0]) edit[strlen(edit) - 1] = 0;
            else { editando = 0; aj_mag[aj_campo - 1].est = M_LIBRE; ab_aviso = 0; }
            return;
        }
    }
    if (t == T_ESC) { editando = 0; edit[0] = 0; ab_aviso = 0; pant = aj_origen; return; }
    if (t == T_ENTER || t == T_IGUAL) {
        if (!ab_confirma() || !ab_aplica()) return;
        editando = 0; edit[0] = 0;
        if (t == T_ENTER) { pant = P_BARRIDO; return; }
        barrido_de(barr_origen, &barr_c0);
    }
}

static void dib_ajustes(void)
{
    static const char *const NOM[2] = { "Tiempo total", "Paso m" S_aa "ximo" };
    if (aj_ctx == AJ_CA) { dib_ajustes_ca(); return; }
    if (aj_ctx == AJ_BODE) { dib_ajustes_bode(); return; }
    if (aj_ctx == AJ_BARRIDO) { dib_ajustes_barrido(); return; }
    const SimRes *s = sim_res();
    char t[64], u[24];
    Buf b;
    int i, y = BARRA_H + 12;
    barra("Emulaci" S_oo "n: tiempo y paso", 0);
    for (i = 0; i < 2; i++, y += 30) {
        const Mag *m = &aj_mag[i];
        u16 col = i == aj_campo ? C_CURSOR : C_COTA;
        gfx_text(8, y, NOM[i], col, 1);
        if (i == aj_campo) gfx_rect(120, y - 3, 190, 17, C_CURSOR);
        buf_init(&b, t, sizeof t);
        if (i == aj_campo && editando) { buf_str(&b, edit); buf_ch(&b, '_'); }
        else if (m->est == M_DATO) buf_str(&b, num_pref(u, sizeof u, m->v, 4, " s"));
        else {
            double ult = i == 0 ? s->t_fin : s->h;
            buf_str(&b, "auto");
            if (ult > 0.0) { buf_str(&b, " (" S_uu "ltimo: "); buf_str(&b, num_pref(u, sizeof u, ult, 3, " s")); buf_ch(&b, ')'); }
        }
        gfx_text(126, y, t, m->est == M_DATO || editando ? C_TEXTO : C_SUAVE, 1);
        if (i == aj_campo && editando && edit[0]) {
            int ok;
            double v = num_lee_ing(edit, &ok);
            if (ok) {
                buf_init(&b, t, sizeof t);
                buf_str(&b, "= ");
                buf_str(&b, num_pref(u, sizeof u, v, 4, " s"));
                gfx_text(126, y + 14, t, C_SUAVE, 1);
            }
        }
    }
    y += 6;
    y += envuelve(8, y, COLS, "Vac" S_ii "o = autom" S_aa "tico: el emulador elige la ventana por "
                  "las constantes de tiempo y los periodos de las fuentes, y el paso para que "
                  "la curva salga fina. El paso que pongas es un m" S_aa "ximo.", C_SUAVE, 5) * LIN_H;
    if (aj_aviso)
        envuelve(8, y + 4, COLS, aj_aviso == AV_NUMERO ? "Eso no es un n" S_uu "mero." :
                 aj_campo == 0 ? "Fuera de rango: entre 1 ns y 1000000 s." : "Fuera de rango: entre 1 ps y 1 s.", C_ERROR, 2);
    pie("[arr/ab] campo [del] auto [enter] listo [=] emular");
}

static void tecla_ajustes(int t)
{
    if (aj_ctx == AJ_BARRIDO) { tecla_ajustes_barrido(t); return; }
    if (aj_ctx == AJ_CA) {
        if (t == T_ARRIBA || t == T_ABAJO) {
            if (!aj_confirma()) return;
            aj_campo = (aj_campo + (t == T_ABAJO ? 1 : 2)) % 3;
            return;
        }
        if ((t == T_IZQ || t == T_DER) && aj_campo > 0) { aj_conv ^= aj_campo == 1 ? CONV_PICO : CONV_SENO; return; }
        if (aj_campo > 0 && t != T_ESC && t != T_ENTER && t != T_IGUAL) return;
        if (t == T_ENTER || t == T_IGUAL) {
            if (!aj_confirma()) return;
            aj_aplica();
            editando = 0; edit[0] = 0;
            if (t == T_ENTER || aj_origen != P_CA) {
                pant = aj_origen;
                if (t == T_IGUAL && aj_origen == P_EDITOR) igual();
                return;
            }
            if (ca_calc) { calcula_ca_de(); return; }
            reanaliza_ca();
            pant = P_CA;
            return;
        }
    }
    if (t == T_ARRIBA || t == T_ABAJO) {
        if (!aj_confirma()) return;
        aj_campo = t == T_ARRIBA ? 0 : 1;
        return;
    }
    if (t >= T_DIG(0) && t <= T_DIG(9)) { aj_aviso = 0; mete_char((char)('0' + (t - T_DIG0))); return; }
    if (t == T_PUNTO) { if (!strchr(edit, ',') && !strchr(edit, 'E')) { aj_aviso = 0; mete_char(','); } return; }
    if (t == T_EE) { if (editando && edit[0] && !strchr(edit, 'E')) mete_char('E'); return; }
    if (tecla_prefijo(t)) { if (editando && admite_prefijo(edit)) mete_char(tecla_prefijo(t)); return; }
    if (t == T_SIGNO || t == T_MENOS) { size_t n = strlen(edit); if (editando && n > 0 && edit[n - 1] == 'E') mete_char('-'); return; }
    if (t == T_BORRAR) {
        if (editando && edit[0]) edit[strlen(edit) - 1] = 0;
        else { editando = 0; aj_mag[aj_campo].est = M_LIBRE; aj_aviso = 0; }
        return;
    }
    if (t == T_ESC) { editando = 0; edit[0] = 0; pant = aj_origen; return; }
    if (t == T_ENTER || t == T_IGUAL) {
        if (!aj_confirma()) return;
        aj_aplica();
        if (aj_ctx == AJ_BODE) {
            editando = 0; edit[0] = 0;
            if (t == T_ENTER) pant = P_BODE;
            else bode_de(bode_origen, &bode_c);
            return;
        }
        if (t == T_ENTER) { pant = aj_origen; return; }
        if (aj_origen == P_EDITOR) emular(P_EDITOR);
        else emula_cres(emu_origen);
    }
}

/* ---- [=] en el osciloscopio: seguir hasta donde se mira, o más fino ----
 * Más fino si la ventana tiene menos de 150 muestras (600 en la ventana) o
 * si las medidas son de [T1, T2] y ahí hay menos de 150 (300 entre T1 y
 * T2: la marca «T1-T2: pocas muestras [=]»; si no caben en el tope de
 * pasos, lo más fino que cabe, con holgura, si con eso llega a 150). Con
 * los cursores puestos, siguen donde estaban (fin_emulacion). */
static void reemular(void)
{
    const SimRes *s = sim_res();
    double tdiv = ESCALA[osc.itdiv], der = osc.tizq + 10.0 * tdiv, h = 0.0;
    double tramo = osc_mide_cursores(&osc) ? fabs(osc.tc[1] - osc.tc[0]) : 0.0;
    int cur;
    Mag tf = cres.t_fin, paso = cres.paso;
    osc_prepara(&osc, sim_reg(), s);              /* n_vent y n_med de esta ventana, sin dibujar */
    cur = tramo > 0.0 && osc.n_med < 150;
    /* Lo nuevo se calcula aparte y solo se aplica si se puede emular: un
     * [=] que se niega no cambia nada */
    if (der > s->t_fin * (1.0 + 1e-9)) { tf.v = der < 1e6 ? der : 1e6; tf.est = M_DATO; }
    if (osc.n_vent < 150) h = 10.0 * tdiv / 600.0;
    if (cur && (h == 0.0 || tramo / 300.0 < h)) h = tramo / 300.0;
    if (h > 0.0 && h < s->h) {                     /* solo más fino, nunca más grueso */
        double t_total = tf.est == M_DATO ? tf.v : s->t_fin;
        if (t_total / h > (double)SIM_MAX_PASOS) {
            double hmin = t_total / (0.8 * (double)SIM_MAX_PASOS);
            if (cur && tramo / hmin >= 150.0 && hmin < s->h) h = hmin;
            else {
                Buf b;
                buf_init(&b, osc.aviso, sizeof osc.aviso);
                buf_str(&b, "Har" S_ii "an falta m" S_aa "s de 20000 pasos: ");
                buf_str(&b, cur ? "separa T1 y T2." : "m" S_aa "s t/div ([-]).");
                osc.aviso_col = C_ERROR;
                return;
            }
        }
        paso.v = h; paso.est = M_DATO;
    }
    cres.t_fin = tf;
    cres.paso = paso;
    if (emu_origen == P_EDITOR) { guarda_hist(); circ.t_fin = cres.t_fin; circ.paso = cres.paso; }
    emula_cres(emu_origen);
    reemula_cur = osc.cursores && pant == P_EMULANDO;
}

static void tecla_osc(int t)
{
    switch (osc_tecla(&osc, sim_reg(), sim_res(), t | mods)) {
    case OSC_SALIR:   pant = emu_origen; break;
    case OSC_TABLA:   pant = P_EMU_TABLA; emu_pag = 0; scroll = 0; break;
    case OSC_AJUSTES: abre_ajustes(P_OSC); break;
    case OSC_EMULAR:  reemular(); break;
    default: break;
    }
}

/* ---- El estado de un interruptor en t (por la derecha) ---- */
static int inter_cerrado(const Elemento *e, double t)
{
    int c = e->variante == INTER_CERRADO;
    if (e->par[Q_T1 - Q_P1].est == M_DATO && t >= e->par[Q_T1 - Q_P1].v) c = !c;
    if (e->par[Q_T2 - Q_P1].est == M_DATO && t >= e->par[Q_T2 - Q_P1].v) c = !c;
    return c;
}

/* El instante de "al final": t_fin, o el del corte */
static double t_final(void)
{
    const SimRes *s = sim_res();
    if ((s->fin == FIN_CORTADO || s->fin == FIN_TOPE) && sim_reg()->n > 0) return sim_reg()->t[sim_reg()->n - 1];
    return s->t_fin;
}

/* ---- Tabla de la emulación: 5 páginas ---- */
#define EMU_PAGS 5
static int emu_lineas;                /* para el desplazamiento */

static void cabeza_tabla(const char *c2, const char *c3, const char *c4, const char *c5)
{
    int y = BARRA_H + 4;
    gfx_text(6, y, "Elem", C_SUAVE, 1);
    gfx_text(40, y, c2, C_SUAVE, 1);
    gfx_text(114, y, c3, C_SUAVE, 1);
    gfx_text(184, y, c4, C_SUAVE, 1);
    gfx_text(254, y, c5, C_SUAVE, 1);
    separador(BARRA_H + 17);
}

/* La columna del valor: R, C, L; e(t) de una fuente; el estado de un S; lo
 * que marca un instrumento */
static void celda_valor(int y, int k, double tf)
{
    const SimRes *s = sim_res();
    const Elemento *e = &cres.el[k];
    if (es_nl_ui(e->tipo)) {                          /* la región al final */
        int r = s->ultimo.reg[k];
        gfx_text(40, y, reg_texto(r), reg_color(r), 1);
    }
    else if (e->tipo == EL_INTER) gfx_text(40, y, inter_cerrado(e, tf) ? "cerrado" : "abierto", C_SUAVE, 1);
    else if (e->tipo == EL_VOLT || e->tipo == EL_AMP) {
        char t[24], u[20];
        Buf b;
        buf_init(&b, t, sizeof t);
        if (e->variante == MED_CA) buf_ch(&b, '~');
        buf_str(&b, num_pref(u, sizeof u, s->lectura[k], 4, e->tipo == EL_VOLT ? "V" : "A"));
        gfx_text(40, y, t, C_INSTR, 1);
    } else if (e->tipo == EL_FUENTE || e->tipo == EL_CORR) celda_s(40, y, s->ultimo.val[k], unidad_val(e), C_TEXTO);
    else {
        char u[20];
        gfx_text(40, y, num_pref(u, sizeof u, e->val.v, 4, unidad_val(e)), C_TEXTO, 1);
    }
}

static void linea_modo(Buf *b, const SimModo *m)
{
    char u[24];
    double al = -m->re, mag = sqrt(m->re * m->re + m->im * m->im);
    buf_str(b, "s = ");
    buf_str(b, num_pref(u, sizeof u, fabs(m->re) < 1e-9 * mag ? 0.0 : m->re, 4, ""));
    if (m->im > 0.0) { buf_str(b, " " S_PM " j"); buf_str(b, num_pref(u, sizeof u, m->im, 4, "")); }
    buf_str(b, " 1/s  ");
    if (m->im > 0.0) {
        buf_str(b, "fd = "); buf_str(b, num_pref(u, sizeof u, m->im / (2.0 * 3.14159265358979323846), 4, "Hz"));
        buf_str(b, "  " S_ZETA " = "); buf_str(b, num_auto(u, sizeof u, mag > 0.0 ? al / mag : 0.0, 3));   /* 0 <= zeta < 1 */
    } else if (al > 0.0) {
        buf_str(b, S_TAU " = "); buf_str(b, num_pref(u, sizeof u, 1.0 / al, 4, " s"));
    }
}

static void dib_emu_tabla(void)
{
    const SimRes *s = sim_res();
    char t[96], u[24];
    Buf b;
    int k, y = BARRA_H + 20, fila = 0, vis = (PIE_Y - y) / 14;
    const char *titulo;
    char pg[8];

    buf_init(&b, pg, sizeof pg);
    buf_int(&b, emu_pag + 1); buf_ch(&b, '/'); buf_int(&b, EMU_PAGS);
    buf_init(&b, t, sizeof t);
    switch (emu_pag) {
    case 0:  buf_str(&b, "Al final (t = "); buf_str(&b, num_pref(u, sizeof u, t_final(), 3, " s")); buf_ch(&b, ')'); break;
    case 1:  buf_str(&b, "M" S_aa "ximos (0 a "); buf_str(&b, num_pref(u, sizeof u, t_final(), 3, " s")); buf_ch(&b, ')'); break;
    case 2:  buf_str(&b, s->t_med0 < 0.9 * s->t_fin * (1.0 - 1e-9) ? "Eficaces y P media (" S_uu "ltimos periodos)"
                                                               : "Eficaces y P media (" S_uu "ltimo 10 %)"); break;
    case 3:  buf_str(&b, "Energ" S_ii "a"); break;
    default: buf_str(&b, "Respuesta natural"); break;
    }
    titulo = t;
    barra(titulo, pg);

    if (emu_pag <= 2) {
        if (emu_pag == 0) cabeza_tabla("Valor", "V", "I", "P");
        else if (emu_pag == 1) cabeza_tabla("Valor", "V m" S_aa "x", "I m" S_aa "x", "P m" S_aa "x");
        else cabeza_tabla("Valor", "V ef", "I ef", "P media");
        int tres[MAX_EL], n3 = 0, ntot;
        for (k = 0; k < cres.nel; k++) if (circ_npatas(&cres.el[k]) == 3) tres[n3++] = k;
        ntot = cres.nel + (emu_pag == 0 ? n3 : 0);
        emu_lineas = ntot;
        if (scroll > ntot - vis) scroll = ntot - vis;
        if (scroll < 0) scroll = 0;
        for (k = scroll; k < ntot && k < scroll + vis; k++, y += 14) {
            const Elemento *e;
            double V, I, P;
            if (k >= cres.nel) {                      /* el segundo puerto de un 3 patas, al final */
                int q = tres[k - cres.nel];
                e = &cres.el[q];
                buf_init(&b, t, sizeof t);
                buf_str(&b, e->etiq);
                buf_str(&b, e->tipo == EL_AO ? "  Vd " : e->tipo == EL_MOS ? "  VGS " : "  VBE ");
                buf_str(&b, num_pref(u, sizeof u, s->ultimo.V2[q], 4, "V"));
                buf_str(&b, e->tipo == EL_AO ? "  Ient " : e->tipo == EL_MOS ? "  IG " : "  IB ");
                buf_str(&b, num_pref(u, sizeof u, s->ultimo.I2[q], 4, "A"));
                gfx_text(6, y, t, C_TEXTO, 1);
                continue;
            }
            e = &cres.el[k];
            gfx_text(6, y, e->etiq, tipo_info(e->tipo)->color, 1);
            celda_valor(y, k, t_final());
            if (emu_pag == 0) { V = s->ultimo.V[k]; I = s->ultimo.I[k]; P = s->ultimo.P[k]; }
            else if (emu_pag == 1) { V = s->Vmax[k]; I = s->Imax[k]; P = s->Pmax[k]; }
            else { V = s->Vef[k]; I = s->Ief[k]; P = s->Pmed[k]; }
            celda_s(114, y, V, "V", C_TENSION);
            celda_s(184, y, I, "A", C_CORRIENTE);
            celda_s(254, y, P, "W", C_POTENCIA);
        }
        pie("[izq/der] p" S_aa "gina  [tab] esquema  [esc] volver");
        return;
    }
    if (emu_pag == 3) {
        double ent = 0.0, dis = 0.0, alm = 0.0, ref;
        cabeza_tabla("Valor", "W final", "W m" S_aa "x", "Q o " S_PHI);
        for (k = 0; k < cres.nel; k++) {                /* el criterio de termina: controladas y AO entregan */
            const Elemento *e = &cres.el[k];
            if (e->tipo == EL_FUENTE || e->tipo == EL_CORR || e->tipo == EL_EDEP || e->tipo == EL_JDEP || e->tipo == EL_AO)
                ent += s->W[k];
            else if (e->tipo == EL_COND || e->tipo == EL_BOB) alm += s->Efin[k] - s->E0[k];
            else if (e->tipo != EL_INTER) dis += s->W[k];
        }
        emu_lineas = 0;
        for (k = 0; k < cres.nel; k++) {
            const Elemento *e = &cres.el[k];
            if (e->tipo != EL_COND && e->tipo != EL_BOB) continue;
            if (fila++ < scroll) continue;
            if (y > PIE_Y - 14 * 5) continue;          /* no cabe: se cuenta, no se dibuja */
            gfx_text(6, y, e->etiq, tipo_info(e->tipo)->color, 1);
            gfx_text(40, y, num_pref(u, sizeof u, e->val.v, 4, unidad_val(e)), C_TEXTO, 1);
            celda(114, y, s->Efin[k], "J", C_POTENCIA);
            celda(184, y, s->Emax[k], "J", C_POTENCIA);
            if (e->tipo == EL_COND) celda_s(254, y, e->val.v * s->ultimo.V[k], "C", C_TEXTO);
            else celda_s(254, y, e->val.v * s->ultimo.I[k], "Wb", C_TEXTO);
            y += 14;
        }
        emu_lineas = fila;
        if (fila == 0) { gfx_text(6, y, "No hay C ni L: nada se almacena.", C_SUAVE, 1); y += 14; }
        separador(y + 1);
        y += 5;
        ref = fabs(ent) > 0.0 ? fabs(ent) : fabs(dis) + fabs(alm);
        {
            static const char *const NOM[4] = { "Entregada por las fuentes", "Disipada (R e instrumentos)",
                                                 "Almacenada (final - inicial)", "Diferencia: saltos y m" S_ee "todo" };
            double val[4];
            int i;
            val[0] = ent; val[1] = dis; val[2] = alm; val[3] = s->dif_balance;
            if (fabs(val[3]) < 1e-9 * ref) val[3] = 0.0;       /* redondeo: es un cero */
            for (i = 0; i < 4; i++, y += 13) {
                gfx_text(6, y, NOM[i], i == 3 ? C_SUAVE : C_COTA, 1);
                buf_init(&b, t, sizeof t);
                buf_str(&b, num_pref(u, sizeof u, val[i], 4, "J"));
                if (ref > 0.0 && i > 0) { buf_str(&b, "  "); buf_str(&b, num_auto(u, sizeof u, 100.0 * val[i] / ref, 2)); buf_str(&b, " %"); }
                gfx_text(SCR_W - 6 - gfx_text_w(t, 1), y, t, C_TEXTO, 1);
            }
        }
        pie("[izq/der] p" S_aa "gina  [tab] esquema  [esc] volver");
        return;
    }
    /* Respuesta natural, por topología */
    {
        int i, j, nest = 0, lin = 0;
        for (k = 0; k < cres.nel; k++) if (cres.el[k].tipo == EL_COND || cres.el[k].tipo == EL_BOB) nest++;
        if (s->ntop == 0 || nest == 0) {
            envuelve(6, y, COLS, "Sin C ni L no hay respuesta natural: el circuito sigue a sus fuentes al instante.", C_SUAVE, 3);
            emu_lineas = 0;
        }
        for (i = 0; i < s->ntop; i++) {
            /* cabecera: los S de esa topología y desde cuándo */
            buf_init(&b, t, sizeof t);
            if (s->ntop > 1 || s->mascara[i]) {
                int n = 0, hay_s = 0;
                buf_str(&b, "con ");
                for (k = 0; k < cres.nel; k++) {
                    if (cres.el[k].tipo != EL_INTER) continue;
                    hay_s = 1;
                    if (b.n + strlen(cres.el[k].etiq) + 10 + 18 > COLS) { buf_str(&b, "..."); break; }
                    if (n++) buf_str(&b, ", ");
                    buf_str(&b, cres.el[k].etiq);
                    buf_str(&b, (s->mascara[i] >> k) & 1u ? " cerrado" : " abierto");
                }
                for (k = 0; k < cres.nel && k < 32; k++) {       /* los AO: su tramo en esa topología (§3.K) */
                    unsigned tr = (unsigned)(s->tramos_top[i] >> (2 * k)) & 3u;
                    if (cres.el[k].tipo != EL_AO) continue;
                    if (b.n + strlen(cres.el[k].etiq) + 9 + (hay_s ? 18 : 0) > COLS) { buf_str(&b, "..."); break; }
                    if (n++) buf_str(&b, ", ");
                    buf_str(&b, cres.el[k].etiq);
                    buf_str(&b, tr == TR_SATP ? " en V+" : tr == TR_SATN ? " en V-" : " lineal");
                }
                if (hay_s) {
                    buf_str(&b, ", desde ");
                    buf_str(&b, num_pref(u, sizeof u, s->t_top[i], 3, " s"));
                }
            } else buf_str(&b, "Modos propios (s = -" S_ALFA " " S_PM " j" S_OMEGA "d)");
            if (lin++ >= scroll && y < PIE_Y - 13) { gfx_text(6, y, t, C_TITULO, 1); y += 13; }
            if (s->nmodos[i] == 0) {
                if (lin++ >= scroll && y < PIE_Y - 13) { gfx_text(12, y, "sin modos (no hay estado libre)", C_SUAVE, 1); y += 13; }
                continue;
            }
            for (j = 0; j < s->nmodos[i]; j++) {
                buf_init(&b, t, sizeof t);
                linea_modo(&b, &s->modo[i][j]);
                if (lin++ >= scroll && y < PIE_Y - 13) { gfx_text(12, y, t, C_TEXTO, 1); y += 13; }
            }
            if (nest == 2) {
                const char *cl;
                double z;
                if (s->nmodos[i] == 1 && s->modo[i][0].im > 0.0) {
                    const SimModo *m = &s->modo[i][0];
                    z = -m->re / sqrt(m->re * m->re + m->im * m->im);
                } else if (s->nmodos[i] == 2) {
                    double a = s->modo[i][0].re, c = s->modo[i][1].re;
                    z = a * c > 0.0 ? -(a + c) / (2.0 * sqrt(a * c)) : 1e9;
                } else z = -1.0;
                cl = z < 0.0 ? 0 : fabs(z - 1.0) < 1e-6 ? "Cr" S_ii "tico" : z < 1.0 ? "Subamortiguado" : "Sobreamortiguado";
                if (cl && lin++ >= scroll && y < PIE_Y - 13) { gfx_text(12, y, cl, C_OK, 1); y += 13; }
            }
        }
        emu_lineas = lin;
    }
    pie("[izq/der] p" S_aa "gina  [tab] esquema  [esc] volver");
}

/* Las líneas de la página que se ve, sin dibujarla (las que cuenta
 * dib_emu_tabla): la cota de [ab] es de la tecla, no del último dibujo */
#define EMU_VIS ((PIE_Y - BARRA_H - 20) / 14)
static int emu_lineas_de(void)
{
    const SimRes *s = sim_res();
    int k, i, n = 0, nest = 0;
    if (emu_pag <= 2) {
        if (emu_pag == 0) for (k = 0; k < cres.nel; k++) if (circ_npatas(&cres.el[k]) == 3) n++;
        return cres.nel + n;
    }
    for (k = 0; k < cres.nel; k++) if (cres.el[k].tipo == EL_COND || cres.el[k].tipo == EL_BOB) nest++;
    if (emu_pag == 3) return nest;
    for (i = 0; i < s->ntop; i++) {                     /* la respuesta natural */
        n++;                                            /* la cabecera          */
        if (s->nmodos[i] == 0) { n++; continue; }
        n += s->nmodos[i];
        if (nest == 2) {
            double z;
            if (s->nmodos[i] == 1 && s->modo[i][0].im > 0.0) {
                const SimModo *m = &s->modo[i][0];
                z = -m->re / sqrt(m->re * m->re + m->im * m->im);
            } else if (s->nmodos[i] == 2) {
                double a = s->modo[i][0].re, c = s->modo[i][1].re;
                z = a * c > 0.0 ? -(a + c) / (2.0 * sqrt(a * c)) : 1e9;
            } else z = -1.0;
            if (!(z < 0.0)) n++;                        /* crítico, sub- o sobreamortiguado */
        }
    }
    return n;
}

/* [ab] en la tabla de la emulación: una página; en las 3 primeras, sin
 * pasar de la última que se llena (lo que acotaba el dibujo) */
static int emu_baja(int sc)
{
    int p = emu_pag == 3 ? 9 : 13;
    if (sc + p >= emu_lineas) return sc;
    sc += p;
    if (emu_pag <= 2 && sc > emu_lineas - EMU_VIS) sc = emu_lineas - EMU_VIS;
    return sc < 0 ? 0 : sc;
}

static void tecla_emu_tabla(int t)
{
    emu_lineas = emu_lineas_de();
    if (t == T_DER) { emu_pag = (emu_pag + 1) % EMU_PAGS; scroll = 0; }
    else if (t == T_IZQ) { emu_pag = (emu_pag + EMU_PAGS - 1) % EMU_PAGS; scroll = 0; }
    else if (t == T_ABAJO && (mods & T_CON_CTRL)) {    /* [ctrl]: hasta el final */
        int i, s;
        for (i = 0; i < 100 && (s = emu_baja(scroll)) != scroll; i++) scroll = s;
    }
    else if (t == T_ABAJO) scroll = emu_baja(scroll);
    else if (t == T_ARRIBA && (mods & T_CON_CTRL)) scroll = 0;
    else if (t == T_ARRIBA) { scroll -= emu_pag == 3 ? 9 : 13; if (scroll < 0) scroll = 0; }
    else if (t == T_TAB) pant = P_EMU_ESQ;
    else if (t == T_ESC) pant = hay_canal_emu(sim_res()) && sim_reg()->n > 1 ? P_OSC : emu_origen;
}

/* ---- El esquema emulado: los valores en t_fin, las sondas y las lecturas ---- */

/* Un valor al final que es ruido de redondeo frente a su escala (lo más que
 * llegó a valer en la emulación) se rotula 0: e(t_fin) = 12·sen(12π) da
 * -1,8E-14 V, y «-0,000015nV» parecería un valor. Lo que no es ruido (la fuga
 * de un diodo en inversa, pA frente a mA) se queda como está. */
static double sin_ruido(double x, double esc)
{
    return fabs(x) <= 1e-12 * fabs(esc) ? 0.0 : x;
}

static void dib_emu_esq(void)
{
    static Circuito c;
    const SimRes *s = sim_res();
    const SimDiag *d = &s->d;
    Marco m;
    Vista v;
    char t[220], u[24];
    Buf b;
    int k, y, error = s->fin == FIN_ERROR || d->estado != SIM_OK, n;

    /* el estado de los S al final */
    c = cres;
    for (k = 0; k < c.nel; k++)
        if (c.el[k].tipo == EL_INTER && !error) {
            c.el[k].variante = (unsigned char)(inter_cerrado(&cres.el[k], t_final()) ? INTER_CERRADO : INTER_ABIERTO);
            c.el[k].par[Q_T1 - Q_P1].est = M_LIBRE;
        }
    barra(error ? "No se puede emular" : "Emulado: al final", esq_capa_nombre(emu_capa));
    esq_marco_ajustado(&c, 0, BARRA_H, SCR_W, 158 - BARRA_H, &m);
    esq_vista(&v);
    v.capa = (unsigned char)emu_capa;
    v.sondas = 1;
    if (error) { v.resalta = d->lazo; v.atenuar = 1; if (d->elem >= 0) v.resalta |= 1u << d->elem; }
    else {
        static Valores vm;
        double vesc = 0.0;
        vm = s->ultimo;
        for (k = 0; k < cres.nel; k++) {
            vm.V[k] = sin_ruido(vm.V[k], s->Vmax[k]);
            vm.I[k] = sin_ruido(vm.I[k], s->Imax[k]);
            vm.P[k] = sin_ruido(vm.P[k], s->Pmax[k]);
            if (cres.el[k].tipo == EL_FUENTE) vm.val[k] = sin_ruido(vm.val[k], s->Vmax[k]);     /* e(t) */
            if (cres.el[k].tipo == EL_CORR) vm.val[k] = sin_ruido(vm.val[k], s->Imax[k]);       /* j(t) */
            if (fabs(s->Vmax[k]) > vesc) vesc = fabs(s->Vmax[k]);
        }
        for (n = 0; n < red.nn; n++) vm.phi[n] = sin_ruido(vm.phi[n], vesc);   /* frente a la mayor V */
        v.val = &vm; v.red = &red; v.lectura = s->lectura;
        v.brillo = brillo_de(&cres, s->Imed);
        if (v.brillo)                                 /* el LED se rotula con la media, como brilla */
            for (k = 0; k < cres.nel; k++) if (cres.el[k].tipo == EL_LED) vm.I[k] = s->Imed[k];
    }
    gfx_clip(0, BARRA_H, SCR_W, 158 - BARRA_H);
    esq_dibuja(&c, &m, &v);
    gfx_clip_reset();

    gfx_fill(0, 159, SCR_W, PIE_Y - 159, C_PANEL);
    y = 161;
    if (error) {
        /* sin la SimRed: si sim_iniciar falló al armarla, sim_red() es la de
         * la emulación anterior (los índices son los del circuito) */
        sim_mensaje_circ(&cres, 0, d, t, sizeof t);
        y += envuelve(6, y, COLS, t, C_ERROR, 4) * LIN_H;
    } else {
        buf_init(&b, t, sizeof t);
        if (s->fin == FIN_CORTADO || s->fin == FIN_TOPE) {
            buf_str(&b, "Cortado en t = ");
            buf_str(&b, num_pref(u, sizeof u, sim_reg()->t[sim_reg()->n - 1], 3, " s"));
        } else {
            buf_str(&b, "Emulado de 0 a ");
            buf_str(&b, num_pref(u, sizeof u, s->t_fin, 3, " s"));
            buf_str(&b, ": ");
            buf_int(&b, s->pasos);
            buf_str(&b, " pasos de ");
            buf_str(&b, num_pref(u, sizeof u, s->h, 3, " s"));
        }
        gfx_text(6, y, t, C_TEXTO, 1);
        y += LIN_H;
        if (circ_no_lineal(&cres)) {
            /* Newton (con suaves) o los cambios de estado (solo tramos), y la
             * línea de los LED con la corriente media */
            buf_init(&b, t, sizeof t);
            if (hay_suave_ui(&cres)) {
                buf_str(&b, "Newton: ");
                buf_str(&b, num_auto(u, sizeof u, s->pasos > 0 ? (double)s->nw.iter / (double)s->pasos : 0.0, 1));
                buf_str(&b, " iteraciones/paso, m" S_aa "x. "); buf_int(&b, s->nw.iter_max);
                buf_str(&b, "; "); buf_int(&b, s->nw.sub); buf_str(&b, " partidos");
            } else { buf_int(&b, s->nw.eventos); buf_str(&b, s->nw.eventos == 1 ? " cambio de estado" : " cambios de estado"); }
            gfx_text(6, y, t, C_SUAVE, 1);
            y += LIN_H;
            if (brillo_de(&cres, s->Imed)) { dib_led_linea(y, "Media: ", &cres, s->Imed); y += LIN_H; }
        }
        /* lecturas de V y A, dos por línea */
        buf_init(&b, t, sizeof t);
        for (k = 0, n = 0; k < cres.nel && y < PIE_Y - 2 * LIN_H; k++) {
            const Elemento *e = &cres.el[k];
            static const char *const MOD[MED_N] = { "CC", "CA", "EF" };
            if (e->tipo != EL_VOLT && e->tipo != EL_AMP) continue;
            if (n % 2) buf_str(&b, "   ");
            buf_str(&b, e->etiq); buf_str(&b, " ("); buf_str(&b, MOD[e->variante < MED_N ? e->variante : 0]); buf_str(&b, ") = ");
            buf_str(&b, num_pref(u, sizeof u, s->lectura[k], 4, e->tipo == EL_VOLT ? "V" : "A"));
            if (++n % 2 == 0) { gfx_text(6, y, t, C_INSTR, 1); y += LIN_H; buf_init(&b, t, sizeof t); }
        }
        if (n % 2) { gfx_text(6, y, t, C_INSTR, 1); y += LIN_H; }
        /* el primer aviso: QUEMA > los de un AO (BIESTABLE y ARRANQUE, que
         * dicen por qué satura, antes que SATURA) > los de antes > los demás
         * nuevos */
        {
            unsigned bit = 0, b2;
            if (s->d.avisos & SIM_AV_QUEMA) bit = SIM_AV_QUEMA;
            else if (s->d.avisos & SIM_AV_BIESTABLE) bit = SIM_AV_BIESTABLE;
            else if (s->d.avisos & SIM_AV_ARRANQUE) bit = SIM_AV_ARRANQUE;
            else if (s->d.avisos & SIM_AV_SATURA) bit = SIM_AV_SATURA;
            else {
                for (b2 = 1; b2 <= SIM_AV_SIN_MODOS && !bit; b2 <<= 1)
                    if (s->d.avisos & b2) bit = b2;
                for (b2 = SIM_AV_BIESTABLE; b2 <= SIM_AV_HUECOS && !bit; b2 <<= 1)
                    if (s->d.avisos & b2) bit = b2;
            }
            if (bit) sim_aviso(&cres, s, bit, t, sizeof t);
            else { buf_init(&b, t, sizeof t); buf_str(&b, hay_canal_emu(s) ? "" : "Nada que mirar: pon una sonda con [O]."); }
            {
                int nl = (PIE_Y - y) / LIN_H;
                if (nl > 2) nl = 2;
                if (t[0] && nl > 0) y += envuelve(6, y, COLS, t, C_CURSOR, nl) * LIN_H;
            }
        }
        if (s->canal[0].automatico && hay_canal_emu(s) && y < PIE_Y - 2)
            gfx_text(6, y, "Canales autom" S_aa "ticos: pon los tuyos con [O].", C_SUAVE, 1);
    }
    if (emu_origen == P_EDITOR && error) pie_error("[esc] al editor");
    else if (emu_origen == P_EDITOR) pie("[izq/der] capa  [tab] osciloscopio  [esc] editor");
    else pie(error ? "[esc] volver" : "[izq/der] capa  [tab] osciloscopio  [esc] volver");
}

static void tecla_emu_esq(int t)
{
    const SimRes *s = sim_res();
    int error = s->fin == FIN_ERROR || s->d.estado != SIM_OK;
    if (t == T_ESC) { a_culpable(); pant = emu_origen; }
    else if (error) return;
    else if (t == T_DER) emu_capa = (emu_capa + 1) % CAPA_N;
    else if (t == T_IZQ) emu_capa = (emu_capa + CAPA_N - 1) % CAPA_N;
    else if (t == T_TAB && hay_canal_emu(s) && sim_reg()->n > 1) pant = P_OSC;
}

/* ==========================================================================
 *  Etapa 3: la alterna. P_CA (el resultado), P_CA_TABLA, P_THEVENIN
 * ======================================================================= */
static CaRes  ca_r;                   /* el análisis que se enseña            */
static const char *ca_nota;           /* P_CA: el aviso de la última tecla    */
static CaThev thev;
static int    th_origen;
static Circuito th_c[2];              /* los dos equivalentes, para dibujarlos */

const CaRes *ui_ca(void) { return &ca_r; }

static void analizar_ca(int desde)
{
    cres = circ;
    red_extraer(&cres, &red);
    ca_analiza(&cres, &red, &ca_r);
    ca_calc = 0;
    ca_capa = CAPA_VALORES;
    ca_origen = desde;
    scroll = 0;
    pant = P_CA;
}

/* ---- La calculadora de CA: por trozos en P_EMULANDO, como el emulador ---- */
int ui_ca_sol(void) { return ca_calc ? ca_isol : -1; }
const Fasorial *ui_fasorial(void) { return &fz; }

/* cres = el problema con la solución ca_isol como datos; ca_r, su análisis */
static void pon_solucion_ca(void)
{
    const CalRes *s = cal_res();
    cres = ca_pose;
    red_extraer(&cres, &red);
    if (s->nsol > 0) {
        if (ca_isol >= s->nsol) ca_isol = 0;
        cal_aplica(s, ca_isol, &cres);
        ca_r = s->sol[ca_isol].g;
    } else {
        memset(&ca_r, 0, sizeof ca_r);
        ca_r.d = s->d;
        ca_r.fuente = -1;
    }
}

static void fin_calca(void)
{
    ca_calc = 1;
    ca_isol = 0;
    scroll = 0;
    pon_solucion_ca();
    pant = P_CA;
}

/* Calcula ca_pose (ca_origen ya puesto) con un primer trozo en el acto: un
 * problema corto no pasa por la barra */
static void calcula_ca_de(void)
{
    red_extraer(&ca_pose, &red);
    trabajo = TR_CALCA;
    if (cal_iniciar(&ca_pose, &red) != CAL_OK || !cal_avanzar(SIM_OPS_TICK)) fin_calca();
    else pant = P_EMULANDO;
}

static void calcular_ca(void)
{
    ca_pose = circ;
    ca_origen = P_EDITOR;
    ca_capa = CAPA_VALORES;
    calcula_ca_de();
}

/* ¿La calculadora no halló nada? (el panel enseña su mensaje) */
static int ca_sin_sol(void) { return ca_calc && cal_res()->nsol == 0; }

/* Vuelve a analizar cres (tras cambiar el convenio o la f) */
static void reanaliza_ca(void)
{
    red_extraer(&cres, &red);
    ca_analiza(&cres, &red, &ca_r);
}

/* El convenio con palabras: "(eficaces, coseno)" */
static const char *conv_txt(int conv)
{
    static const char *const T[4] = { "(eficaces, coseno)", "(de pico, coseno)", "(eficaces, seno)", "(de pico, seno)" };
    return T[conv & 3];
}

/* Un fasor interno en el convenio del circuito */
static char *fasor_txt(char *buf, size_t cap, Cx X, const char *u)
{
    double mod, ang;
    ca_a_conv(cres.conv, X, &mod, &ang);
    return num_fasor(buf, cap, mod, ang, ca_rect, 0, u);
}

/* Una Z (no pasa por el convenio: no es una amplitud) */
static char *z_txt(char *buf, size_t cap, Cx Z)
{
    return num_fasor(buf, cap, cx_abs(Z), cx_arg(Z) * 180.0 / 3.14159265358979323846, ca_rect, 0, S_OHM);
}

static void linea_potencias(Buf *b, Cx S)
{
    char u[24];
    buf_str(b, "P = "); buf_str(b, num_pot(u, sizeof u, S.re, "W"));
    buf_str(b, "  Q = "); buf_str(b, num_pot(u, sizeof u, S.im, "var"));
    buf_str(b, "  S = "); buf_str(b, num_pot(u, sizeof u, cx_abs(S), "VA"));
}

/* fp de una S entregada, con su carácter */
static void linea_fp(Buf *b, Cx S)
{
    char u[24];
    double m = cx_abs(S), fp = m > 0.0 ? fabs(S.re) / m : 1.0;
    buf_str(b, "fp = ");
    buf_str(b, num_auto(u, sizeof u, fp, 4));
    if (fabs(S.im) <= 1e-9 * m) buf_str(b, " (resistivo)");
    else if (S.re * S.im < 0.0) buf_str(b, " capacitivo (I adelanta a V)");
    else buf_str(b, " inductivo (I atrasa a V)");
}

static void dib_ca(void)
{
    Marco m;
    Vista v;
    char t[212], u[40];
    Buf b;
    int lin = 0, ok = ca_r.d.estado == SIM_OK && !ca_sin_sol();
    const int L[5] = { 162, 175, 188, 201, 214 };

    buf_init(&b, t, sizeof t);
    buf_str(&b, cres.nombre);
    barra(t, ok ? esq_capa_nombre(ca_capa) : 0);

    esq_vista(&v);
    v.capa = (unsigned char)ca_capa;
    v.red = &red;
    v.conv = cres.conv;
    v.rect = (unsigned char)ca_rect;
    if (ok) v.ca = &ca_r;
    else if (ca_sin_sol() && cal_res()->estado != CAL_SIM) {
        const CalRes *s = cal_res();                /* lo que nombra el mensaje */
        int a = -1, c2 = -1;
        if (s->estado == CAL_INDET) {
            if (s->elem >= 0 && s->elem < s->ninc && s->elem < CAL_MAX_INC) a = s->inc[s->elem].el;
            if (s->iecu >= 0 && s->iecu < s->ninc && s->iecu < CAL_MAX_INC) c2 = s->inc[s->iecu].el;
        } else if (s->estado == CAL_BORDE) {
            if (s->elem >= 0 && s->elem < s->ninc && s->elem < CAL_MAX_INC) a = s->inc[s->elem].el;
        } else a = s->elem;
        if (a >= 0 && a < cres.nel) v.resalta |= 1u << a;
        if (c2 >= 0 && c2 < cres.nel) v.resalta |= 1u << c2;
        v.atenuar = v.resalta != 0;
    }
    else if (ca_r.d.lazo) { v.resalta = ca_r.d.lazo; v.atenuar = 1; }
    else if (ca_r.d.elem >= 0 && ca_r.d.elem < cres.nel) { v.resalta = 1u << ca_r.d.elem; v.atenuar = 1; }
    esq_convenio(cres.conv);
    esq_marco_ajustado(&cres, 0, BARRA_H, SCR_W, RS_PANEL - BARRA_H, &m);
    gfx_clip(0, BARRA_H, SCR_W, RS_PANEL - BARRA_H);
    esq_dibuja(&cres, &m, &v);
    gfx_clip_reset();
    gfx_fill(0, RS_PANEL, SCR_W, PIE_Y - RS_PANEL, C_PANEL);
    separador(RS_PANEL);

    if (!ok) {
        if (ca_sin_sol()) cal_mensaje(&ca_pose, cal_res(), t, sizeof t);
        else sim_mensaje_circ(&cres, 0, &ca_r.d, t, sizeof t);
        envuelve(6, RS_PANEL + 4, COLS, t, C_ERROR, 4);
        if (ca_origen == P_EDITOR) pie_error("[esc] volver al editor y corregirlo");
        else pie("[esc] volver");
        return;
    }
    /* 1. lo que halló la calculadora, y lo que se pedía con [?!] */
    if (ca_calc) {
        const CalRes *s = cal_res();
        int k, j, col = 0, hay_f = 0, n = 0;
        char it[8][40];                             /* se ven 4 como mucho */
        u16 ic[8];
        for (j = 0; j < s->ninc && j < CAL_MAX_INC; j++) if (s->inc[j].que == IN_F) hay_f = 1;
        for (k = 0; k < cres.nel; k++) {
            const Elemento *e = &cres.el[k];
            if (!((ca_r.buscado >> k) & 1u) || n >= 8) continue;
            buf_init(&b, it[n], sizeof it[n]);
            buf_str(&b, e->etiq); buf_str(&b, " = ");
            if (e->tipo == EL_IMP) buf_str(&b, z_txt(u, sizeof u, ca_r.Z[k]));
            else if (e->tipo == EL_FUENTE || e->tipo == EL_CORR) {
                double mo, an;
                ca_canon_a_conv(cres.conv, ca_r.val[k], ca_r.val_ang[k], &mo, &an);
                buf_str(&b, num_fasor(u, sizeof u, mo, an, ca_rect, 0, tipo_uni_val(e->tipo)));
            } else {
                char uu[12];
                Buf bu;
                buf_init(&bu, uu, sizeof uu); buf_ch(&bu, ' '); buf_str(&bu, unidad_val(e));
                buf_str(&b, num_pref(u, sizeof u, ca_r.val[k], 4, uu));
            }
            ic[n++] = C_INCOG;
        }
        if (hay_f && n < 8) {
            buf_init(&b, it[n], sizeof it[n]);
            buf_str(&b, "f = "); buf_str(&b, num_pref(u, sizeof u, ca_r.f, 4, " Hz"));
            ic[n++] = C_INCOG;
        }
        for (k = 0; k < cres.nel; k++) {
            const Elemento *e = &cres.el[k];
            if (e->tipo == EL_VATI && (e->val.est == M_BUSCAR || e->aux.est == M_BUSCAR) && n < 8) {
                buf_init(&b, it[n], sizeof it[n]);          /* lo que marca el vatímetro */
                buf_str(&b, "P("); buf_str(&b, e->etiq); buf_str(&b, ") = ");
                buf_str(&b, num_pref(u, sizeof u, ca_r.lectura[k], 4, " W"));
                ic[n++] = C_TEXTO;
            }
            for (j = 0; j < 2 && n < 8; j++) {
                const Mag *mg = j ? &e->I : &e->V;
                if (mg->est != M_BUSCAR && mg->est_ang != M_BUSCAR) continue;
                if (e->tipo == EL_VATI) continue;
                buf_init(&b, it[n], sizeof it[n]);
                buf_str(&b, j ? "I(" : "V("); buf_str(&b, e->etiq); buf_str(&b, ") = ");
                buf_str(&b, fasor_txt(u, sizeof u, j ? ca_r.I[k] : ca_r.V[k], j ? "A" : "V"));
                ic[n++] = C_TEXTO;
            }
        }
        /* dos por línea si caben; en las dos primeras líneas */
        for (j = 0; j < n && lin < 2; j++) {
            if (gfx_text_w(it[j], 1) > 152) {
                if (col) { col = 0; lin++; if (lin >= 2) break; }
                gfx_text(6, L[lin++], it[j], ic[j], 1);
                continue;
            }
            gfx_text(6 + col * 160, L[lin], it[j], ic[j], 1);
            if (++col == 2) { col = 0; lin++; }
        }
        if (col) lin++;
    }
    /* 2. la f y el convenio */
    buf_init(&b, t, sizeof t);
    buf_str(&b, "f = "); buf_str(&b, num_pref(u, sizeof u, ca_r.f, 4, " Hz"));
    buf_str(&b, "  " S_OMEGA " = "); buf_str(&b, num_pref(u, sizeof u, ca_r.w, 4, " rad/s"));
    buf_str(&b, "  "); buf_str(&b, conv_txt(cres.conv));
    gfx_text(6, L[lin++], t, C_SUAVE, 1);
    /* 3. con una sola fuente: su I y la Z que ve */
    if (ca_r.fuente >= 0 && lin < 5) {
        buf_init(&b, t, sizeof t);
        buf_str(&b, cres.el[ca_r.fuente].etiq); buf_str(&b, ": I = ");
        buf_str(&b, fasor_txt(u, sizeof u, ca_r.I[ca_r.fuente], "A"));
        if (cx_abs(ca_r.Zeq) > 0.0 && b.n + 22 <= COLS) { buf_str(&b, "  Zeq = "); buf_str(&b, z_txt(u, sizeof u, ca_r.Zeq)); }
        gfx_text(6, L[lin++], t, C_TEXTO, 1);
    }
    /* 4. las potencias que entregan las fuentes */
    if (lin < 5) {
        buf_init(&b, t, sizeof t);
        linea_potencias(&b, ca_r.Sfuentes);
        gfx_text(6, L[lin++], t, C_POTENCIA, 1);
    }
    /* 5. soluciones, V(AB) o el fp, y el primer aviso */
    if (lin < 5 && ca_calc && (cal_res()->nsol > 1 || cal_res()->estado == CAL_CORTADO)) {
        const CalRes *s = cal_res();
        buf_init(&b, t, sizeof t);
        if (s->nsol > 1) {
            buf_str(&b, "Hay "); buf_int(&b, s->nsol);
            buf_str(&b, s->mas ? "+ soluciones; " : " soluciones; ");
            buf_str(&b, s->estado == CAL_CORTADO ? "la " : "esta es la ");
            buf_int(&b, ca_isol + 1); buf_str(&b, " [arr/ab]");
        } else buf_str(&b, "1 soluci" S_oo "n");
        if (s->estado == CAL_CORTADO) buf_str(&b, s->nsol > 1 ? " (cortado)" : " (cortado: puede haber m" S_aa "s)");
        gfx_text(6, L[lin++], t, C_CURSOR, 1);
    }
    if (lin < 5 && cres.term_x[0] >= 0 && cres.term_x[1] >= 0) {
        int na = red.punto[cres.term_y[0] * REJ_COLS + cres.term_x[0]], nb = red.punto[cres.term_y[1] * REJ_COLS + cres.term_x[1]];
        if (na >= 0 && nb >= 0) {
            buf_init(&b, t, sizeof t);
            buf_str(&b, "V(AB) = ");
            buf_str(&b, fasor_txt(u, sizeof u, cx_menos(ca_r.phi[na], ca_r.phi[nb]), "V"));
            gfx_text(6, L[lin], t, C_TENSION, 1);
            gfx_text(6 + gfx_text_w(t, 1) + 12, L[lin], "[T] Th" S_ee "venin", C_SUAVE, 1);
            lin++;
        }
    }
    if (lin < 5) {
        buf_init(&b, t, sizeof t);
        linea_fp(&b, ca_r.Sfuentes);
        gfx_text(6, L[lin++], t, C_TEXTO, 1);
    }
    if (lin < 5 && ca_r.d.avisos) {
        unsigned bit = SIM_AV_SATURA;               /* primero: con no lineales, PEQ_SENAL está siempre */
        if (!(ca_r.d.avisos & bit))
            for (bit = 1; bit && !(ca_r.d.avisos & bit); bit <<= 1) {}
        ca_aviso(&cres, &ca_r, bit, t, sizeof t);
        if (!t[0]) sim_aviso(&cres, sim_res(), bit, t, sizeof t);
        envuelve(6, L[lin], COLS, t, C_CURSOR, 1);
    }
    if (ca_nota) {                               /* una nota de la última tecla, en la línea 5 */
        gfx_fill(0, L[4] - 1, SCR_W, 13, C_PANEL);
        gfx_text(6, L[4], ca_nota, C_CURSOR, 1);
    }
    pie("[izq/der] capa [.] " S_ANG "/j [tab] tabla [enter] diagrama");
}

static void thevenin_de(int desde, const Circuito *c, double f);

/* Un modelo de la alterna (cres ya cargado): 1 = los fasores (o la
 * calculadora si pide algo), 2 = el Bode */
static void abre_modelo_ca(int ca)
{
    if (ca == 2) { bode_de(P_EJEMPLOS, &cres); return; }
    if (circ_pide(&cres)) {
        ca_pose = cres;
        ca_origen = P_EJEMPLOS;
        ca_capa = CAPA_VALORES;
        calcula_ca_de();
        return;
    }
    red_extraer(&cres, &red);
    ca_analiza(&cres, &red, &ca_r);
    ca_calc = 0;
    ca_capa = CAPA_VALORES;
    ca_origen = P_EJEMPLOS;
    scroll = 0;
    pant = P_CA;
}

static int tecla_ca(int t)
{
    ca_nota = 0;                                  /* la nota dura hasta la tecla siguiente */
    if (t == T_ESC) { a_culpable(); pant = ca_origen; return 1; }
    if (t == T_IGUAL && ca_calc) { calcula_ca_de(); return 1; }
    if (ca_r.d.estado != SIM_OK || ca_sin_sol()) { if (t == T_IGUAL) reanaliza_ca(); return 1; }
    if (t == T_DER) ca_capa = (ca_capa + 1) % CAPA_N;
    else if (t == T_IZQ) ca_capa = (ca_capa + CAPA_N - 1) % CAPA_N;
    else if (t == T_PUNTO) ca_rect = !ca_rect;
    else if (t == T_TAB) { ca_pag = 0; scroll = 0; pant = P_CA_TABLA; }
    else if (t == T_ENTER) { fz_nuevo(&fz, &cres, &ca_r); pant = P_FASORIAL; }
    else if ((t == T_ARRIBA || t == T_ABAJO) && ca_calc) {
        int n = cal_res()->nsol;
        if (n > 1) { ca_isol = (ca_isol + (t == T_ABAJO ? 1 : n - 1)) % n; pon_solucion_ca(); }
    }
    else if (t == T_LETRA('T')) {
        if (cres.term_x[0] >= 0 && cres.term_x[1] >= 0) thevenin_de(P_CA, &cres, ca_r.f);
        else ca_nota = "Pon los terminales A y B con [T] en el editor.";
    }
    else if (t == T_LETRA('O')) emula_cres(P_CA);
    else if (t == T_LETRA('B')) {
        if (ca_origen == P_BODE) { pant = P_BODE; return 1; }   /* se vino del Bode: sin barrer */
        bode_de(P_CA, &cres);                                   /* cres ya lleva la solución */
    }
    else if (t == T_MENU) abre_ajustes(P_CA);
    else if (t == T_IGUAL) reanaliza_ca();
    return 1;
}

/* ω sin prefijo (en «377,0t» no cabe un «k»): 4 cifras y, de 1000 en
 * adelante, entera */
static char *omega_txt(char *buf, size_t cap, double w)
{
    int dec = w >= 1000.0 || !(w > 0.0) ? 0 : 3 - (int)floor(log10(w));
    if (w >= 1e6 && w < 1e300) {                 /* desde 1E6, con exponente: «1E9», no «1000000000» */
        int e = (int)floor(log10(w));
        double m = w / pow(10.0, (double)e);
        char t[24];
        Buf b;
        if (floor(m * 1000.0 + 0.5) >= 10000.0) { m /= 10.0; e++; }   /* 9,9996 → 1E(e+1) */
        buf_init(&b, buf, cap);
        buf_str(&b, num_auto(t, sizeof t, m, 3));
        buf_ch(&b, 'E'); buf_int(&b, e);
        return buf;
    }
    return num_txt(buf, cap, w, dec);
}

/* Un fasor interno en el tiempo, como lo pide el libro: la amplitud de
 * pico y el coseno o el seno del convenio del circuito, «13,46 cos(377,0t
 * + 37,5°) A» (el prefijo va con la unidad). Como mucho unas 38 columnas
 * con ω de 4 cifras */
static char *t_txt(char *buf, size_t cap, Cx X, double w, const char *u)
{
    char n[24], a[24], *pre;
    Buf b;
    double mod, ang;
    ca_a_conv(cres.conv | CONV_PICO, X, &mod, &ang);
    num_pref(n, sizeof n, mod, 4, " ");          /* «13,46 m»: la cifra, un espacio y su prefijo */
    pre = strchr(n, ' ');
    if (pre) *pre++ = 0; else pre = n + strlen(n);
    buf_init(&b, buf, cap);
    buf_str(&b, n);
    buf_str(&b, (cres.conv & CONV_SENO) ? " sen(" : " cos(");
    buf_str(&b, omega_txt(a, sizeof a, w));
    buf_ch(&b, 't');
    if (fabs(ang) >= 0.05) {                     /* sin «+ 0,0°» */
        buf_str(&b, ang < 0.0 ? " - " : " + ");
        buf_str(&b, num_txt(a, sizeof a, fabs(ang), 1));
        buf_str(&b, S_DEG);
    }
    buf_str(&b, ") ");
    buf_str(&b, pre);
    buf_str(&b, u);
    return buf;
}

/* ---- La tabla de la CA: 5 páginas ---- */
#define CA_PAGS 5
static void dib_ca_tabla(void)
{
    static const char *const TIT[CA_PAGS] = { "Fasores", "Potencias: absorbida o entregada", "Totales: Boucherot y Tellegen",
                                              "Potenciales de nodo", "En el tiempo" };
    char t[96], u[40];
    Buf b;
    int k, y = 36, fila = 0;
    buf_init(&b, t, sizeof t);
    buf_str(&b, TIT[ca_pag]);
    if (ca_pag == 0) { buf_str(&b, " a "); buf_str(&b, num_pref(u, sizeof u, ca_r.f, 4, " Hz")); buf_ch(&b, ' '); buf_str(&b, conv_txt(cres.conv)); }
    if (ca_pag == 4) { buf_str(&b, " (" S_OMEGA " = "); buf_str(&b, omega_txt(u, sizeof u, ca_r.w)); buf_str(&b, " rad/s)"); }
    {
        char p[8];
        Buf bp;
        buf_init(&bp, p, sizeof p);
        buf_int(&bp, ca_pag + 1); buf_ch(&bp, '/'); buf_int(&bp, CA_PAGS);
        barra(t, p);
    }
    if (ca_pag == 0) {
        gfx_text(6, 20, "Elem", C_TITULO, 1); gfx_text(34, 20, "V", C_TENSION, 1);
        gfx_text(128, 20, "I", C_CORRIENTE, 1);
        gfx_text(222, 20, circ_no_lineal(&cres) ? "Z / regi" S_oo "n" : "Z", C_IMP, 1);
        for (k = 0; k < cres.nel; k++) {
            const Elemento *e = &cres.el[k];
            if (fila++ < scroll) continue;
            if (y > PIE_Y - 14) continue;
            gfx_text(6, y, e->etiq, tipo_info(e->tipo)->color, 1);
            if (e->tipo == EL_VOLT || e->tipo == EL_AMP) {
                buf_init(&b, t, sizeof t);
                buf_str(&b, "marca ~");
                buf_str(&b, num_pref(u, sizeof u, ca_r.lectura[k], 4, e->tipo == EL_VOLT ? "V" : "A"));
                gfx_text(34, y, t, C_INSTR, 1);
            } else if (e->tipo == EL_VATI) {
                buf_init(&b, t, sizeof t);
                buf_str(&b, "marca "); buf_str(&b, num_pref(u, sizeof u, ca_r.lectura[k], 4, "W"));
                gfx_text(34, y, t, C_INSTR, 1);
                gfx_text(128, y, fasor_txt(u, sizeof u, ca_r.I[k], "A"), C_TEXTO, 1);
            } else {
                gfx_text(34, y, fasor_txt(u, sizeof u, ca_r.V[k], "V"), C_TEXTO, 1);
                gfx_text(128, y, fasor_txt(u, sizeof u, ca_r.I[k], "A"), C_TEXTO, 1);
                if (circ_v_convenio(e->tipo) == -1 || e->tipo == EL_INTER) gfx_text(222, y, S_RAYA, C_SUAVE, 1);
                else if (es_nl_ui(e->tipo)) gfx_text(222, y, reg_texto(ca_r.reg[k]), reg_color(ca_r.reg[k]), 1);  /* en Q */
                else gfx_text(222, y, z_txt(u, sizeof u, ca_r.Z[k]), C_TEXTO, 1);
            }
            y += 14;
        }
    } else if (ca_pag == 1) {
        gfx_text(6, 20, "Elem", C_TITULO, 1); gfx_text(34, 20, "P", C_POTENCIA, 1);
        gfx_text(104, 20, "Q", C_REACT, 1); gfx_text(174, 20, "S", C_TEXTO, 1); gfx_text(244, 20, "fp", C_TEXTO, 1);
        for (k = 0; k < cres.nel; k++) {
            const Elemento *e = &cres.el[k];
            Cx S = ca_r.S[k];
            if (e->tipo == EL_AMP || e->tipo == EL_INTER || (e->tipo == EL_VOLT && e->val.est != M_DATO)) continue;
            if (fila++ < scroll) continue;
            if (y > PIE_Y - 14) continue;
            gfx_text(6, y, e->etiq, tipo_info(e->tipo)->color, 1);
            {
                double esc = cx_abs(ca_r.Sfuentes) + cx_abs(ca_r.Scargas) + 1e-300;
                gfx_text(34, y, num_pot(u, sizeof u, fabs(S.re) <= 1e-12 * esc ? 0.0 : S.re, "W"), C_POTENCIA, 1);
                gfx_text(104, y, num_pot(u, sizeof u, fabs(S.im) <= 1e-12 * esc ? 0.0 : S.im, "var"), C_REACT, 1);
                gfx_text(174, y, num_pot(u, sizeof u, cx_abs(S), "VA"), C_TEXTO, 1);
            }
            if (cx_abs(S) > 0.0) {
                buf_init(&b, t, sizeof t);
                double pp = fabs(S.re) <= 1e-9 * cx_abs(S) ? 1.0 : S.re;   /* P = 0: manda Q */
                buf_str(&b, num_auto(u, sizeof u, fabs(S.re) / cx_abs(S), 3));
                if (fabs(S.im) > 1e-9 * cx_abs(S)) buf_str(&b, pp * S.im < 0.0 ? " c" : " i");
                gfx_text(244, y, t, C_TEXTO, 1);
            }
            y += 14;
        }
    } else if (ca_pag == 2) {
        double PR = 0.0, QL = 0.0, QC = 0.0;
        Cx Sc = ca_r.Scargas;
        for (k = 0; k < cres.nel; k++) {
            if (cres.el[k].tipo == EL_BOB) QL += ca_r.S[k].im;
            else if (cres.el[k].tipo == EL_COND) QC += ca_r.S[k].im;
            else if (cres.el[k].tipo == EL_RES || cres.el[k].tipo == EL_IMP) PR += ca_r.S[k].re;
        }
        y = 22;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Entregan las fuentes:  P "); buf_str(&b, num_pot(u, sizeof u, ca_r.Sfuentes.re, "W"));
        buf_str(&b, "   Q "); buf_str(&b, num_pot(u, sizeof u, ca_r.Sfuentes.im, "var"));
        gfx_text(6, y, t, C_POTENCIA, 1); y += 16;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Absorben las cargas:   P "); buf_str(&b, num_pot(u, sizeof u, Sc.re, "W"));
        buf_str(&b, "   Q "); buf_str(&b, num_pot(u, sizeof u, Sc.im, "var"));
        gfx_text(6, y, t, C_TEXTO, 1); y += 16;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "  R: "); buf_str(&b, num_pot(u, sizeof u, PR, "W"));
        buf_str(&b, "   L: "); buf_str(&b, num_pot(u, sizeof u, QL, "var"));
        buf_str(&b, "   C: "); buf_str(&b, num_pot(u, sizeof u, QC, "var"));
        gfx_text(6, y, t, C_SUAVE, 1); y += 16;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "S = "); buf_str(&b, num_pot(u, sizeof u, cx_abs(ca_r.Sfuentes), "VA"));
        buf_str(&b, "   "); linea_fp(&b, ca_r.Sfuentes);
        {
            char *par = strstr(t, " (");                 /* sin el paréntesis: cabe en una línea */
            if (par) *par = 0;
        }
        gfx_text(6, y, t, C_TEXTO, 1); y += 16;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Tellegen: " S_SIGMA "V" S_CDOT "I* = 0 ");
        buf_str(&b, ca_r.tellegen <= 1e-9 ? S_CHECK : "(no cuadra)");
        buf_str(&b, " (");
        if (ca_r.tellegen > 0.0) {
            int ex = (int)floor(log10(ca_r.tellegen));
            buf_str(&b, num_auto(u, sizeof u, floor(ca_r.tellegen / pow(10.0, (double)ex) + 0.5), 0));
            buf_str(&b, "e"); buf_int(&b, ex);
        } else buf_ch(&b, '0');
        buf_str(&b, " de la escala)");
        gfx_text(6, y, t, ca_r.tellegen <= 1e-9 ? C_OK : C_ERROR, 1); y += 16;
        if (ca_r.fuente >= 0 && cx_abs(ca_r.Zeq) > 0.0) {
            buf_init(&b, t, sizeof t);
            buf_str(&b, "Z vista por "); buf_str(&b, cres.el[ca_r.fuente].etiq); buf_str(&b, " = ");
            buf_str(&b, num_fasor(u, sizeof u, cx_abs(ca_r.Zeq), cx_arg(ca_r.Zeq) * 180.0 / 3.14159265358979323846, 0, 0, S_OHM));
            buf_str(&b, " = ");
            buf_str(&b, num_fasor(u, sizeof u, cx_abs(ca_r.Zeq), cx_arg(ca_r.Zeq) * 180.0 / 3.14159265358979323846, 1, 0, S_OHM));
            envuelve(6, y, COLS, t, C_TEXTO, 2);
        }
    } else if (ca_pag == 3) {
        int n, p, col = 0;
        for (p = 0; p < red.nn + 2; p++) {           /* en el orden de sus letras (red_letra) */
            char nombre[4];
            if ((n = red_nodo_puesto(&red, p)) < 0) continue;
            if (fila++ < scroll * 2) continue;
            if (y > PIE_Y - 14) continue;
            nombre[0] = red_letra(&red, n); nombre[1] = 0;
            buf_init(&b, t, sizeof t);
            buf_str(&b, nombre); buf_ch(&b, ' ');
            buf_str(&b, fasor_txt(u, sizeof u, ca_r.phi[n], "V"));
            gfx_text(6 + col * 158, y, t, C_TENSION, 1);
            if (++col == 2) { col = 0; y += 14; }
        }
    } else {
        /* En el tiempo: v(t) e i(t) de cada elemento, dos líneas, sin lo
         * que es nulo (la I de un voltímetro ideal, la V de un amperímetro
         * ideal o de un interruptor cerrado). [arr/ab] de elemento en
         * elemento. Con no lineales o una fuente que no es alterna, el
         * fasor es solo la parte alterna: se dice. */
        double escV = 0.0, escI = 0.0;
        int solo_ca = !circ_no_lineal(&cres);
        for (k = 0; k < cres.nel; k++) {
            int tp = cres.el[k].tipo;
            if (cx_abs(ca_r.V[k]) > escV) escV = cx_abs(ca_r.V[k]);
            if (cx_abs(ca_r.I[k]) > escI) escI = cx_abs(ca_r.I[k]);
            if ((tp == EL_FUENTE || tp == EL_CORR) && !((ca_r.activa >> k) & 1u)) solo_ca = 0;
        }
        gfx_text(6, 20, solo_ca ? ((cres.conv & CONV_SENO) ? "Amplitudes de pico, con seno (el convenio)"
                                                           : "Amplitudes de pico, con coseno (el convenio)")
                                : "Solo la parte alterna: falta la de CC (punto Q)",
                 solo_ca ? C_SUAVE : C_CURSOR, 1);
        for (k = scroll; k < cres.nel && y <= PIE_Y - 13; k++) {
            const Elemento *e = &cres.el[k];
            int hv = cx_abs(ca_r.V[k]) > 1e-12 * escV, hi = cx_abs(ca_r.I[k]) > 1e-12 * escI;
            gfx_text(6, y, e->etiq, tipo_info(e->tipo)->color, 1);
            if (!hv && !hi) { gfx_text(34, y, "v = 0, i = 0", C_SUAVE, 1); y += 13; continue; }
            if (hv) {
                buf_init(&b, t, sizeof t);
                buf_str(&b, "v = "); buf_str(&b, t_txt(u, sizeof u, ca_r.V[k], ca_r.w, "V"));
                gfx_text(34, y, t, C_TENSION, 1);
                y += 13;
            }
            if (hi && y <= PIE_Y - 13) {
                buf_init(&b, t, sizeof t);
                buf_str(&b, "i = "); buf_str(&b, t_txt(u, sizeof u, ca_r.I[k], ca_r.w, "A"));
                gfx_text(34, y, t, C_CORRIENTE, 1);
                y += 13;
            }
        }
    }
    pie("[izq/der] p" S_aa "gina [.] " S_ANG "/j [tab] diagrama [esc]");
}

static int tecla_ca_tabla(int t)
{
    if (t == T_DER) { ca_pag = (ca_pag + 1) % CA_PAGS; scroll = 0; }
    else if (t == T_IZQ) { ca_pag = (ca_pag + CA_PAGS - 1) % CA_PAGS; scroll = 0; }
    else if (t == T_ABAJO || t == T_ARRIBA) {       /* una fila; [shift] una página; [ctrl] al extremo */
        int sg = t == T_ABAJO ? 1 : -1, tope = cres.nel > 1 ? cres.nel - 1 : 0;
        if (mods & T_CON_CTRL) scroll = sg > 0 ? tope : 0;
        else scroll += sg * ((mods & T_CON_SHIFT) ? TB_VIS : 1);
        if (scroll > tope) scroll = tope;
        if (scroll < 0) scroll = 0;
    }
    else if (t == T_PUNTO) ca_rect = !ca_rect;
    else if (t == T_TAB) { fz_nuevo(&fz, &cres, &ca_r); pant = P_FASORIAL; }
    else if (t == T_ESC) pant = P_CA;
    return 1;
}

/* ---- El diagrama fasorial y el triángulo de potencias ---- */
static void dib_fasorial(void) { fz_dibuja(&fz, &cres, &ca_r, cres.conv, ca_rect); }

static int tecla_fasorial(int t)
{
    if (t == T_PUNTO) { ca_rect = !ca_rect; return 1; }
    if (fz_tecla(&fz, &cres, &ca_r, t) == FZ_SALIR) pant = P_CA;
    return 1;
}

/* ---- Thévenin y Norton ---- */
/* El equivalente como un circuito que se dibuja: la E (o la J) a la
 * izquierda y la Z (o la R) en serie (o en paralelo), con A y B */
static void arma_thev(Circuito *c, const CaThev *th, int norton, int conv)
{
    int k, ca = th->f > 0.0;
    circ_init(c, norton ? "Norton" : "Th" S_ee "venin");
    c->conv = (unsigned char)conv;
    if (ca) { c->frec.v = th->f; c->frec.est = M_DATO; }
    if (!norton) {
        k = circ_elemento(c, EL_FUENTE, 0, 1, DIR_ARR);
        if (ca) {
            c->el[k].variante = FORMA_FASOR;
            c->el[k].val.v = cx_abs(th->Vth) / sqrt(2.0);
            c->el[k].val.ang = cx_arg(th->Vth) * 180.0 / 3.14159265358979323846; c->el[k].val.est_ang = M_DATO;
        } else c->el[k].val.v = th->Vth.re;
        c->el[k].val.est = M_DATO;
        k = circ_elemento(c, ca ? EL_IMP : EL_RES, 0, 0, DIR_DER);
        circ_cable(c, 0, 1, DIR_DER);
        c->term_x[0] = 1; c->term_y[0] = 0; c->term_x[1] = 1; c->term_y[1] = 1;
    } else {
        k = circ_elemento(c, EL_CORR, 0, 1, DIR_ARR);
        if (ca) {
            c->el[k].variante = FORMA_FASOR;
            c->el[k].val.v = cx_abs(th->In) / sqrt(2.0);
            c->el[k].val.ang = cx_arg(th->In) * 180.0 / 3.14159265358979323846; c->el[k].val.est_ang = M_DATO;
        } else c->el[k].val.v = th->In.re;
        c->el[k].val.est = M_DATO;
        circ_cable(c, 0, 0, DIR_DER); circ_cable(c, 0, 1, DIR_DER);
        k = circ_elemento(c, ca ? EL_IMP : EL_RES, 1, 0, DIR_ABA);
        c->term_x[0] = 2; c->term_y[0] = 0; c->term_x[1] = 2; c->term_y[1] = 1;
        circ_cable(c, 1, 0, DIR_DER); circ_cable(c, 1, 1, DIR_DER);
    }
    if (ca) {
        c->el[k].val.v = cx_abs(th->Zth);
        c->el[k].val.ang = cx_arg(th->Zth) * 180.0 / 3.14159265358979323846;
        if (c->el[k].val.ang > 90.0) c->el[k].val.ang = 90.0;
        if (c->el[k].val.ang < -90.0) c->el[k].val.ang = -90.0;
        c->el[k].val.est_ang = M_DATO;
    } else c->el[k].val.v = th->Zth.re;
    c->el[k].val.est = M_DATO;
}

static void thevenin_de(int desde, const Circuito *c, double f)
{
    static Circuito tmp;
    static Red rt;
    tmp = *c;
    red_extraer(&tmp, &rt);
    ca_thevenin(&tmp, &rt, f, &thev);
    if (thev.d.estado == SIM_OK) {
        if (thev.hay_th) arma_thev(&th_c[0], &thev, 0, c->conv);
        if (thev.hay_no) arma_thev(&th_c[1], &thev, 1, c->conv);
    }
    th_origen = desde;
    pant = P_THEVENIN;
}

static void dib_thevenin(void)
{
    char t[160], u[40], d[40];
    Buf b;
    int y = 108, conv = cres.conv, ca = thev.f > 0.0;
    barra("Th" S_ee "venin y Norton entre A y B", ca ? num_pref(d, sizeof d, thev.f, 4, " Hz") : "CC");
    if (thev.d.estado != SIM_OK) {
        sim_mensaje_circ(&cres, 0, &thev.d, t, sizeof t);
        envuelve(6, BARRA_H + 8, COLS, t, C_ERROR, 4);
        pie("[esc] volver");
        return;
    }
    {
        int i;
        for (i = 0; i < 2; i++) {
            Marco m;
            Vista v;
            int x0 = i * 160;
            gfx_text_p(x0 + 6, 18, i ? "Norton" : "Th" S_ee "venin", C_TITULO);
            if (!(i ? thev.hay_no : thev.hay_th)) {
                envuelve(x0 + 6, 40, 25, thev.inf ? "Rth infinita en CC: A y B solo los une un C." :
                         "Zth = 0: no hay Norton (IN ser" S_ii "a infinita).", C_SUAVE, 4);
                continue;
            }
            esq_vista(&v);
            v.conv = (unsigned char)conv;
            esq_convenio(conv);
            esq_marco_ajustado(&th_c[i], x0, 28, 160, 76, &m);
            gfx_clip(x0, 28, 160, 76);
            esq_dibuja(&th_c[i], &m, &v);
            gfx_clip_reset();
        }
    }
    separador(105);
    /* Vth */
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Vth = V(AB) en vac" S_ii "o      = ");
    if (ca) { double mo, an; ca_a_conv(conv, thev.Vth, &mo, &an); buf_str(&b, num_fasor(u, sizeof u, mo, an, ca_rect, 0, "V")); }
    else buf_str(&b, num_pref(u, sizeof u, thev.Vth.re, 4, " V"));
    gfx_text(6, y, t, C_TENSION, 1); y += 13;
    /* Zth: el ohmímetro */
    buf_init(&b, t, sizeof t);
    if (thev.inf) buf_str(&b, "Rth = R(AB), fuentes a 0 = infinita");
    else if (ca) { buf_str(&b, "Zth = Z(AB), fuentes a 0  = "); buf_str(&b, z_txt(u, sizeof u, thev.Zth)); }
    else { buf_str(&b, "Rth = R(AB), fuentes a 0 = "); buf_str(&b, num_pref(u, sizeof u, thev.Zth.re, 4, " " S_OHM)); }
    gfx_text(6, y, t, C_IMP, 1); y += 13;
    if (thev.hay_no) {
        buf_init(&b, t, sizeof t);
        buf_str(&b, "IN  = de A a B en corto   = ");
        if (ca) { double mo, an; ca_a_conv(conv, thev.In, &mo, &an); buf_str(&b, num_fasor(u, sizeof u, mo, an, ca_rect, 0, "A")); }
        else buf_str(&b, num_pref(u, sizeof u, thev.In.re, 4, " A"));
        gfx_text(6, y, t, C_CORRIENTE, 1);
    }
    y += 13;
    gfx_text(6, y, "M" S_aa "xima transferencia a una carga entre A y B:", C_TITULO, 1); y += 13;
    if (!thev.max_finito || thev.inf) {
        gfx_text(6, y, thev.inf ? "Sin carga posible: no circula corriente en CC." :
                                  "Rth = 0: la potencia no tiene m" S_aa "ximo finito.", C_SUAVE, 1);
    } else if (ca) {
        buf_init(&b, t, sizeof t);
        buf_str(&b, "ZL = Zth* = ");
        buf_str(&b, num_fasor(u, sizeof u, cx_abs(thev.Zl), cx_arg(thev.Zl) * 180.0 / 3.14159265358979323846, 1, 0, S_OHM));
        buf_str(&b, " " S_RARR " PL = "); buf_str(&b, num_pref(u, sizeof u, thev.Pmax, 4, " W"));
        gfx_text(6, y, t, C_OK, 1); y += 13;
        if (thev.Lc > 0.0 || thev.Cc > 0.0) {
            buf_init(&b, t, sizeof t);
            buf_ch(&b, '('); buf_str(&b, num_pref(u, sizeof u, thev.Zl.re, 4, " " S_OHM));
            buf_str(&b, thev.Lc > 0.0 ? " con L = " : " con C = ");
            buf_str(&b, thev.Lc > 0.0 ? num_pref(u, sizeof u, thev.Lc, 4, " H") : num_pref(u, sizeof u, thev.Cc, 4, " F"));
            buf_str(&b, " en serie, a "); buf_str(&b, num_pref(d, sizeof d, thev.f, 4, " Hz")); buf_ch(&b, ')');
            gfx_text(6, y, t, C_SUAVE, 1);
        }
        y += 13;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Solo R: RL = |Zth| = "); buf_str(&b, num_pref(u, sizeof u, thev.Rsola, 4, " " S_OHM));
        buf_str(&b, " " S_RARR " PL = "); buf_str(&b, num_pref(d, sizeof d, thev.Psola, 4, " W"));
        gfx_text(6, y, t, C_TEXTO, 1);
    } else {
        buf_init(&b, t, sizeof t);
        buf_str(&b, "RL = Rth = "); buf_str(&b, num_pref(u, sizeof u, thev.Zth.re, 4, " " S_OHM));
        buf_str(&b, " " S_RARR " PL = "); buf_str(&b, num_pref(d, sizeof d, thev.Pmax, 4, " W"));
        gfx_text(6, y, t, C_OK, 1);
    }
    pie("[.] polar o rectangular              [esc] volver");
}

static int tecla_thevenin(int t)
{
    if (t == T_ESC) pant = th_origen;
    else if (t == T_PUNTO) ca_rect = !ca_rect;
    return 1;
}

/* ---- El Bode: barrer por trozos en P_EMULANDO y mirarlo en P_BODE ---- */
const Trazador *ui_trazador(void) { return &traz; }

static void bode_de(int desde, const Circuito *c)
{
    int k;
    bode_c = *c;
    red_extraer(&bode_c, &bode_red);
    bode_origen = desde;
    /* la entrada de [F] va por su etiqueta: al borrar un elemento de delante
     * los índices corren y E2 sigue siendo E2; si ya no está, la automática */
    if (bode_aj.ent >= 0) {
        for (k = 0; k < bode_c.nel; k++)
            if ((bode_c.el[k].tipo == EL_FUENTE || bode_c.el[k].tipo == EL_CORR) &&
                strcmp(bode_c.el[k].etiq, bode_ent_etiq) == 0) break;
        bode_aj.ent = (signed char)(k < bode_c.nel ? k : -1);
    }
    bode_iniciar(&bode_c, &bode_red, &bode_aj);
    if (bode_res()->fin == FIN_ERROR) { pant = P_BODE; return; }
    trabajo = TR_BODE;
    /* un primer trozo en el acto: un barrido corto no pasa por la barra */
    if (!bode_avanzar(SIM_OPS_TICK)) fin_bode();
    else pant = P_EMULANDO;
}

static void fin_bode(void)
{
    const BodeRes *r = bode_res();
    traz_nuevo(&traz, bode_reg(), r);
    /* los avisos del barrido (52 columnas como mucho); con no lineales, el
     * AO saturado en Q (la H nula) y si no, la pequeña señal (§4.12) */
    if (r->d.avisos & (SIM_AV_SATURA | SIM_AV_PEQ_SENAL | SIM_AV_OTRAS | SIM_AV_Z_ESCALA)) {
        Buf b;
        buf_init(&b, traz.aviso, sizeof traz.aviso);
        if (r->d.avisos & SIM_AV_SATURA) {             /* el AO de elem_aviso (ca_punto_q); si no, el primero */
            int k = r->d.elem_aviso;
            if (k < 0 || k >= bode_c.nel || bode_c.el[k].tipo != EL_AO)
                for (k = 0; k < bode_c.nel && bode_c.el[k].tipo != EL_AO; k++) ;
            buf_str(&b, k < bode_c.nel ? bode_c.el[k].etiq : "Un AO");
            buf_str(&b, " satura en Q: su salida no sigue (H = 0).");
        } else if (r->d.avisos & SIM_AV_PEQ_SENAL) ca_aviso(&bode_c, 0, SIM_AV_PEQ_SENAL, traz.aviso, sizeof traz.aviso);
        else if (r->d.avisos & SIM_AV_OTRAS) {
            buf_str(&b, "Las dem" S_aa "s fuentes se apagan (entrada: ");
            buf_str(&b, r->nom_ent); buf_str(&b, ").");
        } else buf_str(&b, "La Z es de la f del circuito: a otra, R y L (o C).");
        traz.aviso_col = C_CURSOR;
    }
    pant = P_BODE;
}

/* Otra entrada: la siguiente E o J */
static void bode_otra_entrada(void)
{
    int k, n = bode_c.nel, ent = bode_res()->ent;
    for (k = 1; k <= n; k++) {
        int q = (ent + k) % n;
        if (bode_c.el[q].tipo == EL_FUENTE || bode_c.el[q].tipo == EL_CORR) {
            bode_aj.ent = (signed char)q;
            memcpy(bode_ent_etiq, bode_c.el[q].etiq, sizeof bode_ent_etiq);
            break;
        }
    }
}

/* Otra salida: sonda 1, sonda 2, V(AB), la automática, las que haya */
static void bode_otra_salida(void)
{
    int i, s1 = bode_c.sonda[0].tipo != SONDA_NO, ab = bode_c.term_x[0] >= 0 && bode_c.term_x[1] >= 0;
    int actual = bode_aj.sal;
    /* la automática es la sonda 1 si la hay, o V(AB): entonces no es otra salida */
    if (actual == BS_AUTO && s1) actual = BS_SONDA1;
    else if (actual == BS_AUTO && ab) actual = BS_AB;
    for (i = 1; i <= 4; i++) {
        int s = (actual + i) % 4;
        if (s == BS_AUTO && (s1 || ab)) continue;
        if (s == BS_SONDA1 && !s1) continue;
        if (s == BS_SONDA2 && bode_c.sonda[1].tipo == SONDA_NO) continue;
        if (s == BS_AB && !ab) continue;
        bode_aj.sal = (unsigned char)s;
        return;
    }
}

static void dib_bode(void)
{
    if (bode_res()->fin == FIN_ERROR) {
        char t[212];
        barra("Bode", 0);
        sim_mensaje_circ(&bode_c, 0, &bode_res()->d, t, sizeof t);
        envuelve(6, BARRA_H + 8, COLS, t, C_ERROR, 5);
        pie("[esc] volver");
        return;
    }
    traz_dibuja(&traz, bode_reg(), bode_res());
}

static int tecla_bode(int t)
{
    if (bode_res()->fin == FIN_ERROR) { if (t == T_ESC) pant = bode_origen; return 1; }
    switch (traz_tecla(&traz, bode_reg(), bode_res(), t | mods)) {
    case TZ_SALIR: pant = bode_origen; break;
    case TZ_BARRER: {                              /* la ventana que se ve, con 277 puntos */
        BodeAjustes a = bode_aj;
        bode_aj.f_ini = pow(10.0, traz.lf0);
        bode_aj.f_fin = pow(10.0, traz.lf1);
        bode_de(bode_origen, &bode_c);
        bode_aj = a;
        break;
    }
    case TZ_ENTRADA: bode_otra_entrada(); bode_de(bode_origen, &bode_c); break;
    case TZ_SALIDA: bode_otra_salida(); bode_de(bode_origen, &bode_c); break;
    case TZ_AJUSTES: abre_ajustes(P_BODE); break;
    case TZ_FASORES: {                             /* los fasores a la f del cursor */
        double f = traz.fcur[traz.activo];
        /* si el Bode salió de P_CA, P_CA pasa a volver al Bode y el Bode al
         * origen de aquella P_CA: si no, [esc] iría y vendría entre los dos */
        if (bode_origen == P_CA) bode_origen = ca_origen == P_BODE ? P_EDITOR : ca_origen;
        cres = bode_c;
        red_extraer(&cres, &red);
        ca_analiza_f(&cres, &red, f, &ca_r);
        ca_calc = 0;
        ca_capa = CAPA_VALORES;
        ca_origen = P_BODE;
        pant = P_CA;
        break;
    }
    default: break;
    }
    return 1;
}

/* ==========================================================================
 *  Etapa 4: el punto de operación (P_RESULTADO y P_TABLA con res_nl) y el
 *  barrido de CC (P_BARRIDO, con curva.c)
 * ======================================================================= */
int ui_res_nl(void) { return res_nl; }
const Curva *ui_curva(void) { return &curv; }

/* Las regiones de los no lineales (Valores.reg) y su color */
static const char *const REG_NOM[REG_N] = {
    "", "corte", "conduce", "ruptura", "activa", "saturaci" S_oo "n",
    "inversa", S_oo "hmica", "lineal", "satura +", "satura -"
};
static u16 reg_color(int r) { return r == REG_CONDUCE ? C_OK : r == REG_CORTE ? C_SUAVE : C_CURSOR; }
static const char *reg_texto(int r) { return r >= 0 && r < REG_N ? REG_NOM[r] : ""; }

static int es_nl_ui(int tipo) { return tipo_info(tipo)->ley_cc == LEY_NL; }

/* El primer no lineal (para los avisos: «Sin paso a paso: D1 no es lineal.») */
static const char *primer_nl_ui(const Circuito *c)
{
    int k;
    for (k = 0; k < c->nel; k++) if (es_nl_ui(c->el[k].tipo)) return c->el[k].etiq;
    return "?";
}

/* ¿Algún suave (Newton de verdad)? Si no, todo es por tramos */
static int hay_suave_ui(const Circuito *c)
{
    int k;
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        if ((e->tipo == EL_DIODO && e->variante >= DIO_EXP) || (e->tipo == EL_ZENER && e->variante >= ZEN_EXP) ||
            e->tipo == EL_LED || e->tipo == EL_BJT || e->tipo == EL_MOS) return 1;
    }
    return 0;
}

/* La única fuente independiente, o -1 (como resolver) */
static int fuente_unica(const Circuito *c)
{
    int k, f = -1, nf = 0;
    for (k = 0; k < c->nel; k++) if (tipo_info(c->el[k].tipo)->fuente) { nf++; f = k; }
    return nf == 1 ? f : -1;
}

/* ---- Los LED: su brillo (§4.6) y su línea (§4.5) ---- */
static const char *const LED_NOM[LED_N] = { "rojo", "naranja", "amarillo", "verde", "azul", "blanco" };
static unsigned char brillo_ui[MAX_EL];

static double par_ui(const Elemento *e, int q, double def)
{
    const Mag *m = &e->par[q - Q_P1];
    return m->est == M_DATO ? m->v : def;
}

static void led_if_im(const Elemento *e, double *If, double *Im)
{
    *If = par_ui(e, QL_IF, NL_IF);
    *Im = par_ui(e, QL_IM, NL_IM);
}

/* Los niveles de brillo de cada LED con sus corrientes I; NULL si no hay LED */
static const unsigned char *brillo_de(const Circuito *c, const double *I)
{
    int k, hay = 0;
    memset(brillo_ui, 0, sizeof brillo_ui);
    for (k = 0; k < c->nel; k++)
        if (c->el[k].tipo == EL_LED) {
            double If, Im;
            led_if_im(&c->el[k], &If, &Im);
            brillo_ui[k] = (unsigned char)esq_led_nivel(I[k], If, Im);
            hay = 1;
        }
    return hay ? brillo_ui : 0;
}

/* La línea de los LED: «D3 rojo 21,8 mA 100 %  D4 verde apagado», con los
 * trozos que quepan en 52 columnas (menos 3 para « +N»); si alguno se
 * quemaría, solo ese: «D13 amarillo: 145,2 mA (máx. 30 mA): se quemaría».
 * pre va delante («Media: » en P_EMU_ESQ). Devuelve 0 sin LED, 1 con la
 * línea normal y 2 si se quemaría (en C_ERROR). led[i] y off[i]: el LED y
 * la columna de su nombre (para pintarlo en su color). */
#define LED_COLS 52
static int led_linea(char *buf, int cap, const char *pre, const Circuito *c, const double *I,
                     signed char *led, unsigned char *off, int *nled)
{
    char tr[40], u[24];
    Buf b, bt;
    int k, n = 0, fuera = 0, hay = 0;
    *nled = 0;
    buf_init(&b, buf, (size_t)cap);
    buf_str(&b, pre);
    for (k = 0; k < c->nel; k++) {                       /* el que se quemaría, solo */
        double If, Im;
        if (c->el[k].tipo != EL_LED) continue;
        hay = 1;
        led_if_im(&c->el[k], &If, &Im);
        if (I[k] > Im) {
            /* en LED_COLS con el prefijo: si no cabe, sin el color y luego sin el máximo */
            int forma;
            for (forma = 0; forma < 3; forma++) {
                buf_init(&b, buf, (size_t)cap);
                buf_str(&b, pre);
                led[0] = (signed char)k; off[0] = (unsigned char)b.n; *nled = 1;
                buf_str(&b, c->el[k].etiq);
                if (forma == 0) { buf_ch(&b, ' '); buf_str(&b, LED_NOM[c->el[k].variante < LED_N ? c->el[k].variante : 0]); }
                buf_str(&b, ": "); buf_str(&b, num_pref(u, sizeof u, I[k], 4, "A"));
                if (forma < 2) { buf_str(&b, " (m" S_aa "x. "); buf_str(&b, num_pref(u, sizeof u, Im, 3, "A")); buf_ch(&b, ')'); }
                buf_str(&b, ": se quemar" S_ii "a");
                if ((int)b.n <= LED_COLS) break;
            }
            return 2;
        }
    }
    if (!hay) return 0;
    for (k = 0; k < c->nel; k++) {
        double If, Im;
        int pct;
        if (c->el[k].tipo != EL_LED) continue;
        led_if_im(&c->el[k], &If, &Im);
        buf_init(&bt, tr, sizeof tr);
        buf_str(&bt, c->el[k].etiq); buf_ch(&bt, ' '); buf_str(&bt, LED_NOM[c->el[k].variante < LED_N ? c->el[k].variante : 0]);
        if (esq_led_nivel(I[k], If, Im) == 0) buf_str(&bt, " apagado");
        else {
            pct = (int)(100.0 * I[k] / If + 0.5);
            if (pct > 999) pct = 999;
            buf_ch(&bt, ' '); buf_str(&bt, num_pref(u, sizeof u, I[k], 3, "A"));
            buf_ch(&bt, ' '); buf_int(&bt, pct); buf_str(&bt, " %");
        }
        if (fuera || (int)(b.n + (n ? 2 : 0) + bt.n) > LED_COLS - 3) { fuera++; continue; }
        if (n) buf_str(&b, "  ");
        led[*nled] = (signed char)k; off[*nled] = (unsigned char)b.n; (*nled)++;
        buf_str(&b, tr);
        n++;
    }
    if (fuera) { buf_str(&b, " +"); buf_int(&b, fuera); }
    return 1;
}

int ui_led_linea(char *buf, int cap, const char *pre, const Circuito *c, const double *I)
{
    signed char led[MAX_EL];
    unsigned char off[MAX_EL];
    int n;
    return led_linea(buf, cap, pre, c, I, led, off, &n);
}

const Valores *ui_valores(void) { return &sol.s[isol]; }

/* Pinta la línea de los LED en y; cada nombre en el color de su LED */
static void dib_led_linea(int y, const char *pre, const Circuito *c, const double *I)
{
    char t[96];
    signed char led[MAX_EL];
    unsigned char off[MAX_EL];
    int i, n, r = led_linea(t, sizeof t, pre, c, I, led, off, &n);
    if (!r) return;
    gfx_text(6, y, t, r == 2 ? C_ERROR : C_TEXTO, 1);
    if (r == 2) return;
    for (i = 0; i < n; i++) {
        const Elemento *e = &c->el[(int)led[i]];
        gfx_text(6 + 6 * off[i], y, e->etiq, NL_COLOR[e->variante < LED_N ? e->variante : 0].rgb, 1);
    }
}

/* ---- [=] con no lineales, [P] y la opción 9 del menú (§4.1) ---- */
static void punto_op(int desde, int modo_cc)
{
    if (desde == P_EDITOR) cres = circ;
    if (cres.nel == 0) {                     /* como [I]: SIM_VACIO diría «emular» */
        pon_aviso("Nada que calcular: el circuito est" S_aa " vac" S_ii "o.", C_SUAVE);
        return;
    }
    if (circ_busca_valor(&cres)) {
        pon_aviso("El punto de operaci" S_oo "n no despeja: quita los [?!].", C_CURSOR);
        return;
    }
    practica = PRAC_NO;
    red_extraer(&cres, &red);
    memset(&sol, 0, sizeof sol);
    sol.elem = sol.fuente = -1;
    memset(&op_nw, 0, sizeof op_nw);
    sol.estado = sim_punto_op_cc(&cres, &red, &sol.s[0], &op_d, &op_nw) == SIM_OK ? SOL_OK : SOL_SIM;
    sol.nsol = sol.estado == SOL_OK;
    sol.fuente = fuente_unica(&cres);
    res_nl = 1;
    op_modo_cc = modo_cc;
    res_aviso[0] = 0;
    res_titulo[0] = 0;
    pasos.n = 0;
    isol = 0;
    capa = CAPA_VALORES;
    scroll = 0;
    origen = desde;
    pant = P_RESULTADO;
}

/* Un dato de V o I en un circuito no lineal no es una ecuación: se compara
 * en magnitud (1e-6 relativo) */
static int linea_dato(Buf *b, const Elemento *e, int k, int que, const Valores *v)
{
    char u[24];
    double dato = fabs(circ_mag_c(e, que)->v), da = fabs(valor_de(v, k, que));
    int bien = fabs(da - dato) <= 1e-6 * (dato > 1e-300 ? dato : 1e-300);
    nombre_mag(b, e, que);
    buf_str(b, " = ");
    buf_str(b, num_pref(u, sizeof u, dato, bien ? 4 : 3, unidad_de(e, que)));
    buf_str(b, " (dato)");
    if (bien) buf_str(b, " " S_CHECK);
    else { buf_str(b, ": da "); buf_str(b, num_pref(u, sizeof u, da, 4, unidad_de(e, que))); }
    return bien;
}

static void dib_resultado_nl(void)
{
    static SimRes rs;
    Marco m;
    Vista v;
    char t[212], u[24];
    Buf b;
    int y, k, que, ok = sol.estado == SOL_OK;
    const Valores *vv = &sol.s[isol];
    const int y4 = RS_PANEL + 4 + 3 * LIN_H, y5 = y4 + LIN_H;

    barra(cres.nombre, ok ? esq_capa_nombre(capa) : 0);
    esq_vista(&v);
    v.capa = (unsigned char)capa;
    v.red = &red;
    if (ok) {
        /* lo que marca un V o un A en el punto de operación, con su signo
         * (como al emular): en CC, su V o su I; en CA, 0 (no hay alterna);
         * el eficaz total, su valor absoluto */
        static double lect[MAX_EL];
        v.val = vv; v.brillo = brillo_de(&cres, vv->I);
        for (k = 0; k < cres.nel; k++) {
            double x = cres.el[k].tipo == EL_AMP ? vv->I[k] : vv->V[k];
            lect[k] = cres.el[k].variante == MED_CA ? 0.0 : cres.el[k].variante == MED_EF ? fabs(x) : x;
        }
        v.lectura = lect;
    }
    else {
        v.resalta = op_d.lazo;
        if (op_d.elem >= 0 && op_d.elem < 32) v.resalta |= 1u << op_d.elem;
        v.atenuar = 1;
    }
    esq_marco_ajustado(&cres, 0, BARRA_H, SCR_W, RS_PANEL - BARRA_H, &m);
    gfx_clip(0, BARRA_H, SCR_W, RS_PANEL - BARRA_H);
    esq_dibuja(&cres, &m, &v);
    gfx_clip_reset();

    gfx_fill(0, RS_PANEL, SCR_W, PIE_Y - RS_PANEL, C_PANEL);
    separador(RS_PANEL);
    y = RS_PANEL + 4;
    if (!ok) {
        /* sin SimRed: los nombres salen del circuito del punto de operación */
        sim_mensaje_circ(&cres, 0, &op_d, t, sizeof t);
        envuelve(6, y, COLS, t, C_ERROR, 4);
        if (origen == P_EDITOR) pie_error("[esc] volver y corregirlo");
        else pie("[esc] volver");
        return;
    }
    /* 1. el cursor del barrido, los pedidos y los datos de V e I */
    if (res_titulo[0]) { gfx_text(6, y, res_titulo, C_TITULO, 1); y += LIN_H; }
    for (k = 0; k < cres.nel && y < y4 - LIN_H; k++) {
        const Elemento *e = &cres.el[k];
        for (que = 0; que < Q_N && y < y4 - LIN_H; que++) {
            buf_init(&b, t, sizeof t);
            if (pregunta(e, que)) { resultado_de(&b, k, que, 0); gfx_text(6, y, t, C_INCOG, 1); y += LIN_H; }
            else if ((que == Q_V || que == Q_I) && tipo_campo(e->tipo, que) && circ_mag_c(e, que)->est == M_DATO) {
                int bien = linea_dato(&b, e, k, que, vv);
                gfx_text(6, y, t, bien ? C_OK : C_CURSOR, 1);
                y += LIN_H;
            }
        }
    }
    /* 2. la fuente: I y P (sin Req) */
    if (sol.fuente >= 0 && y < y4) {
        int f = sol.fuente;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "I"); buf_igual(&b, fabs(vv->I[f])); buf_str(&b, " A  ");
        buf_str(&b, "P"); buf_igual(&b, fabs(vv->P[f])); buf_str(&b, " W");
        gfx_text(6, y, t, C_TEXTO, 1);
        y += LIN_H;
    }
    /* 3. V(AB), sin Thévenin */
    if (cres.term_x[0] >= 0 && cres.term_x[1] >= 0 && y < y4) {
        double vab;
        int na, nb;
        buf_init(&b, t, sizeof t);
        switch (sol_vab(&cres, &red, vv, &vab, &na, &nb)) {
        case 1:
            buf_str(&b, "V(AB) = V"); buf_ch(&b, red_letra(&red, na));
            buf_str(&b, " - V"); buf_ch(&b, red_letra(&red, nb));
            buf_igual(&b, vab); buf_str(&b, " V");
            if (!circ_no_lineal(&cres) && !res_titulo[0] && (int)b.n + 14 <= COLS) buf_str(&b, "  [T] Th" S_ee "venin");
            break;
        case 0:  buf_str(&b, "A o B no est" S_aa "n en un punto del circuito."); break;
        default: buf_str(&b, "A y B est" S_aa "n en circuitos separados: no hay V(AB)."); break;
        }
        gfx_text(6, y, t, C_TENSION, 1);
    }
    /* 4. Newton */
    buf_init(&b, t, sizeof t);
    if (res_titulo[0]) buf_str(&b, "Barrido de CC: C abiertos, L cables " S_CHECK);
    else {
        buf_str(&b, op_modo_cc ? "CC, C abiertos, L cables: " : "Punto de operaci" S_oo "n: ");
        if (!circ_no_lineal(&cres)) buf_str(&b, "red lineal, sin Newton " S_CHECK);   /* [P] con una red lineal */
        else {
            if (!hay_suave_ui(&cres)) buf_str(&b, "por tramos, ");
            else buf_str(&b, op_nw.homotopia == 1 ? "Newton+gmin, " : op_nw.homotopia == 2 ? "Newton+rampa, " : "Newton, ");
            if (op_nw.iter > 999) buf_str(&b, "999+");
            else buf_int(&b, op_nw.iter);
            buf_str(&b, " iter. " S_CHECK);
        }
    }
    gfx_text(6, y4, t, C_SUAVE, 1);
    /* 5. el aviso de la tecla, los LED o el primer aviso */
    if (res_aviso[0]) gfx_text(6, y5, res_aviso, C_CURSOR, 1);
    else if (brillo_de(&cres, vv->I)) dib_led_linea(y5, "", &cres, vv->I);
    else {
        /* BIESTABLE antes que SATURA (el mismo AO: dice por qué satura) */
        static const unsigned BITS[3] = { SIM_AV_BIESTABLE, SIM_AV_SATURA, SIM_AV_HOMOTOPIA };
        int i;
        memset(&rs, 0, sizeof rs);
        rs.d = op_d;
        rs.op = *vv;                                  /* el raíl del AO (sim_aviso) */
        for (i = 0; i < 3; i++)
            if (op_d.avisos & BITS[i]) {
                sim_aviso(&cres, &rs, BITS[i], t, sizeof t);
                if (strlen(t) > COLS) t[COLS] = 0;
                gfx_text(6, y5, t, C_CURSOR, 1);
                break;
            }
    }
    (void)u;
    pie("[izq/der] capa  [tab] tabla  [I] barrido  [esc]");
}

static void tecla_resultado_nl(int t)
{
    Buf b;
    res_aviso[0] = 0;
    if (t == T_ESC) { a_culpable(); pant = origen; scroll = 0; return; }
    if (sol.estado != SOL_OK) return;
    if (t == T_DER) capa = (capa + 1) % CAPA_N;
    else if (t == T_IZQ) capa = (capa + CAPA_N - 1) % CAPA_N;
    else if (t == T_TAB) { pant = P_TABLA; scroll = 0; }
    else if (t == T_LETRA('I')) {
        if (res_titulo[0]) pant = P_BARRIDO;          /* el cursor: vuelve a su barrido */
        else { res_guarda.hay = 0; barrido_de(P_RESULTADO, &cres); }
    }
    else if (t == T_LETRA('T') && !circ_no_lineal(&cres) && cres.term_x[0] >= 0 && cres.term_x[1] >= 0)
        thevenin_de(P_RESULTADO, &cres, 0.0);         /* una red lineal (con controladas, D22) */
    else if (t == T_ENTER || t == T_LETRA('T')) {
        int k;
        buf_init(&b, res_aviso, sizeof res_aviso);
        if (t == T_LETRA('T')) {
            if (circ_no_lineal(&cres)) { buf_str(&b, "Th" S_ee "venin es de redes lineales: "); buf_str(&b, primer_nl_ui(&cres)); buf_str(&b, " no lo es."); }
            else buf_str(&b, "Pon los terminales A y B con [T] en el editor.");
            return;
        }
        /* [enter]: por qué no hay paso a paso, con la razón cierta */
        for (k = 0; k < cres.nel && cres.el[k].tipo != EL_EDEP && cres.el[k].tipo != EL_JDEP; k++) ;
        if (circ_no_lineal(&cres)) { buf_str(&b, "Sin paso a paso: "); buf_str(&b, primer_nl_ui(&cres)); buf_str(&b, " no es lineal."); }
        else if (k < cres.nel) buf_str(&b, "Sin paso a paso: no lo hay con fuentes controladas.");
        else if (res_titulo[0]) buf_str(&b, "Sin paso a paso en el cursor del barrido.");
        else if (circ_dinamico(&cres) || circ_alterna(&cres)) buf_str(&b, "Sin paso a paso: es de redes resistivas de CC.");
        else buf_str(&b, "Sin paso a paso aqu" S_ii ": [=] en el editor lo da.");
    }
}

/* ---- P_TABLA con el punto de operación: la región y el segundo puerto ---- */
static void dib_tabla_nl(void)
{
    const Valores *v = &sol.s[isol];
    char t[80], u[24];
    Buf b;
    int k, fila, y = BARRA_H + 20, filas_vis = (PIE_Y - y) / 14, n3 = 0, nCL = 0, ntot;
    int tres[MAX_EL], cl[MAX_EL];
    double ent, ab;
    sol_balance(&cres, v, &ent, &ab);                    /* la escala del ruido de un cero */
    for (k = 0; k < cres.nel; k++) {
        int tp = cres.el[k].tipo;
        if (circ_npatas(&cres.el[k]) == 3) tres[n3++] = k;
        if ((tp == EL_COND || tp == EL_BOB) && cres.el[k].val.est == M_DATO) cl[nCL++] = k;   /* como dib_tabla */
    }
    ntot = cres.nel + nCL + n3 + 1;
    barra("Tabla de valores", 0);
    gfx_text(6, BARRA_H + 4, "Elem", C_SUAVE, 1);
    gfx_text(40, BARRA_H + 4, "Valor/reg.", C_SUAVE, 1);      /* 10 columnas: con aire hasta la V («Valor/región», 12, la tocaba) */
    gfx_text(114, BARRA_H + 4, "V", C_SUAVE, 1);
    gfx_text(184, BARRA_H + 4, "I", C_SUAVE, 1);
    gfx_text(254, BARRA_H + 4, "P abs", C_SUAVE, 1);    /* como dib_tabla: absorbida, negativa si entrega */
    separador(BARRA_H + 17);
    if (scroll > ntot - filas_vis) scroll = ntot - filas_vis;
    if (scroll < 0) scroll = 0;
    for (fila = scroll; fila < ntot && fila < scroll + filas_vis; fila++, y += 14) {
        const Elemento *e;
        if (fila == ntot - 1) {                          /* Newton y Kirchhoff */
            int bien = op_nw.residuo_max <= 1e-9;
            buf_init(&b, t, sizeof t);
            if (res_titulo[0]) buf_str(&b, "Del barrido de CC");
            else if (!circ_no_lineal(&cres)) buf_str(&b, "Red lineal, sin Newton");
            else { buf_str(&b, "Newton: "); buf_int(&b, op_nw.iter); buf_str(&b, " iteraciones"); }
            buf_str(&b, " " S_CDOT " Kirchhoff ");
            if (!(op_nw.residuo_max > 0.0)) buf_ch(&b, '0');
            else {
                int ex = (int)floor(log10(op_nw.residuo_max));
                buf_str(&b, num_auto(u, sizeof u, op_nw.residuo_max / pow(10.0, (double)ex), 1));
                buf_ch(&b, 'E'); buf_int(&b, ex);
            }
            if (bien) buf_str(&b, " " S_CHECK);
            gfx_text(6, y, t, bien ? C_SUAVE : C_ERROR, 1);
            continue;
        }
        if (fila >= cres.nel && fila < cres.nel + nCL) { /* carga o flujo, y energía (§4.5) */
            double x, W;
            k = cl[fila - cres.nel];
            e = &cres.el[k];
            gfx_text(6, y, e->etiq, tipo_info(e->tipo)->color, 1);
            if (e->tipo == EL_COND) {
                x = e->val.v * v->V[k]; W = 0.5 * e->val.v * v->V[k] * v->V[k];
                gfx_text(40, y, "Q", C_SUAVE, 1); celda(52, y, x, "C", C_TEXTO);
            } else {
                x = e->val.v * v->I[k]; W = 0.5 * e->val.v * v->I[k] * v->I[k];
                gfx_text(40, y, S_PHI, C_SUAVE, 1); celda(52, y, x, "Wb", C_TEXTO);
            }
            gfx_text(172, y, "W", C_SUAVE, 1); celda(184, y, W, "J", C_POTENCIA);
            continue;
        }
        if (fila >= cres.nel) {                          /* el segundo puerto de un 3 patas */
            k = tres[fila - cres.nel - nCL];
            e = &cres.el[k];
            buf_init(&b, t, sizeof t);
            buf_str(&b, e->etiq);
            buf_str(&b, e->tipo == EL_AO ? "  Vd " : e->tipo == EL_MOS ? "  VGS " : "  VBE ");
            buf_str(&b, num_pref(u, sizeof u, v->V2[k], 4, "V"));
            buf_str(&b, e->tipo == EL_AO ? "  Ient " : e->tipo == EL_MOS ? "  IG " : "  IB ");
            buf_str(&b, num_pref(u, sizeof u, v->I2[k], 4, "A"));
            gfx_text(6, y, t, C_TEXTO, 1);
            continue;
        }
        k = fila;
        e = &cres.el[k];
        gfx_text(6, y, e->etiq, tipo_info(e->tipo)->color, 1);
        if (e->tipo == EL_VATI) {                        /* lo que marca, como en dib_tabla */
            fila_vatimetro(y, v->V[k], v->I[k], v->P[k]);
            continue;
        }
        if (es_nl_ui(e->tipo)) {
            int r = v->reg[k] < REG_N ? v->reg[k] : REG_NADA;
            gfx_text(40, y, REG_NOM[r], reg_color(r), 1);
        } else if (e->tipo == EL_INTER) gfx_text(40, y, e->variante == INTER_CERRADO ? "cerrado" : "abierto", C_SUAVE, 1);
        else if (ES_IDEAL(e)) gfx_text(40, y, e->tipo == EL_VOLT ? "ideal (" S_INF ")" : "ideal (0)", C_SUAVE, 1);
        else if (e->tipo == EL_COND || e->tipo == EL_BOB) gfx_text(40, y, e->tipo == EL_COND ? "abierto" : "cable", C_SUAVE, 1);
        else celda(40, y, v->val[k], unidad_q(e, Q_VAL), C_TEXTO);
        celda_s(114, y, v->V[k], "V", C_TENSION);
        celda_s(184, y, v->I[k], "A", C_CORRIENTE);
        {                                                /* la absorbida, como en dib_tabla */
            double p = sol_p_abs(&cres, v, k);
            celda_s(254, y, fabs(p) <= 1e-12 * (ent + ab) ? 0.0 : p, "W", C_POTENCIA);
        }
    }
    pie("[arriba/abajo] desplazar   [esc] volver");
}

/* ---- El barrido de CC: por trozos en P_EMULANDO y a la vista en P_BARRIDO ---- */
static void empieza_barrido(void)
{
    trabajo = TR_BARRIDO;
    /* un primer trozo en el acto: un barrido corto no pasa por la barra */
    if (!barr_avanzar(SIM_OPS_TICK)) fin_barrido();
    else pant = P_EMULANDO;
}

static void barrido_de(int desde, const Circuito *c)
{
    int k;
    if (!rebarre) memset(&curv, 0, sizeof curv);   /* una entrada nueva: la pantalla desde cero */
    barr_c0 = *c;
    red_extraer(&barr_c0, &barr_red);
    barr_origen = desde;
    barr_curva_k = -1;
    /* el [?!] de un elemento que se barre: se barre él (§3.J; barrer otro lo
     * dejaría sin valor) */
    for (k = 0; k < barr_c0.nel && !(barrible_ui(barr_c0.el[k].tipo) && barr_c0.el[k].val.est == M_BUSCAR); k++) ;
    /* el elemento y el rango de la sesión: si son de este circuito (un modelo
     * u otro circuito no los heredan), el elemento sigue siendo barrible y no
     * hay un [?!] en otro */
    if (barr_aj_firma != firma_circ(&barr_c0) || barr_aj.el >= barr_c0.nel ||
        (barr_aj.el >= 0 && !barrible_ui(barr_c0.el[(int)barr_aj.el].tipo)) ||
        (k < barr_c0.nel && barr_aj.el >= 0 && barr_aj.el != k)) {
        memset(&barr_aj, 0, sizeof barr_aj);
        barr_aj.el = -1;
    }
    if (barr_iniciar(&barr_c0, &barr_red, &barr_aj) != SIM_OK) {
        if (barr_res()->d.estado == SIM_NADA_BARRER && desde == P_EDITOR) {
            pon_aviso("Nada que barrer: pon una E, una J o una R.", C_CURSOR);
            return;
        }
        pant = P_BARRIDO;                                 /* el error, con su texto */
        return;
    }
    empieza_barrido();
}

/* El trazador de curvas: el componente k solo (§4.10) */
static void curva_de(int desde, const Circuito *c, int k)
{
    memset(&curv, 0, sizeof curv);
    barr_c0 = *c;
    barr_origen = desde;
    barr_curva_k = k;
    if (barr_iniciar_curva(&barr_c0, k) != SIM_OK) { pant = P_BARRIDO; return; }
    empieza_barrido();
}

static int barr_modelo = -1;          /* el modelo de ca = 4 que se barre (su medida 'x') */

static void fin_barrido(void)
{
    if (barr_res()->fin != FIN_ERROR) {
        cv_nuevo(&curv, barr_reg(), barr_res());
        /* un modelo que pregunta un x (el 24: ¿qué E da 1 mA?) sin un dato que
         * lo diga: el cursor, en la respuesta */
        if (barr_modelo >= 0 && MODELOS[barr_modelo].medida == 'x' && barr_res()->tr_dato < 0)
            cv_ir_nivel(&curv, barr_reg(), barr_res(), 0, MODELOS[barr_modelo].t, 1);
    }
    barr_modelo = -1;
    pant = P_BARRIDO;
}

/* [enter] en un modelo de ca = 4: el barrido con su elemento y su rango */
static void barre_modelo(int m)
{
    const CircModelo *md = &MODELOS[m];
    int k;
    memset(&barr_aj, 0, sizeof barr_aj);
    barr_aj.el = -1;
    for (k = 0; md->barre && k < cres.nel; k++)
        if (strcmp(cres.el[k].etiq, md->barre) == 0) {
            barr_aj.el = (signed char)k;
            barr_aj.x0 = md->x0; barr_aj.x1 = md->x1;
            barr_aj.log_x = md->log_x;
        }
    barr_aj_firma = firma_circ(&cres);
    barr_modelo = m;
    barrido_de(P_EJEMPLOS, &cres);
    if (pant != P_EMULANDO && pant != P_BARRIDO) barr_modelo = -1;
}

static void dib_barrido(void)
{
    if (barr_res()->fin == FIN_ERROR) {
        char t[212];
        barra(barr_curva_k >= 0 ? "Curva I-V" : "Barrido de CC", 0);
        sim_mensaje_circ(&barr_c0, 0, &barr_res()->d, t, sizeof t);
        envuelve(6, BARRA_H + 8, COLS, t, C_ERROR, 5);
        pie("[esc] volver");
        return;
    }
    cv_dibuja(&curv, barr_reg(), barr_res());
}

/* [tab] en P_BARRIDO: P_RESULTADO con los valores del cursor (§4.5) */
static void circuito_en_cursor(void)
{
    static Valores vc;                             /* no en sol: la P_RESULTADO de partida se guarda antes */
    const BarrRes *r = barr_res();
    const Circuito *bc;
    SimDiag d;
    Buf b;
    double x = curv.xc[curv.cur];
    int tr = curv.activo;
    if (barr_curva_k >= 0 || barr_valores(tr, x, &vc, &d) != SIM_OK || !(bc = barr_circuito())) {
        buf_init(&b, curv.aviso, sizeof curv.aviso);
        buf_str(&b, barr_curva_k >= 0 ? "La curva I-V es del componente solo: no hay circuito."
                                      : "Ah" S_ii " Newton no converge: mueve el cursor.");
        curv.aviso_col = C_CURSOR;
        return;
    }
    if (barr_origen == P_RESULTADO && !res_guarda.hay) {
        /* la P_RESULTADO de la que salió el barrido: [esc] en P_BARRIDO vuelve a ella */
        res_guarda.hay = 1;
        res_guarda.c = cres; res_guarda.s = sol; res_guarda.d = op_d; res_guarda.nw = op_nw;
        res_guarda.res_nl = res_nl; res_guarda.op_modo_cc = op_modo_cc; res_guarda.origen = origen;
        res_guarda.isol = isol; res_guarda.capa = capa; res_guarda.practica = practica;
        memcpy(res_guarda.titulo, res_titulo, sizeof res_guarda.titulo);
    }
    cres = *bc;
    red_extraer(&cres, &red);
    sol.s[0] = vc;
    sol.estado = SOL_OK;
    sol.nsol = 1;
    sol.elem = -1;
    sol.fuente = fuente_unica(&cres);
    sol.hay_req = 0;
    op_d = d;
    memset(&op_nw, 0, sizeof op_nw);
    op_nw.residuo_max = barr_residuo();            /* el de estos valores (no el peor del barrido) */
    res_nl = 1;
    op_modo_cc = 1;
    res_aviso[0] = 0;
    buf_init(&b, res_titulo, sizeof res_titulo);
    buf_str(&b, "En el cursor: "); buf_str(&b, r->nom_x); buf_str(&b, " = ");
    x_con_unidad(&b, x);
    practica = PRAC_NO;
    pasos.n = 0;
    isol = 0;
    capa = CAPA_VALORES;
    scroll = 0;
    origen = P_BARRIDO;
    pant = P_RESULTADO;
}

/* [esc] en P_BARRIDO: a donde se vino; si era una P_RESULTADO que el cursor
 * pisó, la de antes */
static void sal_barrido(void)
{
    if (barr_origen == P_RESULTADO && res_guarda.hay) {
        cres = res_guarda.c;
        red_extraer(&cres, &red);
        sol = res_guarda.s; op_d = res_guarda.d; op_nw = res_guarda.nw;
        res_nl = res_guarda.res_nl; op_modo_cc = res_guarda.op_modo_cc; origen = res_guarda.origen;
        isol = res_guarda.isol; capa = res_guarda.capa; practica = res_guarda.practica;
        memcpy(res_titulo, res_guarda.titulo, sizeof res_titulo);
        res_aviso[0] = 0;
        pasos.n = 0;
        scroll = 0;
        res_guarda.hay = 0;
    }
    pant = barr_origen;
}

static int tecla_barrido(int t)
{
    if (barr_res()->fin == FIN_ERROR) { if (t == T_ESC) sal_barrido(); return 1; }
    switch (cv_tecla(&curv, barr_reg(), barr_res(), t | mods)) {
    case CV_SALIR: sal_barrido(); break;
    case CV_BARRER:                                /* la ventana que se ve, con 281 puntos */
        if (barr_curva_k >= 0) { curva_de(barr_origen, &barr_c0, barr_curva_k); break; }
        {
            const BarrRes *r = barr_res();
            BarrAjustes a = barr_aj;
            unsigned fa = barr_aj_firma;
            barr_aj_firma = firma_circ(&barr_c0);
            barr_aj.el = (signed char)r->el;
            barr_aj.log_x = r->log_x;
            barr_aj.vuelta = r->vuelta;
            cv_ventana(&curv, &barr_aj.x0, &barr_aj.x1);
            rebarre = 1;
            barrido_de(barr_origen, &barr_c0);
            rebarre = 0;
            barr_aj = a;
            barr_aj_firma = fa;
        }
        break;
    case CV_AJUSTES:
        if (barr_curva_k >= 0) {
            Buf b;
            buf_init(&b, curv.aviso, sizeof curv.aviso);
            buf_str(&b, "La curva I-V no tiene ajustes: su rango es fijo.");
            curv.aviso_col = C_CURSOR;
        } else abre_ajustes(P_BARRIDO);
        break;
    case CV_CIRCUITO: circuito_en_cursor(); break;
    default: break;
    }
    return 1;
}

const SimRes *ui_simres(void) { return sim_res(); }
const Osc *ui_osc(void) { return &osc; }

/* ----------------------------------------------------------------------- */
/* ¿El dibujo de la pantalla la borra él mismo? Empiezan con su gfx_clear
 * osc_dibuja (prepara solo calcula), traz_dibuja y cv_dibuja (sin error) y
 * fz_dibuja (y las de la versión especial). Esas no se borran dos veces */
static int se_borra_sola(void)
{
    switch (pant) {
    case P_OSC: case P_FASORIAL: ESP_SE_BORRAN return 1;
    case P_BODE:    return bode_res()->fin != FIN_ERROR;
    case P_BARRIDO: return barr_res()->fin != FIN_ERROR;
    default:        return 0;
    }
}

void ui_dibuja(void)
{
    if (!se_borra_sola()) gfx_clear(C_FONDO);
    /* el convenio de los rótulos de fasores: el del circuito que se ve
     * (dib_ca, dib_thevenin y las miniaturas ponen el suyo) */
    esq_convenio(pant == P_EDITOR || pant == P_ELEMENTO || pant == P_MODELO ? circ.conv : cres.conv);
    switch (pant) {
        case P_INICIO:    dib_inicio();    break;
        case P_MENU:      dib_menu();      break;
        case P_TALLER:    dib_taller();    break;
        case P_EDITOR:    dib_editor();    break;
        case P_ELEMENTO:  dib_elemento();  break;
        case P_MIS:       dib_mis();       break;
        case P_LECCIONES: dib_lecciones(); break;
        case P_TEORIA:    dib_teoria();    break;
        case P_EJEMPLOS:  dib_ejemplos();  break;
        case P_AYUDA:     dib_ayuda();     break;
        case P_RESULTADO: dib_resultado(); break;
        case P_TABLA:     dib_tabla();     break;
        ESP_DIBUJA
        case P_BANCO:     banco_dibuja();    break;
        case P_MODELO:    dib_modelo();      break;
        case P_EMULANDO:  dib_emulando();    break;
        case P_OSC:       osc_dibuja(&osc, sim_reg(), sim_res()); break;
        case P_CA:        dib_ca(); break;
        case P_BODE:      dib_bode(); break;
        case P_FASORIAL:  dib_fasorial(); break;
        case P_CA_TABLA:  dib_ca_tabla(); break;
        case P_THEVENIN:  dib_thevenin(); break;
        case P_BARRIDO:   dib_barrido(); break;
        case P_EMU_TABLA: dib_emu_tabla();   break;
        case P_EMU_ESQ:   dib_emu_esq();     break;
        case P_AJUSTES:   dib_ajustes();     break;
        case P_CONTROLES: dib_controles();   break;
        case P_CPM:       dib_cpm();         break;
        case P_CPM_RES:   dib_cpm_res();     break;
        case P_CPM_DIB:   dib_cpm_dib();     break;
        case P_CPM_MENU:  dib_cpm_menu();    break;
        case P_CPM_TEO:   dib_cpm_teo();     break;
        case P_CPM_PAG:   dib_cpm_pag();     break;
        case P_CPM_EJ:    dib_cpm_ej();      break;
        default:          dib_pasos();     break;
    }
    marcas(1);
}

/* Lo que se ve y no sale de una tecla ni de un tick: los segundos que le
 * quedan al grabador. El bucle repinta cuando cambia (una vez por segundo
 * grabando, y al acabar), no 30 veces por segundo: el grabador mide la capa
 * en cualquier pantalla, y un cuadro caro por tick le quitaría muestras. Y
 * el turbo: si el SO cambió el reloj (el USB), se apaga y su marca se va */
int ui_rotulo(void)
{
    int ms, g = ent_grabacion(0, &ms);
    turbo_vigila();
    return (g == GRAB_GRABANDO ? GRAB_ERROR + 1 + (ms + 999) / 1000 : g)   /* distinto de cada GRAB_* */
           + 1000 * turbo_nivel();
}

/* El banco solo anima mientras mide: al acabar se queda quieto. Emulando, o
 * con la prueba del turbo en marcha, hay trabajo: no se duerme entre ticks. */
int ui_animando(void)
{
    if (pant == P_EMULANDO || (pant == P_CONTROLES && tb_probando())) return UI_TRABAJANDO;
    return ESP_ANIMA
           (pant == P_BANCO && !banco_hecho()) ||
           (pant == P_CONTROLES && ent_grabacion(0, 0) == GRAB_GRABANDO);   /* la cuenta atrás */
}

void ui_tick(void)
{
    ESP_TICK
    if (pant == P_BANCO) banco_tick();
    else if (pant == P_CONTROLES) tb_tick();              /* la prueba del turbo */
    else if (pant == P_EMULANDO && trabajo == TR_BODE) { if (!bode_avanzar(ops_tick)) fin_bode(); }
    else if (pant == P_EMULANDO && trabajo == TR_CALCA) { if (!cal_avanzar(ops_tick)) fin_calca(); }
    else if (pant == P_EMULANDO && trabajo == TR_BARRIDO) { if (!barr_avanzar(ops_tick)) fin_barrido(); }
    else if (pant == P_EMULANDO) { if (!sim_avanzar(ops_tick)) fin_emulacion(); }
}

/* --------------------------------------------------------------------------
 *  Volver de un error al elemento que lo causa
 * ----------------------------------------------------------------------- */
/* sim_valida sobre circ (índices del circuito, como la línea de estado): el
 * elemento al que le falta algo, o -1 */
static int culpa_sim(int *q, int *falta)
{
    SimDiag d;
    if (sim_valida(&circ, &d) == SIM_OK || d.elem < 0 || d.elem >= circ.nel) return -1;
    if (d.estado == SIM_FALTA) { *q = d.campo; *falta = 1; }
    else if (d.estado == SIM_MODELO) *q = d.campo;
    else if (d.estado == SIM_CTL_MAL) *q = Q_CTRL;
    return d.elem;
}

/* El culpable del error que se ve (P_RESULTADO, P_EMU_ESQ o P_CA venidos del
 * editor): su índice en circ, o -1; en *q la fila de su ficha (Q_*) que
 * falla, o -1, y en *falta 1 si es un dato que falta. No se toma el índice de
 * sim_res() ni de barr_res() (pueden ser de otra red): se recalcula sobre circ
 * con la regla de la línea de estado (sim_valida, ca_valida, cal_plantea). La
 * calculadora de CC da sol.elem, que es de cres: la copia de circ que se
 * resolvió (el editor no la cambió mientras tanto) */
static int culpable(int *q, int *falta)
{
    *q = -1; *falta = 0;
    if (circ.nel == 0) return -1;
    if (pant == P_RESULTADO && origen == P_EDITOR && sol.estado != SOL_OK) {
        int k = sol.elem;
        if (res_nl) return culpa_sim(q, falta);           /* el punto de operación */
        /* con un no lineal y un [?!], el camino es [I], no una ficha */
        if (sol.estado == SOL_NO_LINEAL || k < 0 || k >= circ.nel || k >= cres.nel ||
            strcmp(circ.el[k].etiq, cres.el[k].etiq) != 0) return -1;
        if (sol.estado == SOL_CTL) *q = Q_CTRL;
        else if (sol.estado == SOL_R_INVALIDA || sol.estado == SOL_R_CERO ||
                 sol.estado == SOL_NO_FISICA || sol.estado == SOL_SIN_CORRIENTE) *q = Q_VAL;
        return k;
    }
    if (pant == P_EMU_ESQ && emu_origen == P_EDITOR &&
        (sim_res()->fin == FIN_ERROR || sim_res()->d.estado != SIM_OK)) return culpa_sim(q, falta);
    if (pant == P_CA && ca_origen == P_EDITOR && (ca_r.d.estado != SIM_OK || ca_sin_sol())) {
        if (ca_calc) {
            static CalRes pl;
            int st = cal_plantea(&circ, &pl), k;
            if (st == CAL_SIM && pl.d.estado == SIM_CA_SIN_F) {
                /* sin f (cal_plantea no dice de quién): la fila f de la primera
                 * fuente alterna, como ca_valida sin [?!]; sin ninguna, la f va
                 * en [menu] > Ajustes de CA y no hay ficha */
                for (k = 0; k < circ.nel; k++)
                    if ((circ.el[k].tipo == EL_FUENTE || circ.el[k].tipo == EL_CORR) &&
                        circ.el[k].variante == FORMA_FASOR) { *q = Q_FCIRC; *falta = 1; return k; }
                return -1;
            }
            if (st == CAL_SIM) {
                if (pl.d.elem < 0 || pl.d.elem >= circ.nel) return -1;
                if (pl.d.estado == SIM_FALTA) { *q = pl.d.campo; *falta = 1; }
                return pl.d.elem;
            }
            if (st == CAL_NO_CA && pl.elem >= 0 && pl.elem < circ.nel) {
                /* lo que en CA no se despeja (cal_plantea): la r de una fuente, la
                 * R de un instrumento o la ganancia de una controlada; un
                 * generador que fija la f, su ficha sin más ([izq/der] la forma) */
                const Elemento *e = &circ.el[pl.elem];
                if (e->tipo == EL_FUENTE && e->aux.est == M_BUSCAR) *q = Q_AUX;
                else if ((e->tipo == EL_VOLT || e->tipo == EL_AMP || e->tipo == EL_EDEP || e->tipo == EL_JDEP) &&
                         e->val.est == M_BUSCAR) *q = Q_VAL;
                return pl.elem;
            }
            return -1;
        } else {
            SimDiag d;
            double f;
            if (ca_valida(&circ, &d, &f) == SIM_OK || d.elem < 0 || d.elem >= circ.nel) return -1;
            if (d.estado == SIM_FALTA) { *q = d.campo; *falta = 1; }
            return d.elem;
        }
    }
    return -1;
}

/* El pie de un error venido del editor: con culpable, adónde irá el cursor */
static void pie_error(const char *sin)
{
    int q, falta, k = culpable(&q, &falta);
    char t[56];
    Buf b;
    if (k < 0) { pie(sin); return; }
    buf_init(&b, t, sizeof t);
    buf_str(&b, "[esc] al editor: el cursor ir" S_aa " a ");
    buf_str(&b, circ.el[k].etiq);
    pie(t);
}

/* [esc] en un error venido del editor: el cursor va al borne a del culpable,
 * elegido, y [enter] abre su ficha en la fila que falla. «C1: falta C. [enter]
 * abre su ficha.»; sin culpable, nada cambia */
static void a_culpable(void)
{
    int q, falta, k = culpable(&q, &falta);
    char t[64];
    Buf b;
    if (k < 0) return;
    sel_el = k;
    accion = ACC_NADA;
    cur_x = circ.el[k].xa; cur_y = circ.el[k].ya;
    elegido = 1;
    elegido_q = q;
    buf_init(&b, t, sizeof t);
    if (falta) {
        const CampoInfo *ci = tipo_campo_v(circ.el[k].tipo, circ.el[k].variante, q);
        buf_str(&b, circ.el[k].etiq); buf_str(&b, ": falta ");
        /* 32 columnas son lo fijo: si el nombre de la fila no cabe, «un dato» */
        buf_str(&b, ci && strlen(ci->nombre) + strlen(circ.el[k].etiq) + 32 <= COLS ? ci->nombre : "un dato");
        buf_str(&b, ". [enter] abre su ficha.");
    } else {
        buf_str(&b, "El error est" S_aa " en "); buf_str(&b, circ.el[k].etiq);
        buf_str(&b, ": [enter] abre su ficha.");
    }
    pon_aviso(t, C_CURSOR);
}

/* --------------------------------------------------------------------------
 *  Teclas
 * ----------------------------------------------------------------------- */
static int dir_de(int t)
{
    switch (t) {
        case T_DER:    return DIR_DER;
        case T_ABAJO:  return DIR_ABA;
        case T_IZQ:    return DIR_IZQ;
        case T_ARRIBA: return DIR_ARR;
        default:       return -1;
    }
}

static void abre_ayuda(void)
{
    volver_ayuda = pant;
    scroll = 0;
    pant = P_AYUDA;
}

/* guardar = 1 al editar un elemento que ya estaba: deshacer vuelve a los
 * valores de antes (al salir de la ficha, y solo si cambió algo: sal_ficha).
 * Al colocar uno nuevo ya se guardó el circuito sin él. */
static void abre_formulario(int k, int guardar)
{
    sal_ficha();                          /* la de antes, si quedaba alguna */
    if (guardar) { ficha_antes = circ; ficha_pend = 1; }
    sel_el = k;
    campo = 0;
    sub = 0;
    editando = 0;
    aviso_r = 0;
    aviso_nb = 0;
    aviso_nb_mal = 0;
    edit[0] = 0;
    pant = P_ELEMENTO;
}

/* Al traer un ejemplo o un modelo con [tab] se ve su enunciado (o lo que hay
 * que ver); si en el editor había algo, debajo, en la línea de estado y
 * durante una tecla, esto (49 columnas). Si el enunciado ocupa las dos
 * líneas, esto sale con la tecla siguiente */
#define TRAIDO "Tra" S_ii "do al Taller: [ctrl][Z] vuelve al que ten" S_ii "as."

/* ¿Cabe s en una línea del editor? */
static int una_linea(const char *s)
{
    char lin[64];
    return *primera_linea(s, COLS, lin) == 0;
}

/* Traído al Taller con algo antes: TRAIDO debajo del enunciado; si el
 * enunciado ocupa las dos líneas, tras la tecla siguiente (ver tecla()) */
static void avisa_traido(void)
{
    if (una_linea(aviso)) aviso_bajo = TRAIDO;
    else aviso_luego = TRAIDO;
}

/* Empieza un circuito vacío: Problema nuevo, o antes de abrir uno de Mis
 * problemas o de traer un ejemplo o un modelo. El de antes no se pierde: si
 * tenía algo, va al historial (con su hueco) y [ctrl][Z] lo recupera (el
 * rango del Bode y el del barrido no vuelven con él: son de la sesión y se
 * eligen otra vez). 1 si había algo que recuperar */
static int nuevo_circuito(void)
{
    int habia = circ.nel > 0;
    sal_ficha();
    if (habia) guarda_hist();
    nreh = 0;                             /* lo deshecho del de antes ya no se rehace (vacío,
                                           * guarda_hist no lo vació: [ctrl][Y] lo traería) */
    doc_act = ++doc_n;                    /* otro documento */
    memset(&bode_aj, 0, sizeof bode_aj);
    bode_aj.ent = -1;
    memset(&barr_aj, 0, sizeof barr_aj);
    barr_aj.el = -1;
    circ_init(&circ, "Problema nuevo");
    hueco = -1;
    cur_x = 1; cur_y = 1; sel_el = -1; accion = ACC_NADA;
    pegando = 0;                          /* el portapapeles sigue: se pega en el nuevo */
    if (nombrando == NOM_EDITOR) nombrando = NOM_NO;
    marca_x = marca_y = -1;
    elegido = 0; elegido_q = -1;
    ses_aviso = 0;
    aviso[0] = 0;
    aviso_bajo = aviso_luego = 0;
    ctx_abierto = 0;
    return habia;
}

/* ¿Un nombre que puso el programa? Al guardar en un hueco nuevo se cambia
 * por el suyo; el de un ejemplo o un modelo traído al Taller se conserva.
 * El programa solo pone «Problema nuevo» a uno sin hueco (h < 0) y «Mi
 * problema N» al del hueco N - 1: un «Mi problema 9» en el hueco 0, o un
 * «Problema nuevo» guardado, los escribió el usuario (con [ctrl][N] o en
 * Mis problemas) y se respetan. Sin hueco, «Mi problema N» es del programa
 * (un problema cuyo archivo se borró) salvo que se acabe de escribir: eso
 * lo dice guardar (propio) */
static int nombre_de_omision(const char *s, int h)
{
    static const char mi[] = "Mi problema ";
    const char *p, *cifras;
    int n = 0;
    if (!s[0]) return 1;
    if (strcmp(s, "Problema nuevo") == 0) return h < 0;
    if (strncmp(s, mi, sizeof mi - 1) != 0) return 0;
    for (p = cifras = s + sizeof mi - 1; *p >= '0' && *p <= '9' && n < 1000; p++) n = n * 10 + (*p - '0');
    if (p == cifras || *p != 0 || cifras[0] == '0') return 0;
    return h < 0 || n == h + 1;
}

/* La copia de un problema con nombre propio: «Hallar la corriente (2)», y la
 * de esa, «(3)» (con 23 caracteres no cabe «(copia)»); si no cabe, se recorta
 * el nombre */
static void nombre_copia(char *nom)
{
    char suf[8];
    Buf b;
    size_t n = strlen(nom), m, i, j;
    int num = 2;
    if (n >= 4 && nom[n - 1] == ')') {                    /* ya acaba en « (N)», N de 1 a 3 cifras */
        for (i = n - 1; i > 0 && nom[i - 1] >= '0' && nom[i - 1] <= '9'; i--) {}
        if (i >= 3 && i < n - 1 && i + 4 >= n && nom[i - 1] == '(' && nom[i - 2] == ' ') {
            for (num = 0, j = i; j < n - 1; j++) num = num * 10 + (nom[j] - '0');
            num++;
            n = i - 2;
        }
    }
    buf_init(&b, suf, sizeof suf);
    buf_str(&b, " ("); buf_int(&b, num); buf_ch(&b, ')');
    m = strlen(suf);
    if (n + m > 23) n = 23 - m;
    while (n > 0 && nom[n - 1] == ' ') n--;
    memcpy(nom + n, suf, m + 1);
}

/* [ctrl][S] y [menu] > Guardar (copia = 0) o Guardar como copia (1): en su
 * hueco, o en uno nuevo la primera vez y con la copia (que desde entonces es
 * el problema que se edita, como «Guardar como»). Si no se puede escribir,
 * todo queda como estaba: ni hueco ni nombre nuevos, y un hueco nuevo no se
 * queda con medio archivo. propio: el nombre lo acaba de escribir el
 * usuario ([ctrl][N]) y se guarda tal cual, aunque parezca del programa */
static int guardar(int copia, int propio)
{
    Buf b;
    char t[64], antes[sizeof circ.nombre];
    int h = hueco, nuevo;
    if (circ.nel == 0) { pon_aviso("El circuito está vacío: nada que guardar.", C_ERROR); return 0; }
    if (hueco < 0) copia = 0;             /* sin original, una copia es guardar sin más */
    nuevo = hueco < 0 || copia;
    memcpy(antes, circ.nombre, sizeof antes);
    if (nuevo) {
        h = arch_libre();
        if (h < 0) { pon_aviso("No quedan huecos (máximo 20).", C_ERROR); return 0; }
        if (!propio && nombre_de_omision(circ.nombre, hueco)) {
            buf_init(&b, circ.nombre, sizeof circ.nombre);
            buf_str(&b, "Mi problema ");
            buf_int(&b, h + 1);
        } else if (copia) nombre_copia(circ.nombre);
    }
    if (!arch_guardar(h, &circ)) {
        if (nuevo) arch_borrar(h);
        memcpy(circ.nombre, antes, sizeof antes);
        pon_aviso("No se pudo guardar el archivo.", C_ERROR);
        return 0;
    }
    if (nuevo) { hueco = h; doc_renombra(); }
    buf_init(&b, t, sizeof t);
    buf_str(&b, copia ? "Copia guardada: " : "Guardado: ");
    buf_str(&b, circ.nombre);
    /* con el nombre que puso el programa (14 columnas como mucho): cómo se
     * le pone uno («Guardado: Mi problema 12. [ctrl][N] le pone nombre.», 50) */
    if (!copia && !propio && nombre_de_omision(circ.nombre, h)) buf_str(&b, ". [ctrl][N] le pone nombre.");
    pon_aviso(t, C_OK);
    return 1;
}

/* --------------------------------------------------------------------------
 *  Poner nombre: Mis problemas > [N] (el elegido) y [ctrl][N] en el editor
 *  (le pone nombre y lo guarda). Hasta 23 caracteres: letras, cifras,
 *  espacio y . - + ( ) /
 * ----------------------------------------------------------------------- */
/* Lo que escribe la tecla t en un nombre, o 0. Las letras en minúscula; con
 * [shift], y la primera del nombre, en mayúscula */
static char nom_char(int t)
{
    if (mods & T_CON_CTRL) return 0;              /* [ctrl][S], [ctrl][Z]...: no escriben */
    if (t >= T_LETRA('A') && t <= T_LETRA('Z')) {
        char c = (char)('a' + (t - T_LETRA0));
        return (mods & T_CON_SHIFT) || !nom_edit[0] ? (char)(c - 'a' + 'A') : c;
    }
    if (t >= T_DIG(0) && t <= T_DIG(9)) return (char)('0' + (t - T_DIG0));
    switch (t) {
    case T_ESPACIO: return nom_edit[0] ? ' ' : 0;  /* no al principio */
    case T_PUNTO:   return '.';
    case T_MENOS: case T_SIGNO: return '-';
    case T_MAS:     return '+';
    case T_PAR_A:   return '(';
    case T_PAR_C:   return ')';
    case T_ENTRE:   return '/';
    default:        return 0;
    }
}

/* Una tecla que escribe o borra en el nombre ([ctrl][del]: todo). 1 si lo era */
static int nom_tecla(int t)
{
    size_t n = strlen(nom_edit);
    char c;
    if (t == T_BORRAR) {
        if (mods & T_CON_CTRL) nom_edit[0] = 0;
        else if (n > 0) nom_edit[n - 1] = 0;
        nom_aviso = 0;
        return 1;
    }
    if (!(c = nom_char(t))) return 0;
    nom_aviso = 0;
    if (n + 1 < sizeof nom_edit) { nom_edit[n] = c; nom_edit[n + 1] = 0; }
    else nom_aviso = "No caben m" S_aa "s: hasta 23 caracteres.";
    return 1;
}

/* El nombre escrito, sin espacios al final, en dst (24); 0 si queda vacío */
static int nom_limpio(char *dst)
{
    size_t n = strlen(nom_edit);
    while (n > 0 && nom_edit[n - 1] == ' ') n--;
    memcpy(dst, nom_edit, n);
    dst[n] = 0;
    return n > 0;
}

/* Empieza a escribir un nombre: el que tiene, o vacío si es el que puso el
 * programa («Problema nuevo», «Mi problema 3») */
static void empieza_nombre(int modo)
{
    const char *de = modo == NOM_MIS ? mis_nombre[sel_mis] : circ.nombre;
    int h = modo == NOM_MIS ? mis_hueco[sel_mis] : hueco;
    if (modo == NOM_EDITOR && circ.nel == 0) { pon_aviso("El circuito est" S_aa " vac" S_ii "o: nada que guardar.", C_ERROR); return; }
    nombrando = modo;
    nom_aviso = 0;
    memset(nom_edit, 0, sizeof nom_edit);
    if (!nombre_de_omision(de, h)) memcpy(nom_edit, de, strlen(de) < sizeof nom_edit ? strlen(de) : sizeof nom_edit - 1);
}

/* El problema del hueco h se llama ahora nom: su archivo y, si es el del
 * editor (o el de un nivel de deshacer o rehacer), también ahí; si no,
 * Guardar volvería a escribir el nombre de antes. 1 si se pudo escribir */
static int renombra_hueco(int h, const char *nom)
{
    static Circuito t;
    int i;
    if (!arch_cargar(h, &t)) return 0;
    memset(t.nombre, 0, sizeof t.nombre);
    memcpy(t.nombre, nom, strlen(nom));
    if (!arch_guardar(h, &t)) return 0;
    if (hueco == h) memcpy(circ.nombre, t.nombre, sizeof circ.nombre);
    for (i = 0; i < nhist; i++) if (hist[i].hueco == h) memcpy(hist[i].c.nombre, t.nombre, sizeof t.nombre);
    for (i = 0; i < nreh; i++) if (reh[i].hueco == h) memcpy(reh[i].c.nombre, t.nombre, sizeof t.nombre);
    return 1;
}

#define NOM_VACIO "Escribe un nombre ([esc] lo deja como estaba)."

/* Mis problemas, escribiendo el nombre del elegido */
static void tecla_nombre_mis(int t)
{
    char nom[24];
    if (t == T_ESC) { nombrando = NOM_NO; nom_aviso = 0; return; }
    if (t == T_ENTER) {
        if (!nom_limpio(nom)) { nom_aviso = NOM_VACIO; return; }
        if (!renombra_hueco(mis_hueco[sel_mis], nom)) { nom_aviso = "No se pudo guardar el archivo."; return; }
        nombrando = NOM_NO;
        carga_lista_mis();                         /* la lista, con el nombre nuevo */
        return;
    }
    nom_tecla(t);                                  /* lo demás (las flechas, [menu]...) no hace nada */
}

/* El editor con [ctrl][N]: [enter] le pone el nombre y lo guarda (en su hueco
 * o, la primera vez, en uno nuevo); si no se puede guardar, el nombre de antes */
static void tecla_nombre_editor(int t)
{
    char nom[24], antes[sizeof circ.nombre];
    if (t == T_ESC) { nombrando = NOM_NO; nom_aviso = 0; return; }
    if (t == T_ENTER) {
        if (!nom_limpio(nom)) { nom_aviso = NOM_VACIO; return; }
        memcpy(antes, circ.nombre, sizeof antes);
        memset(circ.nombre, 0, sizeof circ.nombre);
        memcpy(circ.nombre, nom, strlen(nom));
        nombrando = NOM_NO;
        if (guardar(0, 1)) doc_renombra();         /* deshacer no vuelve al nombre de antes */
        else memcpy(circ.nombre, antes, sizeof antes);
        return;
    }
    nom_tecla(t);
}

/* [enter] en el editor: si junto al cursor hay un solo elemento, se abre;
 * si hay varios, se pregunta con una flecha. Se cuentan elementos DISTINTOS
 * (los segmentos de la caja de un 3 patas son uno) y en un punto interior de
 * una caja se abre su componente. */
static void editar_aqui(void)
{
    int d, k = circ_interior(&circ, cur_x, cur_y), n = 0, vistos[4], j;
    if (k >= 0) { abre_formulario(k, 1); return; }
    for (d = 0; d < 4; d++) {
        int e = elem_en(cur_x, cur_y, d);
        if (e < 0) continue;
        for (j = 0; j < n && vistos[j] != e; j++) {}
        if (j == n) { vistos[n++] = e; k = e; }
    }
    if (n == 0) pon_aviso("Junto al cursor no hay ningún elemento.", C_SUAVE);
    else if (n == 1) abre_formulario(k, 1);
    else accion = ACC_EDITAR;
}

static void tecla_ctx(int t)
{
    if (t == T_ARRIBA) sel_ctx = (sel_ctx + N_CTX - 1) % N_CTX;
    else if (t == T_ABAJO) sel_ctx = (sel_ctx + 1) % N_CTX;
    else if (t == T_ESC || t == T_MENU) ctx_abierto = 0;
    else if (t == T_ENTER) {
        ctx_abierto = 0;
        switch (sel_ctx) {
        case 0: igual(); break;
        case 1: practicar(); break;
        case 2: deshacer(); break;
        case 3: guardar(0, 0); break;
        case 4: {
            char nom[24];
            guarda_hist();
            memcpy(nom, circ.nombre, sizeof nom);
            circ_init(&circ, nom);
            sel_el = -1;
            pon_aviso("Circuito borrado. [ctrl][Z] lo recupera.", C_CURSOR);
            break;
        }
        case 5: abre_ayuda(); break;
        case 6: abre_ajustes(P_EDITOR); break;
        case 7:
            if (circ_pide(&circ)) pon_aviso("El Bode no despeja: quita los [?!].", C_CURSOR);
            else bode_de(P_EDITOR, &circ);
            break;
        case 8: barre_editor(); break;                    /* como [I] */
        case 9: punto_op(P_EDITOR, 1); break;             /* como [P] */
        case 10: guardar(1, 0); break;                    /* como copia */
        default: pant = P_MENU; sel_menu = 0; ui_sesion_guardar(); break;
        }
    }
}

/* --------------------------------------------------------------------------
 *  Paleta: todo lo que se puede poner, con su tecla ([catálogo] la abre)
 * ----------------------------------------------------------------------- */
/* PAL_N (ui.h): el dibujo, la navegación y el arnés la usan. Los índices 0 a
 * 13 no se mueven (las pruebas recorren la paleta por posición); cada pieza
 * nueva entra al final, con su modelo (§4.3 de la Etapa 4) */
static const struct { char tecla; int ico; const char *nombre; int tipo; } PAL[PAL_N] = {
    { 'R', ICO_RES,    "Resistencia" },
    { 'E', ICO_FUENTE, "Fuente E" },
    { 'J', ICO_CORR,   "Fuente J" },
    { 'V', ICO_VOLT,   "Voltímetro" },
    { 'A', ICO_AMP,    "Amperímetro" },
    { 'W', ICO_CABLE,  "Cable" },
    { 'G', ICO_TIERRA, "Tierra" },
    { 'T', ICO_TERM,   "Terminal A/B" },
    { 'C', ICO_COND,   "Condensador" },
    { 'L', ICO_BOB,    "Bobina" },
    { 'S', ICO_INTER,  "Interruptor" },
    { 'O', ICO_SONDA,  "Sonda" },
    { 'Z', ICO_IMP,    "Impedancia" },
    { 0,   ICO_VATI,   "Vatímetro", EL_VATI },          /* sin tecla: la W es el cable */
    /* Etapa 4 (paso 11): el zener, el LED y las controladas, sin tecla */
    { 'D', ICO_DIODO,  "Diodo" },
    { 0,   ICO_ZENER,  "Zener", EL_ZENER },
    { 0,   ICO_LED,    "LED", EL_LED },
    { 0,   ICO_EDEP,   "E control.", EL_EDEP },
    { 0,   ICO_JDEP,   "J control.", EL_JDEP },
    { 'U', ICO_AO,     "Amp. op." },
    /* paso 13: el BJT (PNP, en su ficha con [izq/der]) */
    { 'Q', ICO_BJT,    "Transistor" },
    /* paso 14: el MOSFET (canal P, en su ficha con [izq/der]) */
    { 'M', ICO_MOS,    "MOSFET" },
};

int ui_paleta_tipo(int i)
{
    int j;
    if (i < 0 || i >= PAL_N) return -1;
    if (PAL[i].tecla == 0) return PAL[i].tipo;
    for (j = 0; j < EL_N; j++) if (TIPOS[j].tecla == PAL[i].tecla) return j;
    return -1;                                            /* W, G, T y O no son elementos */
}

/* 6 filas de 4 celdas de 30 px: el panel va de y = 22 a 224 */
static void dib_paleta(void)
{
    int x0 = 12, y0 = 22, w = 296, h = 202, i;
    gfx_fill(x0, y0, w, h, C_PANEL);
    gfx_rect(x0, y0, w, h, C_CURSOR);
    gfx_text(x0 + 8, y0 + 5, "Componentes  [flechas] [enter]", C_CURSOR, 1);
    for (i = 0; i < PAL_N; i++) {
        int cx = 16 + 72 * (i % 4), cy = 42 + 30 * (i / 4);
        int tipo = ui_paleta_tipo(i);
        char t[4];
        u16 col;
        col = tipo >= 0 ? tipo_info(tipo)->color : PAL[i].tecla == 'O' ? C_CANAL1 : C_COTA;
        if (i == sel_pal) { gfx_fill(cx, cy - 1, 70, 30, C_BARRA); col = C_CURSOR; }
        esq_icono(PAL[i].ico, cx + 4, cy + 1, col);
        t[0] = '['; t[1] = PAL[i].tecla; t[2] = ']'; t[3] = 0;
        if (PAL[i].tecla) gfx_text(cx + 30, cy + 2, t, i == sel_pal ? C_CURSOR : C_SUAVE, 1);
        gfx_text(cx + 2, cy + 16, PAL[i].nombre, i == sel_pal ? C_TEXTO : C_COTA, 1);
    }
}

static void tecla_editor(int t);

static void tecla_paleta(int t)
{
    /* [ctrl] con Z, Y, S, C, X, V o N: lo del editor, con la paleta cerrada
     * (no la pieza de esa letra: Y, X y N no tienen, y Z, S, C y V sí) */
    if ((mods & T_CON_CTRL) && (t == T_LETRA('Z') || t == T_LETRA('Y') || t == T_LETRA('S') || t == T_LETRA('C') ||
                                t == T_LETRA('X') || t == T_LETRA('V') || t == T_LETRA('N'))) {
        paleta_abierta = 0;
        tecla_editor(t);
        return;
    }
    if (t == T_IZQ && sel_pal % 4 > 0) sel_pal--;
    else if (t == T_DER && sel_pal % 4 < 3 && sel_pal + 1 < PAL_N) sel_pal++;
    else if (t == T_ARRIBA && sel_pal >= 4) sel_pal -= 4;
    else if (t == T_ABAJO && sel_pal + 4 < PAL_N) sel_pal += 4;
    else if (t == T_ESC || t == T_CATALOGO) paleta_abierta = 0;
    else if (t == T_ENTER) {                      /* como pulsar su tecla */
        paleta_abierta = 0;
        if (PAL[sel_pal].tecla == 0) { accion = ACC_PONER; poner_tipo = PAL[sel_pal].tipo; }
        else tecla_editor(T_LETRA(PAL[sel_pal].tecla));
    } else {
        int i;                                    /* o su letra, la que rotula */
        for (i = 0; i < PAL_N; i++)
            if (PAL[i].tecla && t == T_LETRA(PAL[i].tecla)) { paleta_abierta = 0; tecla_editor(t); return; }
    }
}

/* El tipo que se coloca con esa tecla (la letra de su etiqueta), o -1 */
static int tipo_de_tecla(int t)
{
    int i;
    for (i = 0; i < EL_N; i++) if (TIPOS[i].tecla && t == T_LETRA(TIPOS[i].tecla)) return i;   /* sin tecla: solo la paleta */
    return -1;
}

/* Sondas: [O] y luego [enter] (la tensión del punto) o una flecha (la I
 * del elemento, luego su V, luego nada) */
static void sonda_hecha(int r, int canal, int ocupados, const char *que)
{
    char t[64];
    Buf b;
    buf_init(&b, t, sizeof t);
    if (r == SONDA_NO) { buf_str(&b, "Sonda quitada del canal "); buf_int(&b, canal + 1); buf_ch(&b, '.'); pon_aviso(t, C_SUAVE); return; }
    if (ocupados == 2 && canal == 1) buf_str(&b, "Canal 2 movido aqu" S_ii ": ");
    else { buf_str(&b, "Canal "); buf_int(&b, canal + 1); buf_str(&b, ": "); }
    buf_str(&b, que);
    if (circ.tierra_x < 0 && r == SONDA_NODO && ocupados == 0) {
        int k;
        for (k = 0; k < circ.nel && circ.el[k].tipo != EL_FUENTE; k++) ;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Sin tierra: 0 V es el borne - de ");
        buf_str(&b, k < circ.nel ? circ.el[k].etiq : "la fuente");
        buf_str(&b, ". [G] la pone");
        pon_aviso(t, C_CURSOR);
        return;
    }
    pon_aviso(t, C_OK);
}

static int sondas_ocupadas(void) { return (circ.sonda[0].tipo != SONDA_NO) + (circ.sonda[1].tipo != SONDA_NO); }

/* [G], [T] u [O][enter] en un punto interior de una caja */
static void aviso_dentro(int k)
{
    char t[64];
    Buf b;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Dentro de "); buf_str(&b, circ.el[k].etiq); buf_str(&b, " no hay borne: ponla en una pata.");
    pon_aviso(t, C_ERROR);
}

static void sonda_nodo(void)
{
    int canal, ocup = sondas_ocupadas(), r, k;
    accion = ACC_NADA;
    if ((k = circ_interior(&circ, cur_x, cur_y)) >= 0) { aviso_dentro(k); return; }
    if (circ_grado(&circ, cur_x, cur_y) == 0) { pon_aviso("Aqu" S_ii " no hay circuito.", C_ERROR); return; }
    guarda_hist();
    r = circ_sonda_pon_nodo(&circ, cur_x, cur_y, &canal);
    sonda_hecha(r, canal, r == SONDA_NO ? 0 : ocup, "la tensi" S_oo "n de este punto");
}

static void sonda_elem(int d)
{
    int k = elem_en(cur_x, cur_y, d), canal, ocup = sondas_ocupadas(), r, sg;
    char t[24];
    Buf b;
    accion = ACC_NADA;
    if (k < 0) { pon_aviso("Ah" S_ii " no hay elemento: la I de un cable, con un [A].", C_SUAVE); return; }
    if (circ_npatas(&circ.el[k]) == 3) { pon_aviso("Sin sondas en las patas: [O] en el punto o un A.", C_CURSOR); return; }
    guarda_hist();
    sg = circ.el[k].xa == cur_x && circ.el[k].ya == cur_y ? 1 : -1;
    if (circ_sonda_elem(&circ, k) >= 0) ocup = 0;         /* rota la suya: no mueve otra */
    r = circ_sonda_ciclo(&circ, k, sg, &canal);
    buf_init(&b, t, sizeof t);
    buf_str(&b, r == SONDA_I ? "I(" : "V(");
    buf_str(&b, circ.el[k].etiq);
    buf_ch(&b, ')');
    sonda_hecha(r, canal, ocup, t);
}

/* El circuito dejó de ser de alterna (se borró o se tapó su última Z o
 * fuente alterna): fuera los ángulos, el fp del W y lo que solo se ve en CA */
static void quita_ca_si(int era_ca)
{
    if (era_ca && !circ_alterna(&circ) && circ_quita_ca(&circ))
        pon_aviso("Ya no es CA: se quitan " S_aa "ngulos y [?!] de C y L.", C_CURSOR);
}

/* Un cable o un elemento de dos bornes contra la caja k de un 3 patas (el
 * segmento es de la caja, o toca un punto interior): no se toca nada, el
 * cursor no se mueve y la acción sigue */
static void aviso_caja(int d, int k)
{
    char t[64];
    Buf b;
    int s = circ_seg(&circ, cur_x, cur_y, d);
    buf_init(&b, t, sizeof t);
    if (k < 0 || k >= circ.nel) buf_str(&b, "Ah" S_ii " no se puede: es de un componente.");
    else if (s == SEG_EL0 + k) { buf_str(&b, "Ah" S_ii " est" S_aa " "); buf_str(&b, circ.el[k].etiq); buf_str(&b, ": los cables van a sus patas."); }
    else { buf_str(&b, "Ese punto est" S_aa " dentro de "); buf_str(&b, circ.el[k].etiq); buf_str(&b, ": no es una pata."); }
    pon_aviso(t, C_ERROR);
}

/* Lo que se dice al colocar k (en su ficha, que ya está abierta): una
 * controlada va a su fila x; un LED se enciende en el resultado; la salida de
 * un AO sin tierra se mide desde el - de la primera fuente de su subcircuito,
 * y sin fuentes no tiene referencia (si el circuito tiene la tierra en otra
 * parte, U1 aún no está cableado: nada que decir) */
static void al_colocar(int k)
{
    const Elemento *e = &circ.el[k];
    if (tipo_info(e->tipo)->ley_cc == LEY_DEP) {
        int i;
        for (i = 0; i < ncampos(); i++) if (campo_info(i)->q == Q_CTRL) { campo = i; break; }
        aviso_nb = "[+][-] elige qui" S_ee "n la manda (en naranja).";
    } else if (e->tipo == EL_LED) aviso_nb = "Se enciende en el resultado: [=].";
    else if (e->tipo == EL_AO) {
        static Red r;
        static char av[64];
        Buf b;
        int n, nt = -1, f;
        red_extraer(&circ, &r);
        n = r.c[k];
        if (circ.tierra_x >= 0 && circ.tierra_y >= 0) nt = r.punto[circ.tierra_y * REJ_COLS + circ.tierra_x];
        if (n < 0 || (nt >= 0 && r.comp[nt] == r.comp[n])) return;      /* con tierra: nada que decir */
        buf_init(&b, av, sizeof av);
        if (!red_ref_propia(&circ, &r, n)) {
            if (circ.tierra_x >= 0) return;   /* la tierra está en otra parte: aún sin cablear ([G] la movería) */
            buf_str(&b, "Sin [G] ni fuentes, "); buf_str(&b, e->etiq); buf_str(&b, " no tiene referencia: pon [G].");
            aviso_nb_mal = 1;
        } else {
            for (f = 0; f < circ.nel; f++)          /* la de red_cierra: la primera de su subcircuito */
                if (tipo_info(circ.el[f].tipo)->fuente && r.a[f] >= 0 && r.comp[r.a[f]] == r.comp[n]) break;
            buf_str(&b, "Sin [G], la salida de "); buf_str(&b, e->etiq);
            buf_str(&b, " va al - de "); buf_str(&b, f < circ.nel ? circ.el[f].etiq : "la fuente"); buf_ch(&b, '.');
        }
        aviso_nb = av;
    }
}

/* Una flecha con ACC_PONER de un 3 patas: si cabe (circ_cabe), se coloca, el
 * cursor salta a la pata `sigue` de su huella y se abre su ficha; si no, no se
 * toca nada, la acción sigue y la marca señala el punto que estorba */
static void pon_3patas(int d)
{
    static const char *const FL[4] = { S_RARR, S_DARR, S_LARR, S_UARR };
    CabeInfo ci;
    int k, era_ca = circ_alterna(&circ), px, py;
    char t[64];
    Buf b;
    if (circ_cabe(&circ, poner_tipo, cur_x, cur_y, d, &ci) != CM_OK) {
        buf_init(&b, t, sizeof t);
        if (ci.motivo == CM_LLENO) buf_str(&b, "No caben más elementos (máximo 32).");
        else {
            buf_str(&b, "No cabe hacia "); buf_str(&b, FL[d & 3]); buf_str(&b, ": ");
            if (ci.motivo == CM_FUERA) buf_str(&b, "se sale de la rejilla.");
            else {
                buf_str(&b, "estorba ");
                if (ci.motivo == CM_OCUPADO && ci.el >= 0 && ci.el < circ.nel) buf_str(&b, circ.el[ci.el].etiq);
                else if (ci.motivo == CM_OCUPADO) buf_str(&b, "un cable");
                else if (ci.que == 'G') buf_str(&b, "la tierra");
                else if (ci.que == 'A' || ci.que == 'B') { buf_str(&b, "el terminal "); buf_ch(&b, ci.que); }
                else { buf_str(&b, "la sonda "); buf_ch(&b, ci.que); }
                buf_ch(&b, '.');
                marca_x = ci.x; marca_y = ci.y;             /* un punto de la rejilla */
            }
        }
        pon_aviso(t, C_ERROR);
        return;
    }
    guarda_hist();
    k = circ_elemento(&circ, (TipoEl)poner_tipo, cur_x, cur_y, d);
    accion = ACC_NADA;
    if (k < 0) { pegando = 0; return; }                   /* no pasa: cabía */
    if (pegando) aplica_clip(k);                          /* [ctrl][V]: con sus valores (y su espejo) */
    quita_ca_si(era_ca);
    circ_pata(&circ.el[k], tipo_info(poner_tipo)->huella->sigue, &px, &py);
    cur_x = px; cur_y = py;
    abre_formulario(k, 0);
    if (pegando) aviso_nb = COPIA_PUESTA;
    al_colocar(k);                                        /* un AO sin referencia: cede ante eso */
    pegando = 0;
}

/* «R1 (4,7 kΩ)», o «R1» si no tiene valor o si con él la línea pasaría de 52
 * columnas (resto: las que van detrás) */
static void etiq_valor(Buf *b, int k, size_t resto)
{
    char v[32];
    buf_str(b, circ.el[k].etiq);
    esq_valor_c(&circ, 0, k, v, (int)sizeof v);
    if (v[0] && strcmp(v, "?") != 0 && b->n + 3 + strlen(v) + resto <= COLS) {
        buf_str(b, " ("); buf_str(b, v); buf_ch(b, ')');
    }
}

/* Poner una pieza o un cable encima de un elemento lo sustituye, y [del] lo
 * borra: «Sustituye a R1 (4,7 kΩ): [ctrl][Z] lo deshace.». Se escribe ANTES
 * de tocar el circuito (el índice k cambia después) */
static void texto_quita(char *buf, size_t cap, const char *que, int k)
{
    static const char cola[] = ": [ctrl][Z] lo deshace.";
    Buf b;
    buf_init(&b, buf, cap);
    buf_str(&b, que);
    etiq_valor(&b, k, sizeof cola - 1);
    buf_str(&b, cola);
}

/* --------------------------------------------------------------------------
 *  Copiar, cortar y pegar ([ctrl][C], [ctrl][X], [ctrl][V])
 * ----------------------------------------------------------------------- */
/* El elemento de [ctrl][C] y [ctrl][X]: el elegido ([tab], o el de la ficha
 * de la que se vuelve) o, si no hay, el único que toca el cursor; -1 si
 * ninguno o varios */
static int elemento_aqui(void)
{
    int d, e, k = circ_interior(&circ, cur_x, cur_y);
    if (sel_el >= 0 && sel_el < circ.nel) return sel_el;
    if (k >= 0) return k;
    for (d = 0; d < 4; d++)
        if ((e = elem_en(cur_x, cur_y, d)) >= 0) {
            if (k >= 0 && e != k) return -1;              /* dos: ¿cuál? */
            k = e;
        }
    return k;
}

/* La ficha física de k al portapapeles: todo menos la geometría, la
 * etiqueta, los datos de V e I y los [?!]. El control de una controlada va
 * por la etiqueta del que manda (los índices cambian al borrar) */
static void copia_elemento(int k)
{
    const Elemento *e = &circ.el[k];
    int q, d, esp, ox, oy;
    Buf b;
    memset(&clip, 0, sizeof clip);
    clip.hay = 1;
    clip.e = *e;
    memset(&clip.e.V, 0, sizeof clip.e.V);
    memset(&clip.e.I, 0, sizeof clip.e.I);
    for (q = 0; q < Q_TOT; q++) {
        Mag *m = circ_mag(&clip.e, q);
        if (m->est == M_BUSCAR) m->est = M_LIBRE;
        if (m->est_ang == M_BUSCAR) m->est_ang = M_LIBRE;
    }
    memcpy(clip.de, e->etiq, sizeof clip.de);
    if (e->ref >= 0 && e->ref < circ.nel) memcpy(clip.ctl, circ.el[e->ref].etiq, sizeof clip.ctl);
    clip.espejo = circ_npatas(e) == 3 && circ_orienta(e, &d, &esp, &ox, &oy) && esp;
    clip.frec = circ.frec;
    buf_init(&b, clip.pegar, sizeof clip.pegar);          /* «Pegar R2 (4,7 kΩ): una flecha. [esc] no» */
    buf_str(&b, "Pegar ");
    etiq_valor(&b, k, sizeof ": una flecha. [esc] no" - 1);
    buf_str(&b, ": una flecha. [esc] no");
}

/* [ctrl][C] y [ctrl][X] (corta = 1: además lo quita, en un nivel de deshacer) */
static void copia_o_corta(int corta)
{
    static const char cola[] = ": [ctrl][V] y una flecha.";
    int k = elemento_aqui(), era_ca = circ_alterna(&circ);
    Buf b;
    if (k < 0) { pon_aviso("Elige primero un elemento: [tab] o [enter].", C_SUAVE); return; }
    copia_elemento(k);
    buf_init(&b, aviso, sizeof aviso);                    /* «Copiado R2 (4,7 kΩ): [ctrl][V] y una flecha.» */
    buf_str(&b, corta ? "Cortado " : "Copiado ");
    etiq_valor(&b, k, sizeof cola - 1);
    buf_str(&b, cola);
    aviso_col = C_OK;
    if (!corta) return;
    guarda_hist();
    circ_borra(&circ, k);
    sel_el = -1;
    quita_ca_si(era_ca);                                  /* si era la última alterna, lo dice */
}

/* La copia en el elemento k recién puesto (del mismo tipo): sus valores, su
 * espejo y quién la manda (por la etiqueta: si ya no está, -1 y la ficha
 * dice «falta»). Si con ella el circuito pasa a ser de alterna y no tenía f,
 * la del circuito de donde vino (o 60 Hz, como al elegir la forma alterna) */
static void aplica_clip(int k)
{
    Elemento *e = &circ.el[k];
    int i;
    e->val = clip.e.val; e->V = clip.e.V; e->I = clip.e.I; e->aux = clip.e.aux;
    e->variante = clip.e.variante;
    memcpy(e->par, clip.e.par, sizeof e->par);
    e->ref = -1;
    if (clip.ctl[0])
        for (i = 0; i < circ.nel; i++)
            if (i != k && strcmp(circ.el[i].etiq, clip.ctl) == 0) { e->ref = (signed char)i; break; }
    if (clip.espejo) circ_voltear(&circ, k);
    if (circ_alterna(&circ) && circ.frec.est != M_DATO && circ.frec.est != M_BUSCAR) {
        if (clip.frec.est == M_DATO) circ.frec = clip.frec;
        else { memset(&circ.frec, 0, sizeof circ.frec); circ.frec.v = 60.0; circ.frec.est = M_DATO; }
    }
}

/* [ctrl][V]: la flecha siguiente pone la copia (ACC_PONER con pegando) */
static void pega(void)
{
    if (!clip.hay) { accion = ACC_NADA; pon_aviso("Nada que pegar: [ctrl][C] copia el elegido.", C_SUAVE); return; }
    accion = ACC_PONER;
    poner_tipo = clip.e.tipo;
    pegando = 1;
}

/* El nivel de deshacer de un tramo de cable: uno por ráfaga (una flecha
 * mantenida o un gesto del dedo): los T_REPE no gastan otro */
static void guarda_hist_rafaga(void)
{
    if (!rep_act || !hist_rafaga) guarda_hist();
    hist_rafaga = 1;
}

/* ¿El tramo de d desde (x, y) admite cable sin sustituir nada? */
static int tramo_libre(int x, int y, int d)
{
    return circ_seg_libre(&circ, x, y, d, 0) && elem_en(x, y, d) < 0;
}

/* [shift] y una flecha: hasta el siguiente punto con algo (un borne, un
 * cable, una caja) o hasta el borde; [ctrl]: hasta el borde. Con [W], el
 * cable llega hasta ahí en UNA entrada del historial, sin sustituir nada
 * por el camino (se para antes). 0 si no se movió */
static int salto_editor(int d, int hasta_borde)
{
    int x = cur_x, y = cur_y, nx, ny, n = 0, era_ca = circ_alterna(&circ), cable = accion == ACC_CABLE;
    while (circ_vecino(x, y, d, &nx, &ny)) {
        int algo = circ_grado(&circ, nx, ny) > 0 || circ_interior(&circ, nx, ny) >= 0;
        if (cable) {
            if (!tramo_libre(x, y, d)) break;
            if (n == 0) guarda_hist_rafaga();
            circ_cable(&circ, x, y, d);
        }
        x = nx; y = ny; n++;
        if (!hasta_borde && algo) break;
    }
    if (n == 0) return 0;
    cur_x = x; cur_y = y;
    if (cable) quita_ca_si(era_ca);
    return 1;
}

static void tecla_editor(int t)
{
    int d = dir_de(t), nx, ny, k, era_ca, era_elegido, era_q;
    static char sust[64];                 /* el aviso de la ficha al sustituir */

    if (nombrando == NOM_EDITOR) { tecla_nombre_editor(t); return; }
    if (ctx_abierto) { tecla_ctx(t); return; }
    if (paleta_abierta) { tecla_paleta(t); return; }
    aviso[0] = 0;
    aviso_bajo = 0;
    marca_x = marca_y = -1;
    sal_ficha();                          /* por si se salió de una ficha sin pasar por ella */
    /* la marca de [tab] (o del culpable de un error) dura una tecla */
    era_elegido = elegido; era_q = elegido_q;
    elegido = 0; elegido_q = -1;
    /* pegar dura hasta la flecha que pone la copia (o hasta otra tecla) */
    if (d < 0 && !(t == T_LETRA('V') && (mods & T_CON_CTRL))) pegando = 0;

    if (d >= 0) {
        /* los saltos: sin acción, o con [W] si el primer tramo está libre (si
         * no, la flecha de siempre: sustituye o avisa) */
        if ((mods & (T_CON_SHIFT | T_CON_CTRL)) && (accion == ACC_NADA || accion == ACC_CABLE) &&
            circ_vecino(cur_x, cur_y, d, &nx, &ny) && (accion == ACC_NADA || tramo_libre(cur_x, cur_y, d))) {
            salto_editor(d, (mods & T_CON_CTRL) != 0);
            return;
        }
        if (accion == ACC_EDITAR) {
            k = elem_en(cur_x, cur_y, d);
            accion = ACC_NADA;
            if (k >= 0) abre_formulario(k, 1);
            else pon_aviso("En esa dirección no hay un elemento.", C_SUAVE);
            return;
        }
        if (accion == ACC_SONDA) { sonda_elem(d); return; }
        if (accion == ACC_PONER && tipo_info(poner_tipo)->huella) { pon_3patas(d); return; }
        if (!circ_vecino(cur_x, cur_y, d, &nx, &ny)) return;
        era_ca = circ_alterna(&circ);
        /* un cable o un elemento de dos bornes no entra en la caja de un 3 patas */
        if ((accion == ACC_PONER || accion == ACC_CABLE) && !circ_seg_libre(&circ, cur_x, cur_y, d, &k)) {
            aviso_caja(d, k);
            return;
        }
        switch (accion) {
        case ACC_PONER:
            /* sobre un elemento: lo sustituye (y la pieza nueva suele heredar
             * su etiqueta, así que parecería que se borraron sus valores) */
            sust[0] = 0;
            if ((k = elem_en(cur_x, cur_y, d)) >= 0) texto_quita(sust, sizeof sust, "Sustituye a ", k);
            guarda_hist();
            k = circ_elemento(&circ, (TipoEl)poner_tipo, cur_x, cur_y, d);
            accion = ACC_NADA;
            cur_x = nx; cur_y = ny;
            /* [ctrl][V]: la copia, con sus valores, antes de mirar si sigue
             * siendo de alterna (pegar una fuente alterna sobre la otra) */
            if (k >= 0 && pegando) aplica_clip(k);
            if (k >= 0) quita_ca_si(era_ca);
            if (k >= 0) {
                if (circ.el[k].tipo == EL_RES && hay_ultimo_R && !pegando) {
                    circ.el[k].val.v = ultimo_R;
                    circ.el[k].val.est = M_DATO;
                }
                /* en un circuito de alterna, un V o un A nuevo marca en CA:
                 * al emularlo en el tiempo marca lo mismo que en fasores (una
                 * copia, en el modo de la que se copió) */
                if ((circ.el[k].tipo == EL_VOLT || circ.el[k].tipo == EL_AMP) && circ_alterna(&circ) && !pegando)
                    circ.el[k].variante = MED_CA;
                abre_formulario(k, 0);    /* Z vuelve a antes de ponerlo */
                if (pegando) aviso_nb = COPIA_PUESTA;
                if (sust[0]) aviso_nb = sust;             /* cede ante los de al_colocar y el del W */
                /* una controlada pegada ya sabe quién la manda (si sigue ahí) */
                if (!pegando || (tipo_info(circ.el[k].tipo)->ley_cc == LEY_DEP && circ.el[k].ref < 0)) al_colocar(k);
                if (circ.el[k].tipo == EL_VATI && circ.tierra_x < 0) {
                    int f;
                    static char av[64];
                    Buf b;
                    for (f = 0; f < circ.nel; f++) if (tipo_info(circ.el[f].tipo)->fuente) break;
                    buf_init(&b, av, sizeof av);
                    buf_str(&b, "Sin [G], la bobina de tensi" S_oo "n va al - de ");
                    buf_str(&b, f < circ.nel ? circ.el[f].etiq : "la fuente");
                    buf_ch(&b, '.');
                    aviso_nb = av;
                }
            } else {
                pon_aviso("No caben más elementos (máximo 32).", C_ERROR);
            }
            pegando = 0;
            return;
        case ACC_CABLE:
            guarda_hist_rafaga();                                        /* uno por ráfaga */
            if ((k = elem_en(cur_x, cur_y, d)) >= 0) {                   /* lo sustituye */
                texto_quita(aviso, sizeof aviso, "Cable en lugar de ", k);
                aviso_col = C_CURSOR;
                sel_el = -1;
            }
            circ_cable(&circ, cur_x, cur_y, d);
            quita_ca_si(era_ca);                  /* y si deja de ser CA, lo dice */
            break;
        case ACC_BORRAR:
            if (circ_seg(&circ, cur_x, cur_y, d) > 0) {
                guarda_hist();
                /* borrar un elemento corre los índices de los de detrás; un
                 * tramo de cable no se avisa: es lo normal */
                if ((k = elem_en(cur_x, cur_y, d)) >= 0) {
                    texto_quita(aviso, sizeof aviso, "Se borr" S_oo " ", k);
                    aviso_col = C_CURSOR;
                    sel_el = -1;
                }
                circ_quitar(&circ, cur_x, cur_y, d);
                quita_ca_si(era_ca);
            }
            break;
        }
        cur_x = nx; cur_y = ny;
        return;
    }

    if (accion == ACC_SONDA && (t == T_ENTER || t == T_LETRA('O'))) { sonda_nodo(); return; }
    if (t == T_LETRA('O')) { accion = ACC_SONDA; return; }
    if (t == T_LETRA('Z') && (mods & T_CON_CTRL)) { accion = ACC_NADA; deshacer(); return; }   /* antes que la Z */
    if (t == T_LETRA('Y') && (mods & T_CON_CTRL)) { accion = ACC_NADA; rehacer(); return; }
    if (t == T_LETRA('S') && (mods & T_CON_CTRL)) { accion = ACC_NADA; guardar(0, 0); return; }   /* antes que la S */
    /* copiar, cortar y pegar, antes que la C y la V (condensador y voltímetro) */
    if (t == T_LETRA('C') && (mods & T_CON_CTRL)) { accion = ACC_NADA; copia_o_corta(0); return; }
    if (t == T_LETRA('X') && (mods & T_CON_CTRL)) { accion = ACC_NADA; copia_o_corta(1); return; }
    if (t == T_LETRA('V') && (mods & T_CON_CTRL)) { pega(); return; }
    if (t == T_LETRA('N') && (mods & T_CON_CTRL)) { accion = ACC_NADA; empieza_nombre(NOM_EDITOR); return; }
    if (tipo_de_tecla(t) >= 0) { accion = ACC_PONER; poner_tipo = tipo_de_tecla(t); }
    else if (t == T_LETRA('W')) accion = ACC_CABLE;
    else if (t == T_LETRA('B')) {
        accion = ACC_NADA;
        if (circ_pide(&circ)) pon_aviso("El Bode no despeja: quita los [?!].", C_CURSOR);
        else bode_de(P_EDITOR, &circ);
    }
    else if (t == T_LETRA('I')) { accion = ACC_NADA; barre_editor(); }           /* barrido de CC */
    else if (t == T_LETRA('P')) { accion = ACC_NADA; punto_op(P_EDITOR, 1); }    /* punto de operación de CC */
    else if ((t == T_LETRA('G') || t == T_LETRA('T')) && (k = circ_interior(&circ, cur_x, cur_y)) >= 0)
        aviso_dentro(k);                                /* un punto interior no es un borne */
    else if (t == T_LETRA('G') && !(circ.tierra_x == cur_x && circ.tierra_y == cur_y) &&
             circ_grado(&circ, cur_x, cur_y) == 0)      /* en un punto suelto no fija nada */
        pon_aviso("Aquí no hay circuito: la tierra, en cable o borne.", C_ERROR);   /* una línea (52) */
    else if (t == T_LETRA('G')) {                       /* tierra: 0 V aquí */
        guarda_hist();
        if (circ.tierra_x == cur_x && circ.tierra_y == cur_y) {
            circ.tierra_x = circ.tierra_y = -1;
            pon_aviso("Tierra quitada: 0 V en el borne - de la fuente.", C_SUAVE);
        } else {
            circ.tierra_x = (signed char)cur_x; circ.tierra_y = (signed char)cur_y;
            pon_aviso("Tierra (0 V) aquí. Los potenciales, en el resultado.", C_OK);
        }
    }
    else if (t == T_LETRA('T')) {                       /* terminales A y B */
        int i;
        guarda_hist();
        for (i = 0; i < 2; i++)
            if (circ.term_x[i] == cur_x && circ.term_y[i] == cur_y) break;
        if (i < 2) { circ.term_x[i] = circ.term_y[i] = -1; pon_aviso("Terminal quitado.", C_SUAVE); }
        else {
            i = circ.term_x[0] < 0 ? 0 : circ.term_x[1] < 0 ? 1 : 1;
            circ.term_x[i] = (signed char)cur_x; circ.term_y[i] = (signed char)cur_y;
            pon_aviso(i ? "Terminal B aquí. El resultado da V(AB)." : "Terminal A aquí. [T] en otro punto: el B.", C_OK);
        }
    }
    else if (t == T_BORRAR)     accion = ACC_BORRAR;
    else if (t == T_LETRA('F')) {
        if (sel_el >= 0 && tipo_info(circ.el[sel_el].tipo)->polar) {
            guarda_hist();
            circ_voltear(&circ, sel_el);
            if (circ_npatas(&circ.el[sel_el]) == 3) pon_aviso(texto_espejo(&circ.el[sel_el]), C_OK);
        }
        else pon_aviso("Elige primero una fuente ([tab] o [enter]).", C_SUAVE);
    }
    else if (t == T_TAB && circ.nel > 0) {
        /* el siguiente ([shift]: el anterior), elegido: [enter] abre su ficha
         * sin preguntar la flecha aunque en su borne haya otros */
        static const char cola[] = ": [enter] su ficha  [tab] el siguiente";
        Buf b;
        accion = ACC_NADA;
        if (!(mods & T_CON_SHIFT)) sel_el = (sel_el + 1) % circ.nel;
        else sel_el = sel_el <= 0 || sel_el > circ.nel ? circ.nel - 1 : sel_el - 1;
        cur_x = circ.el[sel_el].xa; cur_y = circ.el[sel_el].ya;
        elegido = 1;
        buf_init(&b, aviso, sizeof aviso);        /* «R3 (4 Ω): [enter] su ficha  [tab] el siguiente» */
        etiq_valor(&b, sel_el, sizeof cola - 1);
        buf_str(&b, cola);
        aviso_col = C_CURSOR;
    }
    else if (t == T_ENTER) {
        int i;
        accion = ACC_NADA;
        if (!era_elegido || sel_el < 0 || sel_el >= circ.nel) { editar_aqui(); return; }
        abre_formulario(sel_el, 1);
        /* volviendo de un error: en la fila que falla */
        for (i = 0; era_q >= 0 && i < ncampos(); i++) if (campo_info(i)->q == era_q) { campo = i; break; }
    }
    else if (t == T_IGUAL) { accion = ACC_NADA; igual(); }
    else if (t == T_INCOG) { accion = ACC_NADA; practicar(); }
    else if (t == T_MENU)  { accion = ACC_NADA; ctx_abierto = 1; sel_ctx = 0; }
    else if (t == T_CATALOGO) { accion = ACC_NADA; paleta_abierta = 1; }
    else if (t == T_ESC) {
        if (accion != ACC_NADA) accion = ACC_NADA;
        else if (sel_el >= 0) sel_el = -1;
        else { pant = P_TALLER; sel_taller = 1; ui_sesion_guardar(); }
    }
}

static void tecla_elemento(int t)
{
    Mag *mg = mag_campo(campo);
    Elemento *el = &circ.el[sel_el];
    const TipoInfo *ti = tipo_info(el->tipo);

    aviso_nb = 0;
    aviso_nb_mal = 0;
    if ((t == T_LETRA('Z') || t == T_LETRA('Y')) && (mods & T_CON_CTRL)) {
        /* deshacer y rehacer también desde la ficha, como en el editor (al
         * que se vuelve): lo escrito sin confirmar se tira y se deshace lo
         * último (al poner una pieza: la pieza, y vuelve lo que sustituyó). Si
         * la ficha cambió algo, ese cambio es lo último: rehacer ya no hay */
        editando = 0; edit[0] = 0; aviso_r = 0;
        sal_ficha();
        pant = P_EDITOR;
        if (t == T_LETRA('Z')) deshacer(); else rehacer();
        return;
    }
    if (t == T_MENU || (t == T_TAB && !circ_alterna(&circ)) ||
        ((mods & T_CON_CTRL) && (t == T_LETRA('S') || t == T_LETRA('C') || t == T_LETRA('X') ||
                                 t == T_LETRA('V') || t == T_LETRA('N')))) {
        /* [menu]: lo escrito vale como con [enter] y se abren las opciones del
         * editor (lo que promete su título). [ctrl] con S, C, X, V o N: igual,
         * y se guarda, copia, corta, pega o se le pone nombre desde el editor
         * (sin esto, [ctrl][X] marcaría [?!] y [ctrl][N] escribiría un nano).
         * [tab] en CC (en CA es el ángulo): la ficha del siguiente elemento
         * ([shift]: del anterior) */
        if (!editando) aviso_r = 0;
        confirma_campo();
        if (aviso_r) return;                      /* se queda con su aviso, como [enter] */
        sal_ficha();
        if (t == T_MENU) { pant = P_EDITOR; ctx_abierto = 1; sel_ctx = 0; return; }
        if (t != T_TAB) {
            pant = P_EDITOR;
            aviso[0] = 0; aviso_bajo = 0;         /* como una tecla del editor */
            if (t == T_LETRA('S')) guardar(0, 0);
            else if (t == T_LETRA('C') || t == T_LETRA('X')) copia_o_corta(t == T_LETRA('X'));   /* el de la ficha */
            else if (t == T_LETRA('V')) pega();
            else empieza_nombre(NOM_EDITOR);
            return;
        }
        {
            int k = (mods & T_CON_SHIFT) ? (sel_el + circ.nel - 1) % circ.nel : (sel_el + 1) % circ.nel;
            cur_x = circ.el[k].xa; cur_y = circ.el[k].ya;
            abre_formulario(k, 1);
        }
        return;
    }
    if ((t == T_IZQ || t == T_DER) && ti->nvar > 0) {
        /* otra variante: forma, modo o estado */
        int q_ant = campo_info(campo)->q, i;
        confirma_campo();
        if (aviso_r) return;
        {
            int era_ca = circ_alterna(&circ), antes = el->variante;
            el->variante = (unsigned char)((el->variante + (t == T_DER ? 1 : ti->nvar - 1)) % ti->nvar);
            if (antes == FORMA_FASOR && el->variante == ONDA_CC && el->val.est == M_DATO &&
                el->val.est_ang == M_DATO && fabs(fabs(el->val.ang) - 180.0) < 1e-9) {
                el->val.v = -el->val.v;                   /* 5∠180° vuelve a ser -5 V */
                el->val.ang = 0.0;
            }
            if (el->variante == FORMA_FASOR && (el->tipo == EL_FUENTE || el->tipo == EL_CORR)) {
                /* a la alterna: los datos de V e I se quedan; el ángulo vacío es
                 * 0° y la f del circuito, 60 Hz si no hay otra */
                if (el->val.est_ang == M_LIBRE) { el->val.ang = 0.0; el->val.est_ang = M_DATO; }
                if (el->val.est == M_DATO && el->val.v < 0.0) {       /* -5 V es 5∠180° */
                    el->val.v = -el->val.v;
                    el->val.ang = num_ang(el->val.ang + 180.0);
                }
                if (circ.frec.est != M_DATO && circ.frec.est != M_BUSCAR) { circ.frec.v = 60.0; circ.frec.est = M_DATO; }
                sub = 0;
                campo = 0;
                for (i = 0; i < ncampos(); i++) if (campo_info(i)->q == q_ant) { campo = i; break; }
                return;
            }
            if (era_ca && !circ_alterna(&circ) && circ_quita_ca(&circ))
                aviso_nb = "Ya no es CA: se quitan " S_aa "ngulos y [?!] de C y L.";
        }
        sub = 0;
        if ((el->tipo == EL_FUENTE || el->tipo == EL_CORR) && el->variante != ONDA_CC) {
            /* La calculadora es de CC: sin V ni I de datos; nada que buscar */
            int q2, quita = el->V.est != M_LIBRE || el->I.est != M_LIBRE;
            el->V.est = M_LIBRE; el->I.est = M_LIBRE;
            for (q2 = 0; q2 < Q_N; q2++) {
                Mag *m2 = circ_mag(el, q2);
                if (m2->est == M_BUSCAR) { m2->est = M_LIBRE; quita = 1; }
            }
            if (quita) aviso_nb = "Calculadora de CC: se quitan los datos de V e I.";
            if ((el->variante == ONDA_SENO || el->variante == ONDA_CUADRADA || el->variante == ONDA_TRIANGULAR) &&
                el->par[Q_FREC - Q_P1].est != M_DATO) {
                el->par[Q_FREC - Q_P1].v = 1000.0;          /* 1 kHz, como un generador */
                el->par[Q_FREC - Q_P1].est = M_DATO;
            }
        }
        campo = 0;
        for (i = 0; i < ncampos(); i++) if (campo_info(i)->q == q_ant) { campo = i; break; }
        return;
    }
    if (t == T_CATALOGO) { confirma_campo(); if (!aviso_r) pant = P_MODELO; return; }

    /* La fila x de una controlada: el control se elige con [+][-], no se
     * escribe ni se despeja; [del] lo quita. Las demás teclas, como siempre */
    if (campo_info(campo)->q == Q_CTRL) {
        if (t == T_MAS || t == T_MENOS) {
            int k = circ_control_sig(&circ, sel_el, t == T_MAS ? 1 : -1);
            if (k >= 0) el->ref = (signed char)k;
            else aviso_nb = "No hay otro elemento de dos bornes.";
            return;
        }
        if (t == T_BORRAR) { el->ref = -1; return; }
        if ((t >= T_DIG(0) && t <= T_DIG(9)) || t == T_PUNTO || t == T_EE || t == T_SIGNO ||
            (t >= T_POR && t <= T_PAR_C)) {
            aviso_nb = "El control se elige con [+][-].";
            return;
        }
        if (t == T_INCOG || t == T_LETRA('X')) { aviso_nb = "El control no se despeja: el" S_ii "gelo con [+][-]."; return; }
    }

    /* La alterna: el ángulo con [tab], la parte j de una Z */
    if (t == T_TAB && circ_alterna(&circ)) {
        if (fila_ang(campo)) { confirma_campo(); if (!aviso_r) sub = !sub; }
        else aviso_nb = "Este valor no tiene " S_aa "ngulo.";
        return;
    }
    if ((campo_info(campo)->flags & CF_SOLO_CA) && !circ_alterna(&circ) &&
        ((t >= T_DIG(0) && t <= T_DIG(9)) || t == T_PUNTO || t == T_PAR_A)) {
        aviso_nb = "Los datos del vat" S_ii "metro son de la CA.";
        return;
    }
    if (circ_alterna(&circ) && (campo_info(campo)->flags & CF_CX) && sub == 0 && t >= T_POR && t <= T_PAR_C) {
        aviso_nb = "Aqu" S_ii " [+] y [-] son la parte j: sin cuentas.";
        return;
    }
    if (circ_alterna(&circ) && (campo_info(campo)->flags & CF_CX) && sub == 0 &&
        (t == T_MAS || t == T_MENOS || t == T_LETRA('J'))) {
        size_t n = strlen(edit);
        int cifra = editando && n > 0 && ((edit[n - 1] >= '0' && edit[n - 1] <= '9') || strchr("pnumkMG\xB5", edit[n - 1]));
        if (strchr(edit, 'j')) { if (t == T_MENOS && n > 0 && edit[n - 1] == 'E') mete_char('-'); return; }
        if (t == T_MENOS && n > 0 && edit[n - 1] == 'E') { mete_char('-'); return; }
        if (t == T_LETRA('J') && (!editando || n == 0)) { mete_char('j'); return; }
        if (cifra) { mete_char(t == T_MENOS ? '-' : '+'); mete_char('j'); }
        return;
    }

    if (mete_cuenta(t)) return;                   /* + - × ÷ ^ x² ( ) en una cuenta */

    if (t == T_ARRIBA)      { confirma_campo(); if (aviso_r) return; sub = 0; if (campo > 0) campo--; }
    else if (t == T_ABAJO)  { confirma_campo(); if (aviso_r) return; sub = 0; if (campo < ncampos() - 1) campo++; }
    else if (t >= T_DIG(0) && t <= T_DIG(9)) mete_char((char)('0' + (t - T_DIG0)));
    else if (t == T_PUNTO || t == T_EE) {
        /* la parte j de "2,5+j1,5" lleva su propia coma y su propio exponente,
         * y cada número de una cuenta, los suyos (num_actual) */
        const char *p = num_actual();
        size_t n = strlen(edit);
        if (t == T_PUNTO) { if (!strchr(p, ',') && !strchr(p, 'E') && !(n && edit[n - 1] == ')')) mete_char(','); }
        else if (editando && *p && !(p[0] == 'j' && p[1] == 0) && !strchr(p, 'E')) mete_char('E');
    }
    else if (tecla_prefijo(t)) { if (editando && admite_prefijo(num_actual())) mete_char(tecla_prefijo(t)); }
    else if (t == T_SIGNO || t == T_MENOS) {
        /* Tras la E del exponente, el signo es el del exponente; al
         * principio, el del número, si el campo admite negativos */
        size_t n = strlen(edit);
        if (editando && n > 0 && edit[n - 1] == 'E') mete_char('-');
        else if ((!editando || n == 0) && fila_ang(campo)) { if (sub == 1) mete_char('-'); }   /* el módulo no tiene signo */
        else if ((!editando || n == 0) && con_signo(el, campo_info(campo)->q)) mete_char('-');
    }
    else if ((t == T_INCOG || t == T_LETRA('X')) && fila_ang(campo)) {
        /* en el módulo: el fasor entero (salvo un ángulo que es dato); en el ángulo, solo él */
        editando = 0; edit[0] = 0; aviso_r = 0;
        if (sub == 1) mg->est_ang = M_BUSCAR;
        else { mg->est = M_BUSCAR; if (mg->est_ang != M_DATO) mg->est_ang = M_BUSCAR; }
    }
    else if ((t == T_INCOG || t == T_LETRA('X')) && (campo_info(campo)->flags & CF_NO_BUSCAR) &&
             (campo_info(campo)->flags & CF_BUSCA_CA) && circ_alterna(&circ)) {
        editando = 0; edit[0] = 0; aviso_r = 0; mg->est = M_BUSCAR;         /* el valor de C o L, en CA */
    }
    else if ((t == T_INCOG || t == T_LETRA('X')) && (campo_info(campo)->flags & CF_NO_BUSCAR)) {
        /* un ajuste del emulador, o el valor de C o L: no se despeja */
        aviso_nb = campo_info(campo)->q == Q_VAL && el->tipo == EL_COND ? "En CC el condensador es un abierto: pide su V." :
                   campo_info(campo)->q == Q_VAL && el->tipo == EL_BOB  ? "En CC la bobina es un cable: pide su I." :
                   "Es un ajuste del emulador: no se despeja.";
    }
    else if (t == T_INCOG || t == T_LETRA('X')) { editando = 0; edit[0] = 0; aviso_r = 0; mg->est = M_BUSCAR; }
    else if (t == T_BORRAR) {
        if (editando && edit[0]) { edit[strlen(edit) - 1] = 0; aviso_r = 0; }   /* (el de una cuenta corregida) */
        else if (fila_ang(campo) && sub == 1) { editando = 0; mg->est_ang = M_LIBRE; aviso_r = 0; }
        else if (!ao_vp_vn_vale(&circ.el[sel_el], campo_info(campo)->q, M_LIBRE, 0.0)) { editando = 0; aviso_r = AV_VPVN; }
        else { editando = 0; mg->est = M_LIBRE; aviso_r = 0; }
    }
    else if (t == T_LETRA('F')) {
        circ_voltear(&circ, sel_el);
        if (circ_npatas(el) == 3) aviso_nb = texto_espejo(el);
    }
    else if (t == T_ENTER || t == T_ESC) {
        /* una cuenta rechazada se queda escrita: el segundo [esc] la tira */
        if (t == T_ESC && editando && aviso_r) { editando = 0; edit[0] = 0; aviso_r = 0; }
        if (!editando) aviso_r = 0;               /* el aviso de un valor ya rechazado no retiene */
        confirma_campo();
        if (!aviso_r) { sal_ficha(); pant = P_EDITOR; }
    }
    else if (t == T_IGUAL) {
        confirma_campo();
        if (aviso_r) return;
        sal_ficha();
        igual();
        /* igual() puede quedarse aquí con un aviso: la ficha sigue abierta */
        if (pant == P_ELEMENTO) { ficha_antes = circ; ficha_pend = 1; }
    }
}

/* ↑↓ en una lista de n: una fila; [shift] una página; [ctrl] al
 * principio o al final */
static int mueve_lista(int sel, int n, int t, int pagina)
{
    int sg = t == T_ABAJO ? 1 : -1;
    if (mods & T_CON_CTRL) sel = sg > 0 ? n - 1 : 0;
    else sel += sg * ((mods & T_CON_SHIFT) ? pagina : 1);
    if (sel > n - 1) sel = n - 1;
    if (sel < 0) sel = 0;
    return sel;
}

/* ↑↓ (y ←→ en la Ayuda: una página) sobre un scroll de 0 a max */
static void mueve_scroll(int t, int max, int pagina)
{
    int sg = t == T_ABAJO || t == T_DER ? 1 : -1;
    if (mods & T_CON_CTRL) scroll = sg > 0 ? max : 0;
    else scroll += sg * ((mods & T_CON_SHIFT) || t == T_IZQ || t == T_DER ? pagina : 1);
    if (scroll > max) scroll = max;
    if (scroll < 0) scroll = 0;
}

/* --------------------------------------------------------------------------
 *  Capacitores equivalentes (Taller, 4.ª opción): su menú, los datos (con
 *  cursor en los campos), el resultado en dos pestañas (Opciones y Paso a
 *  paso), el dibujo, la Teoría y los Ejemplos. El cálculo y los textos son
 *  de capmotor.c; el dibujo, de capmotor_dib.c; la Teoría y los Ejemplos,
 *  de capeq_texto.c
 * ----------------------------------------------------------------------- */
#define CPM_BW      50                /* columnas de la caja de la bodega       */
#define CPM_BVIS    4                 /* sus líneas a la vista                  */
#define CPM_BLMAX   (CPM_TXT + 2)     /* sus líneas, como mucho                 */
#define CPM_FIL_Y   (BARRA_H + 8)     /* la fila del valor (la tolerancia, 22 más abajo) */
#define CPM_CAJA_Y  (BARRA_H + 68)    /* la primera línea de la caja de la bodega */
#define CPM_NCAP_Y  (CPM_CAJA_Y + CPM_BVIS * LIN_H + 10)   /* la fila de «Capacitores» */
#define CPM_TAB_Y   (BARRA_H + 3)     /* las pestañas del resultado             */
#define CPM_RES_Y   (BARRA_H + 22)    /* bajo ellas: el veredicto, o el título del paso a paso */
#define CPM_PASO_Y  (CPM_RES_Y + LIN_H + 4)   /* el texto del paso a paso       */
#define CPM_VIS     ((PIE_Y - CPM_PASO_Y) / LIN_H)   /* sus líneas a la vista   */
#define CPM_TEO_VIS ((PIE_Y - BARRA_H - 6) / LIN_H)  /* las de una página de la Teoría */
#define CPM_IDX_VIS 12                /* las filas del índice de la Teoría      */
#define CPM_EJ_VIS  8                 /* las de la lista de ejemplos            */

static const char *const CPM_MODOS[3] = { "Auto", "2", "3" };
static int cpm_es_texto(int f) { return f == CPM_F_VALOR || f == CPM_F_TOL || f == CPM_F_BODEGA; }
static int cpm_cap(int f) { return f == CPM_F_BODEGA ? CPM_TXT : 12; }
static int cpm_modo_i(void) { return cpm_modo == 2 ? 1 : cpm_modo == 3 ? 2 : 0; }

/* ---- Las opciones del resultado ---- */
/* Las que se dibujan: las que valen (las que se enseñan) o, si no hay, lo
 * más cercano */
static int cpm_nopc(void) { return cpm_r.nval > 0 ? cpm_r.nval : cpm_r.hay_cerca; }
static const CpmComb *cpm_opcion(int i) { return cpm_r.nval > 0 ? &cpm_r.val[i] : &cpm_r.cerca; }

/* La lista de la pestaña Opciones: un título por cada número de capacitores
 * (o «Lo más cercano») y debajo sus opciones. cpm_fila_es(f): la opción de
 * la fila f, o -1 - c en el título del grupo de c capacitores (-1 el de lo
 * más cercano) */
static int cpm_nfilas(void)
{
    int c, n = 0;
    if (cpm_r.nval == 0) return cpm_r.hay_cerca ? 2 : 0;
    for (c = 1; c <= 3; c++) if (cpm_r.nver[c] > 0) n += 1 + cpm_r.nver[c];
    return n;
}
static int cpm_fila_es(int f)
{
    int c, base = 0;
    if (cpm_r.nval == 0) return f == 0 ? -1 : 0;
    for (c = 1; c <= 3; c++) {
        if (cpm_r.nver[c] == 0) continue;
        if (f == 0) return -1 - c;
        if (f <= cpm_r.nver[c]) return base + f - 1;
        f -= 1 + cpm_r.nver[c];
        base += cpm_r.nver[c];
    }
    return 0;
}
static int cpm_fila_de(int i)
{
    int c, f = 0;
    if (cpm_r.nval == 0) return 1;
    for (c = 1; c <= 3; c++) {
        if (cpm_r.nver[c] == 0) continue;
        if (i < cpm_r.nver[c]) return f + 1 + i;
        i -= cpm_r.nver[c];
        f += 1 + cpm_r.nver[c];
    }
    return f;
}

/* Las líneas del veredicto (una o dos) y dónde empieza la lista */
static int cpm_lista_y(void)
{
    char t[240];
    const char *p;
    int n = 1;
    cpm_veredicto(&cpm_r, t, sizeof t);
    for (p = t; *p; p++) if (*p == '\n') n++;
    return CPM_RES_Y + n * LIN_H + 4;
}
static int cpm_lvis(void) { return (PIE_Y - cpm_lista_y()) / LIN_H; }

/* La opción elegida, a la vista en la lista (con su título, si es la
 * primera de su grupo) */
static void cpm_lista_vista(void)
{
    int f, vis = cpm_lvis(), n = cpm_nfilas();
    if (cpm_nopc() == 0) { cpm_lsc = 0; return; }
    f = cpm_fila_de(cpm_sel);
    if (f < cpm_lsc) cpm_lsc = f;
    if (f > 0 && cpm_fila_es(f - 1) < 0 && f - 1 < cpm_lsc) cpm_lsc = f - 1;
    if (f >= cpm_lsc + vis) cpm_lsc = f - vis + 1;
    if (cpm_lsc > n - vis) cpm_lsc = n - vis;
    if (cpm_lsc < 0) cpm_lsc = 0;
}

static int cpm_max(void) { int m = cpm_nlin - CPM_VIS; return m > 0 ? m : 0; }

/* El paso a paso de la opción elegida, cortado en líneas, desde arriba */
static void cpm_prepara_paso(void)
{
    cpm_proced(&cpm_r, cpm_r.nval > 0 ? &cpm_r.val[cpm_sel] : 0, cpm_proc, sizeof cpm_proc);
    cpm_nlin = cpm_envuelve(cpm_proc, COLS, cpm_lin, CPM_LIN);
    scroll = 0;
}

/* Otra opción: a la vista en la lista, con su paso a paso */
static void cpm_elige(int i)
{
    int n = cpm_nopc();
    if (i > n - 1) i = n - 1;
    if (i < 0) i = 0;
    if (i == cpm_sel) return;
    cpm_sel = i;
    cpm_lista_vista();
    cpm_prepara_paso();
}

/* Un título que no cabe en max columnas pierde «en serie» (el CT, que es lo
 * que importa, se queda); con corta, si ni así cabe, se corta con «...» */
static void cpm_cabe(char *t, int max, int corta)
{
    char *p;
    if ((int)strlen(t) > max && (p = strstr(t, " en serie")) != 0) memmove(p, p + 9, strlen(p + 9) + 1);
    if (corta && (int)strlen(t) > max && max > 3) strcpy(t + max - 3, "...");
}

/* «Opción 2/5: 20 y 30 en serie = 12 MF», «Lo más cercano: ...». Lo que
 * cabe en la barra (51 columnas): con valores largos, sin «en serie» y, si
 * ni así, sin lo de delante */
static void cpm_rotulo(Buf *b, int i)
{
    char t[96], p[32];
    const CpmComb *k = cpm_opcion(i);
    Buf bp;
    cpm_titulo(&cpm_r, k, t, sizeof t);
    buf_init(&bp, p, sizeof p);
    if (cpm_r.nval > 0) { buf_str(&bp, "Opci" S_oo "n "); buf_int(&bp, i + 1); buf_ch(&bp, '/'); buf_int(&bp, cpm_r.nval); buf_str(&bp, ": "); }
    else buf_str(&bp, "Lo m" S_aa "s cercano: ");
    if (bp.n + strlen(t) > 51) cpm_cabe(t, 51 - (int)bp.n, 0);
    if (bp.n + strlen(t) > 51) p[0] = 0;                  /* (y si aún no cabe, lo corta la barra) */
    buf_str(b, p);
    buf_str(b, t);
}

/* ---- El menú: Resolver, Teoría y Ejemplos ---- */
static void dib_cpm_menu(void)
{
    static const char *const TIT[3] = { "Resolver", "Teor" S_ii "a", "Ejemplos" };
    static const char *const SUB[3][2] = {
        { "El valor, la bodega y Capacitores: las", "opciones, el paso a paso y el dibujo." },
        { "Motor, " S_oo "hmetro, paralelo o serie, tres", "capacitores, cableado y seguridad." },
        { "Los del PDF y m" S_aa "s, resueltos: elige uno", "y se cargan sus datos (editables)." },
    };
    int i;
    barra("Capacitores equivalentes", 0);
    for (i = 0; i < 3; i++) {
        int y = BARRA_H + 8 + i * 66, sel = i == cpm_msel;
        char n[2];
        n[0] = (char)('1' + i); n[1] = 0;
        gfx_fill(8, y, SCR_W - 16, 60, sel ? C_BARRA : C_PANEL);
        gfx_rect(8, y, SCR_W - 16, 60, sel ? C_CURSOR : C_TENUE);
        gfx_text(20, y + 8, n, sel ? C_CURSOR : C_COTA, 2);
        gfx_text(48, y + 8, TIT[i], sel ? C_CURSOR : C_TEXTO, 2);
        gfx_text(48, y + 34, SUB[i][0], sel ? C_TEXTO : C_SUAVE, 1);
        gfx_text(48, y + 46, SUB[i][1], sel ? C_TEXTO : C_SUAVE, 1);
    }
    pie("[arr/ab] elegir   [enter] entrar   [esc] Taller");
}

static void cpm_ajusta_caja(void);
static void cpm_entra_campo(int f);

static void cpm_abre(void)
{
    if (cpm_msel == 0) {
        /* Resolver: los datos del usuario (si se veían los de un ejemplo,
         * vuelven los suyos), en el valor; en la bodega, al final */
        cpm_av = CPM_AV_NADA;
        if (cpm_hay_mio) {
            memcpy(cpm_txt, cpm_mio, sizeof cpm_txt);
            cpm_modo = cpm_mio_modo;
            cpm_hay_mio = 0;
            cpm_av = CPM_AV_MIO;
        }
        pant = P_CPM; cpm_err = 0;
        cpm_campo = CPM_F_VALOR;
        cpm_bpos = (int)strlen(cpm_txt[CPM_F_BODEGA]);
        cpm_entra_campo(CPM_F_VALOR);
        cpm_ajusta_caja();
    }
    else if (cpm_msel == 1) pant = P_CPM_TEO;
    else pant = P_CPM_EJ;
}

static void tecla_cpm_menu(int t)
{
    if (t == T_ARRIBA && cpm_msel > 0) cpm_msel--;
    else if (t == T_ABAJO && cpm_msel < 2) cpm_msel++;
    else if (t >= T_DIG(1) && t <= T_DIG(3)) { cpm_msel = t - T_DIG(1); cpm_abre(); }
    else if (t == T_ENTER) cpm_abre();
    else if (t == T_ESC) { pant = P_TALLER; sel_taller = N_TALLER - 1; }
}

/* ---- Datos: el valor, la tolerancia, la bodega y «Capacitores» ---- */
/* Se entra en el campo f: el cursor al final; en el valor y la tolerancia,
 * la primera tecla escribe encima (el texto se ve resaltado, sin cursor).
 * En la bodega, donde estaba al salir de ella (al abrir Resolver, tras [A]
 * o [B] y con un ejemplo, al final) */
static void cpm_entra_campo(int f)
{
    int len = (int)strlen(cpm_txt[CPM_F_BODEGA]);
    if (cpm_campo == CPM_F_BODEGA) cpm_bpos = cpm_pos;
    cpm_campo = f;
    cpm_nuevo = 1;
    if (f == CPM_F_BODEGA) cpm_pos = cpm_bpos < 0 ? 0 : cpm_bpos > len ? len : cpm_bpos;
    else cpm_pos = cpm_es_texto(f) ? (int)strlen(cpm_txt[f]) : 0;
}

static int cpm_blineas(int *ini) { return cpm_lineas_campo(cpm_txt[CPM_F_BODEGA], CPM_BW, ini, CPM_BLMAX); }

/* La caja de la bodega enseña la línea del cursor (y, fuera de ella, la
 * última, donde se escribe) */
static void cpm_ajusta_caja(void)
{
    static int ini[CPM_BLMAX];
    int len = (int)strlen(cpm_txt[CPM_F_BODEGA]), nl = cpm_blineas(ini);
    int l = cpm_linea_de(ini, nl, len, cpm_campo == CPM_F_BODEGA ? cpm_pos : len, 0);
    if (l < cpm_btop) cpm_btop = l;
    if (l > cpm_btop + CPM_BVIS - 1) cpm_btop = l - CPM_BVIS + 1;
    if (cpm_btop > nl - CPM_BVIS) cpm_btop = nl - CPM_BVIS;
    if (cpm_btop < 0) cpm_btop = 0;
}

/* Adónde va el cursor con [izq] o [der] ([shift]: de valor en valor;
 * [ctrl]: al principio o al final) */
static int cpm_cursor_tras(int t, int m)
{
    const char *s = cpm_txt[cpm_campo];
    int len = (int)strlen(s), p = cpm_pos;
    if (m & T_CON_CTRL) return t == T_DER ? len : 0;
    if (m & T_CON_SHIFT) return cpm_salta_valor(s, p, t == T_DER ? 1 : -1);
    p += t == T_DER ? 1 : -1;
    return p < 0 ? 0 : p > len ? len : p;
}

/* [arr] o [ab] en la caja de la bodega: el cursor en la línea de arriba o
 * de abajo (en la misma columna, o al final de esa línea); -1 si no hay */
static int cpm_cursor_linea(int t)
{
    static int ini[CPM_BLMAX];
    int len = (int)strlen(cpm_txt[CPM_F_BODEGA]), nl = cpm_blineas(ini), col, l, fin;
    l = cpm_linea_de(ini, nl, len, cpm_pos, &col) + (t == T_ABAJO ? 1 : -1);
    if (l < 0 || l >= nl) return -1;
    fin = l + 1 < nl ? ini[l + 1] - 1 : len;
    return ini[l] + col < fin ? ini[l] + col : fin;
}

/* El cursor de texto, una «I» en el hueco entre dos caracteres: x es la
 * columna libre de la celda de la izquierda; y, la del texto */
static void cpm_cursor_i(int x, int y)
{
    gfx_fill(x, y - 1, 1, 13, C_CURSOR);
    gfx_fill(x - 1, y - 2, 3, 1, C_CURSOR);
    gfx_fill(x - 1, y + 12, 3, 1, C_CURSOR);
}

/* «15 capacitores: 1 2 3 4 5 6 7×2 10 12»: lo que cabe en dos líneas (con
 * « ...» si faltan) */
static void cpm_lectura(char *t, size_t cap, const double *v, int n)
{
    static double d[CPM_MAX];
    static int q[CPM_MAX], fin[CPM_MAX];
    Buf b;
    int nd = 0, i, m;
    for (i = 0; i < n; i++) {
        if (nd > 0 && v[i] == d[nd - 1]) q[nd - 1]++;
        else { d[nd] = v[i]; q[nd] = 1; nd++; }
    }
    buf_init(&b, t, cap);
    buf_int(&b, n); buf_str(&b, n == 1 ? " capacitor:" : " capacitores:");
    for (m = 0; m < nd && b.n <= 2 * COLS; m++) {
        char u[24];
        buf_ch(&b, ' '); buf_str(&b, cpm_num(u, sizeof u, d[m], 3));
        if (q[m] > 1) { buf_str(&b, S_TIMES); buf_int(&b, q[m]); }
        fin[m] = (int)b.n;
    }
    for (;;) {
        t[fin[m - 1]] = 0;
        if (m < nd) strcat(t, " ...");
        if (m == 1 || lineas_envuelve(t, COLS) <= 2) break;
        m--;
    }
}

static void dib_cpm(void)
{
    static const char *const NOM[2] = { "Valor", "Tolerancia " S_PM };
    static double v[CPM_MAX];
    static int ini[CPM_BLMAX];
    char t[CPM_TXT + 64];
    const char *s = cpm_txt[CPM_F_BODEGA];
    int i, y, n, e, nl, len, foco;
    barra("Capacitores equivalentes: datos", 0);
    for (i = 0; i < 2; i++) {                             /* el valor y la tolerancia */
        y = CPM_FIL_Y + i * 22;
        foco = i == cpm_campo;
        len = (int)strlen(cpm_txt[i]);
        gfx_text(8, y, NOM[i], foco ? C_CURSOR : C_COTA, 1);
        if (foco) gfx_rect(104, y - 3, 150, 17, C_CURSOR);
        if (foco && cpm_nuevo && len > 0) gfx_fill(108, y - 1, len * FONT_W + 3, 13, C_BARRA);   /* se escribe encima */
        gfx_text(110, y, cpm_txt[i], C_TEXTO, 1);
        if (foco && (!cpm_nuevo || len == 0)) cpm_cursor_i(110 + cpm_pos * FONT_W - 1, y);       /* el cursor (vacío: también) */
        gfx_text(260, y, "MF", C_COTA, 1);
        if (i == CPM_F_TOL && !len && !foco) gfx_text(110, y, "vac" S_ii "a = exacto", C_SUAVE, 1);
    }
    /* la bodega: su caja de cuatro líneas, desde cpm_btop */
    y = CPM_CAJA_Y - 16;
    foco = cpm_campo == CPM_F_BODEGA;
    gfx_text(8, y, "Bodega", foco ? C_CURSOR : C_COTA, 1);
    gfx_text(SCR_W - 6 - gfx_text_w("[A] o [B]: las del pizarr" S_oo "n", 1), y, "[A] o [B]: las del pizarr" S_oo "n", C_SUAVE, 1);
    gfx_rect(4, CPM_CAJA_Y - 3, SCR_W - 8, CPM_BVIS * LIN_H + 5, foco ? C_CURSOR : C_TENUE);
    len = (int)strlen(s);
    nl = cpm_lineas_campo(s, CPM_BW, ini, CPM_BLMAX);
    for (i = cpm_btop; i < nl && i < cpm_btop + CPM_BVIS; i++) {
        char lin[64];
        int fin = i + 1 < nl ? ini[i + 1] : len, k = fin - ini[i];
        if (k > 62) k = 62;
        memcpy(lin, s + ini[i], (size_t)k);
        lin[k] = 0;
        gfx_text(10, CPM_CAJA_Y + (i - cpm_btop) * LIN_H, lin, C_TEXTO, 1);
    }
    if (foco) {                                           /* el cursor */
        int col, l = cpm_linea_de(ini, nl, len, cpm_pos, &col);
        if (l >= cpm_btop && l < cpm_btop + CPM_BVIS) cpm_cursor_i(10 + col * FONT_W - 1, CPM_CAJA_Y + (l - cpm_btop) * LIN_H);
    }
    if (nl > CPM_BVIS) {                                  /* hay más líneas: la barrita */
        int alto = CPM_BVIS * LIN_H, h = alto * CPM_BVIS / nl, p = alto * cpm_btop / nl;
        gfx_fill(SCR_W - 3, CPM_CAJA_Y - 1 + p, 2, h < 6 ? 6 : h, C_SUAVE);
    }
    /* «Capacitores»: Auto, 2 o 3 */
    y = CPM_NCAP_Y;
    foco = cpm_campo == CPM_F_NCAP;
    gfx_text(8, y, "Capacitores", foco ? C_CURSOR : C_COTA, 1);
    {
        int x = 104;
        for (i = 0; i < 3; i++) {
            int w = gfx_text_w(CPM_MODOS[i], 1) + 10, sel = i == cpm_modo_i();
            if (sel) {
                gfx_fill(x, y - 3, w, 16, C_BARRA);
                gfx_rect(x, y - 3, w, 16, foco ? C_CURSOR : C_TENUE);
            }
            gfx_text(x + 5, y, CPM_MODOS[i], sel ? C_TEXTO : C_SUAVE, 1);
            x += w + 8;
        }
        gfx_text(x + 4, y, cpm_modo == 2 ? "solo de dos" : cpm_modo == 3 ? "solo de tres" : "con 1, 2 o 3", C_SUAVE, 1);
    }
    /* lo que se lee de la bodega, o por qué no */
    y = CPM_NCAP_Y + 20;
    e = cpm_lee_bodega(s, v, &n);
    if (e != CPM_OK) strcpy(t, cpm_error(e));
    else if (n == 0) strcpy(t, "Vac" S_ii "a: no hay con qu" S_ee ".");
    else cpm_lectura(t, sizeof t, v, n);
    y += envuelve(8, y, COLS, t, e != CPM_OK ? C_ERROR : C_TITULO, 2) * LIN_H + 3;
    if (cpm_err && cpm_err != e) envuelve(8, y, COLS, cpm_error(cpm_err), C_ERROR, 2);   /* el de la bodega ya se lee arriba */
    else if (cpm_av)
        envuelve(8, y, COLS, cpm_av == CPM_AV_A ? "La bodega A del pizarr" S_oo "n (la izquierda)." :
                             cpm_av == CPM_AV_B ? "La bodega B del pizarr" S_oo "n: del 15 al 42." :
                             cpm_av == CPM_AV_EJ ? "Los datos del ejemplo: c" S_aa "mbialos y [enter].\n"
                                                   "Los tuyos vuelven al abrir Resolver desde el men" S_uu "." :
                             cpm_av == CPM_AV_MIO ? "Tus datos (los de antes del ejemplo)." : "No caben m" S_aa "s.",
                 cpm_av == CPM_AV_LLENO ? C_ERROR : C_OK, 2);
    else if (cpm_campo == CPM_F_BODEGA)
        envuelve(8, y, COLS, "Rangos: 15-42. Quitar: -21-24 o sin 21-24.\n"
                             "[shift][izq/der] salta de valor; [ctrl], al extremo.", C_SUAVE, 2);
    else if (cpm_campo == CPM_F_NCAP)
        envuelve(8, y, COLS, "Auto: primero los de menos capacitores, luego el\n"
                             "m" S_aa "s cercano. 2 o 3: solo si el profe lo pide.", C_SUAVE, 2);
    else
        envuelve(8, y, COLS, "Decimales con [.]. [izq/der] mueven el cursor.\n"
                             "[del] borra; [ctrl][del], todo el campo.", C_SUAVE, 2);
    pie(cpm_campo == CPM_F_NCAP ? "[izq/der] Auto, 2 o 3   [enter] resolver   [esc]"
                                : "[arr/ab] campo [izq/der] cursor [enter] resolver");
}

/* [enter]: lee los campos y resuelve; si algo no vale, lo dice y se va a
 * ese campo */
static void cpm_resolver(void)
{
    static double v[CPM_MAX];
    double C = 0.0, tol = 0.0;
    int n, e;
    cpm_av = CPM_AV_NADA;
    e = cpm_lee_valor(cpm_txt[CPM_F_VALOR], &C);
    if (e != CPM_OK) { cpm_err = e; cpm_entra_campo(CPM_F_VALOR); return; }
    e = cpm_lee_tol(cpm_txt[CPM_F_TOL], &tol);
    if (e == CPM_OK && !(tol < C)) e = CPM_E_TOL;
    if (e != CPM_OK) { cpm_err = e; cpm_entra_campo(CPM_F_TOL); return; }
    e = cpm_lee_bodega(cpm_txt[CPM_F_BODEGA], v, &n);
    if (e != CPM_OK) { cpm_err = e; cpm_entra_campo(CPM_F_BODEGA); return; }
    cpm_err = 0;
    cpm_resuelve_n(&cpm_r, C, tol, cpm_modo, v, n);
    cpm_sel = 0; cpm_tab = 0; cpm_lsc = 0;
    cpm_prepara_paso();
    pant = P_CPM_RES;
}

static void tecla_cpm(int t)
{
    char *s, ch = 0;
    int n;
    if (t == T_ESC) { pant = P_CPM_MENU; cpm_msel = 0; return; }
    if (t == T_ENTER || t == T_IGUAL) { cpm_resolver(); cpm_ajusta_caja(); return; }
    if (t == T_TAB) { cpm_entra_campo((cpm_campo + 1) % CPM_NF); cpm_ajusta_caja(); return; }   /* el siguiente, con vuelta */
    if (t == T_ARRIBA || t == T_ABAJO) {                  /* de línea en línea en la bodega; si no, de campo */
        int p = cpm_campo == CPM_F_BODEGA ? cpm_cursor_linea(t) : -1, c = cpm_campo + (t == T_ABAJO ? 1 : -1);
        if (p >= 0) { cpm_pos = p; cpm_nuevo = 0; }
        else if (c >= 0 && c < CPM_NF) cpm_entra_campo(c);
        cpm_ajusta_caja();
        return;
    }
    if ((t == T_LETRA('A') || t == T_LETRA('B')) && !(mods & T_CON_CTRL)) {   /* las del pizarrón */
        strcpy(cpm_txt[CPM_F_BODEGA], t == T_LETRA('A') ? CPM_BODEGA_A : CPM_BODEGA_B);
        cpm_av = t == T_LETRA('A') ? CPM_AV_A : CPM_AV_B;
        cpm_err = 0;
        cpm_bpos = (int)strlen(cpm_txt[CPM_F_BODEGA]);    /* al final, también al volver a ella */
        if (cpm_campo == CPM_F_BODEGA) cpm_pos = cpm_bpos;
        cpm_ajusta_caja();
        return;
    }
    if (cpm_campo == CPM_F_NCAP) {                        /* «Capacitores»: Auto, 2 o 3 (sin vuelta) */
        static const int M[3] = { 0, 2, 3 };
        int i = cpm_modo_i();
        if (t == T_IZQ && i > 0) i--;
        else if (t == T_DER && i < 2) i++;
        else if (t == T_DIG(2) || t == T_DIG(3)) i = t == T_DIG(2) ? 1 : 2;
        else if (t == T_DIG(0)) i = 0;
        else return;
        cpm_modo = M[i]; cpm_err = 0; cpm_av = CPM_AV_NADA;
        return;
    }
    s = cpm_txt[cpm_campo];
    n = (int)strlen(s);
    if (cpm_pos > n) cpm_pos = n;
    if (t == T_IZQ || t == T_DER) {                       /* el cursor */
        cpm_pos = cpm_cursor_tras(t, mods);
        cpm_nuevo = 0;
        cpm_ajusta_caja();
        return;
    }
    if (t == T_BORRAR) {                                  /* a la izquierda del cursor; [ctrl]: el campo entero */
        if (mods & T_CON_CTRL) { s[0] = 0; cpm_pos = 0; }
        else if (cpm_pos > 0) { memmove(s + cpm_pos - 1, s + cpm_pos, (size_t)(n - cpm_pos + 1)); cpm_pos--; }
        cpm_nuevo = 0; cpm_err = 0; cpm_av = CPM_AV_NADA;
        cpm_ajusta_caja();
        return;
    }
    if (mods & T_CON_CTRL) return;
    if (t >= T_DIG(0) && t <= T_DIG(9)) ch = (char)('0' + (t - T_DIG0));
    else if (t == T_PUNTO) ch = '.';
    else if (cpm_campo == CPM_F_BODEGA) {                 /* los separadores, los rangos y «sin» */
        if (t == T_ESPACIO) ch = ' ';
        else if (t == T_MENOS || t == T_SIGNO) ch = '-';
        else if (t == T_LETRA('S')) ch = 's';
        else if (t == T_LETRA('I')) ch = 'i';
        else if (t == T_LETRA('N')) ch = 'n';
    }
    if (!ch) return;
    if (cpm_nuevo && cpm_campo != CPM_F_BODEGA) { s[0] = 0; n = 0; cpm_pos = 0; }   /* el valor se escribe encima */
    cpm_nuevo = 0; cpm_err = 0; cpm_av = CPM_AV_NADA;
    if (n < cpm_cap(cpm_campo)) {                         /* se inserta en el cursor */
        memmove(s + cpm_pos + 1, s + cpm_pos, (size_t)(n - cpm_pos + 1));
        s[cpm_pos++] = ch;
    } else cpm_av = CPM_AV_LLENO;
    cpm_ajusta_caja();
}

/* ---- Resultado: las pestañas Opciones y Paso a paso ---- */
static u16 cpm_color(int i)
{
    int p = cpm_lin[i].ini;
    while (p > 0 && cpm_proc[p - 1] != '\n') p--;         /* el principio de su línea del texto */
    if (cpm_proc[p] >= '0' && cpm_proc[p] <= '9') return C_TITULO;
    if (!strncmp(cpm_proc + p, "IRRES", 5) || !strncmp(cpm_proc + p, "Lo m", 4)) return C_ERROR;
    return C_TEXTO;
}

/* El título del grupo de c capacitores en la lista: «Con 2 capacitores:»,
 * «Con 3: las 20 más cercanas de 345:» */
static void cpm_titulo_grupo(char *t, size_t cap, int c)
{
    Buf b;
    buf_init(&b, t, cap);
    if (c <= 0) { buf_str(&b, "Lo m" S_aa "s cercano (no entra):"); return; }
    buf_str(&b, "Con "); buf_int(&b, c);
    if (cpm_r.nver[c] >= cpm_r.tot[c]) { buf_str(&b, c == 1 ? " capacitor:" : " capacitores:"); return; }
    buf_str(&b, ": las "); buf_int(&b, cpm_r.nver[c]); buf_str(&b, " m" S_aa "s cercanas de "); buf_int(&b, cpm_r.tot[c]);
    buf_ch(&b, ':');
}

static void dib_cpm_res(void)
{
    static const char *const TAB[2] = { "Opciones", "Paso a paso" };
    char t[240], u[120];
    Buf b;
    int i, x, nop = cpm_nopc();
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Capacitores equivalentes: "); buf_str(&b, cpm_num(u, sizeof u, cpm_r.C, 3));
    if (cpm_r.tol > 0.0) { buf_str(&b, " " S_PM " "); buf_str(&b, cpm_num(u, sizeof u, cpm_r.tol, 3)); }
    buf_str(&b, " MF");
    barra(t, cpm_r.modo == 2 ? "con 2" : cpm_r.modo == 3 ? "con 3" : "Auto");
    /* las pestañas, y cuántas hay */
    for (i = 0, x = 6; i < 2; i++) {
        int w = gfx_text_w(TAB[i], 1) + 12;
        if (i == cpm_tab) { gfx_fill(x, CPM_TAB_Y - 2, w, 15, C_BARRA); gfx_rect(x, CPM_TAB_Y - 2, w, 15, C_CURSOR); }
        gfx_text(x + 6, CPM_TAB_Y, TAB[i], i == cpm_tab ? C_CURSOR : C_SUAVE, 1);
        x += w + 6;
    }
    buf_init(&b, u, sizeof u);
    if (cpm_r.ntot > 0) { buf_int(&b, cpm_r.ntot); buf_str(&b, cpm_r.ntot == 1 ? " opci" S_oo "n" : " opciones"); }
    else buf_str(&b, "ninguna");
    gfx_text(SCR_W - 6 - gfx_text_w(u, 1), CPM_TAB_Y, u, C_SUAVE, 1);
    separador(CPM_TAB_Y + 14);
    if (cpm_tab == 0) {
        /* el veredicto (una o dos líneas) y la lista */
        int ly = cpm_lista_y(), vis = cpm_lvis(), nf = cpm_nfilas(), sc = cpm_lsc, k, y = CPM_RES_Y;
        const char *p = t, *q;
        cpm_veredicto(&cpm_r, t, sizeof t);
        for (k = 0; *p; k++, y += LIN_H) {
            char lin[160];
            size_t m;
            u16 c = cpm_r.nval == 0 ? C_ERROR : (cpm_r.modo == CPM_AUTO && cpm_ncap(cpm_r.val[0].tipo) == 3 && k == 0) ? C_CURSOR : C_OK;
            q = strchr(p, '\n');
            m = q ? (size_t)(q - p) : strlen(p);
            if (m > sizeof lin - 1) m = sizeof lin - 1;
            memcpy(lin, p, m);
            lin[m] = 0;
            cpm_cabe(lin, COLS - 1, 1);
            gfx_text(6, y, lin, c, 1);
            p = q ? q + 1 : p + m;
        }
        separador(ly - 3);
        if (sc > nf - vis) sc = nf - vis;
        if (sc < 0) sc = 0;
        for (i = 0; i < vis && sc + i < nf; i++) {
            int f = sc + i, o = cpm_fila_es(f);
            y = ly + i * LIN_H;
            if (o < 0) {
                cpm_titulo_grupo(t, sizeof t, -1 - o);
                gfx_text(6, y, t, C_TITULO, 1);
                continue;
            }
            /* «12. mixto: (40 ∥ 42) y 31 = 22.496 MF (dif. -0.004)»: si no
             * cabe, pierde « MF», el tipo y «dif. », nunca el CT ni la dif. */
            cpm_fila_lista(&cpm_r, cpm_opcion(o), o + 1, COLS - 3, t, sizeof t);
            if (o == cpm_sel) {
                gfx_fill(0, y - 1, SCR_W - 4, LIN_H, C_BARRA);
                gfx_text(4, y, S_RARR, C_CURSOR, 1);
            }
            gfx_text(16, y, t, o == cpm_sel ? C_TEXTO : C_COTA, 1);
        }
        if (nf > vis) {                                   /* hay más: la barrita */
            int alto = vis * LIN_H, h = alto * vis / nf, pp = alto * sc / nf;
            gfx_fill(SCR_W - 3, ly + pp, 2, h < 6 ? 6 : h, C_SUAVE);
        }
        if (cpm_r.nval == 0)                              /* el irresoluble: dónde está el porqué */
            envuelve(8, ly + (nf + 1) * LIN_H, COLS, cpm_con_faltas(&cpm_r) ?
                     "El porqu" S_ee " y qu" S_ee " te faltar" S_ii "a: en Paso a paso ([der])." :
                     "El porqu" S_ee ": en Paso a paso ([der]).", C_SUAVE, 2);
        pie(nop > 0 ? "[arr/ab] elegir [enter] dibujo [der] pasos [esc]" : "[der] el paso a paso                  [esc] datos");
        return;
    }
    /* el paso a paso de la opción elegida */
    {
        int tot = cpm_nlin, sc = scroll;
        buf_init(&b, t, sizeof t);
        if (nop > 0) cpm_rotulo(&b, cpm_sel);
        else buf_str(&b, cpm_r.n == 0 ? "La bodega est" S_aa " vac" S_ii "a." : "No hay ninguna opci" S_oo "n.");
        cpm_cabe(t, COLS - 1, 1);
        gfx_text(6, CPM_RES_Y, t, cpm_r.nval > 0 ? C_OK : C_ERROR, 1);
        separador(CPM_PASO_Y - 3);
        if (sc > cpm_max()) sc = cpm_max();
        for (i = 0; i < CPM_VIS && sc + i < tot; i++) {
            int f = sc + i;
            char lin[64];
            memcpy(lin, cpm_proc + cpm_lin[f].ini, cpm_lin[f].len);
            lin[cpm_lin[f].len] = 0;
            gfx_text(6 + cpm_lin[f].sangria * FONT_W, CPM_PASO_Y + i * LIN_H, lin, cpm_color(f), 1);
        }
        if (tot > CPM_VIS) {                              /* hay más: la barrita */
            int alto = CPM_VIS * LIN_H, h = alto * CPM_VIS / tot, pp = alto * sc / tot;
            gfx_fill(SCR_W - 3, CPM_PASO_Y + pp, 2, h < 6 ? 6 : h, C_SUAVE);
        }
        pie(nop > 0 ? "[arr/ab] leer [enter] dibujo [izq] opciones [esc]" : "[arr/ab] leer   [izq] opciones   [esc] datos");
    }
}

static void tecla_cpm_res(int t)
{
    int nop = cpm_nopc();
    if (t == T_ESC) { pant = P_CPM; cpm_entra_campo(cpm_campo); cpm_ajusta_caja(); return; }   /* el siguiente: el valor se escribe encima */
    if (t == T_TAB) { cpm_tab = !cpm_tab; return; }
    if (t == T_IZQ || t == T_DER) { cpm_tab = t == T_DER; return; }
    if (t == T_ENTER) { if (nop > 0) pant = P_CPM_DIB; return; }
    if (t != T_ARRIBA && t != T_ABAJO) return;
    if (cpm_tab == 1) { mueve_scroll(t, cpm_max(), CPM_VIS); return; }
    if (nop == 0) return;
    if (mods & T_CON_CTRL) cpm_elige(t == T_ABAJO ? nop - 1 : 0);
    else cpm_elige(cpm_sel + (t == T_ABAJO ? 1 : -1) * ((mods & T_CON_SHIFT) ? cpm_lvis() : 1));
}

/* ---- Dibujo de la opción elegida; las flechas, las demás ---- */
static void dib_cpm_dib(void)
{
    char t[120];
    Buf b;
    int nop = cpm_nopc();
    if (nop == 0) {                                       /* (no se llega: [enter] lo mira) */
        barra("Capacitores equivalentes", 0);
        pie("[esc] al resultado");
        return;
    }
    buf_init(&b, t, sizeof t);
    cpm_rotulo(&b, cpm_sel);
    barra(t, 0);
    cpm_dibuja(&cpm_r, cpm_opcion(cpm_sel), BARRA_H + 1, PIE_Y, cpm_r.nval == 0);
    pie(nop > 1 ? "[izq/der] o [arr/ab] otra opci" S_oo "n   [esc] volver" : "[esc] al resultado");
}

static void tecla_cpm_dib(int t)
{
    int nop = cpm_nopc(), d = t == T_DER || t == T_ABAJO ? 1 : -1;
    if (t == T_ESC || t == T_ENTER) { cpm_lista_vista(); pant = P_CPM_RES; return; }
    if (t != T_IZQ && t != T_DER && t != T_ARRIBA && t != T_ABAJO) return;
    if (mods & T_CON_CTRL) cpm_elige(d > 0 ? nop - 1 : 0);
    else cpm_elige(cpm_sel + d);
}

/* ---- Teoría: el índice y cada página (capeq_texto.c) ---- */
static int cpm_teo_n(void) { int n = cpq_teoria_n(); return n < 0 ? 0 : n > 40 ? 40 : n; }
static int cpm_teo_lineas(void) { return lineas_de(cpq_teoria_texto(cpm_tsel)); }
static int cpm_teo_max(void) { int m = cpm_teo_lineas() - CPM_TEO_VIS; return m > 0 ? m : 0; }

static void dib_cpm_teo(void)
{
    static char nom[40][48];
    static const char *items[40];
    Buf b;
    int i, n = cpm_teo_n(), y = BARRA_H + 8 + CPM_IDX_VIS * LIN_H + 6;
    barra("Capacitores equivalentes: teor" S_ii "a", 0);
    for (i = 0; i < n; i++) {
        buf_init(&b, nom[i], sizeof nom[i]);
        buf_int(&b, i + 1); buf_str(&b, ". "); buf_str(&b, cpq_teoria_titulo(i));
        items[i] = nom[i];
    }
    lista(BARRA_H + 8, CPM_IDX_VIS, items, n, cpm_tsel, cpm_tsel >= CPM_IDX_VIS ? cpm_tsel - CPM_IDX_VIS + 1 : 0);
    separador(y - 3);
    envuelve(8, y, COLS, "[enter] abre la p" S_aa "gina; dentro, [arr/ab] la lee y\n"
                         "[izq/der] pasa a la anterior o a la siguiente.", C_SUAVE, 2);
    pie("[arr/ab] elegir    [enter] leer    [esc] men" S_uu);
}

static void tecla_cpm_teo(int t)
{
    int n = cpm_teo_n();
    if (t == T_ARRIBA && cpm_tsel > 0) cpm_tsel--;
    else if (t == T_ABAJO && cpm_tsel < n - 1) cpm_tsel++;
    else if (t == T_ENTER && n > 0) { pant = P_CPM_PAG; scroll = 0; }
    else if (t == T_ESC) { pant = P_CPM_MENU; cpm_msel = 1; }
}

static void dib_cpm_pag(void)
{
    const char *s = cpq_teoria_texto(cpm_tsel);
    char t[96];
    Buf b;
    int i, n = cpm_teo_lineas(), sc = scroll;
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Teor" S_ii "a "); buf_int(&b, cpm_tsel + 1); buf_ch(&b, '/'); buf_int(&b, cpm_teo_n());
    buf_str(&b, ": "); buf_str(&b, cpq_teoria_titulo(cpm_tsel));
    barra(t, 0);
    if (sc > cpm_teo_max()) sc = cpm_teo_max();
    if (sc < 0) sc = 0;
    for (i = 0; i < sc; i++) { while (*s && *s != '\n') s++; if (*s) s++; }
    for (i = 0; i < CPM_TEO_VIS && *s; i++) {
        char lin[64];
        int k = 0;
        while (s[k] && s[k] != '\n' && k < COLS) k++;
        memcpy(lin, s, (size_t)k);
        lin[k] = 0;
        gfx_text(8, BARRA_H + 4 + i * LIN_H, lin, C_TEXTO, 1);
        while (*s && *s != '\n') s++;
        if (*s) s++;
    }
    if (n > CPM_TEO_VIS) {                                /* hay más: la barrita */
        int alto = CPM_TEO_VIS * LIN_H, h = alto * CPM_TEO_VIS / n, p = alto * sc / n;
        gfx_fill(SCR_W - 3, BARRA_H + 4 + p, 2, h < 6 ? 6 : h, C_SUAVE);
    }
    pie("[arr/ab] leer   [izq/der] p" S_aa "gina   [esc] " S_ii "ndice");
}

static void tecla_cpm_pag(int t)
{
    int n = cpm_teo_n();
    if (t == T_ESC) { pant = P_CPM_TEO; return; }
    if (t == T_ARRIBA || t == T_ABAJO) { mueve_scroll(t, cpm_teo_max(), CPM_TEO_VIS); return; }
    if (t == T_IZQ && cpm_tsel > 0) { cpm_tsel--; scroll = 0; }
    else if (t == T_DER && cpm_tsel < n - 1) { cpm_tsel++; scroll = 0; }
}

/* ---- Ejemplos: al elegir uno se cargan sus datos y se resuelve ---- */
static int cpm_ej_n(void) { int n = cpq_ejemplos_n(); return n < 0 ? 0 : n > 40 ? 40 : n; }

static void dib_cpm_ej(void)
{
    static char nom[40][48];
    static const char *items[40];
    const CpqEjemplo *e = cpq_ejemplo(cpm_esel);
    char t[CPM_TXT + 40], u[32];
    Buf b;
    int i, n = cpm_ej_n(), y = BARRA_H + 8 + CPM_EJ_VIS * LIN_H + 6;
    barra("Capacitores equivalentes: ejemplos", 0);
    for (i = 0; i < n; i++) {
        buf_init(&b, nom[i], sizeof nom[i]);
        buf_int(&b, i + 1); buf_str(&b, ". "); buf_str(&b, cpq_ejemplo(i)->titulo);
        items[i] = nom[i];
    }
    lista(BARRA_H + 8, CPM_EJ_VIS, items, n, cpm_esel, cpm_esel >= CPM_EJ_VIS ? cpm_esel - CPM_EJ_VIS + 1 : 0);
    separador(y - 3);
    if (n > 0) {
        y += envuelve(8, y, COLS, e->ensena, C_TEXTO, 2) * LIN_H + 4;
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Datos: "); buf_str(&b, e->valor);
        if (e->tol[0]) { buf_str(&b, " " S_PM " "); buf_str(&b, e->tol); }
        buf_str(&b, " MF; Capacitores: ");
        if (e->n == 2 || e->n == 3) { cpm_num(u, sizeof u, (double)e->n, 0); buf_str(&b, u); }
        else buf_str(&b, "Auto");
        buf_ch(&b, '\n');
        buf_str(&b, "Bodega: "); buf_str(&b, e->bodega);
        envuelve(8, y, COLS, t, C_SUAVE, 3);
    }
    pie("[arr/ab] elegir   [enter] resolver   [esc] men" S_uu);
}

/* Carga los datos del ejemplo elegido en los campos y resuelve (si algo no
 * valiera, se queda en los datos con el aviso) */
static void cpm_carga_ejemplo(void)
{
    const CpqEjemplo *e = cpq_ejemplo(cpm_esel);
    const char *src[3];
    int f;
    if (!cpm_hay_mio) {                                   /* lo del usuario, para devolvérselo (Resolver) */
        memcpy(cpm_mio, cpm_txt, sizeof cpm_mio);
        cpm_mio_modo = cpm_modo;
        cpm_hay_mio = 1;
    }
    src[0] = e->valor; src[1] = e->tol; src[2] = e->bodega;
    for (f = 0; f < 3; f++) {
        size_t m = strlen(src[f]), cap = (size_t)cpm_cap(f);
        if (m > cap) m = cap;
        memcpy(cpm_txt[f], src[f], m);
        cpm_txt[f][m] = 0;
    }
    cpm_modo = e->n == 2 || e->n == 3 ? e->n : 0;
    pant = P_CPM;
    cpm_campo = CPM_F_VALOR;
    cpm_bpos = (int)strlen(cpm_txt[CPM_F_BODEGA]);
    cpm_entra_campo(CPM_F_VALOR);
    cpm_resolver();
    if (pant == P_CPM_RES) cpm_av = CPM_AV_EJ;
    cpm_ajusta_caja();
}

static void tecla_cpm_ej(int t)
{
    int n = cpm_ej_n();
    if (t == T_ARRIBA && cpm_esel > 0) cpm_esel--;
    else if (t == T_ABAJO && cpm_esel < n - 1) cpm_esel++;
    else if (t == T_ENTER && n > 0) cpm_carga_ejemplo();
    else if (t == T_ESC) { pant = P_CPM_MENU; cpm_msel = 2; }
}

static int tecla(int t)
{
    const Leccion *l = &LECCIONES[sel_lec];

    /* Los modificadores: los de la tecla y los armados; una repetición lleva
     * los del primer evento de su ráfaga y un paso del dedo, los que están
     * abajo (ni gastan ni arman los pegajosos) */
    rep_act = t & T_MARCAS;
    if (t & T_TACTIL) {
        mods = t & T_MODS;
        if (!(rep_ant & T_TACTIL)) hist_rafaga = 0;      /* el primer paso de un gesto */
    }
    else if (t & T_REPE) mods = mods_rafaga;
    else {
        /* shift y ctrl: se pulsan, se sueltan y acompañan a la tecla siguiente
         * (en la Nspire es lo normal); si van a la vez, la capa ya los trae */
        if (t == T_SHIFT || t == T_CTRL) {
            int m = t == T_SHIFT ? T_CON_SHIFT : T_CON_CTRL;
            /* Ahí no sirven: emulando y con el banco midiendo, ninguno; en
             * la portada y el banco ya hecho, solo [ctrl], para [ctrl][menu]
             * (Controles «desde cualquier pantalla»: la tecla siguiente lo
             * gasta, sea cual sea) */
            if (pant == P_EMULANDO || (pant == P_BANCO && !banco_hecho())) return 1;
            if (m == T_CON_SHIFT && (pant == P_INICIO || ESP_SIN_SHIFT
                                     pant == P_BANCO)) return 1;
            mod_pend = (mod_pend & m) ? (mod_pend & ~m) : (mod_pend | m);
            return 1;
        }
        mods = (t & T_MODS) | mod_pend;
        mod_pend = 0;
        mods_rafaga = mods;
        hist_rafaga = 0;
    }
    t &= ~(T_MODS | T_MARCAS);
    /* la lectura del Bode y del barrido: de la malla («≈») mientras corre una
     * ráfaga; la exacta con la primera tecla o con T_SUELTA */
    curv.rafaga = traz.rafaga = 0;
    if (rep_act && pant == P_BARRIDO) curv.rafaga = 1;
    if (rep_act && pant == P_BODE) traz.rafaga = 1;

    /* [ctrl][menu], desde cualquier pantalla (menos con trabajo en marcha):
     * el panel Controles */
    if (t == T_MENU && (mods & T_CON_CTRL) && !rep_act && pant != P_CONTROLES && pant != P_EMULANDO &&
        !(pant == P_BANCO && !banco_hecho())) {
        ctl_origen = pant;
        ctl_campo = 0;
        pant = P_CONTROLES;
        return 1;
    }

    switch (pant) {

    case P_CONTROLES: tecla_controles(t); return 1;

    case P_INICIO:
        ESP_PORTADA(t)
        previa = (t >= T_DIG(0) && t <= T_DIG(9)) ? t - T_DIG(0) : -1;
        if (t == T_ENTER) pant = P_MENU;
        else if (t == T_ESC) return 0;
        return 1;

    ESP_TECLA(t)

    case P_MODELO:
        if (t == T_ESC || t == T_ENTER || t == T_CATALOGO) pant = P_ELEMENTO;
        else if (t == T_LETRA('I') && sel_el >= 0 && tiene_curva(circ.el[sel_el].tipo)) curva_de(P_MODELO, &circ, sel_el);
        return 1;

    case P_EMULANDO:  tecla_emulando(t);  return 1;
    case P_OSC:       tecla_osc(t);       return 1;
    case P_EMU_TABLA: tecla_emu_tabla(t); return 1;
    case P_EMU_ESQ:   tecla_emu_esq(t);   return 1;
    case P_AJUSTES:   tecla_ajustes(t);   return 1;
    case P_CA:        return tecla_ca(t);
    case P_BODE:      return tecla_bode(t);
    case P_FASORIAL:  return tecla_fasorial(t);
    case P_CA_TABLA:  return tecla_ca_tabla(t);
    case P_THEVENIN:  return tecla_thevenin(t);
    case P_BARRIDO:   return tecla_barrido(t);

    case P_BANCO:
        if (t == T_ESC) pant = P_AYUDA;
        else if (t == T_ENTER && banco_hecho()) banco_reinicia();
        else if (t == T_IZQ || t == T_DER) banco_pagina(t == T_DER ? 1 : -1);   /* con la 2.ª página */
        return 1;

    case P_MENU:
        if (t == T_ARRIBA && sel_menu > 0) sel_menu--;
        else if (t == T_ABAJO && sel_menu < 2) sel_menu++;
        else if (t == T_ENTER) {
            if (sel_menu == 0) { pant = P_TALLER; sel_taller = circ.nel > 0 ? 1 : 0; }
            else if (sel_menu == 1) pant = P_LECCIONES;
            else abre_ayuda();
        }
        else if (t == T_ESC) pant = P_INICIO;
        return 1;

    case P_TALLER:
        if (t == T_ARRIBA && sel_taller > 0) sel_taller--;
        else if (t == T_ABAJO && sel_taller < n_taller() - 1) sel_taller++;
        else if (t == T_ENTER && sel_taller == 3) {
            /* los capacitores equivalentes: su menú (Resolver, Teoría y
             * Ejemplos); los datos, con lo de la última vez (o el 17 de la
             * bodega A) */
            pant = P_CPM_MENU; cpm_msel = 0;
        }
        else if (t == T_ENTER) {
            if (sel_taller == 0) {
                if (nuevo_circuito()) pon_aviso("Problema nuevo. [ctrl][Z] recupera el anterior.", C_CURSOR);
                pant = P_EDITOR;
            }
            else if (sel_taller == 1) {
                aviso[0] = 0; ctx_abierto = 0; pant = P_EDITOR;
                if (ses_aviso) pon_aviso("Recuperado de la " S_uu "ltima vez: [ctrl][S] lo guarda.", C_OK);
                ses_aviso = 0;
            }
            else { carga_lista_mis(); scroll = 0; pant = P_MIS; }
        }
        else if (t == T_ESC) pant = P_MENU;
        break;

    case P_MIS:
        if (nombrando == NOM_MIS) { tecla_nombre_mis(t); return 1; }   /* también [menu]: no sale */
        if (t == T_ESC) { if (borrar_confirma) borrar_confirma = 0; else pant = P_TALLER; }
        else if (nmis == 0) break;
        else if (t == T_LETRA('N') && !mis_malo[sel_mis]) { borrar_confirma = 0; empieza_nombre(NOM_MIS); }
        else if (t == T_ARRIBA || t == T_ABAJO) {
            int s = mueve_lista(sel_mis, nmis, t, MIS_VIS);
            if (s != sel_mis) { sel_mis = s; borrar_confirma = 0; }
        }
        else if (t == T_BORRAR) {
            if (!borrar_confirma) borrar_confirma = 1;
            else {
                if (arch_borrar(mis_hueco[sel_mis])) olvida_hueco(mis_hueco[sel_mis]);
                carga_lista_mis();
            }
        }
        else if (t == T_ENTER && !mis_malo[sel_mis]) {
            /* se lee aparte: si no se puede abrir, el del editor sigue */
            static Circuito abierto;
            if (arch_cargar(mis_hueco[sel_mis], &abierto)) {
                int habia = nuevo_circuito();    /* olvida el rango del Bode y el del barrido */
                circ = abierto;
                hueco = mis_hueco[sel_mis];
                cur_x = 0; cur_y = 0;
                pon_aviso(habia ? "Abierto. [ctrl][Z] vuelve al que ten" S_ii "as." :
                                  "Abierto. [ctrl][S] lo sobrescribe.", C_OK);
                pant = P_EDITOR;
            }
        }
        break;

    case P_EDITOR: {
        const char *luego = aviso_luego;    /* lo que esperaba a esta tecla (TRAIDO) */
        aviso_luego = 0;
        tecla_editor(t);
        circ_sondas_limpia(&circ);          /* una sonda en un punto que se vació */
        /* si la tecla no dejó aviso, él; si dejó uno de una línea, debajo; si
         * se fue del editor (o escribe un nombre), ya no hace falta */
        if (luego && pant == P_EDITOR && nombrando != NOM_EDITOR) {
            if (!aviso[0]) pon_aviso(luego, C_CURSOR);
            else if (!aviso_bajo && una_linea(aviso)) aviso_bajo = luego;
        }
        return 1;                           /* [menu] abre las opciones aquí */
    }

    case P_ELEMENTO:
        tecla_elemento(t);
        return 1;                           /* [menu] abre las opciones del editor */

    case P_LECCIONES:
        if (t == T_ARRIBA || t == T_ABAJO) sel_lec = mueve_lista(sel_lec, N_LECCIONES, t, EJ_VIS);
        else if (t == T_ENTER) {
            pag = 0; sel_ejl = 0; scroll = 0;
            pant = LECCIONES[sel_lec].npag > 0 ? P_TEORIA : P_EJEMPLOS;
        }
        else if (t == T_ESC) pant = P_MENU;
        break;

    case P_TEORIA:
        if ((mods & T_CON_CTRL) && (t == T_IZQ || t == T_DER)) pag = t == T_DER ? l->npag - 1 : 0;   /* la primera o la última */
        else if (t == T_IZQ && pag > 0) pag--;
        else if ((t == T_DER || t == T_ABAJO) && pag < l->npag - 1) pag++;
        else if (t == T_ENTER || ((t == T_DER || t == T_ABAJO) && pag == l->npag - 1)) {
            sel_ejl = 0; scroll = 0; pant = P_EJEMPLOS;
        }
        else if (t == T_ARRIBA && pag > 0) pag--;
        else if (t == T_ESC) pant = P_LECCIONES;
        break;

    case P_EJEMPLOS:
        if (l->nmod > 0) {                  /* circuitos modelo: se emulan */
            if (t == T_ARRIBA || t == T_ABAJO) sel_ejl = mueve_lista(sel_ejl, l->nmod, t, EJ_VIS);
            else if (t == T_ENTER) {
                modelo_cargar(l->mod[sel_ejl], &cres);
                /* un modelo es otro circuito: su Bode empieza sin el rango, la
                 * entrada ni la salida del editor (que se olvidan, como al abrir) */
                memset(&bode_aj, 0, sizeof bode_aj);
                bode_aj.ent = -1;
                if (MODELOS[(int)l->mod[sel_ejl]].ca == 3) punto_op(P_EJEMPLOS, 0);
                else if (MODELOS[(int)l->mod[sel_ejl]].ca == 4) barre_modelo(l->mod[sel_ejl]);
                else if (MODELOS[(int)l->mod[sel_ejl]].ca) abre_modelo_ca(MODELOS[(int)l->mod[sel_ejl]].ca);
                else emula_cres(P_EJEMPLOS);
                return 1;
            }
            else if (t == T_TAB) {
                int habia = nuevo_circuito();
                modelo_cargar(l->mod[sel_ejl], &circ);
                cur_x = 0; cur_y = 0;
                pon_aviso(MODELOS[(int)l->mod[sel_ejl]].que_ver, C_TEXTO);
                if (habia) avisa_traido();
                pant = P_EDITOR;
            }
            else if (t == T_ESC) { scroll = 0; pant = l->npag > 0 ? P_TEORIA : P_LECCIONES; }
            break;
        }
        if (t == T_ARRIBA || t == T_ABAJO) sel_ejl = mueve_lista(sel_ejl, l->nej, t, EJ_VIS);
        else if (t == T_INCOG) lista_practica = !lista_practica;
        else if (t == T_ENTER) {
            ej_actual = l->ej[sel_ejl];
            ejemplo_cargar(ej_actual, &cres);
            resolver_y_mostrar(P_EJEMPLOS);
            if (lista_practica) practica_empieza();
        }
        else if (t == T_TAB) {                /* al Taller, para modificarlo */
            int habia = nuevo_circuito();
            ejemplo_cargar(l->ej[sel_ejl], &circ);
            cur_x = 0; cur_y = 0;
            pon_aviso(EJEMPLOS[(int)l->ej[sel_ejl]].enunciado, C_TEXTO);
            if (habia) avisa_traido();
            pant = P_EDITOR;
        }
        else if (t == T_ESC) { scroll = 0; pant = l->npag > 0 ? P_TEORIA : P_LECCIONES; }
        break;

    case P_AYUDA:
        if (t == T_ESC || t == T_MENU) { pant = volver_ayuda; scroll = 0; return 1; }
        else if (t == T_LETRA('T')) { banco_reinicia(); pant = P_BANCO; return 1; }  /* oculto */
        /* una línea; [izq/der] o [shift] una página; [ctrl] al principio o al final */
        else if (t == T_ARRIBA || t == T_ABAJO || t == T_IZQ || t == T_DER) mueve_scroll(t, ayuda_max(), AYUDA_VIS);
        return 1;

    case P_RESULTADO:
        if (res_nl) { tecla_resultado_nl(t); break; }
        if (sol.estado == SOL_NO_LINEAL && t == T_LETRA('I')) { res_guarda.hay = 0; barrido_de(P_RESULTADO, &cres); break; }
        if (practica == PRAC_TAPADO && sol.estado == SOL_OK && tecla_practica(t)) break;
        if (practica == PRAC_HECHO && (t == T_ARRIBA || t == T_ABAJO)) {
            /* la solución ya quedó fijada por lo contestado */
            if (t == T_ARRIBA && sel_preg > 0) sel_preg--;
            else if (t == T_ABAJO && sel_preg < npreg - 1) sel_preg++;
            break;
        }
        if (t == T_ESC) { a_culpable(); pant = origen; scroll = 0; practica = PRAC_NO; }
        else if (sol.estado != SOL_OK) break;
        else if (t == T_DER) capa = (capa + 1) % CAPA_N;
        else if (t == T_IZQ) capa = (capa + CAPA_N - 1) % CAPA_N;
        else if (t == T_ENTER) { pant = P_PASOS; paso_i = 0; }
        else if (t == T_TAB) { pant = P_TABLA; scroll = 0; }
        else if (t == T_LETRA('T') && practica == PRAC_NO && cres.term_x[0] >= 0 && cres.term_x[1] >= 0) {
            /* Thévenin de CC con lo que se despejó */
            static Circuito ap;
            sol_aplica(&cres, &sol, isol, &ap);
            thevenin_de(P_RESULTADO, &ap, 0.0);
        }
        else if ((t == T_ABAJO || t == T_ARRIBA) && sol.nsol > 1) {
            isol = (isol + (t == T_ABAJO ? 1 : sol.nsol - 1)) % sol.nsol;
            metodo = proced_generar(&cres, &red, &sol, isol, &pasos, 0);
        }
        break;

    case P_TABLA:
        if (t == T_ESC || t == T_TAB) pant = P_RESULTADO;
        else if (t == T_ARRIBA || t == T_ABAJO) mueve_scroll(t, tabla_max(), TB_VIS);
        break;

    case P_CPM:     tecla_cpm(t);     break;    /* [menu]: al menú, como en las demás */
    case P_CPM_RES: tecla_cpm_res(t); break;
    case P_CPM_DIB: tecla_cpm_dib(t); break;
    case P_CPM_MENU: tecla_cpm_menu(t); break;
    case P_CPM_TEO: tecla_cpm_teo(t); break;
    case P_CPM_PAG: tecla_cpm_pag(t); break;
    case P_CPM_EJ:  tecla_cpm_ej(t);  break;

    default:  /* P_PASOS */
        if (t == T_ESC) pant = P_RESULTADO;
        else if ((mods & T_CON_CTRL) && (t == T_IZQ || t == T_DER))           /* el primero o el último */
            paso_i = t == T_DER && pasos.n > 0 ? pasos.n - 1 : 0;
        else if (t == T_ENTER && practica == PRAC_TAPADO && paso_i >= pasos_vistos &&
                 paso_i < pasos.n)
            pasos_vistos = paso_i + 1;
        else if ((t == T_DER || t == T_ENTER || t == T_ABAJO) && paso_i < pasos.n - 1) paso_i++;
        else if ((t == T_IZQ || t == T_ARRIBA) && paso_i > 0) paso_i--;
        break;
    }

    if (t == T_MENU && pant != P_INICIO) { pant = P_MENU; editando = 0; }
    return 1;
}

/* Lo que decide si una ráfaga sigue: la pantalla y el modo del editor (el
 * menú, la paleta, la acción pendiente). Si el primer evento lo cambia (R y
 * una flecha: la pieza se pone y se abre su ficha), no hay repetición; si
 * cambia a mitad, se corta */
ESP_FN_JUEGA

static int firma_modo(void)
{
    int f = pant | ctx_abierto << 6 | paleta_abierta << 7 | accion << 8 | nombrando << 16;
    ESP_FIRMA(f)
    return f;
}

/* Fin de ráfaga: la lectura exacta del Bode y del barrido, y el cable
 * siguiente gasta otro nivel de deshacer */
static void suelta_rafaga(void)
{
    curv.rafaga = traz.rafaga = 0;
    hist_rafaga = 0;
    rep_ant = 0;
}

int ui_tecla(int t)
{
    int r, m0;
    if (t == T_SUELTA) { suelta_rafaga(); return 1; }     /* antes de nada: ni vueltas ni mods */
    vueltas = vueltas * 31u + (unsigned)t;
    m0 = firma_modo();
    r = tecla(t);
    if (!(t & T_MARCAS) && t != T_SHIFT && t != T_CTRL) {  /* el primer evento de una ráfaga */
        raf_modo = firma_modo();
        raf_ok = raf_modo == m0;
    }
    rep_ant = t & T_MARCAS;
    cotas_vista();
    return r;
}

/* Las cotas de la vista que antes ponía el dibujo (el scroll de la Ayuda,
 * de las tablas y de las listas; los campos que se ven de la ficha; las
 * líneas de la tabla de la emulación), puestas tras cada tecla: así una
 * tecla no depende de si se dibujó tras la anterior (el bucle junta varias
 * en un cuadro). El dibujo las repite: son idempotentes */
static void acota(int *v, int lo, int hi)
{
    if (*v > hi) *v = hi;
    if (*v < lo) *v = lo;
}

static void cotas_vista(void)
{
    switch (pant) {
    case P_AYUDA: acota(&scroll, 0, ayuda_max()); break;
    case P_TABLA: acota(&scroll, 0, tabla_max()); break;
    case P_EMU_TABLA:
        emu_lineas = emu_lineas_de();
        if (emu_pag <= 2) acota(&scroll, 0, emu_lineas - EMU_VIS > 0 ? emu_lineas - EMU_VIS : 0);
        break;
    case P_MIS: if (nmis > 0) scroll = desplaza(sel_mis, scroll, MIS_VIS); break;
    case P_EJEMPLOS: scroll = desplaza(sel_ejl, scroll, EJ_VIS); break;
    case P_ELEMENTO: if (sel_el >= 0 && sel_el < circ.nel) acota_campos(); break;
    case P_CPM_RES: acota(&scroll, 0, cpm_max()); cpm_lista_vista(); break;
    case P_CPM_PAG: acota(&scroll, 0, cpm_teo_max()); break;
    default: break;
    }
}

/* --------------------------------------------------------------------------
 *  La repetición y el deslizamiento: qué se repite y dónde (entrada.c pone
 *  los tiempos)
 * ----------------------------------------------------------------------- */
/* El perfil de la tecla k (con sus modificadores m) en esta pantalla, o
 * REP_NO. tac: un paso del dedo, que puede más que una tecla mantenida (la
 * capa de P_RESULTADO, las páginas de la teoría): ui_tactil decide dónde se
 * desliza. NUNCA se repiten [esc], [enter], [tab], [menu], [catálogo], [=],
 * [?!], [EE], [.], [(-)], las cifras ni las letras, ni nada con [ctrl] */
static int rep_perfil(int k, int m, int tac, int *tope)
{
    int vert = k == T_ARRIBA || k == T_ABAJO, horiz = k == T_IZQ || k == T_DER;
    if (escribe_nombre())                              /* escribiendo un nombre: solo [del] */
        return k == T_BORRAR && nom_edit[0] ? REP_BORRAR : REP_NO;
    switch (pant) {
    case P_MENU: case P_TALLER: case P_LECCIONES: case P_EJEMPLOS: case P_MIS: case P_TABLA:
        return vert ? REP_LISTA : REP_NO;              /* en Mis, [del] nunca: dos borran */
    case P_CA_TABLA: case P_EMU_TABLA:                 /* las páginas (con vuelta): solo el dedo */
        return vert || (tac && horiz) ? REP_LISTA : REP_NO;
    case P_PASOS: return vert || horiz ? REP_LISTA : REP_NO;
    case P_TEORIA: case P_RESULTADO: case P_CA:        /* página o capa: un paso por contacto */
        return tac && horiz ? REP_LISTA : REP_NO;
    case P_AYUDA:
        if (vert) { *tope = (m & T_CON_SHIFT) ? 1 : 3; return REP_CURSOR; }
        return horiz ? REP_LISTA : REP_NO;
    case P_FASORIAL:
        if (vert) return REP_LISTA;
        if (k == T_MAS || k == T_MENOS) return REP_ESCALA;
        return tac && horiz ? REP_LISTA : REP_NO;      /* la vista: con vuelta */
    case P_ELEMENTO: case P_AJUSTES:                   /* ←→ nunca: la variante borra datos */
        if (k == T_BORRAR) return editando && edit[0] ? REP_BORRAR : REP_NO;   /* vacío: libera la magnitud */
        if (tac && editando) return REP_NO;            /* a medio escribir, el dedo no confirma nada */
        if (pant == P_AJUSTES && aj_ctx != AJ_TIEMPO && aj_ctx != AJ_BODE)
            return tac && vert ? REP_LISTA : REP_NO;   /* campos con vuelta: solo el dedo */
        return vert ? REP_LISTA : REP_NO;
    case P_EDITOR:
        if (!vert && !horiz) return REP_NO;
        if (ctx_abierto) return tac && vert ? REP_LISTA : REP_NO;   /* el menú de opciones tiene vuelta */
        if (paleta_abierta) return REP_LISTA;
        return accion == ACC_NADA || accion == ACC_CABLE ? REP_LISTA : REP_NO;
    case P_OSC:
        if (osc.entrada[0]) return k == T_BORRAR ? REP_BORRAR : REP_NO;
        if (horiz && osc.cursores) { *tope = (m & T_CON_SHIFT) ? 1 : 8; return REP_CURSOR; }
        if (horiz) return REP_ESCALA;                  /* la ventana, una división */
        return !tac && (vert || k == T_MAS || k == T_MENOS) ? REP_ESCALA : REP_NO;
    case P_BODE: case P_BARRIDO: {
        const char *en = pant == P_BODE ? traz.entrada : curv.entrada;
        if (en[0] || (pant == P_BARRIDO && curv.de_nivel)) return k == T_BORRAR && en[0] ? REP_BORRAR : REP_NO;
        if (horiz) { *tope = (m & T_CON_SHIFT) ? 1 : 8; return REP_CURSOR; }
        return !tac && (vert || k == T_MAS || k == T_MENOS) ? REP_ESCALA : REP_NO;
    }
    ESP_REPITE(k, vert)
    case P_CPM:                                     /* los campos y sus líneas, el cursor y [del] */
        if (k == T_BORRAR) return cpm_es_texto(cpm_campo) && cpm_pos > 0 ? REP_BORRAR : REP_NO;
        if (horiz) {
            if (!cpm_es_texto(cpm_campo)) return REP_NO;   /* Auto, 2 o 3: de uno en uno */
            *tope = (m & T_CON_SHIFT) ? 1 : 3;
            return REP_CURSOR;
        }
        return vert ? REP_LISTA : REP_NO;
    case P_CPM_RES:                                    /* las opciones; el texto, como la Ayuda */
        if (horiz) return tac ? REP_LISTA : REP_NO;    /* las pestañas: una por pulsación */
        if (vert && cpm_tab == 1) { *tope = (m & T_CON_SHIFT) ? 1 : 3; return REP_CURSOR; }
        return vert ? REP_LISTA : REP_NO;
    case P_CPM_DIB: return horiz || vert ? REP_LISTA : REP_NO;   /* de opción en opción */
    case P_CPM_MENU: case P_CPM_TEO: case P_CPM_EJ: return vert ? REP_LISTA : REP_NO;
    case P_CPM_PAG:                                    /* como la Ayuda; las páginas, una por pulsación */
        if (vert) { *tope = (m & T_CON_SHIFT) ? 1 : 3; return REP_CURSOR; }
        return tac && horiz ? REP_LISTA : REP_NO;
    default:
        return REP_NO;                                 /* portada, banco, emulando... */
    }
}

/* ¿Cambiaría algo la tecla t (con sus modificadores)? En el borde, no: la
 * ráfaga se apaga y no se repinta en vano. Las pantallas de osc.c,
 * trazador.c, curva.c y fasorial.c se prueban sobre una copia */
static int cambia(int t)
{
    int k = t & ~T_MODS, d, nx, ny;
    const Leccion *l = &LECCIONES[sel_lec];
    if (escribe_nombre()) return k == T_BORRAR && nom_edit[0] != 0;
    switch (pant) {
    case P_MENU:      return k == T_ARRIBA ? sel_menu > 0 : sel_menu < 2;
    case P_TALLER:    return k == T_ARRIBA ? sel_taller > 0 : sel_taller < n_taller() - 1;
    case P_CPM:
        if (k == T_BORRAR) return cpm_es_texto(cpm_campo) && cpm_pos > 0;
        if (k == T_IZQ || k == T_DER) {
            if (!cpm_es_texto(cpm_campo)) return k == T_IZQ ? cpm_modo_i() > 0 : cpm_modo_i() < 2;
            return cpm_cursor_tras(k, t & T_MODS) != cpm_pos;
        }
        if (cpm_campo == CPM_F_BODEGA && cpm_cursor_linea(k) >= 0) return 1;
        return k == T_ARRIBA ? cpm_campo > 0 : cpm_campo < CPM_NF - 1;
    case P_CPM_RES:
        if (k == T_IZQ || k == T_DER) return k == T_IZQ ? cpm_tab > 0 : cpm_tab < 1;
        if (cpm_tab == 1) return k == T_ARRIBA ? scroll > 0 : scroll < cpm_max();
        return k == T_ARRIBA ? cpm_sel > 0 : cpm_sel < cpm_nopc() - 1;
    case P_CPM_DIB:   return k == T_IZQ || k == T_ARRIBA ? cpm_sel > 0 : cpm_sel < cpm_nopc() - 1;
    case P_CPM_MENU:  return k == T_ARRIBA ? cpm_msel > 0 : cpm_msel < 2;
    case P_CPM_TEO:   return k == T_ARRIBA ? cpm_tsel > 0 : cpm_tsel < cpm_teo_n() - 1;
    case P_CPM_EJ:    return k == T_ARRIBA ? cpm_esel > 0 : cpm_esel < cpm_ej_n() - 1;
    case P_CPM_PAG:
        if (k == T_ARRIBA || k == T_ABAJO) return k == T_ARRIBA ? scroll > 0 : scroll < cpm_teo_max();
        return k == T_IZQ ? cpm_tsel > 0 : cpm_tsel < cpm_teo_n() - 1;
    case P_LECCIONES: return k == T_ARRIBA ? sel_lec > 0 : sel_lec < N_LECCIONES - 1;
    case P_EJEMPLOS:  return k == T_ARRIBA ? sel_ejl > 0 : sel_ejl < (l->nmod > 0 ? l->nmod : l->nej) - 1;
    case P_MIS:       return nmis > 0 && (k == T_ARRIBA ? sel_mis > 0 : sel_mis < nmis - 1);
    case P_AYUDA:     return k == T_ARRIBA || k == T_IZQ ? scroll > 0 : scroll < ayuda_max();
    case P_TABLA:     return k == T_ARRIBA ? scroll > 0 : scroll < tabla_max();
    case P_CA_TABLA:  return k == T_ARRIBA ? scroll > 0 : k == T_ABAJO ? scroll + 1 < cres.nel : 1;
    case P_EMU_TABLA: return k == T_ARRIBA ? scroll > 0 : k == T_ABAJO ? emu_baja(scroll) != scroll : 1;
    case P_PASOS:     return k == T_DER || k == T_ABAJO ? paso_i < pasos.n - 1 : paso_i > 0;
    case P_TEORIA:    return k == T_IZQ ? pag > 0 : 1;             /* tras la última, los ejemplos */
    case P_RESULTADO: return sol.estado == SOL_OK && (res_nl || practica != PRAC_TAPADO);
    case P_CA:        return ca_r.d.estado == SIM_OK && !ca_sin_sol();
    case P_ELEMENTO:
        if (k == T_BORRAR) return editando && edit[0];
        if (aviso_r) return 0;                                       /* un número o una cuenta que no vale: se queda */
        return k == T_ARRIBA ? campo > 0 : campo < ncampos() - 1;
    case P_AJUSTES:
        if (k == T_BORRAR) return editando && edit[0];
        if (aj_ctx == AJ_TIEMPO || aj_ctx == AJ_BODE) return k == T_ARRIBA ? aj_campo != 0 : aj_campo != 1;
        return 1;                                                    /* con vuelta */
    case P_EDITOR:
        if (ctx_abierto) return 1;
        if (paleta_abierta)
            return k == T_IZQ ? sel_pal % 4 > 0 : k == T_DER ? sel_pal % 4 < 3 && sel_pal + 1 < PAL_N :
                   k == T_ARRIBA ? sel_pal >= 4 : sel_pal + 4 < PAL_N;
        d = dir_de(k);
        if (d < 0 || !circ_vecino(cur_x, cur_y, d, &nx, &ny)) return 0;
        /* con [W], la ráfaga no sustituye elementos ni entra en una caja */
        return accion != ACC_CABLE || tramo_libre(cur_x, cur_y, d);
    case P_OSC: {
        static Osc o;
        o = osc;
        osc_tecla(&o, sim_reg(), sim_res(), t);
        return o.itdiv != osc.itdiv || o.ivdiv[0] != osc.ivdiv[0] || o.ivdiv[1] != osc.ivdiv[1] ||
               o.pos[0] != osc.pos[0] || o.pos[1] != osc.pos[1] || o.tizq != osc.tizq ||
               o.tc[0] != osc.tc[0] || o.tc[1] != osc.tc[1] || strcmp(o.entrada, osc.entrada) != 0;
    }
    case P_BODE: {
        Trazador z = traz;
        if (bode_res()->fin == FIN_ERROR) return 0;
        traz_tecla(&z, bode_reg(), bode_res(), t);
        return z.lf0 != traz.lf0 || z.lf1 != traz.lf1 || z.fcur[0] != traz.fcur[0] || z.fcur[1] != traz.fcur[1] ||
               z.idb != traz.idb || z.db_top != traz.db_top || strcmp(z.entrada, traz.entrada) != 0;
    }
    case P_BARRIDO: {
        Curva z = curv;
        int q;
        if (barr_res()->fin == FIN_ERROR) return 0;
        cv_tecla(&z, barr_reg(), barr_res(), t);
        if (z.x0 != curv.x0 || z.x1 != curv.x1 || z.xc[0] != curv.xc[0] || z.xc[1] != curv.xc[1] ||
            strcmp(z.entrada, curv.entrada) != 0) return 1;
        for (q = 0; q < BARR_TRAZAS; q++) if (z.idiv[q] != curv.idiv[q] || z.pos[q] != curv.pos[q]) return 1;
        return 0;
    }
    case P_FASORIAL: {
        Fasorial z = fz;
        fz_tecla(&z, &cres, &ca_r, t);
        return z.sel != fz.sel || z.vista != fz.vista || z.selp != fz.selp ||
               z.iesc[0] != fz.iesc[0] || z.iesc[1] != fz.iesc[1];
    }
    default: return 1;                                               /* las demás */
    }
}

int ui_repite(int t, UiRepe *p)
{
    int k = t & ~(T_MODS | T_MARCAS), tac = (t & T_TACTIL) != 0, m, perfil;
    UiRepe nada;
    if (!p) p = &nada;
    p->tope = 1;
    /* una tecla mantenida lleva los modificadores de su primer evento; y si
     * ese evento cambió el modo (o cambió después), no se repite */
    m = tac ? t & T_MODS : (t & T_MODS) | mods_rafaga;
    if (!tac && (!raf_ok || firma_modo() != raf_modo)) return REP_NO;
    if (m & T_CON_CTRL) return REP_NO;                /* [ctrl] nunca repite */
    perfil = rep_perfil(k, m, tac, &p->tope);
    if (perfil == REP_NO || !cambia(k | m)) return REP_NO;
    return perfil;
}

int ui_tactil(UiTactil *p)
{
    memset(p, 0, sizeof *p);
    p->balistica = 1;
    if (escribe_nombre()) return 0;         /* escribiendo un nombre: el dedo no mueve nada */
    switch (pant) {
    case P_EDITOR:
        if (ctx_abierto) { p->ejes = TAC_Y; p->paso_y = 125; }                            /* H/8 */
        else if (paleta_abierta) { p->ejes = TAC_X | TAC_Y; p->paso_x = 250; p->paso_y = 167; }   /* W/4, H/6 */
        else if (accion == ACC_NADA || accion == ACC_CABLE) {                                /* la rejilla 10x7 */
            p->ejes = TAC_X | TAC_Y; p->paso_x = 100; p->paso_y = 143;
        }
        break;                              /* poner, editar, sonda o borrar: una flecha deliberada */
    case P_OSC:
        p->ejes = TAC_X;                    /* V/div y la posición, solo con teclas */
        if (osc.cursores) { p->paso_x = 25; p->balistica = 4; }                 /* 1/10 de división */
        else if (osc.disparo != DISP_LIBRE) { p->paso_x = 100; p->uno = TAC_X; }   /* un evento por gesto */
        else p->paso_x = 100;                                                   /* la ventana: 1 división */
        break;
    case P_BODE: case P_BARRIDO:
        p->ejes = TAC_X; p->paso_x = 25; p->balistica = 4;
        break;
    case P_AJUSTES:
        if (editando) break;                /* a medio escribir: cambiar de campo lo confirmaría */
        p->ejes = TAC_Y; p->paso_y = 125;
        break;
    case P_MENU: case P_TALLER: case P_MIS: case P_LECCIONES: case P_EJEMPLOS: case P_TABLA:
        p->ejes = TAC_Y; p->paso_y = 125;
        break;
    case P_CA_TABLA: case P_EMU_TABLA: case P_FASORIAL:        /* filas; páginas o vista, una por contacto */
        p->ejes = TAC_X | TAC_Y; p->paso_y = 125; p->uno = TAC_X;
        break;
    case P_AYUDA: p->ejes = TAC_Y; p->paso_y = 83; p->balistica = 3; break;   /* H/12 */
    case P_ELEMENTO:                                                          /* H/6: de campo en campo, */
        if (!editando) { p->ejes = TAC_Y; p->paso_y = 167; }                  /* no a medio escribir     */
        break;
    case P_PASOS: p->ejes = TAC_X; p->paso_x = 167; break;                    /* W/6 */
    case P_TEORIA: case P_RESULTADO: case P_CA: p->ejes = TAC_X; p->uno = TAC_X; break;
    case P_CPM:                                        /* de campo (o de línea) en campo; el cursor, W/20 */
        p->ejes = TAC_Y; p->paso_y = 167;
        if (cpm_es_texto(cpm_campo)) { p->ejes |= TAC_X; p->paso_x = 50; }
        break;
    case P_CPM_RES:                                    /* las opciones o el texto; las pestañas, una por contacto */
        p->ejes = TAC_X | TAC_Y; p->uno = TAC_X;
        if (cpm_tab == 1) { p->paso_y = 83; p->balistica = 3; } else p->paso_y = 125;
        break;
    case P_CPM_DIB: p->ejes = TAC_X; p->uno = TAC_X; break;                   /* una opción por contacto */
    case P_CPM_MENU: case P_CPM_TEO: case P_CPM_EJ: p->ejes = TAC_Y; p->paso_y = 125; break;
    case P_CPM_PAG: p->ejes = TAC_X | TAC_Y; p->paso_y = 83; p->balistica = 3; p->uno = TAC_X; break;
    default: break;                         /* portada, banco, emulando, modelo, Thévenin */
    }
    return p->ejes != 0;
}

void ui_trabajo(unsigned long ops) { ops_tick = ops > 0 ? ops : SIM_OPS_TICK; }

int ui_pantalla(void) { return pant; }
/* La pantalla p como la pinta ui_dibuja, sin cambiar a ella (el banco) */
void ui_dibuja_en(int p) { int antes = pant; pant = p; ui_dibuja(); pant = antes; }
void ui_semilla(unsigned s) { vueltas = s; }
int ui_capa(void) { return capa; }
int ui_isol(void) { return isol; }
int ui_paso_tapado(void) { return pant == P_PASOS && practica == PRAC_TAPADO && paso_i >= pasos_vistos; }
int ui_practica(int *bien, int *total)
{
    if (bien) *bien = practica_cuenta(PR_BIEN);
    if (total) *total = npreg;
    return practica;
}
const Circuito *ui_circuito(void) { return &circ; }
int ui_paso(void) { return paso_i; }
int ui_npasos(void) { return pasos.n; }

int ui_sel(void) { return sel_el; }
int ui_campo_q(void) { return pant == P_ELEMENTO && sel_el >= 0 && sel_el < circ.nel ? campo_info(campo)->q : -1; }
int ui_niveles(int *rehacer_n) { if (rehacer_n) *rehacer_n = nreh; return nhist; }

void ui_carpeta(const char *ruta) { arch_carpeta(ruta); }

/* --------------------------------------------------------------------------
 *  El panel «Controles» ([ctrl][menu] desde cualquier pantalla): deslizar,
 *  la repetición, el clic central y, en la CX II, el turbo (turbo.h). En
 *  ces_controles.tns: «CESK», versión 1 y los tres ajustes (los 8 bytes de
 *  siempre) y detrás, solo si hace falta, la sección del turbo (4 bytes):
 *  'T', su versión (1), la marca de cuelgue (el M que se estaba usando; 0,
 *  ninguno) y los niveles que fallaron (bit M - 34). Un lector de antes lee
 *  los 8 primeros y no mira más; sin la sección, nada del turbo. Una versión
 *  nueva de la sección añade al final. [G], oculto: el grabador de la capa.
 * ----------------------------------------------------------------------- */
#define CTL_N 3                          /* las filas de siempre                    */
#define CTL_TURBO 3                      /* y la del turbo, solo en la CX II         */
#define CTL_FILAS (CTL_N + turbo_hay())
#define CTL_ARCHIVO "/ces_controles.tns"
#define CTL_BYTES 8
#define CTL_T_BYTES 4                    /* 'T', versión 1, la marca y los fallidos  */

/* Los ajustes y, si hace falta, la sección del turbo. 1 si quedó escrito
 * (es también la marca de cuelgue del turbo: turbo_marcador) */
static int controles_guarda(void)
{
    unsigned char b[CTL_BYTES + CTL_T_BYTES];
    EntAjustes a;
    int n = CTL_BYTES;
    ent_ajustes(&a);
    memcpy(b, "CESK", 4);
    b[4] = 1; b[5] = a.deslizar; b[6] = a.repetir; b[7] = a.clic_enter;
    if (turbo_marca() || turbo_fallidos()) {
        b[8] = 'T'; b[9] = 1;
        b[10] = (unsigned char)turbo_marca(); b[11] = (unsigned char)turbo_fallidos();
        n += CTL_T_BYTES;
    }
    return arch_bytes_guardar(CTL_ARCHIVO, b, n);      /* si no se puede, valen para esta vez */
}

/* El aviso del turbo que espera [enter] (AVT_*) y su nivel; lo último que
 * pasó (TBM_*, o CTL_MSG_*), hasta la tecla siguiente; y, en esta sesión, si
 * ya se vio el aviso de activar y los fallidos que se aceptó volver a probar */
enum { AVT_NADA = 0, AVT_ACTIVAR, AVT_ALTO, AVT_FALLIDO, AVT_COLGO };
#define CTL_MSG_NADA  (-1)
#define CTL_MSG_BIEN  100
#define CTL_MSG_ELIGE 101
static int ctl_aviso, ctl_aviso_m, ctl_msg = CTL_MSG_NADA, tb_avisado;
static unsigned tb_confirmados;
static int tb_trago;                     /* el [esc] que canceló la medida del reloj: no saca del panel */

void ui_controles_abrir(void)
{
    unsigned char b[16];
    EntAjustes a;
    int n = arch_bytes_leer(CTL_ARCHIVO, b, (int)sizeof b), m;
    if (n < CTL_BYTES || memcmp(b, "CESK", 4) != 0 || b[4] != 1) return;
    a.deslizar = b[5]; a.repetir = b[6]; a.clic_enter = b[7];
    ent_pon_ajustes(&a);                               /* lo que no vale: el de omisión */
    if (n < CTL_BYTES + CTL_T_BYTES || b[8] != 'T' || b[9] < 1) return;   /* sin la sección del turbo */
    if (!(m = turbo_recupera(b[10], b[11]))) return;
    /* La marca seguía ahí: la calculadora se colgó a ese nivel (o se salió
     * sin pasar por main). Queda como fallido, la marca se borra y, en la
     * CX II, el panel lo dice nada más arrancar */
    controles_guarda();
    if (!turbo_hay()) return;
    ctl_origen = pant; ctl_campo = CTL_TURBO;
    ctl_aviso = AVT_COLGO; ctl_aviso_m = m;
    pant = P_CONTROLES;
}

/* El reloj de la capa (el de main.c; en el arnés, solo en su prueba), en dos
 * líneas: el plan, sus cambios y saltos; y lo que se calibra contra el RTC:
 * las cuentas del SP804 por segundo (ENT_SP804_HZ), el tick de idle()
 * (ENT_TICK_US) y los idle() por segundo (con trabajo, menos) */
static void dib_reloj_capa(int y)
{
    const EntReloj *r = ent_reloj_visto();
    unsigned long hz, tick_us, idle10;
    char t[64];
    Buf b;
    if (!r) return;
    ent_reloj_medido(r, &hz, &tick_us, &idle10);
    buf_init(&b, t, sizeof t);
    buf_str(&b, r->plan_a ? "Reloj de la capa: plan A" : "Reloj de la capa: plan B");
    buf_str(&b, "; cambios "); buf_int(&b, r->cambios);
    buf_str(&b, ", saltos "); buf_int(&b, r->saltos);
    gfx_text(8, y, t, C_SUAVE, 1);
    buf_init(&b, t, sizeof t);
    if (hz > 0) { buf_str(&b, "SP804 "); buf_int(&b, (long)hz); buf_str(&b, "/s; "); }
    else if (r->plan_a) buf_str(&b, "SP804 midiendo; ");
    if (tick_us > 0) { buf_str(&b, "tick "); buf_int(&b, (long)tick_us); buf_str(&b, " " S_MU "s; "); }
    buf_str(&b, "idle ");                               /* 52 columnas con 5 cifras de segundos */
    if (r->seg > 0) {
        buf_int(&b, (long)(idle10 / 10)); buf_ch(&b, ','); buf_int(&b, (long)(idle10 % 10));
        buf_str(&b, "/s ("); buf_int(&b, (long)r->seg); buf_str(&b, " s)");
    } else buf_str(&b, "midiendo");
    gfx_text(8, y + LIN_H, t, C_SUAVE, 1);
}

/* El touchpad que vio main.c (en el arnés, solo en su prueba): el tipo y el
 * W×H que dijo; si no es creíble, el de omisión con que trabaja la capa */
static void dib_pad_capa(int y)
{
    static const char *const TIPO[] = { "no hay", "Synaptics", "CapTIvate" };
    int tipo, w, h, uw, uh;
    char t[64];
    Buf b;
    tipo = ent_pad_tipo(&w, &h);
    if (tipo < 0 || tipo > ENT_TP_CAPTIVATE) return;
    ent_pad_usa(&uw, &uh);
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Touchpad: ");
    buf_str(&b, TIPO[tipo]);
    if (tipo != ENT_TP_NINGUNO) {
        buf_str(&b, ", ");
        if (w > 0 && h > 0) { buf_int(&b, w); buf_ch(&b, 'x'); buf_int(&b, h); }
        else buf_str(&b, "sin tama" S_nn "o");
        if (w != uw || h != uh) {                       /* no creíble: el de omisión */
            buf_str(&b, " (usa "); buf_int(&b, uw); buf_ch(&b, 'x'); buf_int(&b, uh); buf_ch(&b, ')');
        }
    }
    gfx_text(8, y, t, C_SUAVE, 1);
}

/* --------------------------------------------------------------------------
 *  El turbo en el panel (turbo.h), solo en la CX II: la fila elige el nivel
 *  (no, 408, 420, 432; 444 y 456 tras su aviso) y lo pone al momento, como
 *  las otras filas; [P] lo prueba. Activarlo pide antes el aviso de lo que
 *  es (una vez por sesión), y un nivel que colgó o no pasó la prueba, el
 *  suyo. Todo lo que se escribe aquí cabe en 52 columnas
 * ----------------------------------------------------------------------- */
#define TB_AYUDA "Experimental y no es de TI: puede colgar la calculadora (reset y " \
    "volver a instalar Ndless; se pierde lo no guardado). No conectes el USB con el " \
    "turbo activo. Tope 432 MHz; 444 y 456, tras un aviso. [P] lo prueba unos 50 s."

static int tb_probando(void)
{
    TurboPrueba p;
    turbo_prueba_estado(&p);
    return p.fase == TBP_REFERENCIA || p.fase == TBP_TURBO;
}

/* El vecino de m en la fila: no, 408, 420, 432, 444, 456 (444, con su aviso);
 * [izq] desde «no», al tope que se permite */
static int tb_vecino(int m, int der)
{
    if (der) return m == 0 ? TURBO_M_MIN : m >= TURBO_M_MAX ? 0 : m + 1;
    return m == 0 ? (turbo_permite(TURBO_M_MAX) ? TURBO_M_MAX : TURBO_M_TOPE) : m == TURBO_M_MIN ? 0 : m - 1;
}

/* El aviso que falta para poner m (AVT_NADA si ninguno) */
static int tb_aviso_de(int m)
{
    unsigned bit = 1u << (m - TURBO_M_MIN);
    if (!tb_avisado) return AVT_ACTIVAR;
    if (!turbo_permite(m)) return AVT_ALTO;
    if ((turbo_fallidos() & bit) && !(tb_confirmados & bit)) return AVT_FALLIDO;
    return AVT_NADA;
}

/* Poner m (0: apagarlo), o su aviso primero */
static void tb_elige(int m)
{
    ctl_msg = CTL_MSG_NADA;
    if (m == 0) { if (turbo_pon(0) != TBM_OK) ctl_msg = turbo_motivo(); return; }
    ctl_aviso_m = m;
    ctl_aviso = tb_aviso_de(m);
    if (ctl_aviso == AVT_NADA && turbo_pon(m) != TBM_OK) ctl_msg = turbo_motivo();
}

/* [enter] en un aviso: visto, y el siguiente (o se pone) */
static void tb_acepta(void)
{
    int av = ctl_aviso;
    ctl_aviso = AVT_NADA;
    if (av == AVT_COLGO) { pant = ctl_origen; return; }
    if (av == AVT_ACTIVAR) tb_avisado = 1;
    else if (av == AVT_ALTO) turbo_alto(1);
    else tb_confirmados |= 1u << (ctl_aviso_m - TURBO_M_MIN);
    tb_elige(ctl_aviso_m);
}

static void tb_probar(void)
{
    int m = turbo_pedido(), r;
    if (!m) { ctl_msg = CTL_MSG_ELIGE; return; }
    r = turbo_prueba(m);
    ctl_msg = r == TBM_OK ? CTL_MSG_NADA : r;
}

/* Un tick con la prueba en marcha; al acabar, lo que dio */
static void tb_tick(void)
{
    TurboPrueba p;
    if (!tb_probando() || turbo_prueba_paso()) return;
    turbo_prueba_estado(&p);
    ctl_msg = p.fase == TBP_BIEN ? CTL_MSG_BIEN : p.motivo;
    /* la medida del reloj bloquea y vio el [esc] ella misma (turbo_salida): la
     * pulsación llega luego como tecla, y ya está servida */
    tb_trago = p.motivo == TBM_CANCELADA && p.esc;
}

static void tb_hex(Buf *b, unsigned v)
{
    int i;
    buf_str(b, "0x");
    for (i = 28; i >= 0; i -= 4) buf_ch(b, "0123456789ABCDEF"[(v >> i) & 0xFu]);
}

static void tb_valor(Buf *b, const TurboPrueba *p)
{
    int m = p->fase == TBP_REFERENCIA || p->fase == TBP_TURBO ? p->m : turbo_atascado() ? turbo_nivel() : turbo_pedido();
    if (!m) { buf_str(b, "no"); return; }
    buf_int(b, TURBO_MHZ(m));
    buf_str(b, " MHz");
}

static void tb_aviso_texto(char *txt, int cap)
{
    Buf b;
    buf_init(&b, txt, (size_t)cap);
    switch (ctl_aviso) {
    case AVT_ACTIVAR:
        buf_str(&b, "Turbo a "); buf_int(&b, TURBO_MHZ(ctl_aviso_m));
        buf_str(&b, " MHz. Experimental y no es de TI: puede colgar la calculadora; "
                    "entonces, reset y volver a instalar Ndless, y se pierde lo no guardado. "
                    "Para escribir un archivo, antes vuelve a 396. No conectes el USB con "
                    "el turbo activo. " S_IQ "Activarlo?");
        break;
    case AVT_ALTO:
        buf_int(&b, TURBO_MHZ(ctl_aviso_m));
        buf_str(&b, " MHz pasa del tope de 432, el " S_uu "nico nivel que aguantaron todas "
                    "las CX II conocidas (una se colgaba a 444 a los pocos minutos). Pru" S_ee
                    "balo con [P] y no te f" S_ii "es de una emulaci" S_oo "n larga. " S_IQ "Seguir?");
        break;
    case AVT_FALLIDO:
        buf_str(&b, "A "); buf_int(&b, TURBO_MHZ(ctl_aviso_m));
        buf_str(&b, " MHz esta calculadora ya se colg" S_oo " o no pas" S_oo " la prueba. Puede "
                    "volver a colgarse: reset y volver a instalar Ndless, y se pierde lo no "
                    "guardado. " S_IQ "Probarlo otra vez?");
        break;
    default:
        buf_str(&b, "Con el turbo a "); buf_int(&b, TURBO_MHZ(ctl_aviso_m));
        buf_str(&b, " MHz la calculadora se colg" S_oo " la " S_uu "ltima vez: ese nivel queda "
                    "marcado y no se ofrece sin avisar. El turbo arranca siempre apagado.");
        break;
    }
}

/* Lo último que pasó, en txt; 0 si nada */
static int tb_msg_texto(char *txt, int cap)
{
    TurboPrueba p;
    Buf b;
    int r = ctl_msg;
    buf_init(&b, txt, (size_t)cap);
    turbo_prueba_estado(&p);
    if (turbo_atascado()) {
        buf_str(&b, "No se pudo volver a 396 MHz: no se guarda nada. Apaga y enciende la calculadora.");
        return 1;
    }
    if (r == CTL_MSG_NADA && !turbo_pedido() &&                      /* lo que pasó sin tocar el panel */
        (turbo_motivo() == TBM_SO || turbo_motivo() == TBM_AJENO || turbo_motivo() == TBM_RARO)) r = turbo_motivo();
    switch (r) {
    case CTL_MSG_BIEN:
        buf_str(&b, "Superada: "); buf_int(&b, p.pasadas); buf_str(&b, " pasadas iguales a ");
        buf_int(&b, TURBO_MHZ(p.m)); buf_str(&b, " MHz. Sigue activo.");
        return 1;
    case CTL_MSG_ELIGE: buf_str(&b, "Elige antes un nivel con [izq/der]."); return 1;
    case TBM_NO_CX2: buf_str(&b, "Solo en la CX II: aqu" S_ii " no se toca el reloj."); return 1;
    case TBM_USB:
        buf_str(&b, "Con el USB (288 MHz) no: descon" S_ee "ctalo; si sigue a 288, apaga y "
                    "enciende la calculadora.");
        return 1;
    case TBM_PATRON:
        buf_str(&b, "Reloj desconocido ("); tb_hex(&b, turbo_patron()); buf_str(&b, "): no se toca.");
        return 1;
    case TBM_DIV: buf_str(&b, "Un divisor inesperado (0x90140020): no se toca."); return 1;
    case TBM_FUENTE: buf_str(&b, "Otra fuente de reloj (0x90140810): no se toca."); return 1;
    case TBM_NIVEL: buf_str(&b, "Ese nivel no se permite."); return 1;
    case TBM_MARCA:
        buf_str(&b, "No se pudo escribir ces_controles.tns: sin la marca de cuelgue no se sube.");
        return 1;
    case TBM_PENDIENTE: buf_str(&b, "La PMU no confirm" S_oo " el cambio: vuelta a 396 MHz y apagado."); return 1;
    case TBM_LECTURA: buf_str(&b, "El reloj no qued" S_oo " como se escribi" S_oo ": vuelta a 396 y apagado."); return 1;
    case TBM_SO: buf_str(&b, "El SO cambi" S_oo " el reloj (" S_IQ "el USB?): turbo apagado."); return 1;
    case TBM_PRUEBA:
        buf_str(&b, "La prueba dio distinto a "); buf_int(&b, TURBO_MHZ(p.m));
        buf_str(&b, " MHz: vuelta a 396 y apagado. Ese nivel queda marcado.");
        return 1;
    case TBM_RELOJ32:
        buf_str(&b, "El contador de 32 kHz cambi" S_oo " con la CPU: vuelta a 396 y apagado.");
        return 1;
    case TBM_CANCELADA: buf_str(&b, "Prueba cancelada: vuelta a 396 MHz y apagado."); return 1;
    case TBM_MEMORIA: buf_str(&b, "No hay memoria para el patr" S_oo "n de la prueba: turbo apagado."); return 1;
    case TBM_SIN_RELOJ:
        buf_str(&b, "No se pudo medir el reloj a 396 (" S_IQ "el RTC?): turbo apagado. El nivel no queda marcado.");
        return 1;
    case TBM_RARO: buf_str(&b, "Reloj desconocido, por encima de 396 o sin calcular: no se toca ni se guarda nada."); return 1;
    case TBM_CPU:
        buf_str(&b, "La CPU no va a "); buf_int(&b, TURBO_MHZ(p.m));
        buf_str(&b, " MHz aunque el registro lo diga: vuelta a 396 y apagado.");
        return 1;
    case TBM_AJENO: buf_str(&b, "El reloj iba por encima de 396 sin el turbo: se baj" S_oo " a 396 para guardar."); return 1;
    default: return 0;
    }
}

/* La fila del turbo elegida: la frecuencia real (del registro, con la
 * fórmula de Linux) y el aviso que espera, la prueba en marcha, o lo que es
 * y lo último que pasó */
static void dib_turbo(int y, const TurboPrueba *p)
{
    char t[64], txt[320];
    Buf b;
    int mhz = turbo_mhz(), nv = turbo_nivel();
    buf_init(&b, t, sizeof t);
    buf_str(&b, "Reloj: "); buf_int(&b, mhz); buf_str(&b, " MHz");
    buf_str(&b, nv ? " (turbo)" : mhz == TURBO_MHZ(TURBO_M_USB) ? " (con el USB)" :
                mhz == TURBO_MHZ(TURBO_M_SERIE) ? " (de serie)" : "");
    gfx_text(8, y, t, nv ? C_POTENCIA : C_TEXTO, 1);
    y += LIN_H + 4;
    if (ctl_aviso) {
        tb_aviso_texto(txt, (int)sizeof txt);
        envuelve(8, y, COLS, txt, C_CURSOR, 7);
        pie(ctl_aviso == AVT_COLGO ? "[enter] entendido" : "[enter] s" S_ii "     [esc] no");
        return;
    }
    if (p->fase == TBP_REFERENCIA || p->fase == TBP_TURBO) {
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Probando "); buf_int(&b, TURBO_MHZ(p->m)); buf_str(&b, " MHz: ");
        if (p->fase == TBP_REFERENCIA) {
            buf_str(&b, "la referencia a 396 ("); buf_int(&b, p->seg);
            buf_str(&b, " de "); buf_int(&b, p->seg_total); buf_str(&b, " s)");
        } else {
            buf_int(&b, p->seg); buf_str(&b, " de "); buf_int(&b, p->seg_total);
            buf_str(&b, " s; "); buf_int(&b, p->pasadas); buf_str(&b, " pasadas iguales");
        }
        gfx_text(8, y, t, C_CURSOR, 1);
        buf_init(&b, t, sizeof t);
        buf_str(&b, "Pasada: "); buf_int(&b, p->vueltas); buf_str(&b, " vueltas (LU, Newton, libm) y ");
        buf_int(&b, (long)p->kb); buf_str(&b, " KB");
        gfx_text(8, y + LIN_H, t, C_SUAVE, 1);
        gfx_text(8, y + 2 * LIN_H, "Si una da distinto: vuelta a 396 y apagado.", C_SUAVE, 1);
        pie("[esc] cancela la prueba");
        return;
    }
    y += envuelve(8, y, COLS, TB_AYUDA, C_SUAVE, 5) * LIN_H;
    if (tb_msg_texto(txt, (int)sizeof txt)) envuelve(8, y + 4, COLS, txt, ctl_msg == CTL_MSG_BIEN ? C_OK : C_ERROR, 2);
    pie("[izq/der] nivel  [P] probar  [enter] listo");
}

static void dib_controles(void)
{
    static const char *const NOM[CTL_N + 1] = { "Deslizar el dedo", "Repetici" S_oo "n", "Clic central",
                                                "Turbo (experimental)" };
    static const char *const REP[REPETIR_N] = { "no", "lenta", "normal", "r" S_aa "pida" };
    EntAjustes a;
    TurboPrueba p;
    char t[64];
    Buf b;
    int i, y = BARRA_H + 12, n, ms, g, filas = CTL_FILAS;
    int fila_h = filas > CTL_N ? 20 : 26, xv = filas > CTL_N ? 134 : 120;   /* con el turbo, más juntas */
    ent_ajustes(&a);
    turbo_prueba_estado(&p);
    barra("Controles: teclas y touchpad", 0);
    for (i = 0; i < filas; i++, y += fila_h) {
        gfx_text(8, y, NOM[i], i == ctl_campo ? C_CURSOR : C_COTA, 1);
        if (i == ctl_campo) gfx_rect(xv, y - 3, SCR_W - 10 - xv, 17, C_CURSOR);
        buf_init(&b, t, sizeof t);
        buf_str(&b, S_LARR " ");
        if (i == CTL_TURBO) tb_valor(&b, &p);
        else buf_str(&b, i == 0 ? (a.deslizar ? "s" S_ii : "no") : i == 1 ? REP[a.repetir % REPETIR_N] :
                         (a.clic_enter ? "[enter]" : "nada"));
        buf_str(&b, " " S_RARR);
        gfx_text(xv + 6, y, t, C_TEXTO, 1);
    }
    y += 4;
    if (ctl_campo == CTL_TURBO) { dib_turbo(y, &p); return; }
    y += envuelve(8, y, COLS, "Deslizar: el dedo, sin hundir el pad, mueve los cursores, las "
                  "listas y la Ayuda. Repetici" S_oo "n: al mantener una flecha, [del] o una "
                  "escala. Clic central: el del centro del pad.", C_SUAVE, 5) * LIN_H;
    g = ent_grabacion(&n, &ms);
    if (g != GRAB_NADA) {
        buf_init(&b, t, sizeof t);
        if (g == GRAB_GRABANDO) {
            buf_str(&b, "Grabando: "); buf_int(&b, (ms + 999) / 1000);
            buf_str(&b, " s. Mueve el dedo y pulsa teclas.");
        } else if (g == GRAB_HECHO) {
            buf_str(&b, "Grabado: "); buf_int(&b, n); buf_str(&b, " muestras en ces_muestras.tns");
        } else buf_str(&b, "No se pudo escribir ces_muestras.tns.");
        gfx_text(8, y + 4, t, g == GRAB_ERROR ? C_ERROR : C_CURSOR, 1);
    }
    dib_pad_capa(PIE_Y - 3 * LIN_H - 4);
    dib_reloj_capa(PIE_Y - 2 * LIN_H - 4);
    pie("[arr/ab] fila  [izq/der] opci" S_oo "n  [enter] listo");
}

static void tecla_controles(int t)
{
    EntAjustes a;
    int filas = CTL_FILAS;
    if (tb_trago) {                                       /* el [esc] que ya canceló la prueba */
        tb_trago = 0;
        if (t == T_ESC) return;
    }
    if (tb_probando()) {                                  /* probando: solo [esc], que la corta */
        if (t == T_ESC) { turbo_prueba_cancela(); ctl_msg = TBM_CANCELADA; }
        return;
    }
    if (ctl_aviso) {                                      /* un aviso: [enter] sí, [esc] no */
        if (t == T_ENTER) tb_acepta();
        else if (t == T_ESC) { if (ctl_aviso == AVT_COLGO) pant = ctl_origen; ctl_aviso = AVT_NADA; }
        return;
    }
    ctl_msg = CTL_MSG_NADA;
    ent_ajustes(&a);
    if (t == T_ARRIBA || t == T_ABAJO) ctl_campo = (ctl_campo + (t == T_ABAJO ? 1 : filas - 1)) % filas;
    else if ((t == T_IZQ || t == T_DER) && ctl_campo == CTL_TURBO) tb_elige(tb_vecino(turbo_pedido(), t == T_DER));
    else if (t == T_LETRA('P') && ctl_campo == CTL_TURBO) tb_probar();
    else if (t == T_IZQ || t == T_DER) {
        int sg = t == T_DER ? 1 : REPETIR_N - 1;
        if (ctl_campo == 0) a.deslizar = (unsigned char)!a.deslizar;
        else if (ctl_campo == 1) a.repetir = (unsigned char)((a.repetir + sg) % REPETIR_N);
        else a.clic_enter = (unsigned char)!a.clic_enter;
        ent_pon_ajustes(&a);
    }
    else if (t == T_LETRA('G')) ent_grabar();             /* oculto: 10 s de muestras crudas */
    else if (t == T_ENTER || t == T_ESC || t == T_MENU) { controles_guarda(); pant = ctl_origen; }
}

#ifndef NDLESS
/* La foto del estado para el arnés (ver ui.h): lo que deciden las teclas,
 * sin cachés ni lo que solo es del dibujo */
static const char *foto_nom[128];     /* qué hay en cada tramo de la foto (para depurar) */
static int foto_ini[128], foto_nc;

const char *ui_foto_campo(int byte)
{
    int i;
    for (i = foto_nc - 1; i >= 0; i--) if (byte >= foto_ini[i]) return foto_nom[i];
    return "?";
}

int ui_foto(unsigned char *buf, int cap)
{
    int n = 0;
    foto_nc = 0;
#define FOTO(x) do { if (n + (int)sizeof(x) <= cap) {                                    \
        if (foto_nc < 128) { foto_nom[foto_nc] = #x; foto_ini[foto_nc++] = n; }          \
        memcpy(buf + n, &(x), sizeof(x)); n += (int)sizeof(x); } } while (0)
    /* un texto, hasta su final: lo de detrás es de textos anteriores */
#define FOTO_TXT(x) do { if (n + (int)sizeof(x) <= cap) {                                \
        size_t l_ = strlen(x) < sizeof(x) ? strlen(x) : sizeof(x);                       \
        if (foto_nc < 128) { foto_nom[foto_nc] = #x; foto_ini[foto_nc++] = n; }          \
        memset(buf + n, 0, sizeof(x)); memcpy(buf + n, (x), l_); n += (int)sizeof(x); } } while (0)
    FOTO(pant); FOTO(origen); FOTO(sel_menu); FOTO(sel_taller); FOTO(scroll);
    FOTO(sel_mis); FOTO(nmis); FOTO(borrar_confirma); FOTO(sel_lec); FOTO(pag); FOTO(sel_ejl);
    FOTO(cur_x); FOTO(cur_y); FOTO(accion); FOTO(sel_el);
    { int poner = accion == ACC_PONER ? poner_tipo : -1; FOTO(poner); }   /* solo vale con ACC_PONER */
    FOTO(paleta_abierta); FOTO(sel_pal); FOTO(ctx_abierto); FOTO(sel_ctx);
    FOTO(campo); FOTO(campo_ini); FOTO(sub); FOTO(editando); FOTO_TXT(edit); FOTO(aviso_r);
    FOTO_TXT(aviso); FOTO(capa); FOTO(isol); FOTO(paso_i); FOTO(practica); FOTO(sel_preg); FOTO_TXT(resp);
    { int luego = aviso_luego != 0; FOTO(luego); }                         /* TRAIDO, tras la tecla siguiente */
    FOTO(ca_capa); FOTO(ca_rect); FOTO(ca_pag); FOTO(ca_isol); FOTO(emu_pag); FOTO(emu_capa); FOTO(emu_lineas);
    FOTO(aj_campo); FOTO(aj_aviso); FOTO(mod_pend); FOTO(nhist); FOTO(nreh); FOTO(ctl_campo);
    FOTO(aviso_cuenta); FOTO(nombrando); FOTO_TXT(nom_edit); FOTO(pegando); FOTO(clip.hay); FOTO_TXT(clip.de);
    {
        /* el circuito sin lo que no es suyo: los huecos de elementos que no
         * se usan y lo que queda tras el final de los textos (restos de
         * circuitos anteriores, que dependen de lo que se hizo antes) */
        static Circuito cn;
        int k;
        cn = circ;
        for (k = cn.nel; k < MAX_EL; k++) memset(&cn.el[k], 0, sizeof cn.el[k]);
        for (k = 0; k < cn.nel && k < MAX_EL; k++) {
            size_t l = strlen(cn.el[k].etiq);
            if (l < sizeof cn.el[k].etiq) memset(cn.el[k].etiq + l, 0, sizeof cn.el[k].etiq - l);
        }
        {
            size_t l = strlen(cn.nombre);
            if (l < sizeof cn.nombre) memset(cn.nombre + l, 0, sizeof cn.nombre - l);
        }
        FOTO(cn.h); FOTO(cn.v); FOTO(cn.el); FOTO(cn.nel); FOTO(cn.nombre); FOTO(cn.tierra_x); FOTO(cn.tierra_y);
        FOTO(cn.term_x); FOTO(cn.term_y); FOTO(cn.conv); FOTO(cn.frec); FOTO(cn.sonda); FOTO(cn.t_fin); FOTO(cn.paso);
    }
    /* el osciloscopio sin lo calculado (c_gen y lo de detrás) */
    FOTO(osc.itdiv); FOTO(osc.ivdiv); FOTO(osc.pos); FOTO(osc.ver); FOTO(osc.activo); FOTO(osc.cursores);
    FOTO(osc.cur); FOTO(osc.panel); FOTO(osc.disparo); FOTO(osc.tizq); FOTO(osc.tc); FOTO_TXT(osc.entrada);
    FOTO_TXT(osc.aviso);
    FOTO(traz.lf0); FOTO(traz.lf1); FOTO(traz.fcur); FOTO(traz.dos); FOTO(traz.activo); FOTO(traz.idb);
    FOTO(traz.db_top); FOTO_TXT(traz.entrada); FOTO_TXT(traz.aviso);
    FOTO(curv.x0); FOTO(curv.x1); FOTO(curv.xc); FOTO(curv.idiv); FOTO(curv.pos); FOTO(curv.ver);
    FOTO(curv.activo); FOTO(curv.dos); FOTO(curv.cur); FOTO(curv.xy); FOTO_TXT(curv.entrada); FOTO_TXT(curv.aviso);
    FOTO(fz.sel); FOTO(fz.ini); FOTO(fz.vista); FOTO(fz.iesc); FOTO(fz.selp); FOTO(fz.inip); FOTO(fz.ver);
    /* los capacitores equivalentes: lo escrito, el campo con su cursor, el
     * modo, el aviso, la opción, la pestaña y lo elegido en su menú, en la
     * Teoría y en los Ejemplos (el resultado sale de lo escrito con [enter]:
     * sus cuentas bastan) */
    FOTO(cpm_campo); FOTO(cpm_nuevo); FOTO(cpm_err); FOTO(cpm_av); FOTO(cpm_sel); FOTO(cpm_nlin); FOTO(cpm_r.nval);
    FOTO(cpm_pos); FOTO(cpm_btop); FOTO(cpm_modo); FOTO(cpm_tab); FOTO(cpm_lsc); FOTO(cpm_msel); FOTO(cpm_tsel);
    FOTO(cpm_esel); FOTO(cpm_r.ntot); FOTO(cpm_bpos); FOTO(cpm_mio_modo); FOTO(cpm_hay_mio);
    FOTO_TXT(cpm_txt[0]); FOTO_TXT(cpm_txt[1]); FOTO_TXT(cpm_txt[2]);
    FOTO_TXT(cpm_mio[0]); FOTO_TXT(cpm_mio[1]); FOTO_TXT(cpm_mio[2]);
#undef FOTO
#undef FOTO_TXT
    return n;
}

int ui_indice(int que)
{
    switch (que) {
    case UI_I_SCROLL:   return scroll;
    case UI_I_SEL_MENU: return sel_menu;
    case UI_I_SEL_MIS:  return sel_mis;
    case UI_I_SEL_LEC:  return sel_lec;
    case UI_I_SEL_EJL:  return sel_ejl;
    case UI_I_PASO:     return paso_i;
    case UI_I_PAG:      return pag;
    case UI_I_CUR_X:    return cur_x;
    case UI_I_CUR_Y:    return cur_y;
    case UI_I_CAMPO:    return campo;
    case UI_I_ACCION:   return accion;
    case UI_I_RAFAGA:   return curv.rafaga | traz.rafaga << 1;
    case UI_I_AJ_CAMPO: return aj_campo;
    case UI_I_SEL_TALLER: return sel_taller;
    case UI_I_CPM_CAMPO: return cpm_campo;
    case UI_I_CPM_SEL:  return cpm_sel;
    case UI_I_CPM_POS:  return cpm_pos;
    case UI_I_CPM_NUEVO: return cpm_nuevo;
    case UI_I_CPM_BTOP: return cpm_btop;
    case UI_I_CPM_MODO: return cpm_modo;
    case UI_I_CPM_TAB:  return cpm_tab;
    case UI_I_CPM_LSC:  return cpm_lsc;
    case UI_I_CPM_MSEL: return cpm_msel;
    case UI_I_CPM_TSEL: return cpm_tsel;
    case UI_I_CPM_ESEL: return cpm_esel;
    case UI_I_CPM_NLIN: return cpm_nlin;
    default:            return -1;
    }
}

const struct CpmRes *ui_cpm(void) { return &cpm_r; }
const char *ui_cpm_texto(int f) { return f >= 0 && f < 3 ? cpm_txt[f] : ""; }
const char *ui_cpm_paso(void) { return cpm_proc; }
void ui_cpm_oculto(int si) { cpm_oculto = si != 0; }
int  ui_cpm_oculto_esta(void) { return cpm_oculto; }
unsigned ui_vueltas(void) { return vueltas; }
#endif

/* La sesión: el circuito del editor con su hueco (si su archivo sigue ahí y es
 * el mismo: lo mira arch_sesion_cargar) y el cursor. Empieza como un problema
 * recién abierto: sin historial. Al entrar con «Seguir» se dice que se
 * recuperó */
void ui_sesion_abrir(void)
{
    static Circuito t;
    int h, cx, cy;
    if (!arch_sesion_cargar(&t, &h, &cx, &cy) || t.nel == 0) return;
    nuevo_circuito();
    circ = t;
    hueco = h;
    cur_x = cx; cur_y = cy;
    nhist = nreh = 0;
    ses_aviso = 1;
}

/* El circuito del editor. Vacío, el último con algo del historial (con su
 * hueco): tras «Problema nuevo», el de antes, que [ctrl][Z] recuperaba y que
 * si no se perdería al salir (la sesión no guarda el historial). Vacío y sin
 * nada detrás, no queda sesión (la próxima vez se empieza como hoy) */
void ui_sesion_guardar(void)
{
    int i;
    if (circ.nel > 0) { arch_sesion_guardar(&circ, hueco, cur_x, cur_y); return; }
    for (i = nhist - 1; i >= 0; i--)
        if (hist[i].c.nel > 0) { arch_sesion_guardar(&hist[i].c, hist[i].hueco, 0, 0); return; }
    arch_sesion_borrar();
}

void ui_init(void)
{
    pant = P_INICIO;
    origen = P_EDITOR;
    sel_menu = 0; sel_taller = 0; scroll = 0; sel_mis = 0; nmis = 0;
    sel_lec = 0; pag = 0; sel_ejl = 0; ej_actual = 0;
    ficha_pend = 0;
    nuevo_circuito();
    nhist = nreh = 0;                   /* nuevo_circuito guarda el de antes: aquí no */
    circ_init(&cres, "");
    hay_ultimo_R = 0; ultimo_R = 0.0;
    campo = 0; sub = 0; editando = 0; aviso_r = 0; edit[0] = 0;
    capa = CAPA_VALORES; isol = 0; paso_i = 0;
    metodo = PROC_NADA;
    volver_ayuda = P_MENU;
    previa = -1;
    mod_pend = 0; mods = 0;
    paleta_abierta = 0; sel_pal = 0;
    practica = PRAC_NO; npreg = 0; lista_practica = 0;
    memset(&osc, 0, sizeof osc);
    osc_tipo_ant[0] = osc_tipo_ant[1] = 0;
    emu_origen = P_EDITOR;
    memset(&traz, 0, sizeof traz);
    memset(&curv, 0, sizeof curv);
    memset(&barr_aj, 0, sizeof barr_aj);
    barr_aj.el = -1;
    res_nl = 0; res_aviso[0] = 0; res_titulo[0] = 0;
    res_guarda.hay = 0;
    trabajo = TR_EMULAR;
    ca_rect = 0; ca_capa = CAPA_VALORES;
    pasos_init(&pasos);
    memset(&sol, 0, sizeof sol);
    sol.estado = SOL_ERR_RED;
    rep_act = 0; rep_ant = 0; mods_rafaga = 0; raf_modo = 0; raf_ok = 0; hist_rafaga = 0;
    ctl_origen = P_INICIO; ctl_campo = 0;
    ctl_aviso = AVT_NADA; ctl_msg = CTL_MSG_NADA; tb_avisado = 0; tb_confirmados = 0; tb_trago = 0;
    turbo_marcador(controles_guarda);   /* la marca de cuelgue va en ces_controles.tns */
    aviso_cuenta = 0;
    nombrando = NOM_NO; nom_edit[0] = 0; nom_aviso = 0;
    memset(&clip, 0, sizeof clip); pegando = 0;
    ent_ajustes_omision();              /* los de entrada.h: el archivo lo abre main.c */
    ent_grabacion_olvida();
    /* los capacitores equivalentes: el ejemplo del pizarrón (17 MF con la
     * bodega A, en Auto), y desde ahí lo que se escriba, mientras dure la
     * sesión */
    strcpy(cpm_txt[CPM_F_VALOR], "17");
    cpm_txt[CPM_F_TOL][0] = 0;
    strcpy(cpm_txt[CPM_F_BODEGA], CPM_BODEGA_A);
    cpm_modo = 0;
    cpm_campo = CPM_F_VALOR; cpm_nuevo = 1; cpm_err = 0; cpm_av = CPM_AV_NADA;
    cpm_pos = (int)strlen(cpm_txt[CPM_F_VALOR]); cpm_btop = 0;
    cpm_sel = 0; cpm_nlin = 0; cpm_tab = 0; cpm_lsc = 0; cpm_msel = 0; cpm_tsel = 0; cpm_esel = 0;
    cpm_bpos = (int)strlen(cpm_txt[CPM_F_BODEGA]);
    memset(cpm_mio, 0, sizeof cpm_mio); cpm_mio_modo = 0; cpm_hay_mio = 0;
    cpm_proc[0] = 0;
    memset(&cpm_r, 0, sizeof cpm_r);
}
