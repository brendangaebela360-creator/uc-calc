/*
 * main.c - Hauptprogramm der Mikrocontroller-Anwendung.
 *
 * Die READY-Zeile nach dem Start faengt den DTR-Reset beim Oeffnen des Ports
 * ab. Danach Schleife Empfangen - Auswerten - Senden. Die Zeilenerkennung ist
 * ein Zustandsautomat (COLLECT/DISCARD); zu lange Zeilen werden verworfen und
 * mit "ERR: line too long" beantwortet, damit der Puffer nie ueberlaeuft.
 */

#include <avr/interrupt.h>

#include "uart.h"
#include "../core/calc_core.h"

/* Versionsangabe, damit die PC-Seite Firmwarestaende unterscheiden kann. */
#define GREETING "READY uc-calc 1.0"

/* Zustaende der Zeilenerkennung. */
typedef enum {
    LINE_COLLECT,   /* Zeichen werden in den Puffer uebernommen */
    LINE_DISCARD    /* Zeile zu lang: Rest bis zum Zeilenende verwerfen */
} line_state_t;

int main(void)
{
    char    line[CALC_LINE_MAX];
    char    answer[CALC_ANSWER_MAX];
    uint8_t len = 0;
    line_state_t state = LINE_COLLECT;
    char    c;

    uart_init();
    sei();                       /* globale Interruptfreigabe */

    uart_write_line(GREETING);

    for (;;) {
        if (!uart_get(&c)) {
            continue;            /* nichts empfangen, weiter warten */
        }

        /* Wagenruecklauf wird ignoriert; als Zeilenende gilt "\n". */
        if (c == '\r') {
            continue;
        }

        if (c != '\n') {
            if (state == LINE_COLLECT) {
                if (len < CALC_LINE_MAX - 1) {
                    line[len++] = c;
                } else {
                    state = LINE_DISCARD;
                }
            }
            continue;            /* Zeile ist noch nicht vollstaendig */
        }

        /* --- Zeilenende erreicht: auswerten und antworten --- */
        line[len] = '\0';

        if (state == LINE_DISCARD) {
            calc_format(answer, sizeof answer, line,
                        CALC_ERR_LINE_TOO_LONG, 0);
        } else if (len == 0) {
            /* Leerzeilen werden stillschweigend uebergangen. */
            state = LINE_COLLECT;
            len = 0;
            continue;
        } else {
            int32_t       result = 0;
            calc_status_t status = calc_eval(line, &result);
            calc_format(answer, sizeof answer, line, status, result);
        }

        uart_write_line(answer);

        /* Ueberlauf melden, statt den Verlust unbemerkt zu lassen. */
        if (uart_overflow()) {
            uart_write_line("ERR: receive buffer overflow");
        }

        len = 0;
        state = LINE_COLLECT;
    }

    return 0;                    /* wird nie erreicht */
}
