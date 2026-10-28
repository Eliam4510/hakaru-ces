/* ============================================================================
 *  test_capmotor.inc  -  [76] Capacitores equivalentes (rev10; la v3 en
 *  rev12: capacitores/ESPEC_CAPEQ.md, sobre ESPEC_CAPMOTOR.md)
 *
 *    a  lo que se escribe: el valor, la tolerancia y la bodega (rangos, «sin»,
 *       REPETIDOS que cuentan, decimales, errores, mucho texto) y las piezas
 *       de la edición con cursor (las líneas del campo, saltar de valor)
 *    b  los cuatro casos del PDF (§4), con el texto de su paso a paso (con
 *       palabras: «12 + ¿5? -> no hay 5»), y los de los repetidos de la v2
 *    c  los bordes: bodega vacía, un valor, el pedido ya en la bodega, el
 *       margen de ±0.05, decimales, el modo «Capacitores» (2 y 3)
 *    d  al azar, contra una fuerza bruta escrita aquí aparte (con repetidos,
 *       los tres modos y los arreglos de tres): las que valen, sus cuentas,
 *       el orden, lo que se enseña, lo más cercano, «te faltaría» y que el
 *       texto cabe en 52 columnas
 *    e  el volcado del oráculo v3 (HAKARU_CAPMOTOR_VOLCADO, §6)
 *    f  las pantallas: el Taller, el menú propio, los datos, el resultado en
 *       pestañas, el dibujo de cada tipo (los de tres también), los errores,
 *       lo que se recuerda y la Ayuda, con sus BMP en out/cm_*.bmp (fuera de
 *       la numeración de vuelca())
 *    g  el dibujo: ningún rótulo pisa nada y las redes de cable conectan lo
 *       que deben (por los píxeles), también los de tres
 *    h  la independencia del dibujo (ce_indep) y el mono oculto (ui.h)
 *    i  los hallazgos de la revisión de rev10 (el cierre), al día
 *    j  la edición con cursor (§3.1): corregir el primer valor de una lista
 *       larga sin borrarla, insertar, borrar, saltar de valor, las líneas
 *    k  las pestañas y las opciones (§3.2)
 *    l  la Teoría y los Ejemplos: cada ejemplo carga y resuelve lo que dice
 *    m  el cierre de la v3: los hallazgos de las dos revisiones (la cuenta
 *       del mixto 2, «te faltaría» con 2 y uno solo que vale, las filas y el
 *       veredicto que caben, el cableado con repetidos, el resumen con
 *       tolerancia, el porqué con mucha tolerancia, las diferencias que se
 *       distinguen, el cursor de la bodega, el campo vacío, los ejemplos que
 *       no se comen lo del usuario y los rótulos largos de la serie de tres)
 *
 *  Va al FINAL de la batería (no corre la numeración de las BMP ni las
 *  huellas) y solo con HAKARU_SOLO_CAPMOTOR=1. Usa ve, TECLAS, repite, ce_indep
 *  (test_entrada.inc), azar y dir_out.
 * ========================================================================= */
#include "capeq_texto.h"

static CpmRes cp_r;
static char cp_txt[32768];

static const CpmRes *cp_caso_n(double C, double tol, int modo, const char *bod)
{
    static double v[CPM_MAX];
    int n = 0;
    cpm_lee_bodega(bod, v, &n);
    cpm_resuelve_n(&cp_r, C, tol, modo, v, n);
    cpm_proced(&cp_r, 0, cp_txt, sizeof cp_txt);
    return &cp_r;
}
static const CpmRes *cp_caso(double C, double tol, const char *bod) { return cp_caso_n(C, tol, CPM_AUTO, bod); }

static int cp_tiene(const char *s) { return strstr(cp_txt, s) != 0; }

/* ¿La lista tiene esta combinación (valores, no índices)? Su posición, o -1 */
static int cp_esta3(const CpmRes *r, int tipo, double a, double b, double c)
{
    int i;
    for (i = 0; i < r->nval; i++) {
        const CpmComb *k = &r->val[i];
        int n = cpm_ncap(k->tipo);
        if (k->tipo == tipo && r->v[k->a] == a && (n < 2 || r->v[k->b] == b) && (n < 3 || r->v[k->c] == c)) return i;
    }
    return -1;
}
static int cp_esta(const CpmRes *r, int tipo, double a, double b) { return cp_esta3(r, tipo, a, b, 0.0); }

static void cp_vuelca(const char *nom)
{
    char ruta[256];
    ui_dibuja();
    sprintf(ruta, "%s/cm_%s.bmp", dir_out, nom);
    gfx_dump_bmp(ruta);
}

/* ---- a. Lo que se escribe ---- */
static void cp_lectura(void)
{
    const char *q = "capacitores: lo que se escribe";
    static double v[CPM_MAX];
    static char largo[CPM_TXT + 1];
    double x = 0.0;
    int n = -1, i;

    chk_cond(q, "el valor: 17", cpm_lee_valor("17", &x) == CPM_OK && x == 17.0);
    chk_cond(q, "el valor con punto y con coma: 22.5 y 22,5", cpm_lee_valor("22.5", &x) == CPM_OK && x == 22.5 &&
             cpm_lee_valor(" 22,5 ", &x) == CPM_OK && x == 22.5);
    chk_cond(q, "el valor: vacío, 0, letras o dos puntos no valen",
             cpm_lee_valor("", &x) == CPM_E_VALOR && cpm_lee_valor("0", &x) == CPM_E_VALOR &&
             cpm_lee_valor("1a", &x) == CPM_E_VALOR && cpm_lee_valor("1.2.3", &x) == CPM_E_VALOR);
    chk_cond(q, "el valor: más de 100000 o más de 15 cifras, grande",
             cpm_lee_valor("200000", &x) == CPM_E_GRANDE && cpm_lee_valor("1234567890123456", &x) == CPM_E_GRANDE);
    chk_cond(q, "el decimal es el mismo double que strtod (0.1, 46.7, 12.345678)",
             cpm_lee_valor("0.1", &x) == CPM_OK && x == strtod("0.1", 0) && cpm_lee_valor("46.7", &x) == CPM_OK &&
             x == strtod("46.7", 0) && cpm_lee_valor("12.345678", &x) == CPM_OK && x == strtod("12.345678", 0));
    chk_cond(q, "la tolerancia: vacía es 0 (exacto); 0.5; letras no",
             cpm_lee_tol("  ", &x) == CPM_OK && x == 0.0 && cpm_lee_tol("0.5", &x) == CPM_OK && x == 0.5 &&
             cpm_lee_tol("x", &x) == CPM_E_TOL);

    chk_cond(q, "la bodega A del pizarrón: 14 capacitores, de 1 a 60",
             cpm_lee_bodega(CPM_BODEGA_A, v, &n) == CPM_OK && n == 14 && v[0] == 1.0 && v[3] == 7.0 && v[13] == 60.0);
    chk_cond(q, "la B: del 15 al 42, los dos incluidos (28)",
             cpm_lee_bodega(CPM_BODEGA_B, v, &n) == CPM_OK && n == 28 && v[0] == 15.0 && v[27] == 42.0);
    chk_cond(q, "15-42 sin 21-24: 24 capacitores, sin el 21 ni el 24",
             cpm_lee_bodega("15-42 sin 21-24", v, &n) == CPM_OK && n == 24 && v[5] == 20.0 && v[6] == 25.0);
    chk_cond(q, "15-42 -21-24: lo mismo", cpm_lee_bodega("15-42 -21-24", v, &n) == CPM_OK && n == 24 && v[6] == 25.0);
    chk_cond(q, "lo que se quita da igual dónde se escriba: -21-24 15-42",
             cpm_lee_bodega("-21-24 15-42", v, &n) == CPM_OK && n == 24 && v[6] == 25.0);
    chk_cond(q, "SIN en mayúsculas y un valor suelto quitado (-7)",
             cpm_lee_bodega("1-10 SIN 3 -7", v, &n) == CPM_OK && n == 8 && v[2] == 4.0 && v[5] == 8.0);
    chk_cond(q, "decimales sueltos: 2.5 7.5", cpm_lee_bodega("7.5 2.5", v, &n) == CPM_OK && n == 2 && v[0] == 2.5 && v[1] == 7.5);
    chk_cond(q, "la coma separa: 2,5 son el 2 y el 5", cpm_lee_bodega("2,5", v, &n) == CPM_OK && n == 2 && v[0] == 2.0 && v[1] == 5.0);
    /* los repetidos cuentan (la v2): cada aparición es un capacitor */
    chk_cond(q, "repetidos: 7 7 3 7 3 10-12 11 son nueve capacitores (3×2, 7×3, 11×2), ordenados",
             cpm_lee_bodega("7 7 3 7 3 10-12 11", v, &n) == CPM_OK && n == 9 && v[0] == 3.0 && v[1] == 3.0 &&
             v[2] == 7.0 && v[4] == 7.0 && v[5] == 10.0 && v[6] == 11.0 && v[7] == 11.0 && v[8] == 12.0);
    chk_cond(q, "repetidos: 1 2 3 4 5 6 7 10 12 7 son diez (dos de 7)",
             cpm_lee_bodega("1 2 3 4 5 6 7 10 12 7", v, &n) == CPM_OK && n == 10 && v[6] == 7.0 && v[7] == 7.0);
    chk_cond(q, "un rango pone uno de cada; un valor suelto que ya está en el rango es otro (15-42 20: 29, dos de 20)",
             cpm_lee_bodega("15-42 20", v, &n) == CPM_OK && n == 29 && v[5] == 20.0 && v[6] == 20.0 && v[7] == 21.0);
    chk_cond(q, "«sin X» quita TODOS los de ese valor (7 7 7 2 sin 7: solo el 2)",
             cpm_lee_bodega("7 7 7 2 sin 7", v, &n) == CPM_OK && n == 1 && v[0] == 2.0);
    chk_cond(q, "15-42 dos veces son 56 capacitores", cpm_lee_bodega("15-42 15-42", v, &n) == CPM_OK && n == 56);
    chk_cond(q, "un rango al revés vale: 42-40", cpm_lee_bodega("42-40", v, &n) == CPM_OK && n == 3 && v[0] == 40.0);
    chk_cond(q, "vacía (o solo espacios, o solo «sin»): 0, sin error",
             cpm_lee_bodega("", v, &n) == CPM_OK && n == 0 && cpm_lee_bodega("   sin ", v, &n) == CPM_OK && n == 0);
    chk_cond(q, "el 0 no vale (ni un rango desde 0)", cpm_lee_bodega("3 0 5", v, &n) == CPM_E_CERO &&
             cpm_lee_bodega("0-5", v, &n) == CPM_E_CERO);
    chk_cond(q, "un rango con decimales no vale", cpm_lee_bodega("1.5-3", v, &n) == CPM_E_RANGO);
    chk_cond(q, "basura: 12x, 3-4-5, 15-, un - suelto, letras",
             cpm_lee_bodega("12x", v, &n) == CPM_E_NUMERO && cpm_lee_bodega("3-4-5", v, &n) == CPM_E_NUMERO &&
             cpm_lee_bodega("15-", v, &n) == CPM_E_NUMERO && cpm_lee_bodega("3 - 4", v, &n) == CPM_E_NUMERO &&
             cpm_lee_bodega("si 3", v, &n) == CPM_E_NUMERO && n == 0);
    chk_cond(q, "100 capacitores sí; 101 no (se dice)", cpm_lee_bodega("1-100", v, &n) == CPM_OK && n == 100 &&
             cpm_lee_bodega("1-101", v, &n) == CPM_E_MUCHOS && cpm_lee_bodega("1-100 200", v, &n) == CPM_E_MUCHOS);
    chk_cond(q, "...contando repetidos: 1-60 1-41 son 101, demasiados; 1-50 1-50 son 100",
             cpm_lee_bodega("1-60 1-41", v, &n) == CPM_E_MUCHOS && cpm_lee_bodega("1-50 1-50", v, &n) == CPM_OK && n == 100);
    chk_cond(q, "...también si el repetido es el último: 1-99 100 100 son 101 (y 1-98 100 100, 100)",
             cpm_lee_bodega("1-99 100 100", v, &n) == CPM_E_MUCHOS && cpm_lee_bodega("1-98 100 100", v, &n) == CPM_OK && n == 100 &&
             v[98] == 100.0 && v[99] == 100.0);
    chk_cond(q, "se cuenta tras quitar: 1-150 sin 101-150 son 100",
             cpm_lee_bodega("1-150 sin 101-150", v, &n) == CPM_OK && n == 100 && v[99] == 100.0);
    chk_cond(q, "un rango enorme no da vueltas: 1-100000", cpm_lee_bodega("1-100000", v, &n) == CPM_E_MUCHOS);
    chk_cond(q, "un número de más de 100000 no", cpm_lee_bodega("5 100001", v, &n) == CPM_E_GRANDE);
    /* mucho texto: el campo lleno (300) de valores repetidos: 75 capacitores de 9 valores */
    for (i = 0; i < CPM_TXT; i++) largo[i] = (char)(i % 4 == 3 ? ' ' : '1' + (i / 4) % 9);
    largo[CPM_TXT] = 0;
    chk_cond(q, "mucho texto: 300 caracteres, 75 capacitores (repetidos)",
             cpm_lee_bodega(largo, v, &n) == CPM_OK && n == 75 && v[0] == 111.0 && v[74] == 999.0);
    {   /* el corte: por palabras, '\n' corta y la continuación lleva sangría */
        static const char T[] = "1. Uno dos tres cuatro\n   cinco seis siete ocho nueve\n\nfin";
        CpmLinea l[8];
        int nl = cpm_envuelve(T, 16, l, 8);
        chk_cond(q, "el corte en líneas: 7, con la sangría de su línea + 2 (3 sin ella) y la vacía",
                 nl == 7 && l[0].len == 15 && l[0].sangria == 0 && l[1].sangria == 3 && !strncmp(T + l[1].ini, "cuatro", 6) &&
                 l[2].sangria == 0 && l[2].len == 13 && l[3].sangria == 5 && l[4].sangria == 5 && l[5].len == 0 &&
                 !strncmp(T + l[6].ini, "fin", 3));
    }
    {   /* las piezas del cursor: las líneas del campo y saltar de valor */
        static const char S[] = "22 23 24 21 24 22 29 43 24";
        int ini[16], nl, col = -1;
        nl = cpm_lineas_campo("aaa bbb ccc ddd", 7, ini, 16);
        chk_cond(q, "el campo en líneas de 7: «aaa bbb » y «ccc ddd» (caben siete; el espacio del corte, al final)",
                 nl == 2 && ini[0] == 0 && ini[1] == 8);
        nl = cpm_lineas_campo("aaa bbb ccc ddd", 6, ini, 16);
        chk_cond(q, "...y en líneas de 6: «aaa » «bbb » «ccc » «ddd»",
                 nl == 4 && ini[0] == 0 && ini[1] == 4 && ini[2] == 8 && ini[3] == 12);
        chk_cond(q, "...el cursor 5 está en la línea 1, columna 1; el 15 (el final), en la 3", cpm_linea_de(ini, nl, 15, 5, &col) == 1 &&
                 col == 1 && cpm_linea_de(ini, nl, 15, 15, &col) == 3 && col == 3);
        nl = cpm_lineas_campo("abcdefghij", 4, ini, 16);
        chk_cond(q, "una palabra más larga que la línea se parte: abcd efgh ij", nl == 3 && ini[1] == 4 && ini[2] == 8);
        nl = cpm_lineas_campo("", 50, ini, 16);
        chk_cond(q, "vacío: una línea", nl == 1 && ini[0] == 0 && cpm_linea_de(ini, nl, 0, 0, &col) == 0 && col == 0);
        nl = cpm_lineas_campo("aaaaaaa b", 7, ini, 16);
        chk_cond(q, "el espacio justo en la columna 7: «aaaaaaa » y «b»", nl == 2 && ini[1] == 8);
        chk_cond(q, "saltar de valor: de 0 al 23 (3), al 24 (6); desde el final, atrás al 24 (24)",
                 cpm_salta_valor(S, 0, 1) == 3 && cpm_salta_valor(S, 3, 1) == 6 && cpm_salta_valor(S, 26, -1) == 24);
        chk_cond(q, "...desde el medio de un valor, atrás a su principio (4 -> 3) y adelante al siguiente (4 -> 6)",
                 cpm_salta_valor(S, 4, -1) == 3 && cpm_salta_valor(S, 4, 1) == 6);
        chk_cond(q, "...en los extremos se queda (0 atrás, el final adelante)", cpm_salta_valor(S, 0, -1) == 0 &&
                 cpm_salta_valor(S, 26, 1) == 26 && cpm_salta_valor(S, 24, 1) == 26);
    }
    for (i = 0; i < CPM_E_N; i++) {
        char det[80];
        sprintf(det, "el aviso del error %d cabe en 52 columnas", i);
        chk_cond(q, det, (int)strlen(cpm_error(i)) <= 52);
    }
}

