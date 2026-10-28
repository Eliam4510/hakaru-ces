"""Cuenta las líneas de código del repositorio y las escribe en el README.

    python herramientas/estadisticas_codigo.py           # actualiza el README y la insignia
    python herramientas/estadisticas_codigo.py --ver     # solo las enseña

Cuenta los archivos que sigue git (git ls-files), así que nunca entra lo
ignorado (out/, los binarios). Cada línea es de código, de comentario o en
blanco, como hace cloc: en C (.c, .h, .inc) con los comentarios /* */ y //,
saltando las cadenas y los caracteres; en Python, # y los docstrings; en sh
y en el Makefile, #. Los archivos que .gitattributes marca como generados
(linguist-generated) se cuentan aparte, igual que los quita la barra de
lenguajes de GitHub.

Escribe:
  * en README.md, entre <!-- ESTADISTICAS:INICIO --> y <!-- ESTADISTICAS:FIN -->,
    un gráfico de pastel en Mermaid (lo dibuja GitHub) con las líneas de
    código por área y una tabla por lenguaje con los totales;
  * .github/insignias/lineas.json, la insignia «líneas de código» en el
    formato endpoint de shields.io.

El workflow .github/workflows/estadisticas.yml lo corre en cada push a main y
sube el resultado si cambió. Solo usa la biblioteca estándar.
"""
import fnmatch
import json
import os
import re
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
README = os.path.join(RAIZ, 'README.md')
INSIGNIA = os.path.join(RAIZ, '.github', 'insignias', 'lineas.json')
INICIO, FIN = '<!-- ESTADISTICAS:INICIO -->', '<!-- ESTADISTICAS:FIN -->'

LENGUAJES = [                  # (nombre, patrones)
    ('C', ['*.c', '*.h', '*.inc']),
    ('Python', ['*.py']),
    ('Shell', ['*.sh']),
    ('Makefile', ['Makefile', '*.mk']),
]

# Las áreas del pastel, en este orden (el primer patrón que encaja manda)
AREAS = [
    ('Pruebas (arnés)', ['host_test.c', 'test_*.inc']),
    ('Interfaz y pantallas', ['ui.[ch]', 'esquema.[ch]', 'gfx.[ch]', 'font*.c', 'font.h', 'kanji_img.[ch]',
                              'app.h', 'osc.[ch]', 'trazador.[ch]', 'curva.[ch]', 'fasorial.[ch]', 'numfmt.[ch]']),
    ('Emulador y Newton', ['sim.[ch]', 'lu.[ch]', 'ondas.[ch]', 'nolin.[ch]']),
    ('CC y procedimiento', ['resuelve.[ch]', 'algebra.[ch]', 'arbol.[ch]', 'proced.[ch]', 'pasos.[ch]',
                            'circuito.[ch]', 'red.[ch]', 'tipos.[ch]', 'barrido.[ch]']),
    ('Corriente alterna', ['ca.[ch]', 'cx.[ch]', 'luc.[ch]', 'calca.[ch]', 'bode.[ch]']),
    ('Plataforma y archivos', ['main.c', 'bucle.[ch]', 'entrada.[ch]', 'archivo.[ch]', 'banco.[ch]',
                               'turbo.[ch]', 'cf_arm.c', 'Makefile', '*.sh']),
    ('Lecciones y capacitores', ['lecciones.[ch]', 'ejemplos.[ch]', 'biblioteca.[ch]', 'capmotor*.[ch]',
                                 'capeq_texto.[ch]']),
    ('Oráculos y herramientas', ['herramientas/*', '*.py']),
]

# La paleta categórica (validada para series vecinas), una por área
COLORES = ['#2a78d6', '#eb6834', '#1baf7a', '#eda100', '#e87ba4', '#008300', '#6250d6', '#e34948']


# --------------------------------------------------------------------------
#  Contar
# --------------------------------------------------------------------------
def contar_c(lineas):
    """(código, comentario, blanco) de un archivo de C"""
    cod = com = bla = 0
    en_bloque = False
    for lin in lineas:
        hay_cod = False
        hay_com = en_bloque
        i, n = 0, len(lin)
        while i < n:
            if en_bloque:
                if lin.startswith('*/', i):
                    en_bloque = False
                    i += 2
                else:
                    i += 1
                continue
            if lin.startswith('/*', i):
                en_bloque = hay_com = True
                i += 2
                continue
            if lin.startswith('//', i):
                hay_com = True
                break
            c = lin[i]
            if c in '"\'':
                hay_cod = True
                i += 1
                while i < n and lin[i] != c:
                    i += 2 if lin[i] == '\\' else 1
                i += 1
                continue
            if not c.isspace():
                hay_cod = True
            i += 1
        if hay_cod:
            cod += 1
        elif hay_com:
            com += 1
        else:
            bla += 1
    return cod, com, bla


DOC = re.compile(r'^[rRuUbB]{0,2}("""|\'\'\')')


def contar_py(lineas):
    """(código, comentario, blanco) de un archivo de Python: # y los
    docstrings (una cadena triple que empieza la línea) son comentario; una
    cadena triple que empieza a media línea es código (datos)"""
    cod = com = bla = 0
    cierre = None              # el cierre de la cadena triple abierta
    es_doc = False
    for lin in lineas:
        s = lin.strip()
        if cierre:
            if es_doc:
                com += 1
            else:
                cod += 1
            if cierre in s:
                cierre = None
            continue
        if not s:
            bla += 1
            continue
        if s.startswith('#'):
            com += 1
            continue
        m = DOC.match(s)
        if m:
            q = m.group(1)
            com += 1
            if s.count(q) == 1:
                cierre, es_doc = q, True
            continue
        cod += 1
        for q in ('"""', "'''"):
            if s.count(q) % 2 == 1:
                cierre, es_doc = q, False
                break
    return cod, com, bla


