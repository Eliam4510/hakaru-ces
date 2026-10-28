#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
ORÁCULO NO LINEAL  --  Hakaru CES / herramientas / oraculo_nl.py
================================================================

Referencia independiente para el motor no lineal de Hakaru CES (sim.c y
nolin.c, Etapa 4): un MNA no lineal propio, escrito aparte a partir de las
ecuaciones de la especificación (docs/historia/espec_e4.md, §1 D3-D10 y §3.C), sin mirar el
código del motor. El arnés (sección [67], test_e4_ora.inc) exporta redes al
azar a out/oraculo_nl:

    caso_NNN.net   la red en CESNET 3 (abajo)
    caso_NNN.ces   lo que da el programa: en CC, el potencial de cada nodo y la
                   corriente de cada elemento (y la de base de cada BJT); en el
                   tiempo, t, canal 1 y canal 2 en el punto que calculó más
                   cercano a cada k·tfin/400 (en un evento, el de después)
    caso_NNN_h4.ces  lo mismo con un cuarto de su paso (dato)
    rechazada_*.net  las que el motor no resuelve (NO_CONVERGE; no cuentan)

y este programa resuelve cada red por su cuenta y compara (§6.13, [67]).

USO
---
    python oraculo_nl.py [carpeta]         (por omisión, ../out/oraculo_nl)
    python oraculo_nl.py carpeta --solo 17 --detalle
    python oraculo_nl.py --pruebas         casos cerrados del propio oráculo
                                           (los números de la especificación)
Con PYTHONIOENCODING=utf-8 en Windows. Código de salida: 0 si todas iguales.

FORMATO DE LA RED (CESNET 3)
----------------------------
CESNET 2 (oraculo_ca.py) con el tiempo o la CC en vez de la CA y los no
lineales. Nodos: enteros >= 0; el 0 es la referencia. Todas las corrientes
van de a hacia b POR DENTRO del elemento; en las fuentes, b es el + (la
corriente sale por b). '#' empieza un comentario, salvo «# canal k V n» y
«# canal k I nom», que dicen qué comparar en el tiempo.

    cesnet 3
    dc                                     punto de operación de CC (la vista de
                                           [P]: C abiertos, L cortos, S al
                                           empezar), o bien
    tran <tfin> <h>                        el tiempo (h: el paso de las muestras;
                                           los instantes salen del .ces)
    R  <nom> <a> <b> <ohm>
    V  <nom> <neg> <pos> <r> <forma...>    V(pos) - V(neg) = e(t) - r·I
    I  <nom> <a> <b> <forma...>            j(t) de a hacia b por dentro
    C  <nom> <a> <b> <F> <v0>              (tran) v0 = V(a) - V(b) en t = 0
    L  <nom> <a> <b> <H> <i0>              (tran) i0 de a hacia b en t = 0
    S  <nom> <a> <b> <cerrado0> <t1> <t2>  conmuta en t1 > 0 y en t2 > 0
    AM <nom> <a> <b> <r>                   r = 0: ideal (un corto)
    VM <nom> <a> <b> <r>                   r = 0: ideal (abierto)
    D  <nom> <a> <c> <Is> <n> <Rs>         unión (D4), ánodo a, cátodo c
    DZ <nom> <a> <c> <Is> <n> <Rs> <BV> <Ibv>   unión con ruptura (D5)
    DT <nom> <a> <c> <Vg> <Ron> <Vz0> <Rz> por tramos (D3; Vz0 = 0: sin ruptura)
    AO <nom> <p> <m> <o> <ref> <A> <V+> <V-> <Ri> <Ro>
                                           A = 0: ideal (nulor); Ri = 0: infinita
    EV <nom> <a> <b> <ctrl> <mu>           V(b) - V(a) = mu·V(ctrl)
    EI <nom> <a> <b> <ctrl> <r>            V(b) - V(a) = r·I(ctrl)
    JV <nom> <a> <b> <ctrl> <g>            J = g·V(ctrl), de a hacia b
    JI <nom> <a> <b> <ctrl> <beta>         J = beta·I(ctrl)
    Q  <nom> <c> <b> <e> NPN|PNP <Is> <bf> <br> <VA>   VA = 0: sin Early
    M  <nom> <d> <g> <s> N|P <Vt> <K> <VA>              VA = 0: sin modulación
  Formas (las de oraculo_sim.py, que ya se compararon en [29]): DC v |
  SIN A f fase nivel | SQR A f ciclo nivel | TRI A f ciclo nivel |
  PUL V1 V2 td ancho periodo.

  La V y la I del que manda a una controlada son las de su tabla (D8): V =
  V(a) - V(b) en los pasivos (R, C, L, S, AM, VM, diodos) y V(b) - V(a) en las
  fuentes (V, I, EV, EI, JV, JI); I, la que lo recorre de a hacia b.

