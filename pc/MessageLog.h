/*
 * MessageLog.h - Protokollierung des Nachrichtenaustauschs.
 *
 * Jede gesendete und jede empfangene Zeile wird mit Zeitstempel und
 * Richtungskennung in eine Textdatei geschrieben. Format einer Zeile:
 *
 *     2026-08-12T14:21:07.412 TX 34 * 72
 *     2026-08-12T14:21:07.485 RX 34 * 72 = 2448
 *
 * Auch diese Klasse folgt dem RAII-Prinzip: Der Konstruktor oeffnet die
 * Datei, der Destruktor schliesst sie. Nach jedem Eintrag wird der Puffer
 * geleert, damit das Protokoll auch bei einem Programmabbruch vollstaendig
 * auf der Platte liegt.
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

    /* Vermerkt ein Ereignis ohne Nachrichtenbezug, etwa einen Zeitablauf. */
    void note(const std::string& text);

private:
    static std::string timestamp();

    std::ofstream out_;
    std::string   path_;
};

#endif /* MESSAGELOG_H */
