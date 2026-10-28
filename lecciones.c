/* ============================================================================
 *  lecciones.c  -  La teoría de "Aprende"  (ver lecciones.h)
 *
 *  Presupuesto de cada página (lo exige el arnés): 52 columnas; 9 líneas si
 *  lleva figura, 15 si no. El texto está en UTF-8 y llega a la pantalla en
 *  Latin-1; lo que no es Latin-1 (ohmio, flechas, paralelo) va con macros.
 * ========================================================================= */
#include "lecciones.h"
#include "ejemplos.h"
#include "gfx.h"

#define O   S_OHM
#define FL  S_RARR
#define PAR S_PAR

/* ---- 1. Ley de Ohm ---- */
static const Pagina P_OHM[] = {
    { "Tres magnitudes:\n"
      "  V  tensión, en voltios (V)\n"
      "  I  corriente, en amperios (A)\n"
      "  R  resistencia, en ohmios (" O ")\n"
      "\n"
      "Se relacionan con la ley de Ohm:\n"
      "            V = I · R",
      FIG_TRIANGULO, -1 },
    { "El triángulo: tapa con el dedo lo que buscas.\n"
      "· Tapas V: quedan I y R lado a lado  " FL " V = I·R\n"
      "· Tapas I: queda V sobre R           " FL " I = V/R\n"
      "· Tapas R: queda V sobre I           " FL " R = V/I\n"
      "\n"
      "Ejemplo: 12\240V y 4\240" O " " FL " I = 12/4 = 3\240A",
      FIG_TRIANGULO, -1 },
    { "Despejar sin triángulo.\n"
      "Parte de V = I · R. Para sacar R, divide los dos\n"
      "lados entre I:\n"
      "     V / I = I · R / I     " FL "     R = V / I\n"
      "Para sacar I, divide entre R:   I = V / R\n"
      "\n"
      "Unidades: 1\240k" O " = 1000\240" O ";  1\240mA = 0,001\240A.\n"
      "Con voltios y kilohmios, la corriente sale en mA:\n"
      "5\240V / 2,2\240k" O " = 2,27\240mA.\n"
      "\n"
      "En la calculadora, 2,2\240k" O " se escribe 2.2 y la\n"
      "tecla K, o 2.2 EE 3.",
      FIG_NADA, -1 },
};

/* ---- 2. Serie ---- */
static const Pagina P_SERIE[] = {
    { "En SERIE las resistencias están una detrás de\n"
      "otra, en un solo camino.\n"
      "· La corriente es LA MISMA en todas.\n"
      "· Las tensiones se reparten y SUMAN la de la\n"
      "  fuente:  E = V1 + V2 + ...",
      FIG_CIRCUITO, EJ_S_DOS },
    { "Resistencia equivalente:\n"
      "        Req = R1 + R2 + R3 + ...\n"
      "Siempre es mayor que la mayor de todas.\n"
      "\n"
      "Pasos para resolver:\n"
      "1. Req = la suma de todas\n"
      "2. I = E / Req  (la misma en todas)\n"
      "3. La tensión de cada una: V = I · R\n"
      "4. Comprueba: V1 + V2 + ... = E\n"
      "\n"
      "Ejemplo: 12\240V con 2\240" O " y 4\240" O ":\n"
      "Req = 6\240" O ", I = 2\240A, V1 = 4\240V, V2 = 8\240V.",
      FIG_NADA, -1 },
    { "Divisor de tensión.\n"
      "La tensión se reparte en proporción a cada R:\n"
      "        V2 = E · R2 / (R1 + R2)\n"
      "La resistencia MAYOR se lleva MÁS tensión.\n"
      "\n"
      "Ejemplo: 9\240V con 1\240k" O " y 2\240k" O " en serie:\n"
      "        V2 = 9 · 2 / 3 = 6\240V",
      FIG_CIRCUITO, EJ_S_DIV },
};

