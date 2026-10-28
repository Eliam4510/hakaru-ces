/* ============================================================================
 *  turbo.c  -  El turbo (experimental)  (ver turbo.h)
 *
 *  Un paso de 12 MHz (paso), el protocolo:
 *    0. sin teclas abajo (con tope): con las IRQ abiertas el SO lee el
 *       teclado, y la tecla que pidió el cambio le llegaría;
 *    1. las IRQ abiertas y el pendiente (bit 0 de 0x90140024) a 0: si hay una
 *       petición a medias (un cambio del SO, o una escritura de Hakaru sin
 *       confirmar), NUNCA se escribe encima (TBM_PENDIENTE; ninguna fuente
 *       lo hace: el reproductor de vídeo, en su versión de hoy, ni empieza
 *       con el pendiente a 1). Bajando, con un tope mayor (un SO que tarda);
 *    2. las IRQ cerradas, 0x90140030 tiene que ser lo que se espera (lo que
 *       puso Hakaru, o 0x21020303): si no, lo cambió otro (del_so o cambiado);
 *    3. se escribe M en los bits 24-31 (escribe_pll: la única escritura);
 *    4. las IRQ ABIERTAS mientras se espera a que el SO atienda INT_POWER y el
 *       pendiente vuelva a 0 (como NoverII, TCT_Local_Control_Interrupts(0),
 *       y la primera versión del reproductor de vídeo: con ellas cerradas
 *       nadie lo atiende), con tope en cuentas de 32 kHz, y 1 ms más;
 *    5. las IRQ como estaban, y la lectura de vuelta: tiene que ser lo
 *       escrito. Si el pendiente no se limpió, la PMU no lo confirmó: la CPU
 *       puede ir a lo de antes o a lo escrito («sin confirmar», sin_conf), y
 *       cuenta como atascado: no se escribe nada (ni la PMU ni archivos) hasta
 *       que se limpie (confirma: en cada reintento, las IRQ abiertas otro rato).
 *  Cerrarlas en 2-3: la lectura y la escritura, juntas.
 *  Hay que abrirlas porque un programa de Ndless corre con ellas CERRADAS:
 *  ld_exec las cierra antes de llamar a main (ndless/src/resources/
 *  ploaderhook.c: TCT_Local_Control_Interrupts(-1), para que el SO no pinte
 *  su reloj); idle() despierta igual (WFI) y reconoce el tick a mano. Por
 *  eso se vuelven a dejar como estaban, y fuera de estas esperas el SO no
 *  atiende nada (tampoco el USB: su cambio de reloj llega en ellas, y la
 *  lectura de vuelta lo ve).
 *
 *  De quién es lo que hay en 0x90140030: del SO, solo lo que el SO pone (M ≤
 *  33: 288 con el USB, 396); eso no se toca (del_so). Por encima de 396 es
 *  cosa de Hakaru: con sus bits bajos (bajable) se baja de 12 en 12; con
 *  otros, o sin confirmar, «atascado»: no se escribe nada (ni la PMU ni
 *  archivos) y se dice. Atascado no es para siempre: cada acción, cada
 *  escritura y la salida lo reintentan (reintenta).
 *
 *  [P] mide además la CPU (mide_cpu): el registro solo dice lo que se
 *  escribió (en Firebird, y según la versión de hoy del reproductor el PLL
 *  lo confirma 0x90140020, que Hakaru no escribe). Si la carga fija no tarda
 *  33/M de lo que tardó a 396, la CPU no va a M: TBM_CPU, a 396 y apagado.
 * ========================================================================= */
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "turbo.h"
#include "lu.h"
#include "nolin.h"

#define TB_TOPE_T32      655u      /* el pendiente: 20 ms de 32 kHz como mucho          */
#define TB_TOPE_BAJA_T32 16384u    /* bajando: 0,5 s (y luego se escribe igual)         */
#define TB_ASIENTA_T32    33u      /* y 1 ms más tras limpiarse                         */
#define TB_SUELTA_T32   16384u     /* las teclas: 0,5 s como mucho (una atascada no para nada) */
#define TB_TOPE_VUELTAS 4000000UL  /* si el contador de 32 kHz no avanzara (va con la CPU:
                                      solo por si acaso; el tope es el de arriba)        */
#define TB_REF_S           3       /* la pasada de referencia: 2-3 s de RTC a 396       */
#define TB_TURBO_S        45       /* a M, pasadas durante 45 s (acaba la que va)       */
#define TB_MEM_BYTES (2UL << 20)   /* el patrón: 2 MB (1 MB o 512 KB si no hay)         */
#define TB_TROZOS          8       /* el patrón se escribe y se lee en 8 trozos          */
#define TB_RELOJ_MIL      20       /* el contador de 32 kHz a M: ±2 % de lo de 396      */
#define TB_SEG_MAX     (4u * 32768u)   /* medir un segundo del RTC: ~4 s de cuentas como mucho */
/* La CPU (mide_cpu): una carga fija de enteros que cabe en la caché, ~0,12 s
 * a 396 en la calculadora (en el PC, menos: el simulador pone su tiempo) */
#ifdef NDLESS
#define TB_CPU_VUELTAS  8000000UL
#else
#define TB_CPU_VUELTAS  80000UL
#endif
#define TB_CPU_MIN      1000UL     /* a 396, menos cuentas que esto: no se puede medir (~30 ms) */
#define TB_CPU_MIL        10UL     /* a M ha de tardar 33/M de lo de 396, a ±1 % (un nivel es ~3 %) */

/* --------------------------------------------------------------------------
 *  El hardware: el de main.c; sin él, «no es una CX II». En el PC, el
 *  simulador (abajo)
 * ----------------------------------------------------------------------- */
