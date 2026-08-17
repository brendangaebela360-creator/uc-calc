/*
 * uart.h - Registernaher UART-Treiber fuer den ATmega328P.
 *
 * Der Treiber kommt ohne Arduino-Bibliotheken aus und spricht den USART0
 * direkt ueber die Register UBRR0, UCSR0A, UCSR0B und UCSR0C an.
 *
 * Empfangen wird interruptgesteuert (RXCIE0) in einen Ringpuffer, damit
 * waehrend der Auswertung eines Ausdrucks keine Zeichen verloren gehen.
 * Gesendet wird blockierend, da die Antwortzeilen kurz sind und die
 * Hauptschleife ohnehin auf die naechste Anfrage wartet.
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

/* Groesse des Empfangsringpuffers. Muss eine Zweierpotenz sein, damit die
 * Indexberechnung ohne Modulo-Division auskommt. */
#define UART_RX_BUFFER_SIZE 32

/* Initialisiert USART0 auf 9600 Baud, 8N1, und schaltet den
 * Empfangsinterrupt frei. Der globale Interruptfreigabe-Aufruf sei()
 * bleibt Aufgabe des Aufrufers. */
void uart_init(void);

/* Holt ein Zeichen aus dem Ringpuffer.
 * Rueckgabe: 1, wenn ein Zeichen entnommen wurde, sonst 0. */
uint8_t uart_get(char *out);

/* Sendet ein einzelnes Zeichen (blockierend). */
void uart_put(char c);

/* Sendet eine nullterminierte Zeichenkette. */
void uart_write(const char *s);

/* Sendet eine Zeichenkette und schliesst sie mit "\r\n" ab, damit die
 * Ausgabe auch in Terminalprogrammen unter Windows sauber umbricht. */
void uart_write_line(const char *s);

/* Meldet, ob der Ringpuffer seit dem letzten Aufruf uebergelaufen ist.
 * Der Zaehler wird beim Lesen zurueckgesetzt. */
uint8_t uart_overflow(void);

#endif /* UART_H */
