/* ============================================================================
 *  tipos.c  -  La tabla de componentes  (ver tipos.h)
 * ========================================================================= */
#include "tipos.h"
#include "gfx.h"

/* V e I con fasor: caja de ángulo en alterna, y el módulo en el convenio */
#define FA (CF_ANG | CF_AMPL)

static const char *const NOM_MED[MED_N] = { "CC (media)", "CA (alterna)", "CA+CC (eficaz)" };
static const char *const NOM_INTER[INTER_N] = { "abierto al empezar", "cerrado al empezar" };

/* ---- Etapa 4: huellas, nombres de las variantes y páginas Modelo ---- */
#define NB CF_NO_BUSCAR
#define SG CF_SIGNO
static const Huella H_AO  = { 2, 0, 2,  { 0, 0, 2 }, { 2, 0, 1 },  1, 2, { "+", "-", "sal" } };
static const Huella H_BJT = { 1, -1, 1, { 1, 1, 0 }, { -1, 1, 0 }, 2, 0, { "C", "E", "B" } };
static const Huella H_MOS = { 1, -1, 1, { 1, 1, 0 }, { -1, 1, 0 }, 2, 0, { "D", "S", "G" } };
const char *const NOM_DIODO[DIO_N] = { "ideal (interruptor)", "con V" S_GAMMA " (codo)",
                                       "exponencial (Shockley)", "1N4148 (se" S_nn "al)", "1N4007 (1 A)" };
const char *const NOM_ZENER[ZEN_N] = { "de libro (Vz, rz)", "exponencial", "1N4733A (5,1 V)" };
const char *const NOM_LED[LED_N]   = { "rojo", "naranja", "amarillo", "verde", "azul", "blanco" };
const char *const NOM_DEP[DEP_N]   = { "por una V", "por una I" };
const char *const NOM_AO[AO_N]     = { "ideal", "real (A, Ri, Ro)" };
const char *const NOM_BJT[BJT_N]   = { "NPN", "PNP" };
const char *const NOM_MOS[MOS_N]   = { "canal N", "canal P" };
#define SIN_CAP "\nSin capacidades: no hay recuperaci" S_oo "n inversa."
static const char *const MOD_DIODO[DIO_N] = {
    "Ideal: un interruptor. Conduce (v " S_APROX " 0) con i > 0;\n"
    "si no, abierto. Aqu" S_ii " conduce con 1 m" S_OHM " y abierto\n"
    "fuga 1 pS: 5 V sobre 1 k" S_OHM " dan 4,999995 mA." SIN_CAP,
    "Con V" S_GAMMA " (el del libro): v = V" S_GAMMA " + rd" S_CDOT "i si conduce;\n"
    "si no, abierto (fuga de 1 pS). V" S_GAMMA " vac" S_ii "a = 0,7 V;\n"
    "rd vac" S_ii "a = 1 m" S_OHM ". Al emular, cambia de estado\n"
    "en el instante exacto." SIN_CAP,
    "Shockley: i = Is" S_CDOT "(exp(vd/(n" S_CDOT "VT)) - 1), con\n"
    "v = vd + Rs" S_CDOT "i y VT = kT/q = 25,86 mV (27 " S_DEG "C).\n"
    "Newton lo resuelve en cada paso." SIN_CAP,
    "1N4148 (de se" S_nn "al): Is 2,52 nA, n 1,752, Rs 0,568 " S_OHM ".\n"
    "0,65 V a 4,3 mA." SIN_CAP,
    "1N4007 (rectificador de 1 A y 1000 V): Is 7,03 nA,\n"
    "n 1,808, Rs 34 m" S_OHM ". 0,77 V a 92 mA." SIN_CAP,
};
static const char *const MOD_ZENER[ZEN_N] = {
    "De libro: directa (V" S_GAMMA "), abierto o ruptura con\n"
    "v = -(Vz0 + rz" S_CDOT "|i|), Vz0 = Vz - rz" S_CDOT "Iz: pasa por\n"
    "(-Vz, -Iz). rz vac" S_ii "a = 1 m" S_OHM ".",
    "Exponencial: Shockley m" S_aa "s la ruptura\n"
    "-Iz" S_CDOT "exp(-(vd + BV)/VT), con Rs = rz - VT/Iz y\n"
    "BV = Vz - Rs" S_CDOT "Iz: pasa por (-Vz, -Iz) con\n"
    "pendiente rz.",
    "1N4733A (1 W): Vz 5,1 V a 49 mA, rz 7 " S_OHM ".",
};
static const char *const MOD_AO[AO_N] = {
    "Ideal: no entra corriente y, en lineal, v+ = v-\n"
    "(la masa virtual).",
    "Real: u = A" S_CDOT "(v+ - v-) recortada a [V-, V+], tras\n"
    "Ro; Ri entre + y -. Sin polo ni slew (Etapa 5).",
};

