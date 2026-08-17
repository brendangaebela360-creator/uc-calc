/*
 * test_pc.cpp - Unit-Tests der Module der PC-Anwendung.
 *
 * Geprueft werden SerialPort und MessageLog einzeln, also unabhaengig vom
 * Gesamtsystem. SerialPort wird dazu gegen ein virtuelles Terminalpaar
 * betrieben: Der Test uebernimmt die Master-Seite und verhaelt sich wie der
 * Mikrocontroller, waehrend die zu pruefende Klasse die Slave-Seite oeffnet.
 */

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "../pc/MessageLog.h"
#include "../pc/SerialPort.h"

namespace {

int tests_run    = 0;
int tests_failed = 0;

void check(bool condition, const std::string& name)
{
    tests_run++;
    if (!condition) {
        tests_failed++;
        std::cout << "  FEHLER  " << name << '\n';
    }
}

/* Legt ein virtuelles Terminalpaar an und liefert Master-Deskriptor und
 * Geraetenamen der Slave-Seite. */
bool makePty(int& master, std::string& slaveName)
{
    master = ::posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (master < 0 || ::grantpt(master) != 0 || ::unlockpt(master) != 0) {
        return false;
    }

    char name[128];
    if (::ptsname_r(master, name, sizeof name) != 0) {
        return false;
    }
    slaveName = name;

    /* Rohmodus, damit das Terminal keine Zeichen zurueckwirft. */
    termios tty{};
    if (::tcgetattr(master, &tty) == 0) {
        ::cfmakeraw(&tty);
        ::tcsetattr(master, TCSANOW, &tty);
    }
    return true;
}

void writeAll(int fd, const std::string& data)
{
    std::size_t written = 0;
    while (written < data.size()) {
        const ssize_t n = ::write(fd, data.data() + written, data.size() - written);
        if (n <= 0) {
            return;
        }
        written += static_cast<std::size_t>(n);
    }
}

std::string readAll(int fd, int attempts = 50)
{
    std::string out;
    char        buffer[128];
    for (int i = 0; i < attempts; ++i) {
        const ssize_t n = ::read(fd, buffer, sizeof buffer);
        if (n > 0) {
            out.append(buffer, static_cast<std::size_t>(n));
        } else if (!out.empty()) {
            break;
        }
        ::usleep(10000);
    }
    return out;
}

/* ------------------------------- SerialPort ------------------------------ */

void testSerialPort()
{
    int         master = -1;
    std::string slaveName;

    if (!makePty(master, slaveName)) {
        std::cout << "  FEHLER  PTY konnte nicht angelegt werden\n";
        tests_run++;
        tests_failed++;
        return;
    }

    SerialPort port(slaveName);
    check(port.device() == slaveName, "SerialPort: Geraetename gemerkt");

    /* Empfangen: eine vollstaendige Zeile. */
    writeAll(master, "34 * 72 = 2448\n");
    auto line = port.readLine(std::chrono::milliseconds(1000));
    check(line.has_value() && *line == "34 * 72 = 2448",
          "SerialPort: vollstaendige Zeile gelesen");

    /* Empfangen: Wagenruecklauf am Zeilenende wird entfernt. */
    writeAll(master, "ERR: division by zero\r\n");
    line = port.readLine(std::chrono::milliseconds(1000));
    check(line.has_value() && *line == "ERR: division by zero",
          "SerialPort: abschliessendes CR entfernt");

    /* Empfangen: zwei Zeilen in einem Block werden einzeln geliefert. */
    writeAll(master, "eins\nzwei\n");
    const auto first  = port.readLine(std::chrono::milliseconds(1000));
    const auto second = port.readLine(std::chrono::milliseconds(1000));
    check(first.has_value() && *first == "eins" &&
          second.has_value() && *second == "zwei",
          "SerialPort: Blockempfang in Zeilen zerlegt");

    /* Empfangen: unvollstaendige Zeile wird erst nach dem Umbruch geliefert. */
    writeAll(master, "unvoll");
    const auto pending = port.readLine(std::chrono::milliseconds(200));
    check(!pending.has_value(), "SerialPort: unvollstaendige Zeile zurueckgehalten");
    writeAll(master, "staendig\n");
    line = port.readLine(std::chrono::milliseconds(1000));
    check(line.has_value() && *line == "unvollstaendig",
          "SerialPort: Zeile ueber zwei Bloecke zusammengesetzt");

    /* Zeitablauf: ohne Daten wird nach Fristende nichts geliefert. */
    const auto start   = std::chrono::steady_clock::now();
    const auto timeout = port.readLine(std::chrono::milliseconds(300));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    check(!timeout.has_value(), "SerialPort: Zeitablauf liefert keinen Wert");
    check(elapsed.count() >= 250 && elapsed.count() < 1500,
          "SerialPort: Frist wird eingehalten");

    /* Senden: die Zeile wird mit Zeilenumbruch uebertragen. */
    port.writeLine("34 * 72");
    check(readAll(master) == "34 * 72\n", "SerialPort: Zeilenumbruch angehaengt");

    /* Ein nicht vorhandenes Geraet muss eine Ausnahme ausloesen. */
    bool threw = false;
    try {
        SerialPort missing("/dev/tty-gibt-es-nicht");
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "SerialPort: Ausnahme bei nicht vorhandenem Geraet");

    ::close(master);
}

/* ------------------------------ MessageLog ------------------------------- */

std::vector<std::string> readLines(const std::string& path)
{
    std::ifstream            in(path);
    std::vector<std::string> lines;
    std::string              line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

/* Prueft das Muster JJJJ-MM-TTTHH:MM:SS.mmm am Zeilenanfang. */
bool hasTimestamp(const std::string& line)
{
    if (line.size() < 23) {
        return false;
    }
    for (std::size_t i = 0; i < 23; ++i) {
        const char c = line[i];
        const bool ok = (i == 4 || i == 7)   ? (c == '-')
                      : (i == 10)            ? (c == 'T')
                      : (i == 13 || i == 16) ? (c == ':')
                      : (i == 19)            ? (c == '.')
                                             : (c >= '0' && c <= '9');
        if (!ok) {
            return false;
        }
    }
    return true;
}

void testMessageLog()
{
    const std::string path = "/tmp/uc-calc-test.log";
    ::remove(path.c_str());

    {
        MessageLog log(path);
        log.log(MessageLog::Direction::Sent, "34 * 72");
        log.log(MessageLog::Direction::Received, "34 * 72 = 2448");
        log.note("Zeitueberschreitung nach Anfrage: 5 / 0");
    }   /* Destruktor schliesst die Datei */

    const auto lines = readLines(path);

    check(lines.size() == 5, "MessageLog: Start, drei Eintraege und Ende geschrieben");
    if (lines.size() < 5) {
        return;
    }

    check(hasTimestamp(lines[0]), "MessageLog: Zeitstempel im erwarteten Format");
    check(lines[1] == lines[1].substr(0, 24) + "TX 34 * 72",
          "MessageLog: Senderichtung als TX gekennzeichnet");
    check(lines[2] == lines[2].substr(0, 24) + "RX 34 * 72 = 2448",
          "MessageLog: Empfangsrichtung als RX gekennzeichnet");
    check(lines[3].find("-- Zeitueberschreitung") != std::string::npos,
          "MessageLog: Vermerk ohne Nachrichtenbezug geschrieben");
    check(lines[4].find("Sitzung beendet") != std::string::npos,
          "MessageLog: Sitzungsende vermerkt");

    /* Ein zweiter Lauf haengt an, statt zu ueberschreiben. */
    {
        MessageLog log(path);
        log.log(MessageLog::Direction::Sent, "7 + 5");
    }
    check(readLines(path).size() > lines.size(),
          "MessageLog: zweiter Lauf haengt an");

    /* Ein nicht beschreibbarer Pfad muss eine Ausnahme ausloesen. */
    bool threw = false;
    try {
        MessageLog log("/verzeichnis-gibt-es-nicht/x.log");
    } catch (const std::exception&) {
        threw = true;
    }
    check(threw, "MessageLog: Ausnahme bei nicht beschreibbarem Pfad");

    ::remove(path.c_str());
}

} // namespace

int main()
{
    std::cout << "Unit-Tests der PC-Module\n";

    testSerialPort();
    testMessageLog();

    std::cout << '\n' << tests_run << " Tests, " << tests_failed << " Fehler\n";
    return tests_failed == 0 ? 0 : 1;
}
