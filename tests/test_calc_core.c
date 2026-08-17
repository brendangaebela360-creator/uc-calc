/*
 * test_calc_core.c - Unit-Tests des Rechenkerns.
 *
 * Die Tests kommen ohne Fremdbibliothek aus, damit sie auf jedem System
 * mit einem C-Compiler laufen. Geprueft werden die Grundrechenarten, die
 * Toleranz gegenueber Leerraum, alle definierten Fehlerfaelle sowie die
 * Grenzen des Wertebereichs.
 */

#include <stdio.h>
#include <string.h>

#include "../core/calc_core.h"

static int tests_run    = 0;
static int tests_failed = 0;

/* Prueft, dass ein Ausdruck gueltig ist und das erwartete Ergebnis liefert. */
static void expect_value(const char *expr, int32_t expected)
{
    int32_t       result = 0;
    calc_status_t status = calc_eval(expr, &result);

    tests_run++;
    if (status != CALC_OK || result != expected) {
        tests_failed++;
        printf("  FEHLER  \"%s\": erwartet %ld, erhalten %ld (Status: %s)\n",
               expr, (long)expected, (long)result, calc_status_text(status));
    }
}

/* Prueft, dass ein Ausdruck genau den erwarteten Fehler ausloest. */
static void expect_error(const char *expr, calc_status_t expected)
{
    int32_t       result = 0;
    calc_status_t status = calc_eval(expr, &result);

    tests_run++;
    if (status != expected) {
        tests_failed++;
        printf("  FEHLER  \"%s\": erwartet \"%s\", erhalten \"%s\"\n",
               expr, calc_status_text(expected), calc_status_text(status));
    }
}

/* Prueft die erzeugte Antwortzeile. */
static void expect_answer(const char *expr, const char *expected)
{
    char          answer[CALC_ANSWER_MAX];
    int32_t       result = 0;
    calc_status_t status = calc_eval(expr, &result);

    calc_format(answer, sizeof answer, expr, status, result);

    tests_run++;
    if (strcmp(answer, expected) != 0) {
        tests_failed++;
        printf("  FEHLER  \"%s\": erwartet \"%s\", erhalten \"%s\"\n",
               expr, expected, answer);
    }
}

int main(void)
{
    printf("Unit-Tests calc_core\n");

    /* --- Grundrechenarten --------------------------------------------- */
    expect_value("34 * 72", 2448);
    expect_value("7 + 5", 12);
    expect_value("7 - 12", -5);
    expect_value("100 / 7", 14);          /* ganzzahlige Division  */
    expect_value("-8 / 2", -4);
    expect_value("0 * 12345", 0);

    /* --- Toleranz gegenueber Leerraum und Vorzeichen ------------------ */
    expect_value("34*72", 2448);
    expect_value("  34   *   72  ", 2448);
    expect_value("\t7\t+\t5\t", 12);
    expect_value("-3 * -4", 12);
    expect_value("+3 * +4", 12);
    expect_value("5 - -5", 10);

    /* --- Grenzen des Wertebereichs ------------------------------------ */
    expect_value("2147483647 + 0", 2147483647);
    expect_value("-2147483648 + 0", -2147483647 - 1);
    expect_value("2147483647 - 2147483647", 0);
    expect_error("2147483647 + 1", CALC_ERR_RANGE);
    expect_error("-2147483648 - 1", CALC_ERR_RANGE);
    expect_error("100000 * 100000", CALC_ERR_RANGE);
    expect_error("-2147483648 / -1", CALC_ERR_RANGE);
    expect_error("4294967296 + 1", CALC_ERR_RANGE);

    /* --- Definierte Fehlerfaelle -------------------------------------- */
    expect_error("5 / 0", CALC_ERR_DIV_ZERO);
    expect_error("0 / 0", CALC_ERR_DIV_ZERO);
    expect_error("5 % 3", CALC_ERR_UNKNOWN_OP);
    expect_error("5 ^ 3", CALC_ERR_UNKNOWN_OP);
    expect_error("5 x 3", CALC_ERR_UNKNOWN_OP);
    expect_error("5 +", CALC_ERR_SYNTAX);
    expect_error("* 3", CALC_ERR_SYNTAX);
    expect_error("5 3", CALC_ERR_SYNTAX);
    expect_error("", CALC_ERR_SYNTAX);
    expect_error("abc", CALC_ERR_SYNTAX);
    expect_error("1 + 2 + 3", CALC_ERR_SYNTAX);
    expect_error("1234567890123 + 1", CALC_ERR_RANGE);
    expect_error("11111111111111111111111111111111 + 1",
                 CALC_ERR_LINE_TOO_LONG);

    /* --- Antwortzeilen ------------------------------------------------ */
    expect_answer("34 * 72", "34 * 72 = 2448");
    expect_answer("7 - 12", "7 - 12 = -5");
    expect_answer("5 / 0", "ERR: division by zero");
    expect_answer("5 % 3", "ERR: unknown operator");
    expect_answer("2147483647 + 1", "ERR: value out of range");
    expect_answer("-2147483648 + 0", "-2147483648 + 0 = -2147483648");
    expect_answer("  34   *   72  ", "34   *   72 = 2448");   /* Raender getrimmt */
    expect_answer("34*72", "34*72 = 2448");

    printf("\n%d Tests, %d Fehler\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
