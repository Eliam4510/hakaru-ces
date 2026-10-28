/* ============================================================================
 *  capeq_texto.c  -  La Teoría y los Ejemplos de «Capacitores equivalentes»
 *  (ver capeq_texto.h)
 *
 *  De dónde sale: el PDF del usuario (Capacitores_motor_monofasico.pdf, sus
 *  §1-§7), el pizarrón del profe y la especificación v3 (ESPEC_CAPEQ.md,
 *  §2, §4 y §5). El usuario no entendió «C₂ = C − C₁, C₁ baja desde 12»:
 *  cada página lo dice con palabras y con un ejemplo corto.
 *
 *  Presupuesto (lo mide capacitores/contenido/comprueba_capeq.c): títulos
 *  de 40 columnas, líneas de 52, «ensena» de dos líneas como mucho. El texto
 *  está en UTF-8 y llega a la pantalla en Latin-1 (-fexec-charset); lo que
 *  no es Latin-1 (paralelo, flechas, subíndices, visto bueno, ohmio) va con
 *  las macros S_* de gfx.h, que ocupan una columna cada una.
 *
 *  Los números de los ejemplos los comprueba una fuerza bruta propia con
 *  fracciones exactas (capacitores/contenido/fuerza_ejemplos.py).
 * ========================================================================= */
#include "capeq_texto.h"
#include "gfx.h"                    /* solo las macros S_* de los glifos */

#define PAR   S_PAR                 /* ∥ */
#define FL    S_RARR                /* → */
#define ABAJO S_DARR                /* ↓ */
#define CHK   S_CHECK               /* ✔ */
#define OHM   S_OHM                 /* Ω */
#define C1    "C" S_SUB1
#define C2    "C" S_SUB2
#define C3    "C" S_SUB3

/* --------------------------------------------------------------------------
 *  Teoría
 * ----------------------------------------------------------------------- */
typedef struct { const char *titulo, *texto; } Pag;