#ifdef NDLESS
static int      nada_cx2(void) { return 0; }
static unsigned nada_lee(unsigned d) { (void)d; return 0; }
static void     nada_pon(unsigned v) { (void)v; }
static int      nada_irq(int m) { (void)m; return 0; }
static unsigned nada_t(void) { return 0; }
static const TurboHw hw_nada = { nada_cx2, nada_lee, nada_pon, nada_irq, nada_t, nada_t, nada_cx2 };
static const TurboHw *hw = &hw_nada;
#else
static const TurboHw hw_sim;
static const TurboHw *hw = &hw_sim;
/* La PMU de una CX II a 396 sin USB (turbo_sim_reinicia). Al empezar, la
 * misma pero «no es una CX II»: lo que ven las pruebas de antes del turbo */
static const TurboSim sim0 = { 1, TURBO_SERIE, 0x00000000u, 0x11u, 0, 0, 3, 0, 1, 8192 };
static TurboSim sim = { 0, TURBO_SERIE, 0x00000000u, 0x11u, 0, 0, 3, 0, 1, 8192 };
#endif

static int (*salir)(void);          /* [esc] abajo (main.c)                     */
static int (*marcador)(void);       /* escribe ces_controles.tns (ui.c)         */

static int      pedido;             /* el M que se quiere (0: apagado)           */
static int      nivel;              /* el M que puso Hakaru y comprobó (0: 396)  */
static unsigned puesto;             /* lo último que escribió y comprobó         */
static int      motivo, atascado, alto, marca, pausas, pausado, pausa_ok;
static int      sin_marca;          /* apagado por otro camino: la marca, fuera (turbo_vigila) */
static int      sin_conf;           /* la última escritura, sin confirmar (el pendiente no se limpió) */
static int      conf_m;             /* ...y el mayor M de los dos (el de antes y el escrito): la
                                       CPU puede ir a cualquiera de ellos                  */
static unsigned fallidos, patron;

#define M_DE(x) ((int)((x) >> 24))

void turbo_hw(const TurboHw *h)
{
#ifdef NDLESS
    hw = h ? h : &hw_nada;
#else
    hw = h ? h : &hw_sim;
#endif
}
void turbo_salida(int (*f)(void)) { salir = f; }
void turbo_marcador(int (*f)(void)) { marcador = f; }

int      turbo_hay(void) { return hw->cx2() != 0; }
int      turbo_nivel(void) { return nivel; }
int      turbo_pedido(void) { return pedido; }
int      turbo_motivo(void) { return motivo; }
unsigned turbo_patron(void) { return patron; }
int      turbo_atascado(void) { return atascado; }
int      turbo_marca(void) { return marca; }
unsigned turbo_fallidos(void) { return fallidos; }
void     turbo_alto(int si) { alto = si != 0; }
int      turbo_permite(int m) { return m >= TURBO_M_MIN && m <= (alto ? TURBO_M_MAX : TURBO_M_TOPE); }

/* La frecuencia, con la fórmula del driver de Linux: 48 MHz si el bit 4 de
 * 0x90140810 está a 0; 24 si lo está el bit 4 de 0x90140030; si no,
 * 24 MHz·M/D1/(1+D2) */
int turbo_mhz(void)
{
    unsigned p, d, d1;
    if (!hw->cx2()) return 0;
    if (!(hw->lee(TURBO_R_FUENTE) & 0x10u)) return 48;
    p = hw->lee(TURBO_R_PLL);
    if (p & 0x10u) return 24;
    d = hw->lee(TURBO_R_DIV);
    d1 = (p >> 16) & 0x1Fu;
    if (d1 == 0) return 0;
    return (int)(24u * (p >> 24) / d1 / (1u + ((d >> 20) & 0xFu)));
}

/* La de la marca naranja: la del nivel; atascado, la que hay de verdad (el
 * registro puede no ser el de un nivel); sin confirmar, la del nivel que
 * quedó (el registro, o el de antes si ya dice 396) */
int turbo_mhz_marca(void)
{
    if (atascado) return sin_conf ? TURBO_MHZ(nivel) : turbo_mhz();
    return nivel ? TURBO_MHZ(nivel) : 0;
}

/* ¿Pudo ponerlo Hakaru? Lo de 0x21020303 con M en 33..38 */
static int propio(unsigned x)
{
    return (x & 0x00FFFFFFu) == (TURBO_SERIE & 0x00FFFFFFu) &&
           M_DE(x) >= TURBO_M_SERIE && M_DE(x) <= TURBO_M_MAX;
}

/* ¿Lo sabe bajar Hakaru? Por encima de 396 con sus bits bajos (hasta 39: la
 * lectura de vuelta de 38 que diera uno más). Cada escritura de la bajada es
 * propia (escribe_pll) */
static int bajable(unsigned x)
{
    return (x & 0x00FFFFFFu) == (TURBO_SERIE & 0x00FFFFFFu) &&
           M_DE(x) > TURBO_M_SERIE && M_DE(x) <= TURBO_M_MAX + 1;
}

/* La ÚNICA escritura de la PMU: 0x90140030 con solo M distinto de 0x21020303
 * y M en 33..38. Cualquier otra cosa no se escribe (1 si se escribió) */
static int escribe_pll(unsigned v)
{
    if (!propio(v)) return 0;
    hw->pon_pll(v);
    return 1;
}

int turbo_puede(void)
{
    unsigned d, f;
    if (!hw->cx2()) return TBM_NO_CX2;
    patron = hw->lee(TURBO_R_PLL);
    d = hw->lee(TURBO_R_DIV);
    f = hw->lee(TURBO_R_FUENTE);
    if ((patron >> 24) == TURBO_M_USB) return TBM_USB;
    if (patron != TURBO_SERIE) return TBM_PATRON;           /* M = 33, D1 = 2, bit 4 a 0 y lo demás */
    if (((d >> 20) & 0xFu) != 0 || (d & 2u)) return TBM_DIV;
    if (!(f & 0x10u)) return TBM_FUENTE;
    return TBM_OK;
}

/* El pendiente a 0 (las IRQ, abiertas por quien llama), con tope en cuentas
 * de 32 kHz; y, si asienta, 1 ms más. 0 si se limpió; 1 si no a tiempo */
