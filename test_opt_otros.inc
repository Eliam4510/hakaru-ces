/* ============================================================================
 *  test_opt_otros.inc  -  [75o] La optimización del grupo «otros» (rev9: los
 *  puntos 6 y 8 del plan y la preparación del 19). Lo nuevo tiene que dar lo
 *  mismo que lo de antes, bit a bit; cada prueba falla sin su arreglo o con
 *  la invalidación rota (comprobado por mutación).
 *
 *    O1  las combinaciones de signos por niveles (algebra.c) frente a sumar
 *        cada juego entero (sis_signos_lento): sistemas al azar de 1 a 9
 *        términos, con filas que se anulan (compatibles o no según el
 *        juego), pares seguidos del mismo tamaño (la tabla se rehace al
 *        reducir otro), el tamaño máximo y juegos con bits de más
 *    O2  resolver() de las dos maneras: los ejemplos, el peor caso del banco
 *        y redes al azar con datos (de sobra, con un 0): la Solucion entera y
 *        las operaciones contadas
 *    O3  numfmt: las potencias de 10 memorizadas, la década por los bits y
 *        formatea en 32 bits frente a pow, log10 y 64 bits (num_lento), con
 *        valores al azar, junto a las potencias de 10 (el margen de 2^14 ulp),
 *        en los bordes de la tabla (1e±40, 2^±120) y de los 32 bits, y 0, -0,
 *        subnormales, enormes, NaN e Inf
 *    O4  proced_generar de las dos maneras: los mismos pasos y el mismo método
 *    O5  la segunda página del banco (punto 19): sin banco_extra, el banco de
 *        siempre; con él, las 12 líneas tras las 13, se ve la que se mide,
 *        [izq/der] cambian de página, [enter] repite desde la primera, [esc]
 *        a mitad de la segunda la cancela; las ventanas largas no tocan el
 *        tope de pasos y el paso de B2_PASO no se reinicia
 *
 *  La coma flotante del ARM (cf_arm.c, punto 8) no se prueba aquí: el PC no
 *  la usa. Su prueba es diferencial, en qemu-arm (rev9/otros_herr).
 *
 *  Va al final de la batería: no mueve la numeración de las BMP ni la
 *  semilla de nadie (usa la suya); sus pantallas van a out/otros_*.bmp.
 * ========================================================================= */

static unsigned oo_sem = 24681357u;
static unsigned oo_az(unsigned n) { oo_sem = oo_sem * 1103515245u + 12345u; return (oo_sem >> 16) % n; }

/* ---- O1. Los juegos de signos ---- */

static Sistema oo_sis;

/* Coeficientes: enteros pequeños (las filas repetidas se anulan exactas),
 * ceros y decimales de varios órdenes */
static double oo_coef(void)
{
    unsigned r = oo_az(10);
    if (r < 3) return 0.0;
    if (r < 7) return (double)((int)oo_az(21) - 10);
    return ((double)oo_az(20001) - 10000.0) / 100.0 * pow(10.0, (double)oo_az(7) - 3.0);
}

/* Un sistema de nunk incógnitas y nterm términos: nind filas al azar y
 * detrás nrep que son la suma de dos anteriores (se anulan al reducir). Sus
 * términos: la suma (compatible), la resta en los datos (compatible en unos
 * juegos y no en otros), la suma movida un poco o todo 0 */
static void oo_sistema(int nunk, int nind, int nrep, int nterm)
{
    int i, j, t;
    sis_init(&oo_sis);
    sis_terminos(&oo_sis, nterm);
    sis_incognitas(&oo_sis, nunk);
    for (i = 0; i < nind + nrep; i++) {
        int e = sis_ecuacion(&oo_sis);
        if (i >= nind && i >= 1) {
            int p = (int)oo_az((unsigned)i), q = (int)oo_az((unsigned)i);
            unsigned forma = oo_az(5);
            for (j = 0; j < nunk; j++) oo_sis.a[e][j] = oo_sis.a[p][j] + oo_sis.a[q][j];
            for (t = 0; t < nterm; t++) {
                double v = oo_sis.b[t][p] + oo_sis.b[t][q];
                if (forma == 2 && t > 0) v = oo_sis.b[t][p] - oo_sis.b[t][q];
                else if (forma == 3 && t == 0) v += (oo_az(2) ? 5e-9 : 3e-8) * (double)oo_az(3);
                else if (forma == 4) v = 0.0;
                oo_sis.b[t][e] = v;
            }
        } else {
            for (j = 0; j < nunk; j++) oo_sis.a[e][j] = oo_coef();
            for (t = 0; t < nterm; t++) oo_sis.b[t][e] = oo_coef();
        }
    }
}