const TipoInfo TIPOS[EL_N] = {
    {   /* EL_RES */
        'R', 'R', "Resistencia", "Resistencia: ¿hacia dónde? (flecha)", C_RESIST, 0, 0, 1, 0, 3,
        {
            { Q_VAL, "R", S_OHM, "Valor de la resistencia" },
            { Q_V,   "V", "V",   "Tensión entre sus extremos (su caída)", 0, FA, 0, 0, 0 },
            { Q_I,   "I", "A",   "Corriente que la atraviesa", 0, FA, 0, 0, 0 },
        },
        1e-6, 1e9, LEY_VALOR, 0, 0, 0,
        "v = R" S_CDOT "i, con i de a hacia b (criterio pasivo).\n"
        "Disipa P = R" S_CDOT "i" S_SUP2 " = v" S_SUP2 "/R. Igual en CC y en el tiempo."
    },
    {   /* EL_FUENTE: en continua se ven E, I, r y V (los índices de la Etapa 1) */
        'E', 'E', "Fuente de tensión", "Fuente: flecha hacia el borne +", C_FUENTE, 1, 1, 0, 0, 17,
        {
            { Q_VAL,   "E",  "V",   "Tensión de la fuente (su fem)", VM(ONDA_CC), 0, 0, 0, 0 },
            { Q_VAL,   "E",  "V",   "Fasor: m" S_oo "dulo, y con [tab] el " S_aa "ngulo", VM(FORMA_FASOR), FA, "falta", 0, 0 },
            { Q_VAL,   "Vp", "V",   "Amplitud (de pico)", VM(ONDA_SENO), CF_NO_BUSCAR, "falta", 0, 0 },
            { Q_VAL,   "V" S_SUB2, "V", "Nivel alto", VM_NIV, CF_NO_BUSCAR, "falta", 0, 0 },
            { Q_BASE,  "CC", "V",   "Nivel de continua que se le suma", VM(ONDA_SENO), CF_NO_BUSCAR | CF_SIGNO, "0", -1e9, 1e9 },
            { Q_BASE,  "V" S_SUB1, "V", "Nivel bajo (también antes de t = 0)", VM_NIV, CF_NO_BUSCAR | CF_SIGNO, "0", -1e9, 1e9 },
            { Q_FREC,  "f",  "Hz",  "Frecuencia", VM_PER, CF_NO_BUSCAR, "falta", 1e-3, 1e9 },
            { Q_FCIRC, "f",  "Hz",  "Frecuencia (la de todo el circuito)", VM(FORMA_FASOR), 0, "falta", 1e-3, 1e9 },
            { Q_FASE,  S_THETA, S_DEG, "Fase: e = CC + Vp" S_CDOT "sen(2" S_PI "ft + " S_THETA ")", VM(ONDA_SENO), CF_NO_BUSCAR | CF_SIGNO, "0", -360, 360 },
            { Q_CICLO, "D",  "%",   "Parte del periodo en V2 (o subiendo)", VM_CT, CF_NO_BUSCAR, "50 %", 0, 100 },
            { Q_T0,    "t" S_SUB0, "s", "Cuándo empieza el pulso", VM(ONDA_PULSO), CF_NO_BUSCAR, "0", 0, 1e6 },
            { Q_ANCHO, S_DELTA "t", "s", "Ancho del pulso; vacío = escalón", VM(ONDA_PULSO), CF_NO_BUSCAR, "escalón", 0, 1e6 },
            { Q_I,     "I",  "A",   "Corriente total que entrega", VM(ONDA_CC), 0, 0, 0, 0 },
            { Q_I,     "I",  "A",   "Corriente que entrega (sale por +)", VM(FORMA_FASOR), FA, 0, 0, 0 },
            { Q_AUX,   "r",  S_OHM, "Resistencia interna; vacío = ideal (0)", 0, 0, 0, 0, 0 },
            { Q_V,     "V",  "V",   "Tensión en sus bornes (E - r·I)", VM(ONDA_CC), 0, 0, 0, 0 },
            { Q_V,     "V",  "V",   "Tensi" S_oo "n en sus bornes", VM(FORMA_FASOR), FA, 0, 0, 0 },
        },
        -1e9, 1e9, LEY_VALOR, 0, FORMA_N, NOM_FORMA,
        "v(+) - v(-) = e(t) - r" S_CDOT "i, con i la corriente que sale por +.\n"
        "Ideal (r vac" S_ii "a o 0): impone su tensi" S_oo "n.\n"
        "Real: una ideal con r en serie (el emulador usa\n"
        "su Norton: e/r en paralelo con r)."
    },
    {   /* EL_CORR: la corriente va de a hacia b por dentro (la flecha) */
        'J', 'J', "Fuente de corriente", "Fuente de corriente: flecha = sentido de J", C_FUENTE_I, 1, 1, 0, 0, 14,
        {
            { Q_VAL,   "J",  "A",  "Corriente que impone (la flecha)", VM(ONDA_CC), 0, 0, 0, 0 },
            { Q_VAL,   "J",  "A",  "Fasor: m" S_oo "dulo, y con [tab] el " S_aa "ngulo", VM(FORMA_FASOR), FA, "falta", 0, 0 },
            { Q_VAL,   "Ip", "A",  "Amplitud (de pico)", VM(ONDA_SENO), CF_NO_BUSCAR, "falta", 0, 0 },
            { Q_VAL,   "I" S_SUB2, "A", "Nivel alto", VM_NIV, CF_NO_BUSCAR, "falta", 0, 0 },
            { Q_BASE,  "CC", "A",  "Nivel de continua que se le suma", VM(ONDA_SENO), CF_NO_BUSCAR | CF_SIGNO, "0", -1e9, 1e9 },
            { Q_BASE,  "I" S_SUB1, "A", "Nivel bajo (también antes de t = 0)", VM_NIV, CF_NO_BUSCAR | CF_SIGNO, "0", -1e9, 1e9 },
            { Q_FREC,  "f",  "Hz", "Frecuencia", VM_PER, CF_NO_BUSCAR, "falta", 1e-3, 1e9 },
            { Q_FCIRC, "f",  "Hz", "Frecuencia (la de todo el circuito)", VM(FORMA_FASOR), 0, "falta", 1e-3, 1e9 },
            { Q_FASE,  S_THETA, S_DEG, "Fase: j = CC + Ip" S_CDOT "sen(2" S_PI "ft + " S_THETA ")", VM(ONDA_SENO), CF_NO_BUSCAR | CF_SIGNO, "0", -360, 360 },
            { Q_CICLO, "D",  "%",  "Parte del periodo en I2 (o subiendo)", VM_CT, CF_NO_BUSCAR, "50 %", 0, 100 },
            { Q_T0,    "t" S_SUB0, "s", "Cuándo empieza el pulso", VM(ONDA_PULSO), CF_NO_BUSCAR, "0", 0, 1e6 },
            { Q_ANCHO, S_DELTA "t", "s", "Ancho del pulso; vacío = escalón", VM(ONDA_PULSO), CF_NO_BUSCAR, "escalón", 0, 1e6 },
            { Q_V,     "V",  "V",  "Tensión entre sus bornes", VM(ONDA_CC), 0, 0, 0, 0 },
            { Q_V,     "V",  "V",  "Tensi" S_oo "n entre sus bornes", VM(FORMA_FASOR), FA, 0, 0, 0 },
        },
        -1e9, 1e9, LEY_VALOR, 0, FORMA_N, NOM_FORMA,
        "i = j(t), de a hacia b por dentro (la flecha).\n"
        "Impone su corriente, sea cual sea su tensi" S_oo "n."
    },
    {   /* EL_VOLT: ideal = no deja pasar corriente */
        'V', 'V', "Voltímetro", "Voltímetro: ¿hacia dónde? (flecha)", C_INSTR, 0, 0, 0, 1, 2,
        {
            { Q_V,   "V", "V",   "Lo que marca (tensión entre sus bornes)" },
            { Q_VAL, "R", S_OHM, "R interna; vacío = ideal (infinita)" },
        },
        1e-6, 1e9, LEY_VALOR, 0, MED_N, NOM_MED,
        "Mide v(a) - v(b).\n"
        "Ideal (R vac" S_ii "a): no deja pasar corriente.\n"
        "Con R: una resistencia R que adem" S_aa "s mide.\n"
        "Al emular marca, seg" S_uu "n su modo ([izq/der]):\n"
        " CC: la media;  CA: el eficaz de la parte\n"
        " alterna;  CA+CC: el eficaz total. En los\n"
        " " S_uu "ltimos periodos (o el " S_uu "ltimo 10 % del tiempo)."
    },
    {   /* EL_AMP: ideal = sin caída de tensión */
        'A', 'A', "Amperímetro", "Amperímetro: ¿hacia dónde? (flecha)", C_INSTR, 0, 0, 0, 1, 2,
        {
            { Q_I,   "I", "A",   "Lo que marca (corriente que lo cruza)" },
            { Q_VAL, "R", S_OHM, "R interna; vacío = ideal (cero)" },
        },
        1e-6, 1e9, LEY_VALOR, 0, MED_N, NOM_MED,
        "Mide la corriente de a hacia b.\n"
        "Ideal (R vac" S_ii "a): un cable (v = 0).\n"
        "Con R: una resistencia en serie que mide.\n"
        "Al emular marca, seg" S_uu "n su modo ([izq/der]):\n"
        " CC: la media;  CA: el eficaz de la parte\n"
        " alterna;  CA+CC: el eficaz total."
    },
    {   /* EL_COND: V0 = phi(a) - phi(b) al empezar (el + en a) */
        'C', 'C', "Condensador", "Condensador: flecha; el + (de V0) queda aqu" S_ii, C_COND, 1, 0, 0, 0, 4,
        {
            { Q_VAL, "C", "F", "Capacidad (100 [U] = 100 " S_MU "F)", 0, CF_NO_BUSCAR | CF_BUSCA_CA, "falta", 0, 0 },
            { Q_V0,  "V" S_SUB0, "V", "Tensi" S_oo "n al empezar (+ en a); vac" S_ii "a = la de CC",
              0, CF_NO_BUSCAR | CF_SIGNO, "de CC", -1e9, 1e9 },
            { Q_V,   "V", "V", "Tensi" S_oo "n en CC (para la calculadora)", 0, FA, 0, 0, 0 },
            { Q_I,   "I", "A", "Corriente (fasor)", 0, FA | CF_VER_CA, 0, 0, 0 },
        },
        1e-15, 1e6, LEY_ABIERTO, 1, 0, 0,
        "i = C" S_CDOT "dv/dt, con i de a hacia b.\n"
        "En CC es un abierto: i = 0.\n"
        "Carga q = C" S_CDOT "v;  energ" S_ii "a W = " "\xBD" S_CDOT "C" S_CDOT "v" S_SUP2 ".\n"
        "Su tensi" S_oo "n no salta sin un impulso de corriente.\n"
        "V0 es la tensi" S_oo "n al empezar (+ en a). Vac" S_ii "a: la\n"
        "del punto de operaci" S_oo "n de CC.\n"
        "El emulador usa el trapecio (paso h) y Euler\n"
        "impl" S_ii "cito justo despu" S_ee "s de cada salto."
    },
    {   /* EL_BOB: I0 de a hacia b al empezar */
        'L', 'L', "Bobina", "Bobina: flecha = sentido de su corriente", C_BOB, 1, 0, 0, 0, 4,
        {
            { Q_VAL, "L", "H", "Inductancia (10 [M] = 10 mH)", 0, CF_NO_BUSCAR | CF_BUSCA_CA, "falta", 0, 0 },
            { Q_I0,  "I" S_SUB0, "A", "Corriente al empezar (a" S_RARR "b); vac" S_ii "a = la de CC",
              0, CF_NO_BUSCAR | CF_SIGNO, "de CC", -1e9, 1e9 },
            { Q_I,   "I", "A", "Corriente en CC (para la calculadora)", 0, FA, 0, 0, 0 },
            { Q_V,   "V", "V", "Tensi" S_oo "n (fasor)", 0, FA | CF_VER_CA, 0, 0, 0 },
        },
        1e-12, 1e6, LEY_CORTO, 1, 0, 0,
        "v = L" S_CDOT "di/dt, con i de a hacia b.\n"
        "En CC es un cable: v = 0.\n"
        "Flujo " S_PHI " = L" S_CDOT "i;  energ" S_ii "a W = " "\xBD" S_CDOT "L" S_CDOT "i" S_SUP2 ".\n"
        "Su corriente no salta sin un impulso de tensi" S_oo "n.\n"
        "I0 es la corriente al empezar. Vac" S_ii "a: la del\n"
        "punto de operaci" S_oo "n de CC."
    },
    {   /* EL_INTER: la variante es el estado al empezar */
        'S', 'S', "Interruptor", "Interruptor: " "\xBF" "hacia d" S_oo "nde? (flecha)", C_INTER, 0, 0, 0, 0, 4,
        {
            { Q_T1, "t" S_SUB1, "s", "Cambia de estado en t1; vac" S_ii "o = nunca", 0, CF_NO_BUSCAR, "nunca", 0, 1e6 },
            { Q_T2, "t" S_SUB2, "s", "Y vuelve en t2; vac" S_ii "o = no vuelve", 0, CF_NO_BUSCAR, "no vuelve", 0, 1e6 },
            { Q_V,  "V", "V", "Tensi" S_oo "n entre sus bornes (abierto)", 0, FA, 0, 0, 0 },
            { Q_I,  "I", "A", "Corriente que lo cruza (cerrado)", 0, FA, 0, 0, 0 },
        },
        0, 0, LEY_INTER, 1, INTER_N, NOM_INTER,
        "Ideal: abierto, i = 0; cerrado, v = 0.\n"
        "Empieza abierto o cerrado ([izq/der]), cambia\n"
        "de estado en t1 y vuelve en t2.\n"
        "En la calculadora de CC cuenta su estado al\n"
        "empezar."
    },
    {   /* EL_IMP: la impedancia a la frecuencia del circuito. Tecla Z: el
         * editor mira [ctrl][Z] (deshacer) antes que ella */
        'Z', 'Z', "Impedancia", "Impedancia: " "\xBF" "hacia d" S_oo "nde? (flecha)", C_IMP, 0, 0, 0, 0, 3,
        {
            { Q_VAL, "Z", S_OHM, "R + jX: 30 [+] 40; o |Z|: 50 [tab] 36,87", 0, CF_ANG | CF_ANG90 | CF_CX, "falta", 0, 0 },
            { Q_V,   "V", "V",   "Tensi" S_oo "n entre sus extremos", 0, FA, 0, 0, 0 },
            { Q_I,   "I", "A",   "Corriente que la atraviesa", 0, FA, 0, 0, 0 },
        },
        1e-6, 1e9, LEY_CA, 0, 0, 0,
        "V = Z" S_CDOT "I con fasores; Z = |Z|" S_ANG S_PHI " = R + jX.\n"
        "Es la impedancia a la frecuencia del circuito f0.\n"
        "A otra f (el Bode) la X escala como una L (X > 0:\n"
        "X" S_CDOT "f/f0) o como una C (X < 0: X" S_CDOT "f0/f).\n"
        "Absorbe P = R" S_CDOT "I" S_SUP2 " y Q = X" S_CDOT "I" S_SUP2 " (I eficaz).\n"
        "No existe en el tiempo: para emularla, dib" S_uu "jala\n"
        "como R y L (o R y C)."
    },
    {   /* EL_VATI: bobina de corriente en serie (a -> b), bobina de tensión
         * de b a la referencia. Solo en la paleta: la W es el cable */
        'W', 0, "Vat" S_ii "metro", "Vat" S_ii "metro: flecha hacia la carga", C_INSTR, 1, 0, 0, 0, 2,
        {
            { Q_VAL, "P",  "W", "Lo que marca: P = V" S_CDOT "I (V de b a tierra)", 0, CF_SOLO_CA, "no se sabe", 0, 1e9 },
            { Q_AUX, "fp", "",  "Factor de potencia: + inductivo, - capacitivo", 0, CF_SOLO_CA | CF_SIGNO | CF_VER_CA, "no se sabe", -1.0, 1.0 },
        },
        0.0, 1e9, LEY_CORTO, 0, 0, 0,
        "Bobina de corriente: en serie, un cable (a" S_RARR "b).\n"
        "Bobina de tensi" S_oo "n: de b a la tierra (sin tierra,\n"
        "al borne - de la primera fuente).\n"
        "Marca P = V" S_CDOT "I: en CA, Re(V" S_CDOT "I*) con eficaces, y el\n"
        "fp = |P|/|S|: + inductivo si P y Q tienen el mismo\n"
        "signo; - capacitivo si no. Al emular en el tiempo,\n"
        "la media de v" S_CDOT "i en los " S_uu "ltimos periodos."
    },
    /* ---- Etapa 4: semiconductores y amplificadores. La tecla entra con su
     * interfaz (§7 de la especificación): D y U en el paso 11; Q y M siguen en 0
     * hasta los pasos 13 y 14 (hasta entonces solo existen en el modelo y en el
     * motor). Zener, LED y controladas no tienen tecla: van en la paleta ---- */
    {   /* EL_DIODO: a = ánodo, b = cátodo */
        'D', 'D', "Diodo", "Diodo: flecha hacia el c" S_aa "todo (la raya)", C_DIODO, 1, 0, 0, 0, 7,
        {
            { QD_VG, "V" S_GAMMA, "V", "Tensi" S_oo "n de codo: conduce a partir de ella", VM(DIO_VG), NB, "0,7 V", 0.0, 5.0 },
            { QD_RD, "rd", S_OHM, "Resistencia al conducir; vac" S_ii "a = 1 m" S_OHM, VM(DIO_VG), NB, "1 m" S_OHM, 0.0, 1e6 },
            { QD_IS, "Is", "A",   "Corriente de saturaci" S_oo "n",                  VM(DIO_EXP), NB, "10 fA", 1e-20, 1e-3 },
            { QD_N,  "n",  "",    "Factor de idealidad (de 1 a 2)",                   VM(DIO_EXP), NB, "1", 0.5, 10.0 },
            { QD_RS, "Rs", S_OHM, "Resistencia en serie; vac" S_ii "a = 0",           VM(DIO_EXP), NB, "0", 0.0, 1e3 },
            { Q_V,   "V",  "V",   "Tensi" S_oo "n " S_aa "nodo - c" S_aa "todo",      0, 0, 0, 0, 0 },
            { Q_I,   "I",  "A",   "Corriente de " S_aa "nodo a c" S_aa "todo",        0, 0, 0, 0, 0 },
        },
        0.0, 0.0, LEY_NL, 0, DIO_N, NOM_DIODO,
        "\xC1nodo a, c" S_aa "todo b (la raya): v = v(a) - v(b) e i\n"
        "de a hacia b. Conduce en directa.",
        0, MOD_DIODO
    },
    {   /* EL_ZENER */
        'D', 0, "Diodo zener", "Zener: flecha hacia el c" S_aa "todo (rompe en inversa)", C_DIODO, 1, 0, 0, 0, 6,
        {
            { QZ_VZ, "Vz", "V",   "Tensi" S_oo "n zener (en inversa, a Iz)",          VM(ZEN_LIBRO) | VM(ZEN_EXP), NB, "5,1 V", 0.5, 1000.0 },
            { QZ_RZ, "rz", S_OHM, "Resistencia en ruptura (a Iz)",                    VM(ZEN_LIBRO) | VM(ZEN_EXP), NB, "0", 0.0, 1e4 },
            { QZ_IZ, "Iz", "A",   "Corriente a la que se da Vz",                      VM(ZEN_LIBRO) | VM(ZEN_EXP), NB, "5 mA", 1e-6, 10.0 },
            { QZ_VG, "V" S_GAMMA, "V", "Tensi" S_oo "n de codo en directa",           VM(ZEN_LIBRO), NB, "0,7 V", 0.0, 5.0 },
            { Q_V, "V", "V", "Tensi" S_oo "n " S_aa "nodo - c" S_aa "todo (-: inversa)", 0, 0, 0, 0, 0 },
            { Q_I, "I", "A", "Corriente de " S_aa "nodo a c" S_aa "todo",             0, 0, 0, 0, 0 },
        },
        0.0, 0.0, LEY_NL, 0, ZEN_N, NOM_ZENER,
        "\xC1nodo a, c" S_aa "todo b (la raya con ganchos). En\n"
        "directa es un diodo; en inversa conduce desde Vz.",
        0, MOD_ZENER
    },
    {   /* EL_LED: siempre exponencial; la variante es el color */
        'D', 0, "LED", "LED: flecha hacia el c" S_aa "todo; se enciende", C_DIODO, 1, 0, 0, 0, 5,
        {
            { QL_VF, "Vf", "V", "Tensi" S_oo "n a If; vac" S_ii "a = la de su color", 0, NB, "del color", 0.5, 6.0 },
            { QL_IF, "If", "A", "Corriente de Vf y de brillo pleno",                  0, NB, "20 mA", 1e-6, 10.0 },
            { QL_IM, "Im", "A", "M" S_aa "xima: con m" S_aa "s, se quemar" S_ii "a",  0, NB, "30 mA", 1e-6, 100.0 },
            { Q_V, "V", "V", "Tensi" S_oo "n " S_aa "nodo - c" S_aa "todo",           0, 0, 0, 0, 0 },
            { Q_I, "I", "A", "Corriente de " S_aa "nodo a c" S_aa "todo",             0, 0, 0, 0, 0 },
        },
        0.0, 0.0, LEY_NL, 0, LED_N, NOM_LED,
        "Un diodo que da luz: " S_aa "nodo a, c" S_aa "todo b.\n"
        "Su color fija Vf a If (20 mA): rojo 1,8 V,\n"
        "naranja 2,0, amarillo 2,1, verde 2,2, azul y\n"
        "blanco 3,2. Es Shockley con la Is que pasa justo\n"
        "por (Vf, If): con R = (E - Vf)/If lleva If.\n"
        "Brillo: la corriente media (lo que ve el ojo), en\n"
        "8 niveles; apagado por debajo de If/100. Con\n"
        "m" S_aa "s de Im, en la vida real se quemar" S_ii "a.",
        0, 0
    },
    {   /* EL_EDEP: la ganancia cambia de nombre y unidad con la variante (el mismo hueco) */
        'E', 0, "E controlada", "E controlada: flecha hacia el borne +", C_FUENTE, 1, 0, 0, 0, 5,
        {
            { Q_VAL,  S_MU, "",    "E = " S_MU S_CDOT "V(control)",                   VM(DEP_V), SG, "falta", 0, 0 },
            { Q_VAL,  "r",  S_OHM, "E = r" S_CDOT "I(control)",                       VM(DEP_I), SG, "falta", 0, 0 },
            { Q_CTRL, "x",  "",    "Qui" S_ee "n la manda: [+][-] elige el elemento", 0, NB, "falta", 0, 0 },
            { Q_V,    "V",  "V",   "Tensi" S_oo "n en sus bornes (+ en b)",           0, FA, 0, 0, 0 },
            { Q_I,    "I",  "A",   "Corriente que entrega (sale por +)",              0, FA, 0, 0, 0 },
        },
        -1e9, 1e9, LEY_DEP, 0, DEP_N, NOM_DEP,
        "Rombo: su tensi" S_oo "n la manda otro elemento de dos\n"
        "bornes (el control: fila x de la ficha).\n"
        "Por una V: e = " S_MU S_CDOT "V(x); por una I: e = r" S_CDOT "I(x).\n"
        "V e I del control, las de su tabla (su criterio).\n"
        "Es lineal: la calculadora despeja su ganancia.",
        0, 0
    },
    {   /* EL_JDEP */
        'J', 0, "J controlada", "J controlada: flecha = sentido de J", C_FUENTE_I, 1, 0, 0, 0, 5,
        {
            { Q_VAL,  "g",    "S", "J = g" S_CDOT "V(control)",                       VM(DEP_V), SG, "falta", 0, 0 },
            { Q_VAL,  S_BETA, "",  "J = " S_BETA S_CDOT "I(control)",                 VM(DEP_I), SG, "falta", 0, 0 },
            { Q_CTRL, "x",    "",  "Qui" S_ee "n la manda: [+][-] elige el elemento", 0, NB, "falta", 0, 0 },
            { Q_V,    "V",    "V", "Tensi" S_oo "n en sus bornes (+ en b)",           0, FA, 0, 0, 0 },
            { Q_I,    "I",    "A", "Corriente que entrega (sale por b)",              0, FA, 0, 0, 0 },
        },
        -1e9, 1e9, LEY_DEP, 0, DEP_N, NOM_DEP,
        "Rombo: su corriente la manda otro elemento de dos\n"
        "bornes (el control: fila x de la ficha).\n"
        "Por una V: j = g" S_CDOT "V(x); por una I: j = " S_BETA S_CDOT "I(x).\n"
        "V e I del control, las de su tabla (su criterio).\n"
        "Es lineal: la calculadora despeja su ganancia.",
        0, 0
    },
    {   /* EL_AO: a = +, b = -, c = salida (respecto a la referencia) */
        'U', 'U', "Amp. operacional", "Amp. op.: flecha hacia la salida (el - aqu" S_ii ")", C_AO, 1, 0, 0, 0, 7,
        {
            { QA_A,  "A",  "",    "Ganancia en lazo abierto",                         VM(AO_REAL), NB, "100 000", 1.0, 1e9 },
            { QA_VP, "V+", "V",   "La salida no pasa de V+ (satura)",                 0, NB | SG, "15 V", -1e3, 1e3 },
            { QA_VN, "V-", "V",   "La salida no baja de V- (satura)",                 0, NB | SG, "-15 V", -1e3, 1e3 },
            { QA_RI, "Ri", S_OHM, "Resistencia entre + y -; vac" S_ii "a = infinita", VM(AO_REAL), NB, "infinita", 1.0, 1e15 },
            { QA_RO, "Ro", S_OHM, "Resistencia de salida; vac" S_ii "a = 0",          VM(AO_REAL), NB, "0", 0.0, 1e6 },
            { Q_V,   "V",  "V",   "Vsal: salida respecto a la tierra",                0, 0, 0, 0, 0 },
            { Q_I,   "I",  "A",   "Corriente que entrega la salida",                  0, 0, 0, 0, 0 },
        },
        0.0, 0.0, LEY_NL, 0, AO_N, NOM_AO,
        "Entradas + (a) y - (b); salida respecto a la\n"
        "tierra (la de su alimentaci" S_oo "n, que no se dibuja).\n"
        "La salida no pasa de V+ ni baja de V- (satura).\n"
        "Con realimentaci" S_oo "n negativa trabaja en lineal;\n"
        "con positiva, o sin ella, salta a V+ o a V-.",
        &H_AO, MOD_AO
    },
    {   /* EL_BJT: a = C, b = E, c = B */
        'Q', 'Q', "Transistor BJT", "Transistor: flecha de la base a C y E", C_TRANS, 1, 0, 0, 0, 6,
        {
            { QB_BF, S_BETA, "",  "Ganancia de corriente (IC/IB en activa)",          0, NB, "100", 1.0, 1e5 },
            { QB_IS, "Is", "A",   "Corriente de saturaci" S_oo "n",                   0, NB, "1 fA", 1e-20, 1e-6 },
            { QB_VA, "VA", "V",   "Tensi" S_oo "n de Early; vac" S_ii "a = sin Early", 0, NB, "infinita", 1.0, 1e4 },
            { QB_BR, S_BETA "r", "", "Ganancia inversa (C y E cambiados)",            0, NB, "1", 1e-3, 1e3 },
            { Q_V,   "V",  "V",   "VCE: colector menos emisor",                       0, 0, 0, 0, 0 },
            { Q_I,   "I",  "A",   "IC: la que entra por el colector",                 0, 0, 0, 0, 0 },
        },
        0.0, 0.0, LEY_NL, 0, BJT_N, NOM_BJT,
        "C, B, E. NPN: la flecha del emisor sale; PNP:\n"
        "entra. Ebers-Moll: iF = Is" S_CDOT "(exp(vBE/VT) - 1), iR\n"
        "igual con vBC; iC = (iF - iR)" S_CDOT "(1 - vBC/VA) - iR/" S_BETA "r;\n"
        "iB = iF/" S_BETA " + iR/" S_BETA "r. VA vac" S_ii "a: sin Early.\n"
        "En la ficha, V = VCE e I = IC; la tabla da VBE\n"
        "e IB. En PNP salen negativas en activa.\n"
        "En CA: gm = IC/VT, r" S_PI " = " S_BETA "/gm; sin capacidades.",
        &H_BJT, 0
    },
    {   /* EL_MOS: a = D, b = S, c = G */
        'M', 'M', "MOSFET", "MOSFET: flecha de la puerta G a D y S", C_TRANS, 1, 0, 0, 0, 5,
        {
            { QM_VT, "Vt", "V",   "Umbral: conduce con |VGS| > Vt",                   0, NB, "2 V", 0.0, 100.0 },
            { QM_K,  "K",  "A/V" S_SUP2, "ID = K" S_CDOT "(VGS - Vt)" S_SUP2 " saturado", 0, NB, "1 mA/V" S_SUP2, 1e-9, 1e3 },
            { QM_VA, "VA", "V",   "Modulaci" S_oo "n del canal; vac" S_ii "a = sin ella", 0, NB, "infinita", 1.0, 1e4 },
            { Q_V,   "V",  "V",   "VDS: drenador menos surtidor",                     0, 0, 0, 0, 0 },
            { Q_I,   "I",  "A",   "ID: la que entra por el drenador",                 0, 0, 0, 0, 0 },
        },
        0.0, 0.0, LEY_NL, 0, MOS_N, NOM_MOS,
        "D, G, S; de acumulaci" S_oo "n, cuerpo unido a S.\n"
        "Nivel 1, con vov = |vGS| - Vt:\n"
        "corte (vov " S_LE " 0): iD = 0;\n"
        S_oo "hmica (vDS < vov): iD = K" S_CDOT "(2" S_CDOT "vov" S_CDOT "vDS - vDS" S_SUP2 ");\n"
        "saturaci" S_oo "n: iD = K" S_CDOT "vov" S_SUP2 "; las dos por (1 + vDS/VA).\n"
        "Canal P: signos al rev" S_ee "s. Sim" S_ee "trico: con vDS < 0\n"
        "cambian D y S. La puerta no lleva corriente.",
        &H_MOS, 0
    },
};