/* ---- 3. Paralelo ---- */
static const Pagina P_PARALELO[] = {
    { "En PARALELO están conectadas entre los MISMOS\n"
      "dos nodos: cada una es un camino distinto.\n"
      "· La tensión es LA MISMA en todas.\n"
      "· Las corrientes se reparten y SUMAN la total:\n"
      "  I = I1 + I2 + ...",
      FIG_CIRCUITO, EJ_P_DOS },
    { "Resistencia equivalente:\n"
      "     1/Req = 1/R1 + 1/R2 + 1/R3 + ...\n"
      "Atajos:\n"
      "· Dos:  Req = R1·R2 / (R1 + R2)\n"
      "· n iguales de valor R:  Req = R / n\n"
      "\n"
      "Req es siempre MENOR que la menor de todas:\n"
      "cada rama nueva es otro camino para la corriente.\n"
      "\n"
      "Ejemplo: 6\240" O " " PAR " 3\240" O " = 18/9 = 2\240" O ".",
      FIG_NADA, -1 },
    { "Pasos para resolver:\n"
      "1. Req con la fórmula\n"
      "2. I total = E / Req\n"
      "3. Cada rama tiene la tensión E: I = E / R\n"
      "4. Comprueba: I1 + I2 + ... = I total\n"
      "\n"
      "Divisor de corriente (dos ramas):\n"
      "        I1 = I · R2 / (R1 + R2)\n"
      "La rama de MENOR resistencia lleva MÁS corriente.",
      FIG_NADA, -1 },
};

/* ---- 4. Mixtos ---- */
static const Pagina P_MIXTO[] = {
    { "Casi todos los problemas mezclan serie y\n"
      "paralelo. Se resuelven reduciendo de dentro\n"
      "hacia fuera, hasta que queda una sola\n"
      "resistencia, y luego se vuelve hacia atrás.",
      FIG_CIRCUITO, EJ_M_SP },
    { "Cómo ver qué está en serie y qué en paralelo:\n"
      "\n"
      "1. Todos los puntos unidos por cable son el\n"
      "   MISMO nodo, aunque en el dibujo estén lejos.\n"
      "2. Dos resistencias están en PARALELO si\n"
      "   comparten sus DOS nodos.\n"
      "3. Están en SERIE si entre ellas hay un nodo\n"
      "   al que no llega nada más.\n"
      "\n"
      "Por eso una fuente dibujada en medio de las\n"
      "ramas no cambia nada: sus dos bornes son los\n"
      "mismos dos nodos que las ramas.",
      FIG_NADA, -1 },
    { "Pasos para resolver:\n"
      "1. Reduce los grupos más internos: la serie de\n"
      "   una rama, o un paralelo pequeño.\n"
      "2. Sigue hasta tener Req.\n"
      "3. I total = E / Req.\n"
      "4. Vuelve hacia atrás: en serie se reparten las\n"
      "   tensiones; en paralelo, las corrientes.\n"
      "\n"
      "En el procedimiento del Taller, cada paso\n"
      "resalta en el dibujo el grupo que se reduce.",
      FIG_NADA, -1 },
};