/* Reduce oo_sis y compara cada juego (en un orden revuelto, y alguno con
 * bits de más) por niveles y sumado entero; cuenta lo que da cada uno y
 * devuelve los distintos */
static long oo_da[4];
static int oo_signos_iguales(long *juegos)
{
    static double x1[MAX_UNK], x2[MAX_UNK];
    unsigned c, n = 1u << (oo_sis.nterm - 1), s, mult = 2u * oo_az(64) + 1u;
    int rango, g1, g2, malos = 0;
    unsigned long o, o1, o2;
    sis_reducir(&oo_sis, &rango, 0);
    for (c = 0; c < n + 3u; c++) {
        s = c < n ? (c * mult + 7u) & (n - 1u) : oo_az(n) | (1u << (oo_sis.nterm - 1 + 3 * (int)(c - n)));
        memset(x1, 0x5A, sizeof x1); memset(x2, 0x5A, sizeof x2);
        sis_signos_lento(0); o = sis_ops(); g1 = sis_combinar_signos(s, x1); o1 = sis_ops() - o;
        sis_signos_lento(1); o = sis_ops(); g2 = sis_combinar_signos(s, x2); o2 = sis_ops() - o;
        sis_signos_lento(0);
        (*juegos)++;
        if (g1 >= 0 && g1 < 4) oo_da[g1]++;
        if (g1 != g2 || o1 != o2 || memcmp(x1, x2, sizeof x1) != 0) malos++;
    }
    return malos;
}

/* x = 1 ± B1, y = 2 ± 3 y otra vez x = 1 + d ± B1: la fila repetida se
 * anula y le queda d, entre las dos tolerancias, 1e-8·(1 + |b|max) del juego
 * 0 (1e-8·(2 + B1)) y del 1 (1e-8·B1): compatible en uno y no en el otro */
static void oo_sistema_borde(double B1, double d)
{
    int e;
    sis_init(&oo_sis);
    sis_terminos(&oo_sis, 2);
    sis_incognitas(&oo_sis, 2);
    e = sis_ecuacion(&oo_sis); oo_sis.a[e][0] = 1.0; oo_sis.b[0][e] = 1.0; oo_sis.b[1][e] = B1;
    e = sis_ecuacion(&oo_sis); oo_sis.a[e][1] = 1.0; oo_sis.b[0][e] = 2.0; oo_sis.b[1][e] = 3.0;
    e = sis_ecuacion(&oo_sis); oo_sis.a[e][0] = 1.0; oo_sis.b[0][e] = 1.0 + d; oo_sis.b[1][e] = B1;
}