/* ---- b. Los casos del PDF, y los repetidos de la v2 ---- */
static void cp_pdf(void)
{
    const char *q = "capacitores: los casos del PDF";
    const CpmRes *r;
    static CpmFalta f[CPM_MAX];
    double lo, hi;
    char t[300];
    int n, i, k38 = -1, k42 = -1, k20 = -1;

    /* Caso 1: 17 MF con la bodega A: 10 || 7, con el intento del 12 */
    r = cp_caso(17.0, 0.0, CPM_BODEGA_A);
    chk_cond(q, "17 A: una sola de dos, 10 " S_PAR " 7 = 17, la primera (no hay de uno)", r->tot[1] == 0 && r->tot[2] == 1 &&
             cp_esta(r, CPM_PAR, 7.0, 10.0) == 0 && r->val[0].ct == 17.0 && r->nver[2] == 1);
    chk_cond(q, "17 A: luego las de tres (Auto: las de menos capacitores primero)", r->nver[3] > 0 && cpm_ncap(r->val[1].tipo) == 3);
    cpm_veredicto(r, t, sizeof t);
    chk_cond(q, "17 A: el veredicto, «Con 2: 10 " S_PAR " 7 = 17 MF " S_CHECK "»", !strcmp(t, "Con 2: 10 " S_PAR " 7 = 17 MF " S_CHECK));
    chk_cond(q, "17 A: no hay de 17", cp_tiene("1. " S_IQ "Uno solo? No hay de 17.\n"));
    chk_cond(q, "17 A: el porqué (hay menores y mayores)",
             cp_tiene("Hay valores menores y mayores que 17: pueden servir paralelo y serie.\n"));
    chk_cond(q, "17 A: el paralelo, con palabras",
             cp_tiene("2. En paralelo los valores SE SUMAN: busco dos que sumen 17. Pruebo con el m" S_aa
                      "s grande que cabe y miro si existe el que falta:\n"));
    chk_cond(q, "17 A: 12 + ¿5? -> no hay 5", cp_tiene("     12 + " S_IQ "5? " S_RARR " no hay 5\n"));
    chk_cond(q, "17 A: 10 + ¿7? -> sí hay 7, con la cuenta del profe (C2 = 17 - 10 = 7)",
             cp_tiene("     10 + " S_IQ "7? " S_RARR " s" S_ii " hay 7 " S_CHECK " (C" S_SUB2 " = 17 - 10 = 7)\n"));
    chk_cond(q, "17 A: se para en la elegida (no prueba el 9) ni sigue con la serie", !cp_tiene("     9 +") && !cp_tiene("serie el total"));
    chk_cond(q, "17 A: la comprobación", cp_tiene("3. Comprobaci" S_oo "n: CT = 10 + 7 = 17 MF " S_CHECK "\n"));
    chk_cond(q, "17 A: el cableado", cp_tiene("   L" S_ii "nea 1 " S_RARR " C (la punteada).\n") &&
             cp_tiene("   Izquierdas del 7 y del 10 unidas " S_RARR " A.\n") &&
             cp_tiene("   Derechas unidas " S_RARR " l" S_ii "nea 2 y puente a T.\n"));
    cpm_proced(r, &r->val[4], cp_txt, sizeof cp_txt);
    chk_cond(q, "17 A, la opción 5 (de tres) elegida: «con uno o dos ya se puede (la opción 1)» y el mixto 2, con su cuenta",
             r->val[4].tipo == CPM_MIX2 && cp_tiene("2. Con uno o dos ya se puede (la opci" S_oo "n 1); esta es con tres.\n") &&
             cp_tiene("3. Mixto: primero la serie a" S_CDOT "b/(a + b) y luego en paralelo con c:") &&
             cp_tiene("   12 y 60 en serie: 12" S_CDOT "60/(12 + 60) = 720/72 = 10 MF\n") && !cp_tiene("En paralelo los valores"));
    r = cp_caso(14.0, 0.0, CPM_BODEGA_B);
    cpm_proced(r, &r->val[4], cp_txt, sizeof cp_txt);
    chk_cond(q, "14 B, la opción 5 (de tres): «(las opciones 1 a 4)»", cp_tiene("Con uno o dos ya se puede (las opciones 1 a 4); esta es con tres.\n"));

    /* Caso 2: 14 MF con la B: 21 y 42 en serie, con los intentos del 15 al 20 */
    r = cp_caso(14.0, 0.0, CPM_BODEGA_B);
    chk_cond(q, "14 B: la primera, 21 y 42 en serie = 14", r->tot[2] == 4 && cp_esta(r, CPM_SER, 21.0, 42.0) == 0 && r->val[0].ct == 14.0);
    chk_cond(q, "14 B: el porqué: todos son mayores, solo la serie",
             cp_tiene("Todos son mayores que 14: el paralelo no puede (sumar" S_ii "a m" S_aa "s); solo la serie.\n"));
    chk_cond(q, "14 B: todos son mayores: no hay paralelo", cp_tiene("2. Paralelo: no hay valores menores que 14.\n"));
    chk_cond(q, "14 B: la serie, con palabras y con la fórmula, desde el 15",
             cp_tiene("3. En serie el total queda MENOR que el m" S_aa "s chico: busco dos mayores que 14. Para cada C" S_SUB1
                      " el que falta es C" S_SUB2 " = 14" S_CDOT "C" S_SUB1 "/(C" S_SUB1 " - 14); subo desde el 15:\n"));
    chk_cond(q, "14 B: la regla de los iguales (solo hay uno de 28)",
             cp_tiene("     Dos iguales dan la mitad: dos de 28 dar" S_ii "an 14, pero solo hay uno.\n"));
    chk_cond(q, "14 B: del 15 al 19, resumidos sin perder el primero ni el último",
             cp_tiene("     con 15 a 19 falta de 210 a 53.2: no hay\n"));
    chk_cond(q, "14 B: con 20 falta 46.7, no hay", cp_tiene("     con 20 falta 46.7 " S_RARR " no hay\n"));
    chk_cond(q, "14 B: con 21 falta 294/7 = 42, sí hay",
             cp_tiene("     con 21 falta 294/7 = 42 " S_RARR " s" S_ii " hay 42 " S_CHECK "\n"));
    chk_cond(q, "14 B: se para en la elegida (no prueba el 22)", !cp_tiene("con 22"));
    chk_cond(q, "14 B: la comprobación con 882/63",
             cp_tiene("4. Comprobaci" S_oo "n:\n   CT = 21" S_CDOT "42/(21 + 42) = 882/63 = 14 MF " S_CHECK "\n"));
    chk_cond(q, "14 B: el cableado de la serie", cp_tiene("   A " S_RARR " izquierda del 21.\n") &&
             cp_tiene("   Derecha del 21 " S_RARR " izquierda del 42 (punto medio, sin otra conexi" S_oo "n).\n") &&
             cp_tiene("   Derecha del 42 " S_RARR " l" S_ii "nea 2 y T.\n"));

    /* Ejercicio: 12 MF con la B: 20 y 30; también 18 y 36, 21 y 28 */
    r = cp_caso(12.0, 0.0, CPM_BODEGA_B);
    chk_cond(q, "12 B: 18 y 36, 20 y 30, 21 y 28 son las tres primeras, exactas",
             cp_esta(r, CPM_SER, 18.0, 36.0) == 0 && cp_esta(r, CPM_SER, 20.0, 30.0) == 1 && cp_esta(r, CPM_SER, 21.0, 28.0) == 2 &&
             r->val[0].ct == 12.0 && r->val[1].ct == 12.0 && r->val[2].ct == 12.0);
    chk_cond(q, "12 B: 600/50 = 12 (20 y 30)", 20.0 * 30.0 / 50.0 == r->val[1].ct);
    chk_cond(q, "12 B: el 17 con el 41 sirve (12.02, dentro del ±0.05) y se dice al pasar",
             cp_esta(r, CPM_SER, 17.0, 41.0) > 2 && cp_tiene("     con 17 falta 40.8 " S_RARR " hay 41: sirve (CT 12.02)\n"));
    cpm_proced(r, &r->val[1], cp_txt, sizeof cp_txt);
    chk_cond(q, "12 B: el paso a paso de la opción 2 (la del PDF) llega al 20 y dice del 18 que sirve",
             cp_tiene("     con 18 falta 216/6 = 36 " S_RARR " hay 36: sirve (CT 12)\n") &&
             cp_tiene("     con 20 falta 240/8 = 30 " S_RARR " s" S_ii " hay 30 " S_CHECK "\n") && !cp_tiene("con 21"));

    /* Caso 3: 22.5 ± 0.5 con la B sin 21-24: con dos no; con tres sí */
    r = cp_caso(22.5, 0.5, "15-42 sin 21-24");
    chk_cond(q, "22.5: ninguna de uno o dos; de tres, muchas", r->tot[1] == 0 && r->tot[2] == 0 && r->tot[3] > 100 &&
             r->ntot == r->tot[3] && r->nver[3] == CPM_VER3);
    cpm_veredicto(r, t, sizeof t);
    chk_cond(q, "22.5: el veredicto: con dos no se puede (¿el profe lo puso mal?); con tres sí",
             !strcmp(t, "Con dos no se puede (" S_IQ "el profe lo puso mal?);\ncon tres s" S_ii ": (18 " S_PAR " 42) y 36 en serie = 22.5 MF " S_CHECK));
    chk_cond(q, "22.5: la primera de tres es el mixto exacto (18 " S_PAR " 42) y 36", r->val[0].tipo == CPM_MIX1 &&
             r->v[r->val[0].a] == 18.0 && r->v[r->val[0].b] == 42.0 && r->v[r->val[0].c] == 36.0 && r->val[0].ct == 22.5);
    chk_cond(q, "22.5: el rango", cp_tiene("Rango aceptable: 22 a 23 MF (22.5 " S_PM " 0.5).\n"));
    chk_cond(q, "22.5: paralelo no sirve (15 + 16 = 31 > 23)",
             cp_tiene("2. Paralelo no sirve: la suma m" S_ii "nima es 15 + 16 = 31 MF > 23.\n"));
    chk_cond(q, "22.5: serie con dos no sirve (41·42/83 = 20.7 < 22)",
             cp_tiene("3. Serie con dos no sirve: el m" S_aa "ximo posible es 41" S_CDOT "42/83 = 20.7 MF < 22.\n"));
    chk_cond(q, "22.5: lo dice y pasa a tres", cp_tiene("4. Con dos no se puede (" S_IQ "el profe lo puso mal?); con tres s" S_ii ":\n") &&
             cp_tiene("5. Mixto: primero el paralelo (a + b) y luego en serie con c: CT = (a + b)" S_CDOT "c/(a + b + c).") &&
             cp_tiene("   Prob" S_ee " todos los tr" S_ii "os (cada capacitor, una vez): "));
    chk_cond(q, "22.5: la cuenta del mixto, 2160/96 = 22.5", cp_tiene("   18 " S_PAR " 42 = 18 + 42 = 60 MF\n") &&
             cp_tiene("   60 en serie con 36: 60" S_CDOT "36/(60 + 36) = 2160/96 = 22.5 MF " S_CHECK "\n"));
    cpm_falta_directo(r, &lo, &hi);
    chk_cond(q, "22.5: y si tiene que ser con dos, te faltaría uno de 22 a 23", lo == 22.0 && hi == 23.0 &&
             cp_tiene("Si tiene que ser con dos, te faltar" S_ii "a") && cp_tiene("   Directo: uno de 22 a 23.\n"));
    n = cpm_faltas(r, CPM_PAR, f, CPM_MAX);
    for (i = 0; i < n; i++) if (f[i].c1 == 20.0) k20 = i;
    chk_cond(q, "22.5: o el 20 con uno de 2 a 3 (paralelo, los seis menores)",
             n == 6 && k20 >= 0 && f[k20].lo == 2.0 && f[k20].hi == 3.0 && cp_tiene("     el 20 con uno de 2 a 3\n"));
    n = cpm_faltas(r, CPM_SER, f, CPM_MAX);
    for (i = 0; i < n; i++) { if (f[i].c1 == 38.0) k38 = i; if (f[i].c1 == 42.0) k42 = i; }
    chk_cond(q, "22.5: o el 38 con uno de 53 a 58 (de 52.25 a 58.27): el 55 del profe",
             k38 >= 0 && f[k38].lo == 22.0 * 38.0 / 16.0 && fabs(f[k38].hi - 23.0 * 38.0 / 15.0) < 1e-12 &&
             f[k38].lo <= 55.0 && 55.0 <= f[k38].hi && cp_tiene("     el 38 con uno de 53 a 58 (p. ej. 55)\n"));
    chk_cond(q, "22.5: o el 42 con uno de 47 a 50 (de 46.2 a 50.8), como la espec.", k42 >= 0 && fabs(f[k42].lo - 46.2) < 1e-12 &&
             f[k42].hi > 50.8 && f[k42].hi < 50.9 && cp_tiene("     el 42 con uno de 47 a 50 (p. ej. 48)\n"));
    chk_cond(q, "22.5: y el menor que haría falta", cp_tiene("   El menor que har" S_ii "a falta: uno de 2 a 3, con el 20 en paralelo.\n"));
    {   /* el mixto del PDF, (20 || 28) y 42 = 22.4, vale (está entre los de tres) */
        double p = 20.0 + 28.0, ct = p * 42.0 / (p + 42.0);
        chk_cond(q, "22.5: el mixto del PDF, (20 " S_PAR " 28) y 42 = 22.4, vale", cpm_vale(r, ct) && fabs(ct - 22.4) < 1e-12);
    }
    /* con «Capacitores: 2», irresoluble, con lo de la v2 */
    r = cp_caso_n(22.5, 0.5, 2, "15-42 sin 21-24");
    chk_cond(q, "22.5 con 2: irresoluble, lo más cercano 41 y 42 (dif. -1.8)", r->ntot == 0 && r->nval == 0 && r->hay_cerca &&
             r->cerca.tipo == CPM_SER && r->v[r->cerca.a] == 41.0 && r->v[r->cerca.b] == 42.0 &&
             cp_tiene("IRRESOLUBLE con dos capacitores.\n") &&
             cp_tiene("Lo m" S_aa "s cercano: 41 y 42 en serie = 20.7 MF (dif. -1.8): no entra.\n"));
    chk_cond(q, "22.5 con 2: te faltaría y que con tres sí", cp_tiene("Te faltar" S_ii "a (uno m" S_aa "s en la bodega lo resuelve con dos):\n") &&
             cp_tiene("     el 38 con uno de 53 a 58 (p. ej. 55)\n") && cp_tiene("Con tres s" S_ii " se puede: ") &&
             cp_tiene(" arreglos (elige Capacitores: Auto o 3).\n"));
    cpm_veredicto(r, t, sizeof t);
    chk_cond(q, "22.5 con 2: el veredicto (sin «en serie»: cabe en 51 columnas)", !strcmp(t, "IRRESOLUBLE con dos (con tres s" S_ii ": elige Auto).\n"
                                                      "Lo m" S_aa "s cercano: 41 y 42 = 20.7 MF (dif. -1.8)"));

    /* Los repetidos (la v2): 14 con «1 2 3 4 5 6 7 10 12 7» */
    r = cp_caso(14.0, 0.0, "1 2 3 4 5 6 7 10 12 7");
    chk_cond(q, "14 con dos de 7: valen 12 " S_PAR " 2, 10 " S_PAR " 4 y 7 " S_PAR " 7, exactas y en ese orden",
             r->tot[2] == 3 && cp_esta(r, CPM_PAR, 2.0, 12.0) == 0 && cp_esta(r, CPM_PAR, 4.0, 10.0) == 1 &&
             cp_esta(r, CPM_PAR, 7.0, 7.0) == 2 && r->val[2].ct == 14.0);
    chk_cond(q, "...el profe para en 12 + ¿2?", cp_tiene("     12 + " S_IQ "2? " S_RARR " s" S_ii " hay 2 " S_CHECK " (C" S_SUB2 " = 14 - 12 = 2)\n") &&
             !cp_tiene("     10 +"));
    cpm_proced(r, &r->val[2], cp_txt, sizeof cp_txt);
    chk_cond(q, "...y con la opción 7 " S_PAR " 7, «hay dos de 7» y su cableado",
             cp_tiene("     7 + " S_IQ "7? " S_RARR " hay dos de 7 " S_CHECK " (C" S_SUB2 " = 14 - 7 = 7)\n") &&
             cp_tiene("CT = 7 + 7 = 14 MF " S_CHECK) &&
             cp_tiene("   Izquierdas de los dos de 7 unidas " S_RARR " A.\n"));
    r = cp_caso(14.0, 0.0, "1 2 3 4 5 6 7 10 12");
    chk_cond(q, "con un solo 7, 7 " S_PAR " 7 no sale", r->tot[2] == 2 && cp_esta(r, CPM_PAR, 7.0, 7.0) < 0);
    r = cp_caso(14.0, 1.0, "7 7 6.5 6.9");
    cpm_proced(r, &r->val[3], cp_txt, sizeof cp_txt);
    chk_cond(q, "14 " S_PM " 1 con dos de 7, la opción 6.9 " S_PAR " 6.5: al pasar por el 7, «hay dos de 7: sirve»",
             r->val[3].tipo == CPM_PAR && r->v[r->val[3].a] == 6.5 && r->v[r->val[3].b] == 6.9 &&
             cp_tiene(S_RARR " hay dos de 7: sirve (CT 14)\n") && cp_tiene("     6.9 + " S_IQ "6.1 a 8.1? " S_RARR " hay 6.5 " S_CHECK "\n"));
    r = cp_caso(22.5, 0.5, "9.8 10 12.5");
    cpm_proced(r, &r->val[1], cp_txt, sizeof cp_txt);
    chk_cond(q, "22.5 " S_PM " 0.5, la opción 2 (12.5 " S_PAR " 9.8): el intento del 12.5 dice el 9.8 elegido, no el 10",
             r->val[1].tipo == CPM_PAR && r->v[r->val[1].a] == 9.8 && cp_tiene(" hay 9.8 " S_CHECK "\n") && !cp_tiene(" hay 10 " S_CHECK));
    r = cp_caso(14.0, 0.0, "7 7");
    chk_cond(q, "con «7 7»: 7 " S_PAR " 7 = 14 " S_CHECK ", «hay dos de 7»", r->nval == 1 && r->val[0].tipo == CPM_PAR &&
             r->val[0].a == r->val[0].b && cp_tiene("     7 + " S_IQ "7? " S_RARR " hay dos de 7 " S_CHECK));
    r = cp_caso(14.0, 0.0, "7");
    chk_cond(q, "con «7» solo: irresoluble, «solo hay un 7» y te faltaría otro de 7", r->ntot == 0 &&
             cp_tiene("IRRESOLUBLE") && cp_tiene("     el 7 con otro de 7\n"));
    r = cp_caso(14.0, 0.0, "28 28 50");
    chk_cond(q, "la serie de dos iguales: dos de 28 dan 14 (hay dos)", cp_esta(r, CPM_SER, 28.0, 28.0) == 0 &&
             cp_tiene("     Dos iguales dan la mitad: dos de 28 dar" S_ii "an 14 " S_RARR " hay dos " S_CHECK "\n") &&
             cp_tiene("     con 28 falta 392/14 = 28 " S_RARR " hay dos de 28 " S_CHECK "\n") &&
             cp_tiene("   A " S_RARR " izquierda de un 28.\n") && cp_tiene("   Su derecha " S_RARR " izquierda del otro 28"));
    r = cp_caso(14.0, 0.0, "28 50");
    chk_cond(q, "...con uno solo, no: «solo hay un 28»", r->tot[2] == 0 && cp_tiene("     con 28 falta 28 " S_RARR " solo hay un 28\n"));
}

/* ---- c. Los bordes ---- */
static void cp_bordes(void)
{
    const char *q = "capacitores: bordes";
    const CpmRes *r;
    static CpmFalta f[CPM_MAX];
    char t[300];

    r = cp_caso(10.0, 0.0, "");
    chk_cond(q, "bodega vacía: nada, ni lo más cercano", r->n == 0 && r->nval == 0 && r->ntot == 0 && !r->hay_cerca);
    chk_cond(q, "bodega vacía: lo dice, y lo que faltaría", cp_tiene("La bodega est" S_aa " vac" S_ii "a") &&
             cp_tiene("   Directo: uno de 10.\n"));
    cpm_veredicto(r, t, sizeof t);
    chk_cond(q, "bodega vacía: el veredicto", !strcmp(t, "IRRESOLUBLE: la bodega est" S_aa " vac" S_ii "a."));
    r = cp_caso(10.0, 0.0, "10");
    chk_cond(q, "un valor que es el pedido: sustitución directa", r->nval == 1 && r->val[0].tipo == CPM_UNO &&
             cp_tiene("1. " S_IQ "Uno solo? Hay de 10: sustituci" S_oo "n directa.\n") && cp_tiene("2. Comprobaci" S_oo "n: CT = 10 MF " S_CHECK));
    cpm_veredicto(r, t, sizeof t);
    chk_cond(q, "...y el veredicto «Con 1»", !strcmp(t, "Con 1: 10 MF solo " S_CHECK));
    r = cp_caso(11.0, 0.0, "10");
    chk_cond(q, "un valor que no: lo más cercano es él",
             r->nval == 0 && r->hay_cerca && r->cerca.tipo == CPM_UNO && cp_tiene("Paralelo: hacen falta dos"));
    chk_cond(q, "un valor que no: el 10 con uno de 1 (paralelo)", cpm_faltas(r, CPM_PAR, f, CPM_MAX) == 1 && f[0].c1 == 10.0 &&
             fabs(f[0].lo - 0.95) < 1e-12 && fabs(f[0].hi - 1.05) < 1e-12 && cp_tiene("     el 10 con uno de 1\n"));
    r = cp_caso(12.0, 0.0, CPM_BODEGA_A);
    chk_cond(q, "el pedido ya en la bodega: el primero, uno solo", r->val[0].tipo == CPM_UNO && r->v[r->val[0].a] == 12.0 &&
             cp_tiene("Hay de 12: sustituci" S_oo "n directa"));
    chk_cond(q, "...y también los arreglos exactos, detrás (10 + 2, 9 + 3)",
             cp_esta(r, CPM_PAR, 2.0, 10.0) > 0 && cp_esta(r, CPM_PAR, 3.0, 9.0) > 0);
    cpm_proced(r, &r->val[1], cp_txt, sizeof cp_txt);
    chk_cond(q, "...y el paso a paso de otra opción dice que la de uno es la 1",
             cp_tiene("1. " S_IQ "Uno solo? Hay de 12 (exacto): es la opci" S_oo "n 1. Sigo con la que elegiste.\n"));
    r = cp_caso(14.05, 0.0, "14");
    chk_cond(q, "sin tolerancia, ±0.05: 14 vale para 14.05", r->nval == 1);
    r = cp_caso(14.06, 0.0, "14");
    chk_cond(q, "...y no para 14.06", r->nval == 0);
    r = cp_caso(14.0, 0.0, "21 42");
    chk_cond(q, "21" S_CDOT "42/63 da 14 exacto (14 " S_CHECK ")", r->nval == 1 && r->val[0].ct == 14.0);
    r = cp_caso(10.0, 0.0, "2.5 7.5");
    chk_cond(q, "decimales: 2.5 + 7.5 = 10", r->nval == 1 && r->val[0].tipo == CPM_PAR && r->val[0].ct == 10.0 &&
             cp_tiene("CT = 7.5 + 2.5 = 10 MF " S_CHECK));
    r = cp_caso(1.875, 0.0, "2.5 7.5");
    chk_cond(q, "decimales: 2.5 y 7.5 en serie = 1.875, como se pidi" S_oo " (sin fracci" S_oo "n de enteros)",
             r->nval == 1 && r->val[0].tipo == CPM_SER && cp_tiene("CT = 2.5" S_CDOT "7.5/(2.5 + 7.5) = 1.875 MF " S_CHECK "\n"));
    r = cp_caso(22.5, 0.5, "23 22.4 10 12.5");
    chk_cond(q, "con tolerancia, en Auto, los de uno primero (22.4 a 0.1, luego 23) y después el paralelo exacto",
             r->nval >= 3 && r->val[0].tipo == CPM_UNO && r->v[r->val[0].a] == 22.4 && r->val[1].tipo == CPM_UNO &&
             r->v[r->val[1].a] == 23.0 && r->val[2].tipo == CPM_PAR && r->val[2].ct == 22.5);
    chk_cond(q, "...y el texto: hay de 22.4, sustitución directa", cp_tiene("Hay de 22.4 (dif. -0.1): sustituci" S_oo "n directa.\n"));
    r = cp_caso(12.0, 0.0, "30 20 10 2");
    chk_cond(q, "a igualdad, el paralelo antes que la serie: 10 " S_PAR " 2 y luego 20 y 30",
             r->tot[2] == 2 && r->val[0].tipo == CPM_PAR && r->val[1].tipo == CPM_SER && r->val[0].ct == 12.0 && r->val[1].ct == 12.0);
    r = cp_caso(50.0, 2.0, "1-100");
    chk_cond(q, "cien valores (50 " S_PM " 2): se resuelve, se enseñan los 5 de uno, 50 de dos y 20 de tres, y el texto no se corta",
             r->n == 100 && r->nver[1] == 5 && r->nver[2] == CPM_VER2 && r->tot[2] > CPM_VER2 && r->nver[3] == CPM_VER3 &&
             r->nval == 5 + CPM_VER2 + CPM_VER3 && strlen(cp_txt) < sizeof cp_txt - 1);
    r = cp_caso(500.5, 0.5, "1-50 2000-2049");
    chk_cond(q, "cien valores sin solución: cien «te faltaría», y el texto no se corta",
             r->n == 100 && r->ntot == 0 && strlen(cp_txt) < 20000 && cp_tiene("     el 1 con uno de 499 a 500\n"));
    /* el modo «Capacitores» */
    r = cp_caso_n(17.0, 0.0, 2, CPM_BODEGA_A);
    chk_cond(q, "modo 2: solo los de dos (17 A: la de 10 " S_PAR " 7, nada de tres)", r->ntot == 1 && r->nval == 1 &&
             r->nver[3] == 0 && r->tot[3] > 0 && r->val[0].tipo == CPM_PAR && cp_tiene("Capacitores: 2 (lo pide el profe)") &&
             !cp_tiene(S_IQ "Uno solo?"));
    r = cp_caso_n(12.0, 0.0, 2, CPM_BODEGA_A);
    chk_cond(q, "modo 2 con el pedido en la bodega: no cuenta el de uno (va 10 + 2)", r->val[0].tipo == CPM_PAR && r->nver[1] == 0);
    r = cp_caso_n(14.0, 0.0, 3, CPM_BODEGA_B);
    chk_cond(q, "modo 3: solo de tres (14 B: el mixto (15 " S_PAR " 27) y 21; la serie de tres no llega)", r->nver[2] == 0 &&
             r->ntot == r->tot[3] && r->val[0].tipo == CPM_MIX1 && r->v[r->val[0].a] == 15.0 && r->v[r->val[0].b] == 27.0 &&
             r->v[r->val[0].c] == 21.0 && cp_esta3(r, CPM_SER3, 0, 0, 0) < 0 && cp_tiene("Capacitores: 3 (lo pide el profe)") &&
             cp_tiene("Con tres: en paralelo se suman"));
    r = cp_caso_n(17.0, 0.0, 3, "10 7");
    chk_cond(q, "modo 3 con dos capacitores: irresoluble, «hacen falta tres» y que con dos sí", r->ntot == 0 && !r->hay_cerca &&
             cp_tiene("IRRESOLUBLE con tres capacitores.\n") && cp_tiene("   Hacen falta tres y en la bodega hay menos.\n") &&
             cp_tiene("Con uno o dos s" S_ii " se puede: elige Capacitores: Auto.\n"));
    cpm_veredicto(r, t, sizeof t);
    chk_cond(q, "...y su veredicto", !strcmp(t, "IRRESOLUBLE con tres (con menos s" S_ii ": elige Auto)."));
    r = cp_caso_n(100.0, 0.0, 3, "10 15 30");
    chk_cond(q, "modo 3 irresoluble: lo más cercano de tres (10 + 15 + 30 = 55)", r->ntot == 0 && r->hay_cerca &&
             r->cerca.tipo == CPM_PAR3 && r->cerca.ct == 55.0 && cp_tiene("Lo m" S_aa "s cercano de tres: 30 " S_PAR " 15 " S_PAR " 10 = 55 MF"));
    r = cp_caso(100.0, 0.0, "10 15 30");
    chk_cond(q, "Auto irresoluble: lo más cercano de 1, 2 o 3 (el de tres, 55) y te faltaría (para dos)", r->ntot == 0 &&
             r->hay_cerca && r->cerca.tipo == CPM_PAR3 && cp_tiene("IRRESOLUBLE con esta bodega (ni con uno, dos o tres).\n") &&
             cp_tiene("   Con tres tampoco sale") && cp_tiene("Te faltar" S_ii "a"));
    r = cp_caso(55.0, 0.0, "10 15 20 25");
    chk_cond(q, "solo con tres en paralelo: 25 + 20 + 10 = 55", r->tot[1] + r->tot[2] == 0 && r->val[0].tipo == CPM_PAR3 &&
             cp_tiene("Con tres en paralelo se suman los tres: busco tres que sumen 55.") &&
             cp_tiene("Comprobaci" S_oo "n: CT = 25 + 20 + 10 = 55 MF " S_CHECK) &&
             cp_tiene("   Izquierdas de los tres (10, 20 y 25) unidas " S_RARR " A.\n"));
    r = cp_caso(5.0, 0.0, "10 15 30");
    chk_cond(q, "solo con tres en serie: 1/10 + 1/15 + 1/30 = 1/5, 4500/900", r->val[0].tipo == CPM_SER3 &&
             cp_tiene("   1/CT = 1/10 + 1/15 + 1/30\n") && cp_tiene("      = 4500/900 = 5 MF " S_CHECK "\n") &&
             cp_tiene("   Derecha del 2.º " S_RARR " izquierda del 3.º, el 30 (otro punto medio).\n"));
    r = cp_caso(17.0, 0.0, "20 30 5");
    chk_cond(q, "el mixto 2: (20 y 30 en serie) " S_PAR " 5 = 12 + 5 = 17", r->val[0].tipo == CPM_MIX2 && r->val[0].ct == 17.0 &&
             cp_tiene("   20 y 30 en serie: 20" S_CDOT "30/(20 + 30) = 600/50 = 12 MF\n") &&
             cp_tiene("   12 " S_PAR " 5: CT = 12 + 5 = 17 MF " S_CHECK "\n") &&
             cp_tiene("   A " S_RARR " izquierdas del 20 de arriba y del 5 de abajo (unidas).\n"));
    cpm_titulo(r, &r->val[0], t, sizeof t);
    chk_cond(q, "...y su título", !strcmp(t, "(20 y 30 en serie) " S_PAR " 5 = 17 MF"));
    cpm_fila(r, &r->val[0], t, sizeof t);
    chk_cond(q, "...y su fila de la lista", !strcmp(t, "mixto: (20 y 30) " S_PAR " 5 = 17 MF"));
    r = cp_caso(9.0, 0.0, "7 7 7 2 30");
    chk_cond(q, "tres iguales (hay tres de 7): 7 y 7 y 7 en serie no da 9; (7 y 7) || 2 = 5.5 tampoco; 7 + 2 sí",
             r->val[0].tipo == CPM_PAR && cp_esta(r, CPM_PAR, 2.0, 7.0) == 0);
    r = cp_caso(21.0, 0.0, "7 7 7");
    chk_cond(q, "...y 7 + 7 + 7 = 21 con tres de 7", r->tot[3] >= 1 && cp_esta3(r, CPM_PAR3, 7.0, 7.0, 7.0) == 0);
    r = cp_caso(21.0, 0.0, "7 7");
    chk_cond(q, "...pero no con dos de 7", cp_esta3(r, CPM_PAR3, 7.0, 7.0, 7.0) < 0 && r->ntot == 0);
}

