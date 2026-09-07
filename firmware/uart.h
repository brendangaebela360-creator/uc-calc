/*
 * uart.h - Registernaher UART-Treiber fuer den ATmega328P (USART0).
 *
 * Ohne Arduino-Bibliotheken. Empfang interruptgesteuert (RXCIE0) in einen
 * Ringpuffer, damit waehrend der Auswertung keine Zeichen verloren gehen.
 * Senden blockierend - die Antwortzeilen sind kurz.
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

/* Zweierpotenz, damit die Indexberechnung ohne Modulo auskommt. */
#define UART_RX_BUFFER_SIZE 32

/* 9600 Baud, 8N1, Empfangsinterrupt frei. sei() macht der Aufrufer. */
void uart_init(void);

/* Holt ein Zeichen aus dem Ringpuffer.
 * Rueckgabe: 1, wenn ein Zeichen entnommen wurde, sonst 0. */
uint8_t uart_get(char *out);

/* Sendet ein einzelnes Zeichen (blockierend). */
void uart_put(char c);

/* Sendet eine nullterminierte Zeichenkette. */
void uart_write(const char *s);

/* Sendet die Zeichenkette und schliesst mit "\r\n" ab. */
void uart_write_line(const char *s);

/* Meldet einen Ringpufferueberlauf und setzt das Kennzeichen zurueck. */
uint8_t uart_overflow(void);

#endif /* UART_H */