static const Pag TEORIA[] = {

/* ---- 1. El contexto ---- */
{ "El contexto: el motor monofásico",
  "Un motor monofásico (compresor, ventilador,\n"
  "bomba) tiene dos devanados y tres terminales:\n"
  "  C  común: donde se unen los dos devanados.\n"
  "  A  arranque: alambre delgado, más resistencia.\n"
  "  T  trabajo o marcha: alambre grueso, menos R.\n"
  "\n"
  "Conexión base: la línea (L1 y L2) entra por C y\n"
  "T, y el capacitor va ENTRE A y T. Desfasa la\n"
  "corriente del devanado de arranque, y con eso el\n"
  "motor arranca y gira.\n"
  "\n"
  "Si el capacitor se daña y en la bodega no hay uno\n"
  "igual, se arma uno EQUIVALENTE con los que hay:\n"
  "dos (o tres) en serie o en paralelo que den lo\n"
  "mismo. «MF» en la placa = microfaradios (µF).\n"
  "\n"
  "Ejemplo: el motor pide 17 MF y no hay de 17.\n"
  "Con 10 y 7 en paralelo: 10 + 7 = 17 MF " CHK },

/* ---- 2. El óhmetro ---- */
{ "Identificar C, A y T con el óhmetro",
  "Con el motor DESCONECTADO, mide la resistencia\n"
  "entre los tres pares de terminales:\n"
  "· La MAYOR es A-T (los dos devanados en serie).\n"
  "· La MENOR es C-T (el devanado de trabajo).\n"
  "· La de en medio es C-A (el de arranque).\n"
  "Comprueba: R(C-A) + R(C-T) = R(A-T). La terminal\n"
  "que NO está en la lectura mayor es C.\n"
  "\n"
  "Ejemplo: terminales 1, 2 y 3. Mides\n"
  "   1-2: 8 " OHM "     1-3: 3 " OHM "     2-3: 5 " OHM "\n"
  "· La mayor (8 " OHM ") es 1-2: son A y T; la 3 es C.\n"
  "· De C a la 1 hay 3 " OHM " (la menor): la 1 es T.\n"
  "· De C a la 2 hay 5 " OHM ": la 2 es A.\n"
  "· Comprueba: 5 + 3 = 8 " CHK },

/* ---- 3. Las fórmulas ---- */
{ "Fórmulas: paralelo y serie",
  "PARALELO: los dos tocan A y T. Se SUMAN:\n"
  "      CT = " C1 " + " C2 "  (+ " C3 " ...)\n"
  "   Lo que falta:  " C2 " = CT - " C1 "\n"
  "   10 " PAR " 7 = 10 + 7 = 17 MF\n"
  "\n"
  "SERIE: uno detrás del otro. Se suman los inversos:\n"
  "      1/CT = 1/" C1 " + 1/" C2 "  (+ 1/" C3 " ...)\n"
  "   Con dos:       CT = " C1 "·" C2 "/(" C1 " + " C2 ")\n"
  "   Lo que falta:  " C2 " = CT·" C1 "/(" C1 " - CT)\n"
  "   21 y 42: 21·42/(21 + 42) = 882/63 = 14 MF\n"
  "\n"
  "DOS IGUALES: en paralelo, el doble (7 " PAR " 7 = 14);\n"
  "en serie, la mitad (28 y 28 en serie = 14).\n"
  "\n"
  "Ojo: es al revés que con resistencias. Con R suma\n"
  "la serie; con capacitores suma el PARALELO." },

/* ---- 4. Por qué paralelo o serie ---- */
{ S_IQ "Paralelo o serie? El porqué",
  "El PARALELO suma: da MÁS que el mayor de los que\n"
  "uses (10 " PAR " 7 = 17, más que 10).\n"
  "La SERIE da MENOS que el menor (21 y 42 en serie\n"
  "dan 14, menos que 21).\n"
  "\n"
  "Por eso:\n"
  "· En paralelo, los dos tienen que ser MENORES que\n"
  "  lo pedido (si uno ya es mayor, la suma se pasa).\n"
  "· En serie, los dos tienen que ser MAYORES (con\n"
  "  uno menor, el total queda todavía más abajo).\n"
  "\n"
  "Y mirando la bodega:\n"
  "· Todos menores que lo pedido: solo paralelo.\n"
  "· Todos mayores: solo serie.\n"
  "· De los dos: se prueban los dos. Puede salir uno,\n"
  "  los dos (tú eliges) o ninguno (y entonces, con\n"
  "  tres). Que no salga en paralelo NO obliga a que\n"
  "  salga en serie.\n"
  "\n"
  "Ejemplo: 14 MF con la bodega B (15 a 42). Todos\n"
  "son mayores que 14: el paralelo daría 31 o más.\n"
  "Solo la serie: 21 y 42 = 14 MF " CHK },

/* ---- 5. El procedimiento del profe ---- */
{ "El procedimiento del profe",
  "El diagrama de flujo del PDF, en pasos:\n"
  "1. Datos: el valor " S_PM " la tolerancia (el rango) y\n"
  "   la bodega.\n"
  "2. " S_IQ "Uno solo entra en el rango? Sí " FL " úsalo\n"
  "   (sustitución directa). No " ABAJO "\n"
  "3. Paralelo: " C2 " = C - " C1 ". " S_IQ "Está " C2 " en la\n"
  "   bodega? Sí " FL " paralelo. No " ABAJO "\n"
  "4. Serie: " C2 " = C·" C1 "/(" C1 " - C). " S_IQ "Está " C2 "?\n"
  "   Sí " FL " serie. No " ABAJO "\n"
  "5. Con tres: el mixto (a " PAR " b) en serie con c,\n"
  "   o los otros arreglos de tres.\n"
  "6. Comprobar CT (dentro del rango), cablear y\n"
  "   medir con el multímetro antes de energizar.\n"
  "\n"
  "Ejemplo: 17 MF con la bodega A.\n"
  "  2. No hay de 17.\n"
  "  3. 12 + " S_IQ "5? no hay; 10 + " S_IQ "7? sí " CHK "\n"
  "  6. CT = 10 + 7 = 17 MF " CHK " (paralelo)" },

/* ---- 6. Paralelo paso a paso ---- */
{ "Paralelo paso a paso",
  "En paralelo se SUMAN: busco DOS que sumen lo\n"
  "pedido. Tomo uno (" C1 ") y calculo el que le falta:\n"
  C2 " = C - " C1 ". Si ese " C2 " está en la bodega, listo.\n"
  "\n"
  S_IQ "Por qué «" C1 " baja desde 12»? Empiezo por el MÁS\n"
  "GRANDE que cabe (el mayor que es menor que C) y\n"
  "voy bajando: así no se escapa ninguno.\n"
  "\n"
  "Ejemplo: 17 MF con la bodega A\n"
  "(1 2 3 7 9 10 12 28 32 ...):\n"
  "  12 + " S_IQ "5?  " FL " no hay 5\n"
  "  10 + " S_IQ "7?  " FL " sí hay 7 " CHK "   (" C2 " = 17 - 10 = 7)\n"
  "  CT = 10 + 7 = 17 MF " CHK "\n"
  "\n"
  "Paro cuando " C1 " ya es menos de la mitad de C: el\n"
  "que falta sería mayor que " C1 ", y esa pareja ya la\n"
  "probé al bajar.\n"
  "\n"
  "Si el que falta es igual a " C1 " (7 + " S_IQ "7?), hacen\n"
  "falta DOS de 7 en la bodega." },

/* ---- 7. Serie paso a paso ---- */
{ "Serie paso a paso",
  "En serie el total queda MENOR que el más chico:\n"
  "busco dos MAYORES que lo pedido. Tomo uno (" C1 ") y\n"
  "el que le falta es  " C2 " = C·" C1 "/(" C1 " - C).\n"
  "\n"
  "Empiezo por el MÁS CHICO que es mayor que C y voy\n"
  "subiendo. Antes, el atajo: dos IGUALES de 2·C dan\n"
  "C (dos de 28 dan 14), si hay dos.\n"
  "\n"
  "Ejemplo: 14 MF con la bodega B (15 a 42):\n"
  "  dos de 28 darían 14: solo hay uno de 28.\n"
  "  con 15 falta 14·15/1 = 210  " FL " no hay\n"
  "  ... (con 16 a 19, tampoco)\n"
  "  con 20 falta 14·20/6 = 46.7 " FL " no hay\n"
  "  con 21 falta 14·21/7 = 42   " FL " sí hay 42 " CHK "\n"
  "  CT = 21·42/(21 + 42) = 882/63 = 14 MF " CHK "\n"
  "\n"
  "Paro cuando " C1 " pasa de 2·C: el que falta ya sería\n"
  "menor que " C1 ", y esa pareja ya la probé." },

/* ---- 8. Tolerancia e irresoluble ---- */
{ "Tolerancia y lo irresoluble",
  "La placa puede traer tolerancia: 22.5 " S_PM " 0.5 MF\n"
  "acepta cualquier CT de 22 a 23 MF.\n"
  "Si dice " S_PM " 5 %: el 5 % de 22.5 es 1.125, así que\n"
  "en «Tolerancia» escribe 1.125 (en MF, no en %).\n"
  "Sin tolerancia, la app acepta " S_PM " 0.05 MF (lo que a\n"
  "un decimal da lo mismo): 13.98 entra, con su dif.\n"
  "\n"
  "Si ninguna combinación entra, es IRRESOLUBLE con\n"
  "esa bodega: la app da lo más cercano y qué\n"
  "capacitor te faltaría.\n"
  "\n"
  "Ejemplo: 22.5 " S_PM " 0.5 con la B sin 21-24. Con dos\n"
  "no sale: la suma mínima es 15 + 16 = 31 > 23, y\n"
  "la serie máxima, 41·42/83 = 20.7 < 22. El profe\n"
  "agregó uno de 55 en serie con el 38:\n"
  "38·55/93 = 22.47 MF " CHK "  (o se arma con tres)." },

/* ---- 9. Con tres: los arreglos ---- */
{ "Con tres capacitores",
  "Si con dos no se puede (o el profe pide tres),\n"
  "hay cuatro arreglos de tres, cada capacitor de la\n"
  "bodega una sola vez:\n"
  "· Paralelo de 3:  CT = a + b + c\n"
  "· Serie de 3:     1/CT = 1/a + 1/b + 1/c\n"
  "· Mixto 1, (a " PAR " b) en serie con c: primero el\n"
  "  paralelo (a + b) y luego la serie con c:\n"
  "      CT = (a + b)·c/(a + b + c)\n"
  "· Mixto 2, (a en serie con b) " PAR " c: primero la\n"
  "  serie de a y b, y luego se le suma c:\n"
  "      CT = a·b/(a + b) + c\n"
  "\n"
  "Ejemplo (el mixto 1 del PDF): 20 " PAR " 28 = 48, y 48\n"
  "en serie con 42:\n"
  "  CT = 48·42/(48 + 42) = 2016/90 = 22.4 MF " CHK "\n"
  "Mixto 2, con la bodega A: (12 en serie con 60) " PAR " 7\n"
  "  CT = 12·60/72 + 7 = 10 + 7 = 17 MF " CHK },

/* ---- 10. Con tres: cuándo y en qué orden ---- */
{ "Tres: cuándo y en qué orden",
  "«Capacitores» en los datos:\n"
  "· Auto: prueba con 1, 2 y 3. Salen primero los\n"
  "  de MENOS capacitores (como en el examen) y, en\n"
  "  cada número, el más cercano. Los de tres, al\n"
  "  final: son «el menos favorable». Pueden ser\n"
  "  cientos: se enseñan los más cercanos.\n"
  "· 2 o 3: solo los de ese número. Si el profe pide\n"
  "  tres, pon 3.\n"
  "\n"
  S_IQ "Cuándo hacen falta tres?\n"
  "· Si ni los dos MAYORES suman lo pedido.\n"
  "  55 con 10 15 20 25: 20 + 25 = 45, no llega;\n"
  "  con tres, 10 + 20 + 25 = 55 MF " CHK "\n"
  "· Si ni los dos más CHICOS en serie bajan tanto.\n"
  "  5 con 10 15 30: 10·15/25 = 6, no baja;\n"
  "  con tres, 1/10 + 1/15 + 1/30 = 1/5 " FL " 5 MF " CHK "\n"
  "· Si no está la pareja justa (el 22.5 del PDF):\n"
  "  un mixto.\n"
  "\n"
  "Con dos no, pero con tres sí: " S_IQ "el profe lo puso\n"
  "mal? Díselo, o arma el de tres." },

/* ---- 11. El cableado ---- */
{ "Cómo se cablea",
  "Lo que dibuja el profe (y la app):\n"
  "· La clavija arriba a la izquierda. La línea 1\n"
  "  (la punteada) va SIEMPRE directo a C.\n"
  "· La línea 2 (la sólida) llega a una terminal del\n"
  "  arreglo, y esa misma terminal se PUENTEA a T.\n"
  "· La terminal opuesta del arreglo va a A.\n"
  "Los de motor no tienen polaridad: da igual qué\n"
  "terminal va a A y cuál a T.\n"
  "\n"
  "Uno solo: A " FL " una terminal; la otra " FL " L2 y T.\n"
  "Paralelo: las izquierdas unidas " FL " A; las\n"
  "  derechas unidas " FL " L2 y puente a T.\n"
  "Serie: A " FL " izquierda del 1.º; derecha del 1.º " FL "\n"
  "  izquierda del 2.º (punto medio: no toca nada\n"
  "  más); derecha del 2.º " FL " L2 y T.\n"
  "Mixto 1: el paralelo (a, b) va de A al punto\n"
  "  medio, y c, del punto medio a L2 y T.\n"
  "Mixto 2: a y b en serie arriba y c abajo, unidos\n"
  "  por los dos extremos: uno " FL " A, otro " FL " L2 y T.\n"
  "\n"
  "Ejemplo: 21 y 42 en serie: A " FL " 21 " FL " 42 " FL " T, y\n"
  "la línea 2 llega a la derecha del 42." },

/* ---- 12. Tensión y seguridad ---- */
{ "Tensión y seguridad",
  "· DESCONECTA el motor y DESCARGA el capacitor\n"
  "  antes de tocarlo: guarda carga aunque el motor\n"
  "  esté apagado. Con una resistencia (p. ej.\n"
  "  20 k" OHM ", 5 W) unos segundos; nunca en corto.\n"
  "· Son de AC y sin polaridad.\n"
  "· Tensión: cada capacitor del arreglo, igual o\n"
  "  MÁS que la del original (la placa: 370 VAC,\n"
  "  440 VAC...). Nunca menos.\n"
  "· En paralelo, el arreglo aguanta lo que el de\n"
  "  MENOR tensión nominal.\n"
  "· En serie la tensión se reparte al revés que C:\n"
  "  el CHICO aguanta más. 21 y 42 con 240 V: el 21\n"
  "  lleva 160 V y el 42, 80 V (240·42/63 = 160).\n"
  "· Antes de energizar, mide el arreglo con el\n"
  "  multímetro (capacitancia).\n"
  "· Si el capacitor está dañado o bajo de valor, el\n"
  "  motor zumba, no arranca o se calienta." },
};