/* ---- d. Al azar, contra una fuerza bruta escrita aparte ---- */
static int cp_bruta_vale(double ct, double C, double tol)
{
    double t = tol > 0.0 ? tol : 0.05;
    return ct >= C - t - 1e-9 && ct <= C + t + 1e-9;
}

/* El CT, con otras cuentas que la app (las de los inversos) */
static double cp_bruta_ct(int tipo, double a, double b, double c)
{
    switch (tipo) {
    case CPM_UNO:  return a;
    case CPM_PAR:  return a + b;
    case CPM_SER:  return 1.0 / (1.0 / a + 1.0 / b);
    case CPM_PAR3: return a + b + c;
    case CPM_SER3: return 1.0 / (1.0 / a + 1.0 / b + 1.0 / c);
    case CPM_MIX1: return 1.0 / (1.0 / (a + b) + 1.0 / c);
    default:       return 1.0 / (1.0 / a + 1.0 / b) + c;
    }
}

/* Todas las combinaciones de la bodega (valores distintos d con sus
 * cuentas q), sin repetir capacitor: las cuenta por número y junta las
 * distancias de las que valen (y la más cercana de cada número) */
static double cp_bd[4][200000];
static int cp_bn[4];
static void cp_bruta(const double *d, const int *q, int nd, double C, double tol, long *cuenta, double *cerca)
{
    int i, j, k, t, c;
    for (c = 0; c < 4; c++) { cuenta[c] = 0; cerca[c] = 1e300; cp_bn[c] = 0; }
    for (i = 0; i < nd; i++)
        for (j = i; j <= nd; j++)
            for (k = j; k <= nd; k++) {
                /* (i), (i, j) o (i, j, k) con j, k = nd: «nada» */
                int n = j == nd ? 1 : k == nd ? 2 : 3, u[3], m, ok = 1;
                if (n == 1 && k != nd) continue;
                u[0] = i; u[1] = j; u[2] = k;
                for (m = 0; m < n; m++) {
                    int cu = 0, z;
                    for (z = 0; z < n; z++) cu += u[z] == u[m];
                    if (cu > q[u[m]]) ok = 0;
                }
                if (!ok) continue;
                for (t = 0; t < CPM_NT; t++) {
                    double ct;
                    int pos;
                    if (cpm_ncap(t) != n) continue;
                    if (t == CPM_MIX1 || t == CPM_MIX2) {
                        /* el tercero es cualquiera de los tres: (i j) k, (i k) j, (j k) i */
                        for (pos = 0; pos < 3; pos++) {
                            int a = pos == 2 ? j : i, b = pos == 0 ? j : k, s = pos == 0 ? k : pos == 1 ? j : i;
                            if (pos > 0 && ((pos == 1 && j == k) || (pos == 2 && i == j))) continue;   /* el mismo arreglo */
                            ct = cp_bruta_ct(t, d[a], d[b], d[s]);
                            if (fabs(ct - C) < cerca[3]) cerca[3] = fabs(ct - C);
                            if (cp_bruta_vale(ct, C, tol)) { cuenta[3]++; if (cp_bn[3] < 200000) cp_bd[3][cp_bn[3]++] = fabs(ct - C); }
                        }
                        continue;
                    }
                    ct = cp_bruta_ct(t, d[i], n > 1 ? d[j] : 0.0, n > 2 ? d[k] : 0.0);
                    if (fabs(ct - C) < cerca[n]) cerca[n] = fabs(ct - C);
                    if (cp_bruta_vale(ct, C, tol)) { cuenta[n]++; if (cp_bn[n] < 200000) cp_bd[n][cp_bn[n]++] = fabs(ct - C); }
                }
            }
}

static int cp_cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}

static void cp_azar(void)
{
    const char *q = "capacitores: al azar contra la fuerza bruta";
    static CpmRes r;
    static double v[CPM_MAX], w[CPM_MAX + 1], d[CPM_MAX];
    static CpmLinea l[CPM_TXT * 8];
    static char txt[32768];
    static CpmFalta f[CPM_MAX];
    int caso, malos[12] = { 0 }, i, j, nl, n, cuantos[4] = { 0 }, modos[4] = { 0 };
    unsigned s0 = semilla;
    semilla = 7621u;
    for (caso = 0; caso < 1500; caso++) {
        double C, tol, cerca[4];
        long cuenta[4];
        int nd = 0, qd[CPM_MAX], modo = (int)azar(3), mask, c;
        modo = modo == 0 ? 0 : modo + 1;
        mask = cpm_mascara(modo);
        modos[modo]++;
        n = (int)azar(13);
        for (i = 0; i < n; i++) {                           /* enteros de 1 a 40 y algún .5; con repetidos */
            if (i > 0 && azar(5) == 0) v[i] = v[azar((unsigned)i)];
            else v[i] = (double)(1 + azar(40)) + (azar(5) == 0 ? 0.5 : 0.0);
        }
        C = (double)(2 + azar(55)) + (azar(4) == 0 ? 0.5 : 0.0);
        tol = azar(3) == 0 ? 0.5 * (double)(1 + azar(2)) : 0.0;
        cpm_resuelve_n(&r, C, tol, modo, v, n);
        /* la bodega: los distintos con sus cuentas */
        for (i = 0; i < n; i++) w[i] = v[i];
        qsort(w, (size_t)n, sizeof w[0], cp_cmp_d);
        for (i = 0; i < n; i++) {
            if (nd > 0 && w[i] == d[nd - 1]) qd[nd - 1]++;
            else { d[nd] = w[i]; qd[nd] = 1; nd++; }
        }
        if (nd != r.n || n != r.ncap) malos[0]++;
        for (i = 0; i < nd && i < r.n; i++) if (d[i] != r.v[i] || qd[i] != r.q[i]) malos[0]++;
        cp_bruta(d, qd, nd, C, tol, cuenta, cerca);
        /* las cuentas de las que valen: las de la fuerza bruta */
        for (c = 1; c <= 3; c++) if (cuenta[c] != r.tot[c]) malos[1]++;
        if (r.ntot != (mask & 1 ? cuenta[1] : 0) + (mask & 2 ? cuenta[2] : 0) + (mask & 4 ? cuenta[3] : 0)) malos[1]++;
        /* lo que se enseña: de cada número del modo, las más cercanas (todas
         * las de uno, 50 de dos y 20 de tres), en orden estricto */
        for (c = 1; c <= 3; c++) {
            int cap = c == 1 ? CPM_MAX : c == 2 ? CPM_VER2 : CPM_VER3;
            long debe = (mask >> (c - 1) & 1) ? (cuenta[c] < cap ? cuenta[c] : cap) : 0;
            if (r.nver[c] != debe) malos[2]++;
            if (debe > 0 && cp_bn[c] > 0) {
                double peor = 0.0;
                qsort(cp_bd[c], (size_t)cp_bn[c], sizeof cp_bd[c][0], cp_cmp_d);
                for (i = 0; i < r.nval; i++) if (cpm_ncap(r.val[i].tipo) == c && fabs(r.val[i].ct - C) > peor) peor = fabs(r.val[i].ct - C);
                if (peor > cp_bd[c][debe - 1] + 1e-9) malos[3]++;       /* ¿alguna de fuera más cerca? */
            }
            if (cuenta[c] > 0) cuantos[c]++;
        }
        for (i = 0; i < r.nval; i++) {
            const CpmComb *k = &r.val[i];
            int nc = cpm_ncap(k->tipo), u[3], m, z;
            double ct = cp_bruta_ct(k->tipo, r.v[k->a], r.v[k->b], r.v[k->c]);
            if (!cp_bruta_vale(ct, C, tol) || fabs(ct - k->ct) > 1e-9 * (1.0 + ct)) malos[4]++;
            u[0] = k->a; u[1] = k->b; u[2] = k->c;
            for (m = 0; m < nc; m++) {                      /* ningún capacitor dos veces */
                int cu = 0;
                for (z = 0; z < nc; z++) cu += u[z] == u[m];
                if (cu > r.q[u[m]]) malos[4]++;
            }
            if (i > 0 && cpm_compara(&r, &r.val[i - 1], &r.val[i]) >= 0) malos[5]++;
        }
        /* lo más cercano: nada del modo más cerca */
        if (r.ntot == 0) {
            double mejor = 1e300;
            for (c = 1; c <= 3; c++) if ((mask >> (c - 1) & 1) && cerca[c] < mejor) mejor = cerca[c];
            if (r.hay_cerca != (mejor < 1e300)) malos[6]++;
            else if (r.hay_cerca && (fabs(fabs(r.cerca.ct - C) - mejor) > 1e-9 || !((mask >> (cpm_ncap(r.cerca.tipo) - 1)) & 1))) malos[6]++;
        } else if (r.hay_cerca) malos[6]++;
        /* te faltaría: con «2», si no hay ninguna de dos (aunque uno solo
         * valga: §5.2); en Auto, si no hay de uno ni de dos; con «3», nunca */
        if (cpm_con_faltas(&r) != (modo != 3 && cuenta[2] == 0 && (modo == 2 || cuenta[1] == 0))) malos[7]++;
        /* ...y añadir lo del medio de cada rango lo resuelve: el directo con
         * uno; los demás, con dos */
        if (cpm_con_faltas(&r) && r.ncap < CPM_MAX) {
            static CpmRes r2;
            double lo, hi;
            int t, m;
            cpm_falta_directo(&r, &lo, &hi);
            for (i = 0; i < n; i++) w[i] = v[i];
            w[n] = (lo + hi) / 2.0;
            cpm_resuelve_n(&r2, C, tol, 2, w, n + 1);
            if (r2.tot[1] == 0) malos[7]++;
            for (t = CPM_PAR; t <= CPM_SER; t++) {
                m = cpm_faltas(&r, t, f, CPM_MAX);
                for (i = 0; i < m; i++) {
                    double x = (f[i].lo + f[i].hi) / 2.0;
                    if (!(f[i].lo < f[i].hi) || (t == CPM_PAR ? !(f[i].c1 < C) : !(f[i].c1 > C))) malos[8]++;
                    for (j = 0; j < n; j++) w[j] = v[j];
                    w[n] = x;                               /* (si es otro del mismo valor, ahora hay dos) */
                    cpm_resuelve_n(&r2, C, tol, 2, w, n + 1);
                    if (r2.tot[2] == 0) malos[7]++;
                }
            }
        }
        /* el paso a paso de cada opción que se enseña (como mucho 6): cabe,
         * no se corta y marca la elegida */
        for (i = 0; i <= r.nval && i < 6; i++) {
            const CpmComb *S = i < r.nval ? &r.val[i] : 0;
            if (!S && r.nval > 0) break;
            cpm_proced(&r, S, txt, sizeof txt);
            nl = cpm_envuelve(txt, 52, l, (int)(sizeof l / sizeof l[0]));
            for (j = 0; j < nl; j++) if (l[j].sangria + l[j].len > 52) malos[9]++;
            if (strlen(txt) >= sizeof txt - 1) malos[10]++;
            if (S) {
                int marcas = 0;
                const char *p = txt;
                while ((p = strstr(p, S_CHECK)) != 0) { marcas++; p++; }
                if (marcas < (cpm_ncap(S->tipo) == 2 ? 2 : 1)) malos[11]++;   /* el intento elegido y la comprobación */
            }
        }
    }
    semilla = s0;
    printf("    1500 bodegas al azar (modos %d/%d/%d; con solución de 1: %d, de 2: %d, de 3: %d): bodega %d, cuentas %d, "
           "enseñadas %d, cercanas %d, válidas %d, orden %d, lo más cercano %d, faltas %d/%d, ancho %d, corte %d, marca %d\n",
           modos[0], modos[2], modos[3], cuantos[1], cuantos[2], cuantos[3], malos[0], malos[1], malos[2], malos[3], malos[4],
           malos[5], malos[6], malos[7], malos[8], malos[9], malos[10], malos[11]);
    chk_cond(q, "la bodega: los distintos con sus cuentas (los repetidos cuentan)", malos[0] == 0);
    chk_cond(q, "las cuentas de las que valen (1, 2 y 3) son las de la fuerza bruta", malos[1] == 0);
    chk_cond(q, "se enseñan las que tocan (todas las de uno, 50 de dos y 20 de tres), las más cercanas", malos[2] == 0 && malos[3] == 0);
    chk_cond(q, "cada una vale, con su CT, y no usa un capacitor dos veces", malos[4] == 0);
    chk_cond(q, "el orden: menos capacitores primero, luego la más cercana, sin empates", malos[5] == 0);
    chk_cond(q, "lo más cercano del modo, sin nada más cerca", malos[6] == 0);
    chk_cond(q, "te faltaría: cuando toca (con «2», sin ninguna de dos aunque uno valga) y añadir lo del medio de cada rango lo resuelve",
             malos[7] == 0 && malos[8] == 0);
    chk_cond(q, "el paso a paso de cada opción cabe en 52 columnas, entero, y marca la elegida",
             malos[9] == 0 && malos[10] == 0 && malos[11] == 0);
    chk_cond(q, "el azar llega a todo (los tres modos; soluciones de 1, 2 y 3)", modos[0] > 300 && modos[2] > 300 &&
             modos[3] > 300 && cuantos[1] > 50 && cuantos[2] > 100 && cuantos[3] > 100);
}

/* ---- e. El volcado del oráculo (el contrato v3, §6) ---- */
/* Cada línea de ent es un caso: «<C> <tol> <n> : <v1> <v2> ...» (n = 0 Auto,
 * 2 o 3; sin n, Auto; los valores pueden repetirse). En sal, por caso:
 * «caso k», «valida N», las N «sol tipo a [b [c]] CT» en el orden de la
 * lista y SIN el tope de pantalla, «cerca tipo ... CT» (si no hay ninguna
 * con el n pedido), «falta directo lo hi», «falta par C1 lo hi» y «falta
 * ser C1 lo hi» (si no hay ninguna de 1 o 2 y n no es 3) y «fin». Números
 * con %.6f */
static const char *const CP_TIPO[CPM_NT] = { "uno", "par", "ser", "par3", "ser3", "mix1", "mix2" };
static CpmComb *cpv_l;
static long cpv_n, cpv_cap;
static const CpmRes *cpv_r;
static void cpv_junta(void *ctx, const CpmComb *k)
{
    (void)ctx;
    if (cpv_n >= cpv_cap) {
        long nc = cpv_cap ? 2 * cpv_cap : 4096;
        CpmComb *p = (CpmComb *)realloc(cpv_l, (size_t)nc * sizeof *p);
        if (!p) return;
        cpv_l = p; cpv_cap = nc;
    }
    cpv_l[cpv_n++] = *k;
}
static int cpv_cmp(const void *x, const void *y) { return cpm_compara(cpv_r, (const CpmComb *)x, (const CpmComb *)y); }
static void cpv_comb(FILE *fo, const char *que, const CpmRes *r, const CpmComb *k)
{
    int n = cpm_ncap(k->tipo);
    fprintf(fo, "%s %s %.6f", que, CP_TIPO[k->tipo], r->v[k->a]);
    if (n > 1) fprintf(fo, " %.6f", r->v[k->b]);
    if (n > 2) fprintf(fo, " %.6f", r->v[k->c]);
    fprintf(fo, " %.6f\n", k->ct);
}

static int cpm_volcado(const char *ent, const char *sal)
{
    static char lin[65536];
    static CpmRes r;
    static CpmFalta f[CPM_MAX];
    FILE *fi = ent ? fopen(ent, "r") : 0, *fo = sal ? fopen(sal, "w") : stdout;
    int caso = 0, i, t;
    if (!fi || !fo) { printf("volcado: no se pudo abrir %s o %s\n", ent ? ent : "(nada)", sal ? sal : "(nada)"); return 1; }
    while (fgets(lin, sizeof lin, fi)) {
        double C, tol, v[CPM_MAX];
        char *p = lin, *e;
        int n = 0, mal = 0, modo = 0;
        long k;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\n' || *p == '\r' || !*p) continue;
        fprintf(fo, "caso %d\n", ++caso);
        C = strtod(p, &e); if (e == p) mal = 1; p = e;
        tol = strtod(p, &e); if (e == p) mal = 1; p = e;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != ':') {                                  /* el modo (sin él, el contrato v1: Auto) */
            long m = strtol(p, &e, 10);
            if (e == p || (m != 0 && m != 2 && m != 3)) mal = 1;
            modo = (int)m;
            p = e;
            while (*p == ' ' || *p == '\t') p++;
        }
        if (*p != ':') mal = 1; else p++;
        for (;;) {
            double x = strtod(p, &e);
            if (e == p) break;
            if (n >= CPM_MAX || !(x > 0.0)) { mal = 1; break; }
            v[n++] = x;
            p = e;
        }
        if (mal || !(C > 0.0) || tol < 0.0) { fprintf(fo, "error\nfin\n"); continue; }
        cpm_resuelve_n(&r, C, tol, modo, v, n);
        cpv_n = 0;
        cpm_recorre(&r, cpm_mascara(r.modo), cpv_junta, 0);
        cpv_r = &r;
        if (cpv_n > 1) qsort(cpv_l, (size_t)cpv_n, sizeof cpv_l[0], cpv_cmp);
        fprintf(fo, "valida %ld\n", cpv_n);
        for (k = 0; k < cpv_n; k++) cpv_comb(fo, "sol", &r, &cpv_l[k]);
        if (cpv_n == 0 && r.hay_cerca) cpv_comb(fo, "cerca", &r, &r.cerca);
        if (cpm_con_faltas(&r)) {
            double lo, hi;
            cpm_falta_directo(&r, &lo, &hi);
            fprintf(fo, "falta directo %.6f %.6f\n", lo, hi);
            for (t = CPM_PAR; t <= CPM_SER; t++) {
                int m = cpm_faltas(&r, t, f, CPM_MAX);
                for (i = 0; i < m; i++) fprintf(fo, "falta %s %.6f %.6f %.6f\n", CP_TIPO[t], f[i].c1, f[i].lo, f[i].hi);
            }
        }
        fprintf(fo, "fin\n");
    }
    fclose(fi);
    if (fo != stdout) fclose(fo);
    printf("volcado: %d casos\n", caso);
    return 0;
}

static void cp_volcado(void)
{
    const char *q = "capacitores: el volcado del oráculo (v3)";
    char ent[300], sal[300];
    static char todo[1 << 20];
    size_t n;
    FILE *f;
    sprintf(ent, "%s/cm_volcado_ent.txt", dir_out);
    sprintf(sal, "%s/cm_volcado_sal.txt", dir_out);
    f = fopen(ent, "w");
    if (!f) { chk_cond(q, "se escribe la entrada", 0); return; }
    fprintf(f, "17 0 2 : 1 2 3 7 9 10 12 28 32 34 35 38 42 60\n\n");
    fprintf(f, "22.5 0.5 2 : 15 16 17 18 19 20 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42\n");
    fprintf(f, "10 0 0 :\n");
    fprintf(f, "12 0 : 30 20 18 36\n");
    fprintf(f, "14 0 0 : 1 2 3 4 5 6 7 10 12 7\n");
    fprintf(f, "5 0 0 : 10 15 30\n");
    fprintf(f, "100 0 3 : 10 15 30\n");
    fprintf(f, "17 0 3 : 20 30 5 7 7\n");
    fprintf(f, "21 0 0 : 7 7 7\n");
    fprintf(f, "17 0 4 : 10 7\n");
    fprintf(f, "22.5 0.5 0 : 15 16 17 18 19 20 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42\n");
    fprintf(f, "28 0 2 : 1 2 3 7 9 10 12 28 32 34 35 38 42 60\n");
    fclose(f);
    chk_cond(q, "corre", cpm_volcado(ent, sal) == 0);
    f = fopen(sal, "r");
    n = f ? fread(todo, 1, sizeof todo - 1, f) : 0;
    if (f) fclose(f);
    todo[n] = 0;
    chk_cond(q, "17 A con 2, entero: una válida, par 7 10 = 17",
             strstr(todo, "caso 1\nvalida 1\nsol par 7.000000 10.000000 17.000000\nfin\n") == todo);
    chk_cond(q, "22.5 con 2: valida 0 y lo más cercano, 41 y 42 en serie",
             strstr(todo, "caso 2\nvalida 0\ncerca ser 41.000000 42.000000 20.746988\nfalta directo 22.000000 23.000000\n") != 0);
    chk_cond(q, "22.5 con 2: las faltas del paralelo en el orden de la bodega (15 primero, 20 último)",
             strstr(todo, "falta par 15.000000 7.000000 8.000000\n") != 0 &&
             strstr(todo, "falta par 20.000000 2.000000 3.000000\nfalta ser 25.000000 ") != 0);
    chk_cond(q, "22.5 con 2: el 38 en serie de 52.25 a 58.27, y ya no hay línea «mixto»",
             strstr(todo, "falta ser 38.000000 52.250000 58.266667\n") != 0 &&
             strstr(todo, "falta ser 42.000000 46.200000 50.842105\nfin\n") != 0 && !strstr(todo, "mixto"));
    chk_cond(q, "bodega vacía: sin cerca, solo el directo",
             strstr(todo, "caso 3\nvalida 0\nfalta directo 9.950000 10.050000\nfin\n") != 0);
    chk_cond(q, "12 sin n (el contrato v1, Auto): las dos exactas, en orden (18 36 antes que 20 30)",
             strstr(todo, "caso 4\nvalida 2\nsol ser 18.000000 36.000000 12.000000\nsol ser 20.000000 30.000000 12.000000\nfin\n") != 0);
    chk_cond(q, "14 con dos de 7 (Auto): las tres de dos, con par 7 7, y luego las de tres, todas, sin tope",
             strstr(todo, "caso 5\nvalida 10\nsol par 2.000000 12.000000 14.000000\nsol par 4.000000 10.000000 14.000000\n"
                          "sol par 7.000000 7.000000 14.000000\nsol par3 1.000000 3.000000 10.000000 14.000000\n") != 0);
    chk_cond(q, "5 (Auto): solo la serie de tres, y como no hay de 1 o 2, las faltas",
             strstr(todo, "caso 6\nvalida 1\nsol ser3 10.000000 15.000000 30.000000 5.000000\nfalta directo 4.950000 5.050000\n") != 0);
    chk_cond(q, "100 con 3: irresoluble, lo más cercano de tres, sin faltas (n = 3)",
             strstr(todo, "caso 7\nvalida 0\ncerca par3 10.000000 15.000000 30.000000 55.000000\nfin\n") != 0);
    chk_cond(q, "17 con 3: los mixtos «a b c» (mix2 (20 serie 30) || 5 primero de los exactos; nada de dos)",
             strstr(todo, "caso 8\nvalida ") != 0 && strstr(todo, "sol mix2 20.000000 30.000000 5.000000 17.000000\n") != 0 &&
             !strstr(strstr(todo, "caso 8\n"), "sol par "));
    chk_cond(q, "21 con tres de 7: par3 7 7 7", strstr(todo, "caso 9\nvalida 1\nsol par3 7.000000 7.000000 7.000000 21.000000\nfalta") != 0);
    chk_cond(q, "un n que no es 0, 2 ni 3: error", strstr(todo, "caso 10\nerror\nfin\n") != 0);
    {   /* 22.5 en Auto: TODAS las de tres (más de las 20 de la pantalla), y las faltas */
        const char *p = strstr(todo, "caso 11\nvalida "), *fin = p ? strstr(p, "fin\n") : 0;
        long nv = p ? atol(p + 15) : -1, nsol = 0;
        const char *s = p;
        while (s && (s = strstr(s, "\nsol ")) != 0 && s < fin) { nsol++; s++; }
        chk_cond(q, "22.5 en Auto: todas las de tres, sin el tope de pantalla, y luego las faltas",
                 nv > CPM_VER3 && nsol == nv && p && strstr(p, "sol mix1 18.000000 42.000000 36.000000 22.500000\n") &&
                 strstr(p, "falta directo 22.000000 23.000000\n") != 0);
    }
    chk_cond(q, "28 con 2 y la A (el 28 ya vale solo): valida 0, lo más cercano de dos y las faltas (§5.2: con 2, te faltaría)",
             strstr(todo, "caso 12\nvalida 0\ncerca par 1.000000 28.000000 29.000000\nfalta directo 27.950000 28.050000\n"
                          "falta par 1.000000 26.950000 27.050000\n") != 0 &&
             strstr(todo, "falta ser 60.000000 52.324493 52.676056\nfin\n") != 0);
}