static int espera(int asienta, unsigned tope)
{
    unsigned t0 = hw->t32(), t;
    unsigned long n = 0;
    for (;;) {
        t = hw->t32();
        if (!(hw->lee(TURBO_R_PEND) & 1u)) break;
        if (t - t0 >= tope || ++n >= TB_TOPE_VUELTAS) return 1;
    }
    if (asienta) while (hw->t32() - t < TB_ASIENTA_T32 && ++n < TB_TOPE_VUELTAS) {}
    return 0;
}

/* Antes de abrir las IRQ, que no haya teclas abajo (con tope) */
static void suelta(void)
{
    unsigned t0;
    unsigned long n = 0;
    if (!hw->tecla) return;
    t0 = hw->t32();
    while (hw->tecla() && hw->t32() - t0 < TB_SUELTA_T32 && ++n < TB_TOPE_VUELTAS) {}
}

/* Un paso: de esperado (lo que tiene que haber) a M. TBM_OK o el motivo; nivel
 * y puesto quedan con lo que se lee al final: arriba con los bits de Hakaru,
 * ese nivel (y se puede bajar); M ≤ 33, 0 (del SO si no es lo de Hakaru);
 * arriba con otros bits, TBM_RARO (atascado, lo decide vuelve). Si la PMU no
 * confirma lo escrito, TBM_PENDIENTE y sin confirmar (atascado, vuelve) */
static int paso(unsigned esperado, int m)
{
    unsigned v = (esperado & 0x00FFFFFFu) | ((unsigned)m << 24), x;
    int baja = m < M_DE(esperado), antes, r;
    unsigned tope = baja ? TB_TOPE_BAJA_T32 : TB_TOPE_T32;
    suelta();
    antes = hw->irq(0);                        /* abiertas: una petición a medias acaba */
    if (espera(0, tope)) { hw->irq(antes); return TBM_PENDIENTE; }   /* nunca encima de una sin atender */
    sin_conf = 0;                              /* la de antes, si la había, ya la atendió el SO */
    hw->irq(-1);                               /* cerradas: leer y escribir, juntos */
    x = hw->lee(TURBO_R_PLL);
    if (x != esperado) { hw->irq(antes); return M_DE(x) <= TURBO_M_SERIE ? TBM_SO : bajable(x) ? TBM_LECTURA : TBM_RARO; }
    if (!escribe_pll(v)) { hw->irq(antes); return TBM_NIVEL; }
    hw->irq(0);                                /* abiertas: el SO atiende INT_POWER */
    r = espera(1, tope);
    hw->irq(antes);
    x = hw->lee(TURBO_R_PLL);
    if (r) {                                   /* la PMU no lo confirmó: lo de antes o lo escrito */
        sin_conf = 1;
        conf_m = M_DE(esperado) > m ? M_DE(esperado) : m;
        puesto = x;
        nivel = M_DE(x) > TURBO_M_SERIE ? M_DE(x) : conf_m;
        return TBM_PENDIENTE;
    }
    if (x == v) { puesto = v; nivel = m > TURBO_M_SERIE ? m : 0; return TBM_OK; }
    if (M_DE(x) <= TURBO_M_SERIE) {            /* a 396 o menos: nada que bajar */
        puesto = 0; nivel = 0;
        return propio(x) ? TBM_LECTURA : TBM_SO;
    }
    if (bajable(x)) { puesto = x; nivel = M_DE(x); return TBM_LECTURA; }
    puesto = 0; nivel = M_DE(x);               /* arriba y con otros bits */
    return TBM_RARO;
}

/* De 396 (0x21020303, ya mirado) a m, de 12 en 12 MHz */
static int subir(int m)
{
    int k, r;
    puesto = TURBO_SERIE;
    for (k = TURBO_M_SERIE + 1; k <= m; k++) if ((r = paso(puesto, k)) != TBM_OK) return r;
    return TBM_OK;
}

/* De lo que puso Hakaru (puesto) a 396, de 12 en 12 MHz */
static int bajar(void)
{
    int k, r;
    for (k = M_DE(puesto) - 1; k >= TURBO_M_SERIE; k--) if ((r = paso(puesto, k)) != TBM_OK) return r;
    return TBM_OK;
}

/* El SO cambió el reloj (M ≤ 33): es suyo, no se toca. Y la marca, fuera */
static void del_so(void)
{
    nivel = 0; puesto = 0; pedido = 0; atascado = 0;
    motivo = TBM_SO;
    sin_marca = 1;
}

/* Lo escrito sin confirmar: las IRQ abiertas (sin teclas abajo) otro rato,
 * por si el SO lo atiende ahora. 1 si ya se limpió el pendiente */
static int confirma(void)
{
    int antes;
    suelta();
    antes = hw->irq(0);
    if (!espera(1, TB_TOPE_BAJA_T32)) sin_conf = 0;
    hw->irq(antes);
    return !sin_conf;
}

/* Tras un fallo: a 396 si lo que hay se sabe bajar; si es del SO (M ≤ 33),
 * se deja. Si no baja, está arriba con otros bits o lo escrito sigue sin
 * confirmar, atascado: el reloj puede seguir arriba y no se escribe nada */
static void vuelve(void)
{
    unsigned x;
    if (sin_conf) confirma();
    x = hw->lee(TURBO_R_PLL);
    if (!sin_conf && bajable(x)) {
        puesto = x;
        if (bajar() != TBM_OK) x = hw->lee(TURBO_R_PLL);
        else x = TURBO_SERIE;
    }
    if (sin_conf) { atascado = 1; nivel = M_DE(x) > TURBO_M_SERIE ? M_DE(x) : conf_m; pedido = 0; return; }
    if (M_DE(x) > TURBO_M_SERIE) { atascado = 1; nivel = M_DE(x); pedido = 0; return; }
    atascado = 0;
    nivel = 0;
    pedido = 0;
}

