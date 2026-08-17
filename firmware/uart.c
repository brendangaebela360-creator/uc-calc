/*
 * uart.c - Implementierung des UART-Treibers fuer den ATmega328P.
 *
 * Baudratenberechnung (Datenblatt, Abschnitt USART, Normalmodus):
 *
 *     UBRR0 = F_CPU / (16 * BAUD) - 1
 *           = 16000000 / (16 * 9600) - 1
 *           = 103,17  ->  103
 *
 * Die tatsaechliche Baudrate betraegt damit 9615 Baud, der Fehler liegt
 * bei 0,2 Prozent und somit deutlich unter der zulaessigen Grenze.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "uart.h"

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#define UART_BAUD 9600UL
#define UART_UBRR ((F_CPU / (16UL * UART_BAUD)) - 1UL)

/* Ringpuffer. head wird ausschliesslich in der ISR veraendert,
 * tail ausschliesslich im Hauptprogramm. Beide Indizes sind volatile,
 * da sie zwischen Interrupt- und Hauptkontext geteilt werden. */
static volatile char    rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static volatile uint8_t rx_overflow = 0;

void uart_init(void)
{
    /* Baudratenteiler setzen (High-Byte zuerst). */
    UBRR0H = (uint8_t)(UART_UBRR >> 8);
    UBRR0L = (uint8_t)(UART_UBRR & 0xFF);

    /* Rahmenformat 8N1: acht Datenbits, kein Paritaetsbit, ein Stoppbit. */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);

    /* Sender und Empfaenger freigeben, Empfangsinterrupt aktivieren. */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
}

/* Empfangsinterrupt: Ein Zeichen wurde vollstaendig empfangen.
 * Die Routine bleibt bewusst kurz - sie legt das Zeichen lediglich ab. */
ISR(USART_RX_vect)
{
    char c = (char)UDR0;
    uint8_t next = (uint8_t)((rx_head + 1) & (UART_RX_BUFFER_SIZE - 1));

    if (next == rx_tail) {
        /* Puffer voll: Das Zeichen wird verworfen und gemerkt. */
        rx_overflow = 1;
        return;
    }

    rx_buffer[rx_head] = c;
    rx_head = next;
}

uint8_t uart_get(char *out)
{
    uint8_t available;

    /* Der Vergleich der beiden Indizes muss unterbrechungsfrei erfolgen. */
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        available = (uint8_t)(rx_head != rx_tail);
        if (available) {
            *out = rx_buffer[rx_tail];
            rx_tail = (uint8_t)((rx_tail + 1) & (UART_RX_BUFFER_SIZE - 1));
        }
    }

    return available;
}

void uart_put(char c)
{
    /* Warten, bis das Senderegister wieder aufnahmebereit ist. */
    while (!(UCSR0A & (1 << UDRE0))) {
        /* aktives Warten */
    }
    UDR0 = (uint8_t)c;
}

void uart_write(const char *s)
{
    while (*s != '\0') {
        uart_put(*s++);
    }
}

void uart_write_line(const char *s)
{
    uart_write(s);
    uart_put('\r');
    uart_put('\n');
}

uint8_t uart_overflow(void)
{
    uint8_t flag;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        flag = rx_overflow;
        rx_overflow = 0;
    }

    return flag;
}