/* ---- f. Las pantallas ---- */
static void cp_entra_menu(void)
{
    ui_init();
    TECLAS(T_ENTER, T_ENTER);               /* la portada, el menú: el Taller */
    repite(T_ABAJO, 3);
    TECLAS(T_ENTER);                        /* su menú */
}
static void cp_entra(void) { cp_entra_menu(); TECLAS(T_ENTER); }   /* Resolver: los datos */
static void cp_monta_menu(void) { cp_entra_menu(); }
static void cp_monta_datos(void) { cp_entra(); }
static void cp_monta_res(void) { cp_entra(); TECLAS(T_ENTER); }
static void cp_monta_paso(void) { cp_entra(); TECLAS(T_ENTER, T_DER); }
static void cp_monta_dib(void) { cp_entra(); TECLAS(T_ENTER, T_ENTER); }
static void cp_monta_teo(void) { cp_entra_menu(); TECLAS(T_DIG(2)); }
static void cp_monta_pag(void) { cp_entra_menu(); TECLAS(T_DIG(2), T_ENTER); }
static void cp_monta_ej(void) { cp_entra_menu(); TECLAS(T_DIG(3)); }
static void cp_monta_irres(void)
{
    cp_entra();
    TECLAS(T_DIG(2), T_DIG(2), T_PUNTO, T_DIG(5), T_ABAJO, T_DIG(0), T_PUNTO, T_DIG(5), T_ABAJO, T_LETRA('B'),
           T_ESPACIO, T_LETRA('S'), T_LETRA('I'), T_LETRA('N'), T_ESPACIO, T_DIG(2), T_DIG(1), T_MENOS, T_DIG(2), T_DIG(4));
}

/* Lo que no se pisa en el dibujo de la opción que se ve: los rótulos sobre
 * fondo limpio, dentro de la pantalla y sin montarse con otro rótulo ni con
 * un óvalo */
static int cp_dibujo_limpio(const char *nom)
{
    CpmCaja c[16];
    int n, i, j, mal = 0;
    ui_dibuja();
    n = cpm_cajas(c, 16);
    for (i = 0; i < n; i++) {
        if (c[i].x < 0 || c[i].y < 16 || c[i].x + c[i].w > SCR_W || c[i].y + c[i].h > 227) mal++;   /* entre la barra y el pie */
        if (c[i].ocupados > 0) { mal++; if (getenv("HAKARU_DEPURA")) printf("    %s: rótulo %d pisa %d píxeles\n", nom, i, c[i].ocupados); }
        for (j = i + 1; j < n; j++)
            if (c[i].x < c[j].x + c[j].w && c[j].x < c[i].x + c[i].w && c[i].y < c[j].y + c[j].h && c[j].y < c[i].y + c[i].h) {
                mal++;
                if (getenv("HAKARU_DEPURA")) printf("    %s: las cajas %d y %d se montan\n", nom, i, j);
            }
    }
    return n >= 6 && mal == 0;
}

/* Las conexiones del dibujo, por los píxeles: las redes de cable (C_TEXTO,
 * 8-vecinos) que salen de un punto, y qué terminales cuadradas toca cada
 * una. Las terminales salen de las cajas de los óvalos (cpm_cajas: el
 * centro, ±13, un cuadrado de 10 de y - 4 a y + 5). Cada terminal es un
 * bit: 2·óvalo + lado (0 izquierda, 1 derecha) */
static unsigned char cp_visto[SCR_H][SCR_W];
static int cp_cola[SCR_W * SCR_H];

static unsigned cp_red(int x0, int y0, const CpmCaja *ov, int nov)
{
    const u16 *fb = gfx_buffer();
    unsigned toca = 0;
    int n = 0, i, k;
    if (fb[y0 * SCR_W + x0] != C_TEXTO) return 0x8000u;      /* la semilla no es cable */
    memset(cp_visto, 0, sizeof cp_visto);
    cp_cola[n++] = y0 * SCR_W + x0;
    cp_visto[y0][x0] = 1;
    for (i = 0; i < n; i++) {
        int x = cp_cola[i] % SCR_W, y = cp_cola[i] / SCR_W, dx, dy2;
        for (k = 0; k < nov; k++) {
            int cx = ov[k].x + 30, cy = ov[k].y + 11, lado;
            for (lado = 0; lado < 2; lado++) {
                int sx = cx + (lado ? 13 : -13) - 5, sy = cy - 4;
                if (x >= sx - 1 && x <= sx + 10 && y >= sy - 1 && y <= sy + 10) toca |= 1u << (2 * k + lado);
            }
        }
        for (dy2 = -1; dy2 <= 1; dy2++)
            for (dx = -1; dx <= 1; dx++) {
                int xx = x + dx, yy = y + dy2;
                if (xx < 0 || yy < 17 || xx >= SCR_W || yy >= 214 || cp_visto[yy][xx]) continue;
                if (fb[yy * SCR_W + xx] != C_TEXTO) continue;
                cp_visto[yy][xx] = 1;
                cp_cola[n++] = yy * SCR_W + xx;
            }
    }
    return toca;
}

/* tipo: el de la opción que se ve. 1 si cada red toca justo lo que debe:
 * A (30, 194), T (66, 194), la sólida (el extremo del cable, 113, 35) y las
 * del medio (los puntos medios y las curvas que unen terminales, sembradas
 * en su primer píxel) */
#define TL(k) (1u << (2 * (k)))
#define TR(k) (1u << (2 * (k) + 1))
static int cp_redes(int tipo, const char *nom, int imprime)
{
    CpmCaja c[16], ov[3];
    int n, i, nov = 0, ok, nm = 0, j;
    unsigned a, t, s, m[3] = { 0, 0, 0 }, ea, et, es, em[3] = { 0, 0, 0 };
    memset(ov, 0, sizeof ov);
    n = cpm_cajas(c, 16);
    for (i = 0; i < n && nov < 3; i++) if (c[i].ocupados == -1) ov[nov++] = c[i];
    a = cp_red(30, 194, ov, nov);
    t = cp_red(66, 194, ov, nov);
    s = cp_red(113, 35, ov, nov);
#define OX(k) (ov[k].x + 30)
#define OY(k) (ov[k].y + 11)
    switch (tipo) {
    case CPM_UNO: ea = TL(0); et = es = TR(0); break;
    case CPM_PAR: ea = TL(0) | TL(1); et = TR(1); es = TR(0) | TR(1); break;
    case CPM_SER:
        ea = TL(0); et = es = TR(1); em[0] = TR(0) | TL(1);
        m[0] = cp_red(OX(0) + 13, OY(0) + 5, ov, nov); nm = 1;   /* la derecha del primero, abajo */
        break;
    case CPM_PAR3:                  /* apilados: las izquierdas a A; la sólida arriba, T abajo, las derechas unidas */
        ea = TL(0) | TL(1) | TL(2); es = TR(0); et = TR(2); em[0] = TR(0) | TR(1) | TR(2);
        m[0] = cp_red(OX(0) + 18, OY(0) + 1, ov, nov); nm = 1;
        break;
    case CPM_SER3:
        ea = TL(0); et = es = TR(2); em[0] = TR(0) | TL(1); em[1] = TR(1) | TL(2);
        m[0] = cp_red(OX(0) + 13, OY(0) + 5, ov, nov); m[1] = cp_red(OX(1) + 13, OY(1) + 5, ov, nov); nm = 2;
        break;
    case CPM_MIX1:
        ea = TL(0) | TL(1); et = es = TR(2); em[0] = TR(0) | TR(1) | TL(2);
        m[0] = cp_red(OX(0) + 13 + 5, OY(0) + 1, ov, nov); nm = 1;   /* la derecha de a, por su lado */
        break;
    default:                        /* a y b arriba (0 y 1), c abajo (2) */
        ea = TL(2); es = TR(1); et = TR(2);
        em[0] = TR(0) | TL(1); em[1] = TL(0) | TL(2); em[2] = TR(1) | TR(2);
        m[0] = cp_red(OX(0) + 13, OY(0) - 4, ov, nov);        /* el punto medio, por arriba */
        m[1] = cp_red(OX(0) - 13, OY(0) + 5, ov, nov);        /* la izquierda de a con la de c */
        m[2] = cp_red(OX(1) + 13, OY(1) + 5, ov, nov);        /* la derecha de b con la de c */
        nm = 3;
        break;
    }
#undef OX
#undef OY
    ok = nov == cpm_ncap(tipo) && a == ea && t == et && s == es;
    for (j = 0; j < nm; j++) if (m[j] != em[j]) ok = 0;
    if (imprime && (!ok || getenv("HAKARU_DEPURA")))
        printf("    %s: A %x (%x), T %x (%x), sólida %x (%x), medios %x %x %x (%x %x %x)\n", nom, a, ea, t, et, s, es,
               m[0], m[1], m[2], em[0], em[1], em[2]);
    return ok;
}
static int cp_conexiones(int tipo, const char *nom) { ui_dibuja(); return cp_redes(tipo, nom, 1); }

static void cp_pantallas(void)
{
    const char *q = "capacitores: pantallas";
    const CpmRes *r;
    int i, nop;

    /* El Taller: cuatro filas; la 4.ª, «Capacitores equivalentes» */
    ui_init();
    TECLAS(T_ENTER, T_ENTER);
    chk_cond(q, "el Taller tiene la 4.ª fila, con el nombre nuevo", ui_pantalla() == P_TALLER && ve("Capacitores equivalentes") &&
             !ve("Capacitor de motor"));
    repite(T_ABAJO, 5);
    chk_cond(q, "abajo del todo es la 4.ª (y no pasa de ahí)", ui_indice(UI_I_SEL_TALLER) == 3 && ve("Con su Teor" S_ii "a y sus Ejemplos"));
    cp_vuelca("taller_4");
    TECLAS(T_ENTER);
    chk_cond(q, "[enter] en ella: su menú, con Resolver, Teoría y Ejemplos", ui_pantalla() == P_CPM_MENU &&
             ve("Resolver") && ve("Teor" S_ii "a") && ve("Ejemplos") && ui_indice(UI_I_CPM_MSEL) == 0);
    cp_vuelca("menu");
    TECLAS(T_ABAJO, T_ABAJO, T_ABAJO);
    chk_cond(q, "el menú no pasa de la 3.ª", ui_indice(UI_I_CPM_MSEL) == 2);
    TECLAS(T_ESC);
    chk_cond(q, "[esc] del menú: al Taller, en la 4.ª", ui_pantalla() == P_TALLER && ui_indice(UI_I_SEL_TALLER) == 3);
    TECLAS(T_ENTER, T_ENTER);
    chk_cond(q, "Resolver: los datos, con el 17, la bodega A y Auto", ui_pantalla() == P_CPM &&
             !strcmp(ui_cpm_texto(0), "17") && !strcmp(ui_cpm_texto(2), CPM_BODEGA_A) && ui_indice(UI_I_CPM_MODO) == 0 &&
             ve("14 capacitores: 1 2 3 7 9 10 12 28") && ve("Capacitores equivalentes: datos") && ve("Auto"));
    cp_vuelca("datos");
    TECLAS(T_ESC);
    chk_cond(q, "[esc] de los datos: al menú (en Resolver)", ui_pantalla() == P_CPM_MENU && ui_indice(UI_I_CPM_MSEL) == 0);
    TECLAS(T_ESC);
    chk_cond(q, "[esc] [esc]: al Taller, en la 4.ª", ui_pantalla() == P_TALLER && ui_indice(UI_I_SEL_TALLER) == 3);

    /* 17 A: [enter] resuelve; las Opciones; [enter] dibuja */
    cp_entra();
    TECLAS(T_ENTER);
    r = ui_cpm();
    chk_cond(q, "17 A: el resultado en Opciones, con el veredicto arriba", ui_pantalla() == P_CPM_RES && ui_indice(UI_I_CPM_TAB) == 0 &&
             r->tot[2] == 1 && ve("Con 2: 10 " S_PAR " 7 = 17 MF " S_CHECK) && ve("Opciones") && ve("Paso a paso"));
    chk_cond(q, "17 A: la lista por número de capacitores, con su tipo y su diferencia",
             ve("Con 2 capacitores:") && ve("1. paralelo: 10 " S_PAR " 7 = 17 MF (exacto)") && ve("Con 3") &&
             ve("2. paralelo: 9 " S_PAR " 7 " S_PAR " 1 = 17 MF (exacto)"));
    cp_vuelca("res_17");
    TECLAS(T_ABAJO | T_CON_CTRL);
    chk_cond(q, "17 A: [ctrl][ab], la última de la lista", ui_indice(UI_I_CPM_SEL) == ui_cpm()->nval - 1 && ve("9. mixto: "));
    cp_vuelca("res_17_fin");
    TECLAS(T_ARRIBA | T_CON_CTRL);
    TECLAS(T_DER);
    chk_cond(q, "17 A: [der], el paso a paso de la elegida", ui_indice(UI_I_CPM_TAB) == 1 &&
             ve("Opci" S_oo "n 1/") && ve("10 + " S_IQ "7? " S_RARR " s" S_ii " hay 7 " S_CHECK));
    cp_vuelca("res_17_paso");
    repite(T_ABAJO, 30);
    chk_cond(q, "17 A: abajo, el cableado", ve("Derechas unidas " S_RARR " l" S_ii "nea 2 y puente a T."));
    cp_vuelca("res_17_paso_fin");
    TECLAS(T_ENTER);
    chk_cond(q, "17 A: el dibujo del paralelo", ui_pantalla() == P_CPM_DIB && ve("Opci" S_oo "n 1/") && ve("10 " S_PAR " 7 = 17 MF") &&
             ve("7 MF") && ve("10 MF") && ve("Paralelo: cada capacitor toca A y T."));
    chk_cond(q, "17 A: el dibujo del paralelo, sin nada pisado", cp_dibujo_limpio("paralelo"));
    chk_cond(q, "17 A: las conexiones del paralelo (cada uno toca A y T; la s" S_oo "lida, al puente)", cp_conexiones(CPM_PAR, "paralelo"));
    cp_vuelca("dib_par");
    TECLAS(T_ESC);
    chk_cond(q, "[esc] del dibujo: al resultado, en la misma pestaña", ui_pantalla() == P_CPM_RES && ui_indice(UI_I_CPM_TAB) == 1);
    TECLAS(T_ESC);
    chk_cond(q, "[esc] del resultado: a los datos", ui_pantalla() == P_CPM);

    /* 14 B: se escribe encima del valor; [B]; la serie */
    TECLAS(T_DIG(1), T_DIG(4), T_LETRA('B'));
    chk_cond(q, "el valor se escribe encima y [B] pone la otra bodega", !strcmp(ui_cpm_texto(0), "14") &&
             !strcmp(ui_cpm_texto(2), CPM_BODEGA_B) && ve("La bodega B del pizarr" S_oo "n"));
    TECLAS(T_IGUAL);
    r = ui_cpm();
    chk_cond(q, "14 B: [=] también resuelve; 21 y 42 en serie", ui_pantalla() == P_CPM_RES && r->tot[2] == 4 &&
             ve("Con 2: 21 y 42 en serie = 14 MF " S_CHECK));
    chk_cond(q, "14 B: las de tres, las 20 más cercanas de todas", ve("Con 3: las 20 m" S_aa "s cercanas de ") && r->nver[3] == CPM_VER3);
    cp_vuelca("res_14");
    TECLAS(T_TAB);
    chk_cond(q, "14 B: [tab] cambia de pestaña", ui_indice(UI_I_CPM_TAB) == 1 && ve("3. En serie el total queda MENOR"));
    repite(T_ABAJO, 8);
    cp_vuelca("res_14_b");
    TECLAS(T_ENTER);
    chk_cond(q, "14 B: el dibujo de la serie, con el punto medio", ui_pantalla() == P_CPM_DIB && ve("21 MF") && ve("42 MF") &&
             ve("punto medio") && cp_dibujo_limpio("serie"));
    chk_cond(q, "14 B: las conexiones de la serie (el punto medio no toca nada m" S_aa "s)", cp_conexiones(CPM_SER, "serie"));
    cp_vuelca("dib_ser");
    TECLAS(T_DER);
    chk_cond(q, "[der]: la opci" S_oo "n 2 (27 y 29 = 13.98)", ve("Opci" S_oo "n 2/24: 27 y 29 en serie = 13.98 MF") && cp_dibujo_limpio("serie 2") &&
             cp_conexiones(CPM_SER, "serie 2"));
    TECLAS(T_ABAJO, T_ABAJO);
    chk_cond(q, "[ab] en el dibujo también pasa de opción (la 4.ª)", ui_indice(UI_I_CPM_SEL) == 3);
    TECLAS(T_DER);
    chk_cond(q, "...y la 5.ª ya es de tres: el mixto (15 " S_PAR " 27) y 21", ui_indice(UI_I_CPM_SEL) == 4 &&
             ve("Opci" S_oo "n 5/24: (15 " S_PAR " 27) y 21 en serie = 14 MF") && cp_dibujo_limpio("mixto 14") &&
             cp_conexiones(CPM_MIX1, "mixto 14"));
    cp_vuelca("dib_mixto");
    TECLAS(T_DER | T_CON_CTRL);
    chk_cond(q, "[ctrl][der]: la última", ui_indice(UI_I_CPM_SEL) == ui_cpm()->nval - 1);
    TECLAS(T_ESC, T_IZQ);
    chk_cond(q, "[esc] y [izq]: las Opciones, con la elegida a la vista", ui_pantalla() == P_CPM_RES && ui_indice(UI_I_CPM_TAB) == 0 &&
             ve("24. "));
    TECLAS(T_ARRIBA | T_CON_CTRL);
    chk_cond(q, "[ctrl][arr]: la primera, y la lista arriba", ui_indice(UI_I_CPM_SEL) == 0 && ui_indice(UI_I_CPM_LSC) == 0);

    /* Lo que se recuerda: al volver al Taller y entrar otra vez */
    TECLAS(T_ESC, T_ESC, T_ESC, T_ENTER, T_ENTER);
    chk_cond(q, "se recuerda lo último (14 y la B)", ui_pantalla() == P_CPM && !strcmp(ui_cpm_texto(0), "14") &&
             !strcmp(ui_cpm_texto(2), CPM_BODEGA_B));

    /* 12 con la A: uno solo */
    TECLAS(T_DIG(1), T_DIG(2), T_LETRA('A'), T_ENTER);
    chk_cond(q, "12 A: «Con 1» y la lista empieza con el de uno", ve("Con 1: 12 MF solo " S_CHECK) && ve("Con 1 capacitor:") &&
             ve("1. uno solo: 12 MF (exacto)"));
    TECLAS(T_ENTER);
    chk_cond(q, "12 A: el dibujo de uno solo", ui_pantalla() == P_CPM_DIB && ve("12 MF solo") && ve("Sustituci" S_oo "n directa") &&
             cp_dibujo_limpio("uno") && cp_conexiones(CPM_UNO, "uno"));
    cp_vuelca("dib_uno");

    /* 12 con la B: la opción 2 es la del PDF, 20 y 30 */
    TECLAS(T_ESC, T_ESC, T_LETRA('B'), T_ENTER, T_ABAJO);
    chk_cond(q, "12 B: la 2.ª opción, 20 y 30 (la del PDF)", ui_indice(UI_I_CPM_SEL) == 1 && ve("2. serie: 20 y 30 = 12 MF (exacto)"));
    cp_vuelca("res_12");
    TECLAS(T_DER);
    chk_cond(q, "12 B: su paso a paso (el de la opción 2)", ve("Opci" S_oo "n 2/") && strstr(ui_cpm_paso(), "con 20 falta 240/8 = 30 " S_RARR " s" S_ii " hay 30 " S_CHECK));
    TECLAS(T_ENTER);
    chk_cond(q, "12 B: su dibujo", ve("Opci" S_oo "n 2/") && ve("20 y 30 en serie = 12 MF") && ve("20 MF") && ve("30 MF"));
    cp_vuelca("dib_ser_2");

    /* 22.5 ± 0.5, la B sin 21-24, escrita tecla a tecla: con dos no; con tres sí */
    cp_monta_irres();
    chk_cond(q, "22.5: lo escrito", !strcmp(ui_cpm_texto(0), "22.5") && !strcmp(ui_cpm_texto(1), "0.5") &&
             !strcmp(ui_cpm_texto(2), "15-42 sin 21-24") && ve("24 capacitores: 15 16 17 18 19 20 25"));
    cp_vuelca("datos_irres");
    TECLAS(T_ENTER);
    chk_cond(q, "22.5: con dos no se puede (¿el profe lo puso mal?); con tres sí, arriba", ui_pantalla() == P_CPM_RES &&
             ui_cpm()->tot[2] == 0 && ui_cpm()->nver[3] > 0 && ve("Con dos no se puede (" S_IQ "el profe lo puso mal?);") &&
             ve("con tres s" S_ii ": (18 " S_PAR " 42) y 36 en serie = 22.5 MF " S_CHECK) &&
             ve("Capacitores equivalentes: 22.5 " S_PM " 0.5 MF"));
    cp_vuelca("res_tres");
    TECLAS(T_DER);
    cp_vuelca("res_tres_paso");
    for (i = 0; i < 4; i++) {
        char nom[40];
        repite(T_ABAJO | T_CON_SHIFT, 1);
        sprintf(nom, "res_tres_paso_%d", i + 2);
        cp_vuelca(nom);
    }
    chk_cond(q, "22.5: el paso a paso sigue con las faltas para dos", strstr(ui_cpm_paso(), "el 38 con uno de 53 a 58 (p. ej. 55)") &&
             ve("El menor que har" S_ii "a falta: uno de 2 a 3"));
    TECLAS(T_ENTER);
    chk_cond(q, "22.5: el dibujo del mixto de tres", ui_pantalla() == P_CPM_DIB && ve("(18 " S_PAR " 42) y 36") &&
             ve("18 MF") && ve("42 MF") && ve("36 MF") && cp_dibujo_limpio("mixto 22.5") && cp_conexiones(CPM_MIX1, "mixto 22.5"));
    /* irresoluble de verdad: con «Capacitores: 2» */
    TECLAS(T_ESC, T_ESC, T_ABAJO, T_ABAJO, T_DER, T_ENTER);
    chk_cond(q, "22.5 con Capacitores: 2: irresoluble, lo dice arriba con lo más cercano", ui_pantalla() == P_CPM_RES &&
             ui_cpm()->modo == 2 && ui_cpm()->ntot == 0 && ve("IRRESOLUBLE con dos (con tres s" S_ii ": elige Auto).") &&
             ve("Lo m" S_aa "s cercano: 41 y 42 = 20.7 MF (dif. -1.8)") && ve("Lo m" S_aa "s cercano (no entra):") &&
             ve("con 2") && ve("ninguna"));
    cp_vuelca("res_irres");
    TECLAS(T_DER);
    cp_vuelca("res_irres_paso");
    for (i = 0; i < 4; i++) {
        char nom[40];
        repite(T_ABAJO | T_CON_SHIFT, 1);
        sprintf(nom, "res_irres_%d", i + 2);
        cp_vuelca(nom);
    }
    repite(T_ABAJO | T_CON_CTRL, 1);
    chk_cond(q, "22.5 con 2: al final, que con tres sí", ve("Con tres s" S_ii " se puede: "));
    TECLAS(T_ENTER);
    chk_cond(q, "22.5 con 2: el dibujo de lo más cercano, con el aviso", ui_pantalla() == P_CPM_DIB &&
             ve("Lo m" S_aa "s cercano: 41 y 42 en serie = 20.7 MF") && ve("NO ENTRA: lo m" S_aa "s cercano, dif. -1.8 MF.") &&
             cp_dibujo_limpio("cercano") && cp_conexiones(CPM_SER, "cercano"));
    cp_vuelca("dib_cerca");

    /* Los dibujos de tres: paralelo, serie y mixto 2 */
    cp_entra();
    TECLAS(T_BORRAR | T_CON_CTRL, T_DIG(5), T_DIG(5), T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    TECLAS(T_DIG(1), T_DIG(0), T_ESPACIO, T_DIG(1), T_DIG(5), T_ESPACIO, T_DIG(2), T_DIG(0), T_ESPACIO, T_DIG(2), T_DIG(5), T_ENTER, T_ENTER);
    chk_cond(q, "el paralelo de tres (25 + 20 + 10 = 55): apilados, todos a A y a T", ui_pantalla() == P_CPM_DIB &&
             ve("25 " S_PAR " 20 " S_PAR " 10 = 55 MF") && ve("10 MF") && ve("20 MF") && ve("25 MF") &&
             ve("Paralelo de 3: cada uno toca A y T.") && cp_dibujo_limpio("paralelo de 3") && cp_conexiones(CPM_PAR3, "paralelo de 3"));
    cp_vuelca("dib_par3");
    TECLAS(T_ESC, T_ESC, T_ARRIBA, T_ARRIBA, T_DIG(5), T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    TECLAS(T_DIG(1), T_DIG(0), T_ESPACIO, T_DIG(1), T_DIG(5), T_ESPACIO, T_DIG(3), T_DIG(0), T_ENTER, T_ENTER);
    chk_cond(q, "la serie de tres (10, 15 y 30 = 5): dos puntos medios", ui_pantalla() == P_CPM_DIB &&
             ve("10, 15 y 30 en serie = 5 MF") && ve("15 MF") && ve("30 MF") && ve("punto medio") &&
             cp_dibujo_limpio("serie de 3") && cp_conexiones(CPM_SER3, "serie de 3"));
    cp_vuelca("dib_ser3");
    TECLAS(T_ESC, T_ESC, T_ARRIBA, T_ARRIBA, T_DIG(1), T_DIG(7), T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    TECLAS(T_DIG(2), T_DIG(0), T_ESPACIO, T_DIG(3), T_DIG(0), T_ESPACIO, T_DIG(5), T_ENTER, T_ENTER);
    chk_cond(q, "el mixto 2 ((20 y 30 en serie) " S_PAR " 5 = 17): arriba en serie, abajo el 5, unidos por los extremos",
             ui_pantalla() == P_CPM_DIB && ve("(20 y 30 en serie) " S_PAR " 5 = 17 MF") && ve("20 MF") && ve("30 MF") && ve("5 MF") &&
             cp_dibujo_limpio("mixto 2") && cp_conexiones(CPM_MIX2, "mixto 2"));
    cp_vuelca("dib_mix2");

    /* Una lista larga: 30 ± 1 con la bodega del 1 al 99 */
    cp_entra();
    TECLAS(T_DIG(3), T_DIG(0), T_ABAJO, T_DIG(1), T_ABAJO, T_BORRAR | T_CON_CTRL, T_DIG(1), T_MENOS, T_DIG(9), T_DIG(9), T_ENTER);
    r = ui_cpm();
    nop = r->nval;
    printf("    la lista larga (30 " S_PM " 1, del 1 al 99): %d opciones a la vista de %ld\n", nop, r->ntot);
    chk_cond(q, "una lista larga: 3 de uno, 50 de dos (de muchas) y 20 de tres", ui_pantalla() == P_CPM_RES && r->nver[1] == 3 &&
             r->nver[2] == CPM_VER2 && r->tot[2] > CPM_VER2 && r->nver[3] == CPM_VER3 && ve("Con 2: las 50 m" S_aa "s cercanas de "));
    cp_vuelca("res_larga");
    TECLAS(T_ABAJO | T_CON_CTRL);
    chk_cond(q, "[ctrl][ab]: la última, a la vista (abajo del todo)", ui_indice(UI_I_CPM_SEL) == nop - 1 && ui_indice(UI_I_CPM_LSC) > 0 &&
             ve("73. "));
    cp_vuelca("res_larga_fin");
    TECLAS(T_ENTER);
    chk_cond(q, "...y se dibuja", ui_pantalla() == P_CPM_DIB && cp_dibujo_limpio("la última de la lista"));
    TECLAS(T_ESC);
    repite(T_ARRIBA | T_CON_SHIFT, 1);
    chk_cond(q, "[shift][arr]: una página arriba", ui_indice(UI_I_CPM_SEL) < nop - 5);

    /* Valores largos: los rótulos en dos líneas, el número y «MF» */
    cp_entra();
    TECLAS(T_DIG(4), T_DIG(7), T_DIG(0), T_DIG(5), T_DIG(8), T_PUNTO, T_DIG(8), T_ABAJO, T_DIG(1), T_ABAJO,
           T_BORRAR | T_CON_CTRL, T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9), T_PUNTO, T_DIG(9), T_DIG(9), T_DIG(9),
           T_ESPACIO, T_DIG(8), T_DIG(8), T_DIG(8), T_DIG(8), T_DIG(8), T_PUNTO, T_DIG(8), T_DIG(8), T_DIG(8), T_ENTER, T_ENTER);
    i = gfx_desbordes();
    chk_cond(q, "valores largos: la serie, en dos líneas, sin salirse ni pisar nada", ui_pantalla() == P_CPM_DIB &&
             ve("88888.888") && ve("99999.999") && ve("MF") && cp_dibujo_limpio("largos") && cp_conexiones(CPM_SER, "largos") &&
             gfx_desbordes() == i);
    cp_vuelca("dib_largos");
    TECLAS(T_ESC, T_ESC, T_ARRIBA, T_ARRIBA, T_DIG(2), T_DIG(0), T_DIG(0), T_PUNTO, T_DIG(4), T_DIG(4),
           T_ABAJO, T_BORRAR | T_CON_CTRL, T_ABAJO, T_BORRAR | T_CON_CTRL, T_DIG(6), T_DIG(0), T_DIG(1), T_PUNTO, T_DIG(3),
           T_DIG(2), T_ESPACIO, T_DIG(3), T_DIG(0), T_DIG(0), T_PUNTO, T_DIG(6), T_DIG(6), T_ENTER, T_ENTER);
    chk_cond(q, "el del mono: 601.32 a la derecha, en dos líneas, sin salirse", ui_pantalla() == P_CPM_DIB &&
             ve("Opci" S_oo "n 1/1: 300.66 y 601.32 en serie = 200.44 MF") && cp_dibujo_limpio("601.32") &&
             cp_conexiones(CPM_SER, "601.32") && gfx_desbordes() == i);
    cp_vuelca("dib_601");

    /* Los errores: se dicen y el cursor va al campo */
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL, T_DIG(0), T_ENTER);
    chk_cond(q, "un 0 en la bodega: no resuelve, lo dice", ui_pantalla() == P_CPM && ui_indice(UI_I_CPM_CAMPO) == 2 &&
             ve("Bodega: el 0 no vale (todo tiene que ser > 0)."));
    cp_vuelca("datos_error");
    TECLAS(T_ARRIBA, T_ARRIBA, T_BORRAR | T_CON_CTRL, T_ENTER);
    chk_cond(q, "sin valor: lo dice y va al valor", ui_indice(UI_I_CPM_CAMPO) == 0 && ve("Valor: escribe un n" S_uu "mero mayor que 0."));
    TECLAS(T_DIG(1), T_DIG(7), T_ABAJO, T_DIG(2), T_DIG(0), T_ENTER);
    chk_cond(q, "una tolerancia mayor que el valor: lo dice", ui_indice(UI_I_CPM_CAMPO) == 1 && ve("Tolerancia: vac" S_ii "a, o un n" S_uu "mero"));
    TECLAS(T_BORRAR, T_BORRAR, T_ABAJO, T_BORRAR | T_CON_CTRL);
    for (i = 0; i < CPM_TXT + 5; i++) TECLAS(T_DIG(1));
    chk_cond(q, "el campo de la bodega no pasa de 300 y lo dice", strlen(ui_cpm_texto(2)) == CPM_TXT && ve("No caben m" S_aa "s."));
    cp_vuelca("datos_lleno");
    TECLAS(T_BORRAR | T_CON_CTRL, T_ENTER);
    chk_cond(q, "bodega vacía: resuelve y lo dice", ui_pantalla() == P_CPM_RES && ve("IRRESOLUBLE: la bodega est" S_aa " vac" S_ii "a."));
    cp_vuelca("res_vacia");
    TECLAS(T_ENTER);
    chk_cond(q, "...sin nada que dibujar ([enter] no hace nada)", ui_pantalla() == P_CPM_RES);
    TECLAS(T_DER);
    chk_cond(q, "...y su paso a paso dice qué faltaría", ve("La bodega est" S_aa " vac" S_ii "a.") && ve("Directo: uno de 17."));

    /* La Ayuda: su entrada, con el ejemplo */
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_ENTER);
    for (i = 0; i < 600 && !ve("CAPACITORES EQUIVALENTES (TALLER, CUARTA OPCI"); i++) TECLAS(T_ABAJO);
    repite(T_ABAJO, 16);
    chk_cond(q, "la Ayuda tiene su entrada, entera en una pantalla, con el ejemplo del 17", ui_pantalla() == P_AYUDA &&
             ve("CAPACITORES EQUIVALENTES (TALLER, CUARTA OPCI") && ve("10 " S_PAR " 7 = 17 " S_CHECK ".") && !ve("Capacitor de motor"));
    cp_vuelca("ayuda");
}