/* Atascado: se vuelve a intentar (el SO pudo tardar, o ya puso lo suyo). El
 * motivo se queda; si ya no está atascado, la marca, fuera */
static void reintenta(void)
{
    if (!atascado) return;
    vuelve();
    if (!atascado) sin_marca = 1;
}

/* El registro no es lo que puso Hakaru (puesto): del SO si M ≤ 33; si no, a
 * 396 si se sabe bajar, o atascado */
static void cambiado(unsigned x)
{
    if (M_DE(x) <= TURBO_M_SERIE) { del_so(); return; }
    vuelve();
    motivo = atascado && !bajable(hw->lee(TURBO_R_PLL)) ? TBM_RARO : TBM_LECTURA;
    sin_marca = 1;
}

/* La marca de cuelgue en ces_controles.tns (solo a 396): 1 si quedó escrita */
static int pon_marca(int m)
{
    int antes = marca;
    marca = m;
    if (marcador && marcador()) return 1;
    marca = antes;
    return 0;
}

static void falla(int r)
{
    vuelve();
    motivo = r;
    if (marca && !atascado) pon_marca(0);
}

/* A 396 lo que haya puesto Hakaru, si sigue siendo suyo. TBM_OK o el motivo
 * (atascado: no se sigue) */
static int a_serie(void)
{
    int r;
    unsigned x;
    reintenta();
    if (atascado) return motivo;
    if (!nivel) return TBM_OK;
    x = hw->lee(TURBO_R_PLL);
    if (x != puesto) { cambiado(x); return atascado ? motivo : TBM_OK; }
    if ((r = bajar()) != TBM_OK) { falla(r); return r; }
    return TBM_OK;
}

static struct {
    int fase, m, motivo, vueltas, pasadas, seg, etapa, vuelta, sub, trozo, esc;
    unsigned h, h_ref, rtc0;
    unsigned long hz0, nmem, kb;             /* kb: el patrón (queda al liberarlo) */
    unsigned long c0;                        /* la carga fija a 396, en cuentas de 32 kHz */
    unsigned *mem;
} pr;

static int probando(void) { return pr.fase == TBP_REFERENCIA || pr.fase == TBP_TURBO; }

int turbo_pon(int m)
{
    int r;
    if (probando()) return motivo = TBM_NIVEL;
    reintenta();
    if (atascado) return motivo;
    motivo = TBM_OK;
    if (m != 0 && !turbo_permite(m)) return motivo = TBM_NIVEL;
    if ((r = a_serie()) != TBM_OK) return r;
    pedido = 0;
    if (m == 0) {
        if (marca) pon_marca(0);
        if (motivo == TBM_SO) return TBM_SO;
        return TBM_OK;
    }
    if ((r = turbo_puede()) != TBM_OK) {
        motivo = r;
        if (marca) pon_marca(0);
        return r;
    }
    if (!pon_marca(m)) return motivo = TBM_MARCA;       /* sin la marca, no se sube */
    pedido = m;
    if ((r = subir(m)) != TBM_OK) { falla(r); return r; }
    return TBM_OK;
}

/* La puerta. Anidada, lo mismo que la de fuera. Pase lo que pase, nunca
 * con el reloj por encima de 396: lo que Hakaru sabe bajar se baja (aunque
 * no lo pusiera esta sesión: otra que salió atascada, u otro programa que
 * cambió solo M); lo demás no se escribe, ni con un reloj que no se sabe
 * calcular (en una CX II, turbo_mhz() = 0: D1 = 0) */
int turbo_pausa(void)
{
    int r;
    unsigned x;
    if (pausas++ > 0) return pausa_ok;
    pausado = 0;
    pausa_ok = 0;
    reintenta();
    if (atascado) return 0;
    if (nivel) {
        x = hw->lee(TURBO_R_PLL);
        if (x != puesto) cambiado(x);                       /* el reloj del SO: se escribe */
        else if ((r = bajar()) != TBM_OK) { vuelve(); motivo = r; sin_marca = 1; }
        else pausado = 1;
        if (atascado) return 0;
    }
    if (hw->cx2() && turbo_mhz() == 0) { motivo = TBM_RARO; return 0; }   /* no se sabe: no se escribe */
    if (turbo_mhz() > TURBO_MHZ(TURBO_M_SERIE)) {         /* arriba sin el turbo */
        x = hw->lee(TURBO_R_PLL);
        if (bajable(x)) {
            puesto = x;
            if (bajar() != TBM_OK) vuelve();
            if (!atascado) motivo = TBM_AJENO;
        } else motivo = TBM_RARO;                           /* no es de Hakaru: no se toca */
        if (atascado || turbo_mhz() > TURBO_MHZ(TURBO_M_SERIE)) return 0;
    }
    return pausa_ok = 1;
}

void turbo_sigue(void)
{
    int r;
    if (pausas == 0 || --pausas > 0 || !pausado) return;
    pausado = 0;
    if (!pedido) return;
    if ((r = turbo_puede()) != TBM_OK) { motivo = r; pedido = 0; sin_marca = 1; return; }   /* el USB, mientras */
    if ((r = subir(pedido)) != TBM_OK) { vuelve(); motivo = r; sin_marca = 1; }
}

int turbo_vigila(void)
{
    unsigned x;
    int r = 0;
    if (pausas) return 0;
    if (atascado) {                                         /* solo mirar: ¿lo cambió ya el SO? */
        if (!sin_conf && M_DE(hw->lee(TURBO_R_PLL)) <= TURBO_M_SERIE) { del_so(); r = 1; }   /* sin confirmar, 396
                                                               puede ser lo de Hakaru: reintenta */
    } else if (nivel && (x = hw->lee(TURBO_R_PLL)) != puesto) {
        cambiado(x);                                        /* la prueba lo ve en su paso */
        r = 1;
    }
    if (sin_marca && !probando()) {                         /* apagado por otro camino: fuera de la puerta */
        sin_marca = 0;
        if (!nivel && !pedido && !atascado && marca) pon_marca(0);
    }
    return r;
}