static void oo_signos(void)
{
    const char *q = "O1 signos por niveles";
    char det[400];
    long juegos = 0;
    int k, malos = 0, nsis = 0, borde = 0;
    memset(oo_da, 0, sizeof oo_da);
    for (k = 0; k < 3000; k++) {
        int nunk = 1 + (int)oo_az(30), nind = nunk - (int)oo_az(3), nrep = oo_az(3) ? (int)oo_az(7) : 0;
        int nterm = 1 + (int)oo_az(9), par;
        if (nind < 1) nind = 1;
        /* dos sistemas seguidos del mismo tamaño: el segundo no puede leer
         * las sumas del primero */
        for (par = 0; par < 2; par++, nsis++) {
            oo_sistema(nunk, nind, nrep, nterm);
            malos += oo_signos_iguales(&juegos);
        }
    }
    /* el tamaño máximo: 100 incógnitas, 128 ecuaciones y 9 términos (las
     * tablas llenas), con y sin filas que se anulan */
    for (k = 0; k < 4; k++, nsis++) {
        oo_sistema(MAX_UNK, k & 1 ? MAX_EQ : MAX_UNK, k & 1 ? 0 : MAX_EQ - MAX_UNK, MAX_TERM);
        malos += oo_signos_iguales(&juegos);
    }
    /* en el borde de la tolerancia: la escala de cada juego decide */
    for (k = 0; k < 200; k++, nsis++) {
        double B1 = 50.0 + (double)oo_az(450);
        double x[MAX_UNK];
        int g0, g1;
        oo_sistema_borde(k & 1 ? B1 : -B1, 1e-8 * (1.0 + B1));
        malos += oo_signos_iguales(&juegos);
        g0 = sis_combinar_signos(0u, x); g1 = sis_combinar_signos(1u, x);
        borde += (g0 == GAUSS_INCOMPATIBLE) + (g1 == GAUSS_INCOMPATIBLE) == 1;   /* uno de los dos juegos */
    }
    /* un sistema vacío y uno de un término */
    oo_sistema(0, 1, 0, 1);
    malos += oo_signos_iguales(&juegos);
    oo_sistema(1, 1, 0, 1);
    malos += oo_signos_iguales(&juegos);
    nsis += 2;
    sprintf(det, "%ld juegos en %d sistemas (%ld con soluci" S_oo "n, %ld indeterminados, %ld incompatibles): por niveles = "
            "sumado entero, bit a bit y las mismas operaciones; %d de 200 en el borde de la tolerancia", juegos, nsis,
            oo_da[GAUSS_OK], oo_da[GAUSS_INDETERMINADO], oo_da[GAUSS_INCOMPATIBLE], borde);
    printf("    %s: %s\n", q, det);
    chk_cond(q, det, malos == 0 && juegos > 300000 && oo_da[GAUSS_OK] > 20000 && oo_da[GAUSS_INDETERMINADO] > 20000 &&
             oo_da[GAUSS_INCOMPATIBLE] > 20000 && borde == 200);
    if (malos) printf("    %d juegos distintos\n", malos);
}

/* ---- O2. resolver() de las dos maneras ---- */

static int oo_resuelve_igual(const Circuito *c, const Red *r, int *estado)
{
    static Solucion s1, s2;
    unsigned long o, o1, o2;
    sis_signos_lento(0); o = sis_ops(); resolver(c, r, &s1); o1 = sis_ops() - o;
    sis_signos_lento(1); o = sis_ops(); resolver(c, r, &s2); o2 = sis_ops() - o;
    sis_signos_lento(0);
    if (estado) *estado = s1.estado;
    return o1 == o2 && memcmp(&s1, &s2, sizeof s1) == 0;
}

/* Una red de fuente_sp con nd datos (V o I en magnitud) y nb R buscadas; con
 * cero, uno de los datos vale 0 */
static void oo_red_datos(int nd, int nb, int cero)
{
    static Solucion s;
    int k, n = 0, f;
    f = fuente_sp(1.0 + (double)oo_az(240));
    resolver(&rc, &rr, &s);
    if (s.estado != SOL_OK) return;
    for (k = 1; k < rc.nel && n < nd; k++) {
        int esI = (int)oo_az(2);
        double v = esI ? fabs(s.s[0].I[k]) : fabs(s.s[0].V[k]);
        if (!(v > 1e-9)) continue;
        if (esI) { rc.el[k].I.est = M_DATO; rc.el[k].I.v = cero && n == 0 ? 0.0 : v; }
        else { rc.el[k].V.est = M_DATO; rc.el[k].V.v = cero && n == 0 ? 0.0 : v; }
        if (n < nb) rc.el[k].val.est = M_BUSCAR;
        n++;
    }
    if (oo_az(4) == 0) { rc.el[f].val.est = M_BUSCAR; rc.el[f].I.est = M_DATO; rc.el[f].I.v = fabs(s.s[0].I[f]); }
}

