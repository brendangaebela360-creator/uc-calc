/*
 * simulator.c - Simulator der Firmware fuer Systemtests ohne Hardware.
 *
 * Gleiche Hauptschleife und derselbe Rechenkern wie die Firmware, statt
 * USART aber ein virtuelles Terminalpaar (PTY). Ein PTY kennt kein DTR, der
 * Verbindungsaufbau ist auf der Master-Seite nicht erkennbar: Die Begruessung
 * wird deshalb alle 250 ms wiederholt, bis das erste Zeichen eintrifft.
 *
 * Aufruf: ./uc-calc-sim - gibt den Geraetenamen fuer --port aus.
 */

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../core/calc_core.h"

#define GREETING          "READY uc-calc 1.0"
#define GREETING_PERIOD_MS 250

typedef enum {
    LINE_COLLECT,   /* Zeichen werden uebernommen                       */
    LINE_DISCARD    /* Zeile zu lang: Rest bis zum Zeilenende verwerfen */
} line_state_t;

/* Schreibt einen Block vollstaendig, auch wenn write() ihn aufteilt. */
static void write_all(int fd, const char *data, size_t len)
{
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, data + written, len - written);
        if (n <= 0) {
            return;
        }
        written += (size_t)n;
    }
}

/* Sendet eine Zeile mit "\r\n", wie es die Firmware tut. */
static void write_line(int fd, const char *text)
{
    write_all(fd, text, strlen(text));
    write_all(fd, "\r\n", 2);
}

/* Monotone Zeit in Millisekunden. */
static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* Wertet eine vollstaendig empfangene Zeile aus und antwortet. */
static void handle_line(int master, const char *line, line_state_t state)
{
    char answer[CALC_ANSWER_MAX];

    if (state == LINE_DISCARD) {
        calc_format(answer, sizeof answer, line, CALC_ERR_LINE_TOO_LONG, 0);
    } else {
        int32_t       result = 0;
        calc_status_t status = calc_eval(line, &result);
        calc_format(answer, sizeof answer, line, status, result);
    }

    write_line(master, answer);
}

int main(void)
{
    int          master;
    char         slave_name[128];
    char         line[CALC_LINE_MAX];
    unsigned     len       = 0;
    line_state_t state     = LINE_COLLECT;
    int          greeting  = 1;      /* Begruessungsphase aktiv */
    long         last_sent = 0;

    master = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        perror("PTY konnte nicht angelegt werden");
        return 1;
    }
    if (ptsname_r(master, slave_name, sizeof slave_name) != 0) {
        perror("Geraetename nicht ermittelbar");
        return 1;
    }

    /* Rohmodus: sonst wirft das Terminal jedes Zeichen zurueck (ECHO) und
     * der Simulator empfaengt seine eigene Ausgabe als Anfrage. */
    {
        struct termios tty;
        if (tcgetattr(master, &tty) == 0) {
            cfmakeraw(&tty);
            tcsetattr(master, TCSANOW, &tty);
        }
    }

    printf("Simulator bereit.\n");
    printf("Port fuer die PC-Anwendung: %s\n", slave_name);
    fflush(stdout);

    for (;;) {
        struct pollfd pfd;
        char          c;
        ssize_t       n;

        /* Begruessung wiederholen, solange noch keine Anfrage kam. */
        if (greeting && now_ms() - last_sent >= GREETING_PERIOD_MS) {
            write_line(master, GREETING);
            last_sent = now_ms();
        }

        pfd.fd      = master;
        pfd.events  = POLLIN;
        pfd.revents = 0;
        poll(&pfd, 1, 50);

        n = read(master, &c, 1);
        if (n <= 0) {
            /* EIO: Gegenseite hat den Port geschlossen - wieder begruessen. */
            if (n < 0 && errno == EIO) {
                greeting = 1;
                len      = 0;
                state    = LINE_COLLECT;
            }
            continue;
        }

        greeting = 0;               /* Gegenstelle ist aktiv */

        if (c == '\r') {
            continue;               /* Wagenruecklauf wird ignoriert */
        }

        if (c != '\n') {
            if (state == LINE_COLLECT) {
                if (len < CALC_LINE_MAX - 1) {
                    line[len++] = c;
                } else {
                    state = LINE_DISCARD;
                }
            }
            continue;
        }

        line[len] = '\0';

        if (state != LINE_DISCARD && len == 0) {
            continue;               /* Leerzeilen werden uebergangen */
        }

        handle_line(master, line, state);

        len   = 0;
        state = LINE_COLLECT;
    }

    return 0;
}
