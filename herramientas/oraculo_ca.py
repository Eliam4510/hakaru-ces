#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
ORÁCULO DE CA  --  Hakaru CES / herramientas / oraculo_ca.py
============================================================

Referencia independiente para el análisis de fasores de Hakaru CES (ca.c): un
MNA complejo propio, escrito aparte y resuelto con numpy.linalg.solve. El
arnés (sección [29b]) exporta redes de CA al azar a out/oraculo_ca:

    caso_NNN.net   la red (formato de abajo)
    caso_NNN.ces   lo que da el programa: fasores de nodo, corrientes,
                   lecturas y puntos del Bode

Este programa resuelve cada red por su cuenta y compara: fasores iguales a
1e-12 de la escala y puntos del Bode a 1e-10.

USO
---
    python oraculo_ca.py [carpeta]          (por omisión, ../out/oraculo_ca)
    python oraculo_ca.py --pruebas          casos cerrados del propio oráculo

FORMATO DE LA RED (CESNET 2)
----------------------------
Una línea por elemento; '#' empieza un comentario. Nodos: enteros >= 0; el 0
es la referencia. Fasores de PICO con referencia COSENO: x(t) = Re(X·e^{jωt}).
Todas las corrientes van de a hacia b POR DENTRO del elemento.

    cesnet 2
    ca <f>                                 la frecuencia del análisis (Hz)
    R  <nom> <a> <b> <ohm>
    C  <nom> <a> <b> <F>
    L  <nom> <a> <b> <H>
    Z  <nom> <a> <b> <R> <X> <f0>          Z = R + jX a f0; a otra f la X
                                           escala como una L (X > 0: X·f/f0) o
                                           como una C (X < 0: X·f0/f)
    V  <nom> <neg> <pos> <r> <pico> <fase> V(pos) - V(neg) = E - r·I; I sale por pos
    I  <nom> <a> <b> <pico> <fase>         la J va de a hacia b por dentro
    AM <nom> <a> <b> <r>                   r = 0: ideal (un corto)
    VM <nom> <a> <b> <r>                   r = 0: ideal (abierto)
    S  <nom> <a> <b> <cerrado>             en su estado final
    W  <nom> <a> <b>                       vatímetro: bobina de corriente a->b (un
                                           corto); la de tensión, de b al nodo 0
    bode <fuente> <nodo> <f>               H = V(nodo)/E (o /J) a la f, con las
                                           demás fuentes apagadas (E en su r, J
                                           abiertas)

SALIDA DEL PROGRAMA (.ces)
----------------------------
    V <nodo> <re> <im>        fasor de nodo (pico, coseno)
    I <nom> <re> <im>         corriente del elemento, de a hacia b por dentro
    M <nom> <valor>           lectura: V y A, el eficaz; W, la P (con eficaces)
    H <f> <re> <im>           un punto del Bode