static void oo_resolver(void)
{
    const char *q = "O2 resolver";
    char det[400];
    static Circuito c;
    static Red r;
    int i, malos = 0, n = 0, ok = 0, e;
    for (i = 0; i < N_EJEMPLOS; i++) {
        ejemplo_cargar(i, &c);
        red_extraer(&c, &r);
        malos += !oo_resuelve_igual(&c, &r, &e);
        n++; ok += e == SOL_OK;
    }
    banco_peor_caso(&c, &r);
    malos += !oo_resuelve_igual(&c, &r, &e);
    n++; ok += e == SOL_OK;
    sprintf(det, "los %d ejemplos y el peor caso del banco: la misma Solucion y las mismas operaciones", N_EJEMPLOS);
    printf("    %s: %s\n", q, det);
    chk_cond(q, det, malos == 0 && e == SOL_OK);

    malos = 0;
    for (i = 0; i < 600; i++) {
        int nd = 1 + (int)oo_az(8), nb = (int)oo_az((unsigned)nd + 1u);
        oo_red_datos(nd, nb, oo_az(10) == 0);
        malos += !oo_resuelve_igual(&rc, &rr, &e);
        n++; ok += e == SOL_OK;
    }
    sprintf(det, "%d redes al azar con 1 a 8 datos (de sobra o con un 0): la misma Solucion; %d con soluci" S_oo "n", 600, ok);
    printf("    %s: %s\n", q, det);
    chk_cond(q, det, malos == 0 && ok > 100);
}

/* ---- O3. numfmt ---- */

static int oo_num_malos, oo_num_n;
static char oo_num_ej[256];

static void oo_mal(const char *que, double v, const char *a, const char *b)
{
    if (!oo_num_malos) sprintf(oo_num_ej, "%s(%.17g): \"%s\" y \"%s\"", que, v, a, b);
    oo_num_malos++;
}

/* Cada formato de v, de las dos maneras */
static void oo_num_uno(double v)
{
    static const char *const UNI[4] = { "V", " s", "", S_OHM };
    char a[80], b[80];
    int i, ra, rb;
    oo_num_n++;
    for (i = 1; i <= 9; i++) {
        num_lento(0); num_pref(a, sizeof a, v, i, UNI[i & 3]);
        num_lento(1); num_pref(b, sizeof b, v, i, UNI[i & 3]);
        if (strcmp(a, b)) oo_mal("num_pref", v, a, b);
    }
    for (i = -1; i <= 10; i++) {
        /* formatea con v·10^dec fuera de un long long: comportamiento
         * indefinido desde siempre (UBSan); ni antes ni ahora se llega */
        if (!(fabs(v) > 1e15) && fabs(v) * pow(10.0, (double)(i < 0 ? 0 : i > 9 ? 9 : i)) + 0.5 >= 9.2e18) continue;
        num_lento(0); num_txt(a, sizeof a, v, i);
        num_lento(1); num_txt(b, sizeof b, v, i);
        if (strcmp(a, b)) oo_mal("num_txt", v, a, b);
        num_lento(0); num_auto(a, sizeof a, v, i);
        num_lento(1); num_auto(b, sizeof b, v, i);
        if (strcmp(a, b)) oo_mal("num_auto", v, a, b);
    }
    num_lento(0); ra = num_sig(a, sizeof a, v);
    num_lento(1); rb = num_sig(b, sizeof b, v);
    if (strcmp(a, b) || ra != rb) oo_mal("num_sig", v, a, b);
    if (v - v == 0.0) {                         /* num_corto es para finitos */
        num_lento(0); num_corto(a, sizeof a, v);
        num_lento(1); num_corto(b, sizeof b, v);
        if (strcmp(a, b)) oo_mal("num_corto", v, a, b);
    }
    num_lento(0); num_pot(a, sizeof a, v, "W");
    num_lento(1); num_pot(b, sizeof b, v, "W");
    if (strcmp(a, b)) oo_mal("num_pot", v, a, b);
    for (i = 0; i < 4; i++) {
        double g = i == 0 ? 0.0 : i == 1 ? -45.0 : i == 2 ? 179.96 : 38.2;
        num_lento(0); num_fasor(a, sizeof a, v, g, i & 1, i >> 1, "A");
        num_lento(1); num_fasor(b, sizeof b, v, g, i & 1, i >> 1, "A");
        if (strcmp(a, b)) oo_mal("num_fasor", v, a, b);
    }
    {
        double x = fabs(v);
        num_lento(0);
        if (x > 0.0 && x < 1e300 && num_decada(x) != (int)floor(log10(x))) {
            sprintf(a, "%d", num_decada(x)); sprintf(b, "%d", (int)floor(log10(x)));
            oo_mal("num_decada", v, a, b);
        }
    }
    num_lento(0);
}