/* ---- h. La independencia del dibujo y el mono oculto ---- */
static void cp_indep(void)
{
    const char *q = "capacitores: el mono de siempre no lo ve";
    int fin, fin_oculto;
    ce_indep("capacitores: menú", cp_monta_menu, P_CPM_MENU);
    ce_indep("capacitores: datos", cp_monta_datos, P_CPM);
    ce_indep("capacitores: resultado (Opciones)", cp_monta_res, P_CPM_RES);
    ce_indep("capacitores: resultado (Paso a paso)", cp_monta_paso, P_CPM_RES);
    ce_indep("capacitores: dibujo", cp_monta_dib, P_CPM_DIB);
    ce_indep("capacitores: irresoluble", cp_monta_irres, P_CPM);
    ce_indep("capacitores: teoría", cp_monta_teo, P_CPM_TEO);
    ce_indep("capacitores: una página", cp_monta_pag, P_CPM_PAG);
    ce_indep("capacitores: ejemplos", cp_monta_ej, P_CPM_EJ);
    /* oculto (las pruebas de antes): el Taller no pasa de la 3.ª fila (ni con
     * una ráfaga) y la Ayuda acaba donde acababa, en ACERCA DE */
    ui_cpm_oculto(1);
    ui_init();
    TECLAS(T_ENTER, T_ENTER);
    repite(T_ABAJO, 6);
    chk_cond(q, "oculto: el Taller se queda en la 3.ª", ui_indice(UI_I_SEL_TALLER) == 2 && !ui_repite(T_ABAJO | T_REPE, 0));
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_ENTER, T_ABAJO | T_CON_CTRL);
    fin_oculto = ui_indice(UI_I_SCROLL);
    chk_cond(q, "oculto: la Ayuda acaba en ACERCA DE, como antes", ve("Hakaru CES, versi" S_oo "n 4.0, para TI-Nspire CX II") &&
             !ve("CAPACITORES EQUIVALENTES"));
    ui_cpm_oculto(0);
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_ENTER, T_ABAJO | T_CON_CTRL);
    fin = ui_indice(UI_I_SCROLL);
    chk_cond(q, "sin ocultar: 15 líneas más, las de su sección, entera al final",
             fin - fin_oculto == 15 && ve("CAPACITORES EQUIVALENTES (TALLER, CUARTA OPCI") && ve("10 " S_PAR " 7 = 17 " S_CHECK "."));
    ui_init();
    TECLAS(T_ENTER, T_ENTER);
    repite(T_ABAJO, 2);
    chk_cond(q, "sin ocultar: el Taller llega a la 4.ª (y la ráfaga lo sabe)", ui_repite(T_ABAJO | T_REPE, 0) == REP_LISTA);
}

/* ---- i. Los hallazgos de la revisión de rev10 (el cierre), al día ---- */
/* Escribe un texto con las teclas de la calculadora (dígitos, punto,
 * espacio, guion y las letras de «sin») */
static void cp_escribe(const char *s)
{
    for (; *s; s++) {
        int t = *s >= '0' && *s <= '9' ? T_DIG(*s - '0') : *s == '.' ? T_PUNTO : *s == ' ' ? T_ESPACIO :
                *s == '-' ? T_MENOS : *s == 's' ? T_LETRA('S') : *s == 'i' ? T_LETRA('I') : *s == 'n' ? T_LETRA('N') : T_NADA;
        if (t != T_NADA) teclas(&t, 1);
    }
}

/* De los datos: valor, tolerancia y bodega, escritos desde cero; [enter] */
static void cp_escribe_caso(const char *C, const char *tol, const char *bod)
{
    cp_entra();
    TECLAS(T_BORRAR | T_CON_CTRL); cp_escribe(C);
    TECLAS(T_ABAJO, T_BORRAR | T_CON_CTRL); cp_escribe(tol);
    TECLAS(T_ABAJO, T_BORRAR | T_CON_CTRL); cp_escribe(bod);
    TECLAS(T_ENTER);
}

/* El cálculo y los textos: lo exacto y la diferencia, los decimales de lo
 * calculado, los rangos estrechos, el paralelo que sirve sin ser el elegido
 * y el valor que no llega a 0.05 */
static void cp_rev_calculo(void)
{
    const char *q = "capacitores: revisi" S_oo "n, c" S_aa "lculo y textos";
    const CpmRes *r;
    char t[200];
    double x = 0.0;

    /* «exacto» solo si lo es; en el borde del ±0.05, la diferencia de verdad */
    cp_caso(14.05, 0.0, "21 42 60");
    chk_cond(q, "14.05 con 21" S_CDOT "42/63 = 14: dif. -0.05 (no -0.1, m" S_aa "s que el margen)",
             cp_tiene("   CT = 21" S_CDOT "42/(21 + 42) = 882/63 = 14 MF " S_CHECK " (dif. -0.05)\n") &&
             cp_tiene("     con 21 falta 42.5 " S_RARR " hay 42 " S_CHECK "\n"));
    cp_caso(16.95, 0.0, "10 7 40");
    chk_cond(q, "16.95 con 10 + 7: dif. +0.05", cp_tiene("CT = 10 + 7 = 17 MF " S_CHECK " (dif. +0.05)\n"));
    cp_caso(22.45, 0.0, "22.5 60");
    chk_cond(q, "22.45 con un 22.5: uno solo, con su diferencia",
             cp_tiene("1. " S_IQ "Uno solo? Hay de 22.5 (dif. +0.05): sustituci" S_oo "n directa.\n") &&
             cp_tiene("2. Comprobaci" S_oo "n: CT = 22.5 MF " S_CHECK " (dif. +0.05)\n"));
    r = cp_caso(17.0, 0.0, "17.03 10 7");
    chk_cond(q, "17 con un 17.03: el de uno va primero (Auto) y dice «(dif. +0.03)», no «(exacto)»",
             r->val[0].tipo == CPM_UNO && cp_tiene("Hay de 17.03 (dif. +0.03): sustituci" S_oo "n directa.\n"));
    r = cp_caso(12.0, 0.0, CPM_BODEGA_B);
    cpm_dif(r, r->val[0].ct, t, sizeof t);
    chk_cond(q, "12 B: la exacta, 'exacto'", !strcmp(t, "exacto"));
    cpm_titulo(r, &r->val[3], t, sizeof t);
    chk_cond(q, "12 B: la cuarta, 17 y 41 = 12.02 (no '= 12 MF')", !strcmp(t, "17 y 41 en serie = 12.02 MF"));
    cpm_dif(r, r->val[3].ct, t, sizeof t);
    chk_cond(q, "12 B: ...y su diferencia, dif. +0.02 (no 'exacto')", !strcmp(t, "dif. +0.02"));
    cpm_titulo(r, &r->val[4], t, sizeof t);
    chk_cond(q, "12 B: la quinta, 23 y 25 = 11.98", !strcmp(t, "23 y 25 en serie = 11.98 MF"));
    cp_entra();
    TECLAS(T_DIG(1), T_DIG(2), T_LETRA('B'), T_ENTER);
    chk_cond(q, "12 B: la lista dice 'exacto' solo de las exactas (y la 4.ª y la 5.ª, con los decimales que las distinguen)",
             ui_pantalla() == P_CPM_RES && ve("3. serie: 21 y 28 = 12 MF (exacto)") && ve("4. serie: 17 y 41 = 12.02 MF (dif. +0.017)") &&
             ve("5. serie: 23 y 25 = 11.98 MF (dif. -0.021)"));
    cp_vuelca("res_12_fin");
    {   /* la diferencia: los decimales que la distinguen de 0, no menos que los de C */
        static CpmRes rd;
        static const double V[2] = { 10.0, 30.0 };
        cpm_resuelve(&rd, 1500.28, 0.0, V, 2);
        cpm_dif(&rd, 1500.28125, t, sizeof t);
        chk_cond(q, "la diferencia con C = 1500.28: dif. +0.001 (con dos decimales ser" S_ii "a +0)", !strcmp(t, "dif. +0.001"));
        cpm_dif(&rd, 1501.7712, t, sizeof t);
        chk_cond(q, "...y 1501.7712 - 1500.28: dif. +1.49 (los decimales de C, no +1.5)", !strcmp(t, "dif. +1.49"));
        cpm_dif(&rd, 1502.0, t, sizeof t);
        chk_cond(q, "...y 1502 - 1500.28, corta: dif. +1.72", !strcmp(t, "dif. +1.72"));
    }

    /* lo calculado: como lo pedido si lo es; C2, el de la bodega */
    r = cp_caso(2.25, 0.0, "1 1.25 3 9");
    cpm_titulo(r, &r->val[0], t, sizeof t);
    chk_cond(q, "2.25 = 1.25 + 1: la comprobaci" S_oo "n dice 2.25 (no 2.3)", cp_tiene("CT = 1.25 + 1 = 2.25 MF " S_CHECK "\n"));
    chk_cond(q, "...y el t" S_ii "tulo: 1.25 " S_PAR " 1 = 2.25 MF", !strcmp(t, "1.25 " S_PAR " 1 = 2.25 MF"));
    cpm_titulo(r, &r->val[1], t, sizeof t);
    chk_cond(q, "...y la serie, 3 y 9 = 2.25", !strcmp(t, "3 y 9 en serie = 2.25 MF"));
    cp_caso(17.05, 0.0, "10 7.05 40");
    chk_cond(q, "17.05 con 10 y 7.05: '¿7.05? -> sí hay 7.05', no '7.1'",
             cp_tiene("     10 + " S_IQ "7.05? " S_RARR " s" S_ii " hay 7.05 " S_CHECK " (C" S_SUB2 " = 17.05 - 10 = 7.05)\n"));
    chk_cond(q, "...y la comprobaci" S_oo "n, 17.05", cp_tiene("CT = 10 + 7.05 = 17.05 MF " S_CHECK "\n"));
    cp_caso_n(0.06, 0.0, 2, "0.01 0.02 0.03 0.05");
    chk_cond(q, "0.06 = 0.05 + 0.01 (con 2: en Auto va antes el 0.05 solo): ni '0' ni '= 0.1 MF'",
             cp_tiene("     0.05 + " S_IQ "0.01? " S_RARR " s" S_ii " hay 0.01 " S_CHECK) &&
             cp_tiene("CT = 0.05 + 0.01 = 0.06 MF " S_CHECK "\n"));
    cp_caso(12.0, 0.0, "17 41");
    chk_cond(q, "12 con 17 y 41: la comprobaci" S_oo "n dice 12.02 y su diferencia (no 12 sin ella)",
             cp_tiene("   CT = 17" S_CDOT "41/(17 + 41) = 697/58 = 12.02 MF " S_CHECK " (dif. +0.02)\n"));
    cp_caso(12015.6, 0.0, "21878.377 26653.936");
    chk_cond(q, "12015.6: el C" S_SUB2 " de la bodega no da justo lo pedido: 'hay', no 's" S_ii "', y la diferencia",
             cp_tiene("     con 21878.377 falta 26653.9 " S_RARR " hay 26653.936 " S_CHECK "\n") &&
             cp_tiene(" = 12015.60001 MF " S_CHECK " (dif. +0.00001)\n"));
    cp_caso(17.05, 0.0, "10 7 40");
    chk_cond(q, "17.05 con 10 y 7: ¿7.05? (no 7.1) -> hay 7",
             cp_tiene("     10 + " S_IQ "7.05? " S_RARR " hay 7 " S_CHECK "\n"));
    cp_caso(17.05, 0.0, "1.5 11 12 13 14 15 16");
    chk_cond(q, "17.05: los intentos resumidos, con sus decimales (de 1.05 a 5.05, no de 1.1 a 5.1)",
             cp_tiene("     del 16 al 12: faltar" S_ii "a de 1.05 a 5.05; no hay\n") &&
             cp_tiene("     11 + " S_IQ "6.05? " S_RARR " no hay 6.05\n"));
    cp_caso(20.0, 0.0, "12 8.004 30 60");
    chk_cond(q, "20 con 12 y 8.004: el que sirve sin ser el elegido, con su CT de verdad (20.004, no 20)",
             cp_tiene(S_RARR " hay 8.004: sirve (CT 20.004)\n"));
    cp_caso(1.875, 0.0, "2.5 7.5");
    chk_cond(q, "1.875 = 2.5 y 7.5 en serie: 1.875, no 1.9", cp_tiene("CT = 2.5" S_CDOT "7.5/(2.5 + 7.5) = 1.875 MF " S_CHECK "\n"));

    /* los rangos estrechos: con más decimales, nunca «de 17 a 17» */
    cp_caso(17.0, 0.01, "10 30");
    chk_cond(q, "17 " S_PM " 0.01: los enteros que sirven, de uno (uno de 17, el 10 con uno de 7)",
             cp_tiene("   Directo: uno de 17.\n") && cp_tiene("     el 10 con uno de 7\n") && !cp_tiene("de 17 a 17") &&
             !cp_tiene("de 7 a 7"));
    chk_cond(q, "...y sin entero, con dos decimales: el 30 con uno de 39.18 a 39.28 (no 'de 39.2 a 39.2')",
             cp_tiene("     el 30 con uno de 39.18 a 39.28\n"));
    cp_caso(17.0, 0.01, "10 7.5 9 30");
    chk_cond(q, "17 " S_PM " 0.01: el intento, ¿6.99 a 7.01? (no '7 a 7')",
             cp_tiene("     10 + " S_IQ "6.99 a 7.01? " S_RARR " no hay\n"));
    cp_caso(1.0, 0.001, "1001-1100");
    chk_cond(q, "1 " S_PM " 0.001: uno de 1, no 'de 1 a 1'", cp_tiene("   Directo: uno de 1.\n") && !cp_tiene("de 1 a 1"));
    cp_caso(2.5, 0.1, "10 20");
    chk_cond(q, "2.5 " S_PM " 0.1: sin entero, el rango con decimales (uno de 2.4 a 2.6)", cp_tiene("   Directo: uno de 2.4 a 2.6.\n"));

    /* un paralelo que sirve y no es el elegido: se dice por qué se sigue */
    r = cp_caso(20.0, 0.0, "12 8.04 30 60");
    chk_cond(q, "20: el paralelo sirve (20.04) pero la serie es exacta: lo dice antes de la serie, con las dos diferencias",
             r->tot[2] == 2 && r->val[0].tipo == CPM_SER &&
             cp_tiene("     12 + " S_IQ "8? " S_RARR " hay 8.04: sirve (CT 20.04)\n"
                      "   Sirve (dif. +0.04), pero en serie queda m" S_aa "s cerca (exacto).\n3. En serie"));
    cp_caso(17.0, 0.0, CPM_BODEGA_A);
    chk_cond(q, "...y no lo dice cuando la elegida es el paralelo", !cp_tiene("Sirve, pero"));

    /* el valor: más que 0.05, con su aviso */
    chk_cond(q, "el valor 0.05 no (C - 0.05 = 0), con su aviso; 0.051 s" S_ii "; 0, el de siempre",
             cpm_lee_valor("0.05", &x) == CPM_E_CHICO && cpm_lee_valor("0.051", &x) == CPM_OK &&
             cpm_lee_valor("0", &x) == CPM_E_VALOR && !strcmp(cpm_error(CPM_E_CHICO), "Valor: tiene que ser mayor que 0.05 MF."));
    cp_entra();
    TECLAS(T_DIG(0), T_PUNTO, T_DIG(0), T_DIG(5), T_ENTER);
    chk_cond(q, "0.05 y [enter]: no resuelve y dice por qu" S_ee, ui_pantalla() == P_CPM &&
             ve("Valor: tiene que ser mayor que 0.05 MF.") && !ve("Valor: escribe un n" S_uu "mero mayor que 0."));
    cp_vuelca("valor_005");
}

/* Las bandas de filas (entre y0 e y1) con algún píxel de color c: cuántas
 * hay; ini/alto de las primeras max */
