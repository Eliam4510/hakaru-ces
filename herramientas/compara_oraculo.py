#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
Compara el emulador de Hakaru CES con el oráculo independiente.

El arnés (sección [29]) exporta a out/oraculo, por cada red al azar:
    caso_NNN.net      la netlist (formato de oraculo_sim.py), con el estado
                      inicial de C y L explícito (el del emulador) y los dos
                      canales en comentarios "# canal k V n" / "# canal k I nombre"
    caso_NNN.ces      t,y1,y2 del emulador con su paso automático h, en t = k*tfin/400
    caso_NNN_h2.ces   lo mismo con h/2

Para cada caso, el oráculo integra el mismo circuito con rtol = 1e-10 y se mide
el error máximo de cada canal relativo a su escala (el máximo de |y| del
oráculo). Se exige:
    - error <= 1e-3 de la escala con el paso automático;
    - con h/2, el error baja a la cuarta parte (cociente en [3, 5]) cuando el
      error con h está por encima del ruido del oráculo (1e-7).
Los casos que el oráculo rechaza (impulsos en un evento, condiciones iniciales
incompatibles: el emulador los resuelve con un salto, el oráculo no) se cuentan
aparte con su motivo.

USO
    python compara_oraculo.py [carpeta]        (por omisión, ../out/oraculo)
Código de salida: 0 si todo pasa; 1 si algún caso falla.
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oraculo_sim as orc  # noqa: E402

TOL = 1e-3
RUIDO = 1e-7


def lee_ces(ruta):
    t, y1, y2 = [], [], []
    with open(ruta, encoding='ascii') as f:
        next(f)
        for lin in f:
            a, b, c = lin.split(',')
            t.append(float(a)); y1.append(float(b)); y2.append(float(c))
    return t, y1, y2


def canales(texto):
    out = []
    for lin in texto.splitlines():
        p = lin.split()
        if len(p) == 5 and p[0] == '#' and p[1] == 'canal':
            out.append((int(p[2]) - 1, p[3], p[4]))
    return out


def serie(res, tipo, cual, n):
    if tipo == 'V':
        if cual == '0':
            return [0.0] * n
        return list(res['V(%s)' % cual])
    return list(res['I(%s)' % cual])


def modos_oraculo(texto):
    """Valores propios del oráculo en la topología de 0+ (sin los nulos, cada
    par una vez: parte imaginaria >= 0)"""
    circ = orc.Circuito(texto)
    estados = tuple(sw.estado(1e-300) for sw in circ.interruptores)
    topo = orc.Topologia(circ, estados, False, 'modos')
    ev = list(topo.autovalores)
    mx = max([abs(z) for z in ev] + [0.0])
    return [z for z in ev if abs(z) > 1e-9 * mx and z.imag >= -1e-9 * abs(z)], mx


def compara_modos(ruta, texto):
    """None si coinciden; si no, el motivo"""
    nuestros = []
    with open(ruta, encoding='ascii') as f:
        for lin in f:
            re_, im = lin.split()
            nuestros.append(complex(float(re_), float(im)))
    suyos, mx = modos_oraculo(texto)
    if len(nuestros) != len(suyos):
        return 'modos: %d en el emulador y %d en el oráculo (%s / %s)' % (
            len(nuestros), len(suyos), ', '.join('%.6g' % z for z in nuestros), ', '.join('%.6g' % z for z in suyos))
    libres = list(suyos)
    for z in nuestros:
        w = min(libres, key=lambda q: abs(q - z))
        if abs(w - z) > 1e-9 * mx:
            return 'modo %.10g del emulador: el más cercano del oráculo es %.10g (%.2g relativo)' % (z, w, abs(w - z) / mx)
        libres.remove(w)
    return None


def main():
    carpeta = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'out', 'oraculo')
    nets = sorted(glob.glob(os.path.join(carpeta, 'caso_*.net')))
    if not nets:
        print('No hay casos en', carpeta)
        return 1
    n_ok = n_mal = n_rech = n_orden = n_modos_ok = n_modos_mal = 0
    peor = (0.0, '')
    motivos = {}
    for net in nets:
        nombre = os.path.basename(net)[:-4]
        texto = open(net, encoding='ascii').read()
        try:
            res = orc.simular(texto)
        except orc.ErrorCircuito as e:
            n_rech += 1
            m = str(e).split(':')[0][:60]
            motivos[m] = motivos.get(m, 0) + 1
            continue
        t1, a1, b1 = lee_ces(net[:-4] + '.ces')
        ruta2 = net[:-4] + '_h2.ces'
        t2, a2, b2 = lee_ces(ruta2) if os.path.exists(ruta2) else (None, None, None)
        bien = True
        for k, tipo, cual in canales(texto):
            ref = serie(res, tipo, cual, len(res.t))
            if len(ref) != len(t1):
                print('%s: el oráculo dio %d instantes y el emulador %d' % (nombre, len(ref), len(t1)))
                bien = False
                continue
            # escala: el máximo del oráculo, con un suelo (1 uV, 1 nA): una
            # corriente que es cero en todo el caso no es una escala
            esc = max(max(abs(x) for x in ref), 1e-6 if tipo == 'V' else 1e-9)
            y1 = a1 if k == 0 else b1
            # el último instante no se compara: si un flanco cae justo en
            # t_fin, el emulador da el valor de antes (el final de la
            # ventana) y el oráculo el de después
            m = len(ref) - 1
            e1 = max(abs(p - q) for p, q in zip(y1[:m], ref[:m])) / esc
            if e1 > peor[0]:
                peor = (e1, '%s canal %d (%s %s)' % (nombre, k + 1, tipo, cual))
            if e1 > TOL:
                print('%s canal %d (%s %s): error %.3g de la escala (máximo %.0e)' % (nombre, k + 1, tipo, cual, e1, TOL))
                bien = False
            if t2 is not None and e1 > RUIDO:
                y2 = a2 if k == 0 else b2
                e2 = max(abs(p - q) for p, q in zip(y2[:m], ref[:m])) / esc
                r = e1 / e2 if e2 > 0 else float('inf')
                if not (3.0 <= r <= 5.0) and e2 > RUIDO:
                    print('%s canal %d (%s %s): cociente h/(h/2) = %.2f (error %.3g -> %.3g)' % (nombre, k + 1, tipo, cual, r, e1, e2))
                    n_orden += 1
        ruta_m = net[:-4] + '.modos'
        if os.path.exists(ruta_m):
            motivo = compara_modos(ruta_m, texto)
            if motivo:
                print('%s: %s' % (nombre, motivo))
                n_modos_mal += 1
            else:
                n_modos_ok += 1
        if bien:
            n_ok += 1
        else:
            n_mal += 1
    print()
    print('Casos: %d iguales al oráculo (tolerancia %.0e de la escala), %d distintos, %d que el oráculo rechaza' %
          (n_ok, TOL, n_mal, n_rech))
    print('Peor error: %.3g de la escala en %s' % peor)
    print('Cocientes h/(h/2) fuera de [3, 5]: %d' % n_orden)
    print('Modos de la topología inicial: %d iguales (1e-9), %d distintos' % (n_modos_ok, n_modos_mal))
    for m, n in sorted(motivos.items(), key=lambda x: -x[1]):
        print('  rechazados por "%s...": %d' % (m, n))
    return 0 if n_mal == 0 and n_modos_mal == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