int turbo_recupera(int m, unsigned f)
{
    fallidos = f & 0x1Fu;
    marca = 0;
    if (m < TURBO_M_MIN || m > TURBO_M_MAX) return 0;
    fallidos |= 1u << (m - TURBO_M_MIN);
    return m;
}

/* --------------------------------------------------------------------------
 *  La prueba de estabilidad. La carga es determinista y del propio motor, sin
 *  tocar nada de lo que enseña la interfaz (el panel se abre desde cualquier
 *  pantalla, con una emulación o un Bode a la vista): el LU disperso del
 *  emulador (lu.c, en un LU propio) con un Newton de diodos con nl_exp y
 *  nl_pnjlim (nolin.c), un sistema en banda y la libm (exp, sin, sqrt, log);
 *  y un patrón sobre 2 MB de memoria (escrito al empezar la pasada y leído
 *  al acabar, invertido en las pasadas impares). Cada pasada da una FNV-1a
 *  de 32 bits de TODOS los resultados intermedios (cada solución de Newton,
 *  cada llamada a la libm): la aritmética no cambia con el reloj, así que a
 *  M tiene que salir la misma, bit a bit, y un bit que cambie en cualquiera
 *  de ellos se ve (Newton corregiría uno de una iteración, y una suma se
 *  comería los bits bajos).
 * ----------------------------------------------------------------------- */