def contar_almohadilla(lineas):
    cod = com = bla = 0
    for lin in lineas:
        s = lin.strip()
        if not s:
            bla += 1
        elif s.startswith('#'):
            com += 1
        else:
            cod += 1
    return cod, com, bla


def encaja(ruta, patrones):
    base = os.path.basename(ruta)
    return any(fnmatch.fnmatch(ruta, p) or ('/' not in p and fnmatch.fnmatch(base, p) and '/' not in ruta)
               or (p.endswith('/*') and ruta.startswith(p[:-1])) for p in patrones)


def archivos():
    r = subprocess.run(['git', 'ls-files', '-z'], cwd=RAIZ, capture_output=True, check=True)
    todos = [f for f in r.stdout.decode('utf-8').split('\0') if f]
    r = subprocess.run(['git', 'check-attr', '-z', 'linguist-generated', '--'] + todos,
                       cwd=RAIZ, capture_output=True, check=True)
    campos = r.stdout.decode('utf-8').split('\0')
    generados = {campos[i] for i in range(0, len(campos) - 2, 3) if campos[i + 2] in ('true', 'set')}
    return todos, generados


def contar():
    todos, generados = archivos()
    por_leng = {n: [0, 0, 0, 0] for n, _ in LENGUAJES}          # archivos, código, comentario, blanco
    por_area = {n: 0 for n, _ in AREAS}
    gen = [0, 0]                                               # archivos, líneas
    for f in todos:
        leng = next((n for n, pats in LENGUAJES if encaja(f, pats)), None)
        if leng is None:
            continue
        with open(os.path.join(RAIZ, f), encoding='utf-8', errors='replace') as h:
            lineas = h.read().splitlines()
        if f in generados:
            gen[0] += 1
            gen[1] += len(lineas)
            continue
        if leng == 'C':
            c = contar_c(lineas)
        elif leng == 'Python':
            c = contar_py(lineas)
        else:
            c = contar_almohadilla(lineas)
        t = por_leng[leng]
        t[0] += 1
        for k in range(3):
            t[k + 1] += c[k]
        area = next((n for n, pats in AREAS if encaja(f, pats)), AREAS[-1][0])
        por_area[area] += c[0]
    return por_leng, por_area, gen


# --------------------------------------------------------------------------
#  Escribir
# --------------------------------------------------------------------------
def num(v):
    return f'{v:,}'.replace(',', ' ')


def bloque(por_leng, por_area, gen):
    tot = [sum(t[k] for t in por_leng.values()) for k in range(4)]
    areas = sorted(por_area.items(), key=lambda a: -a[1])
    colores = ', '.join(f'"pie{i + 1}": "{COLORES[i]}"' for i in range(len(areas)))
    out = [INICIO,
           '',
           '```mermaid',
           '%%{init: {"themeVariables": {' + colores + ', "pieStrokeWidth": "2px", '
           '"pieOuterStrokeWidth": "0px", "pieSectionTextSize": "13px"}}}%%',
           'pie showData',
           f'    title Líneas de código por área ({num(tot[1])})']
    for n, v in areas:
        out.append(f'    "{n}" : {v}')
    out += ['```',
            '',
            '| Lenguaje | Archivos | Código | Comentarios | En blanco | Total |',
            '|---|---:|---:|---:|---:|---:|']
    for n, t in por_leng.items():
        if t[0]:
            out.append(f'| {n} | {t[0]} | {num(t[1])} | {num(t[2])} | {num(t[3])} | {num(t[1] + t[2] + t[3])} |')
    out.append(f'| **Total** | **{tot[0]}** | **{num(tot[1])}** | **{num(tot[2])}** | **{num(tot[3])}** | '
               f'**{num(tot[1] + tot[2] + tot[3])}** |')
    out += ['',
            f'Se cuentan los archivos del repositorio escritos a mano; los generados por las herramientas '
            f'({gen[0]} archivos, {num(gen[1])} líneas: la fuente en mapa de bits, el kanji y las tablas '
            f'de soluciones exactas) quedan aparte, como en la barra de lenguajes de GitHub. El recuento se '
            f'actualiza en cada cambio con el workflow '
            f'[Estadísticas del código](.github/workflows/estadisticas.yml) '
            f'([`herramientas/estadisticas_codigo.py`](herramientas/estadisticas_codigo.py)).',
            '',
            FIN]
    return '\n'.join(out), tot


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    por_leng, por_area, gen = contar()
    texto, tot = bloque(por_leng, por_area, gen)
    if '--ver' in sys.argv:
        print(texto)
        return
    with open(README, encoding='utf-8', newline='') as h:
        s = h.read()
    i, j = s.find(INICIO), s.find(FIN)
    if i < 0 or j < i:
        sys.exit('README.md: faltan las marcas ' + INICIO + ' y ' + FIN)
    s = s[:i] + texto + s[j + len(FIN):]
    with open(README, 'w', encoding='utf-8', newline='') as h:
        h.write(s)
    os.makedirs(os.path.dirname(INSIGNIA), exist_ok=True)
    insignia = {'schemaVersion': 1, 'label': 'líneas de código', 'message': num(tot[1]),
                'color': '3987e5', 'labelColor': '101218'}
    with open(INSIGNIA, 'w', encoding='utf-8', newline='\n') as h:
        json.dump(insignia, h, ensure_ascii=False)
        h.write('\n')
    print(f'código {tot[1]}, comentarios {tot[2]}, en blanco {tot[3]}, total {tot[1] + tot[2] + tot[3]}')


if __name__ == '__main__':
    main()