"""
import math
import os
import sys

import numpy as np


def lee_red(texto):
    red = {'f': None, 'el': [], 'bode': []}
    for linea in texto.splitlines():
        linea = linea.split('#', 1)[0].strip()
        if not linea:
            continue
        p = linea.split()
        k = p[0].upper()
        if k == 'CESNET':
            continue
        if k == 'CA':
            red['f'] = float(p[1])
        elif k == 'BODE':
            red['bode'].append((p[1], int(p[2]), float(p[3])))
        else:
            red['el'].append((k, p[1], int(p[2]), int(p[3]), [float(x) for x in p[4:]]))
    return red


def resuelve(red, f, activas=None):
    """MNA complejo a la frecuencia f. activas: los nombres de las fuentes que
    excitan (None = todas); las demás se apagan. Devuelve (phi, I) con phi
    por nodo e I por nombre."""
    w = 2.0 * math.pi * f
    nodos = sorted({e[2] for e in red['el']} | {e[3] for e in red['el']} | {0})
    idx = {n: i - 1 for i, n in enumerate(nodos)}          # el 0 fuera
    nn = len(nodos) - 1
    # las ramas con corriente propia: V, AM ideal, S cerrado, W
    ramas = [e for e in red['el'] if e[0] in ('V', 'W') or (e[0] == 'AM' and e[4][0] == 0.0) or (e[0] == 'S' and e[4][0] != 0.0)]
    rid = {e[1]: nn + i for i, e in enumerate(ramas)}
    n = nn + len(ramas)
    A = np.zeros((n, n), dtype=complex)
    b = np.zeros(n, dtype=complex)

    def y_entre(a, c, y):
        ia, ic = idx[a], idx[c]
        if ia >= 0:
            A[ia, ia] += y
        if ic >= 0:
            A[ic, ic] += y
        if ia >= 0 and ic >= 0:
            A[ia, ic] -= y
            A[ic, ia] -= y

    def z_de(e):
        tipo, nom, a, c, v = e
        if tipo == 'R':
            return complex(v[0], 0.0)
        if tipo == 'L':
            return complex(0.0, w * v[0])
        if tipo == 'Z':
            R, X, f0 = v
            if X > 0:
                X = X * f / f0
            elif X < 0:
                X = X * f0 / f
            return complex(R, X)
        return None

    for e in red['el']:
        tipo, nom, a, c, v = e
        ia, ic = idx[a], idx[c]
        if tipo in ('R', 'L', 'Z'):
            y_entre(a, c, 1.0 / z_de(e))
        elif tipo == 'C':
            y_entre(a, c, complex(0.0, w * v[0]))
        elif tipo in ('AM', 'VM') and v[0] > 0.0:
            y_entre(a, c, 1.0 / v[0])
        elif tipo == 'I':
            J = v[0] * complex(math.cos(math.radians(v[1])), math.sin(math.radians(v[1])))
            if activas is not None and nom not in activas:
                J = 0.0
            # sale por b hacia el circuito exterior: entra en el nodo b
            if ia >= 0:
                b[ia] -= J
            if ic >= 0:
                b[ic] += J
        if nom in rid:
            k = rid[nom]
            # la corriente de rama entra al elemento por a (sale del nodo a) y
            # sale por b (entra en el nodo b)
            if ia >= 0:
                A[ia, k] += 1.0
            if ic >= 0:
                A[ic, k] -= 1.0
            # su ecuación: V(b) - V(a) = E - r·I  (V: a = neg, b = pos), o 0 (corto)
            if ic >= 0:
                A[k, ic] += 1.0
            if ia >= 0:
                A[k, ia] -= 1.0
            if tipo == 'V':
                r, pico, fase = v
                E = pico * complex(math.cos(math.radians(fase)), math.sin(math.radians(fase)))
                if activas is not None and nom not in activas:
                    E = 0.0
                A[k, k] += r
                b[k] = E
    # cada fila de nodo: lo que sale del nodo por los elementos = lo que le
    # inyectan las J
    x = np.linalg.solve(A, b)
    phi = {0: 0.0 + 0.0j}
    for nd in nodos:
        if nd != 0:
            phi[nd] = x[idx[nd]]
    I = {}
    for e in red['el']:
        tipo, nom, a, c, v = e
        u = phi[a] - phi[c]
        if nom in rid:
            I[nom] = x[rid[nom]]
        elif tipo in ('R', 'L', 'Z'):
            I[nom] = u / z_de(e)
        elif tipo == 'C':
            I[nom] = u * complex(0.0, w * v[0])
        elif tipo in ('AM', 'VM'):
            I[nom] = u / v[0] if v[0] > 0.0 else 0.0
        elif tipo == 'I':
            J = v[0] * complex(math.cos(math.radians(v[1])), math.sin(math.radians(v[1])))
            I[nom] = 0.0 if (activas is not None and nom not in activas) else J
        elif tipo == 'S':
            I[nom] = 0.0
    return phi, I


def lecturas(red, phi, I):
    M = {}
    for tipo, nom, a, c, v in red['el']:
        if tipo == 'VM':
            M[nom] = abs(phi[a] - phi[c]) / math.sqrt(2.0)
        elif tipo == 'AM':
            M[nom] = abs(I[nom]) / math.sqrt(2.0)
        elif tipo == 'W':
            M[nom] = 0.5 * (phi[c] * np.conj(I[nom])).real
    return M


def lee_ces(texto):
    r = {'V': {}, 'I': {}, 'M': {}, 'H': []}
    for linea in texto.splitlines():
        p = linea.split()
        if not p:
            continue
        if p[0] == 'V':
            r['V'][int(p[1])] = complex(float(p[2]), float(p[3]))
        elif p[0] == 'I':
            r['I'][p[1]] = complex(float(p[2]), float(p[3]))
        elif p[0] == 'M':
            r['M'][p[1]] = float(p[2])
        elif p[0] == 'H':
            r['H'].append((float(p[1]), complex(float(p[2]), float(p[3]))))
    return r


def compara(red, ces):
    """Devuelve (peor error relativo de fasores y lecturas, peor del Bode, textos)"""
    phi, I = resuelve(red, red['f'])
    M = lecturas(red, phi, I)
    # escalas físicas: una red casi muerta (fuente en corto, nodo aislado) da
    # fasores de 1e-16 y el error relativo a ellos no significa nada
    amp_v = [e[4][1] for e in red['el'] if e[0] == 'V']
    amp_i = [e[4][0] for e in red['el'] if e[0] == 'I']
    ev = max([abs(z) for z in phi.values()] + amp_v + [1e-300])
    ei = max([abs(z) for z in I.values()] + amp_i + [ev / 1e3])
    peor, notas = 0.0, []
    for nd, z in ces['V'].items():
        e = abs(z - phi.get(nd, 0.0)) / ev
        if e > peor:
            peor = e
        if e > 1e-12:
            notas.append(f'V({nd}): {z} frente a {phi.get(nd)}')
    for nom, z in ces['I'].items():
        e = abs(z - I[nom]) / ei
        if e > peor:
            peor = e
        if e > 1e-12:
            notas.append(f'I({nom}): {z} frente a {I[nom]}')
    for nom, m in ces['M'].items():
        esc = max(ev * ei, 1e-300) if nom.startswith('W') else (ev if nom.startswith('M') else ei)
        e = abs(m - M[nom]) / esc
        if e > peor:
            peor = e
        if e > 1e-12:
            notas.append(f'lectura de {nom}: {m} frente a {M[nom]}')
    peor_h = 0.0
    puntos = []
    for (fuente, nodo, _), (f, H) in zip(red['bode'], ces['H']):
        ph, Ih = resuelve(red, f, activas={fuente})
        e_src = [e for e in red['el'] if e[1] == fuente][0]
        amp = e_src[4][1] if e_src[0] == 'V' else e_src[4][0]
        fase = e_src[4][2] if e_src[0] == 'V' else e_src[4][1]
        X = amp * complex(math.cos(math.radians(fase)), math.sin(math.radians(fase)))
        # piso de |H|: 1e-4 (-80 dB) desde una E; 0,1 Ω desde una J
        piso = 1e-4 if e_src[0] == 'V' else 1e-1
        puntos.append((f, H, ph[nodo] / X, piso))
    # un punto donde |H| es casi nula se mide contra el mayor del barrido
    hmax = max([abs(p[2]) for p in puntos] + [1e-300])
    for f, H, Hr, piso in puntos:
        e = abs(H - Hr) / max(abs(Hr), 1e-6 * hmax, piso)
        if e > peor_h:
            peor_h = e
        if e > 1e-10:
            notas.append(f'H({f:.6g} Hz): {H} frente a {Hr}')
    return peor, peor_h, notas


def pruebas():
    # RC en su corte: E = 10 V eficaz = 14,142 de pico
    f = 1.0 / (2 * math.pi * 1e3 * 1e-6)
    red = lee_red(f'ca {f}\nV E1 0 1 0 {10*math.sqrt(2)} 0\nR R1 1 2 1000\nC C1 2 0 1e-6\n')
    phi, I = resuelve(red, f)
    vc = phi[2] / math.sqrt(2)
    assert abs(vc - complex(5, -5)) < 1e-12, vc
    assert abs(I['E1'] / math.sqrt(2) - complex(0.005, 0.005)) < 1e-15, I['E1']
    # divisor con J: J = 1 mA en 1 k || 1 k -> 0,5 V
    red = lee_red('ca 50\nI J1 0 1 1e-3 0\nR R1 1 0 1000\nR R2 1 0 1000\n')
    phi, I = resuelve(red, 50.0)
    assert abs(phi[1] - 0.5) < 1e-15, phi[1]
    # vatímetro sobre una R de 10 con 10 V eficaces: 10 W
    red = lee_red(f'ca 60\nV E1 0 1 0 {10*math.sqrt(2)} 0\nW W1 1 2\nR R1 2 0 10\n')
    phi, I = resuelve(red, 60.0)
    M = lecturas(red, phi, I)
    assert abs(M['W1'] - 10.0) < 1e-12, M
    print('pruebas del oráculo: bien')


def main():
    if '--pruebas' in sys.argv:
        pruebas()
        return 0
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    carpeta = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'out', 'oraculo_ca')
    casos = sorted(x for x in os.listdir(carpeta) if x.endswith('.net'))
    iguales = malos = 0
    peor_todo = peor_bode = 0.0
    for nombre in casos:
        red = lee_red(open(os.path.join(carpeta, nombre), encoding='ascii').read())
        ruta_ces = os.path.join(carpeta, nombre[:-4] + '.ces')
        if not os.path.exists(ruta_ces):
            continue
        ces = lee_ces(open(ruta_ces, encoding='ascii').read())
        try:
            peor, ph, notas = compara(red, ces)
        except np.linalg.LinAlgError as ex:
            print(f'{nombre}: el oráculo no la resuelve ({ex})')
            malos += 1
            continue
        peor_todo = max(peor_todo, peor)
        peor_bode = max(peor_bode, ph)
        if notas:
            malos += 1
            print(f'{nombre}: DISTINTA (fasores {peor:.2e}, Bode {ph:.2e})')
            for t in notas[:6]:
                print('   ', t)
        else:
            iguales += 1
    print(f'{iguales} de {iguales + malos} iguales; peor error de fasores {peor_todo:.2e} '
          f'(tolerancia 1e-12), del Bode {peor_bode:.2e} (1e-10)')
    return 0 if malos == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
