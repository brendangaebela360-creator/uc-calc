/*
 * MessageLog.h - Protokollierung des Nachrichtenaustauschs.
 *
 * Eine Zeile je Nachricht: Zeitstempel, Richtung (TX/RX), Text, z. B.
 *     2026-08-12T14:21:07.412 TX 34 * 72
 * RAII wie bei SerialPort. Nach jedem Eintrag wird geflusht, damit das
 * Protokoll auch bei einem Abbruch vollstaendig vorliegt.
 */

#ifndef MESSAGELOG_H
#define MESSAGELOG_H

#include <fstream>
#include <string>

class MessageLog {
public:
    enum class Direction { Sent, Received };

    explicit MessageLog(const std::string& path);
    ~MessageLog();

    MessageLog(const MessageLog&)            = delete;
    MessageLog& operator=(const MessageLog&) = delete;

    void log(Direction direction, const std::string& message);

    /* Ereignis ohne Nachrichtenbezug, etwa ein Zeitablauf. */
    void note(const std::string& text);

private:
    static std::string timestamp();

    std::ofstream out_;
    std::string   path_;
};

#endif /* MESSAGELOG_H */