static unsigned fnv(unsigned h, const void *p, unsigned long n)
{
    const unsigned char *b = (const unsigned char *)p;
    unsigned long i;
    for (i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
    return h;
}
static unsigned fnv_u(unsigned h, unsigned v) { return fnv(h, &v, sizeof v); }

#ifndef NDLESS
static void s_trabaja(void);             /* el simulador: el tiempo de la carga de mide_cpu */
/* El simulador: en la pasada falla_pasada a M, la 1.ª vuelta, un bit de un
 * resultado intermedio (turbo.h) */
static int iny_aqui(int donde)
{
    return sim.falla_pasada && pr.fase == TBP_TURBO && pr.pasadas + 1 == sim.falla_pasada && pr.vuelta == 0 &&
           sim.falla_donde == donde;
}
static double voltea(double v, int bit)
{
    unsigned char b[sizeof v];
    memcpy(b, &v, sizeof v);
    b[(bit >> 3) & 7] ^= (unsigned char)(1u << (bit & 7));        /* little-endian, como el ARM y el PC */
    memcpy(&v, b, sizeof v);
    return v;
}
#endif

#define TP_N  16                        /* la escalera de diodos             */
#define TP_NB 48                        /* el sistema en banda               */
static LU tl;
static double tv[TP_N], tb[TP_NB], tx[TP_NB];

/* Una escalera de 16 nodos: E con 100 Ω al primero, 220 Ω entre nodos y en
 * cada nodo un diodo y 10 kΩ a tierra. Newton con pnjlim, como el motor.
 * Cada solución entra en la FNV */
static unsigned u_newton(unsigned u, unsigned h)
{
    const double nvt = 0.02585, is = 1e-14, g0 = 1.0 / 100.0, gs = 1.0 / 220.0, gt = 1.0 / 10000.0;
    double e = 2.0 + 0.173 * (double)(u % 29u), vc = nl_vcrit(is, nvt);
    int i, it, lim, conv = 0;
    for (i = 0; i < TP_N; i++) tv[i] = 0.0;
    for (it = 0; it < 100 && !conv; it++) {
        lu_empieza(&tl, TP_N);
        for (i = 0; i < TP_N; i++) tb[i] = 0.0;
        lu_suma(&tl, 0, 0, g0);
        tb[0] = e * g0;
        for (i = 0; i + 1 < TP_N; i++) {
            lu_suma(&tl, i, i, gs); lu_suma(&tl, i + 1, i + 1, gs);
            lu_suma(&tl, i, i + 1, -gs); lu_suma(&tl, i + 1, i, -gs);
        }
        for (i = 0; i < TP_N; i++) {
            double d, ex = nl_exp(tv[i] / nvt, &d), gd = is * d / nvt;
            lu_suma(&tl, i, i, gd + gt);
            tb[i] += gd * tv[i] - is * (ex - 1.0);
        }
        if (!lu_factoriza(&tl)) break;
        lu_resuelve(&tl, tb, tx);
#ifndef NDLESS
        if (it == 0 && iny_aqui(0)) tx[0] = voltea(tx[0], sim.falla_bit);
#endif
        h = fnv(h, tx, TP_N * sizeof tx[0]);
        conv = 1;
        for (i = 0; i < TP_N; i++) {
            double vn = nl_pnjlim(tx[i], tv[i], nvt, vc, &lim);
            if (lim || fabs(vn - tv[i]) > 1e-9) conv = 0;
            tv[i] = vn;
        }
    }
    h = fnv(h, tv, sizeof tv);
    return fnv_u(h, (unsigned)it);
}

/* Un sistema de 48 en banda (±6) con coeficientes de un generador fijo,
 * diagonal dominante; tres segundos miembros */
static unsigned u_banda(unsigned u, unsigned h)
{
    unsigned s = 0x2545F491u ^ (u * 0x9E3779B9u);
    int i, j, r;
    lu_empieza(&tl, TP_NB);
    for (i = 0; i < TP_NB; i++) {
        double suma = 0.0;
        for (j = i - 6; j <= i + 6; j++) {
            double a;
            if (j < 0 || j >= TP_NB || j == i) continue;
            s = s * 1103515245u + 12345u;
            a = ((double)(s >> 8) / 16777216.0 - 0.5) * 1e-3 * (double)(1 + i % 7);
            lu_suma(&tl, i, j, a);
            suma += fabs(a);
        }
        lu_suma(&tl, i, i, suma + 1e-3);
    }
    if (!lu_factoriza(&tl)) return fnv_u(h, 0xDEADu);
    for (r = 0; r < 3; r++) {
        for (i = 0; i < TP_NB; i++) tb[i] = (double)((i * 7 + r * 3 + (int)(u % 11u)) % 11) - 5.0;
        lu_resuelve(&tl, tb, tx);
        h = fnv(h, tx, sizeof tx);
    }
    return h;
}

/* La libm: 128 vueltas de exp, sin, sqrt y log; cada resultado, en la FNV */
static unsigned u_libm(unsigned u, unsigned h)
{
    unsigned k;
    for (k = 0; k < 128; k++) {
        double x = 0.001 * (double)((u * 257u + k * 31u) % 5000u) - 2.5, y[4];
        y[0] = exp(x);
#ifndef NDLESS
        if (k == 0 && iny_aqui(1)) y[0] = voltea(y[0], sim.falla_bit);
#endif
        y[1] = sin(3.0 * x); y[2] = sqrt(fabs(x) + 1.0); y[3] = log(1.0 + x * x);
        h = fnv(h, y, sizeof y);
    }
    return h;
}

/* La palabra i del patrón (un mezclador de enteros: cada trozo se escribe y
 * se comprueba sin arrastrar estado) */
static unsigned patron_de(unsigned long i)
{
    unsigned x = (unsigned)i * 0x9E3779B1u + 0x7F4A7C15u;
    x ^= x >> 15; x *= 0x2C1B3C6Du;
    x ^= x >> 12; x *= 0x297A2D39u;
    return x ^ (x >> 15);
}

/* Un trozo del patrón: escrito o comprobado (las palabras que no son las
 * que se escribieron entran en la FNV: 0 si la memoria va bien) */
static void u_memoria(int leer)
{
    unsigned long i, a = pr.nmem * (unsigned long)pr.trozo / TB_TROZOS, b = pr.nmem * (unsigned long)(pr.trozo + 1) / TB_TROZOS;
    unsigned inv = (pr.pasadas + (pr.fase == TBP_TURBO)) & 1 ? 0xFFFFFFFFu : 0u, malas = 0;
    if (!leer) {
        for (i = a; i < b; i++) pr.mem[i] = patron_de(i) ^ inv;
#ifndef NDLESS
        if (sim.falla_mem && pr.fase == TBP_TURBO && pr.trozo == 3) pr.mem[a + 17] ^= 0x100u;   /* un bit que cambia */
#endif
        return;
    }
    for (i = a; i < b; i++) if (pr.mem[i] != (patron_de(i) ^ inv)) malas++;
    pr.h = fnv_u(pr.h, malas);
}

static void libera(void)
{
    free(pr.mem);
    pr.mem = 0;
    pr.nmem = 0;
}

/* Cuentas de 32 kHz en un segundo del RTC, de un salto al siguiente; 0 si
 * [esc] (y pr.esc: ese [esc] lo vio la medida); TB_SIN_RTC si el RTC no
 * avanza en ~4 s de cuentas */
#define TB_SIN_RTC 0xFFFFFFFFUL
static unsigned long cuenta_segundo(void)
{
    unsigned r0 = hw->rtc(), r, t, t0 = hw->t32(), ts = t0;
    int borde = 0;
    for (;;) {
        r = hw->rtc();
        t = hw->t32();
        if (salir && salir()) { pr.esc = 1; return 0; }
        if (r != r0) {
            if (borde) return t - t0 ? (unsigned long)(t - t0) : 1UL;
            borde = 1; r0 = r; t0 = t;
        }
        if (t - ts >= TB_SEG_MAX) return TB_SIN_RTC;
    }
}

/* La CPU: una carga fija de enteros (en la caché, sin la memoria), en cuentas
 * de 32 kHz. A M ha de tardar 33/M de lo que tarda a 396: el contador de
 * 32 kHz no va con la CPU (la prueba lo mira antes, cuenta_segundo) */
static volatile unsigned tb_sumidero;
static unsigned long mide_cpu(void)
{
    unsigned t0, x = 0x12345678u, y = 0;
    unsigned long i;
    t0 = hw->t32();
    for (i = 0; i < TB_CPU_VUELTAS; i++) { x = x * 1664525u + 1013904223u; y += x >> 7; }
    tb_sumidero = y;
#ifndef NDLESS
    if (hw == &hw_sim) s_trabaja();
#endif
    return (unsigned long)(hw->t32() - t0);
}

/* ¿Tardó c a M lo que toca, 33/M de c0 (±TB_CPU_MIL por mil)? */
static int cpu_a_m(unsigned long c, int m)
{
    double a = (double)c * (double)m, b = (double)pr.c0 * TURBO_M_SERIE;
    return fabs(a - b) * 1000.0 <= b * (double)TB_CPU_MIL;
}

/* El [esc] que vio la medida del reloj: si al acabar ya no está abajo
 * (termina pudo esperar a que se soltara, en suelta), el bucle no lo verá
 * y no hay nada que tragar (pr.esc, turbo.h) */
static void esc_visto(void)
{
    if (pr.esc && !(salir && salir())) pr.esc = 0;
}

static void termina(int mot)
{
    unsigned bit = 1u << (pr.m - TURBO_M_MIN);
    libera();
    pr.motivo = mot;
    if (mot == TBM_OK) { pr.fase = TBP_BIEN; motivo = TBM_OK; fallidos &= ~bit; return; }   /* el turbo sigue, a M;
                                                             el nivel, superado: ya no es fallido */
    pr.fase = TBP_MAL;
    if (mot == TBM_PRUEBA || mot == TBM_RELOJ32) fallidos |= bit;   /* lo que se vio A M */
    if (mot == TBM_SO && !atascado) del_so();
    else vuelve();
    pedido = 0;
    motivo = mot;
    if (marca && !atascado) pon_marca(0);                /* y los fallidos */
}

int turbo_prueba(int m)
{
    int r;
    if (probando()) return TBM_NIVEL;
    reintenta();
    if (atascado) return motivo;
    libera();
    memset(&pr, 0, sizeof pr);
    pr.m = m;
    pr.motivo = TBM_OK;
    if (!turbo_permite(m)) return pr.motivo = motivo = TBM_NIVEL;
    if ((r = a_serie()) != TBM_OK) return pr.motivo = r;
    pedido = 0;
    motivo = TBM_OK;
    if (marca) pon_marca(0);            /* a 396 hasta subir: la marca de antes, fuera */
    if ((r = turbo_puede()) != TBM_OK) return pr.motivo = motivo = r;
    for (pr.nmem = TB_MEM_BYTES / 4; pr.nmem >= (TB_MEM_BYTES / 16); pr.nmem /= 2) {
#ifndef NDLESS
        if (sim.sin_mem) continue;
#endif
        if ((pr.mem = (unsigned *)malloc(pr.nmem * 4)) != 0) break;
    }
    if (!pr.mem) { pr.nmem = 0; return pr.motivo = motivo = TBM_MEMORIA; }
    pr.kb = pr.nmem / 256;
    pr.fase = TBP_REFERENCIA;
    pr.h = 2166136261u;
    return TBM_OK;
}

/* Empieza una pasada: escribir el patrón, las vueltas y comprobarlo */
static void pasada_nueva(void)
{
    pr.h = 2166136261u;
    pr.etapa = 0; pr.trozo = 0; pr.vuelta = 0; pr.sub = 0;
}

/* Una unidad de la pasada; 1 si la pasada acabó */
static int unidad(void)
{
    if (pr.etapa == 0) {                                  /* el patrón, trozo a trozo */
        u_memoria(0);
        if (++pr.trozo == TB_TROZOS) { pr.etapa = 1; pr.trozo = 0; }
        return 0;
    }
    if (pr.etapa == 1) {                                  /* las vueltas */
        unsigned u = (unsigned)pr.vuelta;
        pr.h = pr.sub == 0 ? u_newton(u, pr.h) : pr.sub == 1 ? u_banda(u, pr.h) : u_libm(u, pr.h);
        if (++pr.sub < 3) return 0;
        pr.sub = 0;
        pr.vuelta++;
        if (pr.fase == TBP_REFERENCIA) {                  /* tantas como quepan en TB_REF_S */
            unsigned s = hw->rtc() - pr.rtc0;
            pr.seg = (int)s;
            if (s >= TB_REF_S) { pr.vueltas = pr.vuelta; pr.etapa = 2; }
        } else {
            pr.seg = (int)(hw->rtc() - pr.rtc0);
            if (pr.vuelta >= pr.vueltas) pr.etapa = 2;
        }
        return 0;
    }
    u_memoria(1);                                          /* comprobarlo */
    return ++pr.trozo == TB_TROZOS;
}

int turbo_prueba_paso(void)
{
    int r;
    unsigned long hz;
    if (!probando()) return 0;
    if (pr.fase == TBP_REFERENCIA && !pr.hz0) {           /* lo primero: el contador de 32 kHz, a 396 */
        pr.hz0 = cuenta_segundo();
        /* sin medir a 396 no se sube: ni el nivel es culpable ni el
         * contador «cambió con la CPU» (no ha cambiado) */
        if (!pr.hz0 || pr.hz0 == TB_SIN_RTC) { termina(pr.hz0 ? TBM_SIN_RELOJ : TBM_CANCELADA); esc_visto(); return 0; }
        pr.c0 = mide_cpu();                               /* y la CPU a 396 */
        if (pr.c0 < TB_CPU_MIN) { termina(TBM_SIN_RELOJ); return 0; }
        pr.rtc0 = hw->rtc();
        pasada_nueva();
        return 1;
    }
    /* el nivel cambió a mitad, o se atascó: el SO (el USB), o la puerta de una
     * escritura que no pudo bajar o volver a subir (su motivo, que dejó puesto) */
    if (pr.fase == TBP_TURBO && (nivel != pr.m || atascado)) { termina(motivo != TBM_OK ? motivo : TBM_SO); return 0; }
    if (!unidad()) return 1;
    if (pr.fase == TBP_REFERENCIA) {                      /* la referencia hecha: a M */
        pr.h_ref = pr.h;
        if (!pon_marca(pr.m)) { termina(TBM_MARCA); return 0; }
        pedido = pr.m;
        if ((r = subir(pr.m)) != TBM_OK) { termina(r); return 0; }
        hz = cuenta_segundo();                            /* ¿el contador de 32 kHz va con la CPU? */
        if (!hz) { termina(TBM_CANCELADA); esc_visto(); return 0; }
        if (hz == TB_SIN_RTC || (hz > pr.hz0 ? hz - pr.hz0 : pr.hz0 - hz) * 1000UL > pr.hz0 * TB_RELOJ_MIL) { termina(TBM_RELOJ32); return 0; }
        if (!cpu_a_m(mide_cpu(), pr.m)) { termina(TBM_CPU); return 0; }   /* el registro cambió y la CPU no */
        pr.fase = TBP_TURBO;
        pr.rtc0 = hw->rtc();
        pr.seg = 0;
        pasada_nueva();
        return 1;
    }
    if (pr.h != pr.h_ref) { termina(TBM_PRUEBA); return 0; }
    pr.pasadas++;
    if (pr.seg >= TB_TURBO_S) { termina(TBM_OK); return 0; }
    pasada_nueva();
    return 1;
}

void turbo_prueba_cancela(void)
{
    if (probando()) termina(TBM_CANCELADA);
}

void turbo_prueba_estado(TurboPrueba *p)
{
    p->fase = pr.fase;
    p->m = pr.m;
    p->vueltas = pr.fase == TBP_REFERENCIA ? pr.vuelta : pr.vueltas;
    p->pasadas = pr.pasadas;
    p->seg = pr.seg;
    p->seg_total = pr.fase == TBP_REFERENCIA ? TB_REF_S : TB_TURBO_S;
    p->kb = pr.kb;
    p->motivo = pr.motivo;
    p->esc = pr.esc;
}

void turbo_sale(void)
{
    unsigned x;
    if (probando()) termina(TBM_CANCELADA);
    libera();
    pausas = 0;
    pausado = 0;
    reintenta();
    if (nivel && !atascado) {
        x = hw->lee(TURBO_R_PLL);
        if (x != puesto) cambiado(x);
        else if (bajar() != TBM_OK) vuelve();
    }
    pedido = 0;
    if (marca && !atascado) pon_marca(0);
}

/* --------------------------------------------------------------------------
 *  El simulador de la PMU (solo el PC): los registros, el pendiente que el
 *  «SO» limpia si las IRQ están abiertas, el USB que pisa el valor, las
 *  lecturas que no cuadran, las teclas y los relojes (32 kHz y RTC) en una
 *  hora propia. Guarda cada escritura (con cómo estaban las IRQ) y el M en
 *  cada archivo
 * ----------------------------------------------------------------------- */
#ifndef NDLESS
static long sim_quedan;

/* El M al que va la CPU de verdad: el del registro (Firebird: al escribir),
 * salvo que la PMU no lo haya llevado a la CPU (pll_quieta, no_toma_pend) */
static int s_cpu(void) { return sim.cpu_m ? sim.cpu_m : M_DE(sim.pll); }

static int      s_cx2(void) { return sim.cx2; }
static unsigned s_lee(unsigned d)
{
    if (d == TURBO_R_PLL) return sim.pll;
    if (d == TURBO_R_DIV) return sim.div;
    if (d == TURBO_R_FUENTE) return sim.fuente;
    if (d == TURBO_R_PEND) {
        sim.esperas++;
        if (sim.irq_abiertas && sim.usb_tras > 0 && --sim.usb_tras == 0) { sim.pll = 0x18020303u; sim.cpu_m = 0; }   /* el USB */
        if ((sim.pend & 1u) && sim.irq_abiertas && sim.atiende >= 0 && --sim_quedan <= 0) {
            sim.pend &= ~1u;                              /* el SO atiende INT_POWER */
            if (sim.usb_en && sim.nesc == sim.usb_en) { sim.pll = 0x18020303u; sim.cpu_m = 0; }   /* ...y era el USB */
        }
        return sim.pend;
    }
    return 0;
}
static void s_pon(unsigned v)
{
    int cpu = s_cpu();                                    /* la CPU, antes */
    if (sim.nesc < (int)(sizeof sim.esc / sizeof sim.esc[0])) {
        sim.esc[sim.nesc].dir = TURBO_R_PLL;
        sim.esc[sim.nesc].v = v;
        sim.esc[sim.nesc].irq_abiertas = sim.irq_abiertas;
        sim.esc[sim.nesc].pend = (int)(sim.pend & 1u);
    }
    sim.nesc++;
    if (!(sim.pega_m && M_DE(v) == sim.pega_m))            /* pega_m: la PMU no lo toma */
        sim.pll = sim.mal_m && M_DE(v) == sim.mal_m ? v - (1u << 24) :
                  sim.sube_m && M_DE(v) == sim.sube_m ? v + (1u << 24) : v;
    /* la CPU sigue al registro, salvo con la PMU que no la cambia, o que no
     * toma una petición con la anterior sin atender */
    sim.cpu_m = sim.pll_quieta || (sim.no_toma_pend && (sim.pend & 1u)) ? cpu : 0;
    sim.pend |= 1u;
    sim_quedan = sim.nesc == 1 && sim.atiende_1 > 0 ? sim.atiende_1 : sim.atiende;
}
static int s_irq(int m)
{
    int antes = sim.irq_abiertas ? 0 : 0xC0;
    sim.irq_abiertas = m == 0;
    if (m == 0 && sim.tecla != 0) sim.abre_con_tecla++;
    return antes;
}
static unsigned s_t32(void)
{
    sim.t += sim.t32_paso;
    if (sim.t32_cpu) return (unsigned)((unsigned long long)sim.t * (sim.pll >> 24) / TURBO_M_SERIE);
    return sim.t;
}
static unsigned s_rtc(void) { sim.t += sim.rtc_paso; return sim.t / 32768u; }
static int s_tecla(void)
{
    if (sim.tecla > 0) { sim.tecla--; return 1; }
    return sim.tecla < 0;
}
static const TurboHw hw_sim = { s_cx2, s_lee, s_pon, s_irq, s_t32, s_rtc, s_tecla };

/* La carga de mide_cpu: 0,1 s a 396 (3277 cuentas), a la CPU de verdad */
static void s_trabaja(void)
{
    int m = s_cpu();
    if (m > 0) sim.t += 3277u * (unsigned)TURBO_M_SERIE / (unsigned)m;
}

TurboSim *turbo_sim(void) { return &sim; }

void turbo_sim_reinicia(void)
{
    libera();
    memset(&pr, 0, sizeof pr);
    sim = sim0;                                           /* 0x810 como Firebird: 0x11; las IRQ,
                                                             cerradas (Ndless); el RTC, 1/4 de
                                                             segundo por lectura */
    sim.t = 1000u * 32768u;
    sim_quedan = 0;
    hw = &hw_sim;
    pedido = nivel = motivo = atascado = alto = marca = pausas = pausado = pausa_ok = sin_marca = sin_conf = conf_m = 0;
    puesto = fallidos = patron = 0;
}

void turbo_sim_archivo(const char *ruta)
{
    const char *n = ruta, *p;
    for (p = ruta; *p; p++) if (*p == '/') n = p + 1;
    if (sim.narch < (int)(sizeof sim.arch / sizeof sim.arch[0])) {
        TurboArch *a = &sim.arch[sim.narch];
        a->m = (int)(hw->lee(TURBO_R_PLL) >> 24);
        a->cpu = s_cpu();
        a->pend = (int)(sim.pend & 1u);
        a->nesc = sim.nesc;
        strncpy(a->nombre, n, sizeof a->nombre - 1);
        a->nombre[sizeof a->nombre - 1] = 0;
    }
    sim.narch++;
}
#endif
