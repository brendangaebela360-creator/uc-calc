# uc-calc — Mikrocontroller-Kalkulator

Ein einfacher Kalkulator, bei dem ein **Arduino Uno (ATmega328P)** über die
serielle Schnittstelle (UART, 9600 Baud, 8N1) mit einem PC kommuniziert.
Die PC-Anwendung sendet Ausdrücke der Form `Zahl Operator Zahl`, die Firmware
wertet sie aus und antwortet. Der gesamte Nachrichtenaustausch wird auf dem PC
protokolliert.

Portfolio zum Kurs *Programmierung mit C/C++ (DLBROEPRS01_D)*, IU Internationale
Hochschule.
Hochschule.

## Aufbau

| Verzeichnis | Inhalt |
|---|---|
| `core/` | `calc_core` — Parser, Rechnung und Antwortformatierung (portables C99) |
| `firmware/` | `uart.c` (registernaher USART-Treiber), `main.c` (Hauptschleife) |
| `pc/` | PC-Anwendung in C++17: `SerialPort`, `MessageLog`, Ablaufsteuerung |
| `sim/` | Simulator der Firmware hinter einem virtuellen Port (PTY) |
| `tests/` | Unit-Tests des Rechenkerns (`test_calc_core.c`) und der PC-Module (`test_pc.cpp`) |

`calc_core` wird **unverändert** von der Firmware, vom Simulator und von den
Unit-Tests verwendet. Dadurch gibt es die Rechenlogik nur einmal, und sie ist
ohne Hardware prüfbar.

## Übersetzen und testen

```bash
make test        # alle Unit-Tests (59: 41 Kern, 18 PC-Module)
make pc          # PC-Anwendung  -> build/uc-calc-pc
make sim         # Simulator     -> build/uc-calc-sim
make firmware    # Firmware      -> build/uc-calc.hex   (benötigt avr-gcc)
make flash PORT=/dev/ttyACM0     # Übertragen           (benötigt avrdude)
```

## Systemtest ohne Hardware

```bash
./build/uc-calc-sim               # gibt den Portnamen aus, z. B. /dev/pts/3
./build/uc-calc-pc --port /dev/pts/3 --log session.log
```

Batch-Betrieb für reproduzierbare Läufe:

```bash
./build/uc-calc-pc --port /dev/pts/3 --batch tests/beispiele.txt
```

## Test an der realen Hardware

```bash
make flash PORT=/dev/ttyACM0
./build/uc-calc-pc --port /dev/ttyACM0 --log session.log
```

Die Kommandos sind dieselben wie im Simulator.

## Protokoll

| Richtung | Beispiel |
|---|---|
| Firmware nach dem Reset | `READY uc-calc 1.0` |
| Anfrage | `34 * 72` |
| Antwort | `34 * 72 = 2448` |
| Fehlerantwort | `ERR: division by zero` |

Definierte Fehlerfälle: `syntax error`, `unknown operator`, `value out of range`,
`division by zero`, `line too long`.

## Ressourcenbedarf

`avr-size` meldet für die Firmware **2634 Byte Flash (8,0 %)** und
**203 Byte RAM (9,9 %)** auf dem ATmega328P.

## Hinweis zu Umlauten im Quelltext

Die Quelltextkommentare verzichten bewusst auf Umlaute und verwenden
Umschreibungen (`ae`, `oe`, `ue`, `ss`). Damit bleiben die Dateien unabhängig
von der Zeichensatzeinstellung des jeweiligen Editors oder Compilers lesbar —
eine in eingebetteten Projekten übliche Konvention. Dokumentation und
Diagramme verwenden dagegen korrekte deutsche Rechtschreibung.
