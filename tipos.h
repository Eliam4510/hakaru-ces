/* ============================================================================
 *  tipos.h  -  Los tipos de componente, en UN solo sitio
 *
 *  Antes "resistencia" y "fuente" estaban escritas a mano por todo el
 *  programa (la etiqueta R1/E1, la unidad, los campos de la ficha, el color,
 *  si se puede girar...). Con un componente nuevo había que tocar veinte
 *  sitios y era fácil olvidar uno. Aquí está lo que es DATO de cada tipo; lo
 *  que es comportamiento (su ley en el motor, su símbolo en el dibujo, su
 *  regla en el procedimiento) sigue en su módulo, con un caso por tipo.
 *
 *  Un tipo nuevo se añade al final del enumerado (el número se guarda en los
 *  archivos) y con su fila en TIPOS[] (tipos.c).
 * ========================================================================= */
#ifndef TIPOS_H
#define TIPOS_H

#include "ondas.h"                     /* ONDA_*: la variante de E y J */

typedef enum { EL_RES = 0, EL_FUENTE, EL_CORR, EL_VOLT, EL_AMP,
               EL_COND,                /* 5  condensador (tecla C) */
               EL_BOB,                 /* 6  bobina      (tecla L) */
               EL_INTER,               /* 7  interruptor (tecla S) */
               EL_IMP,                 /* 8  impedancia Z = |Z|∠φ = R + jX (tecla Z)  */
               EL_VATI,                /* 9  vatímetro W (paleta: la W es el cable)    */
               /* Etapa 4: semiconductores y amplificadores */
               EL_DIODO,               /* 10 diodo (tecla D): a = ánodo, b = cátodo    */
               EL_ZENER,               /* 11 zener (paleta): la raya con ganchos       */
               EL_LED,                 /* 12 LED (paleta): se enciende en el resultado */
               EL_EDEP,                /* 13 E controlada (paleta): rombo, b = +       */
               EL_JDEP,                /* 14 J controlada (paleta): rombo, de a hacia b */
               EL_AO,                  /* 15 amp. op. (tecla U): a = +, b = -, c = salida */
               EL_BJT,                 /* 16 transistor bipolar (tecla Q): a = C, b = E, c = B */
               EL_MOS,                 /* 17 MOSFET (tecla M): a = D, b = S, c = G     */
               EL_N } TipoEl;          /* el número se guarda: solo se añade al final */

/* Magnitudes de un elemento. Q_VAL es la principal: R de una resistencia,
 * E de una fuente. Q_AUX es un parámetro extra (r interna, ganancia...).
 * Q_VAL..Q_AUX son las de la CALCULADORA (dato o [?!]); Q_P1..Q_P6 son
 * los parámetros del EMULADOR (nunca [?!]; vacío = por omisión), con un
 * significado fijo por tipo (ver los Q_* de abajo). */
enum { Q_VAL = 0, Q_V, Q_I, Q_AUX, Q_N };
enum { Q_P1 = Q_N, Q_P2, Q_P3, Q_P4, Q_P5, Q_P6, Q_TOT };
#define NPAR    (Q_TOT - Q_P1)         /* 6 */
#define Q_BASE  Q_P1   /* E, J: CC (seno) o V1/I1 (cuadrada, triangular, pulso) */
#define Q_FREC  Q_P2   /* E, J: f (Hz)                                  */
#define Q_FASE  Q_P3   /* E, J: fase (grados)                           */
#define Q_CICLO Q_P4   /* E, J: D (%)                                   */
#define Q_T0    Q_P5   /* E, J: t0 del pulso (s)                        */
#define Q_ANCHO Q_P6   /* E, J: ancho del pulso (s); vacío o 0 = escalón */
#define Q_V0    Q_P1   /* C: phi(a) - phi(b) al empezar; vacía = la de CC */
#define Q_I0    Q_P1   /* L: corriente a -> b al empezar; vacía = la de CC */
#define Q_T1    Q_P1   /* S: primer cambio (s); vacío = nunca           */
#define Q_T2    Q_P2   /* S: segundo cambio (s); vacío = no vuelve      */

/* La variante de cada tipo (Elemento.variante): la forma en E y J (ONDA_*),
 * el modo en V y A, el estado al empezar en S */
enum { MED_CC = 0, MED_CA, MED_EF, MED_N };            /* modo del multímetro */
enum { INTER_ABIERTO = 0, INTER_CERRADO, INTER_N };
/* Su ley en la calculadora de CC */
enum { LEY_VALOR = 0, LEY_ABIERTO, LEY_CORTO, LEY_INTER,
       LEY_CA,                          /* Z: solo existe en alterna */
       LEY_NL,                          /* D, zener, LED, AO, BJT, MOS: SOL_NO_LINEAL        */
       LEY_DEP };                       /* E y J controladas: su ley nombra a otro elemento  */

/* Etapa 4: las variantes de los tipos nuevos (la primera es la que sale al
 * colocarlo, salvo el diodo, que sale con Vγ) */