SALIDA DEL PROGRAMA (.ces)
----------------------------
  CC:     V <nodo> <phi> | I <nom> <I> | IB <nom> <IB>   (IB: base de un BJT;
          I de un AO: la que entrega su salida; de un BJT, IC; de un MOS, ID)
  tiempo: t,y1,y2 (una línea de cabecera y luego las muestras; al final,
          «# h ...» con el paso, los pasos y los avisos)

MODELOS (los de la especificación; VT = 0,025864925786328750067 V, GMIN = 1e-12 S)
------------------------------------------------------------------------------
  e^u sigue por su tangente por encima de u = 80 (D4): e^80·(1 + u - 80).
  Unión (D, DZ): ij(vd) = Is·(e^(vd/(n·VT)) - 1) - Ibv·e^(-(vd + BV)/VT) (el
      segundo término con BV > 0); en bornes v = vd + Rs·ij e i = ij + GMIN·v.
      Aquí Rs NO se condensa: con Rs > 0 la unión tiene su nodo interno x
      (Rs de a a x, la unión de x a c) y GMIN va de a a c. Es la formulación
      de SPICE que el motor elimina en cada iteración (D4); si la eliminación
      estuviera mal, las soluciones no coincidirían.
  Por tramos (DT): corte GMIN·v; conducción (v - Vg)/Ron + GMIN·Vg; ruptura
      (v + Vz0)/Rz - GMIN·Vz0 (D3).
  AO (D7): u = A·(V+ - V-) recortada a [V-, V+], detrás de Ro, con Ri entre +
      y -; la salida se mide desde ref. Ideal: V+ = V- con la salida entre los
      raíles, o la salida en un raíl con V+ - V- del lado de ese raíl.
  BJT (D9, §3.C): x = pol·vBE, y = pol·vBC; iF = Is·(e^(x/VT) - 1), iR =
      Is·(e^(y/VT) - 1); q = 1 - y/VA (>= 0,05; 1 sin VA); ic = (iF - iR)·q -
      iR/br - GMIN·y; ib = iF/bf + iR/br + GMIN·(x + y); IC = pol·ic, IB = pol·ib.
  MOS (D10, §3.C): x = pol·vGS, y = pol·vDS; con y < 0 D y S cambian; vov = x -
      Vt; óhmica K·(2·vov·y - y²)·(1 + y/VA), saturación K·vov²·(1 + y/VA), más
      GMIN·y; ID = pol·id.

MÉTODO
------
  Incógnitas: el potencial de cada nodo (menos el 0), el del nodo interno de
  cada unión con Rs y la corriente de rama de V ideal, AM ideal, S, EV, EI, la
  salida del AO ideal o con Ro = 0, C (tiempo) y L (CC). Ecuaciones: la ley de
  nodos de cada nodo y la de cada rama. Jacobiano analítico.
  El AO (ideal o real) es lineal a trozos: el oráculo prueba cada combinación
  de tramos (lineal, V+, V-) de los AO y se queda con las soluciones que
  cumplen su tramo (en el ideal: la salida entre los raíles en el lineal; V+ -
  V- >= 0 en V+...). Así encuentra también las varias soluciones de un Schmitt.

  CC: en cada combinación, scipy.optimize.root (hybr, xtol 1e-14) desde x = 0,
  desde la solución del programa perturbada 1e-3 y con su propia rampa de
  fuentes (E y J por lam de 0 a 1, paso adaptativo), pulido con Newton (que
  tiene que converger por su paso); si hybr se queda a medias («no hace
  progreso» lejos de la raíz, lo normal con exponenciales desde 0), Newton
  amortiguado desde el mismo arranque. Pasa si la del programa coincide con
  una de las suyas a 1e-9 de la escala y su residuo en las ecuaciones del
  oráculo es <= 1e-10 (así valen también los circuitos con varias soluciones).
    La escala de tensiones: el mayor de los potenciales y de las E; la de
  corrientes: la mayor de las corrientes y de las J, y como poco la de
  tensiones sobre la R mediana (la resolución de una corriente que sale de una
  diferencia de potenciales); la de la corriente de salida de un AO real con
  Ro > 0, como poco A·escV/Ro por 1e-5 (escalas_ao: un ulp de V+ - V- la
  mueve A/Ro veces). El residuo: en cada fila, |F| frente a la suma de
  los módulos de sus términos más la escala (escI en las leyes de nodos, escV
  en las de tensión), el error regresivo por filas.
  Tiempo: estado = las v de los C y las i de las L; en cada llamada el
  algebraico (C como fuentes de tensión, L como fuentes de corriente) se
  resuelve con Newton desde la solución anterior (y, si no llega, por
  continuación desde el último resuelto), y la integración es Radau (rtol
  1e-10, jacobiano exacto por la función implícita) partida en los flancos de
  las fuentes y en las conmutaciones de los S; el algebraico de t = 0 sale con
  una rampa sobre las fuentes y el estado. Los instantes son los del .ces: los
  puntos que calculó el motor (no la recta entre dos de ellos, que en el codo
  de un transistor que se enciende no es la curva). Pasa con el peor error
  <= 5e-3 de la escala (el máximo de |y| del oráculo, con un suelo de 1e-6 de
  la escala del circuito) y el 90 % de los instantes <= 1e-3. Con
  caso_NNN_h4.ces (el programa con un cuarto de su paso) dice también cuánto
  baja el error. Las redes de HALLAZGOS (no cuadran por el motor, con su
  reproducción mínima) no cuentan si siguen distintas y con h/4 son iguales;
  el resumen lo dice.
  Las rechazada_NNN.net son redes que el motor no emuló (NO_CONVERGE); no
  cuentan: el oráculo dice si él las integra y, si el motor las emuló con más
  iteraciones por paso (rechazada_NNN.ces), si da lo mismo. Las
  rechazada_cc_NNN.net, redes de CC cuyo punto de operación no convergió: el
  oráculo dice cuántas soluciones halla.
"""
import itertools
import math
import os
import sys

import numpy as np
from scipy.integrate import solve_ivp
from scipy.optimize import root

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oraculo_sim as orc  # noqa: E402  (solo las formas de onda)

VT = 0.025864925786328750067         # kT/q a 300,15 K (CODATA 2018)
GMIN = 1e-12                          # S: en paralelo con cada unión
XLIM = 80.0                           # e^u sigue en recta por encima

TOL_CC = 1e-9                         # la del programa frente a una del oráculo
TOL_RES = 1e-10                       # su residuo en las ecuaciones del oráculo
TOL_TR_MAX = 5e-3                     # el tiempo: el peor instante
TOL_TR_90 = 1e-3                      # el tiempo: el 90 % de los instantes

NOTA_MOTOR = '# motor: '              # la línea del .net con lo que hizo el motor

FUENTES = ('V', 'I', 'EV', 'EI', 'JV', 'JI')   # su V es V(b) - V(a)
LIN, SATP, SATN = 0, 1, 2


class ErrorRed(Exception):
    pass


# =============================================================================
# Modelos
# =============================================================================

def ex(u):
    """e^u y su derivada; por encima de 80, la tangente (D4)"""
    if u > XLIM:
        e = math.exp(XLIM)
        return e * (1.0 + (u - XLIM)), e
    e = math.exp(u)
    return e, e


def union(vd, Is, n, BV, Ibv):
    """La corriente de la unión y su derivada en vd (D4, D5)"""
    nvt = n * VT
    e, d = ex(vd / nvt)
    ij = Is * (e - 1.0)
    gj = Is * d / nvt
    if BV > 0.0:
        er, dr = ex(-(vd + BV) / VT)
        ij -= Ibv * er
        gj += Ibv * dr / VT
    return ij, gj


def tramos(v, Vg, Ron, Vz0, Rz):
    """El diodo por tramos (D3): i y di/dv"""
    if v > Vg:
        return (v - Vg) / Ron + GMIN * Vg, 1.0 / Ron
    if Vz0 > 0.0 and v < -Vz0:
        return (v + Vz0) / Rz - GMIN * Vz0, 1.0 / Rz
    return GMIN * v, GMIN


def bjt(pol, Is, bf, br, VA, vbe, vbc):
    """IC, IB (entran por C y por B) y sus derivadas respecto a vBE y vBC"""
    x, y = pol * vbe, pol * vbc
    eF, dF = ex(x / VT)
    eR, dR = ex(y / VT)
    iF, iR = Is * (eF - 1.0), Is * (eR - 1.0)
    gF, gR = Is * dF / VT, Is * dR / VT
    q, dq = 1.0, 0.0
    if VA > 0.0:
        q, dq = 1.0 - y / VA, -1.0 / VA
        if q < 0.05:
            q, dq = 0.05, 0.0
    ic = (iF - iR) * q - iR / br - GMIN * y
    ib = iF / bf + iR / br + GMIN * (x + y)
    # d/dvBE = d/dx·pol y el resultado va por pol: pol² = 1
    dic_be = gF * q
    dic_bc = -gR * q + (iF - iR) * dq - gR / br - GMIN
    dib_be = gF / bf + GMIN
    dib_bc = gR / br + GMIN
    return pol * ic, pol * ib, dic_be, dic_bc, dib_be, dib_bc


def mos(pol, Vt, K, VA, vgs, vds):
    """ID (entra por D), dID/dvGS y dID/dvDS"""
    lam = 1.0 / VA if VA > 0.0 else 0.0
    x, y = pol * vgs, pol * vds
    cambia = y < 0.0
    if cambia:
        x, y = x - y, -y
    vov = x - Vt
    f = 1.0 + lam * y
    if vov <= 0.0:
        i = gm = gds = 0.0
    elif y < vov:
        p = 2.0 * vov * y - y * y
        i, gm, gds = K * p * f, 2.0 * K * y * f, 2.0 * K * (vov - y) * f + lam * K * p
    else:
        i, gm, gds = K * vov * vov * f, 2.0 * K * vov * f, lam * K * vov * vov
    i += GMIN * y
    gds += GMIN
    if cambia:
        i, gds, gm = -i, gm + gds, -gm
    return pol * i, gm, gds


# =============================================================================
# La red
# =============================================================================

class Elem:
    def __init__(self, tipo, nom, nodos, v):
        self.tipo, self.nom, self.nodos, self.v = tipo, nom, nodos, v
        self.forma = None
        self.ctrl = None
        self.pol = 1.0
        self.jb = -1          # su corriente de rama, si la tiene
        self.jx = -1          # el nodo interno (unión con Rs)


class Red:
    def __init__(self, texto):
        self.modo, self.tfin, self.h = None, None, None
        self.el, self.canales = [], []
        for nlin, linea in enumerate(texto.splitlines(), 1):
            s = linea.strip()
            if s.startswith('#'):
                p = s.split()
                if len(p) == 5 and p[1] == 'canal':
                    self.canales.append((int(p[2]) - 1, p[3], p[4]))
                continue
            s = s.split('#', 1)[0].strip()
            if not s:
                continue
            p = s.split()
            k = p[0].upper()
            if k == 'CESNET':
                continue
            if k == 'DC':
                self.modo = 'dc'
                continue
            if k == 'TRAN':
                self.modo, self.tfin, self.h = 'tran', float(p[1]), float(p[2])
                continue
            nom = p[1]
            if k in ('R', 'C', 'L', 'AM', 'VM', 'D', 'DZ', 'DT'):
                e = Elem(k, nom, (int(p[2]), int(p[3])), [float(x) for x in p[4:]])
                if k == 'D':
                    e.v = e.v + [0.0, 0.0]          # sin ruptura
            elif k == 'V':
                e = Elem(k, nom, (int(p[2]), int(p[3])), [float(p[4])])
                e.forma = orc._forma(p[5:], nlin, nom)
            elif k == 'I':
                e = Elem(k, nom, (int(p[2]), int(p[3])), [])
                e.forma = orc._forma(p[4:], nlin, nom)
            elif k == 'S':
                e = Elem(k, nom, (int(p[2]), int(p[3])), [int(p[4]), float(p[5]), float(p[6])])
            elif k in ('EV', 'EI', 'JV', 'JI'):
                e = Elem(k, nom, (int(p[2]), int(p[3])), [float(p[5])])
                e.ctrl = p[4]
            elif k == 'AO':
                e = Elem(k, nom, (int(p[2]), int(p[3]), int(p[4]), int(p[5])), [float(x) for x in p[6:11]])
            elif k == 'Q':
                e = Elem(k, nom, (int(p[2]), int(p[3]), int(p[4])), [float(x) for x in p[6:10]])
                e.pol = 1.0 if p[5].upper() == 'NPN' else -1.0
            elif k == 'M':
                e = Elem(k, nom, (int(p[2]), int(p[3]), int(p[4])), [float(x) for x in p[6:9]])
                e.pol = 1.0 if p[5].upper() == 'N' else -1.0
            else:
                raise ErrorRed(f'línea {nlin}: «{k}» no es de CESNET 3')
            self.el.append(e)
        if self.modo is None:
            raise ErrorRed('falta «dc» o «tran»')


def _valor_dc(forma):
    """El valor de una fuente en CC (la red de CC solo tiene formas DC)"""
    f, _ = forma.pieza(0.0)
    return float(f(0.0))


class Ctx:
    """Lo que no es incógnita: fuentes (nom -> valor, ya por lam), estado de
    C y L (tiempo), S cerrados y tramo de cada AO (None: la forma no suave)"""
    def __init__(self, fuentes, xC=None, xL=None, cerrado=None, ao=None):
        self.fuentes, self.xC, self.xL = fuentes, xC or {}, xL or {}
        self.cerrado, self.ao = cerrado or {}, ao


class Sistema:
    def __init__(self, red):
        self.red = red
        self.dc = red.modo == 'dc'
        nodos = set()
        for e in red.el:
            nodos.update(e.nodos)
        nodos.discard(0)
        self.nodos = sorted(nodos)
        self.iv = {nd: i for i, nd in enumerate(self.nodos)}
        self.iv[0] = -1
        n = len(self.nodos)
        self.tipo_fila = ['I'] * n            # I: una ley de nodos o una corriente; V: una tensión
        for e in red.el:
            e.i = tuple(self.iv[nd] for nd in e.nodos)
            if e.tipo in ('D', 'DZ') and e.v[2] > 0.0:
                e.jx = n
                n += 1
                self.tipo_fila.append('I')
        for e in red.el:
            t = e.tipo
            rama = ((t == 'V' and e.v[0] == 0.0) or (t == 'AM' and e.v[0] == 0.0) or t in ('S', 'EV', 'EI') or
                    (t == 'C' and not self.dc) or (t == 'L' and self.dc) or
                    (t == 'AO' and (e.v[0] == 0.0 or e.v[4] == 0.0)))
            if rama:
                e.jb = n
                n += 1
                self.tipo_fila.append('V')    # S abierto: la fila pasa a ser «rtip·i = 0», también en V
        self.n = n
        # qué incógnitas son corrientes (las de rama) y cuáles potenciales
        self.es_corriente = np.array([k >= len(self.nodos) and t == 'V' for k, t in enumerate(self.tipo_fila)])
        self.por_nombre = {e.nom: e for e in red.el}
        for e in red.el:
            if e.ctrl is not None:
                if e.ctrl not in self.por_nombre:
                    raise ErrorRed(f'{e.nom}: el control {e.ctrl} no existe')
                e.x = self.por_nombre[e.ctrl]
                if e.tipo in ('EI', 'JI') and e.x.tipo in ('D', 'DZ', 'DT', 'JV', 'JI', 'AO', 'Q', 'M'):
                    raise ErrorRed(f'{e.nom}: no se controla por la I de {e.ctrl}')
        self.aos = [e for e in red.el if e.tipo == 'AO']
        # las uniones, para amortiguar Newton (_newton): w = sg·(V(i) - V(k)) + off
        # es su tensión en directa; (i, k, sg, off, n·VT, la w a partir de la que
        # conduce de verdad)
        self.uniones = []
        for e in red.el:
            if e.tipo in ('D', 'DZ'):
                Is, nn, BV, Ibv = e.v[0], e.v[1], e.v[3], e.v[4]
                i = e.jx if e.jx >= 0 else e.i[0]
                self.uniones.append((i, e.i[1], 1.0, 0.0, nn * VT, nn * VT * math.log(nn * VT / Is)))
                if BV > 0.0:                          # la ruptura: w = -(vd + BV)
                    self.uniones.append((i, e.i[1], -1.0, -BV, VT, VT * math.log(VT / Ibv)))
            elif e.tipo == 'Q':
                c, b, em = e.i
                w0 = VT * math.log(VT / e.v[0])
                self.uniones.append((b, em, e.pol, 0.0, VT, w0))
                self.uniones.append((b, c, e.pol, 0.0, VT, w0))
        self.caps = [e for e in red.el if e.tipo == 'C'] if not self.dc else []
        self.inds = [e for e in red.el if e.tipo == 'L'] if not self.dc else []
        self.fuentes = [e for e in red.el if e.tipo in ('V', 'I')]
        # peso de cada fila para que todas vayan en voltios (hybr y Newton)
        rs = [e.v[0] for e in red.el if e.tipo == 'R']
        self.rtip = float(np.median(rs)) if rs else 1e3
        self.peso = np.array([self.rtip if t == 'I' else 1.0 for t in self.tipo_fila])
        # de dónde sale el estado (tiempo): dF/dx es constante (F es afín en x)
        self.nx = len(self.caps) + len(self.inds)
        self.Fx = np.zeros((n, self.nx))
        for k, c in enumerate(self.caps):
            self.Fx[c.jb, k] = -1.0
        for k, l in enumerate(self.inds):
            kx = len(self.caps) + k
            if l.i[0] >= 0:
                self.Fx[l.i[0], kx] += self.peso[l.i[0]]
            if l.i[1] >= 0:
                self.Fx[l.i[1], kx] -= self.peso[l.i[1]]
            for e in red.el:
                if e.ctrl == l.nom and e.tipo == 'EI':
                    self.Fx[e.jb, kx] -= e.v[0]
                elif e.ctrl == l.nom and e.tipo == 'JI':
                    if e.i[0] >= 0:
                        self.Fx[e.i[0], kx] += e.v[0] * self.peso[e.i[0]]
                    if e.i[1] >= 0:
                        self.Fx[e.i[1], kx] -= e.v[0] * self.peso[e.i[1]]

    # ---- el control de una controlada: sum(c·z[k]) + c0 ----
    def control(self, e, ctx):
        x = e.x
        a, b = x.i[0], x.i[1]
        if e.tipo in ('EV', 'JV'):
            if x.tipo in FUENTES:
                a, b = b, a
            return [(a, 1.0), (b, -1.0)], 0.0
        t = x.tipo
        if t == 'R' or (t in ('AM', 'VM') and x.v[0] > 0.0):
            g = 1.0 / x.v[0]
            return [(a, g), (b, -g)], 0.0
        if t == 'V':
            if x.v[0] == 0.0:
                return [(x.jb, 1.0)], 0.0
            g = 1.0 / x.v[0]
            return [(a, g), (b, -g)], ctx.fuentes[x.nom] * g          # (e - (vb - va))/r
        if t == 'I':
            return [], ctx.fuentes[x.nom]
        if t == 'VM':
            return [], 0.0
        if t == 'L' and not self.dc:
            return [], ctx.xL[x.nom]
        if t == 'C' and self.dc:
            return [], 0.0
        return [(x.jb, 1.0)], 0.0                                       # AM, S, C, L, EV, EI: su rama

    @staticmethod
    def _lineal(z, lista):
        return sum(c * z[k] for k, c in lista if k >= 0)

    # ---- F(z) y su jacobiano ----
    def ecuaciones(self, z, ctx, con_j=True, mag=None):
        """F(z) y dF/dz, con las filas en voltios (las de ley de nodos, por rtip).
        mag (si no es None, un vector de ceros): la suma de los módulos de los
        términos de cada fila, con cada término lineal c·z[k] por separado: el
        tamaño contra el que se mide el redondeo de esa fila (residuo)"""
        n = self.n
        F = np.zeros(n)
        J = np.zeros((n, n)) if con_j else None

        def V(k):
            return z[k] if k >= 0 else 0.0

        def pon(i, val):
            if i >= 0:
                F[i] += val
                if mag is not None:
                    mag[i] += abs(val)

        def pj(i, k, val):
            if i >= 0 and k >= 0:
                if con_j:
                    J[i, k] += val
                if mag is not None:
                    mag[i] += abs(val * z[k])

        def cte(i, val):
            """un término conocido de una fila de rama (su tamaño)"""
            if mag is not None and i >= 0:
                mag[i] += abs(val)

        def dos(i, k, cur, g):
            """una corriente cur(v) de i a k por dentro, con v = V(i) - V(k) y g = dcur/dv"""
            pon(i, cur)
            pon(k, -cur)
            pj(i, i, g)
            pj(i, k, -g)
            pj(k, i, -g)
            pj(k, k, g)

        def rama(e, i, k):
            """la corriente de rama de e sale del nodo i y entra en el k"""
            pon(i, z[e.jb])
            pon(k, -z[e.jb])
            pj(i, e.jb, 1.0)
            pj(k, e.jb, -1.0)

        for e in self.red.el:
            t = e.tipo
            if t == 'R':
                a, b = e.i
                dos(a, b, (V(a) - V(b)) / e.v[0], 1.0 / e.v[0])
            elif t == 'V':
                a, b = e.i
                ev = ctx.fuentes[e.nom]
                if e.v[0] == 0.0:
                    rama(e, a, b)
                    F[e.jb] = V(b) - V(a) - ev
                    pj(e.jb, b, 1.0)
                    pj(e.jb, a, -1.0)
                    cte(e.jb, ev)
                else:
                    r = e.v[0]
                    cur = (ev - (V(b) - V(a))) / r
                    pon(a, cur)
                    pon(b, -cur)
                    pj(a, a, 1.0 / r)
                    pj(a, b, -1.0 / r)
                    pj(b, a, -1.0 / r)
                    pj(b, b, 1.0 / r)
            elif t == 'I':
                a, b = e.i
                pon(a, ctx.fuentes[e.nom])
                pon(b, -ctx.fuentes[e.nom])
            elif t in ('AM', 'VM'):
                a, b = e.i
                if e.v[0] > 0.0:
                    dos(a, b, (V(a) - V(b)) / e.v[0], 1.0 / e.v[0])
                elif t == 'AM':
                    rama(e, a, b)
                    F[e.jb] = V(a) - V(b)
                    pj(e.jb, a, 1.0)
                    pj(e.jb, b, -1.0)
            elif t == 'S':
                a, b = e.i
                rama(e, a, b)
                if ctx.cerrado.get(e.nom, bool(e.v[0])):
                    F[e.jb] = V(a) - V(b)
                    pj(e.jb, a, 1.0)
                    pj(e.jb, b, -1.0)
                else:
                    F[e.jb] = self.rtip * z[e.jb]
                    pj(e.jb, e.jb, self.rtip)
            elif t == 'C':
                if not self.dc:
                    a, b = e.i
                    rama(e, a, b)
                    F[e.jb] = V(a) - V(b) - ctx.xC[e.nom]
                    pj(e.jb, a, 1.0)
                    pj(e.jb, b, -1.0)
                    cte(e.jb, ctx.xC[e.nom])
            elif t == 'L':
                a, b = e.i
                if self.dc:
                    rama(e, a, b)
                    F[e.jb] = V(a) - V(b)
                    pj(e.jb, a, 1.0)
                    pj(e.jb, b, -1.0)
                else:
                    pon(a, ctx.xL[e.nom])
                    pon(b, -ctx.xL[e.nom])
            elif t in ('D', 'DZ'):
                a, b = e.i
                Is, nn, Rs, BV, Ibv = e.v[:5]
                dos(a, b, GMIN * (V(a) - V(b)), GMIN)
                if Rs > 0.0:
                    x = e.jx
                    dos(a, x, (V(a) - z[x]) / Rs, 1.0 / Rs)
                    ij, gj = union(z[x] - V(b), Is, nn, BV, Ibv)
                    dos(x, b, ij, gj)
                else:
                    ij, gj = union(V(a) - V(b), Is, nn, BV, Ibv)
                    dos(a, b, ij, gj)
            elif t == 'DT':
                a, b = e.i
                cur, g = tramos(V(a) - V(b), *e.v[:4])
                dos(a, b, cur, g)
            elif t in ('EV', 'EI'):
                a, b = e.i
                rama(e, a, b)
                lista, c0 = self.control(e, ctx)
                k = e.v[0]
                F[e.jb] = V(b) - V(a) - k * (self._lineal(z, lista) + c0)
                pj(e.jb, b, 1.0)
                pj(e.jb, a, -1.0)
                cte(e.jb, k * c0)
                for col, c in lista:
                    pj(e.jb, col, -k * c)
            elif t in ('JV', 'JI'):
                a, b = e.i
                lista, c0 = self.control(e, ctx)
                k = e.v[0]
                cur = k * (self._lineal(z, lista) + c0)
                pon(a, cur)
                pon(b, -cur)
                cte(a, k * c0)
                cte(b, k * c0)
                for col, c in lista:
                    pj(a, col, k * c)
                    pj(b, col, -k * c)
            elif t == 'AO':
                self._ao(e, z, ctx, F, V, pon, pj, cte, dos, rama)
            elif t == 'Q':
                c, b, em = e.i
                Is, bf, br, VA = e.v
                ic, ib, dcbe, dcbc, dbbe, dbbc = bjt(e.pol, Is, bf, br, VA, V(b) - V(em), V(b) - V(c))
                pon(c, ic)
                pon(b, ib)
                pon(em, -(ic + ib))
                # vBE = V(b) - V(em), vBC = V(b) - V(c)
                for fila, d_be, d_bc, sg in ((c, dcbe, dcbc, 1.0), (b, dbbe, dbbc, 1.0), (em, dcbe + dbbe, dcbc + dbbc, -1.0)):
                    pj(fila, b, sg * (d_be + d_bc))
                    pj(fila, em, -sg * d_be)
                    pj(fila, c, -sg * d_bc)
            elif t == 'M':
                d, g, s = e.i
                Vt, K, VA = e.v
                idd, gm, gds = mos(e.pol, Vt, K, VA, V(g) - V(s), V(d) - V(s))
                pon(d, idd)
                pon(s, -idd)
                for fila, sg in ((d, 1.0), (s, -1.0)):
                    pj(fila, g, sg * gm)
                    pj(fila, d, sg * gds)
                    pj(fila, s, -sg * (gm + gds))
        F *= self.peso
        if con_j:
            J *= self.peso[:, None]
        if mag is not None:
            mag *= self.peso
        return F, J

    def _u(self, e, estado, vd):
        """La tensión interna u del AO real y du/dvd, en su tramo (None: recortada)"""
        A, Vp, Vn = e.v[0], e.v[1], e.v[2]
        if estado is None:
            u = A * vd
            if u > Vp:
                return Vp, 0.0
            if u < Vn:
                return Vn, 0.0
            return u, A
        if estado == SATP:
            return Vp, 0.0
        if estado == SATN:
            return Vn, 0.0
        return A * vd, A

    def _ao(self, e, z, ctx, F, V, pon, pj, cte, dos, rama):
        p, m, o, rf = e.i
        A, Vp, Vn, Ri, Ro = e.v
        estado = None if ctx.ao is None else ctx.ao[e.nom]
        vd, vs = V(p) - V(m), V(o) - V(rf)
        if A > 0.0 and Ri > 0.0:
            dos(p, m, vd / Ri, 1.0 / Ri)
        if e.jb >= 0:
            rama(e, rf, o)                    # entra por ref y sale por la salida
            j = e.jb
            if A == 0.0:                      # el nulor con sus raíles
                if estado is None:
                    w = vs + vd
                    estado = SATP if w > Vp else SATN if w < Vn else -1
                if estado == SATP or estado == SATN:
                    F[j] = vs - (Vp if estado == SATP else Vn)
                    pj(j, o, 1.0)
                    pj(j, rf, -1.0)
                    cte(j, Vp if estado == SATP else Vn)
                else:                         # el lineal: V+ = V- (-1: la forma no suave, -vd)
                    sg = -1.0 if estado == -1 else 1.0
                    F[j] = sg * vd
                    pj(j, p, sg)
                    pj(j, m, -sg)
            else:                             # real con Ro = 0: vs = u
                u, du = self._u(e, estado, vd)
                F[j] = vs - u
                pj(j, o, 1.0)
                pj(j, rf, -1.0)
                pj(j, p, -du)
                pj(j, m, du)
                if du == 0.0:
                    cte(j, u)
        else:                                 # real con Ro > 0: (u - vs)/Ro de ref a la salida
            u, du = self._u(e, estado, vd)
            cur = (u - vs) / Ro
            pon(rf, cur)
            pon(o, -cur)
            if du == 0.0:
                cte(rf, u / Ro)
                cte(o, u / Ro)
            for fila, sg in ((rf, 1.0), (o, -1.0)):
                pj(fila, p, sg * du / Ro)
                pj(fila, m, -sg * du / Ro)
                pj(fila, o, -sg / Ro)
                pj(fila, rf, sg / Ro)

    # ---- lo que se compara ----
    def corrientes(self, z, ctx):
        """La I de cada elemento (convenios de su tabla) y la de base de los BJT"""
        def V(k):
            return z[k] if k >= 0 else 0.0
        I, IB = {}, {}
        for e in self.red.el:
            t = e.tipo
            a, b = e.i[0], e.i[1]
            if t == 'R':
                I[e.nom] = (V(a) - V(b)) / e.v[0]
            elif t == 'V':
                I[e.nom] = z[e.jb] if e.v[0] == 0.0 else (ctx.fuentes[e.nom] - (V(b) - V(a))) / e.v[0]
            elif t == 'I':
                I[e.nom] = ctx.fuentes[e.nom]
            elif t in ('AM', 'VM'):
                I[e.nom] = (V(a) - V(b)) / e.v[0] if e.v[0] > 0.0 else (z[e.jb] if t == 'AM' else 0.0)
            elif t in ('S', 'EV', 'EI'):
                I[e.nom] = z[e.jb]
            elif t == 'C':
                I[e.nom] = 0.0 if self.dc else z[e.jb]
            elif t == 'L':
                I[e.nom] = z[e.jb] if self.dc else ctx.xL[e.nom]
            elif t in ('D', 'DZ'):
                vd = (z[e.jx] if e.jx >= 0 else V(a)) - V(b)
                I[e.nom] = union(vd, *e.v[:2], *e.v[3:5])[0] + GMIN * (V(a) - V(b))
            elif t == 'DT':
                I[e.nom] = tramos(V(a) - V(b), *e.v[:4])[0]
            elif t in ('JV', 'JI'):
                lista, c0 = self.control(e, ctx)
                I[e.nom] = e.v[0] * (self._lineal(z, lista) + c0)
            elif t == 'AO':
                p, m, o, rf = e.i
                if e.jb >= 0:
                    I[e.nom] = z[e.jb]
                else:
                    u = self._u(e, None if ctx.ao is None else ctx.ao[e.nom], V(p) - V(m))[0]
                    I[e.nom] = (u - (V(o) - V(rf))) / e.v[4]
            elif t == 'Q':
                c, bb, em = e.i
                ic, ib = bjt(e.pol, *e.v, V(bb) - V(em), V(bb) - V(c))[:2]
                I[e.nom], IB[e.nom] = ic, ib
            elif t == 'M':
                d, g, s = e.i
                I[e.nom] = mos(e.pol, *e.v, V(g) - V(s), V(d) - V(s))[0]
        return I, IB

    def potenciales(self, z):
        return {nd: z[self.iv[nd]] for nd in self.nodos}

    def cumple(self, z, ctx, tol):
        """¿Cumple cada AO su tramo? (tol en voltios)"""
        def V(k):
            return z[k] if k >= 0 else 0.0
        for e in self.aos:
            p, m, o, rf = e.i
            A, Vp, Vn = e.v[0], e.v[1], e.v[2]
            vd, vs = V(p) - V(m), V(o) - V(rf)
            est = ctx.ao[e.nom]
            if A == 0.0:
                ok = (Vn - tol <= vs <= Vp + tol) if est == LIN else (vd >= -tol if est == SATP else vd <= tol)
            else:
                u = A * vd
                ok = (Vn - tol <= u <= Vp + tol) if est == LIN else (u >= Vp - tol if est == SATP else u <= Vn + tol)
            if not ok:
                return False
        return True

    def residuo(self, z, ctx, escV, escI):
        """El residuo de z en las ecuaciones del oráculo (los AO en su forma no
        suave, que no depende del tramo): el mayor |F| de una fila frente a la
        suma de los módulos de sus términos más la escala de la red (escI en las
        leyes de nodos, escV en las de tensión). Es el error regresivo por filas
        (como el residuo del motor, pero en las ecuaciones de aquí: con los nodos
        internos de Rs, el nulor con sus raíles...). Con la escala sola no basta:
        en un AO real con A = 3e5 y Ro = 24 Ω, un ulp de V+ - V- mueve la
        corriente de su salida 2e-11 A, el 3e-9 de las corrientes de la red; con
        los términos solos tampoco: la fila «V+ - V- = 0» de una masa virtual
        solo tiene términos de 1e-17. (residuo, fila)"""
        if self.n == 0:
            return 0.0, 0
        c = Ctx(ctx.fuentes, ctx.xC, ctx.xL, ctx.cerrado, None)
        mag = np.zeros(self.n)
        F, _ = self.ecuaciones(z, c, con_j=False, mag=mag)
        esc = np.array([escI if t == 'I' else escV for t in self.tipo_fila]) * self.peso
        r = np.abs(F) / (mag + esc)
        k = int(np.argmax(r))
        return float(r[k]), k


# =============================================================================
# Resolver
# =============================================================================

def _newton(sis, ctx, z0, esc, itmax=60):
    """Newton desde z0; esc: la escala de cada incógnita. Converge cuando el paso
    baja de 1e-10 de la escala y da uno más (la convergencia es cuadrática: el
    error queda en el redondeo). Más abajo no se puede pedir: con una Rs de
    0,03 Ω entre nodos de 10 V, la corriente (Va - Vx)/Rs solo se resuelve a
    ulp(10 V)/Rs = 5e-14 A. Primero con pasos enteros: con la recta de e^u por
    encima de 80, una unión que se pasa de largo vuelve sola en unas pocas
    iteraciones, y una búsqueda en línea sobre |F| se atasca justo ahí (el paso
    entero multiplica |F| por 1e5). Si no converge, con búsqueda en línea
    (Armijo sobre |F|²). (z, ok)"""
    if sis.n == 0:
        return z0.copy(), True
    for amortigua in (False, True):
        z = z0.copy()
        F, J = sis.ecuaciones(z, ctx)
        nf = float(F @ F)
        for _ in range(itmax):
            try:
                dz = np.linalg.solve(J, -F)
            except np.linalg.LinAlgError:
                break
            if not np.all(np.isfinite(dz)):
                break
            paso = float(np.max(np.abs(dz) / (np.abs(z) + esc)))
            if paso < 1e-10:                        # el paso de más, y ya
                z2 = z + dz
                F2, _ = sis.ecuaciones(z2, ctx, con_j=False)
                return (z2 if float(F2 @ F2) <= nf else z), True
            # una unión que en directa sube más de 10·n·VT de golpe: el paso entero
            # se acorta hasta que suba eso (lejos de la raíz, la tangente de una
            # exponencial manda la tensión a decenas de voltios y volver cuesta una
            # iteración por cada n·VT)
            a = 1.0
            for i, k, sg, off, nvt, w0 in sis.uniones:
                dw = sg * ((dz[i] if i >= 0 else 0.0) - (dz[k] if k >= 0 else 0.0))
                w1 = sg * ((z[i] if i >= 0 else 0.0) - (z[k] if k >= 0 else 0.0)) + off + dw
                if dw > 10.0 * nvt and w1 > w0:
                    a = min(a, 10.0 * nvt / dw)
            while True:                             # |F|² baja al menos 1e-4·a de lo que promete
                z2 = z + a * dz
                F2, J2 = sis.ecuaciones(z2, ctx)
                n2 = float(F2 @ F2)
                if not amortigua or n2 <= (1.0 - 1e-4 * a) * nf or a < 1e-10:
                    break
                a *= 0.5
            if amortigua and not n2 < nf:           # no baja: el redondeo, o un mínimo que no es raíz
                if paso < 1e-9:
                    return z, True
                break
            if not np.isfinite(n2):
                break
            z, F, J, nf = z2, F2, J2, n2
    return z0.copy(), False


def _hybr(sis, ctx, z0):
    """scipy.optimize.root (hybr, xtol 1e-14), sin más. (z, terminó con un número)"""
    if sis.n == 0:
        return z0, True
    try:
        r = root(lambda z: sis.ecuaciones(z, ctx), z0, jac=True, method='hybr',
                 options={'xtol': 1e-14, 'maxfev': 200 * (sis.n + 1)})
    except (ValueError, np.linalg.LinAlgError, OverflowError):
        return z0, False
    return (r.x, True) if np.all(np.isfinite(r.x)) else (z0, False)


def _raiz(sis, ctx, z0, esc):
    """Una raíz desde z0: hybr (xtol 1e-14) pulido con Newton, que tiene que
    converger por su paso (_newton); si hybr se queda a medias («no hace
    progreso» lejos de la raíz, lo normal con exponenciales desde 0), Newton
    amortiguado desde z0. (z, ok)"""
    z, ok = _hybr(sis, ctx, z0)
    if ok:
        z, ok = _newton(sis, ctx, z, esc)
        if ok:
            return z, True
    return _newton(sis, ctx, z0, esc)


def _esc_apriori(sis, fuentes):
    """Escalas de las incógnitas antes de resolver: las E y los raíles de los AO;
    las corrientes, esa tensión sobre la R mediana"""
    ev = [abs(fuentes[e.nom]) for e in sis.fuentes if e.tipo == 'V'] + [1.0]
    ev += [abs(e.v[1]) + abs(e.v[2]) for e in sis.aos]
    escV = max(ev)
    ei = [abs(fuentes[e.nom]) for e in sis.fuentes if e.tipo == 'I'] + [escV / sis.rtip]
    return np.where(sis.es_corriente, max(ei), escV)


def escalas(sis, V, I, IB, fuentes):
    """La escala de tensiones (potenciales y E) y la de corrientes: la mayor de
    las de los elementos y las J, y como poco la escala de tensiones sobre la R
    mediana (rtip). Ese suelo es la resolución de una corriente que sale de una
    diferencia de potenciales: con todos los diodos en inversa las corrientes
    son de pA y la de una R, (Va - Vb)/R, solo tiene las cifras que sobreviven
    a la resta; frente a ellas sola, ni el oráculo ni el motor llegan a 1e-9."""
    ev = [abs(v) for v in V] + [abs(fuentes[e.nom]) for e in sis.fuentes if e.tipo == 'V']
    ei = [abs(v) for v in I] + [abs(v) for v in IB] + [abs(fuentes[e.nom]) for e in sis.fuentes if e.tipo == 'I']
    escV = max(ev + [1e-300])
    return escV, max(ei + [escV / sis.rtip])


def _escalas(sis, z, ctx):
    """Las escalas de una solución del oráculo"""
    I, IB = sis.corrientes(z, ctx)
    return escalas(sis, sis.potenciales(z).values(), I.values(), IB.values(), ctx.fuentes)


def escalas_ao(sis, escV, escI):
    """La escala de la corriente de salida de cada AO real con Ro > 0 (nombre ->
    escala; las demás corrientes van con escI). Esa corriente es (A·(V+ - V-)
    - Vsal)/Ro: un ulp de V+ o de V- (que están a escV) la mueve A/Ro veces
    (con A = 1,6e5 y Ro = 62 Ω, 2600 S: ulp(10 V)·2600 = 5e-12 A, el 1e-9 de
    unos mA). Ni el oráculo ni el motor la tienen mejor: la de los dos sale de
    potenciales resueltos al redondeo, y con Ro = 0 (una rama) no pasa. Su
    escala es como poco A·escV/Ro por 1e-5: 1e-9 de eso son unas 50 ulps de V+
    - V- amplificadas por A/Ro."""
    return {e.nom: max(escI, 1e-5 * e.v[0] * escV / e.v[4]) for e in sis.aos if e.v[0] > 0.0 and e.v[4] > 0.0}


def _combinaciones(sis):
    return [dict(zip([e.nom for e in sis.aos], c)) for c in itertools.product((LIN, SATP, SATN), repeat=len(sis.aos))]


def _fuentes_dc(sis, lam=1.0):
    return {e.nom: lam * _valor_dc(e.forma) for e in sis.fuentes}


def soluciones_cc(sis, z_ces=None, esc=None, semilla=0, detalle=False):
    """Las soluciones del oráculo en CC: por cada combinación de tramos de los AO,
    desde 0, desde la del programa perturbada 1e-3 y con la rampa de fuentes.
    Devuelve [(z, combinación, de dónde)]"""
    rng = np.random.default_rng(semilla)
    sols = []

    def acepta(z, combo, origen):
        ctx = Ctx(_fuentes_dc(sis), ao=combo)
        escV, escI = _escalas(sis, z, ctx)
        r, _ = sis.residuo(z, ctx, escV, escI)
        if not (r <= 1e-12 and sis.cumple(z, ctx, 1e-9 * escV)):
            return
        # ¿ya estaba? (misma solución: a 1e-7 de la escala)
        esc_z = np.where(sis.es_corriente, escI, escV)
        for s in sols:
            if np.max(np.abs(s[0] - z) / esc_z) < 1e-7:
                s[2].add(origen)
                return
        sols.append((z, combo, {origen}))

    esc_ap = _esc_apriori(sis, _fuentes_dc(sis))
    for combo in _combinaciones(sis):
        ctx = Ctx(_fuentes_dc(sis), ao=combo)
        z, ok = _raiz(sis, ctx, np.zeros(sis.n), esc_ap)
        if ok:
            acepta(z, combo, 'cero')
        if z_ces is not None:
            escV, escI = esc
            esc_z = np.where(sis.es_corriente, escI, escV)
            z0 = z_ces + 1e-3 * (np.abs(z_ces) + esc_z) * rng.uniform(-1.0, 1.0, sis.n)
            z, ok = _raiz(sis, ctx, z0, esc_ap)
            if ok:
                acepta(z, combo, 'programa')
        # la rampa de fuentes (las E y las J; no los raíles ni las ganancias)
        lam, dl = 0.0, 0.05
        z, ok = _raiz(sis, Ctx(_fuentes_dc(sis, 0.0), ao=combo), np.zeros(sis.n), esc_ap)
        while ok and lam < 1.0:
            l2 = min(1.0, lam + dl)
            z2, ok2 = _raiz(sis, Ctx(_fuentes_dc(sis, l2), ao=combo), z, esc_ap)
            if ok2:
                lam, z, dl = l2, z2, min(0.25, 1.5 * dl)
            else:
                dl /= 4.0
                ok = dl >= 1e-4
        if ok:
            acepta(z, combo, 'rampa')
    return sols


# =============================================================================
# CC: comparar con lo que da el programa
# =============================================================================

def lee_ces_cc(texto):
    V, I, IB = {}, {}, {}
    for lin in texto.splitlines():
        p = lin.split()
        if not p or p[0].startswith('#'):
            continue
        if p[0] == 'V':
            V[int(p[1])] = float(p[2])
        elif p[0] == 'I':
            I[p[1]] = float(p[2])
        elif p[0] == 'IB':
            IB[p[1]] = float(p[2])
    return V, I, IB


def z_de_ces(sis, V, I):
    """La solución del programa en las incógnitas del oráculo: potenciales, el
    nodo interno de cada unión con Rs (de su corriente: vx = va - Rs·(I - GMIN·v))
    y las corrientes de rama"""
    z = np.zeros(sis.n)
    for nd in sis.nodos:
        z[sis.iv[nd]] = V[nd]
    for e in sis.red.el:
        if e.jb >= 0:
            z[e.jb] = I[e.nom]
        if e.jx >= 0:
            va = V.get(e.nodos[0], 0.0)
            vb = V.get(e.nodos[1], 0.0)
            z[e.jx] = va - e.v[2] * (I[e.nom] - GMIN * (va - vb))
    return z


def compara_cc(nombre, texto, texto_ces, detalle=False):
    """(igual, peor diferencia, residuo del programa, n.º de soluciones, notas, de dónde)"""
    red = Red(texto)
    sis = Sistema(red)
    V, I, IB = lee_ces_cc(texto_ces)
    fu = _fuentes_dc(sis)
    escV, escI = escalas(sis, V.values(), I.values(), IB.values(), fu)
    zE = z_de_ces(sis, V, I)
    resE, fila = sis.residuo(zE, Ctx(fu), escV, escI)
    semilla = sum(ord(c) for c in nombre)
    sols = soluciones_cc(sis, zE, (escV, escI), semilla)
    escAO = escalas_ao(sis, escV, escI)
    notas = []
    mejor, cual = float('inf'), None
    for z, combo, origen in sols:
        ctx = Ctx(fu, ao=combo)
        Io, IBo = sis.corrientes(z, ctx)
        Vo = sis.potenciales(z)
        d = max([abs(V[nd] - Vo[nd]) / escV for nd in sis.nodos] +
                [abs(I[k] - Io[k]) / escAO.get(k, escI) for k in Io] + [abs(IB[k] - IBo[k]) / escI for k in IBo])
        if d < mejor:
            mejor, cual = d, (z, combo, origen, Vo, Io, IBo)
    igual = mejor <= TOL_CC and resE <= TOL_RES
    if not igual:
        notas.append(f'residuo del programa {resE:.3g} (fila {fila}); {len(sols)} soluciones del oráculo; '
                     f'la más cercana a {mejor:.3g} de la escala (escV {escV:.4g}, escI {escI:.4g})')
        if cual is not None:
            z, combo, origen, Vo, Io, IBo = cual
            dif = [(abs(V[nd] - Vo[nd]) / escV, f'V({nd}) {V[nd]!r} frente a {Vo[nd]!r}') for nd in sis.nodos]
            dif += [(abs(I[k] - Io[k]) / escAO.get(k, escI), f'I({k}) {I[k]!r} frente a {Io[k]!r}') for k in Io]
            dif += [(abs(IB[k] - IBo[k]) / escI, f'IB({k}) {IB[k]!r} frente a {IBo[k]!r}') for k in IBo]
            dif.sort(reverse=True)
            notas += [f'{d:.3g}: {t}' for d, t in dif[:4]]
            notas.append(f'tramos de los AO {combo}; hallada {sorted(origen)}')
    if detalle:
        print(f'{nombre}: {len(sols)} soluciones; residuo del programa {resE:.3g}; la más cercana a {mejor:.3g}')
        for z, combo, origen in sols:
            print('   ', combo, sorted(origen))
    origen = cual[2] if cual is not None else set()
    return igual, mejor, resE, len(sols), notas, origen


# =============================================================================
# Tiempo
# =============================================================================

class Transitorio:
    def __init__(self, red):
        self.red = red
        self.sis = Sistema(red)
        self.z = None
        self.ao = None
        self.ctx_ult = None
        amp = [1.0]
        for e in red.el:
            if e.tipo == 'V':
                amp.append(orc._amplitud(e.forma) if hasattr(orc, '_amplitud') else 1.0)
            if e.tipo == 'AO':
                amp.append(abs(e.v[1]) + abs(e.v[2]))
        self.escV = max(amp)
        rs = [e.v[0] for e in red.el if e.tipo == 'R']
        self.escI = self.escV / (min(rs) if rs else 1e3)
        self.esc_z = np.where(self.sis.es_corriente, self.escI, self.escV)

    def _ctx(self, seg, t, x, lam=1.0):
        s = self.sis
        fu = {e.nom: lam * float(seg['f'][e.nom](t)) for e in s.fuentes}
        xC = {c.nom: lam * x[k] for k, c in enumerate(s.caps)}
        xL = {l.nom: lam * x[len(s.caps) + k] for k, l in enumerate(s.inds)}
        return Ctx(fu, xC, xL, seg['s'], dict(self.ao) if self.ao is not None else None)

    def _cambia_tramos(self, z, ctx):
        """El tramo que pide cada AO que no cumple el suyo; None si todos cumplen"""
        s = self.sis
        nuevo, cambia = dict(ctx.ao), False
        tol = 1e-12 * self.escV

        def V(k):
            return z[k] if k >= 0 else 0.0
        for e in s.aos:
            p, m, o, rf = e.i
            A, Vp, Vn = e.v[0], e.v[1], e.v[2]
            vd, vs = V(p) - V(m), V(o) - V(rf)
            est = ctx.ao[e.nom]
            w = vs if A == 0.0 else A * vd
            if est == LIN and (w > Vp + tol or w < Vn - tol):
                nuevo[e.nom] = SATP if w > Vp else SATN
                cambia = True
            elif est != LIN and ((A == 0.0 and (vd < -tol if est == SATP else vd > tol)) or
                                 (A > 0.0 and (w < Vp - tol if est == SATP else w > Vn + tol))):
                nuevo[e.nom] = LIN
                cambia = True
        return nuevo if cambia else None

    def _resuelve(self, ctx):
        """Newton desde la solución anterior; si no converge, hybr desde ella y
        Newton desde lo que dé (que tiene que converger por su paso)"""
        s = self.sis
        z, ok = _newton(s, ctx, self.z, self.esc_z)
        if ok:
            return z, True
        return _raiz(s, ctx, self.z, self.esc_z)

    def _intenta(self, ctx):
        """Desde self.z con los tramos de ctx, cambiando los de los AO que no
        cumplen el suyo hasta que todos cumplan. (z, ctx) o None"""
        s = self.sis
        vistos = set()
        for _ in range(3 ** len(s.aos) + 2):
            vistos.add(tuple(sorted(ctx.ao.items())) if ctx.ao else ())
            z, ok = self._resuelve(ctx)
            if not ok:
                return None
            nuevo = self._cambia_tramos(z, ctx) if s.aos else None
            if nuevo is None:
                return z, ctx
            if tuple(sorted(nuevo.items())) in vistos:
                return None
            ctx = Ctx(ctx.fuentes, ctx.xC, ctx.xL, ctx.cerrado, nuevo)
        return None

    def _acepta(self, z, ctx):
        self.z, self.ao, self.ctx_ult = z, ctx.ao, ctx
        return z, ctx

    def algebraico(self, seg, t, x):
        """El algebraico en (t, x) desde el anterior; deja la solución en self.z.
        Si no sale directo (un flanco que enciende un transistor de golpe), por
        continuación desde el último resuelto: las fuentes y el estado van de
        los de entonces a los de ahora en pasos que se ajustan solos."""
        s = self.sis
        ctx = self._ctx(seg, t, x)
        r = self._intenta(ctx)
        if r is not None:
            return self._acepta(*r)
        if self.ctx_ult is not None:
            c0, z0, ao0 = self.ctx_ult, self.z, self.ao
            lam, dl = 0.0, 0.25
            while lam < 1.0 and dl >= 1e-6:
                l2 = min(1.0, lam + dl)
                c = Ctx({k: (1.0 - l2) * c0.fuentes[k] + l2 * v for k, v in ctx.fuentes.items()},
                        {k: (1.0 - l2) * c0.xC[k] + l2 * v for k, v in ctx.xC.items()},
                        {k: (1.0 - l2) * c0.xL[k] + l2 * v for k, v in ctx.xL.items()},
                        ctx.cerrado, dict(self.ao) if self.ao is not None else None)
                r = self._intenta(c)
                if r is not None:
                    lam, dl = l2, min(0.5, 2.0 * dl)
                    self.z, self.ao = r[0], r[1].ao
                else:
                    dl /= 4.0
            if lam >= 1.0:
                ctx.ao = self.ao
                return self._acepta(self.z, ctx)
            self.z, self.ao = z0, ao0
        # todas las combinaciones, y la más cercana a la anterior que cumpla
        mejor = None
        for combo in _combinaciones(s):
            c = Ctx(ctx.fuentes, ctx.xC, ctx.xL, ctx.cerrado, combo)
            z, ok = self._resuelve(c)
            if ok and s.cumple(z, c, 1e-12 * self.escV):
                d = float(np.max(np.abs(z - self.z)))
                if mejor is None or d < mejor[0]:
                    mejor = (d, z, c)
        if mejor is None:
            # sin la anterior (un S que se abre y deja una base colgada de un C):
            # la rampa desde 0, y de sus soluciones la más cercana a la anterior
            z_ant, ao_ant = self.z, self.ao
            hallada = self._rampa(seg, t, x)
            self.ao = ao_ant
            for z, combo in hallada:
                d = float(np.max(np.abs(z - z_ant)))
                if mejor is None or d < mejor[0]:
                    mejor = (d, z, Ctx(ctx.fuentes, ctx.xC, ctx.xL, ctx.cerrado, combo))
        if mejor is None:
            raise ErrorRed(f'el algebraico no tiene solución en t = {t!r}')
        return self._acepta(mejor[1], mejor[2])

    def _rampa(self, seg, t, x):
        """Las soluciones del algebraico en (t, x) sin mirar la anterior: en cada
        combinación de tramos de los AO, desde 0 con las fuentes y el estado
        por lam de 0 a 1 (paso que se ajusta solo). [(z, tramos)] de las que
        cumplen su tramo, sin repetir"""
        s = self.sis
        hallada = []
        for combo in _combinaciones(s):
            self.ao = combo
            z, ok = _raiz(s, self._ctx(seg, t, x, 0.0), np.zeros(s.n), self.esc_z)
            lam, dl = 0.0, 0.05
            while ok and lam < 1.0:
                l2 = min(1.0, lam + dl)
                z2, ok2 = _raiz(s, self._ctx(seg, t, x, l2), z, self.esc_z)
                if ok2:
                    lam, z, dl = l2, z2, min(0.25, 2.0 * dl)
                else:
                    dl /= 4.0
                    ok = dl >= 1e-5
            c = self._ctx(seg, t, x)
            if ok and s.cumple(z, c, 1e-9 * self.escV):
                if not any(np.max(np.abs(z - h[0])) < 1e-7 * self.escV for h in hallada):
                    hallada.append((z, combo))
        return hallada

    def arranque(self, seg, x0):
        """El algebraico en t = 0 (sin anterior), con _rampa; si hay varias
        soluciones que cumplen su tramo, error"""
        hallada = self._rampa(seg, 0.0, x0)
        if not hallada:
            raise ErrorRed('el algebraico no tiene solución en t = 0')
        if len(hallada) > 1:
            raise ErrorRed(f'el algebraico tiene {len(hallada)} soluciones en t = 0')
        self.z, self.ao = hallada[0]
        self.ctx_ult = self._ctx(seg, 0.0, x0)

    def f(self, seg):
        s = self.sis
        nC = len(s.caps)

        def fun(t, x):
            z, _ = self.algebraico(seg, t, x)
            d = np.empty(s.nx)
            for k, c in enumerate(s.caps):
                d[k] = z[c.jb] / c.v[0]
            for k, l in enumerate(s.inds):
                a, b = l.i
                d[nC + k] = ((z[a] if a >= 0 else 0.0) - (z[b] if b >= 0 else 0.0)) / l.v[0]
            return d

        def jac(t, x):
            z, ctx = self.algebraico(seg, t, x)
            _, J = s.ecuaciones(z, ctx)
            dz = -np.linalg.solve(J, s.Fx)                 # dz/dx por la función implícita
            M = np.empty((s.nx, s.nx))
            for k, c in enumerate(s.caps):
                M[k] = dz[c.jb] / c.v[0]
            for k, l in enumerate(s.inds):
                a, b = l.i
                M[nC + k] = ((dz[a] if a >= 0 else 0.0) - (dz[b] if b >= 0 else 0.0)) / l.v[0]
            return M
        return fun, jac

    def canal(self, tipo, cual, z, ctx):
        s = self.sis
        if tipo == 'V':
            nd = int(cual)
            return 0.0 if nd == 0 else z[s.iv[nd]]
        return s.corrientes(z, ctx)[0][cual]

    def simula(self, ts):
        """Los canales en los instantes ts: {k: [y...]}"""
        red, s = self.red, self.sis
        tfin, h = red.tfin, red.h
        tol_ev = 1e-9 * h
        # los bordes: flancos y vértices de las fuentes, conmutaciones de los S
        bordes = set()
        for e in s.fuentes:
            bordes.update(e.forma.eventos(tfin))
        for e in red.el:
            if e.tipo == 'S':
                for tt in (e.v[1], e.v[2]):
                    if 0.0 < tt < tfin:
                        bordes.add(tt)
        bordes = sorted(tt for tt in bordes if tol_ev < tt < tfin - tol_ev)
        cortes = [0.0]
        for tt in bordes:
            if tt - cortes[-1] > tol_ev:
                cortes.append(tt)
        cortes.append(tfin)
        x = np.array([c.v[1] for c in s.caps] + [l.v[1] for l in s.inds], dtype=float)
        atol = np.array([1e-12 * self.escV] * len(s.caps) + [1e-12 * self.escI] * len(s.inds))
        salida = {k: [None] * len(ts) for k, _, _ in red.canales}
        for iseg in range(len(cortes) - 1):
            ta, tb = cortes[iseg], cortes[iseg + 1]
            tm = 0.5 * (ta + tb)
            seg = {'f': {e.nom: e.forma.pieza(tm)[0] for e in s.fuentes},
                   's': {e.nom: self._cerrado(e, tm) for e in red.el if e.tipo == 'S'}}
            if iseg == 0:
                self.arranque(seg, x)
            else:                             # el límite por la derecha en el borde
                self.algebraico(seg, ta, x)
            inicio = (self.z, self.ao, self.ctx_ult)
            ultimo = iseg == len(cortes) - 2
            idx = [i for i, t in enumerate(ts) if ta - tol_ev <= t < tb - tol_ev or (ultimo and abs(t - tb) <= tol_ev)]
            if s.nx:
                fun, jac = self.f(seg)
                te = sorted(set([min(ts[i], tb) for i in idx if ts[i] > ta] + [tb]))
                sol = solve_ivp(fun, (ta, tb), x, method='Radau', t_eval=te, rtol=1e-10, atol=atol, jac=jac)
                if sol.status != 0:
                    raise ErrorRed(f'Radau no llega de {ta!r} a {tb!r}: {sol.message}')
                X = {t: sol.y[:, j] for j, t in enumerate(sol.t)}
                X[ta] = x
                xb = sol.y[:, -1]
            else:
                X, xb = None, x
            # las muestras, en orden y cada una desde la anterior (desde la del
            # borde la primera): la integración deja el algebraico en tb, y un
            # Newton desde allí hasta ta puede caer en otra raíz o no llegar
            self.z, self.ao, self.ctx_ult = inicio
            for i in idx:
                t = ts[i]
                xi = x if X is None else (X[t] if t in X else X[min(X, key=lambda q: abs(q - t))])
                z, ctx = self.algebraico(seg, min(max(t, ta), tb), xi)
                for k, tipo, cual in red.canales:
                    salida[k][i] = self.canal(tipo, cual, z, ctx)
            x = np.array(xb, dtype=float)
        return salida

    @staticmethod
    def _cerrado(e, tref):
        c = bool(e.v[0])
        if e.v[1] > 0.0 and tref >= e.v[1]:
            c = not c
        if e.v[2] > 0.0 and tref >= e.v[2]:
            c = not c
        return c


def lee_ces_tr(texto):
    t, y = [], {0: [], 1: []}
    for lin in texto.splitlines()[1:]:
        if not lin.strip() or lin.startswith('#'):
            continue
        a, b, c = lin.split(',')
        t.append(float(a))
        y[0].append(float(b))
        y[1].append(float(c))
    return t, y


def _nota_h(texto):
    """La línea «# h ...» que el arnés deja al final de un .ces del tiempo"""
    return next((lin[2:] for lin in texto.splitlines() if lin.startswith('# h ')), '')


def _errores(tr, ts, ref, y):
    """Por canal: (peor error de la escala, fracción de instantes <= 1e-3, índice
    del peor, escala). La escala: el máximo de |y| del oráculo, con un suelo de
    una millonésima de la escala del circuito (la de tensiones, sus fuentes y
    raíles; la de corrientes, esa sobre la R mediana): un canal que es cero en
    todo el caso (la corriente de una fuente sin vuelta) no es una escala, y
    su ruido (1e-13 A frente a corrientes de mA) no es un error"""
    out = []
    for k, tipo, cual in tr.red.canales:
        r = np.array(ref[k])
        suelo = 1e-6 * (tr.escV if tipo == 'V' else tr.escV / tr.sis.rtip)
        esc = max(float(np.max(np.abs(r))), suelo)
        err = np.abs(np.array(y[k]) - r) / esc
        out.append((float(np.max(err)), float(np.mean(err <= TOL_TR_90)), int(np.argmax(err)), esc))
    return out


def compara_tr(nombre, texto, texto_ces, detalle=False, texto_h4=None):
    """(igual, peor error, fracción de instantes <= 1e-3, notas, (igual, peor) con
    h/4 o None). texto_h4: lo que da el programa con un cuarto de su paso"""
    red = Red(texto)
    ts, y = lee_ces_tr(texto_ces)
    ts4, y4 = lee_ces_tr(texto_h4) if texto_h4 is not None else ([], None)
    tr = Transitorio(red)
    # los instantes son los puntos que calculó el motor (con su paso y con h/4):
    # una sola integración para los dos
    todos = sorted(set(ts) | set(ts4))
    sal = tr.simula(todos)
    pos = {t: i for i, t in enumerate(todos)}
    ref = {k: [sal[k][pos[t]] for t in ts] for k in sal}
    peor, frac_min, notas = 0.0, 1.0, []
    for (k, tipo, cual), (e, frac, i0, esc) in zip(red.canales, _errores(tr, ts, ref, y)):
        r = ref[k]
        if detalle:
            print(f'   canal {k + 1} ({tipo} {cual}): peor {e:.3g} en t = {ts[i0]:.6g} (programa {y[k][i0]!r}, '
                  f'oráculo {r[i0]!r}); {100 * frac:.1f} % <= 1e-3; escala {esc:.4g}')
        if e > TOL_TR_MAX or frac < 0.9:
            notas.append(f'canal {k + 1} ({tipo} {cual}): peor {e:.3g} de la escala {esc:.4g} en t = {ts[i0]:.6g} '
                         f'(programa {y[k][i0]:.10g}, oráculo {r[i0]:.10g}); {100 * frac:.1f} % de los instantes <= 1e-3')
        peor = max(peor, e)
        frac_min = min(frac_min, frac)
    if notas and _nota_h(texto_ces):
        notas.append(f'el programa: {_nota_h(texto_ces)}')
    h4 = None
    if y4 is not None:
        ref4 = {k: [sal[k][pos[t]] for t in ts4] for k in sal}
        er4 = _errores(tr, ts4, ref4, y4)
        p4 = max(e for e, _, _, _ in er4)
        h4 = (all(e <= TOL_TR_MAX and f >= 0.9 for e, f, _, _ in er4), p4)
        if notas:
            notas.append(f'con un cuarto del paso ({_nota_h(texto_h4)}): peor {p4:.3g} de la escala, '
                         f'{100 * min(f for _, f, _, _ in er4):.1f} % de los instantes <= 1e-3')
    return not notas, peor, frac_min, notas, h4


# =============================================================================
# Pruebas del propio oráculo: los casos cerrados de la especificación
# =============================================================================

def _op(texto):
    """La solución única del oráculo de una red de CC (desde 0 y con la rampa)"""
    sis = Sistema(Red('cesnet 3\ndc\n' + texto))
    sols = soluciones_cc(sis)
    return sis, sols


def _uno(texto):
    sis, sols = _op(texto)
    assert len(sols) == 1, f'{len(sols)} soluciones'
    z, combo, _ = sols[0]
    ctx = Ctx(_fuentes_dc(sis), ao=combo)
    I, IB = sis.corrientes(z, ctx)
    return sis.potenciales(z), I, IB


def _cerca(a, b, tol, que):
    assert abs(a - b) <= tol * abs(b), f'{que}: {a!r} frente a {b!r} ({abs(a - b) / abs(b):.3g})'


def pruebas():
    # §6.5: diodo exponencial, 1N4148, 1N4007, 100 V con 1 Ω y 10 en serie
    V, I, _ = _uno('V E 0 1 0 DC 5\nR R 1 2 1000\nD D 2 0 1e-14 1 0\n')
    _cerca(I['D'], 4.3071121676219442e-3, 1e-12, 'diodo 5 V 1 kΩ')
    _cerca(V[2], 0.69288783237805583, 1e-12, 'V del diodo')
    V, I, _ = _uno('V E 0 1 0 DC 5\nR R 1 2 1000\nD D 2 0 2.52e-9 1.752 0.568\n')
    _cerca(I['D'], 4.3467715385002356e-3, 1e-12, '1N4148')
    V, I, _ = _uno('V E 0 1 0 DC 10\nR R 1 2 100\nD D 2 0 7.02767e-9 1.80803 0.0341512\n')
    _cerca(I['D'], 92.303421120134247e-3, 1e-12, '1N4007 10 V 100 Ω')
    V, I, _ = _uno('V E 0 1 0 DC 100\nR R 1 2 1\nD D 2 0 1e-14 1 0\n')
    _cerca(I['D'], 99.047348503037482, 1e-12, 'diodo 100 V 1 Ω')
    V, I, _ = _uno('V E 0 1 0 DC -5\nR R 1 2 1000\nD D 2 0 1e-14 1 0\n')
    _cerca(I['D'], -5.00999999499e-12, 1e-9, 'diodo en inversa')
    red = 'V E 0 1 0 DC 10\nR R 1 2 10\n' + ''.join(f'D D{k} {k + 2} {k + 3 if k < 9 else 0} 1e-14 1 0\n' for k in range(10))
    V, I, _ = _uno(red)
    _cerca(I['D0'], 206.95689299461996e-3, 1e-12, '10 diodos en serie')
    # zener 1N4733A (Rs y BV del §3.A) con 12 V, 220 Ω y 1 kΩ; su curva en -5,1 V
    z = 'DZ Z 0 2 1e-14 1 6.4721443717075765 4.7828649257863288 0.049\n'
    V, I, _ = _uno('V E 0 1 0 DC 12\nR Rs 1 2 220\n' + z + 'R RL 2 0 1000\n')
    _cerca(V[2], 4.9431873823062371, 1e-12, 'zener: Vo')
    _cerca(-I['Z'], 27.133233607210867e-3, 1e-12, 'zener: I')
    V, I, _ = _uno('V E 0 2 0 DC 5.1\n' + z)
    _cerca(I['Z'], -49.000000005100754e-3, 1e-9, 'zener en -5,1 V')
    # LED rojo: pasa por (1,8 V, 20 mA); con 9 V y 330 Ω
    led = 'D L 2 0 1.0685493590266111e-16 2 5\n'
    V, I, _ = _uno('V E 0 1 0 DC 9\nR R 1 2 330\n' + led)
    _cerca(I['L'], 21.777894098247443e-3, 1e-12, 'LED 9 V 330 Ω')
    V, I, _ = _uno('V E 0 1 0 DC 9\nR R 1 2 360\n' + led)
    _cerca(I['L'], 20e-3, 1e-12, 'LED 9 V 360 Ω')
    # §6.8: inversor ideal y real; saturado con 1,6 V
    inv = 'V E 0 1 0 DC {}\nR R1 1 2 1000\nR Rf 2 3 10000\nAO U 0 2 3 0 {} 15 -15 0 0\n'
    V, I, _ = _uno(inv.format(1, 0))
    _cerca(V[3], -10.0, 1e-15, 'inversor ideal')
    _cerca(I['U'], -1e-3, 1e-12, 'inversor ideal: I(U)')
    V, I, _ = _uno(inv.format(1, 1e5))
    _cerca(V[3], -9.9989001209866915, 1e-12, 'inversor A = 1e5')
    V, I, _ = _uno(inv.format(1.6, 1e5))
    _cerca(V[3], -15.0, 1e-15, 'inversor saturado')
    _cerca(V[2], 0.090909090909090909, 1e-12, 'inversor saturado: V-')
    V, I, _ = _uno('V E 0 1 0 DC 1\nAO U 1 2 3 0 0 15 -15 0 0\nR R1 2 0 1000\nR R2 3 2 9000\n')
    _cerca(V[3], 10.0, 1e-12, 'no inversor ideal')
    V, I, _ = _uno('V E1 0 1 0 DC 1\nV E2 0 2 0 DC 2\nR Ra 1 3 1e4\nR Rb 2 3 1e4\nR Rf 3 4 1e4\n'
                   'AO U 0 3 4 0 0 15 -15 0 0\n')
    _cerca(V[4], -3.0, 1e-12, 'sumador')
    # Schmitt (V+ = beta·Vsal): la de 0 V (inestable) y las dos saturadas
    sis, sols = _op('V E 0 1 0 DC 0\nR R1 2 0 1000\nR R2 3 2 1000\nAO U 2 1 3 0 0 15 -15 0 0\nR RL 3 0 1000\n')
    assert len(sols) == 3, f'Schmitt: {len(sols)} soluciones'
    # modelo 40: amplificador con una E controlada (100·V(Rin))
    V, I, _ = _uno('V E1 0 1 0 DC 0.01\nR Rs 1 2 1000\nR Rin 2 0 9000\nEV E2 0 3 Rin 100\nR Rsal 3 4 100\nR RL 4 0 900\n')
    _cerca(V[4], 0.81, 1e-12, 'modelo 40')
    # §6.11: los puntos del BJT
    ic, ib = bjt(1.0, 1e-15, 100.0, 1.0, 0.0, 0.7, -4.3)[:2]
    _cerca(ic, 5.6702947265407799e-4, 1e-14, 'BJT IC')
    _cerca(ib, 5.6702910825207799e-6, 1e-14, 'BJT IB')
    ic2, ib2 = bjt(-1.0, 1e-15, 100.0, 1.0, 0.0, -0.7, 4.3)[:2]
    assert ic2 == -ic and ib2 == -ib, 'PNP'
    ic, ib = bjt(1.0, 1e-15, 100.0, 1.0, 0.0, 0.7, 0.6)[:2]
    _cerca(ic, 5.4328572891369173e-4, 1e-14, 'BJT saturado IC')
    _cerca(ib, 1.7542165402713912e-5, 1e-14, 'BJT saturado IB')
    _cerca(bjt(1.0, 1e-15, 100.0, 1.0, 100.0, 0.7, -4.3)[0], 5.9141173979326034e-4, 1e-14, 'BJT con Early')
    # emisor común de Boylestad y de la biblioteca
    ec = 'V E 0 1 0 DC {}\nR R1 1 2 {}\nR R2 2 0 {}\nR RC 1 4 {}\nR RE 3 0 {}\nQ Q 4 2 3 {} 1e-15 {} 1 {}\n'
    V, I, IB = _uno(ec.format(22, 39e3, 3.9e3, 10e3, 1.5e3, 'NPN', 140, 0))
    _cerca(I['Q'], 0.83972087399969434e-3, 1e-9, 'Boylestad IC')
    _cerca(V[4] - V[3], 12.334212956136149, 1e-9, 'Boylestad VCE')
    V, I, IB = _uno(ec.format(22, 39e3, 3.9e3, 10e3, 1.5e3, 'NPN', 140, 100))
    _cerca(I['Q'], 0.84354456360751576e-3, 1e-9, 'Boylestad con Early IC')
    V, I, IB = _uno(ec.format(12, 47e3, 10e3, 2.2e3, 470, 'NPN', 100, 0))
    _cerca(I['Q'], 2.4541907177293845e-3, 1e-9, 'emisor común IC')
    _cerca(V[4] - V[3], 5.4357760891729673, 1e-9, 'emisor común VCE')
    _cerca(IB['Q'], 24.541903169310483e-6, 1e-9, 'emisor común IB')
    Vp, Ip, IBp = _uno(ec.format(-12, 47e3, 10e3, 2.2e3, 470, 'PNP', 100, 0))
    _cerca(Ip['Q'], -I['Q'], 1e-12, 'PNP')
    # el interruptor con LED rojo
    V, I, IB = _uno('V E1 0 1 0 DC 5\nR RB 1 2 4700\nV E2 0 3 0 DC 9\nR R 3 4 330\n' + led.replace('2 0', '4 5') +
                    'Q Q 5 2 0 NPN 1e-15 100 1 0\n')
    _cerca(I['L'], 21.506917662571294e-3, 1e-9, 'interruptor: I(LED)')
    _cerca(V[5], 0.091424804456153236, 1e-9, 'interruptor: VCE')
    # §6.11: el MOSFET canal N (K 1 mA/V², Vt 2 V) y dibujado al revés
    V, I, _ = _uno('V E1 0 1 0 DC 4\nV E2 0 2 0 DC 12\nR RD 2 3 2000\nM M 3 1 0 N 2 1e-3 0\n')
    _cerca(I['M'], 4.000000004e-3, 1e-9, 'MOS ID')
    _cerca(V[3], 3.999999992, 1e-9, 'MOS VDS')
    V, I, _ = _uno('V E1 0 1 0 DC 4\nV E2 0 2 0 DC 12\nR RD 2 3 2000\nM M 3 1 0 N 2 1e-3 50\n')
    _cerca(I['M'], 4.2758620719381688e-3, 1e-9, 'MOS ID con VA')
    V, I, _ = _uno('V E1 0 1 0 DC 6\nV E2 0 2 0 DC 12\nR RD 2 3 2000\nR RS 4 0 1000\nM M 3 1 4 N 2 1e-3 0\n')
    _cerca(I['M'], 2.4384471883273663e-3, 1e-9, 'MOS con RS')
    V, I, _ = _uno('V E1 0 1 0 DC 4\nV E2 0 2 0 DC 12\nR RD 2 3 2000\nM M 0 1 3 N 2 1e-3 0\n')
    _cerca(I['M'], -4.000000004e-3, 1e-9, 'MOS al revés')
    for (x, y, i) in ((3, 0.5, 0.7575000005e-3), (3, 5, 1.100000005e-3), (4, -1, -5.100000001e-3), (1, 5, 5e-12)):
        _cerca(mos(1.0, 2.0, 1e-3, 50.0, x, y)[0], i, 1e-9, f'MOS en ({x}; {y})')
    _cerca(mos(-1.0, 2.0, 1e-3, 0.0, -4.0, -4.0)[0], -4.000000004e-3, 1e-9, 'MOS canal P')
    # derivadas analíticas frente a diferencias centradas
    rng = np.random.default_rng(1)
    hh = 1e-7
    for _ in range(200):
        # (con las dos uniones muy en directa IC es la resta de dos corrientes de
        # amperios y la diferencia centrada solo tiene el redondeo: vBC < 0,5)
        vbe, vbc = rng.uniform(-1.0, 0.9), rng.uniform(-1.0, 0.5)
        for pol in (1.0, -1.0):
            d = bjt(pol, 1e-15, 100.0, 1.0, 50.0, pol * vbe, pol * vbc)
            for k, (dbe, dbc) in enumerate(((d[2], d[3]), (d[4], d[5]))):
                n_be = (bjt(pol, 1e-15, 100.0, 1.0, 50.0, pol * vbe + hh, pol * vbc)[k] -
                        bjt(pol, 1e-15, 100.0, 1.0, 50.0, pol * vbe - hh, pol * vbc)[k]) / (2 * hh)
                n_bc = (bjt(pol, 1e-15, 100.0, 1.0, 50.0, pol * vbe, pol * vbc + hh)[k] -
                        bjt(pol, 1e-15, 100.0, 1.0, 50.0, pol * vbe, pol * vbc - hh)[k]) / (2 * hh)
                # el redondeo de la diferencia centrada: ~eps·|f|/h
                ruido = 100.0 * 2.2e-16 * abs(d[k]) / hh + 1e-18
                assert abs(dbe - n_be) <= 1e-5 * abs(n_be) + ruido, ('d/dvBE', k, pol, vbe, vbc, dbe, n_be)
                assert abs(dbc - n_bc) <= 1e-5 * abs(n_bc) + ruido, ('d/dvBC', k, pol, vbe, vbc, dbc, n_bc)
        vgs, vds = rng.uniform(-5.0, 5.0, 2)
        m0 = mos(1.0, 2.0, 1e-3, 50.0, vgs, vds)
        n2 = (mos(1.0, 2.0, 1e-3, 50.0, vgs, vds + hh)[0] - mos(1.0, 2.0, 1e-3, 50.0, vgs, vds - hh)[0]) / (2 * hh)
        assert abs(m0[2] - n2) <= 1e-5 * abs(n2) + 1e-9, ('dID/dvDS', vgs, vds, m0[2], n2)
    # el tiempo: RC con escalón (cerrada) y la media onda sin C y con filtro (§6.7)
    red = Red('cesnet 3\ntran 5e-3 1e-4\nV E 0 1 0 PUL 0 2 0 0 0\nR R 1 2 1000\nC C 2 0 1e-6 0\n# canal 1 V 2\n# canal 2 I C\n')
    ts = [k * 1e-4 for k in range(51)]
    y = Transitorio(red).simula(ts)
    for t, v in zip(ts, y[0]):
        assert abs(v - 2.0 * (1.0 - math.exp(-t / 1e-3))) <= 1e-9, ('RC', t, v)
    T = 1.0 / 60.0
    red = Red(f'cesnet 3\ntran {T} {T / 400}\nV E 0 1 0 SIN 10 60 0 0\nD D 1 2 7.02767e-9 1.80803 0.0341512\n'
              'R R 2 0 1000\n# canal 1 V 2\n# canal 2 I D\n')
    y = Transitorio(red).simula([T / 4])
    _cerca(y[0][0], 9.3403009505190993, 1e-12, 'media onda sin C')
    red = Red(f'cesnet 3\ntran {6 * T} {T / 4000}\nV E 0 1 0 SIN 10 60 0 0\nD D 1 2 7.02767e-9 1.80803 0.0341512\n'
              'R R 2 0 1000\nC C 2 0 100e-6 0\n# canal 1 V 2\n# canal 2 I D\n')
    ts = [6 * T * k / 24000 for k in range(24001)]
    y = np.array(Transitorio(red).simula(ts)[0][20000:])
    media = float(np.sum((y[1:] + y[:-1]) * 0.5) / 4000)
    # (con muestras cada T/4000: el trapecio de la media y el máximo entre dos
    # muestras cuestan ~1e-7; el mínimo es un codo (la pendiente salta de -80 a
    # +3000 V/s al encender el diodo) y la muestra más baja puede quedar a 3e-4 V)
    _cerca(media, 8.66987970057439, 1e-6, 'media onda con filtro: media')
    _cerca(float(np.max(y)), 9.307509246232415, 1e-6, 'media onda con filtro: máximo')
    _cerca(float(np.min(y)), 8.01694574349012, 1e-4, 'media onda con filtro: mínimo')
    # una unión sin R sobre una E ideal (el D11 de la red 099 de [67]): su única
    # solución va por la recta de e^u (u = 571, 1,1e29 A); con 1 kΩ, por la curva
    d099 = 'D D {} 0 4.0279725794013491e-09 1.0842116475105286 0\n'
    sis, sols = _op('V E 0 1 0 DC 16\n' + d099.format(1))
    f = fuera_de_curva(sis, sols[0][0], Ctx(_fuentes_dc(sis), ao=sols[0][1])) if len(sols) == 1 else []
    assert len(f) == 1 and f[0][0] == 'D' and 570.0 < f[0][1] < 572.0 and f[0][2] > 1e28, f'099: {f}'
    sis, sols = _op('V E 0 1 0 DC 16\nR R 1 2 1000\n' + d099.format(2))
    assert len(sols) == 1 and not fuera_de_curva(sis, sols[0][0], Ctx(_fuentes_dc(sis), ao=sols[0][1])), '099 con R'
    print('pruebas del oráculo no lineal: bien')


# =============================================================================

# Redes que no cuadran por el motor (hallazgos: el motor no se toca aquí). No
# cuentan, y se dice: solo si siguen distintas con el paso del motor y son
# iguales con un cuarto de él (el hallazgo es de precisión del paso, no de las
# leyes); si no, cuentan como cualquier otra.
# caso_204.net (el BJT saturado con C17 en el colector: la y+ de un flanco con
# un modo de ~6 µs y h = 23 µs, un 3,7 % corta por el arranque de Euler) ya
# no está: con el tramo rígido tras cada flanco (DESIGN §17.3, el banco [68])
# sale igual, con 1,3e-4 de la escala.
HALLAZGOS = {
}


def fuera_de_curva(sis, z, ctx):
    """Las uniones (D, DZ) de una solución que van por la recta de e^u (u =
    vd/(n·VT) > XLIM, D4): esa corriente solo existe en la extensión numérica
    del modelo, no es la de un diodo (la red 099 de [67]: 1,1e29 A por un diodo
    sin R sobre la E ideal de 16 V). [(nombre, u, I)]"""
    I, _ = sis.corrientes(z, ctx)
    fuera = []
    for e in sis.red.el:
        if e.tipo not in ('D', 'DZ'):
            continue
        a, b = e.i[0], e.i[1]
        vx = z[e.jx] if e.jx >= 0 else (z[a] if a >= 0 else 0.0)
        u = (vx - (z[b] if b >= 0 else 0.0)) / (e.v[1] * VT)
        if u > XLIM:
            fuera.append((e.nom, u, I[e.nom]))
    return fuera


def rechazada(nombre, texto, texto_ces):
    """Una red que el motor no emula con sus topes (rechazada_NNN.net: la escribe
    el arnés con su mensaje en la línea «# motor: …»). El oráculo la integra (o
    no); si con 50 iteraciones por paso el motor sí la emula
    (rechazada_NNN.ces), se compara. De una de CC dice cuántas soluciones halla
    y si alguna lleva una unión por la recta de e^u (fuera de la curva)"""
    red = Red(texto)
    motivo = next((lin[len(NOTA_MOTOR):] for lin in texto.splitlines() if lin.startswith(NOTA_MOTOR)), '')
    try:
        if red.modo == 'dc':
            sis = Sistema(red)
            sols = soluciones_cc(sis)
            nota = ''
            for z, combo, _ in sols:
                for nom, u, i in fuera_de_curva(sis, z, Ctx(_fuentes_dc(sis), ao=combo)):
                    nota += (f'; en ella {nom} va por la recta de e^u (u = {u:.0f} > {XLIM:.0f}: {i:.3g} A),'
                             ' fuera de la curva del diodo')
            return f'{motivo}; el oráculo halla {len(sols)} soluciones' + (
                ' (desde 0 o con la rampa)' if sols else '') + nota
        if texto_ces is None:
            N = int(round(red.tfin / red.h))
            Transitorio(red).simula([red.tfin * k / N for k in range(N + 1)])
            return f'{motivo}; el oráculo la integra (el motor, con 50 iteraciones por paso, tampoco)'
        igual, e, frac, notas, _ = compara_tr(nombre, texto, texto_ces)
        return (f'{motivo}; el oráculo la integra y el motor con 50 iteraciones por paso da lo mismo '
                f'({"igual" if igual else "DISTINTA"}: peor {e:.3g} de la escala, {100 * frac:.1f} % de los '
                f'instantes <= 1e-3)' + ''.join('; ' + t for t in notas))
    except (ErrorRed, orc.ErrorCircuito, np.linalg.LinAlgError) as ex_:
        return f'{motivo}; el oráculo tampoco la integra: {ex_}'


def main():
    if '--pruebas' in sys.argv:
        pruebas()
        return 0
    args = sys.argv[1:]
    detalle = '--detalle' in args
    solo = None
    if '--solo' in args:
        solo = int(args[args.index('--solo') + 1])
    libres = [a for i, a in enumerate(args) if not a.startswith('--') and (i == 0 or args[i - 1] != '--solo')]
    carpeta = libres[0] if libres else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'out', 'oraculo_nl')
    casos = sorted(x for x in os.listdir(carpeta) if x.endswith('.net'))
    cc = [0, 0]
    tr = [0, 0]
    peor_cc, peor_res, peor_tr, frac_tr = (0.0, ''), (0.0, ''), (0.0, ''), (1.0, '')
    varias = sin_ayuda = 0
    extra, excluidas = [], []
    tr4, peor_h4, cociente, malas_h4 = [0, 0], (0.0, ''), [], []
    for nombre in casos:
        num = int(''.join(ch for ch in nombre if ch.isdigit()) or -1)
        if solo is not None and num != solo:
            continue
        texto = open(os.path.join(carpeta, nombre), encoding='ascii').read()
        ruta_ces = os.path.join(carpeta, nombre[:-4] + '.ces')
        texto_ces = open(ruta_ces, encoding='ascii').read() if os.path.exists(ruta_ces) else None
        if nombre.startswith('rechazada'):
            extra.append((nombre, rechazada(nombre, texto, texto_ces)))
            continue
        if texto_ces is None:
            continue
        es_cc = '\ndc\n' in texto
        try:
            if es_cc:
                igual, dif, res, nsol, notas, origen = compara_cc(nombre, texto, texto_ces, detalle)
                if dif > peor_cc[0]:
                    peor_cc = (dif, nombre)
                if res > peor_res[0]:
                    peor_res = (res, nombre)
                varias += nsol > 1
                sin_ayuda += bool(origen & {'cero', 'rampa'})
            else:
                if detalle:
                    print(nombre)
                ruta4 = os.path.join(carpeta, nombre[:-4] + '_h4.ces')
                texto_h4 = open(ruta4, encoding='ascii').read() if os.path.exists(ruta4) else None
                igual, e, frac, notas, h4 = compara_tr(nombre, texto, texto_ces, detalle, texto_h4)
                if not igual and nombre in HALLAZGOS and h4 is not None and h4[0]:
                    excluidas.append((nombre, notas))
                    continue
                if e > peor_tr[0]:
                    peor_tr = (e, nombre)
                if frac < frac_tr[0]:
                    frac_tr = (frac, nombre)
                if h4 is not None:
                    tr4[0 if h4[0] else 1] += 1
                    if not h4[0]:
                        malas_h4.append(f'{nombre} ({h4[1]:.3g}; con su paso, {e:.3g})')
                    if h4[1] > peor_h4[0]:
                        peor_h4 = (h4[1], nombre)
                    if e > 0.0:
                        cociente.append(e / max(h4[1], 1e-300))
        except (ErrorRed, orc.ErrorCircuito, np.linalg.LinAlgError) as ex_:
            igual, notas = False, [f'el oráculo no la resuelve: {ex_}']
        cuenta = cc if es_cc else tr
        cuenta[0 if igual else 1] += 1
        if not igual:
            print(f'{nombre}: DISTINTA')
            for t in notas:
                print('    ', t)
    print()
    print(f'CC: {cc[0]} de {cc[0] + cc[1]} iguales; peor diferencia {peor_cc[0]:.3g} de la escala ({peor_cc[1]}; '
          f'tolerancia {TOL_CC:.0e}); peor residuo del programa en las ecuaciones del oráculo {peor_res[0]:.3g} '
          f'({peor_res[1]}; tolerancia {TOL_RES:.0e})')
    print(f'    {varias} redes con varias soluciones del oráculo; en {sin_ayuda} la del programa sale también '
          f'desde x = 0 o con la rampa (sin mirar la del programa)')
    print(f'Tiempo: {tr[0]} de {tr[0] + tr[1]} iguales'
          + (f' ({len(excluidas)} excluida{"s" if len(excluidas) > 1 else ""} por un hallazgo del motor)'
             if excluidas else '')
          + f'; peor error {peor_tr[0]:.3g} de la escala ({peor_tr[1]}; '
          f'tolerancia {TOL_TR_MAX:.0e}); la menor fracción de instantes <= {TOL_TR_90:.0e}: '
          f'{100 * frac_tr[0]:.1f} % ({frac_tr[1]}; mínimo 90 %)')
    if tr4[0] + tr4[1]:
        cociente.sort()
        print(f'    con un cuarto del paso (dato): {tr4[0]} de {tr4[0] + tr4[1]} iguales; peor error {peor_h4[0]:.3g} '
              f'({peor_h4[1]}); el peor error baja una mediana de {cociente[len(cociente) // 2]:.3g} veces '
              f'(el trapecio: 16; menos donde manda un flanco)')
        if malas_h4:
            print(f'    distintas con un cuarto del paso: {", ".join(malas_h4)}')
    for nombre, notas in excluidas:
        print(f'    excluida (hallazgo del motor, no cuenta): {nombre}: {HALLAZGOS[nombre]}')
        for t in notas:
            print('        ', t)
    print(f'{cc[0] + tr[0]} de {cc[0] + cc[1] + tr[0] + tr[1]} iguales')
    if extra:
        print(f'Aparte, {len(extra)} redes que el motor no resuelve (NO_CONVERGE; no cuentan):')
        for nombre, t in extra:
            print(f'    {nombre}: {t}')
    return 0 if cc[1] == 0 and tr[1] == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