static int cp_bandas(u16 c, int y0, int y1, int *ini, int *alto, int max)
{
    const u16 *fb = gfx_buffer();
    int y, x, n = 0, en = 0;
    for (y = y0; y < y1; y++) {
        int hay = 0;
        for (x = 0; x < SCR_W && !hay; x++) if (fb[y * SCR_W + x] == c) hay = 1;
        if (hay) {
            if (!en) { if (n < max) { ini[n] = y; alto[n] = 0; } n++; en = 1; }
            if (n <= max) alto[n - 1]++;
        } else en = 0;
    }
    return n;
}

/* Un dibujo suelto (sin la UI): las cajas dentro, los rótulos sin pisar ni
 * montarse, y las redes como deben. 0 bien; si no, la suma de 1 (fuera), 2
 * (un rótulo pisa), 4 (cajas que se montan) y 8 (las conexiones) */
static int cp_dibujo_suelto(const CpmRes *r, const CpmComb *k, int no_entra)
{
    CpmCaja c[16];
    int n, i, j, mal = 0;
    gfx_clear(C_FONDO);
    cpm_dibuja(r, k, 17, 227, no_entra);
    n = cpm_cajas(c, 16);
    for (i = 0; i < n; i++) {
        if (c[i].x < 0 || c[i].y < 16 || c[i].x + c[i].w > SCR_W || c[i].y + c[i].h > 227) mal |= 1;
        if (c[i].ocupados > 0) mal |= 2;
        for (j = i + 1; j < n; j++)
            if (c[i].x < c[j].x + c[j].w && c[j].x < c[i].x + c[i].w && c[i].y < c[j].y + c[j].h && c[j].y < c[i].y + c[i].h)
                mal |= 4;
    }
    if (!cp_redes(k->tipo, "suelto", 0)) mal |= 8;
    return mal;
}

/* Un valor de bodega «de examen» o casi: enteros, medios, cuartos y de tres
 * cifras */
static double cp_valor_azar(void)
{
    unsigned u = azar(100);
    if (u < 50) return (double)(1 + azar(100));
    if (u < 75) return 0.5 + (double)azar(100);
    if (u < 90) return 0.25 * (double)(1 + 2 * azar(400));      /* x.25 y x.75, hasta 200 */
    return (double)(100 + azar(900));
}

/* El dibujo al azar: 6000 bodegas (con repetidos), en los tres modos; cada
 * opción que se dibuja (la primera, la segunda, la última y la primera de
 * cada número de capacitores; en el irresoluble, lo más cercano) */
static void cp_barrido_dibujo(void)
{
    const char *q = "capacitores: revisi" S_oo "n, el dibujo al azar";
    static double v[40];
    static CpmRes r;
    static int cuenta[CPM_NT][16];
    int caso, i, malos = 0, ejemplos = 0, c;
    unsigned s0 = semilla;
    semilla = 20261007u;
    memset(cuenta, 0, sizeof cuenta);
    for (caso = 0; caso < 6000; caso++) {
        int n = 3 + (int)azar(18), nop, op[12], nk = 0, modo = (int)azar(3);
        double C, tol = azar(3) == 0 ? 0.5 : 0.0;
        modo = modo == 0 ? 0 : modo + 1;
        for (i = 0; i < n; i++) v[i] = i > 0 && azar(6) == 0 ? v[azar((unsigned)i)] : cp_valor_azar();
        if (azar(4) == 0) C = v[azar((unsigned)n)] + v[azar((unsigned)n)] + (azar(2) ? v[azar((unsigned)n)] : 0.0);
        else if (azar(2)) {
            double a = v[azar((unsigned)n)], b = v[azar((unsigned)n)];
            C = a * b / (a + b); C = (double)(long)(C * 10.0 + 0.5) / 10.0;
        } else C = 0.5 * (double)(2 + azar(120));
        if (!(C > 0.06) || tol >= C) continue;
        cpm_resuelve_n(&r, C, tol, modo, v, n);
        nop = r.nval > 0 ? r.nval : r.hay_cerca;
        if (r.nval > 0) {
            op[nk++] = 0;
            if (nop > 1) op[nk++] = 1;
            if (nop > 2) op[nk++] = nop - 1;
            for (c = 1, i = 0; c <= 3; c++) { if (r.nver[c] > 0 && i > 0 && nk < 12) op[nk++] = i; i += r.nver[c]; }
        } else for (i = 0; i < nop; i++) op[nk++] = i;
        for (i = 0; i < nk; i++) {
            const CpmComb *k = r.nval > 0 ? &r.val[op[i]] : &r.cerca;
            int mal = cp_dibujo_suelto(&r, k, r.nval == 0);
            cuenta[k->tipo][mal]++;
            if (mal) {
                malos++;
                if (ejemplos++ < 8) {
                    char t[96];
                    cpm_titulo(&r, k, t, sizeof t);
                    printf("    dibujo mal %d (tipo %d): C %g tol %g, %s\n", mal, k->tipo, C, tol, t);
                }
            }
        }
    }
    semilla = s0;
    printf("    6000 bodegas al azar: dibujos de uno %d, paralelo %d, serie %d, paralelo de 3 %d, serie de 3 %d, mixto 1 %d, "
           "mixto 2 %d; malos %d\n", cuenta[0][0], cuenta[1][0], cuenta[2][0], cuenta[3][0], cuenta[4][0], cuenta[5][0],
           cuenta[6][0], malos);
    chk_cond(q, "6000 bodegas (enteros, medios, cuartos, tres cifras, repetidos): ning" S_uu "n dibujo malo (r" S_oo "tulos, cajas, conexiones)",
             malos == 0 && cuenta[CPM_PAR3][0] > 50 && cuenta[CPM_SER3][0] > 50 && cuenta[CPM_MIX1][0] > 100 && cuenta[CPM_MIX2][0] > 100);
}

/* Las pantallas: [esc] del resultado, el error de la bodega una vez, el
 * mixto con valores largos, la lectura en vivo y la diferencia del dibujo */
static void cp_rev_pantallas(void)
{
    const char *q = "capacitores: revisi" S_oo "n, pantallas";
    int ini[8], alto[8], n;
    const CpmRes *r;

    /* [esc] del resultado: el siguiente ejercicio escribe encima del valor */
    cp_entra();
    TECLAS(T_DIG(1), T_DIG(4), T_LETRA('B'), T_ENTER, T_ESC);
    chk_cond(q, "[esc] del resultado: los datos, en el valor, resaltado (la tecla escribe encima)", ui_pantalla() == P_CPM &&
             ui_indice(UI_I_CPM_CAMPO) == 0 && ui_indice(UI_I_CPM_NUEVO) == 1 && ve("14"));
    TECLAS(T_DIG(1), T_DIG(2));
    chk_cond(q, "...y la primera tecla sustituye: 12, no 1412", !strcmp(ui_cpm_texto(0), "12") && ui_indice(UI_I_CPM_POS) == 2);
    TECLAS(T_ENTER);
    chk_cond(q, "...12 con la B resuelve el ejercicio siguiente", ui_pantalla() == P_CPM_RES && ui_cpm()->tot[2] == 5);
    TECLAS(T_ESC, T_ABAJO, T_ABAJO, T_ESPACIO, T_DIG(7), T_ENTER);
    chk_cond(q, "...con [enter] desde la bodega tambi" S_ee "n resuelve", ui_pantalla() == P_CPM_RES && ui_cpm()->ncap == 29);
    TECLAS(T_ESC, T_DIG(1));
    chk_cond(q, "...y tras [esc], en la bodega se inserta en el cursor (al final)", ui_pantalla() == P_CPM &&
             !strcmp(ui_cpm_texto(2), "15-42 71"));

    /* un error de la bodega se dice una vez */
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL, T_DIG(0), T_ENTER);
    ui_dibuja();
    n = cp_bandas(C_ERROR, 17, 227, ini, alto, 8);
    chk_cond(q, "un 0 en la bodega: una l" S_ii "nea roja, y la ayuda de abajo", n == 1 &&
             ve("Bodega: el 0 no vale (todo tiene que ser > 0).") && ve("Rangos: 15-42. Quitar: -21-24 o sin 21-24."));
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL, T_DIG(1), T_DIG(5), T_MENOS, T_ENTER);
    ui_dibuja();
    n = cp_bandas(C_ERROR, 17, 227, ini, alto, 8);
    chk_cond(q, "'15-' en la bodega: el error, una vez", n == 1 && ve("Bodega: hay algo que no es un n" S_uu "mero."));
    cp_vuelca("datos_error_2");
    cp_entra();
    TECLAS(T_BORRAR | T_CON_CTRL, T_ENTER);
    ui_dibuja();
    n = cp_bandas(C_ERROR, 17, 227, ini, alto, 8);
    chk_cond(q, "...y un error del valor s" S_ii " sale, una vez", n == 1 && ve("Valor: escribe un n" S_uu "mero mayor que 0."));

    /* el mixto con rótulos largos: el de arriba, encima de su óvalo */
    cp_escribe_caso("150.37", "", "120.25 180.75 300.5");
    r = ui_cpm();
    chk_cond(q, "150.37 con 120.25 180.75 300.5: irresoluble con dos, con tres sí", ui_pantalla() == P_CPM_RES &&
             r->tot[2] == 0 && r->nval > 0 && cp_esta3(r, CPM_MIX1, 120.25, 180.75, 300.5) >= 0);
    while (ui_cpm()->val[ui_indice(UI_I_CPM_SEL)].tipo != CPM_MIX1 && ui_indice(UI_I_CPM_SEL) < ui_cpm()->nval - 1) TECLAS(T_ABAJO);
    TECLAS(T_ENTER);
    chk_cond(q, "el mixto con r" S_oo "tulos de 9 columnas: nada pisado, bien conectado y el CT en la barra",
             ui_pantalla() == P_CPM_DIB && cp_dibujo_limpio("mixto 9") && cp_conexiones(CPM_MIX1, "mixto 9") &&
             ve("(120.25 " S_PAR " 180.75) y 300.5 = 150.375 MF") && ve("120.25 MF") && ve("180.75 MF"));
    cp_vuelca("dib_mixto_9");
    cp_escribe_caso("1500.28", "", "1000.125 2000.375 3000.625");
    while (ui_cpm()->val[ui_indice(UI_I_CPM_SEL)].tipo != CPM_MIX1 && ui_indice(UI_I_CPM_SEL) < ui_cpm()->nval - 1) TECLAS(T_ABAJO);
    TECLAS(T_ENTER);
    chk_cond(q, "el mixto con r" S_oo "tulos de 11 columnas: nada pisado y el CT en la barra (sin 'en serie' ni 'Opción')",
             ui_pantalla() == P_CPM_DIB && cp_dibujo_limpio("mixto 11") && cp_conexiones(CPM_MIX1, "mixto 11") &&
             ve("(1000.125 " S_PAR " 2000.375) y 3000.625 = 1500.281 MF"));
    cp_vuelca("dib_mixto_11");
    cp_escribe_caso("47058.8", "", "99999.999 88888.888");
    TECLAS(T_ENTER);
    chk_cond(q, "la serie larga: la barra sin 'en serie', con el CT", ui_pantalla() == P_CPM_DIB &&
             ve("Opci" S_oo "n 1/1: 88888.888 y 99999.999 = 47058.82 MF"));

    /* un CT a 3e-9 de lo pedido (nueve decimales): la mejor, arriba, sin «en
     * serie» para caber; el dibujo, sin «Opción 1/1:» */
    n = gfx_desbordes();
    cp_escribe_caso("10000", "", "11111.1111234605 99999.999");
    chk_cond(q, "un CT de nueve decimales: el veredicto cabe arriba (sin 'en serie') y nada se sale", ui_pantalla() == P_CPM_RES &&
             ve("Con 2: 11111.111 y 99999.999 = 10000.000000003 MF " S_CHECK) && gfx_desbordes() == n);
    TECLAS(T_ENTER);
    chk_cond(q, "...y la barra del dibujo, con el CT", ui_pantalla() == P_CPM_DIB &&
             ve("11111.111 y 99999.999 = 10000.000000003 MF") && !ve("Opci" S_oo "n 1/1") && gfx_desbordes() == n);

    /* la lectura en vivo: los que caben en dos líneas, con «...» si faltan */
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    cp_escribe("100.125 200.375 300.625 400.875 500.125 600.375 700.625 800.875 900.125 1000.375 1100.625 1200.875 1300.125");
    chk_cond(q, "lectura en vivo: '13 capacitores' acaba en '...' (no se pierde en una tercera l" S_ii "nea)",
             ve("13 capacitores: 100.125 200.375 300.625 400.875") && ve(" ..."));
    cp_vuelca("datos_dec");
    TECLAS(T_BORRAR | T_CON_CTRL);
    cp_escribe("1-100");
    chk_cond(q, "...y con 1-100", ve("100 capacitores: 1 2 3 4 5 6 7 8 9 10") && ve("..."));
    TECLAS(T_BORRAR | T_CON_CTRL);
    cp_escribe("1 2 3 4 5 6 7 10 12 7");
    chk_cond(q, "...y con los repetidos, sus cantidades (7" S_TIMES "2)", ve("10 capacitores: 1 2 3 4 5 6 7" S_TIMES "2 10 12"));
    cp_vuelca("datos_repetidos");

    /* el dibujo de lo más cercano: la diferencia con sus decimales */
    cp_escribe_caso("17", "", "10 7.06");
    TECLAS(T_ENTER);
    chk_cond(q, "lo m" S_aa "s cercano a 0.06: 'dif. +0.06' en el dibujo (no +0.1)", ui_pantalla() == P_CPM_DIB &&
             ve("NO ENTRA: lo m" S_aa "s cercano, dif. +0.06 MF."));
    cp_barrido_dibujo();
}

/* ---- j. La edición con cursor (§3.1) ---- */
static void cp_cursor(void)
{
    const char *q = "capacitores: la edición con cursor";
    int i;

    /* el pedido del usuario: una lista larga y el primer valor mal */
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    cp_escribe("22 23 24 21 24 22 29 43 24");
    chk_cond(q, "la lista escrita, con el cursor al final", !strcmp(ui_cpm_texto(2), "22 23 24 21 24 22 29 43 24") &&
             ui_indice(UI_I_CPM_POS) == 26);
    TECLAS(T_IZQ | T_CON_CTRL);
    chk_cond(q, "[ctrl][izq]: al principio", ui_indice(UI_I_CPM_POS) == 0);
    cp_vuelca("datos_cursor_0");
    TECLAS(T_DER, T_DER, T_BORRAR, T_BORRAR, T_DIG(1), T_DIG(9));
    chk_cond(q, "el primer 22 pasa a 19 sin borrar la lista: [der][der] [del][del] [1][9]",
             !strcmp(ui_cpm_texto(2), "19 23 24 21 24 22 29 43 24") && ui_indice(UI_I_CPM_POS) == 2);
    cp_vuelca("datos_cursor");
    TECLAS(T_DER | T_CON_SHIFT, T_DER | T_CON_SHIFT);
    chk_cond(q, "[shift][der]: de valor en valor (al 24, el tercero)", ui_indice(UI_I_CPM_POS) == 6);
    cp_escribe("5 ");
    chk_cond(q, "lo que se escribe se inserta en el cursor (un 5 antes del 24)", !strcmp(ui_cpm_texto(2), "19 23 5 24 21 24 22 29 43 24") &&
             ui_indice(UI_I_CPM_POS) == 8);
    TECLAS(T_IZQ | T_CON_SHIFT);
    chk_cond(q, "[shift][izq]: al principio del valor de antes (el 5)", ui_indice(UI_I_CPM_POS) == 6);
    TECLAS(T_DER);
    TECLAS(T_BORRAR, T_BORRAR);
    chk_cond(q, "[del] borra a la izquierda del cursor (el 5 y el espacio de antes)", !strcmp(ui_cpm_texto(2), "19 23 24 21 24 22 29 43 24") &&
             ui_indice(UI_I_CPM_POS) == 5);
    TECLAS(T_DER | T_CON_CTRL);
    chk_cond(q, "[ctrl][der]: al final", ui_indice(UI_I_CPM_POS) == 26);
    TECLAS(T_DER);
    chk_cond(q, "...y de ahí no pasa", ui_indice(UI_I_CPM_POS) == 26 && !ui_repite(T_DER | T_REPE, 0));
    TECLAS(T_ENTER);
    chk_cond(q, "y se resuelve con la lista corregida (19, no 22)", ui_pantalla() == P_CPM_RES && ui_cpm()->v[0] == 19.0 &&
             ui_cpm()->ncap == 9);

    /* el valor: la primera tecla escribe encima; con el cursor, se inserta */
    cp_entra();
    chk_cond(q, "al entrar, el valor resaltado (sin cursor): la tecla escribe encima", ui_indice(UI_I_CPM_NUEVO) == 1);
    TECLAS(T_IZQ);
    chk_cond(q, "[izq] enseña el cursor (antes del 7) y ya no escribe encima", ui_indice(UI_I_CPM_NUEVO) == 0 && ui_indice(UI_I_CPM_POS) == 1);
    TECLAS(T_PUNTO);
    chk_cond(q, "...se inserta: 1.7", !strcmp(ui_cpm_texto(0), "1.7"));
    cp_vuelca("datos_valor_cursor");
    TECLAS(T_BORRAR, T_DIG(4));
    chk_cond(q, "[del] y [4]: 147", !strcmp(ui_cpm_texto(0), "147"));
    TECLAS(T_ABAJO, T_ARRIBA);
    chk_cond(q, "al volver al valor, otra vez resaltado", ui_indice(UI_I_CPM_NUEVO) == 1 && ui_indice(UI_I_CPM_POS) == 3);
    TECLAS(T_BORRAR);
    chk_cond(q, "[del] resaltado: borra el último (como antes)", !strcmp(ui_cpm_texto(0), "14"));
    TECLAS(T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9), T_DIG(9));
    chk_cond(q, "el valor no pasa de 12 caracteres y lo dice", strlen(ui_cpm_texto(0)) == 12 && ve("No caben m" S_aa "s."));

    /* las líneas de la caja de la bodega: [arr][ab] de línea en línea */
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    cp_escribe("100.125 200.375 300.625 400.875 500.125 600.375 700.625 800.875 900.125 1000.375 1100.625 1200.875 1300.125");
    i = ui_indice(UI_I_CPM_POS);
    chk_cond(q, "una bodega de tres líneas: el cursor al final, en la 3.ª", i == (int)strlen(ui_cpm_texto(2)));
    TECLAS(T_ARRIBA);
    chk_cond(q, "[arr]: a la línea de arriba, sin salir del campo", ui_indice(UI_I_CPM_CAMPO) == 2 && ui_indice(UI_I_CPM_POS) < i - 30);
    TECLAS(T_ARRIBA);
    chk_cond(q, "[arr]: a la primera", ui_indice(UI_I_CPM_CAMPO) == 2 && ui_indice(UI_I_CPM_POS) < 50);
    i = ui_indice(UI_I_CPM_POS);
    TECLAS(T_ARRIBA);
    chk_cond(q, "[arr] desde la primera: a la tolerancia", ui_indice(UI_I_CPM_CAMPO) == 1);
    TECLAS(T_ABAJO);
    chk_cond(q, "[ab]: de vuelta a la bodega, donde estaba (la primera línea; el cierre de la v3, REV-1)",
             ui_indice(UI_I_CPM_CAMPO) == 2 && ui_indice(UI_I_CPM_POS) == i);
    TECLAS(T_DER | T_CON_CTRL, T_ABAJO);
    chk_cond(q, "[ctrl][der] (al final, la última línea) y [ab]: a «Capacitores»", ui_indice(UI_I_CPM_CAMPO) == 3);
    cp_vuelca("datos_ncap");
    TECLAS(T_DER, T_DER, T_DER);
    chk_cond(q, "«Capacitores»: [der] Auto -> 2 -> 3 (sin vuelta)", ui_indice(UI_I_CPM_MODO) == 3 && !ui_repite(T_DER | T_REPE, 0));
    TECLAS(T_IZQ, T_DIG(0));
    chk_cond(q, "...[izq] a 2 y [0] a Auto", ui_indice(UI_I_CPM_MODO) == 0);
    TECLAS(T_DIG(3));
    chk_cond(q, "...[3]: 3", ui_indice(UI_I_CPM_MODO) == 3 && ve("solo de tres"));
    TECLAS(T_TAB);
    chk_cond(q, "[tab] desde «Capacitores»: al valor (con vuelta)", ui_indice(UI_I_CPM_CAMPO) == 0);

    /* una bodega de siete líneas: la caja se desplaza para enseñar el cursor */
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    for (i = 0; i < 60; i++) cp_escribe(i % 2 ? "7.25 " : "12.5 ");
    chk_cond(q, "una bodega de seis líneas (300 caracteres): la caja enseña las cuatro últimas",
             strlen(ui_cpm_texto(2)) == CPM_TXT && ui_indice(UI_I_CPM_BTOP) == 2);
    TECLAS(T_IZQ | T_CON_CTRL);
    chk_cond(q, "[ctrl][izq]: el principio, y la caja sube", ui_indice(UI_I_CPM_POS) == 0 && ui_indice(UI_I_CPM_BTOP) == 0);
    cp_vuelca("datos_caja");
    repite(T_ABAJO, 3);
    chk_cond(q, "[ab] tres veces: la 4.ª línea, aún a la vista sin desplazar", ui_indice(UI_I_CPM_BTOP) == 0 && ui_indice(UI_I_CPM_CAMPO) == 2);
    TECLAS(T_ABAJO);
    chk_cond(q, "...y una más: la caja baja una", ui_indice(UI_I_CPM_BTOP) == 1);
    TECLAS(T_LETRA('A'));
    chk_cond(q, "[A] en la bodega: la del pizarrón, con el cursor al final", !strcmp(ui_cpm_texto(2), CPM_BODEGA_A) &&
             ui_indice(UI_I_CPM_POS) == (int)strlen(CPM_BODEGA_A) && ui_indice(UI_I_CPM_BTOP) == 0);
}

/* ---- k. Las pestañas y las opciones (§3.2) ---- */
static void cp_pestanas(void)
{
    const char *q = "capacitores: las pestañas y las opciones";
    cp_entra();
    TECLAS(T_DIG(1), T_DIG(4), T_LETRA('B'), T_ENTER);
    chk_cond(q, "se abre en Opciones, con la 1.ª elegida", ui_indice(UI_I_CPM_TAB) == 0 && ui_indice(UI_I_CPM_SEL) == 0);
    TECLAS(T_ABAJO);
    chk_cond(q, "[ab] elige la 2.ª en la lista", ui_indice(UI_I_CPM_SEL) == 1 && ui_indice(UI_I_CPM_TAB) == 0);
    chk_cond(q, "...y el paso a paso ya es el de ella (27 y 29)", strstr(ui_cpm_paso(), "con 27 falta ") != 0);
    TECLAS(T_IZQ);
    chk_cond(q, "[izq] en Opciones: se queda", ui_indice(UI_I_CPM_TAB) == 0 && !ui_repite(T_IZQ | T_REPE, 0));
    TECLAS(T_DER);
    chk_cond(q, "[der]: Paso a paso, de la opción 2", ui_indice(UI_I_CPM_TAB) == 1 && ve("Opci" S_oo "n 2/24: 27 y 29 en serie"));
    TECLAS(T_ABAJO);
    chk_cond(q, "en Paso a paso, [ab] desplaza (no cambia de opción)", ui_indice(UI_I_SCROLL) == 1 && ui_indice(UI_I_CPM_SEL) == 1);
    chk_cond(q, "[der] no se repite (una pestaña por pulsación)", !ui_repite(T_DER | T_REPE, 0) && !ui_repite(T_IZQ | T_REPE, 0));
    TECLAS(T_TAB);
    chk_cond(q, "[tab]: de vuelta a Opciones", ui_indice(UI_I_CPM_TAB) == 0);
    chk_cond(q, "[ab] mantenida en la lista se repite (la ráfaga)", ui_repite(T_ABAJO | T_REPE, 0) == REP_LISTA);
    repite(T_ABAJO, 30);
    chk_cond(q, "una ráfaga llega a la última y se para", ui_indice(UI_I_CPM_SEL) == ui_cpm()->nval - 1 && !ui_repite(T_ABAJO | T_REPE, 0));
    TECLAS(T_ARRIBA | T_CON_SHIFT);
    chk_cond(q, "[shift][arr]: una página", ui_indice(UI_I_CPM_SEL) == ui_cpm()->nval - 1 - (227 - (16 + 22 + 13 + 4)) / 13);
    TECLAS(T_ENTER);
    chk_cond(q, "[enter] en Opciones: el dibujo de la elegida", ui_pantalla() == P_CPM_DIB);
    TECLAS(T_IZQ);
    chk_cond(q, "...[izq] en el dibujo: la de antes", ui_indice(UI_I_CPM_SEL) == ui_cpm()->nval - 2 - (227 - (16 + 22 + 13 + 4)) / 13);
    TECLAS(T_ESC);
    chk_cond(q, "[esc]: Opciones, con la elegida a la vista", ui_pantalla() == P_CPM_RES && ui_indice(UI_I_CPM_TAB) == 0);
    cp_vuelca("res_opciones_medio");
    TECLAS(T_DER, T_ENTER);
    chk_cond(q, "[enter] en Paso a paso: también dibuja", ui_pantalla() == P_CPM_DIB);
}

