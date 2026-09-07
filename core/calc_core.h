/*
 * calc_core.h - Gemeinsamer Rechenkern (C99).
 *
 * Frei von Hardware- und Betriebssystembezuegen, damit Firmware, Simulator
 * und Unit-Tests dieselbe Datei verwenden koennen.
 * Kein dynamischer Speicher, kein Gleitkomma.
 */

#ifndef CALC_CORE_H
#define CALC_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Laenge der laengsten Antwortzeile inklusive abschliessender Null.
 * Worst Case: "-2147483648 * -2147483648 = -2147483648" -> 39 Zeichen. */
#define CALC_ANSWER_MAX 48

/* Maximale Laenge einer Anfragezeile inklusive abschliessender Null.
 * Bestimmt zugleich die Groesse des Empfangspuffers der Firmware. */
#define CALC_LINE_MAX 32

/* Ergebnis einer Auswertung. CALC_OK bedeutet Erfolg, alle uebrigen
 * Werte beschreiben genau einen definierten Fehlerfall. */
typedef enum {
    CALC_OK = 0,             /* Ausdruck gueltig, Ergebnis gesetzt        */
    CALC_ERR_SYNTAX,         /* Ausdruck entspricht nicht "Zahl Op Zahl"  */
    CALC_ERR_UNKNOWN_OP,     /* Operatorzeichen nicht unterstuetzt        */
    CALC_ERR_RANGE,          /* Operand oder Ergebnis ausserhalb int32_t  */
    CALC_ERR_DIV_ZERO,       /* Division oder Modulo durch null           */
    CALC_ERR_LINE_TOO_LONG   /* Eingabezeile laenger als CALC_LINE_MAX-1  */
} calc_status_t;

/*
 * Wertet einen Ausdruck der Form "Zahl Operator Zahl" aus.
 *
 * Erlaubt sind die Operatoren '+', '-', '*' und '/'. Operanden duerfen ein
 * fuehrendes '+' oder '-' tragen. Leerzeichen und Tabulatoren sind an jeder
 * Stelle zulaessig und werden ignoriert; "34*72" ist damit ebenso gueltig
 * wie "34 * 72".
 *
 * expr    Nullterminierte Eingabezeile ohne Zeilenendezeichen.
 * result  Zeiger auf die Ergebnisvariable. Wird nur bei CALC_OK beschrieben.
 *
 * Rueckgabe: CALC_OK oder der zutreffende Fehlercode. Die Funktion greift
 * nicht auf globale Zustaende zu und ist damit wiedereintrittsfaehig.
 */
calc_status_t calc_eval(const char *expr, int32_t *result);

/*
 * Baut die Antwortzeile zu einer Anfrage auf.
 *
 * Bei CALC_OK entsteht "<Ausdruck> = <Ergebnis>", andernfalls
 * "ERR: <Ursache>". Die Zeile wird ohne Zeilenendezeichen erzeugt; das
 * Anhaengen von "\n" ist Aufgabe der jeweiligen Transportschicht.
 *
 * out     Zielpuffer, mindestens CALC_ANSWER_MAX Bytes gross.
 * out_sz  Groesse des Zielpuffers.
 *
 * Rueckgabe: Anzahl der geschriebenen Zeichen ohne die abschliessende Null.
 */
size_t calc_format(char *out, size_t out_sz, const char *expr,
                   calc_status_t status, int32_t result);

/* Liefert den festen Fehlertext zu einem Statuscode (fuer Tests und Logs). */
const char *calc_status_text(calc_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* CALC_CORE_H */