/* ---- 5. Despejar ---- */
static const Pagina P_DESPEJAR[] = {
    { "A veces lo desconocido es una resistencia.\n"
      "La idea: con los datos de la fuente se saca\n"
      "        Req = E / I\n"
      "y luego se deshace la reducción, despejando.\n"
      "\n"
      "En serie:     Req = R1 + Rx   " FL "  Rx = Req - R1\n"
      "\n"
      "En paralelo:  1/Req = 1/R1 + 1/Rx\n"
      "         " FL "  1/Rx = 1/Req - 1/R1\n"
      "         " FL "  Rx = 1 / (1/Req - 1/R1)",
      FIG_NADA, -1 },
    { "Ejemplo: 12\240V, la fuente da 3\240A; 6\240" O " en\n"
      "paralelo con Rx.\n"
      "   Req = 12 / 3 = 4\240" O "\n"
      "   1/Rx = 1/4 - 1/6 = 3/12 - 2/12 = 1/12\n"
      "   Rx = 12\240" O "\n"
      "\n"
      "Comprueba: 6 " PAR " 12 = 72/18 = 4\240" O " " S_CHECK,
      FIG_CIRCUITO, EJ_P_R },
    { "Si el dato está en otra resistencia, se avanza\n"
      "paso a paso usando en cada uno UNA sola cosa\n"
      "desconocida:\n"
      "· Ohm en la resistencia que tiene dos datos\n"
      "· en serie, la misma corriente\n"
      "· en paralelo, la misma tensión\n"
      "· y las sumas de tensiones o de corrientes.\n"
      "\n"
      "LA REGLA: hacen falta tantos datos (de V o de\n"
      "I) como valores (de R o de E) desconocidos.\n"
      "En el Taller, la línea de abajo lo va contando.",
      FIG_NADA, -1 },
};

/* ---- 6. Potencia ---- */
static const Pagina P_POTENCIA[] = {
    { "La potencia es la energía por segundo que da la\n"
      "fuente o que una resistencia convierte en calor.\n"
      "Se mide en vatios (W).\n"
      "        P = V · I\n"
      "Con la ley de Ohm también:\n"
      "        P = I² · R     y     P = V² / R",
      FIG_POTENCIA, -1 },
    { "¿Cuál disipa más?\n"
      "· En SERIE (misma I), P = I²·R:\n"
      "  disipa más la resistencia MAYOR.\n"
      "· En PARALELO (misma V), P = V²/R:\n"
      "  disipa más la resistencia MENOR.\n"
      "\n"
      "Balance: lo que entrega la fuente es igual a\n"
      "la suma de lo que disipan las resistencias.\n"
      "El programa lo comprueba al final de cada\n"
      "procedimiento. En el resultado, la tabla ([tab])\n"
      "muestra la potencia de cada elemento.",
      FIG_NADA, -1 },
};

/* ---- 7. Kirchhoff ---- */
static const Pagina P_KIRCHHOFF[] = {
    { "Un puente, o dos fuentes en ramas distintas,\n"
      "NO se reducen con serie y paralelo. Entonces\n"
      "se usan las leyes de Kirchhoff:\n"
      "· Nodos: lo que entra = lo que sale.\n"
      "· Mallas: en una vuelta cerrada, las subidas\n"
      "  y bajadas de tensión suman cero.",
      FIG_NODO, -1 },
    { "El Taller escribe una ecuación por nodo y la\n"
      "ley de Ohm de cada resistencia, con los\n"
      "números, y resuelve el sistema.\n"
      "\n"
      "Puente equilibrado: si R1/R3 = R2/R4, los dos\n"
      "nodos del medio tienen la misma tensión y por\n"
      "la resistencia del medio no pasa corriente.\n"
      "\n"
      "Con dos fuentes, una puede estar CARGÁNDOSE: la\n"
      "corriente le entra por el borne +.",
      FIG_NADA, -1 },
};

