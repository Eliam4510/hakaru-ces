#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
ORÁCULO DE SIMULACIÓN DE CIRCUITOS  --  Hakaru CES / herramientas / oraculo_sim.py
==================================================================================

Simulador de REFERENCIA, independiente, para circuitos lineales con conmutación.
Su misión es dar resultados fiables contra los que comparar el simulador en C de
Hakaru CES. Prioriza exactitud y diagnósticos claros sobre la velocidad.

USO
---
    python oraculo_sim.py netlist.txt salida.csv [--estricto] [--metodo auto|Radau|DOP853]
                                                 [--rtol 1e-10] [--detalle]
    python oraculo_sim.py --pruebas

    Código de salida: 0 = bien; 1 = netlist errónea o circuito mal planteado (el motivo va
    a stderr tras "ERROR:"); 2 = uso incorrecto; 3 = fallo interno del oráculo.
    Los avisos (p. ej. subcircuitos flotantes) van a stderr tras "AVISO:".
    --estricto   rechaza también los casos degenerados que el oráculo sabe resolver (lazos
                 de C con fuentes de tensión/cortos y cortes de L con fuentes de corriente/
                 abiertos; ver más abajo).
    --metodo     integrador de solve_ivp. 'auto' (por defecto): DOP853 si el tramo no es rígido;
                 se comprueba a posteriori (radio espectral * paso máximo <= 1.5; si no, el
                 estimador de error de DOP853 no es fiable) y si falla se rehace con Radau.
                 Tramos con max(-Re(autovalor)) * duración > 3000 van directamente a Radau.
    --detalle    imprime el análisis estructural de cada topología y las condiciones iniciales.

    Desde Python:  res = simular(texto_netlist);  res['V(2)'], res['I(C1)'], res.t, res.avisos

FORMATO DE LA NETLIST
---------------------
Texto, una línea por elemento; '#' inicia un comentario hasta el final de la línea; las
líneas vacías se ignoran. Las palabras clave (tfin, h, R, C, ..., DC, SIN, ...) no distinguen
mayúsculas; los nombres de elemento sí, y deben ser únicos. Nodos: enteros >= 0; el 0 es la
referencia (tierra). Los números admiten notación científica y, opcionalmente, un sufijo SI
pegado: f p n u µ m k K M meg G T  (OJO: M = mega, m = mili).

    tfin <s> h <s>                       duración y paso de SALIDA: valores en t = k*h,
                                         k = 0..floor(tfin/h)
    R  <nom> <a> <b> <ohm>               ohm > 0
    C  <nom> <a> <b> <F> <v0|->          v0 = V(a)-V(b) inicial; '-' = del punto de operación
    L  <nom> <a> <b> <H> <i0|->          i0 = corriente inicial de a hacia b; '-' = del p. de op.
    V  <nom> <neg> <pos> <r> <forma...>  V(pos)-V(neg) = e(t) - r*i ; i sale por pos; r >= 0
    I  <nom> <a> <b> <forma...>          j(t) circula de a hacia b por dentro (sale por b)
    S  <nom> <a> <b> <cerrado0> <t1> <t2>  interruptor ideal; cerrado0 = 0|1; conmuta en t1 si
                                         t1 > 0 y otra vez en t2 si t2 > 0 (si ambos > 0, t2 > t1;
                                         con t1 = 0 y t2 > 0 conmuta una sola vez, en t2). El
                                         estado nuevo rige para t >= t_evento.
    AM <nom> <a> <b> <r>                 amperímetro: r = 0 ideal (corto), r > 0 una resistencia
    VM <nom> <a> <b> <r>                 voltímetro:  r = 0 ideal (abierto), r > 0 una resistencia

  Formas de onda e(t) o j(t):
    DC  <valor>
    SIN <A> <f> <fase_grados> <nivel>    nivel + A*sin(2*pi*f*t + fase)
    SQR <A> <f> <ciclo_%> <nivel>        T = 1/f: nivel+A en [0, ciclo*T), nivel-A en [ciclo*T, T)
    TRI <A> <f> <ciclo_%> <nivel>        sube linealmente de nivel-A a nivel+A en [0, ciclo*T) y baja
                                         a nivel-A en [ciclo*T, T); ciclo 50 = triangular simétrica;
                                         ciclo 0 o 100 = diente de sierra (con salto)
    PUL <V1> <V2> <td> <ancho> <periodo> V1 si t < td; V2 en [td, td+ancho); luego V1; si periodo > 0
                                         se repite cada periodo desde td; ancho = 0 = escalón (V2
                                         para siempre desde td)
  Todas las formas son continuas por la derecha: en el instante de un flanco ya vale el valor nuevo.

CONVENIO DE SIGNOS
------------------
  * Cada elemento se orienta de a hacia b (en V, de neg hacia pos). I(elem) es la corriente que
    lo recorre POR DENTRO de a hacia b.
  * R, C, L, AM, VM: convenio pasivo con u = V(a)-V(b):  u = R*I,  I = C*du/dt,  u = L*dI/dt.
  * Fuente de tensión: I(V) es la corriente que SALE por pos (entra por neg; convenio generador):
    V(pos)-V(neg) = e - r*I(V). Potencia que entrega al exterior: e*I - r*I^2.
  * Fuente de corriente: I(J) = j(t) (entra por a, sale por b hacia el circuito exterior).
  * Interruptor: I(S) = corriente de a hacia b si está cerrado; 0 si está abierto.
  * Voltímetro ideal: I = 0 (su lectura es V(a)-V(b)). Amperímetro ideal: V(a) = V(b) y su lectura
    es I(AM). Con r > 0 ambos son resistencias.

SALIDA (CSV)
------------
  Cabecera: t,V(n1),V(n2),...,I(<elem1>),I(<elem2>),...
    - V(k) para cada nodo k != 0 que aparece en la netlist, en orden creciente (V(0) = 0 no va);
    - I(<nombre>) para cada elemento, en el orden de la netlist.
  Una fila por instante t = k*h, con los números en repr de Python (ida y vuelta exacta).
  En un instante de conmutación o de flanco se escribe el límite POR LA DERECHA (el estado nuevo
  rige en t_evento); los eventos que distan menos de 1e-9*h de un t = k*h se consideran en él.

CONDICIONES INICIALES
---------------------
  En t = 0 se resuelve el punto de operación con las fuentes en su valor en t = 0 (y los
  interruptores en su estado inicial): C abiertos y L en corto, salvo los C/L con v0/i0 dados, que
  se imponen (C = fuente de tensión v0, L = fuente de corriente i0). De ahí salen los v_C y las
  i_L de los elementos con '-'. Si el punto de operación no existe (p. ej. una fuente de corriente
  que empuja contra un C abierto) o no determina un v_C o una i_L (p. ej. dos C sin v0 en serie,
  o una L sin i0 en paralelo con un corto), se rechaza pidiendo el v0 / i0 correspondiente.

