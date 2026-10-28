/* --------------------------------------------------------------------------
 *  55. Etapa 4: los componentes de 3 patas en el modelo, el editor y el
 *  archivo (sin motor), y las fuentes controladas en el modelo
 *
 *  Un AO, un BJT o un MOSFET ocupa una caja: la envolvente de sus tres
 *  patas. Se prueban la geometría de las huellas, dónde caben en la rejilla,
 *  el inversor dibujado (la red que sale, los grados, los puntos interiores),
 *  los rechazos del editor, borrar y el espejo, circ_valido, el control de
 *  las controladas y el archivo en su versión 4, byte a byte.
 *  Se incluye en host_test.c (usa chk_cond, falla, azar, mismos,
 *  circuito_sano, limpia_mis, carpeta_mis, lector_e3 y w_mag).
 * ----------------------------------------------------------------------- */

/* El inversor de la especificación (§6.2): E1 1 V, R1 1 kΩ, U1, Rf 10 kΩ y
 * RL 2 kΩ, con la tierra en (4,5) */
static void e4_inversor(Circuito *c)
{
    int k, x;
    circ_init(c, "inversor");
    k = circ_elemento(c, EL_FUENTE, 1, 4, DIR_ARR);
    c->el[k].val.v = 1.0; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 2, 2, DIR_DER);
    c->el[k].val.v = 1e3; c->el[k].val.est = M_DATO;
    circ_elemento(c, EL_AO, 4, 2, DIR_DER);
    k = circ_elemento(c, EL_RES, 4, 1, DIR_DER);
    c->el[k].val.v = 10e3; c->el[k].val.est = M_DATO;
    k = circ_elemento(c, EL_RES, 7, 3, DIR_ABA);
    c->el[k].val.v = 2e3; c->el[k].val.est = M_DATO;
    circ_cable(c, 1, 3, DIR_ARR); circ_cable(c, 1, 2, DIR_DER);
    circ_cable(c, 3, 2, DIR_DER);
    circ_cable(c, 4, 2, DIR_ARR);
    circ_cable(c, 5, 1, DIR_DER); circ_cable(c, 6, 1, DIR_DER);
    circ_cable(c, 7, 1, DIR_ABA); circ_cable(c, 7, 2, DIR_ABA);
    circ_cable(c, 6, 3, DIR_DER);
    circ_cable(c, 4, 4, DIR_ABA);
    circ_cable(c, 1, 4, DIR_ABA);
    for (x = 1; x < 7; x++) circ_cable(c, x, 5, DIR_DER);
    circ_cable(c, 7, 4, DIR_ABA);
    c->tierra_x = 4; c->tierra_y = 5;
}

/* Un circuito con un AO y un BJT (para los archivos rotos de [13]) */
static void e4_circ_3patas(Circuito *c)
{
    int k;
    e4_inversor(c);
    k = circ_elemento(c, EL_BJT, 1, 0, DIR_DER);
    if (k >= 0) c->el[k].par[QB_BF - Q_P1].v = 150.0, c->el[k].par[QB_BF - Q_P1].est = M_DATO;
    k = circ_elemento(c, EL_DIODO, 8, 0, DIR_ABA);
    (void)k;
}

/* El archivo CESP de la versión 4 escrito a mano, byte a byte (§2.5): secciones
 * acumulativas del elemento (la 2 con los par y la 3 con xc, yc en los de 3
 * patas) y la sección 2 del bloque global con las sondas */
static void w_magc(FILE *f, const Mag *m)
{
    unsigned char b[8];
    memcpy(b, &m->v, 8);
    fwrite(b, 1, 8, f);
    fputc(m->est, f);
}