#define N_TEORIA ((int)(sizeof TEORIA / sizeof TEORIA[0]))

int cpq_teoria_n(void) { return N_TEORIA; }

const char *cpq_teoria_titulo(int p) { return p >= 0 && p < N_TEORIA ? TEORIA[p].titulo : ""; }

const char *cpq_teoria_texto(int p) { return p >= 0 && p < N_TEORIA ? TEORIA[p].texto : ""; }

/* --------------------------------------------------------------------------
 *  Ejemplos: los cuatro del PDF, los repetidos, los de tres y, al final, el
 *  mixto del PDF suelto (en el caso 3 es el n.º 107 de 535 y no se ve)
 * ----------------------------------------------------------------------- */
#define BODEGA_A "1 2 3 7 9 10 12 28 32 34 35 38 42 60"
#define BODEGA_B "15-42"

static const CpqEjemplo EJEMPLOS[] = {
    { "17 MF, bodega A (PDF, caso 1)",
      "Paralelo: hay menores que 17. 12 + " S_IQ "5? no hay;\n"
      "10 + " S_IQ "7? sí: 10 " PAR " 7 = 17 MF " CHK,
      "17", "", 0, BODEGA_A },
    { "14 MF, bodega B (PDF, caso 2)",
      "Serie: todos son mayores que 14. Con 21 falta\n"
      "14·21/7 = 42, y sí hay: 21 y 42 = 14 MF " CHK,
      "14", "", 0, BODEGA_B },
    { "12 MF, bodega B (práctica del PDF)",
      "Serie con tres parejas exactas: 18 y 36, 20 y 30\n"
      "(la del PDF) y 21 y 28.",
      "12", "", 0, BODEGA_B },
    { "22.5 " S_PM " 0.5 MF, B sin 21-24 (caso 3)",
      "Con dos no se puede; con tres sí: 535 arreglos.\n"
      "Se ven los 20 más cercanos. El del PDF: ejemplo 10.",
      "22.5", "0.5", 0, BODEGA_B " sin 21-24" },
    { "14 MF con dos de 7 (repetidos)",
      "Cada repetido es un capacitor más: con dos de 7,\n"
      "7 " PAR " 7 = 14 vale (y también 12 " PAR " 2 y 10 " PAR " 4).",
      "14", "", 0, "1 2 3 4 5 6 7 10 12 7" },
    { "14 MF con dos de 28 (serie)",
      "Dos iguales en serie dan la mitad: 28 y 28 = 14.\n"
      "Con un solo 28 sería irresoluble con dos.",
      "14", "", 0, "20 28 28 35 50" },
    { "55 MF: solo con tres (paralelo)",
      "Los dos mayores suman 45: con dos no llega.\n"
      "Con tres en paralelo: 10 + 20 + 25 = 55 MF " CHK,
      "55", "", 0, "10 15 20 25" },
    { "5 MF: solo con tres (serie)",
      "En serie, los dos más chicos dan 6: no baja a 5.\n"
      "Con tres: 1/10 + 1/15 + 1/30 = 1/5 " FL " 5 MF " CHK,
      "5", "", 0, "10 15 30" },
    { "14 MF, bodega B: el profe pide tres",
      "Capacitores: 3. La serie de tres no llega a 14 (da\n"
      "13.7 como mucho); el mixto sí: (15 " PAR " 27) y 21 = 14.",
      "14", "", 3, BODEGA_B },
    { "22.5 " S_PM " 0.5 MF: el mixto del PDF",
      "Con 20, 28 y 42: (20 " PAR " 28) = 48, en serie con 42:\n"
      "48·42/(48 + 42) = 2016/90 = 22.4 MF " CHK " (dif. -0.1)",
      "22.5", "0.5", 0, "20 28 42" },
};

#define N_EJEMPLOS ((int)(sizeof EJEMPLOS / sizeof EJEMPLOS[0]))

static const CpqEjemplo VACIO = { "", "", "", "", 0, "" };

int cpq_ejemplos_n(void) { return N_EJEMPLOS; }

const CpqEjemplo *cpq_ejemplo(int i) { return i >= 0 && i < N_EJEMPLOS ? &EJEMPLOS[i] : &VACIO; }
