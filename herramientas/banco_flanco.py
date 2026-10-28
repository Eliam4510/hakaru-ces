#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
BANCO DEL ARRANQUE TRAS UN FLANCO  --  herramientas/banco_flanco.py
===================================================================

Escribe las tablas de ../test_flanco.inc (entre «/* == TABLAS» y
«/* == FIN DE LAS TABLAS == */»): los casos del banco y su solución exacta.

  * Redes LINEALES (RC, RL, RLC, las tres de la biblioteca y las dos escaleras
    RC del juez, con hasta 15 modos: FL_MMAX en el .inc): x' = A·x + b en
    cada pieza (entre dos flancos o conmutaciones), y = Cy·x + d. La solución
    es analítica y se calcula con mpmath (40 cifras):
        y(t) = c0 + suma_k e^(re_k·u)·(A_k·cos(im_k·u) + B_k·sin(im_k·u)),
    u = t - ta; el .inc lleva c0, los modos y los coeficientes de cada pieza,
    así que el arnés la evalúa exacta en CUALQUIER instante (también en los
    que un arreglo añada). Antes de t = 0, una pieza constante: el punto de
    operación con las fuentes en reposo.
  * Redes NO LINEALES (diodo con un C pequeño, BJT saturado con C en el
    colector): el oráculo no lineal (oraculo_nl.py, una copia del de
    rev2/oraculo, con su propio MNA escrito aparte del motor) con Radau,
    rtol 1e-12, partido en los flancos, y salida densa. El .inc lleva una
    tabla (t, y1, y2) por pieza, refinada hasta que la cúbica de 4 puntos
    reproduce el punto medio de cada intervalo a 1e-9 de la escala; la y+
    (el primer punto de cada pieza) es el algebraico en te+ con el estado de
    te-, exacta.

La tau de cada pieza es la del modo más rápido en su arranque (1/max|s| de
A; en las no lineales, de la jacobiana de la EDO en te+). La escala de cada
canal es el máximo de |y| de la exacta en [0, t_fin] (0: el canal no se mide).