static void escribe_cesp4(const char *ruta, const Circuito *c)
{
    FILE *f = fopen(ruta, "wb");
    int x, y, k, i, g2;
    if (!f) return;
    fwrite("CESP", 1, 4, f);
    fputc(4, f);                                       /* versión */
    fputc(REJ_COLS, f); fputc(REJ_FILAS, f);
    for (i = 0; i < 24; i++) fputc(c->nombre[i], f);
    for (y = 0; y < REJ_FILAS; y++) for (x = 0; x < REJ_COLS - 1; x++) fputc(c->h[y][x], f);
    for (y = 0; y < REJ_FILAS - 1; y++) for (x = 0; x < REJ_COLS; x++) fputc(c->v[y][x], f);
    g2 = c->sonda[0].tipo != SONDA_NO || c->sonda[1].tipo != SONDA_NO || c->t_fin.est || c->paso.est;
    fputc(g2 ? 53 : 25, f);                            /* bloque global */
    fputc(c->tierra_x & 0xFF, f); fputc(c->tierra_y & 0xFF, f);
    fputc(c->term_x[0] & 0xFF, f); fputc(c->term_y[0] & 0xFF, f);
    fputc(c->term_x[1] & 0xFF, f); fputc(c->term_y[1] & 0xFF, f);
    fputc(c->conv, f);
    w_mag(f, &c->frec);
    if (g2) {
        for (i = 0; i < 2; i++) {
            fputc(c->sonda[i].tipo, f); fputc(c->sonda[i].x & 0xFF, f); fputc(c->sonda[i].y & 0xFF, f);
            fputc(c->sonda[i].el & 0xFF, f); fputc(c->sonda[i].sg & 0xFF, f);
        }
        w_magc(f, &c->t_fin); w_magc(f, &c->paso);
    }
    fputc(c->nel, f);
    for (k = 0; k < c->nel; k++) {
        const Elemento *e = &c->el[k];
        int tres = circ_npatas(e) == 3, dos = tres, n = 85;
        for (i = 0; i < NPAR && !dos; i++) if (e->par[i].est || e->par[i].v != 0.0) dos = 1;
        if (dos) n += 54;
        if (tres) n += 2;
        fputc(n, f);
        fputc(e->tipo, f); fputc(e->xa, f); fputc(e->ya, f); fputc(e->xb, f); fputc(e->yb, f);
        for (i = 0; i < 6; i++) fputc(e->etiq[i], f);
        w_mag(f, &e->val); w_mag(f, &e->V); w_mag(f, &e->I); w_mag(f, &e->aux);
        fputc(e->variante, f);
        fputc(e->ref & 0xFF, f);
        if (dos) for (i = 0; i < NPAR; i++) w_magc(f, &e->par[i]);
        if (tres) { fputc(e->xc, f); fputc(e->yc, f); }
    }
    fclose(f);
}

/* Bytes de un archivo, o -1 */
static long e4_tam(const char *ruta)
{
    FILE *f = fopen(ruta, "rb");
    long n;
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fclose(f);
    return n;
}

static int e4_mismos_bytes(const char *r1, const char *r2)
{
    FILE *a = fopen(r1, "rb"), *b = fopen(r2, "rb");
    int ca, cb, igual = a && b;
    while (igual) {
        ca = fgetc(a); cb = fgetc(b);
        if (ca != cb) igual = 0;
        if (ca == EOF) break;
    }
    if (a) fclose(a);
    if (b) fclose(b);
    return igual;
}

/* Cuántos segmentos con elemento k hay en la rejilla */
static int e4_segs_de(const Circuito *c, int k)
{
    int x, y, n = 0;
    for (y = 0; y < REJ_FILAS; y++)
        for (x = 0; x < REJ_COLS; x++) {
            if (x < REJ_COLS - 1 && c->h[y][x] == SEG_EL0 + k) n++;
            if (y < REJ_FILAS - 1 && c->v[y][x] == SEG_EL0 + k) n++;
        }
    return n;
}

