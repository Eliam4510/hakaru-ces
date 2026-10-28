/* ============================================================================
 *  test_opt_turbo.inc  -  [75] El turbo (experimental): el reloj de la CX II
 *  de 396 a 408-456 MHz (turbo.c), con el simulador de la PMU del arnés
 *  (turbo_sim_*: los registros, el pendiente que el «SO» limpia si las IRQ
 *  están abiertas, el USB, las lecturas que no cuadran y los relojes).
 *
 *    T1  precondiciones: no ser CX II, el USB (M = 24) y patrones raros: no
 *        se escribe nada y se dice por qué; sin CX II, el panel es el de antes
 *    T2  límites: nunca más de 38; 37-38 solo tras el aviso
 *    T3  la secuencia exacta: solo 0x90140030, solo los bits 24-31, de 12 en
 *        12 MHz, con las IRQ cerradas al escribir y abiertas al esperar
 *    T4  el pendiente que no se limpia: el tope (en cuentas de 32 kHz, no en
 *        vueltas), la vuelta a 396 o, si no se puede, atascado
 *    T5  la lectura que no cuadra: vuelta a 396
 *    T6  a 396 antes de CADA escritura (todas las de archivo.c, el grabador y
 *        Controles por la interfaz) y de vuelta después; al salir
 *    T7  el SO que cambia el registro (el USB): no se restaura encima
 *    T8  la marca de cuelgue: a 396 antes de subir; el arranque tras un
 *        cuelgue; el nivel fallido y su aviso; el formato de Controles
 *    T9  la prueba de estabilidad: bien, un resultado distinto, un bit de la
 *        memoria, [esc], el contador de 32 kHz que va con la CPU, sin memoria,
 *        el USB a mitad
 *    T10 el reloj: la fórmula de Linux; el banco dice a qué frecuencia midió
 *    T11 la barra: las marcas (turbo, ctrl, grabando) y un título largo
 *
 *  Va al final de la batería: sus BMP (out/tb_*.bmp) no corren la numeración
 *  y no emula nada (no añade huellas). Deja el simulador como estaba («no es
 *  una CX II») y sin ces_controles.tns. Usa ce_*, cm_*, r3u_abre, r4_vuelca
 *  (test_entrada.inc y compañía).
 * ========================================================================= */

#include "turbo.h"

static TurboSim *tb_s;

/* Una CX II a 396 sin USB, el turbo como al arrancar (un programa nuevo), la
 * interfaz limpia, sin ces_controles.tns y los registros del simulador a 0 */
static void tb_limpio(void)
{
    turbo_sim_reinicia();
    tb_s = turbo_sim();
    arch_bytes_borrar("/ces_controles.tns");
    ui_init();
    tb_s->nesc = 0;
    tb_s->narch = 0;
}

/* Las M escritas, en texto («34 35 36») */
static const char *tb_ms(void)
{
    static char t[256];
    int i, n = 0, k = tb_s->nesc < 64 ? tb_s->nesc : 64;
    t[0] = 0;
    for (i = 0; i < k && n < 240; i++) n += sprintf(t + n, i ? " %u" : "%u", tb_s->esc[i].v >> 24);
    return t;
}

/* Todas las escrituras de la PMU: en 0x90140030, con solo M distinto de
 * 0x21020303, M en 33..38 y las IRQ cerradas; y, si seguidas, de una en una
 * (12 MHz) desde 396 */
static int tb_escrituras_bien(int seguidas)
{
    int i, m_ant = TURBO_M_SERIE, n = tb_s->nesc < 64 ? tb_s->nesc : 64;
    for (i = 0; i < n; i++) {
        const TurboEsc *e = &tb_s->esc[i];
        int m = (int)(e->v >> 24);
        if (e->dir != TURBO_R_PLL || (e->v & 0x00FFFFFFu) != (TURBO_SERIE & 0x00FFFFFFu)) return 0;
        if (m < TURBO_M_SERIE || m > TURBO_M_MAX || e->irq_abiertas) return 0;
        if (seguidas && m != m_ant + 1 && m != m_ant - 1) return 0;
        m_ant = m;
    }
    return 1;
}

/* Al panel Controles desde el menú y a la fila del turbo */
static void tb_panel(void)
{
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL, T_ABAJO, T_ABAJO, T_ABAJO);
}

/* La prueba hasta que acabe (ticks, como el bucle); cuántos */
static long tb_prueba_entera(void)
{
    long i;
    for (i = 0; i < 100000L && turbo_prueba_paso(); i++) {}
    return i;
}

/* ---------------------------------------------------------------------------
 *  T1. Las precondiciones
 * ------------------------------------------------------------------------ */
