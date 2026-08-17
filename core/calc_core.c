/*
 * calc_core.c - Implementierung des gemeinsamen Rechenkerns.
 *
 * Entwurfsentscheidungen:
 *   - Kein dynamischer Speicher: alle Puffer werden vom Aufrufer gestellt.
 *   - Keine Gleitkommaarithmetik: der ATmega328P besitzt keine FPU.
 *   - Keine stdio-Funktionen: snprintf() der avr-libc belegt mehrere
 *     Kilobyte Programmspeicher. Die Zahlenausgabe erfolgt daher ueber
 *     eine eigene, knapp gehaltene Umwandlungsroutine.
 *   - Alle Bereichsueberschreitungen werden vor der Rechnung geprueft.
 *     Ein vorzeichenbehafteter Ueberlauf waere undefiniertes Verhalten
 *     und darf deshalb nicht erst nachtraeglich erkannt werden.
 */

#include "calc_core.h"

/* ------------------------------------------------------------------ */
/* Kleine Hilfsfunktionen                                              */
/* ------------------------------------------------------------------ */

static int is_space(char c)
{
    return c == ' ' || c == '\t';
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static void skip_spaces(const char **pp)
{
    while (is_space(**pp)) {
        (*pp)++;
    }
}

/* Laenge einer nullterminierten Zeichenkette, ohne <string.h>. */
static size_t str_len(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Zahlenumwandlung                                                    */
/* ------------------------------------------------------------------ */

/*
 * Liest eine vorzeichenbehaftete Dezimalzahl und setzt den Lesezeiger
 * hinter die letzte gelesene Ziffer. Der Betrag wird waehrend des Lesens
 * gegen die Grenze geprueft, sodass kein Ueberlauf entstehen kann.
 */
static calc_status_t parse_int(const char **pp, int32_t *out)
{
    const char *p = *pp;
    int negative = 0;
    uint32_t magnitude = 0;
    uint32_t limit;
    int digits = 0;

    skip_spaces(&p);

    if (*p == '+' || *p == '-') {
        negative = (*p == '-');
        p++;
    }

    /* Der darstellbare Betrag ist bei negativen Zahlen um eins groesser. */
    limit = negative ? 2147483648u : 2147483647u;

    while (is_digit(*p)) {
        uint32_t d = (uint32_t)(*p - '0');
        if (magnitude > (limit - d) / 10u) {
            return CALC_ERR_RANGE;
        }
        magnitude = magnitude * 10u + d;
        digits++;
        p++;
    }

    if (digits == 0) {
        return CALC_ERR_SYNTAX;
    }

    if (negative) {
        /* 2147483648 laesst sich nicht als positiver int32_t darstellen
         * und wird deshalb gesondert behandelt. */
        *out = (magnitude == 2147483648u)
                   ? (-2147483647 - 1)
                   : -(int32_t)magnitude;
    } else {
        *out = (int32_t)magnitude;
    }

    *pp = p;
    return CALC_OK;
}

/*
 * Haengt eine vorzeichenbehaftete Zahl an einen Puffer an.
 * Gibt die Anzahl der geschriebenen Zeichen zurueck.
 */
static size_t append_int(char *out, size_t out_sz, size_t pos, int32_t value)
{
    char tmp[11];               /* -2147483648 benoetigt 11 Zeichen */
    size_t n = 0;
    uint32_t magnitude;

    if (value < 0) {
        /* Der Betrag wird unsigned gebildet, damit INT32_MIN korrekt ist. */
        magnitude = (uint32_t)(-(value + 1)) + 1u;
        if (pos + 1 < out_sz) {
            out[pos++] = '-';
        }
    } else {
        magnitude = (uint32_t)value;
    }

    do {
        tmp[n++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0u);

    while (n > 0 && pos + 1 < out_sz) {
        out[pos++] = tmp[--n];
    }

    return pos;
}

/* ------------------------------------------------------------------ */
/* Arithmetik mit vorgelagerter Bereichspruefung                       */
/* ------------------------------------------------------------------ */

#define CALC_INT32_MAX  2147483647
#define CALC_INT32_MIN  (-2147483647 - 1)

static calc_status_t apply_op(char op, int32_t a, int32_t b, int32_t *out)
{
    switch (op) {
    case '+':
        if (b > 0 && a > CALC_INT32_MAX - b) return CALC_ERR_RANGE;
        if (b < 0 && a < CALC_INT32_MIN - b) return CALC_ERR_RANGE;
        *out = a + b;
        return CALC_OK;

    case '-':
        if (b < 0 && a > CALC_INT32_MAX + b) return CALC_ERR_RANGE;
        if (b > 0 && a < CALC_INT32_MIN + b) return CALC_ERR_RANGE;
        *out = a - b;
        return CALC_OK;

    case '*':
        if (a != 0 && b != 0) {
            if (a > 0 && b > 0 && a > CALC_INT32_MAX / b) return CALC_ERR_RANGE;
            if (a > 0 && b < 0 && b < CALC_INT32_MIN / a) return CALC_ERR_RANGE;
            if (a < 0 && b > 0 && a < CALC_INT32_MIN / b) return CALC_ERR_RANGE;
            if (a < 0 && b < 0 && b < CALC_INT32_MAX / a) return CALC_ERR_RANGE;
        }
        *out = a * b;
        return CALC_OK;

    case '/':
        if (b == 0) return CALC_ERR_DIV_ZERO;
        /* Der einzige Ueberlauf der Division. */
        if (a == CALC_INT32_MIN && b == -1) return CALC_ERR_RANGE;
        *out = a / b;
        return CALC_OK;

    default:
        return CALC_ERR_UNKNOWN_OP;
    }
}

/* ------------------------------------------------------------------ */
/* Oeffentliche Schnittstelle                                          */
/* ------------------------------------------------------------------ */

calc_status_t calc_eval(const char *expr, int32_t *result)
{
    const char *p = expr;
    int32_t left = 0;
    int32_t right = 0;
    char op;
    calc_status_t st;

    if (expr == 0 || result == 0) {
        return CALC_ERR_SYNTAX;
    }

    if (str_len(expr) >= CALC_LINE_MAX) {
        return CALC_ERR_LINE_TOO_LONG;
    }

    st = parse_int(&p, &left);
    if (st != CALC_OK) {
        return st;
    }

    skip_spaces(&p);

    /* Ein Zeilenende oder eine Ziffer an dieser Stelle bedeutet, dass der
     * Ausdruck nicht dem Muster "Zahl Operator Zahl" entspricht. Jedes
     * andere Zeichen ist ein nicht unterstuetzter Operator. */
    if (*p == '\0' || is_digit(*p)) {
        return CALC_ERR_SYNTAX;
    }
    op = *p++;
    if (op != '+' && op != '-' && op != '*' && op != '/') {
        return CALC_ERR_UNKNOWN_OP;
    }

    st = parse_int(&p, &right);
    if (st != CALC_OK) {
        return st;
    }

    /* Hinter dem zweiten Operanden darf nur noch Leerraum stehen. */
    skip_spaces(&p);
    if (*p != '\0') {
        return CALC_ERR_SYNTAX;
    }

    return apply_op(op, left, right, result);
}

const char *calc_status_text(calc_status_t status)
{
    switch (status) {
    case CALC_OK:                 return "ok";
    case CALC_ERR_SYNTAX:         return "syntax error";
    case CALC_ERR_UNKNOWN_OP:     return "unknown operator";
    case CALC_ERR_RANGE:          return "value out of range";
    case CALC_ERR_DIV_ZERO:       return "division by zero";
    case CALC_ERR_LINE_TOO_LONG:  return "line too long";
    default:                      return "internal error";
    }
}

size_t calc_format(char *out, size_t out_sz, const char *expr,
                   calc_status_t status, int32_t result)
{
    size_t pos = 0;
    const char *src;

    if (out == 0 || out_sz == 0) {
        return 0;
    }

    if (status == CALC_OK) {
        /* "<Ausdruck> = <Ergebnis>". Der Ausdruck wird dabei von fuehrendem
         * und abschliessendem Leerraum befreit, damit die Antwortzeile
         * unabhaengig von der Formatierung der Anfrage einheitlich aussieht. */
        const char *begin = (expr != 0) ? expr : "";
        const char *end;

        while (is_space(*begin)) {
            begin++;
        }
        end = begin;
        for (src = begin; *src != '\0'; src++) {
            if (!is_space(*src)) {
                end = src + 1;
            }
        }

        for (src = begin; src < end && pos + 1 < out_sz; src++) {
            out[pos++] = *src;
        }
        for (src = " = "; *src != '\0' && pos + 1 < out_sz; src++) {
            out[pos++] = *src;
        }
        pos = append_int(out, out_sz, pos, result);
    } else {
        /* "ERR: <Ursache>" */
        for (src = "ERR: "; *src != '\0' && pos + 1 < out_sz; src++) {
            out[pos++] = *src;
        }
        for (src = calc_status_text(status); *src != '\0' && pos + 1 < out_sz; src++) {
            out[pos++] = *src;
        }
    }

    out[pos] = '\0';
    return pos;
}
