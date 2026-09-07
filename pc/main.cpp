/*
 * main.cpp - Ablaufsteuerung der PC-Anwendung.
 *
 *   uc-calc-pc --port <Geraet> [--log <Datei>] [--batch <Datei>]
 *
 * Interaktiv oder, mit --batch, zeilenweise aus einer Datei. Vor der ersten
 * Anfrage wird die Begruessungszeile "READY ..." abgewartet; damit ist der
 * DTR-Reset beim Oeffnen des Ports abgefangen.
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

/* false bei unvollstaendigem oder fehlerhaftem Aufruf. */
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
 * Unaufgeforderte READY-Zeilen werden ueberlesen - das Board kann sich
 * jederzeit zuruecksetzen, und eine solche Zeile wuerde sonst die Zuordnung
 * von Anfrage und Antwort verschieben. */
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

        std::cout << *reply << std::endl;
        return;
    }

    log.note("Zeitueberschreitung nach Anfrage: " + request);
    std::cout << "Keine Antwort innerhalb des Zeitfensters." << std::endl;
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
        for (;;) {
            std::cout << "> " << std::flush;

            if (!std::getline(std::cin, request)) {
                break;                  /* Eingabe beendet */
            }
            if (request == "quit" || request == "exit") {
                break;                  /* lokal, nicht an den uC senden */
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