static void tb_precondiciones(void)
{
    static const struct { int cx2; unsigned pll, div, fuente; int motivo; const char *que; } C[] = {
        { 0, TURBO_SERIE, 0, 0x11u, TBM_NO_CX2, "no es una CX II" },
        { 1, 0x18020303u, 0, 0x11u, TBM_USB, "con el USB: M = 24 (288 MHz)" },
        { 1, 0x22020303u, 0, 0x11u, TBM_PATRON, "ya a 408 (M = 34)" },
        { 1, 0x21010303u, 0, 0x11u, TBM_PATRON, "D1 = 1" },
        { 1, 0x21020313u, 0, 0x11u, TBM_PATRON, "el bit 4 de 0x90140030 (24 MHz)" },
        { 1, 0x21020301u, 0, 0x11u, TBM_PATRON, "otros bits bajos de 0x90140030" },
        { 1, 0x21420303u, 0, 0x11u, TBM_PATRON, "un bit entre D1 y M" },
        { 1, TURBO_SERIE, 0x00100000u, 0x11u, TBM_DIV, "D2 = 1" },
        { 1, TURBO_SERIE, 0x00000002u, 0x11u, TBM_DIV, "el bit 1 de 0x90140020 (dormir)" },
        { 1, TURBO_SERIE, 0, 0x01u, TBM_FUENTE, "el bit 4 de 0x90140810 a 0 (48 MHz)" },
    };
    const char *q = "[75] T1 precondiciones";
    char det[200];
    EntAjustes a;
    int i, r, r2;

    tb_limpio();
    chk_cond(q, "de f" S_aa "brica (0x21020303, D2 = 0, bit 4 de 0x810, CX II): se puede", turbo_puede() == TBM_OK);
    for (i = 0; i < (int)(sizeof C / sizeof C[0]); i++) {
        tb_limpio();
        tb_s->cx2 = C[i].cx2; tb_s->pll = C[i].pll; tb_s->div = C[i].div; tb_s->fuente = C[i].fuente;
        r = turbo_pon(TURBO_M_MIN);
        r2 = turbo_prueba(TURBO_M_MIN);
        sprintf(det, "%s: no se escribe nada y dice por qu" S_ee " (%d y %d)", C[i].que, r, r2);
        chk_cond(q, det, r == C[i].motivo && r2 == C[i].motivo && tb_s->nesc == 0 && tb_s->pll == C[i].pll &&
                 tb_s->div == C[i].div && turbo_nivel() == 0 && turbo_pedido() == 0 && tb_s->narch == 0);
    }

    /* sin CX II, el panel es el de siempre: tres filas, sin el turbo */
    tb_limpio();
    tb_s->cx2 = 0;
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    chk_cond(q, "sin CX II, el panel no ense" S_nn "a el turbo", ui_pantalla() == P_CONTROLES && !ve("Turbo") && !ve("MHz"));
    TECLAS(T_ABAJO, T_ABAJO, T_ABAJO, T_DER, T_LETRA('P'));
    ent_ajustes(&a);
    chk_cond(q, "...y sus flechas dan la vuelta en tres filas (la 4a es la 1a: deslizar); [P], nada",
             a.deslizar == !DESLIZAR_OMISION && tb_s->nesc == 0 && ui_pantalla() == P_CONTROLES);
    TECLAS(T_ESC);
    ent_ajustes_omision();

    /* con el USB, en el panel: 288 MHz, el aviso de activar y el porqué */
    tb_limpio();
    tb_s->pll = 0x18020303u;
    tb_panel();
    chk_cond(q, "con el USB, la fila del turbo: \"Reloj: 288 MHz (con el USB)\"", ve("Reloj: 288 MHz (con el USB)") && ve(S_LARR " no " S_RARR));
    TECLAS(T_DER);
    chk_cond(q, "[der]: primero el aviso de lo que es", ve("Turbo a 408 MHz. Experimental y no es de TI") && tb_s->nesc == 0);
    TECLAS(T_ENTER);
    chk_cond(q, "[enter]: no se toca (USB) y lo dice", ve("Con el USB (288 MHz) no: descon" S_ee "ctalo") &&
             tb_s->nesc == 0 && turbo_nivel() == 0 && ve(S_LARR " no " S_RARR));
    r4_vuelca("tb_usb");
    tb_s->pll = 0x21020301u;
    TECLAS(T_DER);
    chk_cond(q, "un patr" S_oo "n raro: \"Reloj desconocido (0x21020301)\" y nada escrito",
             ve("Reloj desconocido (0x21020301): no se toca.") && tb_s->nesc == 0);
    TECLAS(T_ESC);
}

/* ---------------------------------------------------------------------------
 *  T2. Los límites
 * ------------------------------------------------------------------------ */
