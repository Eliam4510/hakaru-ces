/* ============================================================================
 *  app.h  -  El nombre del programa, en un solo sitio
 *
 *  Hakaru (測る, «medir» en japonés) CES: Circuit Emulator & Solver, un
 *  emulador y calculadora de circuitos de CC y CA. Hasta la Etapa 4 se
 *  llamó ElectroFock.
 *
 *  Lo usan la portada, el menú y la Ayuda (ACERCA DE). El
 *  nombre del .tns y el de la cabecera Zehn van en el Makefile (EXE y
 *  --name); el kanji de la portada es una imagen (kanji_img.h). Solo ASCII:
 *  la sigla y el nombre no llevan tildes.
 * ========================================================================= */
/* La guarda no se llama como ninguna constante: una guarda que se llama
 * como una constante vacía la cabecera en silencio */
#ifndef APP_API_H
#define APP_API_H

#define APP_NOMBRE   "Hakaru CES"                  /* el nombre completo    */
#define APP_TITULO   "HAKARU"                      /* el de la portada, x3  */
#define APP_SIGLA    "CES"
#define APP_SIGLA_L  "Circuit Emulator & Solver"   /* lo que dice la sigla  */
#define APP_VERSION  "4.0"                         /* la de la Ayuda        */

#endif /* APP_API_H */