enum { DIO_IDEAL = 0, DIO_VG, DIO_EXP, DIO_1N4148, DIO_1N4007, DIO_N };
enum { ZEN_LIBRO = 0, ZEN_EXP, ZEN_1N4733A, ZEN_N };
enum { LED_ROJO = 0, LED_NARANJA, LED_AMARILLO, LED_VERDE, LED_AZUL, LED_BLANCO, LED_N };
enum { DEP_V = 0, DEP_I, DEP_N };      /* controlada por la V o por la I de Elemento.ref    */
enum { AO_IDEAL = 0, AO_REAL, AO_N };
enum { BJT_NPN = 0, BJT_PNP, BJT_N };
enum { MOS_CN = 0, MOS_CP, MOS_N };
#define Q_CTRL 0xFD                    /* pseudo-magnitud de la ficha: el control (ref)     */

/* Huecos Q_P* de cada tipo: UNO por significado (tipo_rango y circ_valido toman la
 * primera fila de cada q). Vacío = el valor por omisión de nolin.h. */
#define QD_VG  Q_P1   /* diodo: Vγ (con Vγ)          */
#define QD_RD  Q_P2   /*        rd (con Vγ)          */
#define QD_IS  Q_P3   /*        Is (exponencial)     */
#define QD_N   Q_P4   /*        n  (exponencial)     */
#define QD_RS  Q_P5   /*        Rs (exponencial)     */
#define QZ_VZ  Q_P1   /* zener: Vz a Iz              */
#define QZ_RZ  Q_P2   /*        rz (dinámica en Iz)  */
#define QZ_IZ  Q_P3   /*        Iz (la de prueba)    */
#define QZ_VG  Q_P4   /*        Vγ de la directa (de libro) */
#define QL_VF  Q_P1   /* LED: Vf a If (vacía = la del color) */
#define QL_IF  Q_P2   /*      If (brillo pleno)      */
#define QL_IM  Q_P3   /*      Im (se quemaría)       */
#define QA_A   Q_P1   /* AO: A (real)                */
#define QA_VP  Q_P2   /*     V+ (saturación de arriba) */
#define QA_VN  Q_P3   /*     V- (saturación de abajo)  */
#define QA_RI  Q_P4   /*     Ri (real; vacía = infinita) */
#define QA_RO  Q_P5   /*     Ro (real; vacía = 0)    */
#define QB_BF  Q_P1   /* BJT: β                      */
#define QB_IS  Q_P2   /*      Is                     */
#define QB_VA  Q_P3   /*      VA (vacía = sin Early) */
#define QB_BR  Q_P4   /*      βR                     */
#define QM_VT  Q_P1   /* MOS: Vt (en módulo)         */
#define QM_K   Q_P2   /*      K                      */
#define QM_VA  Q_P3   /*      VA = 1/λ (vacía = infinita) */

/* ---- Huella de un componente de 3 patas ----
 * Coordenadas locales: u a lo largo de la flecha con que se colocó; v hacia abajo
 * si la flecha es horizontal y hacia la derecha si es vertical. El ancla (0,0) es
 * el punto del cursor al colocarlo. El espejo cambia v por v0 + v1 - v: la caja no
 * se mueve, a y b se intercambian y c se queda (pv[c] = (v0 + v1)/2). */
#define NPATAS 3
typedef struct Huella {
    signed char u1, v0, v1;            /* la caja: u de 0 a u1; v de v0 a v1              */
    signed char pu[NPATAS], pv[NPATAS];/* patas a, b, c                                   */
    unsigned char ancla;               /* la pata que está en (0,0)                       */
    unsigned char sigue;               /* a qué pata salta el cursor al colocarlo         */
    const char *nom[NPATAS];           /* "+", "-", "sal" / "C", "E", "B" / "D", "S", "G" */
} Huella;

/* La fuente alterna (Etapa 3): una variante más de E y J, fuera de ondas.c.
 * Su valor es un fasor (eficaz, coseno y grados) a la f del circuito
 * (Circuito.frec), que la ficha enseña como una fila más (Q_FCIRC). */
#define FORMA_FASOR  ONDA_N             /* 5: E, J en alterna (fasor)        */
#define FORMA_N      (ONDA_N + 1)       /* variantes de E y J                */
#define Q_FCIRC      0xFE               /* pseudo-magnitud: Circuito.frec    */

#define VM(v)   (1u << (v))            /* máscara de variantes */
#define VM_PER  (VM(ONDA_SENO) | VM(ONDA_CUADRADA) | VM(ONDA_TRIANGULAR))
#define VM_NIV  (VM(ONDA_CUADRADA) | VM(ONDA_TRIANGULAR) | VM(ONDA_PULSO))
#define VM_CT   (VM(ONDA_CUADRADA) | VM(ONDA_TRIANGULAR))