USO:  python -B banco_flanco.py        (con PYTHONIOENCODING=utf-8 en Windows)
"""
import math
import os
import sys

sys.dont_write_bytecode = True
import mpmath as mp  # noqa: E402
import numpy as np  # noqa: E402
from scipy.integrate import solve_ivp  # noqa: E402

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import oraculo_nl as onl  # noqa: E402

mp.mp.dps = 40
INC = os.path.join(AQUI, '..', 'test_flanco.inc')
MARCA_INI = '/* == TABLAS'
MARCA_FIN = '/* == FIN DE LAS TABLAS == */'

RTOL_NL = 1e-12          # Radau de las no lineales
TOL_TABLA = 1e-9         # la cúbica de la tabla, frente a la escala


def M(filas):
    return mp.matrix([[mp.mpf(v) for v in f] for f in filas])


def V(v):
    return mp.matrix([mp.mpf(x) for x in v])


# =============================================================================
# Lineales: piezas analíticas
# =============================================================================

def piezas_lineales(defs, x0, tfin, medir=(1, 1)):
    """defs: [(ta, A, b, Cy, d)] (mp.matrix) en orden; x0: el estado en 0 (el
    del punto de operación). La pieza de antes de 0: el punto de operación con
    las fuentes en reposo (la da quien llama como defs[-1] en ta = -1).
    Devuelve (piezas, esc)."""
    piezas = []
    x = V(x0)
    for i, (ta, A, b, Cy, d) in enumerate(defs):
        tb = defs[i + 1][0] if i + 1 < len(defs) else tfin
        n = A.rows
        if ta < 0:                                   # antes de 0: constante
            y = Cy * x + d
            piezas.append(dict(ta=-1.0, tau=0.0, modos=[], c0=[y[0], y[1]]))
            continue
        xss = -(mp.inverse(A) * b)
        c0 = Cy * xss + d
        if n == 1:
            ev, Vm = [A[0, 0]], mp.matrix([[1]])
        else:
            ev, Vm = mp.eig(A)
        w = mp.inverse(Vm) * (x - xss)
        CV = Cy * Vm
        modos, usados = [], set()
        for k in range(n):
            if k in usados:
                continue
            lam = mp.mpc(ev[k])
            usados.add(k)
            if abs(mp.im(lam)) <= mp.mpf('1e-25') * abs(lam):
                modos.append((mp.re(lam), mp.mpf(0), [mp.re(CV[ch, k] * w[k]) for ch in range(2)], [mp.mpf(0)] * 2))
            else:
                j = min((j for j in range(n) if j not in usados), key=lambda j: abs(ev[j] - mp.conj(lam)))
                usados.add(j)
                kk = k if mp.im(lam) > 0 else j
                lam = mp.mpc(ev[kk])
                c = [CV[ch, kk] * w[kk] for ch in range(2)]
                modos.append((mp.re(lam), mp.im(lam), [2 * mp.re(ci) for ci in c], [-2 * mp.im(ci) for ci in c]))
        tau = 1 / max(abs(mp.mpc(e)) for e in ev)
        piezas.append(dict(ta=float(ta), tau=tau, modos=modos, c0=[c0[0], c0[1]]))
        # el estado en tb (continuo: C y L no saltan sin impulso)
        dt = mp.mpf(tb) - mp.mpf(ta)
        E = mp.diag([mp.exp(mp.mpc(ev[k]) * dt) for k in range(n)])
        xb = xss + Vm * E * w
        x = mp.matrix([mp.re(xb[k]) for k in range(n)])
    # la escala: el máximo de |y| (muestreo denso de cada pieza y sus bordes)
    esc = [0.0, 0.0]
    for i, p in enumerate(piezas):
        if p['ta'] < 0:
            us = np.array([0.0])
        else:
            tb = piezas[i + 1]['ta'] if i + 1 < len(piezas) else tfin
            L = tb - p['ta']
            tau = float(p['tau'])
            us = np.unique(np.concatenate([np.linspace(0.0, L, 4001),
                                           [u for u in tau * np.logspace(-4, 2, 601) if u < L], [L]]))
        for ch in range(2):
            y = eval_pieza(p, ch, us)
            esc[ch] = max(esc[ch], float(np.max(np.abs(y))))
    esc = [e if medir[ch] else 0.0 for ch, e in enumerate(esc)]
    return piezas, esc


def eval_pieza(p, ch, u):
    y = np.full_like(u, float(p['c0'][ch]), dtype=float)
    for re, im, A, B in p['modos']:
        y += np.exp(float(re) * u) * (float(A[ch]) * np.cos(float(im) * u) + float(B[ch]) * np.sin(float(im) * u))
    return y


def eval_exacta_mp(piezas, ch, t, lado=1):
    """y(t) exacta con mpmath (lado -1: el límite por la izquierda)"""
    j = 0
    for i, p in enumerate(piezas):
        if (p['ta'] <= t) if lado > 0 else (p['ta'] < t):
            j = i
    p = piezas[j]
    u = mp.mpf(t) - mp.mpf(max(p['ta'], 0.0))
    y = p['c0'][ch]
    for re, im, A, B in p['modos']:
        y += mp.exp(re * u) * (A[ch] * mp.cos(im * u) + B[ch] * mp.sin(im * u))
    return y


# ---- Los casos lineales -------------------------------------------------------

def pulso(td, ancho, V2, tfin):
    """[(ta, valor)] del pulso 0 -> V2 en td, vuelve a 0 en td + ancho"""
    cortes = [(0.0, 0.0), (td, V2)]
    if ancho > 0.0 and td + ancho < tfin:
        cortes.append((td + ancho, 0.0))
    return cortes


def cuadrada(A, f, nivel, tfin):
    """ciclo 50: nivel + A en [kT, kT + T/2), nivel - A en [kT + T/2, (k+1)T)"""
    T = 1.0 / f
    cortes, k = [], 0
    while True:
        for d, v in ((0.0, nivel + A), (0.5, nivel - A)):
            t = k * T + d * T
            if t >= tfin - 1e-12 * tfin:
                return cortes
            cortes.append((t, v))
        k += 1


def caso_rc(nom, r, C, R, V2, td, ancho, tfin, h, grupo):
    r_, C_, R_ = mp.mpf(r), mp.mpf(C), mp.mpf(R)
    g = 1 / r_ + 1 / R_
    defs = []
    for ta, e in [(-1.0, 0.0)] + pulso(td, ancho, V2, tfin):
        e = mp.mpf(e)
        defs.append((ta, M([[-g / C_]]), V([e / (r_ * C_)]), M([[1], [-g]]), V([0, e / r_])))
    piezas, esc = piezas_lineales(defs, [0.0], tfin)
    return dict(nom=nom, tipo='FL_RC', par=[r, C, R, V2, td, ancho, tfin, h], piezas=piezas, esc=esc, grupo=grupo,
                canales='V(C), I(C)')


def caso_rl(nom, A, f, R, L, tfin, h, grupo):
    R_, L_ = mp.mpf(R), mp.mpf(L)
    defs = []
    for ta, e in [(-1.0, 0.0)] + cuadrada(A, f, A, tfin):
        e = mp.mpf(e)
        defs.append((ta, M([[-R_ / L_]]), V([e / L_]), M([[1], [-R_]]), V([0, e])))
    piezas, esc = piezas_lineales(defs, [0.0], tfin)
    return dict(nom=nom, tipo='FL_RL', par=[A, f, R, L, tfin, h, 0.0, 0.0], piezas=piezas, esc=esc, grupo=grupo,
                canales='I(L), V(L)')


def caso_rlc(nom, R, L, C, V2, td, ancho, tfin, h, grupo):
    R_, L_, C_ = mp.mpf(R), mp.mpf(L), mp.mpf(C)
    defs = []
    for ta, e in [(-1.0, 0.0)] + pulso(td, ancho, V2, tfin):
        e = mp.mpf(e)
        defs.append((ta, M([[0, 1 / C_], [-1 / L_, -R_ / L_]]), V([0, e / L_]), M([[1, 0], [0, 1]]), V([0, 0])))
    piezas, esc = piezas_lineales(defs, [0.0, 0.0], tfin)
    return dict(nom=nom, tipo='FL_RLC', par=[R, L, C, V2, td, ancho, tfin, h], piezas=piezas, esc=esc, grupo=grupo,
                canales='V(C), I(L)')


def caso_integrador():
    """Modelo 8 de la biblioteca: E cuadrada 0/10 V, 1 kHz; R 10 k, C 1 uF; V(C1) y V(E1)"""
    R, C, A, f, niv, tfin = 1e4, 1e-6, 5.0, 1000.0, 5.0, 0.1
    k = 1 / (mp.mpf(R) * mp.mpf(C))
    defs = []
    for ta, e in [(-1.0, 0.0)] + cuadrada(A, f, niv, tfin):
        e = mp.mpf(e)
        defs.append((ta, M([[-k]]), V([e * k]), M([[1], [0]]), V([0, e])))
    piezas, esc = piezas_lineales(defs, [0.0], tfin, medir=(1, 0))
    return dict(nom='bib integrador', tipo='FL_BIB', par=[7, R, C, A, f, niv, tfin, 0.0], piezas=piezas, esc=esc,
                grupo=5, canales='V(C1)')


def caso_derivador():
    """Modelo 9: E cuadrada 0/10 V, 100 Hz; C 100 nF en serie, R 1 k a tierra; V(R1) = e - v_C"""
    R, C, A, f, niv, tfin = 1e3, 1e-7, 5.0, 100.0, 5.0, 0.05
    k = 1 / (mp.mpf(R) * mp.mpf(C))
    defs = []
    for ta, e in [(-1.0, 0.0)] + cuadrada(A, f, niv, tfin):
        e = mp.mpf(e)
        defs.append((ta, M([[-k]]), V([e * k]), M([[-1], [0]]), V([e, e])))
    piezas, esc = piezas_lineales(defs, [0.0], tfin, medir=(1, 0))
    return dict(nom='bib derivador', tipo='FL_BIB', par=[8, R, C, A, f, niv, tfin, 0.0], piezas=piezas, esc=esc,
                grupo=5, canales='V(R1)')


def caso_interruptor():
    """Modelo 11: E 10 V, S (abierto) cierra en 1 ms y abre en 3 ms, R1 1 k, C 1 uF con R2 1 k en paralelo"""
    E, R1, C, R2, t1, t2, tfin = 10.0, 1e3, 1e-6, 1e3, 1e-3, 3e-3, 0.01
    E_, R1_, C_, R2_ = mp.mpf(E), mp.mpf(R1), mp.mpf(C), mp.mpf(R2)
    abierto = (M([[-1 / (R2_ * C_)]]), V([0]))
    cerrado = (M([[-(1 / R1_ + 1 / R2_) / C_]]), V([E_ / (R1_ * C_)]))
    Cy, d = M([[1], [0]]), V([0, 0])
    defs = [(-1.0, *abierto, Cy, d), (0.0, *abierto, Cy, d), (t1, *cerrado, Cy, d), (t2, *abierto, Cy, d)]
    piezas, esc = piezas_lineales(defs, [0.0], tfin, medir=(1, 0))
    return dict(nom='bib interruptor', tipo='FL_BIB', par=[10, E, R1, C, R2, t1, t2, tfin], piezas=piezas, esc=esc,
                grupo=5, canales='V(C1)')


def caso_esc(nom, nsec, r, c0, f, tfin, h, grupo):
    """La escalera del juez (rev3/juez_flanco/test_juez.inc): E cuadrada 0/5 V a f
    (ciclo 50) con r interna; C0 a la entrada; nsec tramos de R = 1 k y C = 100 nF;
    carga de 10 k. Canales: V(nodo 1) e I(C0). La uniformidad: 20 flancos en
    10 ms, con el criterio del banco en todos (no solo en el primero). La
    exacta, como las demás lineales (la de exacta_escalera.py del juez, por
    matriz exponencial, da lo mismo)"""
    R, CS, RL = mp.mpf(1000), mp.mpf('100e-9'), mp.mpf(10000)
    n = nsec + 1
    r_, c0_ = mp.mpf(r), mp.mpf(c0)
    G = mp.zeros(n, n)
    G[0, 0] += 1 / r_
    for k in range(nsec):
        G[k, k] += 1 / R
        G[k + 1, k + 1] += 1 / R
        G[k, k + 1] -= 1 / R
        G[k + 1, k] -= 1 / R
    G[n - 1, n - 1] += 1 / RL
    Cd = [c0_] + [CS] * nsec
    A = mp.matrix(n, n)
    for i in range(n):
        for j in range(n):
            A[i, j] = -G[i, j] / Cd[i]
    Cy = mp.zeros(2, n)
    Cy[0, 0] = 1
    Cy[1, 0] = -(1 / r_ + 1 / R)
    Cy[1, 1] = 1 / R
    defs = []
    for ta, e in [(-1.0, 0.0)] + cuadrada(2.5, f, 2.5, tfin):
        e = mp.mpf(e)
        b = mp.zeros(n, 1)
        b[0] = e / (r_ * c0_)
        defs.append((ta, A, b, Cy, V([0, e / r_])))
    piezas, esc = piezas_lineales(defs, [0.0] * n, tfin)
    return dict(nom=nom, tipo='FL_ESC', par=[nsec, r, c0, f, tfin, h], piezas=piezas, esc=esc, grupo=grupo,
                canales='V(1), I(C0)')


def caso_bib_huella(nom, indice):
    """Un modelo no lineal y no rígido de la biblioteca, sin exacta: solo la
    huella (bit a bit) y el costo. 36: derivador con AO (cuadrada, tau/h 40);
    38: astable con AO (los cambios de saturación, tau/h 100)"""
    return dict(nom=nom, tipo='FL_BIB', par=[indice], piezas=[], esc=[0.0, 0.0], grupo=5, canales='')


# =============================================================================
# No lineales: el oráculo, tabla densa
# =============================================================================

NET_DIODO = """# Hakaru CES: banco del flanco, el diodo con un C pequeño (CESNET 3)
cesnet 3
tran 0.002 5e-06
V V0 0 2 0 PUL 0 5 0.0002 0.0006 0
R R1 2 3 100
D D2 3 4 2.52e-09 1.752 0.568
C C3 4 0 1e-09 0
R R4 4 0 10000
# canal 1 V 4
# canal 2 I C3
"""

# La revisión del arreglo (rev4/rev_flanco): el mismo diodo con una cuadrada
# de 1 kHz, 2 k y 100 nF (tau 167 us al conducir, 8,4h con el paso automático
# de 20 us: NO rígido; la presonda no puede costar ni cambiar nada)
NET_DIODO_NR = """# Hakaru CES: banco del flanco, el diodo que no es rígido (CESNET 3)
cesnet 3
tran 0.01 2e-05
V V0 0 2 0 SQR 2.5 1000 50 2.5
R R1 2 3 2000
D D2 3 4 2.52e-09 1.752 0.568
C C3 4 0 1e-07 0
R R4 4 0 10000
# canal 1 V 4
# canal 2 I C3
"""

# ... y con 100 k de carga, una cuadrada de 10 kHz y h = 1/140000 s a mano: al
# conducir, tau 0,1 us = h/70, z = (h/16)/tau = 4,5 en el primer subpaso del
# tramo. La segunda etapa de SDIRK2 sobrecargaba el C, el diodo se cortaba y
# el rebote se quedaba (0,24 de la escala en te + h/2, 4e-2 tras cada flanco)
NET_DIODO_Z = """# Hakaru CES: banco del flanco, el diodo con z = 4,5 (CESNET 3)
cesnet 3
tran 0.0005 7.142857142857143e-06
V V0 0 2 0 SQR 2.5 10000 50 2.5
R R1 2 3 100
D D2 3 4 2.52e-09 1.752 0.568
C C3 4 0 1e-09 0
R R4 4 0 100000
# canal 1 V 4
# canal 2 I C3
"""


def caso_imp(nom, r, C, R, V2, td, ancho, tfin, h, grupo):
    """La y+ con impulso: E ideal de pulso con un C1 de 1 uF encima (algebraico:
    salta con un impulso en cada flanco), r = 10 ohm hasta un C de 150 nF con
    R = 1 k. C2 ve la E por r: la exacta de V(C2) e I(C2) es la del RC con r
    interna (caso_rc); la y+ de V(C2) es la de te- (no salta)"""
    c = caso_rc(nom, r, C, R, V2, td, ancho, tfin, h, grupo)
    c['tipo'] = 'FL_IMP'
    c['canales'] = 'V(C2), I(C2)'
    return c


def net_204():
    """caso_204 del oráculo no lineal ([67]): BJT saturado con C17 en el colector.
    Los canales: V(C17) (el nodo 10) e I(C17)"""
    with open(os.path.join(AQUI, 'c204', 'caso_204.net'), encoding='ascii') as f:
        lineas = [lin.rstrip('\n') for lin in f]
    out = []
    for lin in lineas:
        if lin.startswith('# canal'):
            continue
        if lin.startswith('# motor:'):
            out.append('# (caso_204 del oraculo no lineal [67]; ' + lin[2:] + ')')
            continue
        out.append(lin)
    out += ['# canal 1 V 10', '# canal 2 I C17']
    return '\n'.join(out) + '\n'


def cubica(ts, ys, t):
    """Lagrange de 4 puntos (o los que haya)"""
    n = len(ts)
    y = 0.0
    for i in range(n):
        c = 1.0
        for m in range(n):
            if m != i:
                c *= (t - ts[m]) / (ts[i] - ts[m])
        y += c * ys[i]
    return y


def interp_tabla(T, Y, t):
    """la cúbica de 4 puntos de la tabla (T ascendente) en t"""
    j = int(np.searchsorted(T, t, side='right')) - 1
    j = min(max(j, 0), len(T) - 2)
    a = min(max(j - 1, 0), max(len(T) - 4, 0))
    b = min(a + 4, len(T))
    return cubica(T[a:b], Y[a:b], t)


def referencia_nl(nom, texto, par, grupo, canales):
    red = onl.Red(texto)
    tr = onl.Transitorio(red)
    s = tr.sis
    tfin = red.tfin
    tol_ev = 1e-9 * red.h
    bordes = set()
    for e in s.fuentes:
        bordes.update(e.forma.eventos(tfin))
    bordes = sorted(t for t in bordes if tol_ev < t < tfin - tol_ev)
    cortes = [0.0]
    for t in bordes:
        if t - cortes[-1] > tol_ev:
            cortes.append(t)
    cortes.append(tfin)
    x = np.array([c.v[1] for c in s.caps] + [l.v[1] for l in s.inds], dtype=float)
    atol = np.array([1e-13 * tr.escV] * len(s.caps) + [1e-13 * tr.escI] * len(s.inds))
    canal_orden = sorted(red.canales)

    def canales_en(z, ctx):
        return [float(tr.canal(tipo, cual, z, ctx)) for _, tipo, cual in canal_orden]

    segs = []
    for iseg in range(len(cortes) - 1):
        ta, tb = cortes[iseg], cortes[iseg + 1]
        tm = 0.5 * (ta + tb)
        seg = {'f': {e.nom: e.forma.pieza(tm)[0] for e in s.fuentes},
               's': {e.nom: tr._cerrado(e, tm) for e in red.el if e.tipo == 'S'}}
        if iseg == 0:
            tr.arranque(seg, x)
        else:
            tr.algebraico(seg, ta, x)
        z0, ctx0 = tr.z.copy(), tr.ctx_ult
        fun, jac = tr.f(seg)
        J = jac(ta, x)
        lam = np.linalg.eigvals(J)
        tau = 1.0 / float(np.max(np.abs(lam)))
        tr.z, tr.ctx_ult = z0.copy(), ctx0
        sol = solve_ivp(fun, (ta, tb), x, method='Radau', rtol=RTOL_NL, atol=atol, jac=jac, dense_output=True)
        if sol.status != 0:
            raise onl.ErrorRed(f'{nom}: Radau no llega de {ta!r} a {tb!r}: {sol.message}')
        segs.append(dict(ta=ta, tb=tb, tau=tau, seg=seg, sol=sol, x0=x.copy(), z0=z0, ctx0=ctx0, lam=lam))
        x = np.array(sol.y[:, -1], dtype=float)

    def evalua(sg, t, zini, ctxini):
        tr.z, tr.ctx_ult = zini.copy(), ctxini
        xt = sg['x0'] if t <= sg['ta'] else sg['sol'].sol(t)
        z, ctx = tr.algebraico(sg['seg'], t, xt)
        return canales_en(z, ctx), z.copy(), ctx

    # la malla inicial: geométrica desde tau/1000 tras el flanco y uniforme
    for sg in segs:
        ta, tb, tau = sg['ta'], sg['tb'], sg['tau']
        L = tb - ta
        us = set(np.linspace(0.0, L, 97).tolist())
        us.update(u for u in (tau * 10.0 ** (k / 8.0) for k in range(-24, 40)) if u < L)
        T = sorted(ta + u for u in us)
        T[-1] = tb
        pts = []
        z, ctx = sg['z0'], sg['ctx0']
        for t in T:
            y, z, ctx = evalua(sg, t, z, ctx)
            pts.append([t, y, z, ctx])
        sg['pts'] = pts
    esc = [max(abs(p[1][ch]) for sg in segs for p in sg['pts']) for ch in range(2)]

    # refinado: el punto medio de cada intervalo, frente a la cúbica
    for sg in segs:
        pend = list(range(len(sg['pts']) - 1))
        for _ in range(40):
            if not pend:
                break
            pts = sg['pts']
            T = np.array([p[0] for p in pts])
            Ys = [np.array([p[1][ch] for p in pts]) for ch in range(2)]
            nuevos = []
            for j in pend:
                ta_, tb_ = pts[j][0], pts[j + 1][0]
                if tb_ - ta_ < 1e-13 * tfin:
                    continue
                tm = 0.5 * (ta_ + tb_)
                y, z, ctx = evalua(sg, tm, pts[j][2], pts[j][3])
                malo = any(abs(interp_tabla(T, Ys[ch], tm) - y[ch]) > TOL_TABLA * esc[ch] for ch in range(2) if esc[ch] > 0)
                if malo:
                    nuevos.append([tm, y, z, ctx])
            if not nuevos:
                break
            todos = sorted(pts + nuevos, key=lambda p: p[0])
            tn = set(p[0] for p in nuevos)
            pend = sorted(set(k for k, p in enumerate(todos) if p[0] in tn for k in (k - 1, k) if 0 <= k < len(todos) - 1))
            sg['pts'] = todos
    # comprobación final: todos los puntos medios
    peor_tab = 0.0
    for sg in segs:
        pts = sg['pts']
        T = np.array([p[0] for p in pts])
        Ys = [np.array([p[1][ch] for p in pts]) for ch in range(2)]
        for j in range(len(pts) - 1):
            tm = 0.5 * (pts[j][0] + pts[j + 1][0])
            y, _, _ = evalua(sg, tm, pts[j][2], pts[j][3])
            for ch in range(2):
                if esc[ch] > 0:
                    peor_tab = max(peor_tab, abs(interp_tabla(T, Ys[ch], tm) - y[ch]) / esc[ch])
    # comprobación de la salida densa: unos instantes integrados aparte (rtol 1e-13,
    # el paso cae justo en el instante)
    peor_dens = 0.0
    rng = np.random.default_rng(20260926)
    for sg in segs[:3]:
        for _ in range(3):
            t = sg['ta'] + (sg['tb'] - sg['ta']) * float(rng.uniform(0.001, 0.2)) ** 2
            tr.z, tr.ctx_ult = sg['z0'].copy(), sg['ctx0']
            fun, jac = tr.f(sg['seg'])
            s2 = solve_ivp(fun, (sg['ta'], t), sg['x0'], method='Radau', rtol=1e-13, atol=atol * 0.1, jac=jac)
            xd = sg['sol'].sol(t)
            peor_dens = max(peor_dens, float(np.max(np.abs(s2.y[:, -1] - xd))) / tr.escV)
    piezas = []
    for sg in segs:
        piezas.append(dict(ta=sg['ta'], tau=sg['tau'], puntos=[(p[0], p[1][0], p[1][1]) for p in sg['pts']]))
    print(f'    {nom}: {len(segs)} piezas, {sum(len(sg["pts"]) for sg in segs)} puntos; cúbica frente al punto medio '
          f'{peor_tab:.2e} de la escala; salida densa frente a rtol 1e-13: {peor_dens:.2e} (de escV)')
    for sg in segs:
        print(f'      pieza en {sg["ta"]:.6g}: tau {sg["tau"]:.4g}, y+ = {sg["pts"][0][1][0]:.12g}, {sg["pts"][0][1][1]:.12g}')
    return dict(nom=nom, tipo='FL_NET', par=par, piezas=piezas, esc=esc, grupo=grupo, net=texto, canales=canales)


# =============================================================================
# Salida C
# =============================================================================

def g17(x):
    x = float(x)
    if x == 0.0:
        return '0.0'
    s = '%.17g' % x
    if 'e' not in s and '.' not in s and 'n' not in s:
        s += '.0'
    return s


def ident(nom):
    return ''.join(c if c.isalnum() else '_' for c in nom)


def c_caso(c, i):
    L = []
    idn = f'FLX{i:02d}'
    if c['tipo'] == 'FL_NET':
        filas = []
        for p in c['piezas']:
            for (t, y0, y1) in p['puntos']:
                filas.append(f'    {{ {g17(t)}, {g17(y0)}, {g17(y1)} }},')
        L.append(f'static const double {idn}_pt[][3] = {{   /* {c["nom"]}: t, y1, y2 */')
        L += filas
        L.append('};')
        L.append(f'static const FlPieza {idn}_p[] = {{')
        i0 = 0
        for p in c['piezas']:
            n = len(p['puntos'])
            L.append(f'    {{ {g17(p["ta"])}, {g17(p["tau"])}, -1, {{0.0, 0.0}}, {{0.0, 0.0}}, {{0.0, 0.0}}, '
                     f'{{{{0.0, 0.0}}, {{0.0, 0.0}}}}, {{{{0.0, 0.0}}, {{0.0, 0.0}}}}, {i0}, {n} }},')
            i0 += n
        L.append('};')
        net = c['net'].rstrip('\n').split('\n')
        L.append(f'static const char {idn}_net[] =')
        for lin in net:
            L.append('    "' + lin.replace('\\', '\\\\').replace('"', '\\"') + '\\n"')
        L[-1] += ';'
    else:
        L.append(f'static const FlPieza {idn}_p[] = {{   /* {c["nom"]} */')
        for p in c['piezas']:
            mods = p['modos']
            nmx = max(2, len(mods))                  # FL_MMAX en el .inc (las escaleras: hasta 15)
            re = ', '.join([g17(m[0]) for m in mods] + ['0.0'] * (nmx - len(mods)))
            im = ', '.join([g17(m[1]) for m in mods] + ['0.0'] * (nmx - len(mods)))
            A = [', '.join([g17(m[2][ch]) for m in mods] + ['0.0'] * (nmx - len(mods))) for ch in range(2)]
            B = [', '.join([g17(m[3][ch]) for m in mods] + ['0.0'] * (nmx - len(mods))) for ch in range(2)]
            L.append(f'    {{ {g17(p["ta"])}, {g17(p["tau"])}, {len(mods)}, {{{re}}}, {{{im}}}, '
                     f'{{{g17(p["c0"][0])}, {g17(p["c0"][1])}}}, {{{{{A[0]}}}, {{{A[1]}}}}}, '
                     f'{{{{{B[0]}}}, {{{B[1]}}}}}, 0, 0 }},')
        L.append('};')
    return L


def escribe(casos):
    L = [MARCA_INI + ': las escribe herramientas/banco_flanco.py (no se tocan a mano) == */']
    emitidos, idns = {}, []
    for i, c in enumerate(casos):
        if not c['piezas']:                        # sin exacta (solo la huella)
            idns.append(None)
            continue
        if id(c['piezas']) in emitidos:            # la misma exacta (el BJT con h/2 y h/4)
            idns.append(emitidos[id(c['piezas'])])
            continue
        L += c_caso(c, i)
        emitidos[id(c['piezas'])] = f'FLX{i:02d}'
        idns.append(f'FLX{i:02d}')
    L.append(f'#define FL_NCASOS {len(casos)}')
    L.append('static const FlCaso FL_CASOS[FL_NCASOS] = {')
    for i, c in enumerate(casos):
        idn = idns[i]
        par = ', '.join(g17(v) for v in (list(c['par']) + [0.0] * 8)[:8])
        pt = f'{idn}_pt' if c['tipo'] == 'FL_NET' else 'NULL'
        net = f'{idn}_net' if c['tipo'] == 'FL_NET' else 'NULL'
        pp = f'{idn}_p' if idn else 'NULL'
        L.append(f'    {{ "{c["nom"]}", {c["grupo"]}, {c["tipo"]}, {{{par}}}, {net}, "{c["canales"]}", '
                 f'{{ {len(c["piezas"])}, {pp}, {pt}, {{{g17(c["esc"][0])}, {g17(c["esc"][1])}}} }} }},')
    L.append('};')
    L.append(MARCA_FIN)
    with open(INC, encoding='utf-8') as f:
        texto = f.read()
    a = texto.index(MARCA_INI)
    b = texto.index(MARCA_FIN) + len(MARCA_FIN)
    texto = texto[:a] + '\n'.join(L) + texto[b:]
    with open(INC, 'w', encoding='utf-8', newline='\n') as f:
        f.write(texto)


def resumen(c):
    print(f'  {c["nom"]}: {len(c["piezas"])} piezas, escala {c["esc"][0]:.6g} / {c["esc"][1]:.6g}')
    if c['tipo'] != 'FL_NET':
        for p in c['piezas']:
            if p['ta'] >= 0 and len(c['piezas']) <= 12:
                print(f'      pieza en {p["ta"]:.6g}: tau {float(p["tau"]):.4g}, y+ = '
                      f'{float(eval_exacta_mp(c["piezas"], 0, p["ta"])):.12g}, {float(eval_exacta_mp(c["piezas"], 1, p["ta"])):.12g}')


def main():
    h = 5e-6
    C, R = 150e-9, 1e3
    casos = [caso_rc('minimo', 10.0, C, R, 5.0, 2e-3, 4e-3, 10e-3, 0.0, 1)]
    r_barr = []
    for q, nq in ((0.01, '0,01'), (0.1, '0,1'), (0.3, '0,3'), (1.0, '1'), (3.0, '3'), (10.0, '10')):
        tau = q * h
        r = 1.0 / (C / tau - 1.0 / R)
        r_barr.append(r)
        casos.append(caso_rc(f'RC tau/h={nq}', r, C, R, 5.0, 2e-3, 4e-3, 10e-3, h, 2))
    casos.append(caso_rl('RL cuadrada', 5.0, 1000.0, 100.0, 50e-6, 5e-3, 0.0, 3))
    casos.append(caso_rlc('RLC rigido', 100.0, 25e-6, 1e-6, 10.0, 0.5e-3, 1e-3, 2e-3, 0.0, 4))
    casos.append(caso_rlc('RLC no rigido', 28.0, 10e-3, 4e-6, 10.0, 1e-3, 4e-3, 10e-3, 0.0, 4))
    casos += [caso_integrador(), caso_derivador(), caso_interruptor()]
    for c in casos:
        resumen(c)
    casos += [caso_bib_huella('bib deriv. AO', 36), caso_bib_huella('bib astable AO', 38)]
    print('  no lineales (el oráculo, Radau rtol %.0e):' % RTOL_NL)
    casos.append(referencia_nl('diodo C', NET_DIODO, [0.0], 6, 'V(C3), I(C3)'))
    c204 = referencia_nl('BJT sat 204', net_204(), [0.0], 6, 'V(C17), I(C17)')
    casos.append(c204)
    # h/2 y h/4 del paso automático de hoy (23 us, agrandado por el tope de 2 s),
    # fijos: un arreglo que cambie el modelo de costo del arranque mueve el automático
    for hh, nom in ((23e-6 / 2.0, 'BJT sat 204 h/2'), (23e-6 / 4.0, 'BJT sat 204 h/4')):
        c = dict(c204)
        c['nom'], c['par'] = nom, [hh]
        casos.append(c)
    for c in casos[-5:]:
        print(f'  {c["nom"]}: escala {c["esc"][0]:.6g} / {c["esc"][1]:.6g}')
    # la uniformidad (el juez): dos escaleras con un modo rápido a la entrada y
    # 20 flancos; el paso automático (2,5 us), tau/h 0,06 y 0,55
    for c in (caso_esc('esc14_1k_r1', 14, 1.0, 150e-9, 1e3, 10e-3, 0.0, 7),
              caso_esc('esc8_1k', 8, 10.0, 150e-9, 1e3, 10e-3, 0.0, 7)):
        casos.append(c)
        resumen(c)
        print(f'      tau {float(c["piezas"][1]["tau"]):.4g}; modos {len(c["piezas"][1]["modos"])}')
    # la revisión del arreglo (rev4/rev_flanco), al final (FL_HOY va por orden):
    # la banda de 5h a 7,5h (no rígida: el arranque de siempre, bit a bit), la
    # y+ con impulso, el diodo que no es rígido y el de z = 4,5
    tau = 6.0 * h
    c = caso_rc('RC tau/h=6', 1.0 / (C / tau - 1.0 / R), C, R, 5.0, 2e-3, 4e-3, 10e-3, h, 2)
    casos.append(c)
    resumen(c)
    c = caso_imp('imp C', 10.0, C, R, 5.0, 2e-3, 4e-3, 10e-3, h, 8)
    casos.append(c)
    resumen(c)
    casos.append(referencia_nl('diodo NR', NET_DIODO_NR, [0.0], 6, 'V(C3), I(C3)'))
    casos.append(referencia_nl('diodo C z4.5', NET_DIODO_Z, [1.0 / 140e3], 6, 'V(C3), I(C3)'))
    for c in casos[-2:]:
        print(f'  {c["nom"]}: escala {c["esc"][0]:.6g} / {c["esc"][1]:.6g}')
    escribe(casos)
    print(f'escrito: {os.path.normpath(INC)} ({len(casos)} casos)')


if __name__ == '__main__':
    main()