static void tb_limites(void)
{
    const char *q = "[75] T2 l" S_ii "mites";
    int r;

    tb_limpio();
    r = turbo_pon(39);
    chk_cond(q, "M = 39 (468 MHz): no", r == TBM_NIVEL && tb_s->nesc == 0);
    chk_cond(q, "M = 33, 0 negativo o 255: no son niveles",
             turbo_pon(TURBO_M_SERIE) == TBM_NIVEL && turbo_pon(-3) == TBM_NIVEL && turbo_pon(255) == TBM_NIVEL && tb_s->nesc == 0);
    chk_cond(q, "444 y 456 sin el aviso: no", turbo_pon(37) == TBM_NIVEL && turbo_pon(38) == TBM_NIVEL &&
             turbo_prueba(37) == TBM_NIVEL && tb_s->nesc == 0 && !turbo_permite(37));
    turbo_alto(1);
    chk_cond(q, "con el aviso, tampoco m" S_aa "s de 456", turbo_pon(39) == TBM_NIVEL && turbo_pon(40) == TBM_NIVEL &&
             turbo_pon(63) == TBM_NIVEL && tb_s->nesc == 0);
    r = turbo_pon(38);
    chk_cond(q, "con el aviso, 456: de 408 en 408... hasta 38", r == TBM_OK && !strcmp(tb_ms(), "34 35 36 37 38") &&
             tb_s->pll == 0x26020303u && turbo_mhz() == 456 && turbo_nivel() == 38 && tb_escrituras_bien(1));
    turbo_pon(0);
    chk_cond(q, "y apagado, de vuelta de 12 en 12", !strcmp(tb_ms(), "34 35 36 37 38 37 36 35 34 33") && tb_s->pll == TURBO_SERIE);

    /* en el panel: 432, [der] pide el aviso; [esc] se queda; [enter] sube */
    tb_limpio();
    tb_panel();
    chk_cond(q, "panel de una CX II a 396: la fila del turbo, apagada, y lo que es",
             ve("Turbo (experimental)") && ve(S_LARR " no " S_RARR) && ve("Reloj: 396 MHz (de serie)") &&
             ve("Experimental y no es de TI: puede colgar la") && ve("[izq/der] nivel  [P] probar  [enter] listo"));
    r4_vuelca("tb_panel");
    TECLAS(T_DER);
    r4_vuelca("tb_aviso");
    TECLAS(T_ENTER, T_DER, T_DER);
    chk_cond(q, "panel: 408, 420, 432", turbo_nivel() == 36 && ve(S_LARR " 432 MHz " S_RARR) && ve("Reloj: 432 MHz (turbo)"));
    r4_vuelca("tb_activo");
    tb_s->nesc = 0;
    TECLAS(T_DER);
    chk_cond(q, "de 432, [der]: el aviso de 444 (y nada escrito)", ve("444 MHz pasa del tope de 432") && tb_s->nesc == 0 && turbo_nivel() == 36);
    r4_vuelca("tb_aviso_alto");
    TECLAS(T_ESC);
    chk_cond(q, "[esc]: se queda en 432", turbo_nivel() == 36 && tb_s->nesc == 0 && !ve("444 MHz pasa"));
    TECLAS(T_DER, T_ENTER);
    chk_cond(q, "[der] y [enter]: 444", turbo_nivel() == 37 && tb_s->pll == 0x25020303u && ve(S_LARR " 444 MHz " S_RARR));
    TECLAS(T_DER);
    chk_cond(q, "456, ya sin aviso", turbo_nivel() == 38 && !ve("pasa del tope"));
    TECLAS(T_DER);
    chk_cond(q, "de 456, [der]: apagado (396)", turbo_nivel() == 0 && tb_s->pll == TURBO_SERIE && ve(S_LARR " no " S_RARR));
    TECLAS(T_IZQ);
    chk_cond(q, "[izq] desde \"no\", con el aviso ya dado: 456", turbo_nivel() == 38);
    TECLAS(T_ESC);
    turbo_sale();
    tb_limpio();                                           /* un programa nuevo: sin el aviso */
    tb_panel();
    TECLAS(T_IZQ, T_ENTER);
    chk_cond(q, "otra sesi" S_oo "n: [izq] desde \"no\" va a 432 (el tope sin el aviso)", turbo_nivel() == 36);
    TECLAS(T_ESC);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  T3. La secuencia exacta
 * ------------------------------------------------------------------------ */
static void tb_secuencia(void)
{
    const char *q = "[75] T3 secuencia";
    unsigned char b[16];
    int r;

    tb_limpio();
    r = turbo_pon(36);
    chk_cond(q, "432: 34, 35 y 36 en 0x90140030, solo los bits 24-31, con las IRQ cerradas al escribir",
             r == TBM_OK && !strcmp(tb_ms(), "34 35 36") && tb_escrituras_bien(1) && tb_s->pll == 0x24020303u &&
             turbo_nivel() == 36 && turbo_mhz() == 432);
    chk_cond(q, "...y abiertas al esperar el pendiente (el \"SO\" solo lo limpia as" S_ii "); luego, cerradas como en Ndless",
             tb_s->esperas > 0 && tb_s->irq_abiertas == 0 && !(tb_s->pend & 1u));
    chk_cond(q, "0x90140020 y 0x90140810, sin tocar", tb_s->div == 0 && tb_s->fuente == 0x11u);
    tb_s->nesc = 0;
    chk_cond(q, "la puerta: a 396 de 12 en 12", turbo_pausa() == 1 && !strcmp(tb_ms(), "35 34 33") && tb_s->pll == TURBO_SERIE);
    turbo_sigue();
    chk_cond(q, "y de vuelta a 432", !strcmp(tb_ms(), "35 34 33 34 35 36") && turbo_nivel() == 36 && tb_escrituras_bien(0));
    tb_s->nesc = 0;
    turbo_pausa(); turbo_pausa(); turbo_sigue();
    chk_cond(q, "anidadas: baja una vez y no sube hasta la " S_uu "ltima", !strcmp(tb_ms(), "35 34 33"));
    turbo_sigue();
    chk_cond(q, "...ahora s" S_ii, !strcmp(tb_ms(), "35 34 33 34 35 36"));
    tb_s->nesc = 0;
    tb_s->narch = 0;
    r = turbo_pon(35);
    chk_cond(q, "de 432 a 420: por 396, con la marca escrita all" S_ii " (y antes de subir)",
             r == TBM_OK && !strcmp(tb_ms(), "35 34 33 34 35") && tb_s->narch == 1 && tb_s->arch[0].m == TURBO_M_SERIE &&
             tb_s->arch[0].nesc == 3 && !strcmp(tb_s->arch[0].nombre, "ces_controles.tns"));
    tb_s->nesc = 0;
    r = turbo_pon(0);
    chk_cond(q, "apagado: 34, 33; la marca, borrada (ces_controles.tns de 8 bytes)",
             r == TBM_OK && !strcmp(tb_ms(), "34 33") && turbo_marca() == 0 && arch_bytes_leer("/ces_controles.tns", b, 16) == 8);
    tb_s->nesc = 0;
    chk_cond(q, "apagado, la puerta no toca nada", turbo_pausa() == 1 && tb_s->nesc == 0);
    turbo_sigue();
    chk_cond(q, "...ni al seguir", tb_s->nesc == 0 && turbo_nivel() == 0);
}

/* ---------------------------------------------------------------------------
 *  T4. El pendiente que no se limpia
 * ------------------------------------------------------------------------ */
static void tb_pendiente(void)
{
    const char *q = "[75] T4 pendiente";
    char det[160];
    unsigned char b[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    long e0, n1, n5;
    int r;

    /* nunca: el tope al subir, y lo escrito queda sin confirmar: nunca se
     * escribe 33 encima del pendiente (la revisión final); se espera otro
     * 0,5 s (confirma) y queda atascado a 408, con su marca */
    tb_limpio();
    tb_s->atiende = -1;
    e0 = tb_s->esperas;
    r = turbo_pon(34);
    n1 = tb_s->esperas - e0;
    sprintf(det, "nunca se limpia: el tope al subir y no se escribe encima: atascado a 408 (%ld lecturas)", n1);
    chk_cond(q, det, r == TBM_PENDIENTE && turbo_atascado() && turbo_nivel() == 34 && !strcmp(tb_ms(), "34") &&
             tb_s->pll == 0x22020303u && turbo_pedido() == 0 && turbo_marca() == 34);

    /* la PMU no toma el 33 (pega_m): no puede volver, atascado */
    tb_limpio();
    turbo_pon(34);
    tb_s->pega_m = TURBO_M_SERIE;
    tb_s->nesc = 0;
    r = turbo_pon(0);
    chk_cond(q, "la PMU no toma el 33: no puede volver, atascado a 408", r == TBM_LECTURA && turbo_atascado() &&
             turbo_nivel() == 34 && tb_s->pll == 0x22020303u && turbo_pedido() == 0 && !strcmp(tb_ms(), "33 33"));
    tb_s->narch = 0;
    chk_cond(q, "atascado, no se escribe ni se borra nada (no pasa de la puerta)",
             !arch_bytes_guardar("/ces_tb_prueba.tns", b, 8) && arch_bytes_leer("/ces_tb_prueba.tns", b, 8) < 0 &&
             !arch_sesion_borrar() && tb_s->narch == 0);
    tb_s->nesc = 0;
    chk_cond(q, "atascado, turbo_pon y la prueba solo reintentan bajar (nunca suben)",
             turbo_pon(36) == TBM_LECTURA && turbo_prueba(34) == TBM_LECTURA && turbo_atascado() &&
             !strcmp(tb_ms(), "33 33") && tb_s->pll == 0x22020303u);
    tb_panel();
    chk_cond(q, "el panel lo dice, y la marca de 408 sigue en la barra",
             ve("No se pudo volver a 396 MHz: no se guarda nada.") && ve("408 MHz"));
    r4_vuelca("tb_atascado");
    TECLAS(T_ESC);
    tb_s->pega_m = 0;
    r = turbo_pon(0);
    chk_cond(q, "cuando la PMU ya lo toma, el reintento baja: a 396, apagado y sin la marca",
             r == TBM_OK && !turbo_atascado() && turbo_nivel() == 0 && tb_s->pll == TURBO_SERIE && turbo_marca() == 0 &&
             arch_bytes_leer("/ces_controles.tns", b, 8) == 8);

    /* tarde: la primera escritura no se atiende a tiempo; al volver, sí */
    tb_limpio();
    tb_s->atiende_1 = 1000;
    r = turbo_pon(34);
    chk_cond(q, "se limpia tarde: el tope, vuelta a 396 y apagado", r == TBM_PENDIENTE && !turbo_atascado() &&
             turbo_nivel() == 0 && turbo_pedido() == 0 && tb_s->pll == TURBO_SERIE && !strcmp(tb_ms(), "34 33"));
    chk_cond(q, "...y la marca, borrada", turbo_marca() == 0 && arch_bytes_leer("/ces_controles.tns", b, 8) == 8);

    /* el tope es de tiempo (cuentas de 32 kHz), no de vueltas: con un
     * contador que avanza 5 por lectura, 5 veces menos lecturas */
    tb_limpio();
    tb_s->atiende = -1; tb_s->t32_paso = 5;
    e0 = tb_s->esperas;
    turbo_pon(34);
    n5 = tb_s->esperas - e0;
    sprintf(det, "el tope, en cuentas de 32 kHz: %ld lecturas con 1 cuenta por lectura y %ld con 5", n1, n5);
    chk_cond(q, det, n1 >= 655 + 16384 && n1 <= 655 + 16384 + 30 &&      /* 20 ms subiendo; 0,5 s confirmando */
             n5 * 5 >= n1 - 15 && n5 * 5 <= n1 + 15);

    /* con las IRQ abiertas al llamar (no es lo de Ndless): las deja abiertas */
    tb_limpio();
    tb_s->irq_abiertas = 1;
    r = turbo_pon(34);
    chk_cond(q, "llamado con las IRQ abiertas: sube y quedan abiertas (como estaban)",
             r == TBM_OK && turbo_nivel() == 34 && tb_s->irq_abiertas == 1);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  T5. La lectura que no cuadra
 * ------------------------------------------------------------------------ */
static void tb_lectura(void)
{
    const char *q = "[75] T5 lectura";
    int r;
    tb_limpio();
    tb_s->mal_m = 35;
    r = turbo_pon(36);
    chk_cond(q, "escribe 35 y lee 34: vuelta a 396 desde lo le" S_ii "do, apagado", r == TBM_LECTURA &&
             !strcmp(tb_ms(), "34 35 33") && tb_s->pll == TURBO_SERIE && turbo_nivel() == 0 && turbo_pedido() == 0 &&
             !turbo_atascado() && turbo_marca() == 0 && tb_escrituras_bien(0));
    tb_limpio();
    tb_s->mal_m = 35;
    tb_panel();
    TECLAS(T_DER, T_ENTER, T_DER);
    chk_cond(q, "en el panel: \"El reloj no qued" S_oo " como se escribi" S_oo "\"", ve("El reloj no qued" S_oo " como se escribi" S_oo) &&
             ve(S_LARR " no " S_RARR) && tb_s->pll == TURBO_SERIE);
    TECLAS(T_ESC);
}

/* ---------------------------------------------------------------------------
 *  T6. A 396 antes de cada escritura, y de vuelta después
 * ------------------------------------------------------------------------ */
static int tb_n0, tb_e0, tb_malas;

/* Antes de una operación: los registros del simulador a 0 (los archivos de
 * antes que no fueron a 396 quedan en tb_malas) */
static void tb_antes(void)
{
    int i;
    for (i = 0; i < tb_s->narch && i < 32; i++) if (tb_s->arch[i].m != TURBO_M_SERIE) tb_malas++;
    tb_s->narch = tb_s->nesc = 0;
    tb_n0 = tb_e0 = 0;
}

/* La última operación de archivo: una (con ese nombre), a 396, bajando y
 * subiendo de 12 en 12 alrededor; y el turbo, otra vez a 432 */
static void tb_ruta(const char *q, const char *que, const char *nombre, int ok)
{
    char det[200];
    const TurboArch *a = &tb_s->arch[tb_n0];
    int i, bien = 1;
    static const unsigned M[6] = { 35, 34, 33, 34, 35, 36 };
    if (tb_s->nesc - tb_e0 != 6) bien = 0;
    for (i = 0; bien && i < 6; i++) if (tb_s->esc[tb_e0 + i].v >> 24 != M[i]) bien = 0;
    sprintf(det, "%s (%s): a 396 al escribir, y otra vez a 432", que, nombre);
    chk_cond(q, det, ok && tb_s->narch == tb_n0 + 1 && a->m == TURBO_M_SERIE && a->nesc == tb_e0 + 3 &&
             !strcmp(a->nombre, nombre) && bien && turbo_nivel() == 36 && tb_s->pll == 0x24020303u);
}

static void tb_escrituras(void)
{
    const char *q = "[75] T6 escrituras";
    static Circuito c;
    unsigned char b[16] = { 'C', 'E', 'S', 'M', 1, 0, 0, 0, 0, 0, 0, 0 };
    int n, bien;

    tb_limpio();
    limpia_mis();
    ejemplo_cargar(0, &c);
    tb_malas = 0;
    tb_antes();
    chk_cond(q, "a 432", turbo_pon(36) == TBM_OK);
    tb_antes(); tb_ruta(q, "guardar un problema", "ces_01.tns", arch_guardar(0, &c));
    tb_antes(); tb_ruta(q, "la sesi" S_oo "n", "ces_sesion.tns", arch_sesion_guardar(&c, 0, 1, 2));
    ESP_TB_ARCH
    tb_antes(); tb_ruta(q, "un archivo de bytes", "ces_muestras.tns", arch_bytes_guardar("/ces_muestras.tns", b, 12));
    tb_antes(); tb_ruta(q, "borrar uno de bytes", "ces_muestras.tns", arch_bytes_borrar("/ces_muestras.tns"));
    tb_antes(); tb_ruta(q, "borrar la sesi" S_oo "n", "ces_sesion.tns", arch_sesion_borrar());
    tb_antes(); tb_ruta(q, "borrar un problema", "ces_01.tns", arch_borrar(0));
    tb_antes();
    arch_carpeta("/no/existe/tb");                         /* no se abre: la puerta sigue igual */
    n = arch_bytes_guardar("/ces_muestras.tns", b, 12);
    arch_carpeta(carpeta_mis);
    chk_cond(q, "si no se puede abrir: sube igual (la puerta, en pareja)", !n && tb_s->nesc - tb_e0 == 6 &&
             turbo_nivel() == 36 && tb_s->narch == tb_n0 + 1 && tb_s->arch[tb_n0].m == TURBO_M_SERIE);

    /* Controles, por la interfaz: [enter] lo guarda (con la marca de 432) */
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL);
    tb_antes();
    TECLAS(T_ENTER);
    tb_ruta(q, "Controles, [enter] en el panel", "ces_controles.tns", 1);
    chk_cond(q, "...con la marca de 432 detr" S_aa "s de los 8 bytes de siempre",
             arch_bytes_leer("/ces_controles.tns", b, 16) == 12 && !memcmp(b, "CESK", 4) && b[4] == 1 &&
             b[8] == 'T' && b[9] == 1 && b[10] == 36 && b[11] == 0);

    /* el grabador, de punta a punta: 10 s de muestras y su archivo */
    arch_bytes_borrar("/ces_muestras.tns");
    ui_init();
    TECLAS(T_ENTER);
    TECLAS(T_MENU | T_CON_CTRL);
    ce_abre(CE_W, CE_H, 1);
    ce_pulsa(TF_LETRA('G'));
    ce_pulsa(TF_ESC);                                       /* guarda Controles y sigue grabando */
    tb_antes();
    ce_pasa(11000);
    tb_ruta(q, "el grabador, a los 10 s", "ces_muestras.tns", ent_grabacion(0, 0) == GRAB_HECHO);
    ent_grabacion_olvida();
    arch_bytes_borrar("/ces_muestras.tns");

    /* al salir: a 396 y la marca fuera (Controles, escrito a 396), y la sesión */
    tb_antes();
    turbo_sale();
    n = arch_bytes_leer("/ces_controles.tns", b, 16);
    chk_cond(q, "al salir: 35, 34, 33; Controles sin la marca (8 bytes), escrito a 396",
             tb_s->nesc - tb_e0 == 3 && tb_s->pll == TURBO_SERIE && turbo_nivel() == 0 && n == 8 &&
             tb_s->narch == tb_n0 + 1 && tb_s->arch[tb_n0].m == TURBO_M_SERIE && tb_s->arch[tb_n0].nesc == tb_e0 + 3);
    tb_antes();
    ui_sesion_guardar();
    turbo_sale();                                           /* atexit, tras main: no hace nada */
    chk_cond(q, "la sesi" S_oo "n, despu" S_ee "s, a 396; turbo_sale otra vez (atexit) no toca nada",
             tb_s->nesc == tb_e0 && tb_s->narch >= tb_n0 + 1 && tb_s->arch[tb_s->narch - 1].m == TURBO_M_SERIE);
    tb_antes();
    bien = tb_malas == 0;
    chk_cond(q, "todas las escrituras, a 396", bien);
    arch_sesion_borrar();
    limpia_mis();

    /* turbo_sale baja por su cuenta, aunque no haya marca que borrar (la
     * puerta no lo haría por él) */
    tb_limpio();
    turbo_pon(36);
    turbo_recupera(0, 0);                                   /* sin marca, a propósito */
    tb_s->nesc = 0; tb_s->narch = 0;
    turbo_sale();
    chk_cond(q, "al salir sin marca que borrar: baja igual (35, 34, 33) y no escribe nada",
             !strcmp(tb_ms(), "35 34 33") && tb_s->pll == TURBO_SERIE && turbo_nivel() == 0 && tb_s->narch == 0);
}

/* ---------------------------------------------------------------------------
 *  T7. El SO cambia el registro
 * ------------------------------------------------------------------------ */
static void tb_so(void)
{
    const char *q = "[75] T7 el SO";
    unsigned char b[4] = { 1, 2, 3, 4 };
    int n0, r;

    tb_limpio();
    turbo_pon(36);
    n0 = tb_s->nesc;
    tb_s->pll = 0x18020303u;                                /* el USB: el SO pone 288 */
    ui_rotulo();                                            /* el bucle lo mira en cada vuelta */
    chk_cond(q, "el USB a 432: el bucle lo ve, se apaga sin escribir nada", turbo_nivel() == 0 && turbo_pedido() == 0 &&
             turbo_motivo() == TBM_SO && tb_s->nesc == n0);
    tb_s->narch = 0;
    arch_bytes_guardar("/ces_muestras.tns", b, 4);
    turbo_sale();
    chk_cond(q, "luego se escribe al reloj del SO (288) y al salir no se restaura encima",
             tb_s->nesc == n0 && tb_s->pll == 0x18020303u && tb_s->narch >= 1 && tb_s->arch[0].m == TURBO_M_USB);
    tb_panel();
    chk_cond(q, "el panel: \"El SO cambi" S_oo " el reloj\", 288 MHz y el turbo en \"no\"",
             ve("El SO cambi" S_oo " el reloj (" S_IQ "el USB?)") && ve("Reloj: 288 MHz (con el USB)") && ve(S_LARR " no " S_RARR));
    r4_vuelca("tb_so");
    TECLAS(T_ESC);
    arch_bytes_borrar("/ces_muestras.tns");

    tb_limpio();
    turbo_pon(36);
    n0 = tb_s->nesc;
    tb_s->pll = TURBO_SERIE;                                /* el SO puso 396 (el USB al quitarlo) */
    r = turbo_pausa();
    turbo_sigue();
    chk_cond(q, "otro valor al ir a escribir: se escribe y no se toca", r == 1 && tb_s->nesc == n0 && turbo_nivel() == 0 &&
             turbo_motivo() == TBM_SO);

    tb_limpio();
    tb_s->usb_en = 2;                                       /* al atender la 2.ª escritura */
    r = turbo_pon(36);
    chk_cond(q, "el USB mientras sube: lo del SO, sin escribir encima", r == TBM_SO && !strcmp(tb_ms(), "34 35") &&
             tb_s->pll == 0x18020303u && turbo_nivel() == 0 && turbo_pedido() == 0);

    /* el USB entre dos pasos: el SO lo atiende en la espera de antes de
     * escribir (las IRQ abiertas); luego, cerradas, el registro no es el que
     * dejó Hakaru y no se escribe encima */
    tb_limpio();
    tb_s->usb_tras = 5;                                     /* 1 + 3 lecturas del 1.er paso; la 5.ª, antes del 2.º */
    r = turbo_pon(36);
    chk_cond(q, "el USB en la espera de antes de escribir: no se escribe encima", r == TBM_SO &&
             !strcmp(tb_ms(), "34") && tb_s->pll == 0x18020303u && turbo_nivel() == 0);

    tb_limpio();
    turbo_pon(36);
    tb_s->nesc = 0;
    turbo_pausa();
    tb_s->pll = 0x18020303u;                                /* el USB mientras se escribe */
    turbo_sigue();
    chk_cond(q, "el USB mientras se escribe: no se vuelve a subir", !strcmp(tb_ms(), "35 34 33") && turbo_nivel() == 0 &&
             turbo_pedido() == 0 && turbo_motivo() == TBM_USB);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  T8. La marca de cuelgue y el formato de Controles
 * ------------------------------------------------------------------------ */
static void tb_marca(void)
{
    const char *q = "[75] T8 marca";
    unsigned char b[20];
    EntAjustes a;
    int r;

    tb_limpio();
    r = turbo_pon(35);
    chk_cond(q, "antes de la primera escritura de la PMU, a 396, la marca de 420", r == TBM_OK && tb_s->narch == 1 &&
             tb_s->arch[0].nesc == 0 && tb_s->arch[0].m == TURBO_M_SERIE && !strcmp(tb_s->arch[0].nombre, "ces_controles.tns"));
    chk_cond(q, "ces_controles.tns: los 8 de siempre y 'T', 1, 35, 0", arch_bytes_leer("/ces_controles.tns", b, 20) == 12 &&
             !memcmp(b, "CESK", 4) && b[4] == 1 && b[8] == 'T' && b[9] == 1 && b[10] == 35 && b[11] == 0);

    /* se cuelga (reset: el boot1 vuelve a 396 y el programa arranca de nuevo) */
    turbo_sim_reinicia();
    ui_init();
    ui_controles_abrir();
    chk_cond(q, "al arrancar tras el cuelgue: el panel lo dice, 420 queda fallido y la marca, borrada",
             ui_pantalla() == P_CONTROLES && ve("Con el turbo a 420 MHz la calculadora se colg" S_oo) && turbo_fallidos() == 2u &&
             turbo_marca() == 0 && arch_bytes_leer("/ces_controles.tns", b, 20) == 12 && b[10] == 0 && b[11] == 2);
    chk_cond(q, "...y arranca apagado (nunca se guarda encendido)", turbo_nivel() == 0 && turbo_pedido() == 0 &&
             tb_s->pll == TURBO_SERIE && ve(S_LARR " no " S_RARR));
    r4_vuelca("tb_colgo");
    TECLAS(T_ENTER);
    chk_cond(q, "[enter]: a la portada", ui_pantalla() == P_INICIO);

    tb_s->nesc = 0;
    tb_panel();
    TECLAS(T_DER, T_ENTER);
    chk_cond(q, "408 no fall" S_oo ": solo el aviso de activar", turbo_nivel() == 34);
    TECLAS(T_DER);
    chk_cond(q, "420 fall" S_oo ": su aviso, antes de nada", ve("A 420 MHz esta calculadora ya se colg" S_oo) && turbo_nivel() == 34);
    r4_vuelca("tb_aviso_fallido");
    TECLAS(T_ESC);
    chk_cond(q, "[esc]: se queda en 408", turbo_nivel() == 34 && !ve("ya se colg"));
    TECLAS(T_DER, T_ENTER);
    chk_cond(q, "[enter]: 420", turbo_nivel() == 35);
    TECLAS(T_IZQ, T_DER);
    chk_cond(q, "en esta sesi" S_oo "n ya no lo vuelve a pedir", turbo_nivel() == 35 && !ve("ya se colg"));
    TECLAS(T_ESC);
    turbo_sale();
    chk_cond(q, "al salir bien: la marca fuera, el fallido se queda", arch_bytes_leer("/ces_controles.tns", b, 20) == 12 &&
             b[10] == 0 && b[11] == 2);
    turbo_sim_reinicia();
    ui_init();
    ui_controles_abrir();
    chk_cond(q, "el arranque siguiente: sin aviso, con el fallido", ui_pantalla() == P_INICIO && turbo_fallidos() == 2u);

    /* sin la marca escrita, no se sube */
    tb_limpio();
    arch_carpeta("/no/existe/tb");
    r = turbo_pon(34);
    arch_carpeta(carpeta_mis);
    chk_cond(q, "si ces_controles.tns no se puede escribir, no se sube", r == TBM_MARCA && tb_s->nesc == 0 && turbo_nivel() == 0);

    /* el formato: los de antes, secciones de otra versión, rotas */
    tb_limpio();
    memcpy(b, "CESK", 4); b[4] = 1; b[5] = 0; b[6] = REPETIR_RAPIDA; b[7] = 0;
    arch_bytes_guardar("/ces_controles.tns", b, 8);
    ui_controles_abrir();
    ent_ajustes(&a);
    chk_cond(q, "un archivo de antes (8 bytes): los ajustes, y nada del turbo", a.deslizar == 0 && a.repetir == REPETIR_RAPIDA &&
             a.clic_enter == 0 && turbo_fallidos() == 0 && ui_pantalla() == P_INICIO);
    b[8] = 'T'; b[9] = 2; b[10] = 0; b[11] = 4; b[12] = 9; b[13] = 9; b[14] = 9; b[15] = 9;
    arch_bytes_guardar("/ces_controles.tns", b, 16);
    ui_controles_abrir();
    chk_cond(q, "una secci" S_oo "n de una versi" S_oo "n nueva, m" S_aa "s larga: se lee lo que se sabe", turbo_fallidos() == 4u);
    turbo_recupera(0, 0);
    b[8] = 'X';
    arch_bytes_guardar("/ces_controles.tns", b, 16);
    ui_controles_abrir();
    chk_cond(q, "otra marca de secci" S_oo "n: no es del turbo", turbo_fallidos() == 0);
    b[8] = 'T'; b[9] = 1; b[10] = 35;
    arch_bytes_guardar("/ces_controles.tns", b, 10);
    ui_controles_abrir();
    chk_cond(q, "una secci" S_oo "n cortada: nada", turbo_fallidos() == 0 && ui_pantalla() == P_INICIO);
    b[10] = 40; b[11] = 1;
    arch_bytes_guardar("/ces_controles.tns", b, 12);
    ui_controles_abrir();
    chk_cond(q, "una marca fuera de 34..38: no es un cuelgue", turbo_fallidos() == 1u && ui_pantalla() == P_INICIO);
    tb_limpio();
    cm_menu();
    TECLAS(T_MENU | T_CON_CTRL, T_ENTER);
    chk_cond(q, "sin nada del turbo, Controles guarda sus 8 bytes, como siempre",
             arch_bytes_leer("/ces_controles.tns", b, 20) == 8 && !memcmp(b, "CESK", 4) && b[4] == 1);
    ent_ajustes_omision();
}

/* ---------------------------------------------------------------------------
 *  T9. La prueba de estabilidad
 * ------------------------------------------------------------------------ */
static void tb_prueba(void)
{
    const char *q = "[75] T9 prueba";
    char det[200];
    unsigned char b[20];
    TurboPrueba p;
    long i;
    int r, dos = 0, n0;

    /* bien, por el panel */
    tb_limpio();
    tb_panel();
    TECLAS(T_LETRA('P'));
    chk_cond(q, "[P] apagado: \"Elige antes un nivel\"", ve("Elige antes un nivel con [izq/der].") && tb_s->nesc == 0);
    TECLAS(T_DER, T_ENTER);
    tb_s->nesc = 0; tb_s->narch = 0;
    TECLAS(T_LETRA('P'));
    turbo_prueba_estado(&p);
    chk_cond(q, "[P] a 408: la referencia, a 396", p.fase == TBP_REFERENCIA && ui_animando() == UI_TRABAJANDO &&
             !strcmp(tb_ms(), "33") && ve("Probando 408 MHz: la referencia a 396"));
    for (i = 0; i < 100000L && ui_animando() == UI_TRABAJANDO; i++) {
        ui_tick();
        turbo_prueba_estado(&p);
        if (!dos && p.fase == TBP_TURBO && p.pasadas == 2) { dos = 1; r4_vuelca("tb_probando"); }
    }
    turbo_prueba_estado(&p);
    sprintf(det, "acaba bien: %d pasadas iguales de %d vueltas en %d s, con %lu KB", p.pasadas, p.vueltas, p.seg, p.kb);
    chk_cond(q, det, p.fase == TBP_BIEN && p.motivo == TBM_OK && p.pasadas >= 10 && p.vueltas >= 4 && p.seg >= 45 &&
             p.kb == 2048 && dos);
    chk_cond(q, "...a 396 para la referencia (sin la marca de antes), la marca de 408 a 396 y luego arriba; sigue a 408",
             !strcmp(tb_ms(), "33 34") && tb_s->narch == 2 && tb_s->arch[0].nesc == 1 && tb_s->arch[0].m == TURBO_M_SERIE &&
             tb_s->arch[1].nesc == 1 && tb_s->arch[1].m == TURBO_M_SERIE &&
             turbo_nivel() == 34 && turbo_pedido() == 34 && ui_animando() != UI_TRABAJANDO);
    chk_cond(q, "el panel: \"Superada: N pasadas iguales a 408 MHz\"", ve("Superada: ") && ve(" pasadas iguales a 408 MHz."));
    r4_vuelca("tb_prueba_bien");
    TECLAS(T_ESC);

    /* en la 3.ª pasada cambia el bit 0 de la 1.ª solución de Newton (Newton
     * lo corrige: solo lo ve la FNV de cada solución): a 396, apagado, 408
     * fallido */
    tb_s->falla_pasada = 3;
    tb_panel();
    TECLAS(T_LETRA('P'));                                  /* sigue a 408 */
    for (i = 0; i < 100000L && ui_animando() == UI_TRABAJANDO; i++) ui_tick();
    turbo_prueba_estado(&p);
    chk_cond(q, "una pasada distinta (la 3a): a 396, apagado y 408 fallido", p.fase == TBP_MAL && p.motivo == TBM_PRUEBA &&
             p.pasadas == 2 && turbo_nivel() == 0 && turbo_pedido() == 0 && tb_s->pll == TURBO_SERIE && turbo_fallidos() == 1u);
    chk_cond(q, "...en ces_controles.tns: sin marca y con el fallido", arch_bytes_leer("/ces_controles.tns", b, 20) == 12 &&
             b[10] == 0 && b[11] == 1);
    chk_cond(q, "el panel lo dice", ve("La prueba dio distinto a 408 MHz: vuelta a 396 y") && ve(S_LARR " no " S_RARR));
    r4_vuelca("tb_prueba_mal");
    TECLAS(T_ESC);
    tb_s->falla_pasada = 0;

    /* un bit de la memoria cambia a 408 */
    tb_limpio();
    turbo_pon(34);
    tb_s->falla_mem = 1;
    r = turbo_prueba(34);
    tb_prueba_entera();
    turbo_prueba_estado(&p);
    chk_cond(q, "un bit del patr" S_oo "n que cambia: la 1a pasada ya es distinta", r == TBM_OK && p.fase == TBP_MAL &&
             p.motivo == TBM_PRUEBA && p.pasadas == 0 && tb_s->pll == TURBO_SERIE && turbo_nivel() == 0);

    /* [esc] a mitad */
    tb_limpio();
    tb_panel();
    TECLAS(T_DER, T_ENTER, T_LETRA('P'));
    for (i = 0; i < 300; i++) ui_tick();
    turbo_prueba_estado(&p);
    n0 = p.fase == TBP_TURBO;
    TECLAS(T_ESC);
    turbo_prueba_estado(&p);
    chk_cond(q, "[esc] a 408: cancelada, a 396 y apagado (sin fallido); sigue en el panel",
             n0 && p.fase == TBP_MAL && p.motivo == TBM_CANCELADA && tb_s->pll == TURBO_SERIE && turbo_nivel() == 0 &&
             turbo_fallidos() == 0 && ui_pantalla() == P_CONTROLES && ve("Prueba cancelada: vuelta a 396 MHz y apagado."));
    TECLAS(T_ESC);

    /* el contador de 32 kHz que fuera con la CPU (con el APB) */
    tb_limpio();
    turbo_pon(34);
    tb_s->t32_cpu = 1;
    turbo_prueba(34);
    tb_prueba_entera();
    turbo_prueba_estado(&p);
    chk_cond(q, "si el contador de 32 kHz subiera con la CPU (34/33: un 3 %): no, y a 396", p.fase == TBP_MAL &&
             p.motivo == TBM_RELOJ32 && tb_s->pll == TURBO_SERIE && turbo_nivel() == 0);

    /* sin memoria */
    tb_limpio();
    tb_s->sin_mem = 1;
    r = turbo_prueba(34);
    chk_cond(q, "sin memoria para el patr" S_oo "n: no empieza", r == TBM_MEMORIA && tb_s->nesc == 0 && turbo_nivel() == 0);

    /* el USB a mitad de la prueba */
    tb_limpio();
    turbo_pon(34);
    turbo_prueba(34);
    for (i = 0; i < 100000L; i++) {
        turbo_prueba_paso();
        turbo_prueba_estado(&p);
        if (p.fase == TBP_TURBO && p.pasadas == 1) break;
    }
    n0 = tb_s->nesc;
    tb_s->pll = 0x18020303u;
    ui_rotulo();
    tb_prueba_entera();
    turbo_prueba_estado(&p);
    chk_cond(q, "el USB a mitad: se para, sin escribir la PMU", p.fase == TBP_MAL && p.motivo == TBM_SO &&
             tb_s->nesc == n0 && tb_s->pll == 0x18020303u && turbo_nivel() == 0);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  T10. El reloj
 * ------------------------------------------------------------------------ */
static void tb_reloj(void)
{
    static const struct { int cx2; unsigned pll, div, fuente; int mhz; } F[] = {
        { 1, 0x21020303u, 0, 0x11u, 396 }, { 1, 0x22020303u, 0, 0x11u, 408 }, { 1, 0x24020303u, 0, 0x11u, 432 },
        { 1, 0x26020303u, 0, 0x11u, 456 }, { 1, 0x18020303u, 0, 0x11u, 288 }, { 1, 0x21020303u, 0x00100000u, 0x11u, 198 },
        { 1, 0x21020303u, 0, 0x01u, 48 }, { 1, 0x21020313u, 0, 0x11u, 24 }, { 1, 0x21010303u, 0, 0x11u, 792 },
        { 1, 0x21000303u, 0, 0x11u, 0 }, { 0, 0x21020303u, 0, 0x11u, 0 },
    };
    const char *q = "[75] T10 reloj";
    char det[120];
    int i, bien = 1;

    for (i = 0; i < (int)(sizeof F / sizeof F[0]); i++) {
        tb_limpio();
        tb_s->cx2 = F[i].cx2; tb_s->pll = F[i].pll; tb_s->div = F[i].div; tb_s->fuente = F[i].fuente;
        if (turbo_mhz() != F[i].mhz) {
            bien = 0;
            sprintf(det, "0x%08X, 0x%08X, 0x%X: %d MHz y no %d", F[i].pll, F[i].div, F[i].fuente, turbo_mhz(), F[i].mhz);
            chk_cond(q, det, 0);
        }
    }
    chk_cond(q, "la f" S_oo "rmula de Linux: 24" S_CDOT "M/D1/(1+D2); 48 sin el PLL (0x810), 24 con el bit 4; 0 sin CX II", bien);

    /* el banco dice a qué frecuencia midió (la 1.ª prueba, el flop: sin
     * emular nada) y si el reloj cambió a mitad (la 2.ª, un ejemplo) */
    tb_limpio();
    turbo_pon(36);
    cm_ayuda();
    TECLAS(T_LETRA('T'));
    ui_tick();
    chk_cond(q, "el banco a 432: \"ciclos a 432 MHz\"", ui_pantalla() == P_BANCO && ve(" ciclos a 432 MHz"));
    turbo_pon(0);
    ui_tick();
    chk_cond(q, "...y si cambia a mitad (a 396): \"el reloj cambi" S_oo " a mitad\"", ve("Flop: el reloj cambi" S_oo " a mitad") &&
             !ve("ciclos a"));
    TECLAS(T_ESC);
    turbo_sale();
}

/* ---------------------------------------------------------------------------
 *  T11. La barra: las marcas y un título largo
 * ------------------------------------------------------------------------ */
static void tb_barra(void)
{
    const char *q = "[75] T11 barra";
    const char *largo = "Ejemplos: Fuentes reales, J e instrumentos";     /* 42 columnas */
    static Circuito c;
    int w1, w2, izq;

    /* la lección 8: su título entero, sin marcas y con la del turbo */
    tb_limpio();
    cm_lecciones();
    repite(T_ABAJO, 7);
    TECLAS(T_ENTER, T_ENTER);
    chk_cond(q, "los ejemplos de la lecci" S_oo "n 8: el t" S_ii "tulo entero", ui_pantalla() == P_EJEMPLOS && ve(largo));
    turbo_pon(36);
    chk_cond(q, "con la marca del turbo (\"432 MHz\", 48 px) todav" S_ii "a cabe", ve(largo) && ve("432 MHz"));
    turbo_pon(0);
    ent_grabar();                                           /* «grabando 10 s», sin el panel */
    chk_cond(q, "con \"grabando 10 s\" (84 px) se corta antes de la marca (antes, la marca tapaba el final)",
             ve("grabando 10 s") && !ve(largo) && ve("Ejemplos: Fuentes reales, J e..."));
    turbo_pon(36);
    chk_cond(q, "con las dos, m" S_aa "s corto", ve("grabando 10 s") && ve("432 MHz") && ve("Ejemplos: Fuentes reales..."));
    r4_vuelca("tb_barra_titulo");
    ent_grabacion_olvida();
    turbo_pon(0);

    /* el editor: lo de la derecha va a la izquierda de las marcas */
    ce_escalera(&c, 0);
    strcpy(c.nombre, "Escalera de nueve peldanos con fuente E");      /* 39 */
    r3u_abre(&c);
    turbo_pon(36);
    TECLAS(T_CTRL);
    w1 = gfx_text_w("432 MHz", 1) + 6;
    w2 = gfx_text_w("ctrl", 1) + 6;
    izq = SCR_W - 2 - w1 - 2 - w2;
    chk_cond(q, "editor con 432 y [ctrl]: las dos marcas y \"[menu] opciones\" entero", ve("432 MHz") && ve("ctrl") &&
             ve("[menu] opciones") && ve("..."));
    ui_dibuja();
    chk_cond(q, "...y nada de texto en los 4 px antes de las marcas", gfx_ocupados(izq - 4, 1, 4, 14, C_BARRA) == 0);
    r4_vuelca("tb_barra_editor");
    TECLAS(T_CTRL);
    turbo_sale();
    chk_cond(q, "apagado, la marca se va", !ve("432 MHz"));
    limpia_mis();

    /* la marca, también donde no hay barra: la portada */
    tb_limpio();
    turbo_pon(34);
    chk_cond(q, "en la portada, la marca", ui_pantalla() == P_INICIO && ve("408 MHz"));
    r4_vuelca("tb_marca_portada");
    ESP_TB_MARCA
    turbo_sale();
}

/* ----------------------------------------------------------------------- */
static void test_opt_turbo(void)
{
    printf("[75] El turbo (experimental), con el simulador de la PMU\n");
    gfx_desbordes_reset();
    tb_precondiciones();
    tb_limites();
    tb_secuencia();
    tb_pendiente();
    tb_lectura();
    tb_escrituras();
    tb_so();
    tb_marca();
    tb_prueba();
    tb_reloj();
    tb_barra();
    chk_cond("[75] turbo", "los textos del turbo caben (52 columnas: ninguno se sale)", gfx_desbordes() == 0);
    /* como estaba: «no es una CX II», sin ces_controles.tns */
    turbo_sim_reinicia();
    turbo_sim()->cx2 = 0;
    arch_bytes_borrar("/ces_controles.tns");
    ent_grabacion_olvida();
    ent_ajustes_omision();
    ui_init();
}