const TipoInfo *tipo_info(int tipo)
{
    return &TIPOS[(tipo >= 0 && tipo < EL_N) ? tipo : EL_RES];
}

const char *tipo_nom_val(int tipo) { const CampoInfo *c = tipo_campo(tipo, Q_VAL); return c ? c->nombre : "R"; }
const char *tipo_uni_val(int tipo) { const CampoInfo *c = tipo_campo(tipo, Q_VAL); return c ? c->unidad : "?"; }

const char *const NOM_FORMA[FORMA_N] = {
    "continua", "senoidal", "cuadrada", "triangular", "pulso/escal" S_oo "n", "alterna (fasor)"
};

const char *const FORMA_MODELO[FORMA_N] = {
    "\nContinua: vale siempre lo mismo (tambi" S_ee "n antes de\nt = 0).",
    "\nSenoidal: CC + Vp" S_CDOT "sen(2" S_PI S_CDOT "f" S_CDOT "t + " S_THETA ").\n"
    "Antes de t = 0 (en reposo) vale CC: se enciende en\nt = 0, como un generador.",
    "\nCuadrada: V2 durante D" S_CDOT "T al principio de cada\n"
    "periodo (T = 1/f) y V1 el resto. Antes de t = 0: V1.",
    "\nTriangular: sube de V1 a V2 en D" S_CDOT "T y baja en\n"
    "(1 - D)" S_CDOT "T. D = 50 %: sim" S_ee "trica; 100 %: diente de\nsierra. Antes de t = 0: V1.",
    "\nPulso: V1, y V2 entre t0 y t0 + " S_DELTA "t. Con " S_DELTA "t vac" S_ii "o\n"
    "es un escal" S_oo "n (V2 para siempre desde t0).",
    "\nAlterna: el fasor E" S_ANG S_THETA " a la f del circuito. Con el\n"
    "convenio por omisi" S_oo "n (eficaz y coseno):\n"
    "e(t) = " S_SQRT "2" S_CDOT "E" S_CDOT "cos(2" S_PI "ft + " S_THETA "). En el tiempo es la\n"
    "senoidal de pico " S_SQRT "2" S_CDOT "E y fase " S_THETA " + 90" S_DEG ",\n"
    "encendida en t = 0.",
};