static double oo_de_bits(unsigned long long u) { double d; memcpy(&d, &u, sizeof d); return d; }
static unsigned long long oo_bits(double d) { unsigned long long u; memcpy(&u, &d, sizeof u); return u; }

static void oo_numfmt(void)
{
    const char *q = "O3 numfmt";
    char det[512];
    static const double ESP[] = { 0.0, 1.0, 10.0, 999.95, 999.949999, 9.9995, 0.099995, 4.9e-324, 1e-310,
                                  2.2250738585072014e-308, 1e-300, 1e15, 1.0000000000000002e15, 1e21, 1e299,
                                  9.9999e299, 1e300, 1.7976931348623157e308, 12345.6789, 0.5, 2.5e-6, 6.321 };
    static const long SALTO[] = { 1, 2, 3, 7, 50, 1000, 16383, 16384, 16385, 20000, 100000, 1000000 };
    int i, k, j, s;
    oo_num_malos = oo_num_n = 0;
    for (i = 0; i < (int)(sizeof ESP / sizeof ESP[0]); i++) { oo_num_uno(ESP[i]); oo_num_uno(-ESP[i]); }
    oo_num_uno(oo_de_bits(0x7FF8000000000000ull));     /* NaN, con los dos signos */
    oo_num_uno(oo_de_bits(0xFFF8000000000001ull));
    oo_num_uno(oo_de_bits(0x7FF0000000000000ull));
    oo_num_uno(oo_de_bits(0xFFF0000000000000ull));
    /* junto a cada potencia de 10 de la tabla y fuera de ella: a unos ulp y
     * a los lados del margen de 2^14 */
    for (k = -46; k <= 46; k++) {
        unsigned long long p = oo_bits(pow(10.0, (double)k));
        oo_num_uno(oo_de_bits(p));
        for (j = 0; j < (int)(sizeof SALTO / sizeof SALTO[0]); j++)
            for (s = -1; s <= 1; s += 2) oo_num_uno(oo_de_bits(p + (unsigned long long)((long long)s * SALTO[j])));
    }
    /* los bordes de 2^±120 (la década por los bits) */
    for (k = -122; k <= 122; k += 244)
        for (j = -3; j <= 3; j++) oo_num_uno(ldexp(1.0, k + j) * (1.0 + (double)j * 1e-3));
    /* formatea: y = v·10^dec + 0,5 junto a 2^32 */
    for (k = 0; k <= 9; k++) {
        double v0 = (4294967296.0 - 0.5) / pow(10.0, (double)k);
        unsigned long long p = oo_bits(v0);
        for (j = -40; j <= 40; j++) oo_num_uno(oo_de_bits(p + (unsigned long long)(long long)j));
    }
    /* al azar: bits (todos los órdenes) y decimales «de circuito» */
    for (i = 0; i < 30000; i++) {
        unsigned long long u = ((unsigned long long)oo_az(65536) << 48) ^ ((unsigned long long)oo_az(65536) << 32) ^
                               ((unsigned long long)oo_az(65536) << 16) ^ (unsigned long long)oo_az(65536);
        oo_num_uno(oo_de_bits(u));
        oo_num_uno(((double)oo_az(2000001) - 1000000.0) * pow(10.0, (double)oo_az(40) - 26.0));
    }
    sprintf(det, "%d valores, cada formato de las dos maneras (memorizadas y por los bits frente a pow, log10 y 64 bits): "
            "%d distintos%s%s", oo_num_n, oo_num_malos, oo_num_malos ? "; " : "", oo_num_malos ? oo_num_ej : "");
    printf("    %s: %s\n", q, det);
    chk_cond(q, det, oo_num_malos == 0 && oo_num_n > 60000);
}