MÉTODO (independiente de los modelos compañeros)
------------------------------------------------
  1. Variables de estado: v de cada C y la i de cada L; son continuas en todo instante.
  2. Para cada topología (combinación de estados de los interruptores) se resuelve la red
     resistiva (análisis nodal modificado) sustituyendo cada C por una fuente de tensión v_C y cada
     L por una fuente de corriente i_L. Eso da i_C y v_L como funciones lineales de (v_C, i_L, u):
          C dv_C/dt = i_C,   L di_L/dt = v_L   ==>   dx/dt = A x + B u(t) [+ B' du/dt]
  3. Casos degenerados que SÍ están bien planteados (físicamente): lazos formados por C junto con
     fuentes de tensión ideales / interruptores cerrados / amperímetros ideales (p. ej. dos C en
     paralelo, o un C sobre una fuente ideal) y cortes formados por L junto con fuentes de
     corriente / interruptores abiertos / voltímetros ideales (p. ej. dos L en serie, o una L en
     serie con una fuente de corriente). En ellos la red resistiva es singular: la LTK del lazo
     (o la LCK del corte) liga los v_C (o las i_L) entre sí y con las fuentes. El oráculo lo trata
     con exactitud: reduce el estado a coordenadas independientes sobre esa variedad y completa
     el sistema con la derivada de la ligadura (conservación de carga / de flujo), que determina
     la corriente del lazo (o la tensión del corte). Es entonces cuando aparece du/dt (B'), que se
     evalúa analíticamente (SIN, TRI; nula en DC, SQR, PUL entre flancos). Las bases de esas
     coordenadas (y las de los multiplicadores) se ortonormalizan con Gram-Schmidt, que conserva
     los ceros estructurales: un C o una L que no está en ningún lazo/corte conserva su propia
     coordenada y las partes independientes del circuito no se mezclan numéricamente.
  4. Integración: scipy.integrate.solve_ivp (DOP853 o Radau con jacobiano exacto), rtol = 1e-10 y
     atol = 1e-12 * (máximo de |v_C| o |i_L| medido en una pasada previa gruesa), con un suelo de
     1e-14 * (escala de tensiones o corrientes de todo el circuito): sin él, una clase de estado
     nula en todo el intervalo (i_L de un puente equilibrado, tanque LC sin energía) tenía
     atol = 1e-30 y el integrador se arrastraba persiguiendo el redondeo. La integración se
     PARTE en todos los eventos (conmutaciones, flancos de SQR y PUL, vértices de TRI) para no
     cruzar discontinuidades; con DOP853 las salidas se toman de su salida densa de orden 7; con
     Radau se integra de salida en salida (nunca se interpola con su polinomio de colocación).
     DOP853 solo se acepta si ningún paso quedó limitado por estabilidad (rho*dt <= 1.5): en ese
     régimen su estimador de error falla (medido: error 1e-6 con rtol 1e-10 en un RLC
     sobreamortiguado) y el tramo se rehace con Radau.
  5. En cada evento se toman v_C e i_L del tramo anterior (límite por la izquierda) y se comprueba
     que cumplen las ligaduras de la topología y de las fuentes nuevas; si no (habría que saltar un
     v_C o una i_L) se rechaza: exigiría un impulso. Tolerancia: 1e-8 * max(escala de v_C o i_L,
     1e-2 * escala de tensiones o corrientes del circuito), de modo que un estado que es nulo en
     teoría (y vale ~1e-17 por redondeo) no se toma por un salto. En t = 0 los desajustes de
     LTK/LCK se miden frente a la amplitud de las fuentes, no frente a su valor en t = 0 (un SIN
     de fase 180 vale 6e-16 en t = 0).
  6. Las variables algebraicas (potenciales y corrientes de todos los elementos) se reconstruyen en
     cada instante de salida a partir del estado y de u(t), du/dt.

CASOS RECHAZADOS (con mensaje)
------------------------------
  * Netlist mal escrita: campos de más o de menos, números inválidos, R/C/L <= 0, r < 0, ciclo fuera
    de [0,100], f <= 0 en SQR/TRI, nombres repetidos, elementos con los dos terminales en el mismo
    nodo, falta 'tfin ... h ...'.
  * Lazo formado SOLO por fuentes de tensión ideales, interruptores cerrados y amperímetros ideales
    (red singular: tensiones contradictorias o corriente de lazo indeterminada).
  * Corte formado solo por fuentes de corriente, interruptores abiertos y voltímetros ideales (la
    fuente de corriente no tiene camino de vuelta).
  * Condiciones iniciales incompatibles: v0 que violan la LTK en un lazo de C/fuentes/cortos, o i0
    que violan la LCK en un corte de L/fuentes/abiertos; punto de operación inexistente o que no
    determina algún v_C / i_L con '-'.
  * Conmutaciones o flancos que exigirían un impulso (salto de v_C o de i_L): cerrar un interruptor
    sobre un C cargado, poner en paralelo C a distinta tensión, abrir el circuito de una L con
    corriente, una SQR ideal directamente sobre un C, una SQR de corriente en serie con una L...
  * En modo --estricto, además, cualquier lazo C/fuente o corte L/fuente (aunque sea compatible).
  Subcircuitos flotantes (partes sin camino conductor al nodo 0): no se rechazan; se toma como
  referencia (V = 0) el nodo más bajo de cada uno y se emite un AVISO (por topología).

LIMITACIONES
------------
  * Solo elementos lineales e invariantes; interruptores ideales conmutados por tiempo (no por
    tensión/corriente); sin fuentes dependientes, sin inductancias mutuas.
  * Las rarezas numéricas extremas (p. ej. R de 1e-15 junto a R de 1e15) pueden degradar el
    condicionamiento; el oráculo no escala las ecuaciones.
  * Los eventos a menos de 1e-9*h entre sí o de una salida se funden en uno.
  * No hay límite de energía ni detección de resonancias no acotadas (un LC ideal excitado a su
    frecuencia crece sin límite, como debe).
"""

import sys
import math
import re
import io
import time
import argparse
import contextlib

import numpy as np
from scipy.integrate import solve_ivp
from scipy.linalg import null_space

TOL_PRUEBAS = 1e-7
MAX_MUESTRAS = 2_000_000
MAX_EVENTOS = 2_000_000


class ErrorCircuito(Exception):
    """Netlist errónea o circuito mal planteado: se rechaza con un mensaje claro."""


class ErrorInterno(Exception):
    """Falla una comprobación interna del oráculo (no debería ocurrir)."""


# =============================================================================
# Lectura de números y nodos
# =============================================================================

_SUFIJOS = {'T': 1e12, 'G': 1e9, 'M': 1e6, 'meg': 1e6, 'Meg': 1e6, 'MEG': 1e6,
            'k': 1e3, 'K': 1e3, 'm': 1e-3, 'u': 1e-6, 'µ': 1e-6, 'n': 1e-9,
            'p': 1e-12, 'f': 1e-15}
_RE_NUM = re.compile(r'^([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)(meg|Meg|MEG|[TGMkKmuµnpf])$')


def _num(tok, que, nlin):
    try:
        v = float(tok)
    except ValueError:
        m = _RE_NUM.match(tok)
        if not m:
            raise ErrorCircuito(f"línea {nlin}: '{tok}' no es un número válido ({que})")
        v = float(m.group(1)) * _SUFIJOS[m.group(2)]
    if not math.isfinite(v):
        raise ErrorCircuito(f"línea {nlin}: '{tok}' no es un número finito ({que})")
    return v


def _nodo(tok, nlin):
    if not re.fullmatch(r'\d+', tok):
        raise ErrorCircuito(f"línea {nlin}: el nodo '{tok}' debe ser un entero >= 0")
    return int(tok)


# =============================================================================
# Formas de onda (a trozos suaves). pieza(tref) devuelve (f, df/dt) válidas en el
# trozo que contiene tref; eventos(tmax) da los instantes de discontinuidad en (0, tmax].
# =============================================================================

def _cte(v):
    return (lambda t: v + 0.0 * np.asarray(t, dtype=float),
            lambda t: 0.0 * np.asarray(t, dtype=float))


def _recta(v0, t0, m):
    return (lambda t: v0 + m * (np.asarray(t, dtype=float) - t0),
            lambda t: m + 0.0 * np.asarray(t, dtype=float))


def _eventos_periodicos(T, desfases, tmax, t0=0.0):
    n = int(max(tmax - t0, 0.0) / T) + 2
    if n * len(desfases) > MAX_EVENTOS:
        raise ErrorCircuito(f"demasiados eventos de conmutación (más de {MAX_EVENTOS}); "
                            f"reduce tfin o la frecuencia")
    ev = []
    for k in range(n):
        for d in desfases:
            t = t0 + k * T + d
            if 0.0 < t <= tmax:
                ev.append(t)
    return ev


class Forma:
    evento = 'discontinuidad'

    def eventos(self, tmax):
        return []


class FormaDC(Forma):
    def __init__(self, v):
        self.v = v

    def pieza(self, tref):
        return _cte(self.v)


class FormaSIN(Forma):
    def __init__(self, A, f, fase, nivel):
        self.A, self.f, self.fase, self.nivel = A, f, fase, nivel

    def pieza(self, tref):
        A, w, ph, niv = self.A, 2.0 * math.pi * self.f, math.radians(self.fase), self.nivel
        return (lambda t: niv + A * np.sin(w * np.asarray(t, dtype=float) + ph),
                lambda t: A * w * np.cos(w * np.asarray(t, dtype=float) + ph))


class FormaSQR(Forma):
    evento = 'flanco'

    def __init__(self, A, f, ciclo, nivel):
        self.A, self.f, self.c, self.nivel = A, f, ciclo / 100.0, nivel
        self.T = 1.0 / f

    def pieza(self, tref):
        A, T, c, niv = self.A, self.T, self.c, self.nivel
        if c <= 0.0:
            return _cte(niv - A)
        if c >= 1.0:
            return _cte(niv + A)
        k = math.floor(tref / T)
        p = tref - k * T
        return _cte(niv + A if p < c * T else niv - A)

    def eventos(self, tmax):
        if self.c <= 0.0 or self.c >= 1.0:
            return []
        return _eventos_periodicos(self.T, [self.c * self.T, self.T], tmax)


class FormaTRI(Forma):
    evento = 'vértice'

    def __init__(self, A, f, ciclo, nivel):
        self.A, self.f, self.c, self.nivel = A, f, ciclo / 100.0, nivel
        self.T = 1.0 / f

    def pieza(self, tref):
        A, T, c, niv = self.A, self.T, self.c, self.nivel
        k = math.floor(tref / T)
        t0 = k * T
        p = tref - t0
        if c >= 1.0 or (c > 0.0 and p < c * T):
            return _recta(niv - A, t0, 2.0 * A / (c * T))          # subida
        return _recta(niv + A, t0 + c * T, -2.0 * A / ((1.0 - c) * T))  # bajada

    def eventos(self, tmax):
        if 0.0 < self.c < 1.0:
            return _eventos_periodicos(self.T, [self.c * self.T, self.T], tmax)
        return _eventos_periodicos(self.T, [self.T], tmax)


class FormaPUL(Forma):
    evento = 'flanco'

    def __init__(self, V1, V2, td, ancho, per):
        self.V1, self.V2, self.td, self.ancho, self.per = V1, V2, td, ancho, per

    def pieza(self, tref):
        if tref < self.td:
            return _cte(self.V1)
        if self.ancho <= 0.0:
            return _cte(self.V2)
        tau = tref - self.td
        if self.per > 0.0:
            tau -= math.floor(tau / self.per) * self.per
        return _cte(self.V2 if tau < self.ancho else self.V1)

    def eventos(self, tmax):
        ev = [self.td] if self.td > 0.0 else []
        if self.ancho > 0.0:
            if self.per <= 0.0:
                ev.append(self.td + self.ancho)
            elif self.ancho < self.per:
                ev += _eventos_periodicos(self.per, [self.ancho, self.per], tmax, t0=self.td)
        return [t for t in ev if 0.0 < t <= tmax]


_FORMAS = {'DC': (1, FormaDC), 'SIN': (4, FormaSIN), 'SQR': (4, FormaSQR),
           'TRI': (4, FormaTRI), 'PUL': (5, FormaPUL)}


def _forma(tk, nlin, quien):
    if not tk:
        raise ErrorCircuito(f"línea {nlin}: {quien}: falta la forma de onda (DC, SIN, SQR, TRI o PUL)")
    clave = tk[0].upper()
    if clave not in _FORMAS:
        raise ErrorCircuito(f"línea {nlin}: {quien}: forma de onda '{tk[0]}' desconocida "
                            f"(DC, SIN, SQR, TRI o PUL)")
    nargs, cls = _FORMAS[clave]
    if len(tk) - 1 != nargs:
        raise ErrorCircuito(f"línea {nlin}: {quien}: {clave} necesita {nargs} valor(es) y hay {len(tk) - 1}")
    v = [_num(x, f"parámetro de {clave}", nlin) for x in tk[1:]]
    if clave in ('SQR', 'TRI'):
        if v[1] <= 0:
            raise ErrorCircuito(f"línea {nlin}: {quien}: la frecuencia de {clave} debe ser > 0")
        if not 0.0 <= v[2] <= 100.0:
            raise ErrorCircuito(f"línea {nlin}: {quien}: el ciclo de {clave} debe estar entre 0 y 100 %")
    if clave == 'SIN' and v[1] < 0:
        raise ErrorCircuito(f"línea {nlin}: {quien}: la frecuencia de SIN debe ser >= 0")
    if clave == 'PUL' and (v[2] < 0 or v[3] < 0 or v[4] < 0):
        raise ErrorCircuito(f"línea {nlin}: {quien}: en PUL td, ancho y periodo deben ser >= 0")
    return cls(*v)


# =============================================================================
# Netlist
# =============================================================================

class Elemento:
    def __init__(self, tipo, nombre, a, b, nlin):
        self.tipo, self.nombre, self.a, self.b, self.nlin = tipo, nombre, a, b, nlin
        self.val = None      # R (ohm), C (F), L (H)
        self.ini = None      # v0 / i0 (None = del punto de operación)
        self.r = 0.0         # V, AM, VM
        self.forma = None    # V, I
        self.q = None        # índice de fuente (V, I) en el vector u
        self.cerrado0, self.t1, self.t2 = False, 0.0, 0.0

    def estado(self, tref):
        """Estado (True = cerrado) de un interruptor en el tramo que contiene tref."""
        s = self.cerrado0
        if self.t1 > 0.0 and tref >= self.t1:
            s = not s
        if self.t2 > 0.0 and tref >= self.t2:
            s = not s
        return s


class Circuito:
    TIPOS = ('R', 'C', 'L', 'V', 'I', 'S', 'AM', 'VM')

    def __init__(self, texto):
        self.elementos, self.fuentes, self.interruptores = [], [], []
        self.tfin = self.h = None
        vistos = {}
        for nlin, bruta in enumerate(texto.splitlines(), 1):
            linea = bruta.split('#', 1)[0].strip()
            if not linea:
                continue
            tk = linea.split()
            clave = tk[0].upper()
            if clave == 'TFIN':
                if self.tfin is not None:
                    raise ErrorCircuito(f"línea {nlin}: 'tfin' repetido")
                if len(tk) != 4 or tk[2].upper() != 'H':
                    raise ErrorCircuito(f"línea {nlin}: el formato es 'tfin <s> h <s>'")
                self.tfin = _num(tk[1], 'tfin', nlin)
                self.h = _num(tk[3], 'h', nlin)
                if not (self.tfin > 0 and self.h > 0):
                    raise ErrorCircuito(f"línea {nlin}: tfin y h deben ser > 0")
                continue
            if clave not in self.TIPOS:
                raise ErrorCircuito(f"línea {nlin}: tipo de elemento '{tk[0]}' desconocido "
                                    f"(R, C, L, V, I, S, AM, VM o tfin)")
            if len(tk) < 4:
                raise ErrorCircuito(f"línea {nlin}: faltan campos: '{linea}'")
            nombre = tk[1]
            if any(ch in nombre for ch in ',"()'):
                raise ErrorCircuito(f"línea {nlin}: el nombre '{nombre}' no puede contener , \" ( )")
            if nombre in vistos:
                raise ErrorCircuito(f"línea {nlin}: el nombre '{nombre}' ya se usó en la línea {vistos[nombre]}")
            vistos[nombre] = nlin
            a, b = _nodo(tk[2], nlin), _nodo(tk[3], nlin)
            quien = f"{clave} {nombre}"
            if a == b:
                raise ErrorCircuito(f"línea {nlin}: {quien} tiene los dos terminales en el mismo nodo ({a})")
            e = Elemento(clave, nombre, a, b, nlin)
            resto = tk[4:]

            def cuenta(k):
                if len(resto) != k:
                    raise ErrorCircuito(f"línea {nlin}: {quien}: se esperaban {4 + k} campos y hay {len(tk)}")

            if clave == 'R':
                cuenta(1)
                e.val = _num(resto[0], 'resistencia', nlin)
                if e.val <= 0:
                    raise ErrorCircuito(f"línea {nlin}: {quien}: R debe ser > 0 (para un corto usa un "
                                        f"amperímetro con r = 0 o un interruptor cerrado)")
            elif clave in ('C', 'L'):
                cuenta(2)
                e.val = _num(resto[0], 'capacidad' if clave == 'C' else 'inductancia', nlin)
                if e.val <= 0:
                    raise ErrorCircuito(f"línea {nlin}: {quien}: el valor debe ser > 0")
                e.ini = None if resto[1] == '-' else _num(resto[1], 'v0' if clave == 'C' else 'i0', nlin)
            elif clave == 'V':
                if len(resto) < 2:
                    raise ErrorCircuito(f"línea {nlin}: {quien}: el formato es "
                                        f"'V <nombre> <neg> <pos> <r> <forma...>'")
                e.r = _num(resto[0], 'r interna', nlin)
                if e.r < 0:
                    raise ErrorCircuito(f"línea {nlin}: {quien}: la resistencia interna debe ser >= 0")
                e.forma = _forma(resto[1:], nlin, quien)
            elif clave == 'I':
                e.forma = _forma(resto, nlin, quien)
            elif clave == 'S':
                cuenta(3)
                if resto[0] not in ('0', '1'):
                    raise ErrorCircuito(f"línea {nlin}: {quien}: el estado inicial debe ser 0 (abierto) o 1 (cerrado)")
                e.cerrado0 = resto[0] == '1'
                e.t1 = _num(resto[1], 't1', nlin)
                e.t2 = _num(resto[2], 't2', nlin)
                if e.t1 < 0 or e.t2 < 0:
                    raise ErrorCircuito(f"línea {nlin}: {quien}: t1 y t2 deben ser >= 0")
                if e.t1 > 0 and e.t2 > 0 and e.t2 <= e.t1:
                    raise ErrorCircuito(f"línea {nlin}: {quien}: t2 debe ser mayor que t1")
            else:  # AM, VM
                cuenta(1)
                e.r = _num(resto[0], 'resistencia del medidor', nlin)
                if e.r < 0:
                    raise ErrorCircuito(f"línea {nlin}: {quien}: r debe ser >= 0")
            if clave in ('V', 'I'):
                e.q = len(self.fuentes)
                self.fuentes.append(e)
            if clave == 'S':
                self.interruptores.append(e)
            self.elementos.append(e)
        if self.tfin is None:
            raise ErrorCircuito("falta la línea 'tfin <s> h <s>'")
        if not self.elementos:
            raise ErrorCircuito("la netlist no tiene elementos")
        self.nodos = sorted({e.a for e in self.elementos} | {e.b for e in self.elementos})
        self.nodos_salida = [nd for nd in self.nodos if nd != 0]
        self.C = [e for e in self.elementos if e.tipo == 'C']
        self.L = [e for e in self.elementos if e.tipo == 'L']


# =============================================================================
# Álgebra auxiliar
# =============================================================================

def _rref(A, tol=1e-10):
    A = np.array(A, dtype=float)
    filas, cols = A.shape
    f = 0
    for c in range(cols):
        if f >= filas:
            break
        p = f + int(np.argmax(np.abs(A[f:, c])))
        if abs(A[p, c]) < tol:
            continue
        if p != f:
            A[[f, p]] = A[[p, f]]
        A[f] /= A[f, c]
        for i in range(filas):
            if i != f and A[i, c] != 0.0:
                A[i] -= A[i, c] * A[f]
        f += 1
    return A[:f]


def _base_nula(Mat):
    """Base del núcleo {x : Mat x = 0}, por filas y en forma escalonada reducida.
    Con matrices de incidencia sale con coeficientes 0, +-1 (lazos / cortes fundamentales)."""
    Mat = np.asarray(Mat, dtype=float)
    r, c = Mat.shape
    if c == 0:
        return np.zeros((0, 0))
    Ns = np.eye(c) if (r == 0 or not np.any(Mat)) else null_space(Mat)
    if Ns.shape[1] == 0:
        return np.zeros((0, c))
    R = _rref(Ns.T)
    Rr = np.round(R)
    return np.where(np.abs(R - Rr) < 1e-9, Rr, R)


def _incid(n, idx, a, b):
    d = np.zeros(n)
    if a in idx:
        d[idx[a]] += 1.0
    if b in idx:
        d[idx[b]] -= 1.0
    return d


def _cols(vecs, n):
    return np.column_stack(vecs) if vecs else np.zeros((n, 0))


def _q_ortonormal(filas):
    """Base ortonormal (en columnas) del espacio generado por las filas.

    Gram-Schmidt modificado con reortogonalización, NO Householder: conserva los ceros
    estructurales, de modo que filas con soportes disjuntos (lazos o cortes de partes
    independientes del circuito) dan columnas con soportes disjuntos. Con QR de Householder
    las reflexiones mezclan partes independientes y el error de integración de una se filtra a
    otra (p. ej. un tanque LC sin energía acoplado numéricamente a un lazo E-C-C)."""
    filas = np.asarray(filas, dtype=float)
    Q = []
    for a in filas:
        v = a.copy()
        for _ in range(2):
            for q in Q:
                v -= (q @ v) * q
        nv = float(np.linalg.norm(v))
        if nv > 1e-10 * max(1.0, float(np.linalg.norm(a))):
            Q.append(v / nv)
    return np.column_stack(Q) if Q else np.zeros((filas.shape[1], 0))


def _cond(e):
    return 1.0 / (e.val if e.tipo == 'R' else e.r)


def _texto_estados(circ, estados):
    if not circ.interruptores:
        return "sin interruptores"
    return ", ".join(f"{s.nombre} {'cerrado' if v else 'abierto'}"
                     for s, v in zip(circ.interruptores, estados))


def _w(piezas, t):
    if not piezas:
        return np.zeros((0,) + np.shape(t))
    return np.array([p[0](t) for p in piezas], dtype=float)


def _wd(piezas, t):
    if not piezas:
        return np.zeros((0,) + np.shape(t))
    return np.array([p[1](t) for p in piezas], dtype=float)


# =============================================================================
# Una topología: red resistiva con C -> fuentes de tensión y L -> fuentes de corriente
# =============================================================================

class Topologia:
    """Análisis de una topología (estado fijo de los interruptores).

    Incógnitas de la red resistiva: z = [phi (nodos no referencia); i_W (corrientes de las ramas
    de tipo tensión: fuentes de tensión ideales y cortos, y luego los C)]:
        [ Y     D_W ] [phi]   [ -(D_I j + D_L i_L + D_N g_N e_N) ]
        [ D_W^T  0  ] [i_W] = [ s (= -e en fuentes, 0 en cortos) ; v_C ]
    Lazos de ramas-tensión (alfa en nucleo(D_W)) y cortes de ramas-corriente (psi en
    nucleo([D_G D_N D_W]^T)) hacen singular la matriz; se completan con
        alfa_C^T C^-1 i_C = -alfa_V^T ds/dt        (derivada de la LTK del lazo)
        psi^T D_L L^-1 D_L^T phi = -psi^T D_I dj/dt (derivada de la LCK del corte)
    y el estado se reduce a x = [xi; eta] con v_C = N_C xi + Pi_C u, i_L = N_L eta + Pi_L u.
    """

    def __init__(self, circ, estados, estricto, contexto):
        self.estados = estados
        self.contexto = contexto
        nw = len(circ.fuentes)
        est = {s.nombre: v for s, v in zip(circ.interruptores, estados)}
        G, N, Ws, Is, Ab = [], [], [], [], []
        for e in circ.elementos:
            t = e.tipo
            if t == 'R' or (t in ('AM', 'VM') and e.r > 0):
                G.append(e)
            elif t == 'AM':
                Ws.append(e)
            elif t == 'VM':
                Ab.append(e)
            elif t == 'S':
                (Ws if est[e.nombre] else Ab).append(e)
            elif t == 'V':
                (N if e.r > 0 else Ws).append(e)
            elif t == 'I':
                Is.append(e)
        Cs, Ls = circ.C, circ.L

        # ---- subcircuitos (conexión por ramas que fijan relaciones de tensión)
        padre = {nd: nd for nd in circ.nodos}

        def raiz(x):
            while padre[x] != x:
                padre[x] = padre[padre[x]]
                x = padre[x]
            return x

        for e in G + N + Ws + Cs + Ls:
            ra, rb = raiz(e.a), raiz(e.b)
            if ra != rb:
                padre[ra] = rb
        grupos = {}
        for nd in circ.nodos:
            grupos.setdefault(raiz(nd), []).append(nd)
        refs = set()
        self.flotantes = []
        for lst in grupos.values():
            lst.sort()
            refs.add(lst[0])
            if lst[0] != 0:
                self.flotantes.append((tuple(lst), lst[0]))
        for e in Is:
            if raiz(e.a) != raiz(e.b):
                ga = grupos[raiz(e.a)]
                gb = grupos[raiz(e.b)]
                raise ErrorCircuito(
                    f"{contexto}: la fuente de corriente {e.nombre} no tiene camino de vuelta: une dos "
                    f"partes del circuito (nodos {{{', '.join(map(str, ga))}}} y {{{', '.join(map(str, gb))}}}) "
                    f"conectadas solo por fuentes de corriente, interruptores abiertos o voltímetros "
                    f"ideales (corte de fuentes de corriente): la LCK exigiría j = 0.")

        self.refs = refs
        self.nodos_inc = [nd for nd in circ.nodos if nd not in refs]
        idx = {nd: i for i, nd in enumerate(self.nodos_inc)}
        self.idx = idx
        n = len(idx)
        inc = lambda e: _incid(n, idx, e.a, e.b)  # noqa: E731

        D_G = _cols([inc(e) for e in G], n)
        g = np.array([_cond(e) for e in G], dtype=float)
        D_N = _cols([inc(e) for e in N], n)
        gN = np.array([1.0 / e.r for e in N], dtype=float)
        D_Ws = _cols([inc(e) for e in Ws], n)
        D_C = _cols([inc(e) for e in Cs], n)
        D_L = _cols([inc(e) for e in Ls], n)
        D_I = _cols([inc(e) for e in Is], n)
        Cv = np.array([e.val for e in Cs], dtype=float)
        Lv = np.array([e.val for e in Ls], dtype=float)
        mN, mVs, mC, mL, mI = len(N), len(Ws), len(Cs), len(Ls), len(Is)
        mW = mVs + mC

        S_w = np.zeros((mVs, nw))
        for i, e in enumerate(Ws):
            if e.tipo == 'V':
                S_w[i, e.q] = -1.0          # v(neg->pos) = V(neg)-V(pos) = -e
        J_w = np.zeros((mI, nw))
        for i, e in enumerate(Is):
            J_w[i, e.q] = 1.0
        E_N = np.zeros((mN, nw))
        for i, e in enumerate(N):
            E_N[i, e.q] = 1.0

        # ---- lazos solo de fuentes de tensión ideales / cortos: red singular
        if mVs:
            Bv = _base_nula(D_Ws)
            if Bv.shape[0]:
                sel = np.nonzero(np.abs(Bv[0]) > 1e-9)[0]
                noms = [Ws[i].nombre for i in sel]
                hay_fuente = any(Ws[i].tipo == 'V' for i in sel)
                raise ErrorCircuito(
                    f"{contexto}: lazo formado solo por fuentes de tensión ideales, interruptores "
                    f"cerrados y/o amperímetros ideales: {', '.join(noms)}. "
                    + ("Las fuentes fijan tensiones contradictorias o dejan indeterminada la corriente "
                       "del lazo (red singular)." if hay_fuente else
                       "La corriente que circula por el lazo queda indeterminada (red singular)."))

        # ---- lazos C/fuente/corto y cortes L/fuente/abierto
        D_W = np.hstack([D_Ws, D_C])
        Alfa = _base_nula(D_W) if mW else np.zeros((0, 0))
        if Alfa.shape[0] == 0:
            Alfa = np.zeros((0, mW))
        Mc = np.hstack([D_G, D_N, D_W]).T
        Psi = _base_nula(Mc) if n else np.zeros((0, 0))
        if Psi.shape[0] == 0:
            Psi = np.zeros((0, n))
        nloop, ncut = Alfa.shape[0], Psi.shape[0]
        P_L = Psi @ D_L
        P_I = Psi @ D_I
        if ncut and np.linalg.matrix_rank(P_L) < ncut:
            raise ErrorInterno(f"{contexto}: corte sin inductores no detectado como corte de fuentes de corriente")

        nomW = [e.nombre for e in Ws] + [e.nombre for e in Cs]
        self.lazos = [[nomW[i] for i in np.nonzero(np.abs(f) > 1e-9)[0]] for f in Alfa]
        self.cortes = []
        for f in Psi:
            noms = [e.nombre for e in Ls + Is + Ab if abs(f @ inc(e)) > 1e-9]
            self.cortes.append(noms)
        if estricto and (nloop or ncut):
            partes = []
            if nloop:
                partes.append("lazos de C con fuentes de tensión/cortos/otros C: "
                              + "; ".join("{" + ", ".join(l) + "}" for l in self.lazos))
            if ncut:
                partes.append("cortes de L con fuentes de corriente/abiertos/otras L: "
                              + "; ".join("{" + ", ".join(c) + "}" for c in self.cortes))
            raise ErrorCircuito(f"{contexto}: modo estricto: la red resistiva (C = fuentes de tensión, "
                                f"L = fuentes de corriente) es singular por " + " y ".join(partes) + ".")

        K_Vs, K_C = Alfa[:, :mVs], Alfa[:, mVs:]
        # Bases del núcleo con la estructura de bloques del circuito: un C (o una L) que no
        # participa en ningún lazo (corte) conserva su propia coordenada, y lazos (cortes) de
        # partes independientes no se mezclan. (null_space/pinv por SVD dan una base
        # arbitraria que mezcla elementos no relacionados: el error de integración de unos se
        # filtra a otros, y un estado que debe ser nulo se vuelve ruido; ver la prueba del
        # tanque LC sin energía.) K_C y P_L tienen rango de filas completo.
        if nloop:
            N_C = _q_ortonormal(_base_nula(K_C))
            Pi_C = -K_C.T @ np.linalg.solve(K_C @ K_C.T, K_Vs @ S_w)
        else:
            N_C, Pi_C = np.eye(mC), np.zeros((mC, nw))
        if ncut:
            N_L = _q_ortonormal(_base_nula(P_L))
            Pi_L = -P_L.T @ np.linalg.solve(P_L @ P_L.T, P_I @ J_w)
        else:
            N_L, Pi_L = np.eye(mL), np.zeros((mL, nw))
        nxi, neta = N_C.shape[1], N_L.shape[1]
        nx = nxi + neta

        # ---- sistema orlado (no singular): [[M, Nm], [Nr, 0]] [z; lam] = [r; q]
        sz = n + mW
        k = ncut + nloop
        Y = (D_G * g) @ D_G.T + (D_N * gN) @ D_N.T
        BM = np.zeros((sz + k, sz + k))
        BM[:n, :n] = Y
        BM[:n, n:sz] = D_W
        BM[n:sz, :n] = D_W.T
        if ncut:
            BM[:n, sz:sz + ncut] = _q_ortonormal(Psi)
        if nloop:
            BM[n:sz, sz + ncut:sz + k] = _q_ortonormal(Alfa)
        Linv = 1.0 / Lv if mL else np.zeros(0)
        Cinv = 1.0 / Cv if mC else np.zeros(0)
        for i in range(ncut):
            BM[sz + i, :n] = ((Psi[i] @ D_L) * Linv) @ D_L.T
        for j in range(nloop):
            BM[sz + ncut + j, n + mVs:sz] = K_C[j] * Cinv

        # p = [v_C (mC); i_L (mL); u (nw); du/dt (nw)]
        o_vc, o_il, o_w, o_wd = 0, mC, mC + mL, mC + mL + nw
        npar = mC + mL + 2 * nw
        Rp = np.zeros((sz + k, npar))
        Rp[:n, o_il:o_il + mL] = -D_L
        Rp[:n, o_w:o_w + nw] = -D_I @ J_w - (D_N * gN) @ E_N
        Rp[n:n + mVs, o_w:o_w + nw] = S_w
        Rp[n + mVs:sz, o_vc:o_vc + mC] = np.eye(mC)
        Rp[sz:sz + ncut, o_wd:o_wd + nw] = -P_I @ J_w
        Rp[sz + ncut:sz + k, o_wd:o_wd + nw] = -K_Vs @ S_w
        if sz + k:
            Sol = np.linalg.solve(BM, Rp)
        else:
            Sol = np.zeros((0, npar))

        Tx = np.zeros((npar, nx))
        Tx[o_vc:o_vc + mC, :nxi] = N_C
        Tx[o_il:o_il + mL, nxi:] = N_L
        Tw = np.zeros((npar, nw))
        Tw[o_vc:o_vc + mC] = Pi_C
        Tw[o_il:o_il + mL] = Pi_L
        Tw[o_w:o_w + nw] = np.eye(nw)
        Td = np.zeros((npar, nw))
        Td[o_wd:o_wd + nw] = np.eye(nw)

        # comprobaciones internas: multiplicadores nulos (sistema compatible) y N_C^T Pi_C = 0
        Lam = Sol[sz:]
        esc = max(1.0, float(np.max(np.abs(Sol), initial=0.0)))
        for T_ in (Tx, Tw, Td):
            if Lam.size and T_.size and np.max(np.abs(Lam @ T_)) > 1e-8 * esc:
                raise ErrorInterno(f"{contexto}: sistema orlado incompatible (multiplicadores no nulos)")
        if Pi_C.size and N_C.size and np.max(np.abs(N_C.T @ Pi_C)) > 1e-9 * max(1.0, np.max(np.abs(Pi_C))):
            raise ErrorInterno(f"{contexto}: parametrización de los v_C no ortogonal")

        Sz = Sol[:sz]
        F = np.zeros((nx, sz))
        if nxi:
            F[:nxi, n + mVs:sz] = N_C.T * Cinv
        if neta:
            F[nxi:, :n] = (N_L.T * Linv) @ D_L.T
        self.A = F @ Sz @ Tx
        self.B = F @ Sz @ Tw
        self.Bd = F @ Sz @ Td
        self.autovalores = np.linalg.eigvals(self.A) if nx else np.zeros(0)
        self.rho = float(np.max(np.abs(self.autovalores), initial=0.0))          # radio espectral
        self.sigma = float(np.max(-self.autovalores.real, initial=0.0))          # amortiguamiento máx.

        # ---- salidas: V(nodos) e I(elementos)
        nsal = len(circ.nodos_salida) + len(circ.elementos)
        O = np.zeros((nsal, sz))
        Op = np.zeros((nsal, npar))
        for r, nd in enumerate(circ.nodos_salida):
            if nd in idx:
                O[r, idx[nd]] = 1.0
        posG = {id(e): i for i, e in enumerate(G)}
        posN = {id(e): i for i, e in enumerate(N)}
        posW = {id(e): i for i, e in enumerate(Ws)}
        posC = {id(e): i for i, e in enumerate(Cs)}
        posL = {id(e): i for i, e in enumerate(Ls)}
        posI = {id(e): i for i, e in enumerate(Is)}
        base = len(circ.nodos_salida)
        for j, e in enumerate(circ.elementos):
            r = base + j
            if id(e) in posG:
                O[r, :n] = g[posG[id(e)]] * inc(e)
            elif id(e) in posN:
                gg = gN[posN[id(e)]]
                O[r, :n] = gg * inc(e)            # i = g (V(neg)-V(pos)) + e/r
                Op[r, o_w + e.q] = gg
            elif id(e) in posW:
                O[r, n + posW[id(e)]] = 1.0
            elif id(e) in posC:
                O[r, n + mVs + posC[id(e)]] = 1.0
            elif id(e) in posL:
                Op[r, o_il + posL[id(e)]] = 1.0
            elif id(e) in posI:
                Op[r, o_w + e.q] = 1.0
            # abiertos (interruptor abierto, voltímetro ideal): corriente 0
        Q = O @ Sz + Op
        self.Ox, self.Ow, self.Od = Q @ Tx, Q @ Tw, Q @ Td

        # ---- para las comprobaciones de continuidad
        self.N_C, self.Pi_C, self.N_L, self.Pi_L = N_C, Pi_C, N_L, Pi_L
        self.K_C, self.KW_C = K_C, K_Vs @ S_w
        self.P_L, self.PW_L = P_L, P_I @ J_w
        self.nxi, self.neta, self.nx = nxi, neta, nx
        self.nloop, self.ncut = nloop, ncut

    def fisico(self, x, w):
        """(v_C, i_L) a partir del estado reducido x y de u."""
        return (self.N_C @ x[:self.nxi] + self.Pi_C @ w,
                self.N_L @ x[self.nxi:] + self.Pi_L @ w)


# =============================================================================
# Punto de operación en t = 0
# =============================================================================

def punto_operacion(circ, topo, w0):
    """v_C e i_L en t = 0: C sin v0 abiertos, L sin i0 en corto; C con v0 = fuentes de tensión,
    L con i0 = fuentes de corriente; fuentes en su valor en t = 0."""
    est = {s.nombre: v for s, v in zip(circ.interruptores, topo.estados)}
    idx = topo.idx
    n = len(idx)
    G, N, W, Q, Ab = [], [], [], [], []
    for e in circ.elementos:
        t = e.tipo
        if t == 'R' or (t in ('AM', 'VM') and e.r > 0):
            G.append(e)
        elif t == 'AM':
            W.append((e, 0.0))
        elif t == 'VM':
            Ab.append(e)
        elif t == 'S':
            (W.append((e, 0.0)) if est[e.nombre] else Ab.append(e))
        elif t == 'V':
            (N.append(e) if e.r > 0 else W.append((e, -float(w0[e.q]))))
        elif t == 'I':
            Q.append((e, float(w0[e.q])))
        elif t == 'C':
            (W.append((e, e.ini)) if e.ini is not None else Ab.append(e))
        elif t == 'L':
            (Q.append((e, e.ini)) if e.ini is not None else W.append((e, 0.0)))

    def inc(e):
        return _incid(n, idx, e.a, e.b)

    def desc(e):
        return {'V': f"fuente de tensión {e.nombre}", 'S': f"interruptor {e.nombre}",
                'AM': f"amperímetro ideal {e.nombre}", 'VM': f"voltímetro ideal {e.nombre}",
                'I': f"fuente de corriente {e.nombre}",
                'C': f"C {e.nombre} " + ("con v0" if e.ini is not None else "sin v0 (abierto en DC)"),
                'L': f"L {e.nombre} " + ("con i0" if e.ini is not None else "sin i0 (corto en DC)"),
                }.get(e.tipo, e.nombre)

    D_G = _cols([inc(e) for e in G], n)
    g = np.array([_cond(e) for e in G], dtype=float)
    D_N = _cols([inc(e) for e in N], n)
    gN = np.array([1.0 / e.r for e in N], dtype=float)
    eN = np.array([float(w0[e.q]) for e in N], dtype=float)
    D_W = _cols([inc(e) for e, _ in W], n)
    s0 = np.array([v for _, v in W], dtype=float)
    D_Q = _cols([inc(e) for e, _ in Q], n)
    jq = np.array([v for _, v in Q], dtype=float)
    mW = len(W)
    # Magnitud de cada término para las tolerancias: la amplitud de la fuente, no su valor en
    # t = 0. Con el valor en t = 0, un SIN de fase 180 (e(0) = 6e-16 por redondeo) frente a un C
    # con v0 = 0 daba un desajuste "relativo" de 1 y se rechazaba un circuito compatible.
    mag_W = np.array([_amplitud(e.forma) if e.tipo == 'V' else abs(v) for e, v in W], dtype=float)
    mag_Q = np.array([_amplitud(e.forma) if e.tipo == 'I' else abs(v) for e, v in Q], dtype=float)

    Alfa = _base_nula(D_W) if mW else np.zeros((0, 0))
    if Alfa.shape[0] == 0:
        Alfa = np.zeros((0, mW))
    Psi = _base_nula(np.hstack([D_G, D_N, D_W]).T) if n else np.zeros((0, 0))
    if Psi.shape[0] == 0:
        Psi = np.zeros((0, n))

    # compatibilidad de los lazos (LTK)
    for f in Alfa:
        sel = np.nonzero(np.abs(f) > 1e-9)[0]
        res = float(f @ s0)
        esc = max(float(np.max(np.abs(s0[sel]), initial=0.0)), float(np.max(mag_W[sel], initial=0.0)), 1e-300)
        if abs(res) > 1e-9 * esc:
            els = [W[i][0] for i in sel]
            pista = ""
            if any(e.tipo == 'L' for e in els):
                pista = (" En DC una L sin i0 es un corto: aquí queda en paralelo con una tensión no nula "
                         "(fuente o C con v0) y su corriente no tiene régimen; indica i0 en la L.")
            elif any(e.tipo == 'C' for e in els):
                pista = " Los v0 dados son incompatibles con el resto del lazo (C con v0 incompatibles)."
            raise ErrorCircuito(
                f"Punto de operación en t = 0 imposible: el lazo {{{'; '.join(desc(e) for e in els)}}} "
                f"exige tensiones contradictorias (desajuste LTK = {res:.6g} V).{pista}")
    # compatibilidad de los cortes (LCK)
    iny = D_Q @ jq
    for f in Psi:
        res = float(f @ iny)
        cruzan = [e for e, _ in Q if abs(f @ inc(e)) > 1e-9]
        esc = max(float(np.max(np.abs(jq), initial=0.0)), float(np.max(mag_Q, initial=0.0)), 1e-300)
        if abs(res) > 1e-9 * esc:
            abiertos = [e for e in Ab if abs(f @ inc(e)) > 1e-9]
            els = cruzan + abiertos
            pista = ""
            if any(e.tipo == 'C' for e in abiertos):
                pista += " Hay un C sin v0 (abierto en DC) en el camino: indica su v0."
            if any(e.tipo == 'L' for e, _ in Q if e in cruzan):
                pista += " Los i0 dados son incompatibles con la LCK en ese corte."
            raise ErrorCircuito(
                f"Punto de operación en t = 0 imposible: el corte {{{'; '.join(desc(e) for e in els)}}} "
                f"no deja pasar la corriente impuesta (desajuste LCK = {res:.6g} A).{pista}")
    # determinación de los valores pedidos con '-'
    for e in circ.C:
        if e.ini is None and Psi.shape[0] and np.max(np.abs(Psi @ inc(e))) > 1e-9:
            raise ErrorCircuito(
                f"El punto de operación en t = 0 no determina la tensión inicial de {e.nombre}: en DC "
                f"(C abiertos, L en corto) queda sin camino a la referencia, p. ej. en serie con otro C o "
                f"tras un interruptor abierto. Indica su v0 en lugar de '-'.")
    for i, (e, _) in enumerate(W):
        if e.tipo == 'L' and Alfa.shape[0] and np.max(np.abs(Alfa[:, i])) > 1e-9:
            j = int(np.nonzero(np.abs(Alfa[:, i]) > 1e-9)[0][0])
            lazo = [W[m][0] for m in np.nonzero(np.abs(Alfa[j]) > 1e-9)[0]]
            raise ErrorCircuito(
                f"El punto de operación en t = 0 no determina la corriente inicial de {e.nombre}: en DC "
                f"forma un lazo de cortos {{{'; '.join(desc(x) for x in lazo)}}} y la corriente que circula "
                f"por él es arbitraria. Indica su i0 en lugar de '-'.")

    sz = n + mW
    k2, k1 = Psi.shape[0], Alfa.shape[0]
    BM = np.zeros((sz + k1 + k2, sz + k1 + k2))
    BM[:n, :n] = (D_G * g) @ D_G.T + (D_N * gN) @ D_N.T
    BM[:n, n:sz] = D_W
    BM[n:sz, :n] = D_W.T
    Nm = np.zeros((sz, k1 + k2))
    if k2:
        Nm[:n, :k2] = _q_ortonormal(Psi)
    if k1:
        Nm[n:sz, k2:] = _q_ortonormal(Alfa)
    BM[:sz, sz:] = Nm
    BM[sz:, :sz] = Nm.T
    rhs = np.zeros(sz + k1 + k2)
    rhs[:n] = -(iny + (D_N * gN) @ eN)
    rhs[n:sz] = s0
    z = np.linalg.solve(BM, rhs) if rhs.size else rhs
    phi, iW = z[:n], z[n:sz]
    posW = {id(e): i for i, (e, _) in enumerate(W)}
    vC0 = np.array([e.ini if e.ini is not None else float(inc(e) @ phi) for e in circ.C], dtype=float)
    iL0 = np.array([e.ini if e.ini is not None else float(iW[posW[id(e)]]) for e in circ.L], dtype=float)
    return vC0, iL0


# =============================================================================
# Simulación
# =============================================================================

def _amplitud(forma):
    if isinstance(forma, FormaDC):
        return abs(forma.v)
    if isinstance(forma, (FormaSIN, FormaSQR, FormaTRI)):
        return abs(forma.nivel) + abs(forma.A)
    if isinstance(forma, FormaPUL):
        return max(abs(forma.V1), abs(forma.V2))
    return 0.0


def _escalas_a_priori(circ, vC0, iL0):
    """(Rc, VS0, IS0): resistencia característica (media geométrica de las resistencias, o
    sqrt(L/C) si no hay) y escalas de tensión y corriente estimadas a partir de las fuentes y
    de las condiciones iniciales. Solo fijan tolerancias absolutas; no afectan al resultado."""
    rs = ([e.val for e in circ.elementos if e.tipo == 'R']
          + [e.r for e in circ.elementos if e.tipo in ('V', 'AM', 'VM') and e.r > 0])
    if rs:
        Rc = math.exp(sum(math.log(x) for x in rs) / len(rs))
    elif circ.L and circ.C:
        Rc = math.sqrt(float(np.mean([e.val for e in circ.L])) / float(np.mean([e.val for e in circ.C])))
    else:
        Rc = 1.0
    V = [abs(float(x)) for x in vC0] + [_amplitud(f.forma) for f in circ.fuentes if f.tipo == 'V']
    I = [abs(float(x)) for x in iL0] + [_amplitud(f.forma) for f in circ.fuentes if f.tipo == 'I']
    VS0 = max(max(V, default=0.0), max(I, default=0.0) * Rc)
    IS0 = max(max(I, default=0.0), max(V, default=0.0) / Rc)
    return Rc, VS0, IS0


class Resultado:
    def __init__(self, t, columnas, datos, avisos, informe, estadistica):
        self.t, self.columnas, self.datos = t, columnas, datos
        self.avisos, self.informe, self.estadistica = avisos, informe, estadistica

    def __getitem__(self, nombre):
        if nombre == 't':
            return self.t
        return self.datos[:, self.columnas.index(nombre)]


class _Simulacion:
    def __init__(self, circ, estricto, metodo, rtol):
        self.circ, self.estricto, self.metodo, self.rtol = circ, estricto, metodo, rtol
        h = circ.h
        nk = int(math.floor(circ.tfin / h + 1e-9))
        if nk + 1 > MAX_MUESTRAS:
            raise ErrorCircuito(f"demasiados instantes de salida ({nk + 1}); aumenta h o reduce tfin")
        self.t_out = np.arange(nk + 1, dtype=float) * h
        self.t_last = float(self.t_out[-1])
        self.tol_t = 1e-9 * h
        self.topos = {}
        self.uso = {}
        self.estadistica = {'DOP853': 0, 'Radau': 0, 'DOP853->Radau': 0}
        self.dop853_rigido = []
        self.columnas = ([f"V({nd})" for nd in circ.nodos_salida]
                         + [f"I({e.nombre})" for e in circ.elementos])
        self._preparar_eventos(nk)

    # ---------------------------------------------------------------- eventos
    def _preparar_eventos(self, nk):
        circ, h, tol = self.circ, self.circ.h, self.tol_t
        tmax = self.t_last + h
        crudos = []
        for s in circ.interruptores:
            for tt in (s.t1, s.t2):
                if tt > 0:
                    crudos.append((tt, f"conmuta {s.nombre}"))
        for f in circ.fuentes:
            for tt in f.forma.eventos(tmax):
                crudos.append((tt, f"{f.forma.evento} de {f.nombre}"))
        ajust = []
        for tt, d in crudos:
            k = int(round(tt / h))
            if k >= 0 and abs(tt - k * h) <= tol:
                tt = float(self.t_out[k]) if k <= nk else k * h
            if tt <= tol:
                continue
            ajust.append((tt, d))
        ajust.sort(key=lambda x: x[0])
        ev = []
        for tt, d in ajust:
            if ev and tt - ev[-1][0] <= tol:
                if d not in ev[-1][1]:
                    ev[-1][1].append(d)
            else:
                ev.append([tt, [d]])
        self.desc_evento = {tt: ", ".join(ds) for tt, ds in ev}
        interior = [tt for tt, _ in ev if tt < self.t_last - tol]
        finales = [tt for tt, _ in ev if abs(tt - self.t_last) <= tol]
        self.evento_final = bool(finales) and self.t_last > 0
        self.t_evento_final = finales[0] if finales else None
        prox = [tt for tt, _ in ev if tt > self.t_last + tol]
        self.cortes = [0.0] + interior + ([self.t_last] if self.t_last > 0 else [])
        sig = min(prox[0], self.t_last + h) if prox else self.t_last + h
        self.tref_final = self.t_last + 0.5 * (sig - self.t_last)

    def config(self, tref, ta, tb):
        circ = self.circ
        estados = tuple(s.estado(tref) for s in circ.interruptores)
        topo = self.topos.get(estados)
        if topo is None:
            ctx = (f"Topología de t = {ta:.9g} s a {tb:.9g} s ({_texto_estados(circ, estados)})"
                   if circ.interruptores else "Circuito")
            topo = Topologia(circ, estados, self.estricto, ctx)
            self.topos[estados] = topo
        piezas = [f.forma.pieza(tref) for f in circ.fuentes]
        self.uso.setdefault(estados, []).append((ta, tb))
        return topo, piezas

    # ----------------------------------------------------- continuidad del estado
    def _transferir(self, topo, vC, iL, w, t, tol_rel, esc_v, esc_i, motivo):
        rC = topo.K_C @ vC + topo.KW_C @ w
        rL = topo.P_L @ iL + topo.PW_L @ w
        ev = max(esc_v, float(np.max(np.abs(vC), initial=0.0)),
                 float(np.max(np.abs(topo.KW_C @ w), initial=0.0)), 1e-300)
        ei = max(esc_i, float(np.max(np.abs(iL), initial=0.0)),
                 float(np.max(np.abs(topo.PW_L @ w), initial=0.0)), 1e-300)
        malos = np.nonzero(np.abs(rC) > tol_rel * ev)[0]
        if len(malos):
            j = int(malos[0])
            lazo = ", ".join(topo.lazos[j])
            if motivo is None:
                raise ErrorCircuito(
                    f"Condiciones iniciales incompatibles: en el lazo {{{lazo}}} las tensiones iniciales "
                    f"de los condensadores no cumplen la LTK con las fuentes (desajuste {rC[j]:.6g} V).")
            raise ErrorCircuito(
                f"En t = {t:.9g} s ({motivo}) la tensión de los condensadores del lazo {{{lazo}}} tendría "
                f"que saltar {-rC[j]:.6g} V para cumplir la LTK: exigiría un impulso de corriente "
                f"(conmutación o flanco mal planteado).")
        malos = np.nonzero(np.abs(rL) > tol_rel * ei)[0]
        if len(malos):
            j = int(malos[0])
            corte = ", ".join(topo.cortes[j])
            if motivo is None:
                raise ErrorCircuito(
                    f"Condiciones iniciales incompatibles: en el corte {{{corte}}} las corrientes iniciales "
                    f"de las bobinas no cumplen la LCK con las fuentes (desajuste {rL[j]:.6g} A).")
            raise ErrorCircuito(
                f"En t = {t:.9g} s ({motivo}) la corriente de las bobinas del corte {{{corte}}} tendría "
                f"que saltar {-rL[j]:.6g} A para cumplir la LCK: exigiría un impulso de tensión "
                f"(conmutación o flanco mal planteado).")
        return np.concatenate([topo.N_C.T @ vC, topo.N_L.T @ iL])

    # ---------------------------------------------------------------- integración
    def _integrar(self, topo, piezas, x0, ta, tb, ts, rtol, atol_v, atol_i):
        nx = topo.nx
        nt = len(ts)
        if nx == 0:
            return np.zeros((0, nt)), x0
        if tb <= ta:
            return np.tile(x0[:, None], (1, nt)), x0
        atol = np.concatenate([np.full(topo.nxi, atol_v), np.full(topo.neta, atol_i)])
        A, B, Bd = topo.A, topo.B, topo.Bd
        if piezas:
            def fun(t, x):
                return A @ x + B @ _w(piezas, t) + Bd @ _wd(piezas, t)
        else:
            def fun(t, x):
                return A @ x
        metodo = self.metodo
        if metodo == 'auto':
            # DOP853 solo si el tramo no es rígido. Se comprueba a posteriori: si algún paso quedó
            # limitado por estabilidad (rho*dt > 1.5) su estimador de error deja de ser fiable
            # (se midieron errores de 1e-6 con rtol = 1e-10) y el tramo se rehace con Radau.
            metodo = 'Radau' if topo.sigma * (tb - ta) > 3000.0 else 'DOP853?'
        if metodo in ('DOP853', 'DOP853?'):
            sol = solve_ivp(fun, (ta, tb), x0, method='DOP853', rtol=rtol, atol=atol,
                            dense_output=nt > 0)
            self._comprobar(sol, ta, tb)
            rigido = topo.rho * float(np.max(np.diff(sol.t), initial=0.0)) > 1.5
            if metodo == 'DOP853' or not rigido:
                if rigido:
                    self.dop853_rigido.append((ta, tb))
                self.estadistica['DOP853'] += 1
                X = sol.sol(ts).reshape(nx, nt) if nt else np.zeros((nx, 0))
                return X, sol.y[:, -1].copy()
            self.estadistica['DOP853->Radau'] += 1
        # Radau: se integra de salida en salida (no se interpola con su polinomio de colocación)
        X = np.zeros((nx, nt))
        x = x0.copy()
        tcur = ta
        for i, t in enumerate(ts):
            if t > tcur:
                x, tcur = self._radau(fun, A, x, tcur, t, rtol, atol), t
            X[:, i] = x
        if tb > tcur:
            x = self._radau(fun, A, x, tcur, tb, rtol, atol)
        return X, x

    def _radau(self, fun, A, x, ta, tb, rtol, atol):
        sol = solve_ivp(fun, (ta, tb), x, method='Radau', jac=A, rtol=rtol, atol=atol)
        self.estadistica['Radau'] += 1
        if sol.status != 0 and len(sol.t):
            # Radau de scipy puede quedarse a 1-2 ulp del final y fallar en el micro-paso que
            # falta (Newton con h ~ 1e-20). Ese resto se completa con un paso de Euler (error
            # ~ resto^2, despreciable). Cualquier otro fallo es un error.
            t_ult, x_ult = sol.t[-1], sol.y[:, -1]
            resto = tb - t_ult
            if 0.0 <= resto <= 1e-12 * (tb - ta) + 64 * np.spacing(abs(tb)):
                return x_ult + resto * fun(tb, x_ult)
        self._comprobar(sol, ta, tb)
        return sol.y[:, -1].copy()

    @staticmethod
    def _comprobar(sol, ta, tb):
        if sol.status != 0:
            raise ErrorInterno(f"solve_ivp falló entre t = {ta:.9g} y {tb:.9g} s: {sol.message}")

    def _evaluar(self, topo, piezas, X, ts):
        ts = np.asarray(ts, dtype=float)
        Y = topo.Ox @ X + topo.Ow @ _w(piezas, ts) + topo.Od @ _wd(piezas, ts)
        return Y.T

    # ---------------------------------------------------------------- una pasada
    def _pasada(self, rtol, atol_v, atol_i, tol_ev, esc_v, esc_i):
        t_out, tol = self.t_out, self.tol_t
        sal = np.zeros((len(t_out), len(self.columnas)))
        self.uso = {}
        cortes = self.cortes
        nint = len(cortes) - 1
        if nint >= 1:
            topo, piezas = self.config(0.5 * (cortes[0] + cortes[1]), cortes[0], cortes[1])
        else:
            topo, piezas = self.config(self.tref_final, 0.0, 0.0)
        vC, iL = self.vC0, self.iL0
        x = self._transferir(topo, vC, iL, _w(piezas, 0.0), 0.0, tol_ev, esc_v, esc_i, None)
        vmax = float(np.max(np.abs(vC), initial=0.0))
        imax = float(np.max(np.abs(iL), initial=0.0))
        for j in range(nint):
            ta, tb = cortes[j], cortes[j + 1]
            if j > 0:
                topo, piezas = self.config(0.5 * (ta + tb), ta, tb)
                x = self._transferir(topo, vC, iL, _w(piezas, ta), ta, tol_ev, esc_v, esc_i,
                                     self.desc_evento.get(ta, 'evento'))
            k0 = int(np.searchsorted(t_out, ta - tol, 'left'))
            if j == nint - 1 and not self.evento_final:
                k1 = len(t_out)
            else:
                k1 = int(np.searchsorted(t_out, tb - tol, 'left'))
            ts = t_out[k0:k1]
            X, x = self._integrar(topo, piezas, x, ta, tb, ts, rtol, atol_v, atol_i)
            if len(ts):
                sal[k0:k1] = self._evaluar(topo, piezas, X, ts)
                W = _w(piezas, ts)
                if topo.N_C.size or topo.Pi_C.size:
                    vmax = max(vmax, float(np.max(np.abs(topo.N_C @ X[:topo.nxi] + topo.Pi_C @ W),
                                                  initial=0.0)))
                if topo.N_L.size or topo.Pi_L.size:
                    imax = max(imax, float(np.max(np.abs(topo.N_L @ X[topo.nxi:] + topo.Pi_L @ W),
                                                  initial=0.0)))
            vC, iL = topo.fisico(x, _w(piezas, tb))
            vmax = max(vmax, float(np.max(np.abs(vC), initial=0.0)))
            imax = max(imax, float(np.max(np.abs(iL), initial=0.0)))
        if nint == 0:
            sal[0] = self._evaluar(topo, piezas, x[:, None], np.array([0.0]))[0]
        elif self.evento_final:
            tl = self.t_last
            topo, piezas = self.config(self.tref_final, tl, tl)
            x = self._transferir(topo, vC, iL, _w(piezas, tl), tl, tol_ev, esc_v, esc_i,
                                 self.desc_evento.get(self.t_evento_final, 'evento'))
            sal[-1] = self._evaluar(topo, piezas, x[:, None], np.array([tl]))[0]
        return sal, vmax, imax

    # ---------------------------------------------------------------- informe
    def _texto_intervalos(self, usos):
        usos = sorted(usos)
        unidos = []
        for a, b in usos:
            if unidos and abs(a - unidos[-1][1]) <= self.tol_t:
                unidos[-1][1] = max(unidos[-1][1], b)
            else:
                unidos.append([a, b])
        if len(unidos) == 1 and unidos[0][0] <= self.tol_t and unidos[0][1] >= self.t_last - self.tol_t:
            return "en toda la simulación"
        partes = [(f"en t = {a:.9g} s" if b - a <= self.tol_t else f"para t en [{a:.9g}, {b:.9g}) s")
                  for a, b in unidos[:4]]
        return ", ".join(partes) + (", ..." if len(unidos) > 4 else "")

    def _avisos(self):
        av = []
        for estados, usos in self.uso.items():
            topo = self.topos[estados]
            for nodos, ref in topo.flotantes:
                cuando = self._texto_intervalos(usos)
                cual = f" ({_texto_estados(self.circ, estados)})" if self.circ.interruptores else ""
                av.append(f"{cuando}{cual}: el subcircuito de nodos {{{', '.join(map(str, nodos))}}} no "
                          f"está unido al nodo 0; se toma el nodo {ref} como referencia (V({ref}) = 0).")
        if self.dop853_rigido:
            av.append(f"{self._texto_intervalos(self.dop853_rigido)}: con --metodo DOP853 hubo pasos limitados "
                      f"por estabilidad (circuito rígido); el estimador de error de DOP853 no es fiable ahí "
                      f"y el error puede superar la tolerancia pedida. Usa --metodo auto o Radau.")
        return av

    def _informe(self):
        circ = self.circ
        lin = ["Condiciones iniciales (t = 0):"]
        for e, v in zip(circ.C, self.vC0):
            lin.append(f"  v({e.nombre}) = {v:.15g} V" + ("" if e.ini is not None else "  (punto de operación)"))
        for e, v in zip(circ.L, self.iL0):
            lin.append(f"  i({e.nombre}) = {v:.15g} A" + ("" if e.ini is not None else "  (punto de operación)"))
        lin.append("Topologías:")
        for estados, topo in self.topos.items():
            usos = self.uso.get(estados, [])
            lin.append(f"- {_texto_estados(circ, estados)}: {self._texto_intervalos(usos) if usos else 'no usada'}")
            lin.append(f"    referencias {sorted(topo.refs)}; potenciales incógnita {len(topo.nodos_inc)}; "
                       f"estado {topo.nx} (v_C independientes {topo.nxi}, i_L independientes {topo.neta})")
            if topo.lazos:
                lin.append("    lazos C/fuente/corto: " + "; ".join("{" + ", ".join(l) + "}" for l in topo.lazos))
            if topo.cortes:
                lin.append("    cortes L/fuente/abierto: " + "; ".join("{" + ", ".join(c) + "}" for c in topo.cortes))
            if topo.nx:
                ev = sorted(topo.autovalores, key=lambda z: (z.real, z.imag))
                lin.append("    autovalores de A: " + ", ".join(f"{z.real:.6g}{z.imag:+.6g}j" for z in ev))
        lin.append(f"Llamadas a solve_ivp (pasada fina): DOP853 {self.estadistica['DOP853']}, "
                   f"Radau {self.estadistica['Radau']} (tramos pasados de DOP853 a Radau por rigidez: "
                   f"{self.estadistica['DOP853->Radau']})")
        return "\n".join(lin)

    # ---------------------------------------------------------------- principal
    def ejecutar(self):
        circ = self.circ
        if len(self.cortes) >= 2:
            tref0, tb0 = 0.5 * (self.cortes[0] + self.cortes[1]), self.cortes[1]
        else:
            tref0, tb0 = self.tref_final, 0.0
        topo0, piezas0 = self.config(tref0, 0.0, tb0)
        self.vC0, self.iL0 = punto_operacion(circ, topo0, _w(piezas0, 0.0))
        Rc, VS0, IS0 = _escalas_a_priori(circ, self.vC0, self.iL0)
        if circ.C or circ.L:
            # pasada gruesa: detecta errores pronto y mide la escala de v_C e i_L para atol.
            # Su atol sigue la escala a priori (fuentes, v0, i0): con 1e-14 fijo, un circuito de
            # kV con un estado nulo se arrastraría con pasos minúsculos persiguiendo el redondeo.
            # La escala de la comprobación de continuidad tampoco puede ser 0: un v_C o una i_L
            # que es nula en teoría (puente equilibrado) vale ~1e-17 por redondeo, y una
            # conmutación sobre ella se rechazaba como "impulso" con una tolerancia de 1e-21.
            sal0, ev, ei = self._pasada(1e-6, 1e-14 * max(1.0, VS0), 1e-14 * max(1.0, IS0), 1e-4,
                                        1e-2 * VS0, 1e-2 * IS0)
            nv = len(circ.nodos_salida)
            VS = max(ev, float(np.max(np.abs(sal0[:, :nv]), initial=0.0)))
            IS = max(ei, float(np.max(np.abs(sal0[:, nv:]), initial=0.0)))
            if VS == 0.0:
                VS = IS * Rc
            if IS == 0.0:
                IS = VS / Rc
        else:
            ev = ei = VS = IS = 0.0
        # atol de cada clase de estado: 1e-12 de su escala, pero nunca por debajo de 1e-14 de la
        # escala de tensiones (corrientes) de TODO el circuito. Sin ese suelo, una clase de estado
        # que es nula en todo el intervalo (p. ej. las i_L de un tanque LC sin energía, o las de
        # un puente equilibrado) recibe atol ~ 1e-30 y el integrador se arrastra persiguiendo el
        # ruido de redondeo (pasos de 1e-15 s: el oráculo se cuelga).
        esc_v, esc_i = max(ev, 1e-2 * VS), max(ei, 1e-2 * IS)
        atol_v, atol_i = max(1e-12 * esc_v, 1e-30), max(1e-12 * esc_i, 1e-30)
        self.estadistica = {'DOP853': 0, 'Radau': 0, 'DOP853->Radau': 0}
        self.dop853_rigido = []
        sal, _, _ = self._pasada(self.rtol, atol_v, atol_i, 1e-8, esc_v, esc_i)
        return Resultado(self.t_out.copy(), list(self.columnas), sal, self._avisos(),
                         self._informe(), dict(self.estadistica))


def simular(netlist, estricto=False, metodo='auto', rtol=1e-10):
    """Simula una netlist (texto o Circuito). Devuelve un Resultado; lanza ErrorCircuito."""
    if metodo not in ('auto', 'Radau', 'DOP853'):
        raise ValueError("metodo debe ser 'auto', 'Radau' o 'DOP853'")
    circ = netlist if isinstance(netlist, Circuito) else Circuito(netlist)
    return _Simulacion(circ, estricto, metodo, rtol).ejecutar()


def escribir_csv(res, ruta):
    with open(ruta, 'w', newline='', encoding='utf-8') as f:
        f.write(','.join(['t'] + res.columnas) + '\n')
        filas = np.column_stack([res.t, res.datos])
        for fila in filas:
            f.write(','.join(repr(float(v) + 0.0) for v in fila) + '\n')


# =============================================================================
# Pruebas contra soluciones exactas
# =============================================================================

_PRUEBAS = []


def _prueba(nombre):
    def deco(f):
        _PRUEBAS.append((nombre, f))
        return f
    return deco


def _err(sim, exacta, escala=None):
    sim = np.asarray(sim, dtype=float)
    exacta = np.asarray(exacta, dtype=float) * np.ones_like(sim)
    if escala is None:
        escala = float(np.max(np.abs(exacta)))
    return float(np.max(np.abs(sim - exacta)) / escala)


def _rc_escalones(t, tau, v0, bordes, valores):
    """Exacta de v' = (e - v)/tau con e = valores[k] en [bordes[k], bordes[k+1])."""
    eps = 1e-12
    v = np.empty_like(t)
    vk = v0
    for k, ta in enumerate(bordes):
        tb = bordes[k + 1] if k + 1 < len(bordes) else np.inf
        e = valores[k]
        m = (t >= ta - eps) & (t < tb - eps)
        v[m] = e + (vk - e) * np.exp(-(t[m] - ta) / tau)
        if np.isfinite(tb):
            vk = e + (vk - e) * math.exp(-(tb - ta) / tau)
    return v


def _trozos(t, bordes, valores):
    """e(t) constante a trozos, continua por la derecha."""
    k = np.searchsorted(np.asarray(bordes) - 1e-12, t, side='right') - 1
    return np.asarray(valores, dtype=float)[k]


def _trozos_rampa(t, bordes, e0s, pend):
    """e(t) = e0s[k] + pend[k] (t - bordes[k]) en [bordes[k], bordes[k+1])."""
    k = np.searchsorted(np.asarray(bordes) - 1e-12, t, side='right') - 1
    return np.asarray(e0s)[k] + np.asarray(pend)[k] * (t - np.asarray(bordes)[k])


def _rc_rampas(t, tau, v0, bordes, e0s, pend):
    """Exacta de v' = (e - v)/tau con e = e0s[k] + pend[k] (t - bordes[k]) en cada trozo."""
    eps = 1e-12
    v = np.empty_like(t)
    vk = v0
    for k, ta in enumerate(bordes):
        tb = bordes[k + 1] if k + 1 < len(bordes) else np.inf
        e0, m_ = e0s[k], pend[k]
        msk = (t >= ta - eps) & (t < tb - eps)
        s = t[msk] - ta
        v[msk] = e0 + m_ * s - m_ * tau + (vk - e0 + m_ * tau) * np.exp(-s / tau)
        if np.isfinite(tb):
            s = tb - ta
            vk = e0 + m_ * s - m_ * tau + (vk - e0 + m_ * tau) * math.exp(-s / tau)
    return v


@_prueba("RC carga (E=10 V, R=1k, C=1u, v0=0)")
def _p_rc_carga():
    r = simular("tfin 5e-3 h 1e-5\nV E1 0 1 0 DC 10\nR R1 1 2 1000\nC C1 2 0 1e-6 0\n")
    t, tau = r.t, 1e-3
    vc, i = 10 * (1 - np.exp(-t / tau)), 0.01 * np.exp(-t / tau)
    return max(_err(r['V(2)'], vc), _err(r['V(1)'], 10.0), _err(r['I(R1)'], i),
               _err(r['I(C1)'], i), _err(r['I(E1)'], i)), ""


@_prueba("RC descarga (v0=5 V, R=1k, C=1u)")
def _p_rc_descarga():
    r = simular("tfin 5e-3 h 1e-5\nR R1 1 0 1000\nC C1 1 0 1e-6 5\n")
    v = 5 * np.exp(-r.t / 1e-3)
    return max(_err(r['V(1)'], v), _err(r['I(R1)'], v / 1000), _err(r['I(C1)'], -v / 1000)), ""


@_prueba("RL carga (E=10 V, R=100, L=0.1 H, i0=0)")
def _p_rl_carga():
    r = simular("tfin 5e-3 h 1e-5\nV E1 0 1 0 DC 10\nR R1 1 2 100\nL L1 2 0 0.1 0\n")
    t, tau = r.t, 1e-3
    i, vl = 0.1 * (1 - np.exp(-t / tau)), 10 * np.exp(-t / tau)
    return max(_err(r['I(L1)'], i), _err(r['V(2)'], vl), _err(r['I(E1)'], i), _err(r['I(R1)'], i)), ""


@_prueba("RL descarga (i0=50 mA, R=100, L=0.1 H)")
def _p_rl_descarga():
    r = simular("tfin 5e-3 h 1e-5\nR R1 1 0 100\nL L1 1 0 0.1 0.05\n")
    i = 0.05 * np.exp(-r.t / 1e-3)
    return max(_err(r['I(L1)'], i), _err(r['V(1)'], -100 * i), _err(r['I(R1)'], -i)), ""


def _rlc_exacta(t, E, R, L, C):
    a, w0 = R / (2 * L), 1 / math.sqrt(L * C)
    if abs(a - w0) <= 1e-12 * w0:
        vc = E * (1 - (1 + a * t) * np.exp(-a * t))
        i = E / L * t * np.exp(-a * t)
    elif a < w0:
        wd = math.sqrt(w0 ** 2 - a ** 2)
        vc = E * (1 - np.exp(-a * t) * (np.cos(wd * t) + a / wd * np.sin(wd * t)))
        i = E / (L * wd) * np.exp(-a * t) * np.sin(wd * t)
    else:
        rq = math.sqrt(a ** 2 - w0 ** 2)
        s1, s2 = -a + rq, -a - rq
        vc = E * (1 + (s2 * np.exp(s1 * t) - s1 * np.exp(s2 * t)) / (s1 - s2))
        i = E / (L * (s1 - s2)) * (np.exp(s1 * t) - np.exp(s2 * t))
    return vc, i


def _p_rlc(R, metodo='auto'):
    E, L, C = 10.0, 0.01, 1e-6
    r = simular(f"tfin 3e-3 h 2e-6\nV E1 0 1 0 DC {E}\nR R1 1 2 {R}\nL L1 2 3 {L} 0\nC C1 3 0 {C} 0\n",
                metodo=metodo)
    vc, i = _rlc_exacta(r.t, E, R, L, C)
    err = max(_err(r['V(3)'], vc), _err(r['I(L1)'], i), _err(r['I(C1)'], i),
              _err(r['V(2)'], vc + (E - R * i - vc)))
    return err, (f"DOP853 x{r.estadistica['DOP853']}, Radau x{r.estadistica['Radau']}, "
                 f"DOP853->Radau x{r.estadistica['DOP853->Radau']}")


@_prueba("RLC serie subamortiguado (R=50, L=10m, C=1u; alfa=2500, w0=1e4)")
def _p_rlc_sub():
    return _p_rlc(50.0)


@_prueba("RLC serie críticamente amortiguado (R=200 = 2*sqrt(L/C))")
def _p_rlc_crit():
    return _p_rlc(200.0)


@_prueba("RLC serie sobreamortiguado (R=1000)")
def _p_rlc_sobre():
    return _p_rlc(1000.0)


@_prueba("RLC subamortiguado forzando Radau")
def _p_rlc_radau():
    return _p_rlc(50.0, 'Radau')


@_prueba("RLC subamortiguado forzando DOP853")
def _p_rlc_dop():
    return _p_rlc(50.0, 'DOP853')


@_prueba("RLC sobreamortiguado (rígido) forzando DOP853: debe AVISAR de rigidez")
def _p_rlc_dop_rigido():
    r = simular("tfin 3e-3 h 2e-6\nV E1 0 1 0 DC 10\nR R1 1 2 1000\nL L1 2 3 0.01 0\nC C1 3 0 1e-6 0\n",
                metodo='DOP853')
    if not any('rígido' in a for a in r.avisos):
        return math.inf, "no avisó"
    vc, i = _rlc_exacta(r.t, 10.0, 1000.0, 0.01, 1e-6)
    e = _err(r['I(L1)'], i)
    return 0.0, (f"avisa (error real de DOP853 aquí: {e:.1e}; en modo auto el tramo pasa a Radau: "
                 f"ver 'RLC serie sobreamortiguado')")


@_prueba("LC ideal (C=1u v0=5, L=10m): cos, energía constante, periodo 2*pi*sqrt(LC)")
def _p_lc():
    L, C, V0 = 1e-2, 1e-6, 5.0
    w0 = 1 / math.sqrt(L * C)
    T = 2 * math.pi / w0
    r = simular(f"tfin {5 * T!r} h {T / 400!r}\nC C1 1 0 {C} {V0}\nL L1 1 0 {L} 0\n")
    t = r.t
    v, i = V0 * np.cos(w0 * t), V0 * C * w0 * np.sin(w0 * t)
    e1 = max(_err(r['V(1)'], v), _err(r['I(L1)'], i), _err(r['I(C1)'], -i))
    En = 0.5 * C * r['V(1)'] ** 2 + 0.5 * L * r['I(L1)'] ** 2
    E0 = 0.5 * C * V0 ** 2
    e_en = float(np.max(np.abs(En - E0)) / E0)
    kT = np.arange(0, len(t), 400)
    kT2 = np.arange(200, len(t), 400)
    e_per = max(float(np.max(np.abs(r['V(1)'][kT] - V0))) / V0,
                float(np.max(np.abs(r['V(1)'][kT2] + V0))) / V0,
                float(np.max(np.abs(r['I(L1)'][kT]))) / (V0 * C * w0))
    return max(e1, e_en, e_per), f"energía {e_en:.1e}, v(kT)-V0 y v(kT+T/2)+V0: {e_per:.1e}"


@_prueba("Fuente real con r (resistiva, ambas orientaciones)")
def _p_fuente_r():
    r = simular("tfin 1e-3 h 1e-4\nV E1 0 1 50 DC 10\nR R1 1 0 450\nV E2 3 0 20 DC 6\nR R2 3 0 40\n")
    return max(_err(r['V(1)'], 9.0), _err(r['I(E1)'], 0.02), _err(r['I(R1)'], 0.02),
               _err(r['V(3)'], -4.0), _err(r['I(E2)'], 0.1), _err(r['I(R2)'], -0.1)), ""


@_prueba("Fuente real con r cargando un RC (tau = (r+R)C)")
def _p_fuente_r_rc():
    r = simular("tfin 5e-3 h 1e-5\nV E1 0 1 50 DC 10\nR R1 1 2 950\nC C1 2 0 1e-6 0\n")
    t = r.t
    i = 0.01 * np.exp(-t / 1e-3)
    return max(_err(r['V(2)'], 10 * (1 - np.exp(-t / 1e-3))), _err(r['V(1)'], 10 - 50 * i),
               _err(r['I(E1)'], i), _err(r['I(C1)'], i)), ""


@_prueba("Interruptor que cierra (1 ms) y abre (3 ms) un RC con descarga por R2")
def _p_interruptor():
    r = simular("tfin 6e-3 h 1e-5\nV E1 0 1 0 DC 10\nS S1 1 2 0 1e-3 3e-3\n"
                "R R1 2 3 1000\nC C1 3 0 1e-6 2\nR R2 3 0 1000\n")
    t = r.t
    eps = 1e-12
    t2, t1 = 1e-3, 5e-4       # tau2 = R2 C ; tau1 = (R1||R2) C
    v1 = 2 * math.exp(-1e-3 / t2)
    v3 = 5 + (v1 - 5) * math.exp(-2e-3 / t1)
    v = np.where(t < 1e-3 - eps, 2 * np.exp(-t / t2),
                 np.where(t < 3e-3 - eps, 5 + (v1 - 5) * np.exp(-(t - 1e-3) / t1),
                          v3 * np.exp(-(t - 3e-3) / t2)))
    cerrado = (t >= 1e-3 - eps) & (t < 3e-3 - eps)
    iS = np.where(cerrado, (10 - v) / 1000, 0.0)
    v2 = np.where(cerrado, 10.0, v)
    return max(_err(r['V(3)'], v), _err(r['I(S1)'], iS, 0.01), _err(r['V(2)'], v2),
               _err(r['I(C1)'], iS - v / 1000, 0.01)), "límite por la derecha en t1 y t2"


def _fasor_rc(R, C, f):
    w = 2 * math.pi * f
    H = 1 / (1 + 1j * w * R * C)
    return w, abs(H), math.atan2(H.imag, H.real)


@_prueba("SIN en régimen permanente de un RC vs fasor (v0 = v_rp(0))")
def _p_sin_rp():
    R, C, A, f, fase = 1000.0, 1e-7, 10.0, 1000.0, 30.0
    w, mH, aH = _fasor_rc(R, C, f)
    ph = math.radians(fase)
    v0 = A * mH * math.sin(ph + aH)
    r = simular(f"tfin 3e-3 h 5e-6\nV E1 0 1 0 SIN {A} {f} {fase} 0\nR R1 1 2 {R}\nC C1 2 0 {C} {v0!r}\n")
    t = r.t
    v = A * mH * np.sin(w * t + ph + aH)
    i = C * A * mH * w * np.cos(w * t + ph + aH)
    return max(_err(r['V(2)'], v), _err(r['I(C1)'], i), _err(r['V(1)'], A * np.sin(w * t + ph))), \
        f"|H| = {mH:.6f}, fase(H) = {math.degrees(aH):.4f} grados"


@_prueba("SIN sobre RC con v0 del punto de operación (régimen + transitorio exacto)")
def _p_sin_trans():
    R, C, A, f, fase = 1000.0, 1e-7, 10.0, 1000.0, 30.0
    w, mH, aH = _fasor_rc(R, C, f)
    ph = math.radians(fase)
    r = simular(f"tfin 2e-3 h 2e-6\nV E1 0 1 0 SIN {A} {f} {fase} 0\nR R1 1 2 {R}\nC C1 2 0 {C} -\n")
    t, tau = r.t, R * C
    vss0 = A * mH * math.sin(ph + aH)
    v = A * mH * np.sin(w * t + ph + aH) + (A * math.sin(ph) - vss0) * np.exp(-t / tau)
    dv = A * mH * w * np.cos(w * t + ph + aH) - (A * math.sin(ph) - vss0) / tau * np.exp(-t / tau)
    return max(_err(r['V(2)'], v), _err(r['I(C1)'], C * dv)), ""


def _bordes_sqr(A, f, ciclo, nivel, tfin):
    T, c = 1 / f, ciclo / 100
    bordes, vals, k = [], [], 0
    while k * T <= tfin + T:
        bordes += [k * T, (k + c) * T]
        vals += [nivel + A, nivel - A]
        k += 1
    return bordes, vals


@_prueba("Cuadrada 30 % sobre RC (exponencial exacta en cada semiperiodo)")
def _p_sqr():
    r = simular("tfin 10e-3 h 1e-5\nV E1 0 1 0 SQR 5 500 30 1\nR R1 1 2 1000\nC C1 2 0 1e-6 0\n")
    b, vals = _bordes_sqr(5, 500, 30, 1, 10e-3)
    v = _rc_escalones(r.t, 1e-3, 0.0, b, vals)
    e = _trozos(r.t, b, vals)
    return max(_err(r['V(2)'], v), _err(r['V(1)'], e), _err(r['I(C1)'], (e - v) / 1000)), ""


@_prueba("Cuadrada 50 % en régimen periódico: v = -+A*tanh(T/4tau) en cada flanco")
def _p_sqr_periodica():
    A, f, tau = 5.0, 500.0, 1e-3
    T = 1 / f
    Vss = A * math.tanh(T / (4 * tau))
    r = simular(f"tfin 10e-3 h 1e-5\nV E1 0 1 0 SQR {A} {f} 50 0\nR R1 1 2 1000\nC C1 2 0 1e-6 {-Vss!r}\n")
    b, vals = _bordes_sqr(A, f, 50, 0, 10e-3)
    v = _rc_escalones(r.t, tau, -Vss, b, vals)
    k = np.arange(0, len(r.t), 100)     # t = k*T/2
    signo = np.where((k // 100) % 2 == 0, -1.0, 1.0)
    e_fl = float(np.max(np.abs(r['V(2)'][k] - signo * Vss))) / Vss
    return max(_err(r['V(2)'], v), e_fl), f"en los flancos: {e_fl:.1e}"


@_prueba("Cuadrada 700 Hz con flancos entre muestras (eventos fuera de la rejilla)")
def _p_sqr_fuera():
    r = simular("tfin 6e-3 h 1e-5\nV E1 0 1 0 SQR 5 700 50 0\nR R1 1 2 1000\nC C1 2 0 1e-6 0\n")
    b, vals = _bordes_sqr(5, 700, 50, 0, 6e-3)
    v = _rc_escalones(r.t, 1e-3, 0.0, b, vals)
    return _err(r['V(2)'], v), ""


@_prueba("Triangular 25 % sobre RC (respuesta exacta a rampas)")
def _p_tri():
    A, f, c = 5.0, 250.0, 0.25
    T = 1 / f
    r = simular(f"tfin 12e-3 h 1e-5\nV E1 0 1 0 TRI {A} {f} 25 0\nR R1 1 2 1000\nC C1 2 0 1e-6 0\n")
    b, e0, m = [], [], []
    for k in range(5):
        b += [k * T, (k + c) * T]
        e0 += [-A, A]
        m += [2 * A / (c * T), -2 * A / ((1 - c) * T)]
    v = _rc_rampas(r.t, 1e-3, 0.0, b, e0, m)
    e = _trozos_rampa(r.t, b, e0, m)
    return max(_err(r['V(2)'], v), _err(r['V(1)'], e), _err(r['I(C1)'], (e - v) / 1000)), ""


@_prueba("Pulso periódico sobre RL (PUL 0 10 0.5m 1m 3m)")
def _p_pul():
    r = simular("tfin 8e-3 h 1e-5\nV E1 0 1 0 PUL 0 10 0.5e-3 1e-3 3e-3\nR R1 1 2 100\nL L1 2 0 0.1 0\n")
    b = [0.0, 0.5e-3, 1.5e-3, 3.5e-3, 4.5e-3, 6.5e-3, 7.5e-3]
    vals = [0.0, 10.0, 0.0, 10.0, 0.0, 10.0, 0.0]
    u = _rc_escalones(r.t, 1e-3, 0.0, b, vals)       # u = R i
    e = _trozos(r.t, b, vals)
    return max(_err(r['I(L1)'], u / 100), _err(r['V(2)'], e - u), _err(r['V(1)'], e)), ""


@_prueba("J sobre RC en paralelo (J=2 mA DC)")
def _p_j_rc():
    r = simular("tfin 5e-3 h 1e-5\nI J1 0 1 DC 2e-3\nR R1 1 0 1000\nC C1 1 0 1e-6 0\n")
    t = r.t
    v = 2 * (1 - np.exp(-t / 1e-3))
    return max(_err(r['V(1)'], v), _err(r['I(C1)'], 2e-3 * np.exp(-t / 1e-3)),
               _err(r['I(R1)'], v / 1000), _err(r['I(J1)'], 2e-3)), ""


@_prueba("J senoidal sobre RC en régimen permanente vs fasor")
def _p_j_sin():
    R, C, Aj, f = 1000.0, 1e-7, 1e-3, 2000.0
    w = 2 * math.pi * f
    Z = R / (1 + 1j * w * R * C)
    mZ, aZ = abs(Z), math.atan2(Z.imag, Z.real)
    v0 = Aj * mZ * math.sin(aZ)
    r = simular(f"tfin 2e-3 h 2e-6\nI J1 0 1 SIN {Aj} {f} 0 0\nR R1 1 0 {R}\nC C1 1 0 {C} {v0!r}\n")
    v = Aj * mZ * np.sin(w * r.t + aZ)
    return max(_err(r['V(1)'], v), _err(r['I(C1)'], C * Aj * mZ * w * np.cos(w * r.t + aZ))), ""


@_prueba("J sobre C solo (rampa v = v0 + J t / C)")
def _p_j_c():
    r = simular("tfin 1e-3 h 1e-5\nI J1 0 1 DC 1e-3\nC C1 1 0 1e-6 2\n")
    return max(_err(r['V(1)'], 2 + 1000 * r.t), _err(r['I(C1)'], 1e-3)), ""


@_prueba("Amperímetro y voltímetro con r (divisor con cargas de los medidores)")
def _p_medidores_r():
    r = simular("tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 12\nR R1 1 2 1000\nAM A1 2 3 10\n"
                "R R2 3 0 2000\nVM M1 3 0 100000\n")
    Rp = 2000 * 1e5 / (2000 + 1e5)
    i = 12 / (1010 + Rp)
    v3 = i * Rp
    return max(_err(r['I(A1)'], i), _err(r['V(3)'], v3), _err(r['V(2)'], v3 + 10 * i),
               _err(r['I(M1)'], v3 / 1e5), _err(r['I(R2)'], v3 / 2000)), ""


@_prueba("Amperímetro y voltímetro ideales (r = 0)")
def _p_medidores_ideales():
    r = simular("tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 12\nR R1 1 2 1000\nAM A1 2 3 0\n"
                "R R2 3 0 2000\nVM M1 3 0 0\n")
    return max(_err(r['I(A1)'], 4e-3), _err(r['V(3)'], 8.0), _err(r['V(2)'], 8.0),
               _err(r['I(M1)'], 0.0, 4e-3)), ""


@_prueba("Amperímetro con r en serie con un RC y voltímetro ideal sobre C")
def _p_medidores_rc():
    r = simular("tfin 5e-3 h 1e-5\nV E1 0 1 0 DC 10\nAM A1 1 2 50\nR R1 2 3 950\n"
                "C C1 3 0 1e-6 0\nVM M1 3 0 0\n")
    t = r.t
    i = 0.01 * np.exp(-t / 1e-3)
    return max(_err(r['I(A1)'], i), _err(r['V(2)'], 10 - 50 * i),
               _err(r['V(3)'], 10 * (1 - np.exp(-t / 1e-3))), _err(r['I(M1)'], 0.0, 0.01)), ""


@_prueba("Degenerado: dos C en paralelo = C1 + C2")
def _p_c_paralelo():
    r = simular("tfin 8e-3 h 1e-5\nV E1 0 1 0 DC 10\nR R1 1 2 1000\nC C1 2 0 1e-6 0\nC C2 2 0 2e-6 0\n")
    t, tau = r.t, 3e-3
    i = 0.01 * np.exp(-t / tau)
    return max(_err(r['V(2)'], 10 * (1 - np.exp(-t / tau))), _err(r['I(C1)'], i / 3),
               _err(r['I(C2)'], 2 * i / 3), _err(r['I(E1)'], i)), ""


@_prueba("Degenerado: dos L en serie = L1 + L2")
def _p_l_serie():
    r = simular("tfin 5e-3 h 1e-5\nV E1 0 1 0 DC 10\nR R1 1 2 100\nL L1 2 3 0.04 0\nL L2 3 0 0.06 0\n")
    t, tau = r.t, 1e-3
    i = 0.1 * (1 - np.exp(-t / tau))
    return max(_err(r['I(L1)'], i), _err(r['I(L2)'], i), _err(r['V(3)'], 6 * np.exp(-t / tau)),
               _err(r['V(2)'], 10 * np.exp(-t / tau))), ""


@_prueba("Degenerado: C sobre fuente ideal SIN (i_C = C de/dt) con R en paralelo")
def _p_c_sobre_v():
    r = simular("tfin 2e-3 h 5e-6\nV E1 0 1 0 SIN 10 1000 0 0\nC C1 1 0 1e-6 -\nR R1 1 0 1000\n")
    w = 2 * math.pi * 1000
    e, de = 10 * np.sin(w * r.t), 10 * w * np.cos(w * r.t)
    return max(_err(r['V(1)'], e), _err(r['I(C1)'], 1e-6 * de), _err(r['I(E1)'], e / 1000 + 1e-6 * de)), ""


@_prueba("Degenerado: L en serie con fuente de corriente SIN (v_L = L dj/dt)")
def _p_l_serie_j():
    r = simular("tfin 2e-3 h 5e-6\nI J1 0 1 SIN 1e-3 1000 0 0\nL L1 1 2 0.1 -\nR R1 2 0 1000\n")
    w = 2 * math.pi * 1000
    j, dj = 1e-3 * np.sin(w * r.t), 1e-3 * w * np.cos(w * r.t)
    return max(_err(r['I(L1)'], j), _err(r['V(2)'], 1000 * j), _err(r['V(1)'], 1000 * j + 0.1 * dj)), ""


@_prueba("Degenerado: lazo E-C1-C2 con v0 compatibles y R sobre C2")
def _p_lazo_cv():
    r = simular("tfin 8e-3 h 1e-5\nV E1 0 1 0 DC 10\nC C1 1 2 1e-6 4\nC C2 2 0 3e-6 6\nR R1 2 0 1000\n")
    ex = np.exp(-r.t / 4e-3)
    return max(_err(r['V(2)'], 6 * ex), _err(r['I(C1)'], 1.5e-3 * ex), _err(r['I(C2)'], -4.5e-3 * ex),
               _err(r['I(R1)'], 6e-3 * ex), _err(r['I(E1)'], 1.5e-3 * ex)), ""


@_prueba("Fuente ideal directamente sobre L con i0 (rampa i = E t / L)")
def _p_v_sobre_l():
    r = simular("tfin 1e-3 h 1e-5\nV E1 0 1 0 DC 5\nL L1 1 0 1e-3 0\n")
    return max(_err(r['I(L1)'], 5000 * r.t), _err(r['I(E1)'], 5000 * r.t), _err(r['V(1)'], 5.0)), ""


@_prueba("Subcircuito flotante (sin nodo 0): referencia en el nodo más bajo + aviso")
def _p_flotante():
    r = simular("tfin 3e-3 h 1e-5\nC C1 1 2 1e-6 5\nR R1 1 2 1000\n")
    v = 5 * np.exp(-r.t / 1e-3)
    err = max(_err(r['V(1)'], 0.0, 5.0), _err(r['V(2)'], -v), _err(r['I(C1)'], -v / 1000),
              _err(r['I(R1)'], v / 1000))
    if not any('referencia' in a and '{1, 2}' in a for a in r.avisos):
        return math.inf, "falta el aviso de subcircuito flotante"
    return err, "aviso: " + r.avisos[0][:60] + "..."


@_prueba("RC rígido (tau = 1 us, h = 10 us, SIN, v0 del p. de op.): auto y Radau forzado")
def _p_rigido():
    w, mH, aH = _fasor_rc(1.0, 1e-6, 1000.0)
    tau = 1e-6
    vss0 = mH * math.sin(aH)
    errs, notas = [], []
    for met in ('auto', 'Radau'):
        r = simular("tfin 2e-3 h 1e-5\nV E1 0 1 0 SIN 1 1000 0 0\nR R1 1 2 1\nC C1 2 0 1e-6 -\n", metodo=met)
        t = r.t
        v = mH * np.sin(w * t + aH) - vss0 * np.exp(-t / tau)
        dv = mH * w * np.cos(w * t + aH) + vss0 / tau * np.exp(-t / tau)
        errs.append(max(_err(r['V(2)'], v), _err(r['I(C1)'], 1e-6 * dv)))
        notas.append(f"{met}: {errs[-1]:.1e} (DOP853 x{r.estadistica['DOP853']}, Radau x{r.estadistica['Radau']})")
    return max(errs), "; ".join(notas)


@_prueba("CLI: netlist -> CSV (cabecera, orden de columnas y valores)")
def _p_cli():
    import os
    import tempfile
    net = "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 10\nR R1 1 2 1000\nC C1 2 0 1e-6 0  # comentario\nVM M1 2 0 0\n"
    with tempfile.TemporaryDirectory() as d:
        pn, pc = os.path.join(d, 'n.txt'), os.path.join(d, 's.csv')
        with open(pn, 'w', encoding='utf-8') as fh:
            fh.write(net)
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            rc = main([pn, pc])
        with open(pc, encoding='utf-8') as fh:
            lineas = fh.read().splitlines()
    cab = lineas[0].split(',')
    esperada = ['t', 'V(1)', 'V(2)', 'I(E1)', 'I(R1)', 'I(C1)', 'I(M1)']
    if rc != 0 or cab != esperada:
        return math.inf, f"rc={rc}, cabecera {cab}"
    d = np.array([[float(x) for x in ln.split(',')] for ln in lineas[1:]])
    if d.shape != (11, 7):
        return math.inf, f"forma {d.shape}"
    t = d[:, 0]
    return max(_err(t, np.arange(11) * 1e-4), _err(d[:, 2], 10 * (1 - np.exp(-t / 1e-3)))), \
        "cabecera " + ",".join(cab)


@_prueba("Cierre sin salto: S une dos RC de igual tau y igual tensión (lazo C1-S-C2 nuevo)")
def _p_cierre_continuo():
    r = simular("tfin 3e-3 h 1e-5\nC C1 1 0 1e-6 5\nR R1 1 0 1000\nC C2 2 0 2e-6 5\nR R2 2 0 500\n"
                "S S1 1 2 0 1e-3 0\n")
    v = 5 * np.exp(-r.t / 1e-3)
    return max(_err(r['V(1)'], v), _err(r['V(2)'], v), _err(r['I(S1)'], 0.0, 5e-3),
               _err(r['I(C2)'], -2 * v / 1000)), "la conmutación se acepta (no exige impulso)"


@_prueba("Interruptor que abre un RL con camino de descarga (salto de tensión permitido)")
def _p_abre_rl():
    r = simular("tfin 2e-3 h 1e-5\nV E1 0 1 0 DC 10\nS S1 1 2 1 1e-3 0\nR R1 2 3 100\n"
                "L L1 3 0 0.1 0\nR R2 3 0 1000\n")
    t, eps = r.t, 1e-12
    t1 = 0.1 / (100 * 1000 / 1100)          # L / (R1 || R2)
    i1 = 0.1 * (1 - math.exp(-1e-3 / t1))
    antes = t < 1e-3 - eps
    i = np.where(antes, 0.1 * (1 - np.exp(-t / t1)), i1 * np.exp(-(t - 1e-3) / 1e-4))
    v3 = np.where(antes, 0.01 / t1 * np.exp(-t / t1), -1000 * i)      # L di/dt ; luego -R2 i
    iS = np.where(antes, (10 - v3) / 100, 0.0)
    return max(_err(r['I(L1)'], i), _err(r['V(3)'], v3), _err(r['I(S1)'], iS), _err(r['I(R1)'], iS),
               _err(r['V(2)'], np.where(antes, 10.0, v3))), \
        f"V(3) salta de {0.01 / t1 * math.exp(-1e-3 / t1):+.4g} V a {-1000 * i1:+.4g} V en t1"


@_prueba("Subcircuito flotante que se conecta a tierra al cerrar S (aviso solo antes de t1)")
def _p_flotante_s():
    r = simular("tfin 3e-3 h 1e-5\nC C1 1 2 1e-6 5\nR R1 1 2 1000\nS S1 1 0 0 1e-3 0\n")
    v = 5 * np.exp(-r.t / 1e-3)
    err = max(_err(r['V(1)'], 0.0, 5.0), _err(r['V(2)'], -v), _err(r['I(S1)'], 0.0, 5e-3))
    ok = len(r.avisos) == 1 and '[0, 0.001)' in r.avisos[0] and '{1, 2}' in r.avisos[0]
    return (err if ok else math.inf), ("aviso: " + r.avisos[0][:70] + "...") if r.avisos else "sin aviso"


@_prueba("Escalón PUL (ancho = 0) sobre RC y C con v0 '-' en paralelo con C con v0 dado")
def _p_escalon():
    r = simular("tfin 4e-3 h 1e-5\nV E1 0 1 0 PUL 0 5 1e-3 0 0\nR R1 1 2 1000\nC C1 2 0 1e-6 -\n"
                "C C2 2 0 2e-6 0\n")
    v = _rc_escalones(r.t, 3e-3, 0.0, [0.0, 1e-3], [0.0, 5.0])
    return max(_err(r['V(2)'], v), _err(r['V(1)'], _trozos(r.t, [0.0, 1e-3], [0.0, 5.0]))), ""


@_prueba("Sufijos SI (1k, 1u, 10m, 2meg) = notación científica")
def _p_sufijos():
    a = simular("tfin 5m h 10u\nV E1 0 1 0 DC 10\nR R1 1 2 1k\nC C1 2 0 1u 0\nL L1 2 3 10m 0\nR R2 3 0 2meg\n")
    b = simular("tfin 5e-3 h 1e-5\nV E1 0 1 0 DC 10\nR R1 1 2 1000\nC C1 2 0 1e-6 0\nL L1 2 3 0.01 0\n"
                "R R2 3 0 2e6\n")
    if a.datos.shape != b.datos.shape:
        return math.inf, "formas distintas"
    return float(np.max(np.abs(a.datos - b.datos))), "idénticos"


_MIXTO = """tfin 4e-3 h 1e-6
V  E1 0 1 0  SIN 10 500 20 1
V  E2 0 5 25 SQR 3 1000 40 2
I  J1 0 6 TRI 2e-3 750 30 1e-3
R  R1 1 2 220
AM A1 2 3 0
C  C1 3 0 2.2e-6 0
C  C2 3 0 1e-6 0
L  L1 3 4 0.02 0
R  R2 4 0 150
AM A2 5 4 330
S  S1 4 6 0 1.3e-3 2.9e-3
R  R4 6 0 470
L  L2 6 7 0.01 0
L  L3 7 0 0.015 0
VM M1 3 5 0
VM M2 4 0 1e5
"""


@_prueba("Consistencia física de un circuito mixto (LCK, leyes de elemento, C y L por diferencias)")
def _p_consistencia():
    circ = Circuito(_MIXTO)
    r = simular(_MIXTO)
    t, h = r.t, circ.h

    def V(nd):
        return r[f'V({nd})'] if nd != 0 else np.zeros_like(t)

    I = {e.nombre: r[f'I({e.nombre})'] for e in circ.elementos}
    imax = max(float(np.max(np.abs(x))) for x in I.values())
    vmax = max(float(np.max(np.abs(V(nd)))) for nd in circ.nodos)
    e_lck = max(float(np.max(np.abs(sum(I[e.nombre] for e in circ.elementos if e.a == nd)
                                    - sum(I[e.nombre] for e in circ.elementos if e.b == nd)))) / imax
                for nd in circ.nodos)
    tref = t + 1e-3 * h                       # límite por la derecha de las formas
    fuente = {e.nombre: np.array([e.forma.pieza(tr)[0](tt) for tt, tr in zip(t, tref)])
              for e in circ.fuentes}
    e_ley = 0.0
    eventos = []
    for e in circ.elementos:
        u, i = V(e.a) - V(e.b), I[e.nombre]
        if e.tipo == 'R' or (e.tipo in ('AM', 'VM') and e.r > 0):
            d = (u - (e.val if e.tipo == 'R' else e.r) * i) / vmax
        elif e.tipo == 'AM':
            d = u / vmax
        elif e.tipo == 'VM':
            d = i / imax
        elif e.tipo == 'V':
            d = (-u - (fuente[e.nombre] - e.r * i)) / vmax
        elif e.tipo == 'I':
            d = (i - fuente[e.nombre]) / imax
        elif e.tipo == 'S':
            cerr = np.array([e.estado(tt) for tt in tref])
            d = np.where(cerr, u / vmax, i / imax)
            eventos += [e.t1, e.t2]
        else:
            continue
        e_ley = max(e_ley, float(np.max(np.abs(d))))
    for f in circ.fuentes:
        eventos += f.forma.eventos(circ.tfin + h)
    lejos = np.ones(len(t), dtype=bool)
    lejos[:3] = lejos[-3:] = False
    for te in eventos:
        lejos &= np.abs(t - te) > 2.5 * h
    k = np.nonzero(lejos)[0]

    def deriv(x):
        return (-x[k + 2] + 8 * x[k + 1] - 8 * x[k - 1] + x[k - 2]) / (12 * h)

    e_din = 0.0
    for e in circ.elementos:
        u, i = V(e.a) - V(e.b), I[e.nombre]
        if e.tipo == 'C':
            e_din = max(e_din, float(np.max(np.abs(i[k] - e.val * deriv(u)))) / imax)
        elif e.tipo == 'L':
            e_din = max(e_din, float(np.max(np.abs(u[k] - e.val * deriv(i)))) / vmax)
    return max(e_lck, e_ley, e_din), (f"LCK {e_lck:.1e}, leyes algebraicas {e_ley:.1e}, "
                                      f"i=C du/dt y u=L di/dt (dif. finitas de 4º orden) {e_din:.1e}")


# ---- Pruebas añadidas en la revisión adversarial (soluciones exactas derivadas aparte) ----

@_prueba("Puente equilibrado con L en la diagonal (i_L = 0 siempre), a 10 V y a 100 kV")
def _p_puente_l():
    R, C, L, f = 1000.0, 1e-6, 0.05, 300.0
    tau = R * C / 2                     # v' = (e/2 - v)/tau en los nodos 2 y 3
    w, mH, aH = _fasor_rc(tau, 1.0, f)
    errs = []
    for A in (10.0, 1e5):
        r = simular(f"tfin 5e-3 h 1e-5\nV E1 0 1 0 SIN {A} {f} 0 0\nR R1 1 2 {R}\nR R2 1 3 {R}\n"
                    f"R R3 2 0 {R}\nR R4 3 0 {R}\nC C1 2 0 {C} -\nC C2 3 0 {C} -\nL L1 2 3 {L} -\n")
        t = r.t
        e = A * np.sin(w * t)
        v = A / 2 * mH * (np.sin(w * t + aH) - math.sin(aH) * np.exp(-t / tau))
        iesc = A / R
        errs.append(max(_err(r['V(2)'], v), _err(r['V(3)'], v), _err(r['I(L1)'], 0.0, iesc),
                        _err(r['I(C1)'], (e - 2 * v) / R, iesc), _err(r['I(E1)'], 2 * (e - v) / R)))
    return max(errs), "antes se colgaba: atol de i_L = 1e-30 porque i_L es nula en todo el intervalo"


@_prueba("Puente R-L equilibrado con C en la diagonal (v_C = 0 siempre)")
def _p_puente_c():
    R, L, C, f, A = 100.0, 0.05, 1e-6, 300.0, 10.0
    tau = L / R
    w, mH, aH = _fasor_rc(tau, 1.0, f)
    r = simular(f"tfin 5e-3 h 1e-5\nV E1 0 1 0 SIN {A} {f} 0 0\nR R1 1 2 {R}\nR R2 1 3 {R}\n"
                f"L L3 2 0 {L} -\nL L4 3 0 {L} -\nC C1 2 3 {C} -\n")
    t = r.t
    e = A * np.sin(w * t)
    i = A / R * mH * (np.sin(w * t + aH) - math.sin(aH) * np.exp(-t / tau))
    return max(_err(r['I(L3)'], i), _err(r['I(L4)'], i), _err(r['V(2)'], e - R * i, A),
               _err(r['V(3)'], e - R * i, A), _err(r['I(C1)'], 0.0, A / R), _err(r['I(E1)'], 2 * i)), ""


@_prueba("Puentes equilibrados: S cortocircuita la C de la diagonal (v_C = 0) / abre la rama de la L (i_L = 0)")
def _p_puente_conmuta():
    f, A = 300.0, 10.0
    # 1) puente R-L con C en la diagonal; S la cortocircuita en 2.5 ms: v_C = 0, no hay impulso
    R, L = 100.0, 0.05
    w, mH, aH = _fasor_rc(L / R, 1.0, f)
    r = simular(f"tfin 5e-3 h 1e-5\nV E1 0 1 0 SIN {A} {f} 0 0\nR R1 1 2 {R}\nR R2 1 3 {R}\nL L3 2 0 {L} -\n"
                f"L L4 3 0 {L} -\nC C1 2 3 1e-6 -\nS S1 2 3 0 2.5e-3 0\n")
    t = r.t
    i = A / R * mH * (np.sin(w * t + aH) - math.sin(aH) * np.exp(-t * R / L))
    e1 = max(_err(r['I(L3)'], i), _err(r['V(2)'], A * np.sin(w * t) - R * i, A), _err(r['I(S1)'], 0.0, A / R),
             _err(r['I(C1)'], 0.0, A / R))
    # 2) puente R-C con L en la diagonal en serie con S, que se abre en 2.5 ms: i_L = 0, no hay impulso
    R, C = 1000.0, 1e-6
    w, mH, aH = _fasor_rc(R * C / 2, 1.0, f)
    r = simular(f"tfin 5e-3 h 1e-5\nV E1 0 1 0 SIN {A} {f} 0 0\nR R1 1 2 {R}\nR R2 1 3 {R}\nR R3 2 0 {R}\n"
                f"R R4 3 0 {R}\nC C1 2 0 {C} -\nC C2 3 0 {C} -\nL L1 2 4 0.05 -\nS S1 4 3 1 2.5e-3 0\n")
    t = r.t
    v = A / 2 * mH * (np.sin(w * t + aH) - math.sin(aH) * np.exp(-t / (R * C / 2)))
    e2 = max(_err(r['V(2)'], v), _err(r['V(3)'], v), _err(r['I(L1)'], 0.0, A / R), _err(r['I(S1)'], 0.0, A / R))
    return max(e1, e2), "antes se rechazaban como 'impulso' (el redondeo, ~1e-17, frente a tolerancia ~1e-21)"


@_prueba("v0 = 0 / i0 = 0 frente a SIN de fase 180 y 360 (e(0) = 1e-16 por redondeo): se aceptan")
def _p_sin_fase_180():
    w = 2 * math.pi * 100
    errs = []
    for fase in (180, 360):
        ph = math.radians(fase)
        r = simular(f"tfin 2e-3 h 1e-5\nV E1 0 1 0 SIN 5 100 {fase} 0\nC C1 1 0 1e-6 0\nR R1 1 0 100\n")
        e, de = 5 * np.sin(w * r.t + ph), 5 * w * np.cos(w * r.t + ph)
        errs.append(max(_err(r['V(1)'], e, 5.0), _err(r['I(C1)'], 1e-6 * de), _err(r['I(E1)'], e / 100 + 1e-6 * de)))
        r = simular(f"tfin 2e-3 h 1e-5\nI J1 0 1 SIN 1e-3 100 {fase} 0\nL L1 1 2 0.1 0\nR R1 2 0 100\n")
        j, dj = 1e-3 * np.sin(w * r.t + ph), 1e-3 * w * np.cos(w * r.t + ph)
        errs.append(max(_err(r['I(L1)'], j, 1e-3), _err(r['V(1)'], 100 * j + 0.1 * dj)))
    return max(errs), "antes: 'punto de operación imposible' (tolerancia relativa al valor en t = 0, ~1e-16)"


@_prueba("Tanque LC sin energía junto a un lazo E-C-C con fuente real TRI (V, I del tanque = 0)")
def _p_tanque():
    E2, C6, C3, rr, A, f, c, niv = 7.43, 1.64e-7, 4.92e-7, 85.76, 6.37, 333.3, 0.13, 3.62
    T = 1 / f
    r = simular(f"tfin 4e-3 h 1e-5\nC C5 5 0 4.33e-7 -\nV E2 0 2 0 DC {E2}\nC C6 2 3 {C6} -\n"
                f"C C3 3 0 {C3} -\nV E3 3 2 {rr} TRI {A} {f} 13 {niv}\nL L1 5 0 0.0587 -\n")
    t = r.t
    tau = rr * (C3 + C6)               # w = V(3):  w' = (E2 - e3 - w) / tau
    b, e0, m = [], [], []
    for k in range(3):
        b += [k * T, (k + c) * T]
        e0 += [E2 - (niv - A), E2 - (niv + A)]
        m += [-2 * A / (c * T), 2 * A / ((1 - c) * T)]
    wv = _rc_rampas(t, tau, E2 - (niv - A), b, e0, m)
    dw = (_trozos_rampa(t, b, e0, m) - wv) / tau
    i3 = -(C3 + C6) * dw
    iesc = float(np.max(np.abs(i3)))
    err = max(_err(r['V(3)'], wv), _err(r['V(2)'], E2), _err(r['V(5)'], 0.0, E2),
              _err(r['I(L1)'], 0.0, iesc), _err(r['I(C5)'], 0.0, iesc), _err(r['I(C3)'], C3 * dw, iesc),
              _err(r['I(C6)'], -C6 * dw, iesc), _err(r['I(E3)'], i3, iesc), _err(r['I(E2)'], -C6 * dw - i3, iesc))
    exacto = float(np.max(np.abs(r['V(5)']))) == 0.0 and float(np.max(np.abs(r['I(L1)']))) == 0.0
    return (err if exacto else math.inf), ("tanque exactamente en 0" if exacto else "el tanque no quedó en 0")


@_prueba("RLC paralelo con J escalón (PUL, td fuera de la rejilla)")
def _p_rlc_paralelo_j():
    I0, R, L, C, td = 2e-3, 500.0, 0.01, 1e-6, 0.3725e-3
    r = simular(f"tfin 3e-3 h 1e-5\nI J1 0 1 PUL 0 {I0} {td} 0 0\nR R1 1 0 {R}\nL L1 1 0 {L} 0\n"
                f"C C1 1 0 {C} 0\n")
    t = r.t
    a, w0 = 1 / (2 * R * C), 1 / math.sqrt(L * C)
    wd = math.sqrt(w0 ** 2 - a ** 2)
    s = np.maximum(t - td, 0.0)
    on = t >= td
    v = np.where(on, I0 / (C * wd) * np.exp(-a * s) * np.sin(wd * s), 0.0)
    iL = np.where(on, I0 * (1 - np.exp(-a * s) * (np.cos(wd * s) + a / wd * np.sin(wd * s))), 0.0)
    j = np.where(on, I0, 0.0)
    return max(_err(r['V(1)'], v), _err(r['I(L1)'], iL, I0), _err(r['I(C1)'], j - v / R - iL, I0),
               _err(r['I(J1)'], j, I0)), ""


@_prueba("Interruptor cerrado -> abierto (t1 fuera de la rejilla) -> cerrado (t2 en la rejilla)")
def _p_interruptor_doble():
    t1, t2, C = 1.2345e-3, 2.5e-3, 1e-6
    r = simular(f"tfin 5e-3 h 1e-5\nV E1 0 1 0 DC 10\nS S1 1 2 1 {t1} {t2}\nR R1 2 3 1000\n"
                f"C C1 3 0 {C} 0\nR R2 3 0 4000\n")
    t, eps = r.t, 1e-12
    ta1, ta2 = 800 * C, 4000 * C             # cerrado: Thevenin 8 V, 800 ohm; abierto: descarga por R2
    v1 = 8 * (1 - math.exp(-t1 / ta1))
    v2 = v1 * math.exp(-(t2 - t1) / ta2)
    v = np.where(t < t1 - eps, 8 * (1 - np.exp(-t / ta1)),
                 np.where(t < t2 - eps, v1 * np.exp(-(t - t1) / ta2), 8 + (v2 - 8) * np.exp(-(t - t2) / ta1)))
    cerr = (t < t1 - eps) | (t >= t2 - eps)
    iS = np.where(cerr, (10 - v) / 1000, 0.0)
    return max(_err(r['V(3)'], v), _err(r['V(2)'], np.where(cerr, 10.0, v)), _err(r['I(S1)'], iS, 0.01),
               _err(r['I(C1)'], iS - v / 4000, 0.01)), "I(S1) en t2 = límite por la derecha (cerrado)"


@_prueba("Lazo E-C1-C2 con fugas (R en cada C) y SIN con nivel: du/dt exacta, v0 dados y '-'")
def _p_divisor_capacitivo():
    A, f, fase, niv = 10.0, 1000.0, 30.0, 2.0
    C1, C2, R1, R2 = 1e-6, 3e-6, 2000.0, 1000.0
    w, ph = 2 * math.pi * f, math.radians(fase)
    G, Ct = 1 / R1 + 1 / R2, C1 + C2
    tau = Ct / G
    H = (1j * w * C1 + 1 / R1) / (1j * w * Ct + G)
    mH, aH = abs(H), math.atan2(H.imag, H.real)
    udc = niv * (1 / R1) / G
    errs = []
    for v1, v2 in (('4', '3'), ('-', '-')):
        r = simular(f"tfin 5e-3 h 1e-5\nV E1 0 1 0 SIN {A} {f} {fase} {niv}\nC C1 1 2 {C1} {v1}\n"
                    f"R R1 1 2 {R1}\nC C2 2 0 {C2} {v2}\nR R2 2 0 {R2}\n")
        t = r.t
        e0 = niv + A * math.sin(ph)
        u0 = 3.0 if v2 == '3' else e0 * R2 / (R1 + R2)
        K = u0 - udc - A * mH * math.sin(ph + aH)
        u = udc + A * mH * np.sin(w * t + ph + aH) + K * np.exp(-t / tau)
        du = A * mH * w * np.cos(w * t + ph + aH) - K / tau * np.exp(-t / tau)
        e = niv + A * np.sin(w * t + ph)
        de = A * w * np.cos(w * t + ph)
        iesc = float(np.max(np.abs(C1 * (de - du) + (e - u) / R1)))
        errs.append(max(_err(r['V(2)'], u, 12.0), _err(r['I(C2)'], C2 * du, iesc), _err(r['I(R2)'], u / R2, iesc),
                        _err(r['I(C1)'], C1 * (de - du), iesc), _err(r['I(E1)'], C1 * (de - du) + (e - u) / R1, iesc)))
    return max(errs), ""


@_prueba("Corte J-L1-L2 (J SIN con nivel) con R en cada rama: dj/dt exacta, i0 dados y '-'")
def _p_corte_inductivo():
    A, f, fase, niv = 5e-3, 700.0, 20.0, 1e-3
    L1, L2, R2, R3 = 0.02, 0.05, 100.0, 300.0
    w, ph = 2 * math.pi * f, math.radians(fase)
    Lt, Rt = L1 + L2, R2 + R3
    tau = Lt / Rt
    H = (1j * w * L2 + R3) / (1j * w * Lt + Rt)
    mH, aH = abs(H), math.atan2(H.imag, H.real)
    idc = niv * R3 / Rt
    j0 = niv + A * math.sin(ph)
    errs = []
    for i1t, i2t in (('0.002', repr(j0 - 0.002)), ('-', '-')):
        r = simular(f"tfin 5e-3 h 1e-5\nI J1 0 1 SIN {A} {f} {fase} {niv}\nL L1 1 2 {L1} {i1t}\n"
                    f"L L2 1 3 {L2} {i2t}\nR R2 2 0 {R2}\nR R3 3 0 {R3}\n")
        t = r.t
        i10 = 0.002 if i1t != '-' else j0 * R3 / Rt
        K = i10 - idc - A * mH * math.sin(ph + aH)
        i1 = idc + A * mH * np.sin(w * t + ph + aH) + K * np.exp(-t / tau)
        di1 = A * mH * w * np.cos(w * t + ph + aH) - K / tau * np.exp(-t / tau)
        j = niv + A * np.sin(w * t + ph)
        v1 = R2 * i1 + L1 * di1
        vesc = float(np.max(np.abs(v1)))
        errs.append(max(_err(r['I(L1)'], i1, 6e-3), _err(r['I(L2)'], j - i1, 6e-3), _err(r['V(2)'], R2 * i1, vesc),
                        _err(r['V(3)'], R3 * (j - i1), vesc), _err(r['V(1)'], v1, vesc)))
    return max(errs), ""


@_prueba("TRI ideal directamente sobre C (i_C cuadrada) y J TRI en serie con L (v_L cuadrada)")
def _p_tri_derivadas():
    A, f, c, niv = 4.0, 500.0, 0.3, 1.0
    T = 1 / f
    b, e0, m = [], [], []
    for k in range(4):
        b += [k * T, (k + c) * T]
        e0 += [niv - A, niv + A]
        m += [2 * A / (c * T), -2 * A / ((1 - c) * T)]
    r = simular(f"tfin 5e-3 h 1e-5\nV E1 0 1 0 TRI {A} {f} 30 {niv}\nC C1 1 0 2e-6 -\nR R1 1 0 1000\n")
    e, de = _trozos_rampa(r.t, b, e0, m), _trozos_rampa(r.t, b, m, [0.0] * len(m))
    err1 = max(_err(r['V(1)'], e), _err(r['I(C1)'], 2e-6 * de), _err(r['I(E1)'], e / 1000 + 2e-6 * de))
    s = 1e-3 / A                          # J = TRI de 1 mA con el mismo perfil
    r = simular(f"tfin 5e-3 h 1e-5\nI J1 0 1 TRI {A * s} {f} 30 {niv * s}\nL L1 1 2 0.05 -\nR R1 2 0 200\n")
    j, dj = s * _trozos_rampa(r.t, b, e0, m), s * _trozos_rampa(r.t, b, m, [0.0] * len(m))
    err2 = max(_err(r['I(L1)'], j), _err(r['V(2)'], 200 * j), _err(r['V(1)'], 200 * j + 0.05 * dj))
    return max(err1, err2), f"C: {err1:.1e}; L: {err2:.1e}"


@_prueba("LC serie excitado en su resonancia: i = A t sin(w0 t) / (2L)")
def _p_resonancia():
    L, C, A = 0.01, 1e-6, 1.0
    w0 = 1 / math.sqrt(L * C)
    r = simular(f"tfin 10e-3 h 1e-5\nV E1 0 1 0 SIN {A} {w0 / (2 * math.pi)!r} 0 0\nL L1 1 2 {L} 0\n"
                f"C C1 2 0 {C} 0\n")
    t = r.t
    i = A / (2 * L) * t * np.sin(w0 * t)
    q = A / (2 * L * w0 ** 2) * np.sin(w0 * t) - A / (2 * L * w0) * t * np.cos(w0 * t)
    return max(_err(r['I(L1)'], i), _err(r['V(2)'], q / C)), ""


@_prueba("Subcircuito flotante {3,4,5} junto a otro con tierra, y circuito entero sin nodo 0")
def _p_flotantes():
    r = simular("tfin 6e-3 h 1e-5\nV E1 0 1 0 DC 5\nR R1 1 0 1000\nC C1 3 4 1e-6 2\nR R2 4 5 1000\n"
                "R R3 5 3 1000\n")
    v = 2 * np.exp(-r.t / 2e-3)
    e1 = max(_err(r['V(1)'], 5.0), _err(r['V(3)'], 0.0, 5.0), _err(r['V(4)'], -v, 5.0),
             _err(r['V(5)'], -v / 2, 5.0), _err(r['I(C1)'], -v / 2000), _err(r['I(R1)'], 5e-3))
    if not any('{3, 4, 5}' in a for a in r.avisos):
        return math.inf, "falta el aviso del subcircuito {3, 4, 5}"
    w, mH, aH = _fasor_rc(1000.0, 1e-6, 500.0)
    r = simular("tfin 4e-3 h 1e-5\nV E1 2 3 0 SIN 3 500 0 0\nR R1 3 4 1000\nC C1 4 2 1e-6 0\n")
    t = r.t
    u = 3 * mH * (np.sin(w * t + aH) - math.sin(aH) * np.exp(-t / 1e-3))
    e = 3 * np.sin(w * t)
    e2 = max(_err(r['V(2)'], 0.0, 3.0), _err(r['V(3)'], e), _err(r['V(4)'], u, 3.0),
             _err(r['I(E1)'], (e - u) / 1000, 3e-3))
    return max(e1, e2), f"con tierra: {e1:.1e}; sin nodo 0 (ref. nodo 2): {e2:.1e}"


@_prueba("P. de op.: C colgante con v0, C '-' en serie con C con v0, voltímetro ideal en serie con L")
def _p_op_varios():
    r1 = simular("tfin 2e-3 h 1e-4\nV E1 0 1 0 DC 4\nR R1 1 2 100\nR R2 2 0 100\nC C1 2 3 1e-6 1.5\n")
    r2 = simular("tfin 2e-3 h 1e-4\nV E1 0 1 0 DC 10\nR R1 1 2 1000\nC C1 2 3 1e-6 2\nC C2 3 0 1e-6 -\n")
    r3 = simular("tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 5\nR R1 1 2 100\nL L1 2 3 0.1 -\nVM M1 3 0 0\n")
    return max(_err(r1['V(3)'], 0.5), _err(r1['I(C1)'], 0.0, 0.02), _err(r2['V(3)'], 8.0),
               _err(r2['I(C2)'], 0.0, 0.01), _err(r3['V(3)'], 5.0), _err(r3['I(L1)'], 0.0, 0.05)), ""


_PARALELO_C = "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 10\nR R1 1 2 1000\nC C1 2 0 1e-6 0\nC C2 2 0 2e-6 0\n"

_RECHAZOS = [
    ("Rechazo: dos fuentes de tensión ideales en paralelo",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 5\nV E2 0 1 0 DC 3\nR R1 1 0 10\n", r"lazo", {}),
    ("Rechazo: interruptor cerrado en paralelo con amperímetro ideal",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 5\nR R1 1 2 10\nS S1 2 0 1 0 0\nAM A1 2 0 0\n", r"lazo", {}),
    ("Rechazo: fuente de corriente contra un interruptor abierto",
     "tfin 1e-3 h 1e-4\nI J1 0 1 DC 1e-3\nS S1 1 2 0 0 0\nR R1 2 0 10\n", r"corte", {}),
    ("Rechazo: fuente de corriente que se queda sin camino al abrir S",
     "tfin 2e-3 h 1e-4\nI J1 0 1 DC 1e-3\nS S1 1 2 1 1e-3 0\nR R1 2 0 10\n", r"corte", {}),
    ("Rechazo: C en paralelo con v0 incompatibles",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 1\nR R1 1 2 1\nC C1 2 0 1e-6 1\nC C2 2 0 1e-6 2\n", r"incompatib", {}),
    ("Rechazo: C con v0 distinto de la fuente ideal en paralelo",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 5\nC C1 1 0 1e-6 3\nR R1 1 0 1\n", r"incompatib", {}),
    ("Rechazo: L en serie con i0 incompatibles",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 1\nR R1 1 2 1\nL L1 2 3 1e-3 0.1\nL L2 3 0 1e-3 0.2\n",
     r"incompatib", {}),
    ("Rechazo: cerrar un interruptor sobre un C cargado (impulso)",
     "tfin 2e-3 h 1e-4\nC C1 1 0 1e-6 5\nR R1 1 0 1000\nS S1 1 0 0 1e-3 0\n", r"impulso", {}),
    ("Rechazo: abrir el circuito de una L con corriente (impulso)",
     "tfin 2e-3 h 1e-4\nV E1 0 1 0 DC 10\nR R1 1 2 100\nS S1 2 3 1 1e-3 0\nL L1 3 0 0.1 0\n", r"impulso", {}),
    ("Rechazo: cuadrada ideal directamente sobre un C (impulso en el flanco)",
     "tfin 2e-3 h 1e-5\nV E1 0 1 0 SQR 5 1000 50 0\nC C1 1 0 1e-6 -\nR R1 1 0 1000\n", r"impulso", {}),
    ("Rechazo: cuadrada de corriente en serie con una L (impulso)",
     "tfin 2e-3 h 1e-5\nI J1 0 1 SQR 1e-3 1000 50 0\nL L1 1 2 0.1 -\nR R1 2 0 100\n", r"impulso", {}),
    ("Rechazo: L sin i0 en paralelo con interruptor cerrado (i_L indeterminada)",
     "tfin 2e-3 h 1e-4\nV E1 0 1 0 DC 1\nR R1 1 2 1\nL L1 2 0 1e-3 -\nS S1 2 0 1 1e-3 0\n", r"i0", {}),
    ("Rechazo: C sin v0 aislado por un interruptor abierto (v_C indeterminada)",
     "tfin 2e-3 h 1e-4\nV E1 0 1 0 DC 5\nS S1 1 2 0 1e-3 0\nR R1 2 3 1000\nC C1 3 0 1e-6 -\n", r"v0", {}),
    ("Rechazo: dos C sin v0 en serie (reparto de tensión indeterminado)",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 5\nR R1 1 2 1000\nC C1 2 3 1e-6 -\nC C2 3 0 1e-6 -\n", r"v0", {}),
    ("Rechazo: fuente de corriente sobre C sin v0 (no hay punto de operación)",
     "tfin 1e-3 h 1e-4\nI J1 0 1 DC 1e-3\nC C1 1 0 1e-6 -\n", r"punto de operaci", {}),
    ("Rechazo: L sin i0 en paralelo con fuente de tensión (no hay punto de operación)",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 5\nL L1 1 0 1e-3 -\n", r"punto de operaci", {}),
    ("Rechazo: LC con C cargado y L sin i0 (en DC la L cortocircuita al C: pide i0)",
     "tfin 1e-3 h 1e-4\nC C1 1 0 1e-6 5\nL L1 1 0 1e-2 -\n", r"indica i0", {}),
    ("Rechazo (--estricto): dos C en paralelo", _PARALELO_C, r"estricto", {'estricto': True}),
    ("Rechazo (--estricto): dos L en serie",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 10\nR R1 1 2 100\nL L1 2 3 0.04 0\nL L2 3 0 0.06 0\n",
     r"estricto", {'estricto': True}),
    ("Rechazo: elemento con los dos terminales en el mismo nodo",
     "tfin 1e-3 h 1e-4\nR R1 1 1 10\n", r"mismo nodo", {}),
    ("Rechazo: resistencia nula", "tfin 1e-3 h 1e-4\nR R1 1 0 0\n", r"R debe ser > 0", {}),
    ("Rechazo: falta la línea tfin", "R R1 1 0 1\n", r"tfin", {}),
    ("Rechazo: forma SIN con parámetros de menos",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 SIN 1 2 3\nR R1 1 0 1\n", r"SIN necesita 4", {}),
    ("Rechazo: interruptor que se cierra directamente sobre una fuente ideal",
     "tfin 2e-3 h 1e-4\nV E1 0 1 0 DC 5\nR R1 1 0 10\nS S1 1 0 0 1e-3 0\n", r"lazo", {}),
    ("Rechazo: lazo E1-E2-amperímetro ideal",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 5\nV E2 1 2 0 DC 3\nAM A1 2 0 0\nR R1 2 0 10\n", r"lazo", {}),
    ("Rechazo: interruptor que pone en paralelo dos fuentes ideales",
     "tfin 2e-3 h 1e-4\nV E1 0 1 0 DC 5\nV E2 0 2 0 DC 3\nR R1 1 0 10\nR R2 2 0 10\nS S1 1 2 0 1e-3 0\n",
     r"lazo", {}),
    ("Rechazo: escalón PUL ideal directamente sobre un C (impulso en td)",
     "tfin 2e-3 h 1e-4\nV E1 0 1 0 PUL 0 5 1e-3 0 0\nC C1 1 0 1e-6 -\nR R1 1 0 100\n", r"impulso", {}),
    ("Rechazo: C colgante sin v0 (v_C indeterminada)",
     "tfin 1e-3 h 1e-4\nV E1 0 1 0 DC 4\nR R1 1 2 100\nR R2 2 0 100\nC C1 2 3 1e-6 -\n", r"v0", {}),
    ("Rechazo: C con v0 = 1 uV sobre SIN de fase 180 (e(0) = 0): sigue siendo incompatible",
     "tfin 2e-3 h 1e-5\nV E1 0 1 0 SIN 5 100 180 0\nC C1 1 0 1e-6 1e-6\nR R1 1 0 100\n", r"incompatib", {}),
    ("Rechazo: puente desequilibrado (1 %): S que cortocircuita la C de la diagonal (impulso)",
     "tfin 5e-3 h 1e-5\nV E1 0 1 0 SIN 10 300 0 0\nR R1 1 2 100\nR R2 1 3 101\nL L3 2 0 0.05 -\n"
     "L L4 3 0 0.05 -\nC C1 2 3 1e-6 -\nS S1 2 3 0 2.5e-3 0\n", r"impulso", {}),
    ("Rechazo: S en paralelo con L se abre con i_L distinta de J (impulso)",
     "tfin 2e-3 h 1e-4\nI J1 0 1 DC 2e-3\nL L1 1 2 0.01 1e-3\nS S1 1 2 1 1e-3 0\nR R1 2 0 1000\n", r"impulso", {}),
]


def ejecutar_pruebas():
    print(f"Pruebas del oráculo (tolerancia relativa {TOL_PRUEBAS:g}; error = max|sim - exacta| / "
          f"max|exacta| sobre todas las señales comparadas)\n")
    n_ok = n_tot = 0
    for nombre, f in _PRUEBAS:
        t0 = time.perf_counter()
        try:
            err, nota = f()
            ok = err <= TOL_PRUEBAS
            txt = f"{err:.2e}"
        except Exception as ex:  # noqa: BLE001
            ok, txt, nota = False, "   ---  ", f"EXCEPCIÓN {type(ex).__name__}: {ex}"
        dt = time.perf_counter() - t0
        n_tot += 1
        n_ok += ok
        print(f"[{'OK   ' if ok else 'FALLO'}] {nombre:<78s} err_max = {txt}  ({dt:5.2f} s)"
              + (f"  {nota}" if nota else ""))
    print()
    for nombre, texto, patron, kw in _RECHAZOS:
        try:
            simular(texto, **kw)
            ok, nota = False, "NO se rechazó"
        except ErrorCircuito as ex:
            ok = re.search(patron, str(ex), re.I | re.S) is not None
            nota = str(ex)
        except Exception as ex:  # noqa: BLE001
            ok, nota = False, f"EXCEPCIÓN {type(ex).__name__}: {ex}"
        n_tot += 1
        n_ok += ok
        print(f"[{'OK   ' if ok else 'FALLO'}] {nombre}\n         -> {nota}")
    print(f"\n{n_ok}/{n_tot} pruebas superadas")
    return 0 if n_ok == n_tot else 1


# =============================================================================
# Línea de órdenes
# =============================================================================

def main(argv=None):
    for s in (sys.stdout, sys.stderr):
        # Consola: Python ya escribe Unicode. Tubería con una codificación que no tiene las
        # tildes (p. ej. cp932): se pasa a UTF-8. En cualquier caso, nunca revienta al escribir.
        try:
            enc = (s.encoding or 'ascii')
            'áéíóúñ¿µ'.encode(enc)
            s.reconfigure(errors='replace')
        except (LookupError, UnicodeEncodeError):
            try:
                s.reconfigure(encoding='utf-8', errors='replace')
            except (AttributeError, ValueError):
                pass
        except (AttributeError, ValueError):
            pass
    ap = argparse.ArgumentParser(
        prog='oraculo_sim.py',
        description="Simulador de referencia (oráculo) de circuitos lineales con conmutación.")
    ap.add_argument('netlist', nargs='?', help="fichero de netlist")
    ap.add_argument('salida', nargs='?', help="fichero CSV de salida")
    ap.add_argument('--pruebas', action='store_true', help="ejecuta las pruebas contra soluciones exactas")
    ap.add_argument('--estricto', action='store_true',
                    help="rechaza también lazos C/fuente y cortes L/fuente compatibles")
    ap.add_argument('--metodo', choices=['auto', 'Radau', 'DOP853'], default='auto')
    ap.add_argument('--rtol', type=float, default=1e-10)
    ap.add_argument('--detalle', action='store_true', help="imprime el análisis de las topologías")
    a = ap.parse_args(argv)
    if a.pruebas:
        return ejecutar_pruebas()
    if not a.netlist or not a.salida:
        ap.print_usage(sys.stderr)
        print("ERROR: hacen falta la netlist y el CSV de salida (o --pruebas)", file=sys.stderr)
        return 2
    try:
        with open(a.netlist, encoding='utf-8-sig', errors='replace') as fh:
            texto = fh.read()
        res = simular(texto, estricto=a.estricto, metodo=a.metodo, rtol=a.rtol)
    except ErrorCircuito as ex:
        print(f"ERROR: {ex}", file=sys.stderr)
        return 1
    except OSError as ex:
        print(f"ERROR: no se puede leer la netlist: {ex}", file=sys.stderr)
        return 1
    except ErrorInterno as ex:
        print(f"ERROR INTERNO: {ex}", file=sys.stderr)
        return 3
    for av in res.avisos:
        print(f"AVISO: {av}", file=sys.stderr)
    try:
        escribir_csv(res, a.salida)
    except OSError as ex:
        print(f"ERROR: no se puede escribir el CSV: {ex}", file=sys.stderr)
        return 1
    print(f"OK: {len(res.t)} instantes x {len(res.columnas)} columnas -> {a.salida}")
    if a.detalle:
        print(res.informe)
    return 0


if __name__ == '__main__':
    sys.exit(main())
