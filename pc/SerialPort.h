/*
 * SerialPort.h - Serielle Schnittstelle (POSIX/termios).
 *
 * RAII: Konstruktor oeffnet und konfiguriert, Destruktor schliesst - auch
 * bei einer Ausnahme. Kopieren unterbunden, Verschieben erlaubt, damit ein
 * Dateideskriptor nicht doppelt geschlossen wird.
 */

#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <chrono>
#include <optional>
#include <string>

class SerialPort {
public:
    /* 9600 Baud, 8N1, ohne Flusssteuerung. Wirft std::runtime_error. */
    SerialPort(const std::string& device, unsigned baud = 9600);
    ~SerialPort();

    SerialPort(const SerialPort&)            = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    /* Sendet die Zeichenkette und haengt "\n" an. */
    void writeLine(const std::string& line);

    /* Liest bis zum Zeilenende; std::nullopt bei Zeitablauf. */
    std::optional<std::string> readLine(std::chrono::milliseconds timeout);

    const std::string& device() const { return device_; }

private:
    static unsigned toBaudConstant(unsigned baud);

    std::string device_;
    int         fd_{-1};
    std::string pending_;   /* bereits gelesene, noch unvollstaendige Zeile */
};

#endif /* SERIALPORT_H */