/* ---- Ejemplos de cada lección ---- */
static const Pagina P_REAL[] = {
    { "FUENTE DE CORRIENTE (J): impone la corriente de\n"
      "su rama, en el sentido de su flecha. Su tensión\n"
      "la decide el resto del circuito.\n"
      "\n"
      "Con una sola J se razona al revés que con E:\n"
      "   V = J" S_CDOT "Req   y luego se reparte como\n"
      "siempre. Lo que va en serie con J lleva J.\n"
      "\n"
      "En el Taller: [J] y una flecha (su sentido).", FIG_NADA, 0 },
    { "FUENTE REAL: una E ideal con una resistencia r\n"
      "dentro, en serie. Con corriente, en los bornes\n"
      "queda menos que E:\n"
      "   V = E - r" S_CDOT "I\n"
      "La r se trata como una resistencia más:\n"
      "   I = E / (r + Req)\n"
      "y dentro se pierde r" S_CDOT "I" S_SUP2 ".\n"
      "\n"
      "En la ficha de la fuente: r (vacío = ideal).", FIG_NADA, 0 },
    { "AMPERÍMETRO (A): se pone en SERIE. Ideal: sin\n"
      "caída de tensión, como un cable.\n"
      "VOLTÍMETRO (V): se pone en PARALELO con lo que\n"
      "mide. Ideal: no deja pasar corriente.\n"
      "\n"
      "Con su R interna escrita dejan de ser ideales:\n"
      "un voltímetro de 10 k" O " sobre una R de 10 k" O "\n"
      "marca MENOS de lo que habría sin él.\n"
      "Un amperímetro en paralelo con una fuente la\n"
      "pone en cortocircuito (se avisa).", FIG_NADA, 0 },
    { "TIERRA ([G] en el Taller): el punto que vale\n"
      "0\240V. Los potenciales de los nodos se miden\n"
      "desde ahí: capa \"Potenciales\" del resultado.\n"
      "Sin tierra, 0\240V es el borne - de la fuente.\n"
      "\n"
      "TERMINALES ([T], dos veces): A y B. El resultado\n"
      "da la tensión entre ellos:\n"
      "   V(AB) = V(A) - V(B)", FIG_NADA, 0 },
};