const CampoInfo *tipo_campo_v(int tipo, int variante, int q)
{
    const TipoInfo *t = tipo_info(tipo);
    int i;
    for (i = 0; i < t->ncampos; i++)
        if (t->campos[i].q == q && (t->campos[i].vars == 0 ||
            (variante >= 0 && variante < 8 && (t->campos[i].vars & VM(variante)))))   /* vars: 8 bits */
            return &t->campos[i];
    return 0;
}

void tipo_rango(int tipo, int q, double *lo, double *hi)
{
    const TipoInfo *t = tipo_info(tipo);
    int i;
    *lo = *hi = 0.0;
    if (q == Q_VAL) { *lo = t->val_min; *hi = t->val_max; return; }
    if (q == Q_FCIRC) { *lo = 1e-3; *hi = 1e9; return; }
    if (q == Q_AUX) {
        const CampoInfo *c = tipo_campo(tipo, Q_AUX);
        if (c && !(c->min == 0.0 && c->max == 0.0)) { *lo = c->min; *hi = c->max; }
        else { *lo = 0.0; *hi = 1e9; }
        return;
    }
    if (q == Q_V || q == Q_I) { *lo = -1e9; *hi = 1e9; return; }
    for (i = 0; i < t->ncampos; i++)
        if (t->campos[i].q == q) { *lo = t->campos[i].min; *hi = t->campos[i].max; return; }
}

const CampoInfo *tipo_campo(int tipo, int q)
{
    const TipoInfo *t = tipo_info(tipo);
    int i;
    for (i = 0; i < t->ncampos; i++) if (t->campos[i].q == q) return &t->campos[i];
    return 0;
}