/* ---- O4. Los pasos de los ejemplos ---- */

static int oo_pasos_igual(const Circuito *c, const Red *r)
{
    static Solucion s1, s2;
    static Pasos p1, p2;
    static ValArbol v1, v2;
    int m1, m2;
    memset(&p1, 0, sizeof p1); memset(&p2, 0, sizeof p2);
    memset(&v1, 0, sizeof v1); memset(&v2, 0, sizeof v2);
    num_lento(0); sis_signos_lento(0);
    resolver(c, r, &s1);
    m1 = proced_generar(c, r, &s1, 0, &p1, &v1);
    num_lento(1); sis_signos_lento(1);
    resolver(c, r, &s2);
    m2 = proced_generar(c, r, &s2, 0, &p2, &v2);
    num_lento(0); sis_signos_lento(0);
    return m1 == m2 && memcmp(&p1, &p2, sizeof p1) == 0 && memcmp(&v1, &v2, sizeof v1) == 0;
}

static void oo_pasos(void)
{
    const char *q = "O4 pasos";
    char det[400];
    static Circuito c;
    static Red r;
    int i, malos = 0;
    for (i = 0; i < N_EJEMPLOS; i++) {
        ejemplo_cargar(i, &c);
        red_extraer(&c, &r);
        malos += !oo_pasos_igual(&c, &r);
    }
    banco_peor_caso(&c, &r);
    malos += !oo_pasos_igual(&c, &r);
    for (i = 0; i < 300; i++) {
        oo_red_datos(1 + (int)oo_az(4), 1, 0);
        malos += !oo_pasos_igual(&rc, &rr);
    }
    sprintf(det, "los %d ejemplos, el peor caso y 300 redes: los mismos pasos, el mismo m" S_ee "todo y el mismo " S_aa "rbol", N_EJEMPLOS);
    printf("    %s: %s\n", q, det);
    chk_cond(q, det, malos == 0);
}

/* ---- O5. La segunda página del banco ---- */

/* Una pantalla con nombre propio (out/otros_*.bmp), fuera de la numeración
 * de vuelca(); las que llevan tiempos, con «banco_hecho» en el nombre (las
 * comparaciones de BMP no las miran: cambian en cada corrida) */
static void oo_vuelca(const char *nombre)
{
    char ruta[300];
    ui_dibuja();
    sprintf(ruta, "%s/otros_%s.bmp", dir_out, nombre);
    gfx_dump_bmp(ruta);
}

/* Una ventana entera del emulador con h fijo, como la carga del banco: los
 * pasos que da (con los de los flancos) y cómo acaba */
static int oo_ventana(void (*red)(Circuito *, Red *), double t_fin, double h, long *np)
{
    static Circuito c;
    static Red r;
    int fin;
    red(&c, &r);
    c.t_fin.v = t_fin; c.t_fin.est = M_DATO; c.paso.v = h; c.paso.est = M_DATO;
    sim_iniciar(&c, &r);
    while (sim_avanzar(~0ul)) {}
    *np = sim_res()->pasos;
    fin = sim_res()->fin;
    sim_cortar();
    return fin;
}