/* ---- 10. Circuitos en el tiempo ---- */
static const Pagina P_TIEMPO[] = {
    { "Con C, L, interruptores o fuentes que varían, el\n"
      "circuito cambia con el tiempo: [=] lo EMULA, como\n"
      "un laboratorio. Con [?!] en algo, [=] calcula en CC.\n"
      "\n"
      "C: i = C·dv/dt. Su tensión no salta; en CC es un\n"
      "   abierto. V0 es la tensión al empezar (+ en a).\n"
      "L: v = L·di/dt. Su corriente no salta; en CC es\n"
      "   un cable. I0 es la corriente al empezar.\n"
      "S: interruptor; [izq/der] en su ficha: cómo empieza.\n"
      "   t1 y t2: cuándo cambia.\n"
      "E y J: [izq/der] elige la forma: continua,\n"
      "   senoidal, cuadrada, triangular o pulso.\n"
      "\n"
      "Constante de tiempo: RC, o L/R. En 5·RC se llega\n"
      "al régimen permanente.",
      FIG_NADA, -1 },
    { "El osciloscopio enseña dos canales. Con [O] en el\n"
      "Taller se pone una sonda: [enter], la tensión de\n"
      "un punto; una flecha, la corriente o la tensión\n"
      "de un elemento. Sin sondas, el emulador elige.\n"
      "\n"
      "Teclas: [+][-] tiempo por división, [arr][ab]\n"
      "voltios por división, [X] cursores (escribe un\n"
      "tiempo: 1 [M] es 1 ms), [P] el siguiente pico,\n"
      "[D] disparo, [A] autoescala, [tab] la tabla.\n"
      "\n"
      "La tabla da lo de t final, los máximos, los\n"
      "eficaces, las energías con su balance y la\n"
      "respuesta natural: s = -" S_ALFA " " S_PM " j" S_OMEGA "d.\n"
      "El multímetro (V y A) mide en CC, CA o eficaz.",
      FIG_NADA, -1 },
};
static const signed char M_TIEMPO[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

/* ---- 11. Corriente alterna ---- */
static const Pagina P_ALTERNA[] = {
    { "Con una fuente en \"alterna (fasor)\" ([izq] en su\n"
      "ficha), [=] resuelve el circuito con FASORES a una\n"
      "frecuencia: V" S_ANG S_THETA " (eficaz, referido al coseno).\n"
      "\n"
      "Z: R;  L: j" S_OMEGA "L;  C: 1/(j" S_OMEGA "C) = -j/(" S_OMEGA "C).\n"
      "V = Z" S_CDOT "I, y Kirchhoff igual que en CC.\n"
      "\n"
      "Potencia (eficaces): S = V" S_CDOT "I* = P + jQ.\n"
      "P se disipa en las R; Q va y viene en L (+) y C (-).\n"
      "fp = P/|S|: inductivo si Q > 0, capacitivo si Q < 0.\n"
      "\n"
      "[.] polar o rectangular; [tab] la tabla; [enter] el\n"
      "diagrama fasorial; [T] Th" S_ee "venin entre A y B.",
      FIG_NADA, -1 },
    { "[B] en el editor traza el BODE: |H| en dB y la fase\n"
      "de una fuente (a 1" S_ANG "0) a una sonda o a V(AB).\n"
      "\n"
      "Pasa-bajas RC: fc = 1/(2" S_PI "RC), -20 dB por d" S_ee "cada.\n"
      "Pasa-altas RL: fc = R/(2" S_PI "L). En fc: -3 dB y " S_PM "45" S_DEG ".\n"
      "RLC serie: f0 = 1/(2" S_PI S_SQRT "LC); BW = R/(2" S_PI "L); Q = f0/BW.\n"
      "En resonancia XL = XC y solo queda R.\n"
      "\n"
      "Correcci" S_oo "n del fp: una C en paralelo con la carga,\n"
      "   C = P" S_CDOT "(tan " S_PHI "1 - tan " S_PHI "2)/(" S_OMEGA S_CDOT "V" S_SUP2 ")\n"
      "La calculadora de CA la despeja: [?!] en la C y el\n"
      "fp que se quiere en un vat" S_ii "metro de la l" S_ii "nea.",
      FIG_NADA, -1 },
};
static const signed char M_ALTERNA[] = { 12, 13, 14, 15, 16, 17, 18, 19, 20, 21 };

/* ---- 12. Diodos ---- */
static const Pagina P_DIODOS[] = {
    { "El DIODO deja pasar la corriente en un solo\n"
      "sentido: del " S_aa "nodo a (el tri" S_aa "ngulo) al c" S_aa "todo b\n"
      "(la raya). En el Taller, [D] y una flecha hacia\n"
      "el c" S_aa "todo. Sus modelos, de menos a m" S_aa "s fino:\n"
      S_CDOT " ideal: un interruptor; conduce si i > 0.\n"
      S_CDOT " con V" S_GAMMA " (el codo, 0,7\240V en silicio): no\n"
      "  conduce hasta v = V" S_GAMMA "; luego es una fuente de V" S_GAMMA ".\n"
      S_CDOT " exponencial (Shockley): i = Is" S_CDOT "(e^(v/nVT) - 1),\n"
      "  con VT = kT/q = 25,9\240mV a 27\240" S_DEG "C.\n"
      "\n"
      "La curva I-V: casi nada hasta el codo y luego\n"
      "sube casi en vertical (" S_TIMES "10 cada 60\240mV con n = 1).\n"
      "Resistencia din" S_aa "mica: rd = n" S_CDOT "VT/I (26\240" O " a 1\240mA).\n"
      "[=] halla su punto de operaci" S_oo "n (Newton) y [I]\n"
      "traza la curva, barriendo una E o una R.",
      FIG_NADA, -1 },
    { "RECTIFICAR: media onda (un diodo), onda completa\n"
      "(dos, con toma central) o puente (cuatro: pierde\n"
      "dos codos). Con C en paralelo con la carga, C se\n"
      "carga en el pico y se descarga por R hasta el\n"
      "siguiente: rizado Vr " S_APROX " Vp/(f" S_CDOT "R" S_CDOT "C), con f la del\n"
      "rizado (el doble en onda completa). La f" S_oo "rmula\n"
      "supone que C se descarga todo el periodo y en\n"
      "recta: de verdad es menos tiempo y exponencial,\n"
      "as" S_ii " que el emulador da algo menos.\n"
      "RECORTAR: un diodo con una E en serie no deja\n"
      "pasar de E + V" S_GAMMA ". FIJAR: C y un diodo suben o\n"
      "bajan la onda entera (C guarda la diferencia).\n"
      "ZENER: en inversa aguanta Vz; con Rs en serie\n"
      "regula la tensi" S_oo "n de la carga. LED: un diodo que\n"
      "da luz; con R = (E - Vf)/If lleva su If.",
      FIG_NADA, -1 },
};
static const signed char M_DIODOS[] = { 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };

/* ---- 13. AO y transistores (el 41 y el 42: el BJT) ---- */
static const Pagina P_AO[] = {
    { "El AMPLIFICADOR OPERACIONAL ([U]) amplifica la\n"
      "diferencia entre la + y la -; su salida no pasa\n"
      "de V+ ni baja de V- (satura en " S_PM "15\240V).\n"
      "Con realimentaci" S_oo "n negativa (de la salida a la\n"
      "-), la - copia a la +: si la + va a tierra, la -\n"
      "es una MASA VIRTUAL. A las entradas no entra\n"
      "corriente.\n"
      S_CDOT " inversor: Vsal = -(Rf/R1)" S_CDOT "E\n"
      S_CDOT " no inversor: Vsal = (1 + R2/R1)" S_CDOT "E\n"
      S_CDOT " sumador: Vsal = -Rf" S_CDOT "(E1/R1 + E2/R2)\n"
      S_CDOT " integrador (C en la realimentaci" S_oo "n) y derivador\n"
      "  (C a la entrada): -" S_INT "e dt/(RC) y -RC" S_CDOT "de/dt\n"
      "Sin realimentaci" S_oo "n, COMPARADOR; con positiva, un\n"
      "Schmitt; y con R y C a la -, un ASTABLE:\n"
      "  f = 1/(2RC" S_CDOT "ln(1 + 2R2/R1)).",
      FIG_NADA, -1 },
    { "FUENTES CONTROLADAS (el rombo, en la paleta): su\n"
      "valor lo manda la V o la I de otro elemento (x):\n"
      "e = " S_MU S_CDOT "V(x) o r" S_CDOT "I(x); j = g" S_CDOT "V(x) o " S_BETA S_CDOT "I(x).\n"
      "El TRANSISTOR BJT ([Q]; C, B y E) es casi una J\n"
      "controlada por su base. NPN con VBE " S_APROX " 0,7\240V:\n"
      S_CDOT " ACTIVA: IC = " S_BETA S_CDOT "IB (VCE de m" S_aa "s de 0,3\240V);\n"
      S_CDOT " CORTE: VBE baja, IB = 0 y no conduce;\n"
      S_CDOT " SATURACIÓN: IB sobra, VCE " S_APROX " 0,1\240V: un interruptor\n"
      "  cerrado (IC < " S_BETA S_CDOT "IB). El PNP, con signos al rev" S_ee "s.\n"
      "PUNTO Q: con un divisor en la base y RE, IC casi\n"
      "no depende de " S_BETA "; Q est" S_aa " en la recta de carga\n"
      "VCE = VCC - IC" S_CDOT "(RC + RE). En peque" S_nn "a se" S_nn "al, gm =\n"
      "IC/VT y r" S_PI " = " S_BETA "/gm; el emisor com" S_uu "n da -gm" S_CDOT "RC con RE\n"
      "desacoplada por un C (sin " S_ee "l, " S_APROX " -RC/RE). El MOSFET\n"
      "es parecido, pero lo manda VGS: conduce si VGS > Vt.",
      FIG_NADA, -1 },
};
static const signed char M_AO[] = { 32, 33, 34, 35, 36, 37, 38, 39, 40, 41 };

static const signed char E_OHM[] = { EJ_OHM_I, EJ_OHM_V, EJ_OHM_R, EJ_OHM_K, EJ_PA, EJ_PB, EJ_PC };
static const signed char E_SERIE[] = { EJ_S_DOS, EJ_S_TRES, EJ_PD, EJ_S_DIV, EJ_S_DATO, EJ_S_R };
static const signed char E_PAR[] = { EJ_P_DOS, EJ_P_TRES, EJ_PE, EJ_P_DIV, EJ_P_R };
static const signed char E_MIXTO[] = { EJ_M_SP, EJ_PF, EJ_PG, EJ_M_RAMA, EJ_M_ESC };
static const signed char E_DESP[] = { EJ_S_R, EJ_P_R, EJ_PH, EJ_PI, EJ_D_OTRA, EJ_D_FUENTE, EJ_D_DOS };
static const signed char E_POT[] = { EJ_W_SERIE, EJ_W_PAR };
static const signed char E_KIR[] = { EJ_K_PUENTE, EJ_K_EQUIL, EJ_K_DOS };
static const signed char E_REAL[] = { EJ_J_SOLA, EJ_J_SERIE, EJ_J_HALLAR, EJ_F_REAL, EJ_F_R, EJ_I_AMP, EJ_I_VOLT };
static const signed char E_PIZ[] = { EJ_PA, EJ_PB, EJ_PC, EJ_PD, EJ_PE, EJ_PF, EJ_PG, EJ_PH, EJ_PI };

#define N(a) ((int)(sizeof a / sizeof a[0]))

const Leccion LECCIONES[] = {
    { "1. La ley de Ohm", "V = I·R y cómo despejar I o R.", P_OHM, N(P_OHM), E_OHM, N(E_OHM) },
    { "2. Resistencias en serie", "Misma corriente; las tensiones suman.", P_SERIE, N(P_SERIE), E_SERIE, N(E_SERIE) },
    { "3. Resistencias en paralelo", "Misma tensión; las corrientes suman.", P_PARALELO, N(P_PARALELO), E_PAR, N(E_PAR) },
    { "4. Circuitos mixtos", "Reducir de dentro hacia fuera y volver.", P_MIXTO, N(P_MIXTO), E_MIXTO, N(E_MIXTO) },
    { "5. Despejar una incógnita", "Hallar una R o la fuente desde los datos.", P_DESPEJAR, N(P_DESPEJAR), E_DESP, N(E_DESP) },
    { "6. Potencia", "P = V·I; quién disipa más; el balance.", P_POTENCIA, N(P_POTENCIA), E_POT, N(E_POT) },
    { "7. Cuando no se puede reducir", "Puentes y dos fuentes: Kirchhoff.", P_KIRCHHOFF, N(P_KIRCHHOFF), E_KIR, N(E_KIR) },
    { "8. Fuentes reales, J e instrumentos", "Corriente impuesta, r interna, V y A.", P_REAL, N(P_REAL), E_REAL, N(E_REAL) },
    { "9. Problemas del pizarrón", "Los nueve problemas de clase, resueltos.", 0, 0, E_PIZ, N(E_PIZ) },
    { "10. Circuitos en el tiempo", "C, L, interruptores y formas: se emulan.", P_TIEMPO, N(P_TIEMPO), 0, 0,
      M_TIEMPO, N(M_TIEMPO) },
    { "11. Corriente alterna", "Fasores, potencias, Bode y Th" S_ee "venin.", P_ALTERNA, N(P_ALTERNA), 0, 0,
      M_ALTERNA, N(M_ALTERNA) },
    { "12. Diodos", "Rectificar, recortar y fijar; zener y LED.", P_DIODOS, N(P_DIODOS), 0, 0,
      M_DIODOS, N(M_DIODOS) },
    { "13. AO y transistores", "El AO, las fuentes controladas y el BJT.", P_AO, N(P_AO), 0, 0,
      M_AO, N(M_AO) },
};
const int N_LECCIONES = N(LECCIONES);