/* ---- l. La Teoría y los Ejemplos ---- */
/* Lo que dice cada ejemplo, comprobado sobre su resultado. Devuelve -1 si
 * el ejemplo no está en la tabla (se dice: hay que añadirlo) */
static int cp_ejemplo_cuadra(const CpqEjemplo *e, const CpmRes *r)
{
    char t[300];
    const char *bod = e->bodega, *A = CPM_BODEGA_A, *B = CPM_BODEGA_B;
    if (!strcmp(e->valor, "17") && !e->tol[0] && e->n == 0 && !strcmp(bod, A))
        return r->val[0].tipo == CPM_PAR && r->v[r->val[0].a] == 7.0 && r->v[r->val[0].b] == 10.0 && r->tot[2] == 1;
    if (!strcmp(e->valor, "14") && !e->tol[0] && e->n == 0 && !strcmp(bod, B))
        return r->val[0].tipo == CPM_SER && r->v[r->val[0].a] == 21.0 && r->v[r->val[0].b] == 42.0;
    if (!strcmp(e->valor, "12") && !e->tol[0] && e->n == 0 && !strcmp(bod, B))
        return cp_esta(r, CPM_SER, 18.0, 36.0) == 0 && cp_esta(r, CPM_SER, 20.0, 30.0) == 1 && cp_esta(r, CPM_SER, 21.0, 28.0) == 2 &&
               r->val[2].ct == 12.0;
    if (!strcmp(e->valor, "22.5") && !strcmp(e->tol, "0.5") && e->n == 0 && !strcmp(bod, "15-42 sin 21-24")) {
        /* «con tres sí: 535 arreglos. Se ven los 20 más cercanos. El del
         * PDF: ejemplo 10»: el del PDF, (20 ∥ 28) y 42, vale pero no se ve,
         * y el ejemplo 10 es él */
        int i, ve_pdf = 0;
        const CpqEjemplo *e10 = cpq_ejemplo(9);
        for (i = 0; i < r->nval; i++)
            if (r->val[i].tipo == CPM_MIX1 && r->v[r->val[i].a] == 20.0 && r->v[r->val[i].b] == 28.0 && r->v[r->val[i].c] == 42.0) ve_pdf = 1;
        return r->tot[1] + r->tot[2] == 0 && r->tot[3] == 535 && r->nver[3] == 20 && strstr(e->ensena, "535") &&
               strstr(e->ensena, "20 m" S_aa "s cercanos") && r->val[0].tipo == CPM_MIX1 && r->val[0].ct == 22.5 &&
               r->v[r->val[0].a] == 18.0 && r->v[r->val[0].b] == 42.0 && r->v[r->val[0].c] == 36.0 &&
               cpm_vale(r, 48.0 * 42.0 / 90.0) && !ve_pdf &&
               strstr(e->ensena, "ejemplo 10") && cpq_ejemplos_n() >= 10 && !strcmp(e10->bodega, "20 28 42") &&
               !strcmp(e10->valor, "22.5") && !strcmp(e10->tol, "0.5");
    }
    if (!strcmp(e->valor, "22.5") && !strcmp(e->tol, "0.5") && e->n == 0 && !strcmp(bod, "20 28 42")) {
        /* el mixto del PDF, solo: (20 ∥ 28) = 48 en serie con 42 = 2016/90 = 22.4 */
        cpm_proced(r, 0, cp_txt, sizeof cp_txt);
        cpm_veredicto(r, t, sizeof t);
        return r->tot[1] + r->tot[2] == 0 && r->ntot == 1 && r->val[0].tipo == CPM_MIX1 && r->v[r->val[0].a] == 20.0 &&
               r->v[r->val[0].b] == 28.0 && r->v[r->val[0].c] == 42.0 && fabs(r->val[0].ct - 22.4) < 1e-12 &&
               !strcmp(t, "Con dos no se puede (" S_IQ "el profe lo puso mal?);\ncon tres s" S_ii ": (20 " S_PAR " 28) y 42 en serie = 22.4 MF " S_CHECK) &&
               cp_tiene("   48 en serie con 42: 48" S_CDOT "42/(48 + 42) = 2016/90 = 22.4 MF " S_CHECK " (dif. -0.1)\n") &&
               strstr(e->ensena, "2016/90 = 22.4 MF " S_CHECK " (dif. -0.1)");
    }
    if (!strcmp(e->valor, "14") && !e->tol[0] && e->n == 0 && !strcmp(bod, "1 2 3 4 5 6 7 10 12 7"))
        return r->tot[2] == 3 && cp_esta(r, CPM_PAR, 2.0, 12.0) >= 0 && cp_esta(r, CPM_PAR, 4.0, 10.0) >= 0 &&
               cp_esta(r, CPM_PAR, 7.0, 7.0) >= 0;
    if (!strcmp(e->valor, "14") && !e->tol[0] && e->n == 0 && !strcmp(bod, "20 28 28 35 50")) {
        static CpmRes r1;
        static const double V[4] = { 20.0, 28.0, 35.0, 50.0 };
        cpm_resuelve(&r1, 14.0, 0.0, V, 4);
        return cp_esta(r, CPM_SER, 28.0, 28.0) >= 0 && r1.tot[1] + r1.tot[2] == 0;   /* con un solo 28, irresoluble con dos */
    }
    if (!strcmp(e->valor, "55") && !e->tol[0] && e->n == 0 && !strcmp(bod, "10 15 20 25"))
        return r->tot[1] + r->tot[2] == 0 && r->val[0].tipo == CPM_PAR3 && r->val[0].ct == 55.0 &&
               r->v[r->val[0].a] == 10.0 && r->v[r->val[0].b] == 20.0 && r->v[r->val[0].c] == 25.0 && 25.0 + 20.0 == 45.0;
    if (!strcmp(e->valor, "5") && !e->tol[0] && e->n == 0 && !strcmp(bod, "10 15 30"))
        return r->tot[1] + r->tot[2] == 0 && r->val[0].tipo == CPM_SER3 && r->val[0].ct == 5.0;
    if (!strcmp(e->valor, "14") && !e->tol[0] && e->n == 3 && !strcmp(bod, B)) {
        int i, ser3 = 0;
        double maxser3 = 39.0 * 40.0 * 41.0 + 0.0;
        for (i = 0; i < r->nval; i++) if (r->val[i].tipo == CPM_SER3) ser3++;
        maxser3 = 40.0 * 41.0 * 42.0 / (40.0 * 41.0 + 41.0 * 42.0 + 40.0 * 42.0);
        cpm_titulo(r, &r->val[0], t, sizeof t);
        return r->modo == 3 && r->nver[2] == 0 && ser3 == 0 && maxser3 < 13.95 && fabs(maxser3 - 13.7) < 0.05 &&
               !strcmp(t, "(15 " S_PAR " 27) y 21 en serie = 14 MF");
    }
    return -1;
}

static void cp_teoria_ejemplos(void)
{
    const char *q = "capacitores: Teoría y Ejemplos";
    int n = cpq_teoria_n(), i, malos = 0, largos = 0, tit = 0, ne = cpq_ejemplos_n(), sin_tabla = 0, no_cuadra = 0, carga = 0;

    /* el contenido: títulos de 40 columnas, líneas de 52 */
    for (i = 0; i < n; i++) {
        const char *s = cpq_teoria_texto(i), *p = s;
        if ((int)strlen(cpq_teoria_titulo(i)) > 40 || !cpq_teoria_titulo(i)[0]) tit++;
        while (*p) {
            int k = 0;
            while (p[k] && p[k] != '\n') k++;
            if (k > 52) { largos++; if (largos < 4) printf("    línea larga en la página %d: %.60s\n", i + 1, p); }
            p += k;
            if (*p) p++;
        }
        if (!s[0]) malos++;
    }
    chk_cond(q, "la Teoría tiene páginas, con títulos de 40 columnas y líneas de 52", n >= 2 && tit == 0 && largos == 0 && malos == 0);
    for (i = 0; i < ne; i++) {
        const CpqEjemplo *e = cpq_ejemplo(i);
        const char *p = e->ensena;
        int k = 0, lin = 1;
        if ((int)strlen(e->titulo) > 40 || !e->titulo[0]) tit++;
        for (; *p; p++) { if (*p == '\n') { if (k > 52) largos++; k = 0; lin++; } else k++; }
        if (k > 52 || lin > 2) largos++;
    }
    chk_cond(q, "los Ejemplos: títulos de 40, «ensena» de dos líneas de 52 como mucho", ne >= 2 && tit == 0 && largos == 0);

    /* el menú -> Teoría: el índice; [enter] una página; [der] la siguiente */
    cp_entra_menu();
    TECLAS(T_ABAJO, T_ENTER);
    chk_cond(q, "el menú: [ab] [enter] abre la Teoría (su índice)", ui_pantalla() == P_CPM_TEO && ve("1. ") && ve(cpq_teoria_titulo(0)));
    cp_vuelca("teo");
    TECLAS(T_ABAJO, T_ENTER);
    chk_cond(q, "[ab] [enter]: la página 2, con su título en la barra", ui_pantalla() == P_CPM_PAG && ui_indice(UI_I_CPM_TSEL) == 1 &&
             ve("Teor" S_ii "a 2/"));
    cp_vuelca("teo_pag");
    TECLAS(T_DER);
    chk_cond(q, "[der]: la página siguiente, desde arriba", ui_indice(UI_I_CPM_TSEL) == 2 && ui_indice(UI_I_SCROLL) == 0);
    for (i = 0; i < n && !ui_repite(T_ABAJO | T_REPE, 0); i++) TECLAS(T_DER);
    TECLAS(T_ABAJO);
    chk_cond(q, "una página más larga que la pantalla se desplaza con [ab]", ui_indice(UI_I_SCROLL) == 1);
    repite(T_ABAJO | T_CON_CTRL, 1);
    cp_vuelca("teo_pag_fin");
    TECLAS(T_DER | T_CON_CTRL);
    for (i = 0; i < n; i++) TECLAS(T_DER);
    chk_cond(q, "[der] no pasa de la última", ui_indice(UI_I_CPM_TSEL) == n - 1);
    TECLAS(T_ESC);
    chk_cond(q, "[esc]: al índice, en esa página", ui_pantalla() == P_CPM_TEO && ui_indice(UI_I_CPM_TSEL) == n - 1);
    TECLAS(T_ESC);
    chk_cond(q, "[esc]: al menú, en Teoría", ui_pantalla() == P_CPM_MENU && ui_indice(UI_I_CPM_MSEL) == 1);
    {   /* todas las páginas se ven sin salirse */
        int d = gfx_desbordes();
        cp_entra_menu();
        TECLAS(T_DIG(2), T_ENTER);
        for (i = 0; i < n; i++) { repite(T_ABAJO, 60); ui_dibuja(); TECLAS(T_DER); }
        chk_cond(q, "todas las páginas, enteras, sin textos que se salgan", gfx_desbordes() == d);
    }

    /* los Ejemplos: cada uno carga sus datos y resuelve lo que dice */
    cp_entra_menu();
    TECLAS(T_DIG(3));
    chk_cond(q, "el menú: [3] abre los Ejemplos", ui_pantalla() == P_CPM_EJ && ve(cpq_ejemplo(0)->titulo));
    cp_vuelca("ej");
    for (i = 0; i < ne; i++) {
        const CpqEjemplo *e = cpq_ejemplo(i);
        int k, c;
        char nom[32];
        cp_entra_menu();
        TECLAS(T_DIG(3));
        for (k = 0; k < i; k++) TECLAS(T_ABAJO);
        TECLAS(T_ENTER);
        if (ui_pantalla() == P_CPM_RES && !strcmp(ui_cpm_texto(0), e->valor) && !strcmp(ui_cpm_texto(1), e->tol) &&
            !strcmp(ui_cpm_texto(2), e->bodega) && ui_indice(UI_I_CPM_MODO) == (e->n == 2 || e->n == 3 ? e->n : 0)) carga++;
        else printf("    el ejemplo %d (%s) no carga\n", i + 1, e->titulo);
        c = ui_pantalla() == P_CPM_RES ? cp_ejemplo_cuadra(e, ui_cpm()) : 0;
        if (c < 0) { sin_tabla++; printf("    el ejemplo %d (%s) no está en la tabla de la prueba\n", i + 1, e->titulo); }
        else if (!c) { no_cuadra++; printf("    el ejemplo %d (%s) NO resuelve lo que dice\n", i + 1, e->titulo); }
        if (i == 3 || i == 4 || i == ne - 1) { sprintf(nom, "ej_res_%d", i + 1); cp_vuelca(nom); }
        if (i == 4 && ui_pantalla() == P_CPM_RES) {      /* 14 con dos de 7: la opción 3, 7 ∥ 7, y su paso a paso */
            TECLAS(T_ABAJO, T_ABAJO, T_DER);
            chk_cond(q, "14 con dos de 7: la opción 3 es 7 " S_PAR " 7 y su paso a paso dice «hay dos de 7»",
                     ui_indice(UI_I_CPM_SEL) == 2 && strstr(ui_cpm_paso(), "7 + " S_IQ "7? " S_RARR " hay dos de 7 " S_CHECK) != 0);
            repite(T_ABAJO, 4);
            cp_vuelca("ej_res_5_paso");
        }
        if (i == 0) {
            TECLAS(T_ESC);
            chk_cond(q, "[esc] del resultado de un ejemplo: los datos, editables, con el aviso", ui_pantalla() == P_CPM &&
                     ve("Los datos del ejemplo: c" S_aa "mbialos y [enter]."));
        }
    }
    printf("    %d ejemplos: cargan %d, sin comprobación %d, no cuadran %d\n", ne, carga, sin_tabla, no_cuadra);
    chk_cond(q, "cada ejemplo carga sus datos (valor, tolerancia, Capacitores y bodega) y abre su resultado", carga == ne);
    chk_cond(q, "cada ejemplo resuelve lo que dice su «ensena» (y todos están en la tabla)", sin_tabla == 0 && no_cuadra == 0);
}

/* ---- m. El cierre de la v3: los hallazgos de las dos revisiones (la del
 *  cálculo y los textos, y la de los controles, las pantallas y el dibujo),
 *  cada uno con su prueba ---- */

/* La fila o de la lista, como en la pantalla (49 columnas): ¿cabe sin «...»
 * y se leen el CT (como en el título) y la diferencia (la de la lista)? */
static int cq_fila_bien(const CpmRes *r, int o, char *t, size_t cap)
{
    char d[48], tt[160], ct[48];
    const char *dd, *p, *q;
    cpm_fila_lista(r, &r->val[o], o + 1, 52 - 3, t, cap);
    cpm_dif_op(r, &r->val[o], d, sizeof d);
    dd = strncmp(d, "dif. ", 5) ? d : d + 5;
    if ((int)strlen(t) > 49 || strstr(t, "...")) return 0;
    if (!strstr(t, dd) || t[strlen(t) - 1] != ')') return 0;
    if (r->val[o].tipo == CPM_UNO) return 1;
    cpm_titulo(r, &r->val[o], tt, sizeof tt);         /* «... = 22.496 MF»: el CT */
    p = strstr(tt, " = ");
    q = p ? strstr(p, " MF") : 0;
    if (!p || !q || q - p - 3 >= (int)sizeof ct) return 0;
    memcpy(ct, p + 3, (size_t)(q - p - 3));
    ct[q - p - 3] = 0;
    return strstr(t, ct) != 0;
}

/* Las líneas de un veredicto: cuántas pasan de 51 columnas */
static int cq_ver_largas(const CpmRes *r, int *nl)
{
    char t[400];
    const char *p;
    int mal = 0;
    cpm_veredicto(r, t, sizeof t);
    *nl = 0;
    for (p = t; *p;) {
        const char *q = strchr(p, '\n');
        int m = q ? (int)(q - p) : (int)strlen(p);
        if (m > CPM_VER_COLS) mal++;
        (*nl)++;
        p += m;
        if (*p) p++;
    }
    return mal;
}

/* Un valor de bodega «de examen»: enteros (como la A y la B) o, si no,
 * también medios y cuartos */
static double cq_valor(int enteros)
{
    unsigned u = azar(10);
    if (enteros || u < 5) return (double)(1 + azar(60));
    if (u < 8) return 0.5 + (double)azar(60);
    return 0.25 * (double)(1 + 2 * azar(120));
}

/* REV-3 (y el 4.º de la del cálculo): las filas y el veredicto, al azar */
static void cq_filas(void)
{
    const char *q = "capacitores: cierre v3, las filas y el veredicto caben";
    static CpmRes r;
    static double v[30];
    int enteros, caso, i, malas[2] = { 0, 0 }, filas[2] = { 0, 0 }, ver[2] = { 0, 0 }, tres = 0, ej = 0;
    unsigned s0 = semilla;
    char t[200];
    for (enteros = 1; enteros >= 0; enteros--) {
        semilla = 777u + (unsigned)enteros;
        for (caso = 0; caso < 3000; caso++) {
            int n = 4 + (int)azar(16), modo = (int)azar(3), nl;
            double C, tol;
            modo = modo == 0 ? 0 : modo + 1;
            for (i = 0; i < n; i++) v[i] = cq_valor(enteros);
            C = 0.5 * (double)(4 + azar(100));
            tol = azar(3) == 0 ? 0.0 : azar(2) ? 0.5 : 0.05 * C;
            if (tol >= C) continue;
            cpm_resuelve_n(&r, C, tol, modo, v, n);
            for (i = 0; i < r.nval; i++) {
                filas[enteros]++;
                if (!cq_fila_bien(&r, i, t, sizeof t)) { malas[enteros]++; if (ej++ < 4) printf("    fila mal: «%s»\n", t); }
            }
            ver[enteros] += cq_ver_largas(&r, &nl);
            if (nl >= 3) tres++;
        }
    }
    semilla = s0;
    printf("    filas al azar: enteros %d (mal %d), medios y cuartos %d (mal %d); veredictos largos %d y %d; en tres líneas %d\n",
           filas[1], malas[1], filas[0], malas[0], ver[1], ver[0], tres);
    chk_cond(q, "3000 bodegas de enteros: ninguna fila se corta con «...» (se leen el CT y la diferencia, en 49 columnas)", malas[1] == 0 && filas[1] > 10000);
    chk_cond(q, "...ni con medios y cuartos", malas[0] == 0 && filas[0] > 10000);
    chk_cond(q, "...y ningún veredicto pasa de 51 columnas", ver[0] == 0 && ver[1] == 0);
    /* el caso 3 del PDF: la fila 15 (antes «... = 22.496 MF (dif. -0...») */
    cp_caso(22.5, 0.5, "15-42 sin 21-24");
    cpm_fila_lista(&cp_r, &cp_r.val[14], 15, 49, t, sizeof t);
    chk_cond(q, "22.5, B sin 21-24, la fila 15: sin « MF», con el CT y la diferencia enteros",
             !strcmp(t, "15. mixto: (40 " S_PAR " 42) y 31 = 22.496 (dif. -0.004)"));
    cpm_fila_lista(&cp_r, &cp_r.val[0], 1, 49, t, sizeof t);
    chk_cond(q, "...y la 1, entera (cabe)", !strcmp(t, "1. mixto: (18 " S_PAR " 42) y 36 = 22.5 MF (exacto)"));
    cp_caso_n(17.0, 0.0, 3, "10 7 50");
    cpm_veredicto(&cp_r, t, sizeof t);
    chk_cond(q, "«17 : 10 7 50» con 3: el veredicto, con su diferencia (antes «(dif....»)",
             !strcmp(t, "IRRESOLUBLE con tres (con menos s" S_ii ": elige Auto).\nLo m" S_aa "s cercano: (7 y 50) " S_PAR " 10 = 16.1 MF (dif. -0.9)"));
    cp_caso(1500.28, 0.0, "1000.125 2000.375 3000.625");
    cpm_veredicto(&cp_r, t, sizeof t);
    chk_cond(q, "un título que no cabe ni sin «en serie»: a la línea siguiente, entero",
             !strcmp(t, "Con dos no se puede (" S_IQ "el profe lo puso mal?);\ncon tres s" S_ii ":\n"
                        "(1000.125 " S_PAR " 2000.375) y 3000.625 = 1500.281 MF " S_CHECK));
    /* en la pantalla, la del caso 3 */
    cp_escribe_caso("22.5", "0.5", "15-42 sin 21-24");
    repite(T_ABAJO, 14);
    chk_cond(q, "la pantalla enseña la fila 15 entera", ui_indice(UI_I_CPM_SEL) == 14 &&
             ve("15. mixto: (40 " S_PAR " 42) y 31 = 22.496 (dif. -0.004)"));
    cp_vuelca("res_fila_15");
}

/* El 1.º de la del cálculo: la cuenta del mixto 2 se ve entera (la suma que
 * se lee da el CT que se lee), sin «= ≈» */
static void cq_mixto2(void)
{
    const char *q = "capacitores: cierre v3, la cuenta del mixto 2";
    static CpmRes r;
    static double v[20];
    static char txt[32768];
    int caso, i, n_mix2 = 0, mal_suma = 0, mal_aprox = 0, ej = 0;
    unsigned s0 = semilla;
    const CpmRes *r17 = cp_caso(17.0, 0.0, CPM_BODEGA_A);
    cpm_proced(r17, &r17->val[6], cp_txt, sizeof cp_txt);
    chk_cond(q, "17 A, la opción 7: «9·32/(9 + 32) = 288/41 " S_APROX " 7.02 MF» y «CT = 7.02 + 10 = 17.02 MF " S_CHECK "»",
             r17->val[6].tipo == CPM_MIX2 && cp_tiene("   9 y 32 en serie: 9" S_CDOT "32/(9 + 32) = 288/41 " S_APROX " 7.02 MF\n") &&
             cp_tiene("   7.02 " S_PAR " 10: CT = 7.02 + 10 = 17.02 MF " S_CHECK " (dif. +0.02)\n") && !cp_tiene("= " S_APROX));
    cpm_proced(r17, &r17->val[4], cp_txt, sizeof cp_txt);
    chk_cond(q, "...y la 5, exacta, como antes: «720/72 = 10 MF» y «10 + 7 = 17»",
             cp_tiene("   12 y 60 en serie: 12" S_CDOT "60/(12 + 60) = 720/72 = 10 MF\n") && cp_tiene("   10 " S_PAR " 7: CT = 10 + 7 = 17 MF " S_CHECK "\n"));
    /* al azar: en cada mixto 2 que se enseña, lo que se suma da lo que se ve */
    semilla = 90817u;
    for (caso = 0; caso < 2500; caso++) {
        int n = 3 + (int)azar(10), modo = azar(2) ? 0 : 3;
        double C, tol = azar(3) == 0 ? 0.5 : 0.0;
        for (i = 0; i < n; i++) v[i] = azar(4) ? (double)(1 + azar(60)) : 0.25 * (double)(1 + 2 * azar(120));
        C = 0.5 * (double)(4 + azar(100));
        cpm_resuelve_n(&r, C, tol, modo, v, n);
        for (i = 0; i < r.nval; i++) {
            const char *p, *e;
            double a, b, c;
            if (r.val[i].tipo != CPM_MIX2) continue;
            n_mix2++;
            cpm_proced(&r, &r.val[i], txt, sizeof txt);
            if (strstr(txt, "= " S_APROX)) mal_aprox++;
            p = strstr(txt, ". Comprobaci" S_oo "n:");
            p = p ? strstr(p, ": CT = ") : 0;
            if (!p) { mal_suma++; continue; }
            a = strtod(p + 7, (char **)&e);
            if (strncmp(e, " + ", 3)) { mal_suma++; continue; }
            b = strtod(e + 3, (char **)&p);
            if (strncmp(p, " = ", 3)) { mal_suma++; continue; }
            c = strtod(p + 3, 0);
            if (floor((a + b) * 1e9 + 0.5) != floor(c * 1e9 + 0.5)) {
                mal_suma++;
                if (ej++ < 3) printf("    mixto 2: %.6g + %.6g no da %.10g\n", a, b, c);
            }
        }
    }
    semilla = s0;
    printf("    mixtos 2 al azar: %d; la suma no da el CT %d; «= " S_APROX "» %d\n", n_mix2, mal_suma, mal_aprox);
    chk_cond(q, "al azar (enteros y cuartos, con y sin tolerancia): la suma que se ve da el CT que se ve, y nunca «= " S_APROX "»",
             n_mix2 > 300 && mal_suma == 0 && mal_aprox == 0);
}

