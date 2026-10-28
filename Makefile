# ============================================================================
#  Makefile para Ndless (TI-Nspire CX II CAS)  ->  genera hakaru_ces.tns
#  Plantilla del Ndless SDK (genzehn + make-prg).
#
#  Con el SDK en el PATH (en WSL, por ejemplo):
#    export PATH="/opt/Ndless/ndless-sdk/toolchain/install/bin:/opt/Ndless/ndless-sdk/bin:$PATH"
#    make
#  o, sin tocar el PATH:  sh build_tns.sh
# ============================================================================
DEBUG = FALSE

# La versión especial (ESPECIAL=1) es privada: sus archivos viven en
# especial/, que no se publica. Sin esa carpeta solo hay la pública, la de
# siempre de este repositorio:  make
ESPECIAL ?= 0

# El turbo (turbo.h), experimental y FUERA de la versión: ninguna fuente de
# hoy respalda escribir solo 0x90140030 (la revisión final, 2026-09-29).
# Sin él, main.c no le da el hardware: turbo_hay() = 0, sin la fila de
# Controles y sin tocar ningún registro. Para probarlo en la calculadora,
# bajo tu riesgo:  make clean && make TURBO=1
TURBO ?= 0

GCC = nspire-gcc
LD  = nspire-ld
GENZEHN = genzehn

# -DNDLESS activa lcd_init/lcd_blit y desactiva el volcado a BMP.
# El código está en UTF-8; las cadenas llegan a la pantalla en Latin-1 (gfx.h)
GCCFLAGS  = -Wall -W -Wno-missing-field-initializers -marm -Wno-unused-parameter -DNDLESS -finput-charset=UTF-8 -fexec-charset=ISO-8859-1
# La coma flotante rápida del ARM (cf_arm.c): la división, la raíz y las
# comparaciones de double, exactas (IEEE, los mismos bits que libgcc y
# newlib), en lugar de las de la biblioteca: --wrap lleva a ellas todas las
# llamadas, también las de la libm (exp, pow, hypot...). Sin CF_WRAP, cf_arm.o
# no enlaza (llama a las __real_ de la biblioteca para 0, Inf, NaN...)
CF_WRAP   = -Wl,--wrap=__aeabi_ddiv,--wrap=__ieee754_sqrt,--wrap=__aeabi_dcmpeq,--wrap=__aeabi_dcmplt,--wrap=__aeabi_dcmple,--wrap=__aeabi_dcmpge,--wrap=__aeabi_dcmpgt
LDFLAGS   = $(CF_WRAP)
ZEHNFLAGS = --name "Hakaru CES"

ifeq ($(DEBUG),FALSE)
	GCCFLAGS += -Os
else
	GCCFLAGS += -O0 -g
endif

GCCFLAGS += -DESPECIAL=$(ESPECIAL) -DHAKARU_TURBO=$(TURBO)

# La especial: sus .c y .h, en especial/ (ver especial/LEEME.md)
ESP_O =
ifeq ($(ESPECIAL),1)
	include especial/especial.mk
endif

EXE  = hakaru_ces
# El orden de los objetos es el orden del código en el .tns: ESP_O va donde
# siempre han ido los suyos, para que la especial no cambie
OBJS = main.o tipos.o lu.o cx.o luc.o ca.o calca.o bode.o fasorial.o trazador.o ondas.o sim.o nolin.o barrido.o curva.o osc.o biblioteca.o gfx.o font6x8.o font6x12.o numfmt.o pasos.o algebra.o circuito.o red.o resuelve.o arbol.o proced.o esquema.o ejemplos.o lecciones.o archivo.o turbo.o $(ESP_O) kanji_img.o banco.o entrada.o bucle.o ui.o
DEPS = tipos.h lu.h cx.h luc.h ca.h calca.h bode.h fasorial.h trazador.h ondas.h sim.h nolin.h barrido.h curva.h osc.h biblioteca.h gfx.h font.h numfmt.h pasos.h algebra.h circuito.h red.h resuelve.h arbol.h proced.h esquema.h ejemplos.h lecciones.h archivo.h kanji_img.h app.h banco.h entrada.h bucle.h ui.h turbo.h $(ESP_H)
DISTDIR = .
vpath %.tns $(DISTDIR)
vpath %.elf $(DISTDIR)
# la coma flotante rápida (cf_arm.c, arriba): aparte, para no tocar la lista
OBJS += cf_arm.o
# los capacitores equivalentes (Taller, 4.ª opción): el cálculo, el dibujo y
# la Teoría y los Ejemplos
OBJS += capmotor.o capmotor_dib.o capeq_texto.o
DEPS += capmotor.h capeq_texto.h

all: $(EXE).tns

%.o: %.c $(DEPS)
	$(GCC) $(GCCFLAGS) -c $<

$(EXE).elf: $(OBJS)
	mkdir -p $(DISTDIR)
	$(LD) $^ -o $(DISTDIR)/$@ $(LDFLAGS)

$(EXE).tns: $(EXE).elf
	$(GENZEHN) --input $(DISTDIR)/$^ --output $(DISTDIR)/$@.zehn $(ZEHNFLAGS)
	make-prg $(DISTDIR)/$@.zehn $(DISTDIR)/$@
	rm -f $(DISTDIR)/$@.zehn
	@echo ""
	@echo "OK -> $(EXE).tns  (cópialo a la calculadora)"

# Al cambiar de versión (ESPECIAL, HAKARI, TURBO), make clean: make no ve
# el cambio de las banderas
clean:
	rm -f *.o $(DISTDIR)/$(EXE).tns $(DISTDIR)/$(EXE).elf $(DISTDIR)/$(EXE).zehn

# Las banderas de enlace de cf_arm.o, para quien enlace los .o a mano (un
# perfil del ARM, por ejemplo: EXTRA_LD="`make -s cf-wrap`")
cf-wrap:
	@echo $(CF_WRAP)

.PHONY: all clean cf-wrap
