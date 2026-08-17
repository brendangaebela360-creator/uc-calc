/*
 * main.cpp - Ablaufsteuerung der PC-Anwendung.
 *
 * Aufruf:
 *   uc-calc-pc --port <Geraet> [--log <Datei>] [--batch <Datei>]
 *
 * Ohne --batch laeuft die Anwendung interaktiv: Der Benutzer gibt Ausdruecke
 * ein, die Antwort des Mikrocontrollers wird angezeigt. "quit" beendet das
 * Programm. Mit --batch werden die Zeilen einer Datei nacheinander gesendet;
 * das erlaubt reproduzierbare Testlaeufe.
 *
 * Vor der ersten Anfrage wartet die Anwendung auf die Begruessungszeile
 * "READY ...". Damit ist der Reset abgefangen, den das DTR-Signal beim
 * Oeffnen des Ports auf dem Arduino ausloest.
 */

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "MessageLog.h"
#include "SerialPort.h"

namespace {

constexpr auto kReplyTimeout = std::chrono::milliseconds(1000);
constexpr auto kReadyTimeout = std::chrono::milliseconds(5000);

struct Options {
    std::string port;
    std::string logfile = "session.log";
    std::string batch;
};

void printUsage(const char* program)
{
    std::cerr << "Aufruf: " << program
              << " --port <Geraet> [--log <Datei>] [--batch <Datei>]\n"
              << "Beispiel: " << program << " --port /dev/ttyACM0\n";
}

/* Wertet die Kommandozeile aus. Gibt false zurueck, wenn der Aufruf
 * unvollstaendig oder fehlerhaft ist. */
bool parseArguments(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue   = (i + 1 < argc);

        if (arg == "--port" && hasValue) {
            options.port = argv[++i];
        } else if (arg == "--log" && hasValue) {
            options.logfile = argv[++i];
        } else if (arg == "--batch" && hasValue) {
            options.batch = argv[++i];
        } else {
            return false;
        }
    }
    return !options.port.empty();
}

/* Wartet auf die Begruessungszeile der Firmware. */
bool awaitReady(SerialPort& port, MessageLog& log)
{
    const auto deadline = std::chrono::steady_clock::now() + kReadyTimeout;

    while (std::chrono::steady_clock::now() < deadline) {
        const auto line = port.readLine(std::chrono::milliseconds(500));
        if (!line) {
            continue;
        }
        log.log(MessageLog::Direction::Received, *line);
        if (line->rfind("READY", 0) == 0) {
            std::cout << "Verbunden: " << *line << '\n';
            return true;
        }
    }

    log.note("Zeitueberschreitung beim Warten auf READY.");
    return false;
}

/* Sendet einen Ausdruck und gibt die Antwort aus.
 *
 * Unaufgeforderte READY-Zeilen werden ueberlesen: Das Board kann sich
 * jederzeit zuruecksetzen, etwa durch einen Spannungseinbruch oder durch
 * ein erneutes Oeffnen des Ports. Eine solche Zeile ist keine Antwort auf
 * die Anfrage und darf die Zuordnung nicht verschieben. */
void exchange(SerialPort& port, MessageLog& log, const std::string& request)
{
    port.writeLine(request);
    log.log(MessageLog::Direction::Sent, request);

    const auto deadline = std::chrono::steady_clock::now() + kReplyTimeout;

    while (std::chrono::steady_clock::now() < deadline) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        const auto reply = port.readLine(remaining);
        if (!reply) {
            break;
        }

        log.log(MessageLog::Direction::Received, *reply);

        if (reply->rfind("READY", 0) == 0) {
            log.note("Unaufgeforderte Begruessung ueberlesen.");
            continue;
        }

        std::cout << *reply << '\n';
        return;
    }

    log.note("Zeitueberschreitung nach Anfrage: " + request);
    std::cout << "Keine Antwort innerhalb des Zeitfensters.\n";
}

/* Liest die Ausdruecke einer Batchdatei ein. */
std::vector<std::string> readBatch(const std::string& path)
{
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Batchdatei " + path + " nicht zu oeffnen.");
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty() && line[0] != '#') {   /* '#' leitet Kommentare ein */
            lines.push_back(line);
        }
    }
    return lines;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseArguments(argc, argv, options)) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        MessageLog log(options.logfile);
        SerialPort port(options.port);
        log.note("Port geoeffnet: " + port.device());

        if (!awaitReady(port, log)) {
            std::cerr << "Der Mikrocontroller hat sich nicht gemeldet.\n";
            return 2;
        }

        if (!options.batch.empty()) {
            for (const auto& request : readBatch(options.batch)) {
                std::cout << "> " << request << '\n';
                exchange(port, log, request);
            }
            return 0;
        }

        std::cout << "Ausdruecke im Format \"Zahl Operator Zahl\" eingeben, "
                     "\"quit\" beendet.\n";
        std::string request;
        while (std::cout << "> " && std::getline(std::cin, request)) {
            if (request == "quit" || request == "exit") {
                break;
            }
            if (request.empty()) {
                continue;
            }
            exchange(port, log, request);
        }
        std::cout << '\n';
        return 0;

    } catch (const std::exception& error) {
        std::cerr << "Fehler: " << error.what() << '\n';
        return 3;
    }
}
