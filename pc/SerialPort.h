/*
 * SerialPort.h - Kapselung der seriellen Schnittstelle (POSIX/termios).
 *
 * Die Klasse folgt dem RAII-Prinzip: Der Konstruktor oeffnet und
 * konfiguriert den Port, der Destruktor schliesst ihn in jedem Fall -
 * auch dann, wenn die Ablaufsteuerung durch eine Ausnahme verlassen wird.
 * Kopieren ist unterbunden, Verschieben erlaubt, damit ein Dateideskriptor
 * nicht versehentlich doppelt geschlossen wird.
 */

#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <chrono>
#include <optional>
#include <string>

class SerialPort {
public:
    /* Oeffnet den Port und stellt ihn auf 9600 Baud, 8N1, ohne Fluss-
     * steuerung ein. Wirft std::runtime_error, wenn das misslingt. */
    SerialPort(const std::string& device, unsigned baud = 9600);
    ~SerialPort();

    SerialPort(const SerialPort&)            = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    /* Sendet die Zeichenkette und haengt "\n" an. */
    void writeLine(const std::string& line);

    /* Liest bis zum naechsten Zeilenende. Liefert std::nullopt, wenn
     * innerhalb des Zeitfensters keine vollstaendige Zeile eintraf. */
    std::optional<std::string> readLine(std::chrono::milliseconds timeout);

    const std::string& device() const { return device_; }

private:
    static unsigned toBaudConstant(unsigned baud);

    std::string device_;
    int         fd_{-1};
    std::string pending_;   /* bereits gelesene, noch unvollstaendige Zeile */
};

#endif /* SERIALPORT_H */