#define MAX_CAMPOS   18                /* la E usa 17 */
#define CF_NO_BUSCAR 0x001             /* no admite [?!] */
#define CF_SIGNO     0x002             /* admite (-) al principio */
/* Etapa 3: la alterna */
#define CF_ANG       0x004             /* en CA lleva caja de ángulo: módulo∠ángulo      */
#define CF_AMPL      0x008             /* su módulo es una amplitud: se ve y se lee en conv */
#define CF_ANG90     0x010             /* su ángulo va de -90 a 90 (Z pasiva)             */
#define CF_CX        0x020             /* admite a + jb al escribir (Z)                   */
#define CF_BUSCA_CA  0x040             /* con CF_NO_BUSCAR: en CA sí admite [?!] (C, L)   */
#define CF_SOLO_CA   0x080             /* un número (dato) solo en CA (P y fp del W)      */
#define CF_VER_CA    0x100             /* la fila solo se ve en un circuito de alterna    */

typedef struct {
    unsigned char q;           /* Q_*                                   */
    const char *nombre;        /* "R", "V", "I" en la ficha (<= 2 col.) */
    const char *unidad;        /* S_OHM, "V", "A"                       */
    const char *ayuda;         /* la línea de ayuda de la ficha         */
    /* Etapa 2 (a 0 en los campos de antes) */
    unsigned char vars;        /* VM(): con qué variantes se ve; 0 = siempre */
    unsigned short flags;      /* CF_*                                  */
    const char *vacio;         /* rótulo del vacío ("de CC", "nunca"...); NULL = el de siempre */
    double min, max;           /* rango del dato; 0 y 0 = el de siempre */
} CampoInfo;

typedef struct {
    char letra;                /* prefijo de la etiqueta: R1, E1        */
    char tecla;                /* tecla del editor                      */
    const char *nombre;        /* título de la ficha                    */
    const char *al_poner;      /* instrucción del editor al colocarlo   */
    unsigned short color;      /* RGB565                                */
    unsigned char polar;       /* tiene + y -: [F] lo gira              */
    unsigned char fuente;      /* fuente independiente                  */
    unsigned char pasivo;      /* entra en la reducción serie-paralelo  */
    unsigned char ideal;       /* val vacío (M_LIBRE) = ideal, no una
                                  incógnita: un instrumento sin R interna */
    unsigned char ncampos;
    CampoInfo campos[MAX_CAMPOS];
    double val_min, val_max;   /* rango del valor principal             */
    /* Etapa 2 (a 0 en las filas de antes, salvo el modelo) */
    unsigned char ley_cc;      /* LEY_*: su ley en la calculadora de CC */
    unsigned char dinamico;    /* C, L, S: con uno así, [=] sin [?!] emula */
    unsigned char nvar;        /* variantes que se eligen con [izq/der]  */
    const char *const *nom_var;/* sus nombres                           */
    const char *modelo;        /* página Modelo (\n; <= 13 líneas de 52) */
    /* Etapa 4 (NULL en las filas de antes) */
    const struct Huella *huella;       /* NULL: dos bornes                                */
    const char *const *modelo_var;     /* página Modelo de cada variante (tras `modelo`)  */
} TipoInfo;

extern const TipoInfo TIPOS[EL_N];

/* La fila de un tipo; un tipo fuera de rango da la de la resistencia (no
 * debería pasar: circ_valido los rechaza). */
const TipoInfo *tipo_info(int tipo);

/* El campo de la magnitud q en la ficha de ese tipo, o NULL si no la tiene
 * (la V de una fuente ideal es su E: no se pregunta aparte). */
const CampoInfo *tipo_campo(int tipo, int q);

/* El campo de q que se ve con esa variante (con varias formas, la misma q
 * tiene nombres distintos: E, Vp, V2), o NULL */
const CampoInfo *tipo_campo_v(int tipo, int variante, int q);

/* Rango de un dato: Q_VAL val_min..val_max; Q_AUX el de su campo si no es
 * 0 y 0 (el fp del vatímetro, -1..1), si no 0..1e9; Q_V, Q_I -1e9..1e9;
 * Q_FCIRC 1e-3..1e9; Q_P* el de su primer campo; 0 y 0 si no la usa */
void tipo_rango(int tipo, int q, double *lo, double *hi);

/* Nombres de las formas de onda (la variante de E y J) y su ecuación para
 * la página Modelo */
extern const char *const NOM_FORMA[FORMA_N];
extern const char *const FORMA_MODELO[FORMA_N];

/* Etapa 4: los nombres de las variantes de los tipos nuevos */
extern const char *const NOM_DIODO[DIO_N];
extern const char *const NOM_ZENER[ZEN_N];
extern const char *const NOM_LED[LED_N];
extern const char *const NOM_DEP[DEP_N];
extern const char *const NOM_AO[AO_N];
extern const char *const NOM_BJT[BJT_N];
extern const char *const NOM_MOS[MOS_N];

/* Nombre y unidad de la magnitud principal (Q_VAL): "R" y ohm, "E" y V... */
const char *tipo_nom_val(int tipo);
const char *tipo_uni_val(int tipo);

/* Un elemento de un tipo "ideal" con su valor vacío: no es una incógnita */
#define ES_IDEAL(e) (tipo_info((e)->tipo)->ideal && (e)->val.est == M_LIBRE)

#endif /* TIPOS_H */
