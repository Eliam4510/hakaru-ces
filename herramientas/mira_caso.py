#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Enseña, para un caso del oráculo, dónde difieren el emulador y el oráculo.
USO: python mira_caso.py ../out/oraculo/caso_013 [n]"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oraculo_sim as orc  # noqa: E402
from compara_oraculo import lee_ces, canales, serie  # noqa: E402

base = sys.argv[1]
n = int(sys.argv[2]) if len(sys.argv) > 2 else 12
texto = open(base + '.net', encoding='ascii').read()
res = orc.simular(texto)
t, a, b = lee_ces(base + '.ces')
for k, tipo, cual in canales(texto):
    ref = serie(res, tipo, cual, len(res.t))
    y = a if k == 0 else b
    d = [abs(p - q) for p, q in zip(y, ref)]
    i0 = max(range(len(d)), key=lambda i: d[i])
    print('canal %d (%s %s): peor en t = %.6g: emulador %.6g, oráculo %.6g' % (k + 1, tipo, cual, t[i0], y[i0], ref[i0]))
    for i in list(range(0, min(n, len(t)))) + list(range(max(0, i0 - 3), min(len(t), i0 + 4))):
        print('   t=%-12.6g emu=%-14.8g orc=%-14.8g' % (t[i], y[i], ref[i]))
if res.avisos:
    print('avisos del oráculo:', res.avisos)