static void test_e4_modelo(void)
{
    static Circuito c, d, v;
    static Red r;
    static const int T3[3] = { EL_AO, EL_BJT, EL_MOS };
    char ruta[260], ruta2[260];
    int t, dd, esp, k, x, y, n, i, ok;
    CabeInfo ci;

    printf("[55] Etapa 4: los 3 patas en el modelo, el editor y el archivo\n");

    /* ---- Geometría (§3.N.1) ---- */
    circ_init(&c, "g");
    k = circ_elemento(&c, EL_AO, 4, 2, DIR_DER);
    chk_cond("huella", "AO con " S_RARR " en (4,2): - (4,2), + (4,4), salida (6,3)", k == 0 &&
             c.el[0].xb == 4 && c.el[0].yb == 2 && c.el[0].xa == 4 && c.el[0].ya == 4 && c.el[0].xc == 6 && c.el[0].yc == 3);
    circ_voltear(&c, 0);
    chk_cond("huella", "con espejo: + (4,2), - (4,4)", c.el[0].xa == 4 && c.el[0].ya == 2 && c.el[0].xb == 4 && c.el[0].yb == 4 &&
             c.el[0].xc == 6 && c.el[0].yc == 3 && circ_valido(&c));
    circ_init(&c, "g");
    k = circ_elemento(&c, EL_BJT, 4, 3, DIR_ABA);
    chk_cond("huella", "BJT con " S_DARR " en (4,3): C (3,4), E (5,4), B (4,3)", k == 0 &&
             c.el[0].xa == 3 && c.el[0].ya == 4 && c.el[0].xb == 5 && c.el[0].yb == 4 && c.el[0].xc == 4 && c.el[0].yc == 3);
    ok = 1;
    for (t = 0; t < 3; t++)
        for (dd = 0; dd < 4; dd++)
            for (esp = 0; esp < 2; esp++) {
                const Huella *h = tipo_info(T3[t])->huella;
                Elemento e;
                int px, py, x0, y0, x1, y1, d2, e2, ox, oy, segs;
                memset(&e, 0, sizeof e);
                e.tipo = (unsigned char)T3[t];
                circ_huella_pt(T3[t], dd, esp, 4, 3, h->pu[0], h->pv[0], &px, &py); e.xa = (unsigned char)px; e.ya = (unsigned char)py;
                circ_huella_pt(T3[t], dd, esp, 4, 3, h->pu[1], h->pv[1], &px, &py); e.xb = (unsigned char)px; e.yb = (unsigned char)py;
                circ_huella_pt(T3[t], dd, esp, 4, 3, h->pu[2], h->pv[2], &px, &py); e.xc = (unsigned char)px; e.yc = (unsigned char)py;
                circ_caja(&e, &x0, &y0, &x1, &y1);
                segs = (x1 - x0) * (y1 - y0 + 1) + (x1 - x0 + 1) * (y1 - y0);
                if (segs != (T3[t] == EL_AO ? 12 : 7)) ok = 0;
                if (!circ_orienta(&e, &d2, &e2, &ox, &oy) || d2 != dd || e2 != esp) ok = 0;
                circ_pata(&e, h->ancla, &px, &py);
                if (ox != 4 || oy != 3 || (!esp && (px != 4 || py != 3))) ok = 0;
            }
    chk_cond("huella", "3 huellas " S_TIMES " 4 flechas " S_TIMES " espejo: caja de 12 o 7 segmentos y circ_orienta", ok);

    /* ---- En la rejilla vacía: 160 colocaciones del AO y 186 del transistor ---- */
    circ_init(&c, "vacío");
    {
        int por_d[2][4], tot[2] = { 0, 0 };
        memset(por_d, 0, sizeof por_d);
        for (t = 0; t < 2; t++)
            for (dd = 0; dd < 4; dd++)
                for (y = 0; y < REJ_FILAS; y++)
                    for (x = 0; x < REJ_COLS; x++)
                        if (circ_cabe(&c, t ? EL_BJT : EL_AO, x, y, dd, &ci) == CM_OK) { por_d[t][dd]++; tot[t]++; }
        chk_cond("rejilla vacía", "el AO cabe en 160 sitios (40 por flecha)", tot[0] == 160 &&
                 por_d[0][0] == 40 && por_d[0][1] == 40 && por_d[0][2] == 40 && por_d[0][3] == 40);
        chk_cond("rejilla vacía", "el transistor en 186 (45, 48, 45 y 48)", tot[1] == 186 &&
                 por_d[1][DIR_DER] == 45 && por_d[1][DIR_IZQ] == 45 && por_d[1][DIR_ABA] == 48 && por_d[1][DIR_ARR] == 48);
    }

    /* ---- No cabe ---- */
    chk_cond("no cabe", "AO " S_RARR " en (8,2): CM_FUERA", circ_cabe(&c, EL_AO, 8, 2, DIR_DER, &ci) == CM_FUERA);
    chk_cond("no cabe", "AO " S_RARR " en (4,5): CM_FUERA", circ_cabe(&c, EL_AO, 4, 5, DIR_DER, &ci) == CM_FUERA);
    chk_cond("no cabe", "BJT " S_RARR " en (4,0): CM_FUERA", circ_cabe(&c, EL_BJT, 4, 0, DIR_DER, &ci) == CM_FUERA);
    chk_cond("no cabe", "BJT " S_RARR " en (9,3): CM_FUERA", circ_cabe(&c, EL_BJT, 9, 3, DIR_DER, &ci) == CM_FUERA);
    k = circ_elemento(&c, EL_AO, 4, 2, DIR_DER);
    chk_cond("no cabe", "con U1, otro AO " S_RARR " en (5,3): CM_OCUPADO en (5,3) por U1",
             circ_cabe(&c, EL_AO, 5, 3, DIR_DER, &ci) == CM_OCUPADO && ci.x == 5 && ci.y == 3 && ci.el == k);
    chk_cond("no cabe", "y circ_elemento no toca nada", circ_elemento(&c, EL_AO, 5, 3, DIR_DER) == -1 && c.nel == 1);
    circ_init(&c, "cable");
    circ_cable(&c, 5, 1, DIR_ABA);
    chk_cond("no cabe", "el cable (5,1)-(5,2): CM_OCUPADO en (5,2) con el = -1",
             circ_cabe(&c, EL_AO, 4, 2, DIR_DER, &ci) == CM_OCUPADO && ci.x == 5 && ci.y == 2 && ci.el == -1);
    circ_init(&c, "tierra");
    c.tierra_x = 5; c.tierra_y = 4;
    chk_cond("no cabe", "la tierra en (5,4): CM_MARCA 'G'", circ_cabe(&c, EL_AO, 4, 2, DIR_DER, &ci) == CM_MARCA && ci.que == 'G');
    circ_init(&c, "patas");
    circ_cable(&c, 3, 2, DIR_DER); circ_cable(&c, 4, 1, DIR_ABA);
    chk_cond("no cabe", "los cables (3,2)-(4,2) y (4,1)-(4,2) llegan a la pata -: CM_OK",
             circ_cabe(&c, EL_AO, 4, 2, DIR_DER, &ci) == CM_OK && circ_elemento(&c, EL_AO, 4, 2, DIR_DER) == 0 && circ_valido(&c));

    /* ---- El inversor en la rejilla ---- */
    e4_inversor(&c);
    chk_cond("inversor", "5 elementos, válido y sano", c.nel == 5 && circ_valido(&c) && circuito_sano(&c) == 0);
    red_extraer(&c, &r);
    chk_cond("inversor", "nn = 4", r.nn == 4);
    {
        int A = r.punto[1 * REJ_COLS + 4], B = r.punto[1 * REJ_COLS + 5], C = r.punto[2 * REJ_COLS + 1], D = r.punto[5 * REJ_COLS + 4];
        chk_cond("inversor", "A (el -), B (la salida), C (la entrada) y D (tierra) en orden de lectura",
                 A == 0 && B == 1 && C == 2 && D == 3 && r.punto[2 * REJ_COLS + 4] == A && r.punto[2 * REJ_COLS + 3] == A &&
                 r.punto[3 * REJ_COLS + 6] == B && r.punto[3 * REJ_COLS + 7] == B && r.punto[2 * REJ_COLS + 2] == C);
        chk_cond("inversor", "U1: a = D, b = A, c = B", r.a[2] == D && r.b[2] == A && r.c[2] == B);
        chk_cond("inversor", "red_ref_de(B) = D (la tierra)", red_ref_de(&r, B) == D && red_ref_propia(&c, &r, B));
        chk_cond("inversor", "red_grado: A 3, B 3, C 2, D 3", red_grado(&r, A) == 3 && red_grado(&r, B) == 3 &&
                 red_grado(&r, C) == 2 && red_grado(&r, D) == 3);
        chk_cond("inversor", "los puntos interiores no son de ningún nodo",
                 r.punto[2 * REJ_COLS + 5] == -1 && r.punto[2 * REJ_COLS + 6] == -1 && r.punto[3 * REJ_COLS + 4] == -1 &&
                 r.punto[3 * REJ_COLS + 5] == -1 && r.punto[4 * REJ_COLS + 5] == -1 && r.punto[4 * REJ_COLS + 6] == -1);
        chk_cond("inversor", "los de dos bornes, sin pata c", r.c[0] == -1 && r.c[1] == -1 && r.c[3] == -1 && r.c[4] == -1);
    }
    chk_cond("inversor", "circ_grado: (4,2) 3, (6,3) 2, (4,4) 2, (5,3) 0, (7,3) 3",
             circ_grado(&c, 4, 2) == 3 && circ_grado(&c, 6, 3) == 2 && circ_grado(&c, 4, 4) == 2 &&
             circ_grado(&c, 5, 3) == 0 && circ_grado(&c, 7, 3) == 3);
    chk_cond("inversor", "circ_interior: (5,3) es de U1, (4,2) no (es pata)", circ_interior(&c, 5, 3) == 2 && circ_interior(&c, 4, 2) == -1);

    /* ---- Rechazos del editor ---- */
    d = c;
    chk_cond("rechazos", "circ_cable(5,1," S_DARR ") da 0 y el circuito no cambia", circ_cable(&d, 5, 1, DIR_ABA) == 0 && mismos(&c, &d));
    chk_cond("rechazos", "circ_cable(4,2," S_RARR ") da 0 (segmento de la caja)", circ_cable(&d, 4, 2, DIR_DER) == 0 && mismos(&c, &d));
    chk_cond("rechazos", "circ_elemento(R, 6,2," S_RARR ") da -1 (toca un punto interior)", circ_elemento(&d, EL_RES, 6, 2, DIR_DER) == -1 && mismos(&c, &d));
    k = circ_elemento(&d, EL_RES, 7, 3, DIR_IZQ);
    chk_cond("rechazos", "circ_elemento(R, 7,3," S_LARR ") sustituye el cable (6,3)-(7,3) (está fuera)",
             k == 5 && circ_seg(&d, 7, 3, DIR_IZQ) == SEG_EL0 + 5 && circ_valido(&d));

    /* ---- Borrar y espejo ---- */
    d = c;
    circ_quitar(&d, 5, 2, DIR_ABA);
    chk_cond("borrar", "quitar un segmento de la caja borra U1 entero", d.nel == 4 && e4_segs_de(&d, 3) == 1 && circ_valido(&d));
    {
        int vacios = 1, y2, x2;
        for (y2 = 2; y2 <= 4; y2++)
            for (x2 = 4; x2 <= 6; x2++) {
                if (x2 < 6 && d.h[y2][x2] != SEG_VACIO) vacios = 0;
                if (y2 < 4 && d.v[y2][x2] != SEG_VACIO) vacios = 0;
            }
        chk_cond("borrar", "los 12 segmentos de la caja quedan vacíos", vacios);
    }
    chk_cond("borrar", "los cables de fuera, intactos", circ_seg(&d, 3, 2, DIR_DER) == SEG_CABLE && circ_seg(&d, 4, 2, DIR_ARR) == SEG_CABLE &&
             circ_seg(&d, 6, 3, DIR_DER) == SEG_CABLE && circ_seg(&d, 4, 4, DIR_ABA) == SEG_CABLE);
    chk_cond("borrar", "Rf pasa del índice 3 al 2", d.el[2].tipo == EL_RES && d.el[2].val.v == 10e3 && circ_seg(&d, 4, 1, DIR_DER) == SEG_EL0 + 2);
    d = c;
    circ_voltear(&d, 2);
    red_extraer(&d, &r);
    chk_cond("espejo", "+ en (4,2) y - en (4,4); en la red a = A y b = D",
             d.el[2].xa == 4 && d.el[2].ya == 2 && d.el[2].xb == 4 && d.el[2].yb == 4 && r.a[2] == 0 && r.b[2] == 3 && circ_valido(&d));

    /* ---- circ_valido rechaza ---- */
    d = c; d.h[2][4] = SEG_CABLE;
    chk_cond("válido", "un segmento de la caja cambiado a cable", !circ_valido(&d));
    d = c; d.el[2].yc++;
    chk_cond("válido", "yc corrido en 1", !circ_valido(&d));
    d = c; d.el[2].xa = 5;
    chk_cond("válido", "unas patas que no forman una huella", !circ_valido(&d));
    d = c; d.el[1].xc = 1;
    chk_cond("válido", "una R con xc = 1", !circ_valido(&d));
    d = c; d.tierra_x = 5; d.tierra_y = 3;
    chk_cond("válido", "la tierra en un punto interior", !circ_valido(&d));
    d = c; d.term_x[0] = 6; d.term_y[0] = 2;
    chk_cond("válido", "un terminal en un punto interior", !circ_valido(&d));
    d = c; d.sonda[0].tipo = SONDA_I; d.sonda[0].el = 2; d.sonda[0].sg = 1;
    chk_cond("válido", "una sonda I sobre un 3 patas", !circ_valido(&d));
    d = c; d.sonda[0].tipo = SONDA_V; d.sonda[0].el = 2; d.sonda[0].sg = 1;
    chk_cond("válido", "una sonda V sobre un 3 patas", !circ_valido(&d));
    {
        /* dos cajas que se pisan, armadas a mano */
        circ_init(&d, "pisan");
        circ_elemento(&d, EL_AO, 2, 1, DIR_DER);
        k = d.nel++;
        d.el[k] = d.el[0];
        d.el[k].etiq[1] = '2';
        d.el[k].xa++; d.el[k].xb++; d.el[k].xc++;
        chk_cond("válido", "dos cajas que se pisan", !circ_valido(&d));
    }
    circ_init(&d, "variantes");
    k = circ_elemento(&d, EL_BJT, 4, 3, DIR_ABA);
    d.el[k].variante = 2;
    chk_cond("válido", "variante 2 en un BJT", !circ_valido(&d));
    circ_init(&d, "variantes");
    k = circ_elemento(&d, EL_DIODO, 1, 1, DIR_DER);
    chk_cond("válido", "el diodo nuevo sale con V" S_GAMMA " y es válido", d.el[k].variante == DIO_VG && circ_valido(&d));
    d.el[k].variante = 5;
    chk_cond("válido", "variante 5 en un diodo", !circ_valido(&d));
    circ_init(&d, "variantes");
    k = circ_elemento(&d, EL_ZENER, 1, 1, DIR_DER);
    d.el[k].variante = 3;
    chk_cond("válido", "variante 3 en un zener", !circ_valido(&d));
    circ_init(&d, "variantes");
    k = circ_elemento(&d, EL_LED, 1, 1, DIR_DER);
    d.el[k].variante = 6;
    chk_cond("válido", "variante 6 en un LED", !circ_valido(&d));
    d = c;
    k = circ_elemento(&d, EL_EDEP, 8, 1, DIR_ABA);
    d.el[k].ref = (signed char)k;
    chk_cond("válido", "una EDEP con ref = ella misma", !circ_valido(&d));
    d.el[k].ref = 2;
    chk_cond("válido", "una EDEP controlada por un AO", !circ_valido(&d));
    d.el[k].ref = 1;
    chk_cond("válido", "una EDEP controlada por R1: válida", circ_valido(&d));
    d = c;
    k = circ_elemento(&d, EL_AO, 0, 0, DIR_DER);
    chk_cond("válido", "otro AO en la esquina cabe", k < 0 || circ_valido(&d));

    /* ---- Controladas ---- */
    circ_init(&d, "ctl");
    circ_elemento(&d, EL_FUENTE, 0, 1, DIR_ARR);
    circ_elemento(&d, EL_RES, 0, 0, DIR_DER);
    circ_elemento(&d, EL_RES, 1, 0, DIR_ABA);
    k = circ_elemento(&d, EL_EDEP, 2, 1, DIR_ARR);
    chk_cond("control", "una controlada nueva sale sin control", d.el[k].ref == -1);
    n = circ_control_sig(&d, k, +1);
    chk_cond("control", "desde sin control, [+] da E1 (0)", n == 0);
    d.el[k].ref = (signed char)n; n = circ_control_sig(&d, k, +1);
    chk_cond("control", "luego R1 (1)", n == 1);
    d.el[k].ref = (signed char)n; n = circ_control_sig(&d, k, +1);
    chk_cond("control", "luego R2 (2)", n == 2);
    d.el[k].ref = (signed char)n; n = circ_control_sig(&d, k, +1);
    chk_cond("control", "y vuelve a E1 (0), sin pasar por ella", n == 0);
    d.el[k].ref = -1;
    chk_cond("control", "hacia atrás desde sin control: R2 (2)", circ_control_sig(&d, k, -1) == 2);
    circ_init(&v, "sola");
    k = circ_elemento(&v, EL_EDEP, 2, 1, DIR_ARR);
    chk_cond("control", "con E2 sola, -1 (0 sería E1)", circ_control_sig(&v, k, +1) == -1 && circ_control_sig(&v, k, -1) == -1);
    v = d; v.el[3].ref = 1;
    circ_quitar(&v, 0, 0, DIR_DER);
    chk_cond("control", "borrar R1 con ref = R1 deja ref = -1", v.el[2].ref == -1 && circ_valido(&v));
    v = d; v.el[3].ref = 1;
    circ_quitar(&v, 0, 1, DIR_ARR);
    chk_cond("control", "borrar E1 con ref = R1 (1) deja ref = 0", v.el[2].ref == 0 && circ_valido(&v));
    /* circ_ctl_motivo */
    d.el[3].ref = -1;
    chk_cond("ctl_motivo", "sin control: CTL_FALTA", circ_ctl_motivo(&d, 3) == CTL_FALTA);
    d.el[3].ref = 3;
    chk_cond("ctl_motivo", "ref = ella: CTL_SI_MISMA", circ_ctl_motivo(&d, 3) == CTL_SI_MISMA);
    {
        int ku, kd, kj, ka;
        v = d;
        ku = circ_elemento(&v, EL_AO, 4, 2, DIR_DER);
        kd = circ_elemento(&v, EL_DIODO, 0, 4, DIR_DER);
        kj = circ_elemento(&v, EL_JDEP, 2, 4, DIR_DER);
        ka = circ_elemento(&v, EL_AMP, 8, 0, DIR_ABA);
        chk_cond("ctl_motivo", "los de la prueba caben", ku >= 0 && kd >= 0 && kj >= 0 && ka >= 0);
        v.el[3].ref = (signed char)ku;
        chk_cond("ctl_motivo", "hacia U1: CTL_3PATAS", circ_ctl_motivo(&v, 3) == CTL_3PATAS);
        v.el[3].variante = DEP_I; v.el[3].ref = (signed char)kd;
        chk_cond("ctl_motivo", "por la I de un diodo: CTL_I_NO", circ_ctl_motivo(&v, 3) == CTL_I_NO);
        v.el[3].ref = (signed char)kj;
        chk_cond("ctl_motivo", "por la I de una J controlada: CTL_I_NO", circ_ctl_motivo(&v, 3) == CTL_I_NO);
        v.el[3].variante = DEP_V; v.el[3].ref = (signed char)kd;
        chk_cond("ctl_motivo", "por la V de un diodo: CTL_OK", circ_ctl_motivo(&v, 3) == CTL_OK);
        v.el[3].variante = DEP_I;
        v.el[3].ref = 1;
        chk_cond("ctl_motivo", "por la I de una R: CTL_OK", circ_ctl_motivo(&v, 3) == CTL_OK);
        v.el[3].ref = 0;
        chk_cond("ctl_motivo", "por la I de una E: CTL_OK", circ_ctl_motivo(&v, 3) == CTL_OK);
        v.el[3].ref = (signed char)ka;
        chk_cond("ctl_motivo", "por la I de un A: CTL_OK", circ_ctl_motivo(&v, 3) == CTL_OK);
        k = circ_elemento(&v, EL_CORR, 8, 3, DIR_ABA);
        v.el[3].ref = (signed char)k;
        chk_cond("ctl_motivo", "por la I de una J: CTL_OK", k >= 0 && circ_ctl_motivo(&v, 3) == CTL_OK);
    }

    /* ---- Archivo: versión 4 ---- */
    limpia_mis();
    sprintf(ruta, "%s/ces_01.tns", carpeta_mis);
    sprintf(ruta2, "%s/cesp4_a_mano.tns", carpeta_mis);
    chk_cond("archivo v4", "el inversor se guarda", arch_guardar(0, &c));
    {
        FILE *f = fopen(ruta, "rb");
        unsigned char cab[5];
        int ver = f && fread(cab, 1, 5, f) == 5 ? cab[4] : -1;
        if (f) fclose(f);
        chk_cond("archivo v4", "en la versión 4", ver == 4);
    }
    chk_cond("archivo v4", "de 667 bytes (85, 85, 141, 85 y 85; global de 25)", e4_tam(ruta) == 667);
    escribe_cesp4(ruta2, &c);
    chk_cond("archivo v4", "escrito a mano, byte a byte igual", e4_mismos_bytes(ruta, ruta2));
    chk_cond("archivo v4", "se abre igual", arch_cargar(0, &d) && mismos(&c, &d));
    chk_cond("archivo v4", "el lector de la Etapa 3 lo rechaza", !lector_e3(ruta));
    v = c;
    v.sonda[0].tipo = SONDA_NODO; v.sonda[0].x = 6; v.sonda[0].y = 3; v.sonda[0].el = -1; v.sonda[0].sg = 0;
    chk_cond("archivo v4", "con una sonda de nodo en la salida, válido", circ_valido(&v));
    arch_guardar(0, &v);
    chk_cond("archivo v4", "695 bytes (global de 53)", e4_tam(ruta) == 695);
    escribe_cesp4(ruta2, &v);
    chk_cond("archivo v4", "y byte a byte igual al escrito a mano", e4_mismos_bytes(ruta, ruta2));
    chk_cond("archivo v4", "y se abre igual", arch_cargar(0, &d) && mismos(&v, &d));
    /* con espejo, PNP, canal P y controladas */
    v = c;
    circ_voltear(&v, 2);
    k = circ_elemento(&v, EL_BJT, 1, 0, DIR_DER);
    if (k >= 0) v.el[k].variante = BJT_PNP;
    k = circ_elemento(&v, EL_MOS, 8, 0, DIR_ABA);
    if (k >= 0) { v.el[k].variante = MOS_CP; v.el[k].par[QM_VT - Q_P1].v = 1.5; v.el[k].par[QM_VT - Q_P1].est = M_DATO; }
    k = circ_elemento(&v, EL_JDEP, 0, 5, DIR_ARR);
    if (k >= 0) { v.el[k].variante = DEP_I; v.el[k].ref = 1; v.el[k].val.v = -2.5; v.el[k].val.est = M_DATO; }
    chk_cond("archivo v4", "espejo, PNP, canal P y una J controlada: válido", circ_valido(&v));
    chk_cond("archivo v4", "se guarda y se abre igual", arch_guardar(0, &v) && arch_cargar(0, &d) && mismos(&v, &d));
    escribe_cesp4(ruta2, &v);
    chk_cond("archivo v4", "byte a byte igual al escrito a mano", e4_mismos_bytes(ruta, ruta2));
    /* un circuito de hoy sigue en su versión (no pasa a la 4) */
    ejemplo_cargar(0, &d);
    arch_guardar(0, &d);
    chk_cond("archivo v4", "un circuito sin tipos nuevos sigue en la versión 1", lector_e3(ruta));
    arch_borrar(0);
    remove(ruta2);

    /* ---- Aleatorio del API: 20 000 operaciones ---- */
    {
        static const int TT[] = { EL_RES, EL_FUENTE, EL_CORR, EL_COND, EL_DIODO, EL_ZENER, EL_LED, EL_EDEP, EL_JDEP,
                                  EL_AO, EL_BJT, EL_MOS, EL_AO, EL_BJT, EL_MOS, EL_VOLT, EL_AMP };
        const int NTT = (int)(sizeof TT / sizeof TT[0]);
        int op, malos = 0, cabe_mal = 0, red_mal = 0, arch_mal = 0;
        unsigned sem0 = semilla;
        semilla = 5555u;
        circ_init(&c, "azar");
        for (op = 0; op < 20000; op++) {
            int q = (int)azar(10), t2 = TT[azar((unsigned)NTT)];
            x = (int)azar(REJ_COLS); y = (int)azar(REJ_FILAS); dd = (int)azar(4);
            if (c.nel > 20 && q < 6) q = 7;                       /* que no se llene del todo */
            if (q < 4) {
                int cabe = tipo_info(t2)->huella ? circ_cabe(&c, t2, x, y, dd, &ci) == CM_OK : -1;
                k = circ_elemento(&c, (TipoEl)t2, x, y, dd);
                if (cabe >= 0 && cabe != (k >= 0)) cabe_mal++;
                if (k >= 0 && (t2 == EL_EDEP || t2 == EL_JDEP)) c.el[k].ref = (signed char)circ_control_sig(&c, k, +1);
            } else if (q < 6) circ_cable(&c, x, y, dd);
            else if (q < 8) circ_quitar(&c, x, y, dd);
            else if (q == 8) { if (c.nel) circ_voltear(&c, (int)azar((unsigned)c.nel)); }
            else if (circ_grado(&c, x, y) > 0 && circ_interior(&c, x, y) < 0) { c.tierra_x = (signed char)x; c.tierra_y = (signed char)y; }
            if (circuito_sano(&c) != 0 || !circ_valido(&c)) { if (!malos) printf("    op %d: %s\n", op, circuito_sano(&c)); malos++; break; }
            red_extraer(&c, &r);
            if (r.nn > MAX_NODOS) red_mal++;
            for (k = 0; k < c.nel; k++) {
                if (r.a[k] < 0 || r.b[k] < 0) red_mal++;
                if (circ_npatas(&c.el[k]) == 3 && r.c[k] < 0) red_mal++;
            }
            for (i = 0; i < REJ_PUNTOS; i++)
                if (circ_interior(&c, i % REJ_COLS, i / REJ_COLS) >= 0 && r.punto[i] != -1) red_mal++;
            if (op % 100 == 99) {
                if (!arch_guardar(0, &c) || !arch_cargar(0, &d) || !mismos(&c, &d)) arch_mal++;
            }
        }
        arch_borrar(0);
        semilla = sem0;
        chk_cond("azar 3 patas", "20 000 operaciones: el circuito siempre sano y válido", malos == 0);
        chk_cond("azar 3 patas", "circ_cabe == CM_OK si y solo si circ_elemento lo pone", cabe_mal == 0);
        chk_cond("azar 3 patas", "toda pata con nodo, los interiores sin él, nn <= 70", red_mal == 0);
        chk_cond("azar 3 patas", "cada 100, guardar y abrir da lo mismo", arch_mal == 0);
    }
}
