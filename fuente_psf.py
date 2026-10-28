import gzip, struct, sys

def carga(ruta):
    d = gzip.open(ruta).read()
    if d[:4] != b'\x72\xb5\x4a\x86':
        raise SystemExit('no es PSF2')
    ver, hs, fl, n, cs, h, w = struct.unpack('<7I', d[4:32])
    glifos = [d[hs + i * cs: hs + (i + 1) * cs] for i in range(n)]
    mapa = {}
    p = hs + n * cs
    i = 0
    cur = b''
    while p < len(d) and i < n:
        b = d[p]
        if b == 0xFF:
            simples = cur.split(b'\xfe')[0]
            try:
                for ch in simples.decode('utf-8'):
                    mapa.setdefault(ord(ch), i)
            except Exception:
                pass
            cur = b''
            i += 1
        else:
            cur += bytes([b])
        p += 1
    return w, h, glifos, mapa

if __name__ == '__main__':
    w, h, g, m = carga(sys.argv[1])
    print('ancho', w, 'alto', h, 'glifos', len(g), 'unicode', len(m))
    necesarios = [0x3b8, 0x2081, 0x2082, 0x2083, 0x2084, 0x3bc, 0xb5, 0x3b1, 0x3b2, 0x3a3, 0xb0,
                  0x3c6, 0x394, 0x3c0, 0xb1, 0x2264, 0x2265, 0xb2, 0xb3, 0x2192, 0x221a, 0xb7,
                  0xd7, 0x222b, 0x3b3, 0x3c1, 0x3c9, 0x2080, 0x2260, 0x221e, 0x2190, 0x2191,
                  0x2193, 0xe1, 0xe9, 0xed, 0xf3, 0xfa, 0xf1, 0xfc, 0xd1, 0xbf, 0xa1, 0x3a9,
                  0x2225, 0x2248, 0x2713, 0x2014, 0x2016]
    print('faltan:', [hex(u) for u in necesarios if u not in m])

    def muestra(u):
        for r in g[m[u]]:
            print(''.join('#' if r & (0x80 >> x) else '.' for x in range(w)))
        print()

    for u in [ord('a'), ord('g'), ord('m'), ord('W'), 0xf1, 0x3a3, 0xb7]:
        if u in m:
            print(chr(u))
            muestra(u)