static void oo_banco(void)
{
    const char *q = "O5 banco, segunda p" S_aa "gina";
    char det[400];
    int n, i, bien = 1, p2, f1, f2;
    long np1, np2;
    gfx_desbordes_reset();

    /* Las ventanas de B2_PASO (16 ms) y de B2_PEQUE (20 ms) con h = 2 µs,
     * enteras: sin tocar el tope de pasos. La larga, con más de 16 000: en
     * los 4 s de la calculadora no se acaba mientras un paso tarde más de
     * 0,25 ms (hoy tarda ~1) */
    f1 = oo_ventana(banco_peor_sim, 16e-3, 2e-6, &np1);
    f2 = oo_ventana(banco_rc, 20e-3, 2e-6, &np2);
    sprintf(det, "las ventanas del banco enteras: la del peor emulador, %ld pasos; la del RC, %ld (tope %d)",
            np1, np2, SIM_MAX_PASOS);
    printf("    %s: %s\n", q, det);
    chk_cond(q, det, f1 == FIN_COMPLETO && f2 == FIN_COMPLETO && np1 > 16000 && np1 < SIM_MAX_PASOS &&
             np2 > 10000 && np2 < SIM_MAX_PASOS);

    /* Sin ella (el PC, por omisión): el banco de siempre */
    banco_extra(0);
    ui_init();
    TECLAS(T_ENTER, T_ABAJO, T_ABAJO, T_ENTER, T_LETRA('T'));
    for (n = 0; n < 40 && !banco_hecho(); n++) { ui_dibuja(); ui_tick(); }
    TECLAS(T_DER);
    chk_cond(q, "sin la segunda: las 13 pruebas, [der] no cambia de p" S_aa "gina y de ella nada",
             n == BN_N && banco_en_pagina() == 0 && banco_ms2(B2_DDIV) < 0.0 && !ve("(1 de 2)"));

    /* Con ella: la primera y luego la segunda, que se ve mientras se mide */
    banco_extra(1);
    TECLAS(T_ESC, T_LETRA('T'));                     /* otra vez, con las dos */
    p2 = -1;
    for (n = 0; n < 40 && !banco_hecho(); n++) {
        if (n == BN_N) { p2 = banco_en_pagina(); oo_vuelca("banco_p2_empieza"); }
        ui_dibuja();
        ui_tick();
    }
    for (i = 0; i < B2_N; i++) if (!(banco_ms2(i) > 0.0)) bien = 0;
    sprintf(det, "las %d de la segunda tras las 13 (%d ticks), con un tiempo cada una; se ve la segunda al medirla y al acabar",
            B2_N, n);
    chk_cond(q, det, n == BN_N + B2_N && bien && p2 == 1 && banco_en_pagina() == 1 && !ui_animando());
    /* (lo que depende del reloj, las operaciones medias, no se escribe: la
     * salida ha de ser la misma en cada corrida y por la capa) */
    sprintf(det, "el paso sin reinicios: ventana de %.0f pasos sin flancos (la de arriba), %ld reinicios (la red peque" S_nn "a, %ld); "
            "m" S_aa "s de 1000 op. por paso (la peque" S_nn "a, entre 10 y un quinto de eso); un paso o m" S_aa "s por vuelta",
            banco_ventana2(), banco_reinicios2(B2_PASO), banco_reinicios2(B2_PEQUE));
    printf("    %s: %s\n", q, det);
    chk_cond(q, det, fabs(banco_ventana2() - 16e-3 / 2e-6) < 1e-6 && banco_reinicios2(B2_PASO) == 0 &&
             banco_reinicios2(B2_PEQUE) == 0 && banco_reinicios2(B2_CA) == 0 && banco_ops2(B2_PASO) > 1000.0 &&
             banco_ops2(B2_PEQUE) > 10.0 && banco_ops2(B2_PEQUE) < 0.2 * banco_ops2(B2_PASO) &&
             banco_pasos2(B2_PASO) >= 1.0 && banco_pasos2(B2_PEQUE) >= 1.0);
    {
        /* las de un arranque suelto, como la carga de B2_INICIO: por vuelta
         * tienen que salir esas, exactas (todas las vueltas cuentan igual) */
        static Circuito c;
        static Red r;
        unsigned long o;
        banco_peor_sim(&c, &r);
        c.t_fin.v = 2e-3; c.t_fin.est = M_DATO; c.paso.v = 2e-6; c.paso.est = M_DATO;
        sim_ops_reset();
        sim_iniciar(&c, &r);
        o = sim_ops();
        sim_cortar();
        sprintf(det, "el arranque cuenta %.0f op. por vuelta, las de uno suelto (%lu); el punto de CA sin la topolog" S_ii "a, "
                "m" S_aa "s de 1000", banco_ops2(B2_INICIO), o);
        printf("    %s: %s\n", q, det);
        chk_cond(q, det, banco_ops2(B2_INICIO) == (double)o && o > 1000ul && banco_ops2(B2_CA) > 1000.0 && banco_cp15() == 0);
    }
    /* ui_dibuja_en pinta otra pantalla sin cambiar a ella */
    gfx_textos_reset();
#if ESPECIAL
    ESP_OO_DIBUJA_EN
#else
    ui_dibuja_en(P_MENU);
    i = gfx_texto_visto("Aprende") && !gfx_texto_visto("Banco de tiempos") && ui_pantalla() == P_BANCO;
    chk_cond(q, "ui_dibuja_en pinta el men" S_uu " y se queda en el banco", i && ve("(2 de 2)"));
#endif
    /* hecho, un tick de más no mide nada */
    {
        double m[B2_N], o[B2_N], a;
        for (i = 0; i < B2_N; i++) { m[i] = banco_ms2(i); o[i] = banco_ops2(i); }
        banco_tick();
        for (i = 0, bien = 1; i < B2_N; i++) {
            a = banco_ms2(i);  if (memcmp(&a, &m[i], sizeof a)) bien = 0;
            a = banco_ops2(i); if (memcmp(&a, &o[i], sizeof a)) bien = 0;
        }
        chk_cond(q, "hecho, un tick de m" S_aa "s no mide nada", bien && banco_hecho() && banco_en_pagina() == 1);
    }
    oo_vuelca("banco_hecho_p2");
    TECLAS(T_IZQ);
    i = banco_en_pagina() == 0 && ve("(1 de 2)") && ve("[izq/der] p" S_aa "gina");
    oo_vuelca("banco_hecho_p1");
    TECLAS(T_DER);
    chk_cond(q, "[izq] y [der] cambian de p" S_aa "gina, con su n" S_uu "mero y su pie",
             i && banco_en_pagina() == 1 && ve("(2 de 2)"));

    /* [enter] repite desde la primera; [esc] a mitad de la segunda la corta */
    TECLAS(T_ENTER);
    i = banco_en_pagina() == 0 && !banco_hecho();
    for (n = 0; n < BN_N; n++) { ui_dibuja(); ui_tick(); }
    esc_tras = 3;
    banco_salida(esc_falso);
    ui_tick();
    banco_salida(0);
    chk_cond(q, "[enter] repite desde la primera; [esc] a mitad de la segunda la cancela y se queda en ella",
             i && banco_hecho() && banco_cancelado() && banco_en_pagina() == 1 && banco_ms2(B2_DDIV) < 0.0);
    oo_vuelca("banco_p2_cancelado");
    TECLAS(T_ESC);
    chk_cond(q, "[esc] vuelve a la Ayuda y sin textos fuera de la pantalla",
             ui_pantalla() == P_AYUDA && gfx_desbordes() == 0);

    /* [esc] a mitad de la primera, con la segunda: hecho y cancelado en la
     * primera, y la segunda ya no se mide */
    TECLAS(T_LETRA('T'));
    for (n = 0; n < 2; n++) { ui_dibuja(); ui_tick(); }
    esc_tras = 3;
    banco_salida(esc_falso);
    ui_tick();
    banco_salida(0);
    for (n = 0; n < 3; n++) banco_tick();
    chk_cond(q, "[esc] a mitad de la primera la cancela ah" S_ii ", y la segunda no se mide",
             banco_hecho() && banco_cancelado() && banco_en_pagina() == 0 && !ui_animando() &&
             banco_ms(BN_EJEMPLO) > 0.0 && banco_ms(BN_PEOR) < 0.0 && banco_ms2(B2_DDIV) < 0.0 && ve("Cancelado"));
    TECLAS(T_DER);
    chk_cond(q, "y en la segunda, vac" S_ii "a, no se mide ninguna", banco_en_pagina() == 1 && ve("(2 de 2)") &&
             !ve("midiendo") && ve("Cancelado"));
    oo_vuelca("banco_p1_cancelado_p2");
    TECLAS(T_ESC);
    banco_extra(0);
}

static void test_opt_otros(void)
{
    printf("[75o] Optimizaci" S_oo "n (grupo otros): signos por niveles, potencias de 10 y d" S_ee "cadas, pasos, banco\n");
    oo_signos();
    oo_resolver();
    oo_numfmt();
    oo_pasos();
    oo_banco();
}