/* ¿t empieza por p? */
static int cq_empieza(const char *t, const char *p) { return !strncmp(t, p, strlen(p)); }

/* El 2.º de la del cálculo: con «Capacitores: 2», uno solo que ya vale */
static void cq_dos_con_uno(void)
{
    const char *q = "capacitores: cierre v3, con 2 y uno solo que vale";
    char t[300];
    const CpmRes *r = cp_caso_n(28.0, 0.0, 2, CPM_BODEGA_A);
    cpm_veredicto(r, t, sizeof t);
    chk_cond(q, "28 con 2 y la A: el veredicto dice que el 28 solo sí (no manda a tres)",
             !strcmp(t, "IRRESOLUBLE con dos (el 28 solo s" S_ii ": elige Auto).\nLo m" S_aa "s cercano: 28 " S_PAR " 1 = 29 MF (dif. +1)"));
    chk_cond(q, "...lleva «te faltaría» (§5.2), sin el directo (ya lo hay), y lo de uno al final",
             cpm_con_faltas(r) && cp_tiene("Ojo: el 28 solo ya vale (exacto): es la sustituci" S_oo "n directa; aqu" S_ii " se piden dos.\n") &&
             cp_tiene("Te faltar" S_ii "a (uno m" S_aa "s en la bodega lo resuelve con dos):\n") &&
             cp_tiene("   Directo: ya lo hay (el 28), pero se piden dos.\n") && cp_tiene("     el 12 con uno de 16\n") &&
             cp_tiene("   El menor que har" S_ii "a falta: uno de 16, con el 12 en paralelo.\n") &&
             cp_tiene("Con uno solo s" S_ii " se puede: el 28 (elige Capacitores: Auto).\n"));
    r = cp_caso(28.0, 0.0, CPM_BODEGA_A);
    chk_cond(q, "...y en Auto, el 28 solo, primero, sin faltas", r->val[0].tipo == CPM_UNO && !cpm_con_faltas(r));
    r = cp_caso_n(22.5, 0.5, 2, CPM_BODEGA_B);
    cpm_veredicto(r, t, sizeof t);
    chk_cond(q, "22.5 " S_PM " 0.5 con 2 y la B entera (22 y 23 valen solos): el más cercano, el 22, y las faltas",
             cpm_con_faltas(r) && cq_empieza(t, "IRRESOLUBLE con dos (el 22 solo s" S_ii ": elige Auto).\n") &&
             cp_tiene("     el 22 con uno de 1\n"));
    r = cp_caso_n(22.5, 0.5, 2, "15-42 sin 21-24");
    chk_cond(q, "...sin ninguno que valga solo, como antes: «con tres sí»", cpm_con_faltas(r) && !cp_tiene("Ojo:") &&
             !cp_tiene("ya lo hay"));
    cp_caso_n(22.25, 0.5, 2, "22.25 99.75");
    cpm_veredicto(&cp_r, t, sizeof t);
    chk_cond(q, "...con «22.25» cabe justo (51 columnas)", cq_empieza(t, "IRRESOLUBLE con dos (el 22.25 solo s" S_ii ": elige Auto).\n"));
    cp_caso_n(122.25, 0.5, 2, "122.25 999.75");
    cpm_veredicto(&cp_r, t, sizeof t);
    chk_cond(q, "...y si el valor no cabe, «uno solo sí»", cq_empieza(t, "IRRESOLUBLE con dos (uno solo s" S_ii ": elige Auto).\n"));
}

/* El 5.º al 8.º de la del cálculo: el cableado del mixto 1 con repetidos, el
 * resumen con tolerancia, el porqué con mucha tolerancia y las diferencias
 * que se escriben igual */
static void cq_textos(void)
{
    const char *q = "capacitores: cierre v3, textos";
    static CpmRes r;
    static double v[24];
    char t[200], u[200];
    int caso, i, mal_igual = 0, mal_orden = 0, mal_tanda = 0, pares = 0;
    unsigned s0 = semilla;
    cp_caso_n(5.25, 0.0, 3, "7 7 14");
    chk_cond(q, "(7 " S_PAR " 14) y 7: el de la serie es «el otro 7»",
             cp_tiene("   Sus derechas (unidas) " S_RARR " izquierda del otro 7 (punto medio).\n") &&
             cp_tiene("   Derecha del otro 7 " S_RARR " l" S_ii "nea 2 y T.\n"));
    cp_caso_n(14.0 / 3.0, 0.0, 3, "7 7 7");
    chk_cond(q, "(7 " S_PAR " 7) y 7: «el tercer 7»", cp_r.val[0].tipo == CPM_MIX1 &&
             cp_tiene("   Sus derechas (unidas) " S_RARR " izquierda del tercer 7 (punto medio).\n"));
    cp_caso_n(17.0, 0.0, 3, "20 30 5 7 7");
    chk_cond(q, "...y sin repetir, como antes", !cp_tiene("del otro") && !cp_tiene("del tercer"));

    cp_caso_n(50.0, 1.0, 2, "10 30-49");
    chk_cond(q, "50 " S_PM " 1: el resumen de los «no hay» da lo que se buscó (de 1 a 8), no lo nominal (de 2 a 7)",
             cp_tiene("     del 48 al 43: faltar" S_ii "a de 1 a 8; no hay\n") && cp_tiene("     42 + " S_IQ "7 a 9? " S_RARR " no hay\n"));
    cp_caso_n(27.8, 1.0, 2, "35-42 50-57");
    chk_cond(q, "27.8 " S_PM " 1, en serie: «con 35 a 41 falta de 77.4 a 162.5» (la unión), y la tanda se corta en el hueco (42 y 50)",
             cp_tiene("     con 35 a 41 falta de 77.4 a 162.5: no hay\n") && cp_tiene("     con 42 falta de 74.1 a 91.6 " S_RARR " no hay\n") &&
             cp_tiene("     con 50 falta de 57.8 a 67.9 " S_RARR " no hay\n"));
    cp_caso_n(50.0, 0.25, 2, "10 30-49");
    chk_cond(q, "50 " S_PM " 0.25 (rangos con huecos): sin resumen, intento por intento", !cp_tiene("     del 48") &&
             cp_tiene("     48 + " S_IQ "1.8 a 2.2? " S_RARR " no hay\n") && cp_tiene("     47 + " S_IQ "2.8 a 3.2? " S_RARR " no hay\n"));

    cp_caso_n(3.01, 2.0, 2, "2 3");
    chk_cond(q, "3.01 " S_PM " 2 con 2 y 3: el porqué ya no dice que la serie no puede, y la serie se busca",
             cp_r.val[0].tipo == CPM_SER && !cp_tiene("va en paralelo (la serie da menos") && !cp_tiene("no hay valores mayores") &&
             cp_tiene("sirve el paralelo; con esta tolerancia (vale desde 1.01) quiz" S_aa " tambi" S_ee "n la serie") &&
             cp_tiene("busco dos mayores que 1.01.") && cp_tiene("     con 2 falta desde 2.1 " S_RARR " hay 3 " S_CHECK "\n"));
    cp_caso(17.0, 0.0, "10 7 2");
    chk_cond(q, "...y sin esa tolerancia, como antes: «la serie no puede»", cp_tiene("la serie no puede (dar" S_ii "a menos que el menor)"));

    cp_caso(14.0, 0.0, "3 10.98 27 29");
    cpm_fila_lista(&cp_r, &cp_r.val[0], 1, 49, t, sizeof t);
    cpm_fila_lista(&cp_r, &cp_r.val[1], 2, 49, u, sizeof u);
    chk_cond(q, "14 con 27 y 29 (13.982) y 10.98 + 3 (13.98): las diferencias se distinguen (-0.018 y -0.02)",
             !strcmp(t, "1. serie: 27 y 29 = 13.98 MF (dif. -0.018)") && !strcmp(u, "2. paralelo: 10.98 " S_PAR " 3 = 13.98 MF (dif. -0.02)"));
    chk_cond(q, "...y el paso a paso dice las dos al decir que la serie queda más cerca, y la comprobación, la de la lista",
             cp_tiene("   Sirve (dif. -0.02), pero en serie queda m" S_aa "s cerca (dif. -0.018).\n") &&
             cp_tiene("= 783/56 = 13.98 MF " S_CHECK " (dif. -0.018)\n"));
    cp_caso(13.9996, 0.0, "3 10.976 27 29");
    chk_cond(q, "...y con C de cuatro decimales (13.9996), también la del paralelo con tres: -0.024 y -0.017 (con dos, -0.02 y -0.02)",
             cp_r.val[0].tipo == CPM_SER && cp_tiene("   Sirve (dif. -0.024), pero en serie queda m" S_aa "s cerca (dif. -0.017).\n"));
    cp_caso(22.5, 0.5, "15-42 sin 21-24");
    cpm_fila_lista(&cp_r, &cp_r.val[15], 16, 49, t, sizeof t);
    cpm_fila_lista(&cp_r, &cp_r.val[16], 17, 49, u, sizeof u);
    chk_cond(q, "las que están a la misma distancia se escriben igual (16 y 17: las dos +0.019)",
             strstr(t, "(dif. +0.019)") && strstr(u, "(dif. +0.019)"));
    /* al azar: dos vecinas de un grupo a distancias distintas no se escriben
     * igual y se leen en orden; a la misma, igual */
    semilla = 4321u;
    for (caso = 0; caso < 1500; caso++) {
        int n = 3 + (int)azar(18), modo = (int)azar(3);
        double C, tol = azar(3) == 0 ? 0.5 : 0.0;
        modo = modo == 0 ? 0 : modo + 1;
        for (i = 0; i < n; i++) v[i] = azar(3) ? (double)(1 + azar(50)) : 0.5 + (double)azar(50);
        C = 0.5 * (double)(4 + azar(100));
        cpm_resuelve_n(&r, C, tol, modo, v, n);
        for (i = 0; i + 1 < r.nval; i++) {
            double a, b;
            const char *pa, *pb;
            if (cpm_ncap(r.val[i].tipo) != cpm_ncap(r.val[i + 1].tipo)) continue;
            cpm_dif_op(&r, &r.val[i], t, sizeof t);
            cpm_dif_op(&r, &r.val[i + 1], u, sizeof u);
            pa = strncmp(t, "dif. ", 5) ? "0" : t + 6;            /* sin el signo */
            pb = strncmp(u, "dif. ", 5) ? "0" : u + 6;
            a = strtod(pa, 0); b = strtod(pb, 0);
            pares++;
            if (fabs(fabs(r.val[i].ct - C) - fabs(r.val[i + 1].ct - C)) < 0.5e-9) { if (strcmp(pa, pb)) mal_tanda++; }
            else if (!strcmp(pa, pb)) {
                mal_igual++;
                if (mal_igual <= 4) printf("    iguales: C %.10g, %.12g y %.12g: «%s» y «%s»\n", C, r.val[i].ct, r.val[i + 1].ct, t, u);
            }
            else if (!(a < b)) mal_orden++;
        }
    }
    semilla = s0;
    printf("    diferencias vecinas al azar: %d pares; iguales a distancias distintas %d, fuera de orden %d, la misma distancia escrita distinto %d\n",
           pares, mal_igual, mal_orden, mal_tanda);
    chk_cond(q, "al azar: vecinas a distancias distintas no se escriben igual y se leen en orden; a la misma, igual",
             pares > 3000 && mal_igual == 0 && mal_orden == 0 && mal_tanda == 0);
}

/* REV-1, REV-2 y REV-4 de la de los controles */
#define CQ_TOL_Y (16 + 8 + 22)      /* la fila de la tolerancia (CPM_FIL_Y + 22 en ui.c) */
static int cq_pix(u16 c, int x0, int y0, int w, int h)
{
    const u16 *fb = gfx_buffer();
    int x, y, n = 0;
    for (y = y0; y < y0 + h; y++) for (x = x0; x < x0 + w; x++) if (fb[y * SCR_W + x] == c) n++;
    return n;
}
static const char *cq_larga(void)
{
    static char t[CPM_TXT + 1];
    int i;
    t[0] = 0;
    for (i = 0; i < 85 && strlen(t) + 5 < CPM_TXT; i++) {
        char u[8];
        sprintf(u, i ? " %d" : "%d", 10 + (i * 7) % 80);
        strcat(t, u);
    }
    return t;
}
static void cq_controles(void)
{
    const char *q = "capacitores: cierre v3, controles";
    char mia[CPM_TXT + 1];
    int p0, i;

    /* REV-1: [arr] a la tolerancia y [ab] de vuelta, en el mismo sitio */
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    cp_escribe(cq_larga());
    TECLAS(T_IZQ | T_CON_CTRL, T_DER, T_DER, T_DER);
    p0 = ui_indice(UI_I_CPM_POS);
    TECLAS(T_ARRIBA);
    chk_cond(q, "REV-1: [arr] desde la primera línea de una bodega larga: a la tolerancia", ui_indice(UI_I_CPM_CAMPO) == 1);
    TECLAS(T_ABAJO);
    chk_cond(q, "REV-1: ...y [ab]: de vuelta al mismo sitio (la primera línea, arriba la caja; no al final)",
             ui_indice(UI_I_CPM_CAMPO) == 2 && ui_indice(UI_I_CPM_POS) == p0 && ui_indice(UI_I_CPM_BTOP) == 0);
    TECLAS(T_TAB, T_TAB, T_TAB, T_TAB);
    chk_cond(q, "REV-1: ...y con [tab] dando la vuelta, también", ui_indice(UI_I_CPM_CAMPO) == 2 && ui_indice(UI_I_CPM_POS) == p0);
    TECLAS(T_ENTER, T_ESC);
    chk_cond(q, "REV-1: tras resolver, [esc]: en la bodega, donde estaba", ui_pantalla() == P_CPM && ui_indice(UI_I_CPM_CAMPO) == 2 &&
             ui_indice(UI_I_CPM_POS) == p0);
    TECLAS(T_ESC, T_ENTER, T_ABAJO, T_ABAJO);
    chk_cond(q, "REV-1: al abrir Resolver, la bodega otra vez al final", ui_indice(UI_I_CPM_CAMPO) == 2 &&
             ui_indice(UI_I_CPM_POS) == (int)strlen(ui_cpm_texto(2)));

    /* REV-2: un campo vacío con el foco enseña el cursor */
    cp_entra();
    TECLAS(T_ABAJO);
    ui_dibuja();
    chk_cond(q, "REV-2: la tolerancia vacía con el foco enseña el cursor (§3.1)", ui_indice(UI_I_CPM_NUEVO) == 1 &&
             !ui_cpm_texto(1)[0] && cq_pix(C_CURSOR, 106, CQ_TOL_Y - 1, 146, 13) >= 13);
    cp_vuelca("datos_tol_vacia");

    /* REV-4: un ejemplo no se come lo del usuario */
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    cp_escribe(cq_larga());
    TECLAS(T_ABAJO, T_DIG(2));                              /* y «Capacitores: 2» */
    strcpy(mia, ui_cpm_texto(2));
    TECLAS(T_ESC, T_DIG(3));
    for (i = 0; i < 8; i++) TECLAS(T_ABAJO);                /* el 9, con «Capacitores: 3» */
    TECLAS(T_ENTER);
    chk_cond(q, "REV-4: el ejemplo 9 carga los suyos (con 3)", ui_pantalla() == P_CPM_RES && ui_indice(UI_I_CPM_MODO) == 3 &&
             !strcmp(ui_cpm_texto(2), "15-42"));
    TECLAS(T_ESC);
    chk_cond(q, "REV-4: [esc]: los datos del ejemplo, con el aviso de que los suyos vuelven", ui_pantalla() == P_CPM &&
             ve("Los datos del ejemplo: c" S_aa "mbialos y [enter].") && ve("Los tuyos vuelven al abrir Resolver desde el men" S_uu "."));
    cp_vuelca("datos_ejemplo");
    TECLAS(T_ESC, T_DIG(3));                                /* otro ejemplo (el 1) encima */
    for (i = 0; i < 8; i++) TECLAS(T_ARRIBA);
    TECLAS(T_ENTER);
    chk_cond(q, "REV-4: otro ejemplo encima (el 1)", ui_pantalla() == P_CPM_RES && !strcmp(ui_cpm_texto(0), "17") &&
             ui_indice(UI_I_CPM_MODO) == 0);
    TECLAS(T_ESC, T_ESC);
    TECLAS(T_DIG(1));
    chk_cond(q, "REV-4: Resolver desde el menú devuelve la bodega y el modo del usuario (aunque haya abierto dos ejemplos)",
             ui_pantalla() == P_CPM && !strcmp(ui_cpm_texto(2), mia) && ui_indice(UI_I_CPM_MODO) == 2 &&
             ve("Tus datos (los de antes del ejemplo)."));
    cp_vuelca("datos_tuyos");
    TECLAS(T_ESC, T_DIG(1));
    chk_cond(q, "REV-4: ...y una vez devueltos, se quedan", !strcmp(ui_cpm_texto(2), mia) && !ve("Tus datos"));

    /* la vista de la edición a mitad de una bodega larga (para el usuario) */
    cp_entra();
    TECLAS(T_ABAJO, T_ABAJO, T_BORRAR | T_CON_CTRL);
    cp_escribe(cq_larga());
    TECLAS(T_IZQ | T_CON_CTRL, T_ABAJO, T_ABAJO, T_DER | T_CON_SHIFT, T_DER | T_CON_SHIFT);
    chk_cond(q, "a mitad de la bodega: la 3.ª línea, en el principio de un valor, y la caja enseña desde la 1.ª",
             ui_indice(UI_I_CPM_CAMPO) == 2 && ui_indice(UI_I_CPM_POS) > 100 && ui_indice(UI_I_CPM_BTOP) == 0 &&
             ui_cpm_texto(2)[ui_indice(UI_I_CPM_POS) - 1] == ' ');
    cp_vuelca("datos_cursor_medio");
}

/* REV-5: los dibujos de tres con rótulos largos (hasta «xxxxx.xxx»): nada se
 * pisa ni se sale (antes, la serie de tres con 7 cifras o más) */
static double cq_clase(int k)
{
    switch (k) {
    case 0: return (double)(1 + azar(100));
    case 1: return (double)(100 + azar(900));
    case 2: return (double)(1000 + azar(9000));
    case 3: return (double)(100 + azar(900)) + (azar(2) ? 0.25 : 0.75);
    case 4: return (double)(10000 + azar(89999));
    case 5: return 1.0 + 0.001 * (double)(1 + 2 * azar(4499));
    case 6: return 10.0 + 0.001 * (double)(1 + 2 * azar(44999));
    case 7: return 100.0 + 0.001 * (double)(1 + 2 * azar(449999));
    default: return 10000.0 + 0.001 * (double)(1 + 2 * azar(44999999));
    }
}
/* La pantalla tal como está (un dibujo suelto), a out/cm_<nom>.bmp */
static void cq_vuelca_fb(const char *nom)
{
    char ruta[256];
    sprintf(ruta, "%s/cm_%s.bmp", dir_out, nom);
    gfx_dump_bmp(ruta);
}
static void cq_dibujo_largo(void)
{
    const char *q = "capacitores: cierre v3, dibujos de tres con rótulos largos";
    static CpmRes r;
    static double v[3];
    int k, caso, tipo, malos[9], total = 0, ej = 0, mal_total = 0;
    unsigned s0 = semilla;
    semilla = 4242u;
    for (k = 0; k < 9; k++) {
        malos[k] = 0;
        for (caso = 0; caso < 120; caso++) {
            int i;
            CpmComb c;
            for (i = 0; i < 3; i++) v[i] = i == 0 || azar(2) ? cq_clase(k) : cq_clase((int)azar((unsigned)k + 1));
            for (tipo = CPM_PAR3; tipo <= CPM_MIX2; tipo++) {
                int mal;
                cpm_resuelve_n(&r, 1.0, 0.0, 3, v, 3);
                if (r.n < 3) continue;
                memset(&c, 0, sizeof c);
                c.tipo = (unsigned char)tipo;
                if (tipo == CPM_MIX1 || tipo == CPM_MIX2) {
                    int z = (int)azar(3);
                    c.c = (unsigned char)z; c.a = (unsigned char)(z == 0 ? 1 : 0); c.b = (unsigned char)(z == 2 ? 1 : 2);
                } else { c.a = 0; c.b = 1; c.c = 2; }
                c.ct = cpm_ct(&r, tipo, c.a, c.b, c.c);
                cpm_resuelve_n(&r, c.ct, 0.0, 3, v, 3);
                mal = cp_dibujo_suelto(&r, &c, 0);
                {   /* y están todos los rótulos: la clavija, C, A, T, los tres valores y la leyenda (8 o más) */
                    CpmCaja cj[16];
                    int nc = cpm_cajas(cj, 16), ncr = 0, z;
                    for (z = 0; z < nc && z < 16; z++) if (cj[z].ocupados != -1) ncr++;
                    if (ncr < 8) mal |= 16;
                }
                total++;
                if (mal) {
                    malos[k]++; mal_total++;
                    if (ej++ < 4) { char t[120]; cpm_titulo(&r, &c, t, sizeof t); printf("    dibujo mal %d (clase %d, tipo %d): %s\n", mal, k, tipo, t); }
                }
            }
        }
    }
    semilla = s0;
    printf("    dibujos de tres con rótulos de 1 a 9 cifras: %d; malos por clase %d %d %d %d %d %d %d %d %d\n", total,
           malos[0], malos[1], malos[2], malos[3], malos[4], malos[5], malos[6], malos[7], malos[8]);
    chk_cond(q, "REV-5: los cuatro de tres, con rótulos de hasta 9 cifras («xxxxx.xxx»): nada se pisa ni se sale",
             mal_total == 0 && total > 4000);
    {   /* los dos del revisor, a la vista */
        static const double V1[3] = { 36.0, 65.327, 143.459 }, V2[3] = { 7215.375, 334.9, 3.086 };
        CpmComb c;
        memset(&c, 0, sizeof c);
        c.tipo = CPM_SER3; c.a = 0; c.b = 1; c.c = 2;
        cpm_resuelve_n(&r, 1.0, 0.0, 3, V1, 3);
        c.ct = cpm_ct(&r, CPM_SER3, 0, 1, 2);
        cpm_resuelve_n(&r, c.ct, 0.0, 3, V1, 3);
        chk_cond(q, "REV-5: 36, 65.327 y 143.459 en serie: limpio", cp_dibujo_suelto(&r, &c, 0) == 0);
        cq_vuelca_fb("dib_ser3_largo_1");
        cpm_resuelve_n(&r, 1.0, 0.0, 3, V2, 3);
        c.ct = cpm_ct(&r, CPM_SER3, 0, 1, 2);
        cpm_resuelve_n(&r, c.ct, 0.0, 3, V2, 3);
        chk_cond(q, "REV-5: 3.086, 334.9 y 7215.375 en serie: limpio", cp_dibujo_suelto(&r, &c, 0) == 0);
        cq_vuelca_fb("dib_ser3_largo_2");
    }
}

static void cq_cierre(void)
{
    cq_filas();
    cq_mixto2();
    cq_dos_con_uno();
    cq_textos();
    cq_controles();
    cq_dibujo_largo();
}

static void test_capmotor(void)
{
    int oc = ui_cpm_oculto_esta();
    printf("[76] Capacitores equivalentes (Taller, cuarta opci" S_oo "n)\n");
    ui_cpm_oculto(0);                   /* todo a la vista: es la prueba de lo nuevo */
    gfx_desbordes_reset();
    cp_lectura();
    cp_pdf();
    cp_bordes();
    cp_azar();
    cp_volcado();
    cp_pantallas();
    cp_indep();
    cp_rev_calculo();
    cp_rev_pantallas();
    cp_cursor();
    cp_pestanas();
    cp_teoria_ejemplos();
    cq_cierre();
    pruebas++;
    if (gfx_desbordes() != 0) falla("capacitores", "textos que se salen de la pantalla", gfx_desbordes(), 0);
    ui_cpm_oculto(oc);
}
