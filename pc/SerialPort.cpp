#include "SerialPort.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace {
constexpr std::size_t kReadChunk = 64;
}

unsigned SerialPort::toBaudConstant(unsigned baud)
{
    switch (baud) {
    case 9600:   return B9600;
    case 19200:  return B19200;
    case 38400:  return B38400;
    case 57600:  return B57600;
    case 115200: return B115200;
    default:
        throw std::runtime_error("Nicht unterstuetzte Baudrate: " +
                                 std::to_string(baud));
    }
}

SerialPort::SerialPort(const std::string& device, unsigned baud)
    : device_(device)
{
    /* O_NOCTTY: kein steuerndes Terminal. */
    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        throw std::runtime_error("Port " + device + " nicht zu oeffnen: " +
                                 std::strerror(errno));
    }

    termios tty{};
    if (::tcgetattr(fd_, &tty) != 0) {
        ::close(fd_);
        throw std::runtime_error("tcgetattr fehlgeschlagen: " +
                                 std::string(std::strerror(errno)));
    }

    ::cfmakeraw(&tty);                       /* keine Zeichenumsetzung */
    const unsigned speed = toBaudConstant(baud);
    ::cfsetispeed(&tty, speed);
    ::cfsetospeed(&tty, speed);

    tty.c_cflag &= ~static_cast<tcflag_t>(PARENB);   /* kein Paritaetsbit */
    tty.c_cflag &= ~static_cast<tcflag_t>(CSTOPB);   /* ein Stoppbit      */
    tty.c_cflag &= ~static_cast<tcflag_t>(CSIZE);
    tty.c_cflag |= CS8;                              /* acht Datenbits    */
    tty.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);  /* keine HW-Flusskontrolle */
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 0;

    if (::tcsetattr(fd_, TCSANOW, &tty) != 0) {
        ::close(fd_);
        throw std::runtime_error("tcsetattr fehlgeschlagen: " +
                                 std::string(std::strerror(errno)));
    }

    ::tcflush(fd_, TCIOFLUSH);
}

SerialPort::~SerialPort()
{
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

SerialPort::SerialPort(SerialPort&& other) noexcept
    : device_(std::move(other.device_)),
      fd_(other.fd_),
      pending_(std::move(other.pending_))
{
    other.fd_ = -1;
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept
{
    if (this != &other) {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        device_   = std::move(other.device_);
        fd_       = other.fd_;
        pending_  = std::move(other.pending_);
        other.fd_ = -1;
    }
    return *this;
}

void SerialPort::writeLine(const std::string& line)
{
    const std::string payload = line + "\n";
    std::size_t written = 0;

    while (written < payload.size()) {
        const ssize_t n = ::write(fd_, payload.data() + written,
                                  payload.size() - written);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Schreiben fehlgeschlagen: " +
                                     std::string(std::strerror(errno)));
        }
        written += static_cast<std::size_t>(n);
    }
}

std::optional<std::string> SerialPort::readLine(std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    for (;;) {
        /* Schon eine vollstaendige Zeile im Zwischenpuffer? */
        const auto pos = pending_.find('\n');
        if (pos != std::string::npos) {
            std::string line = pending_.substr(0, pos);
            pending_.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return line;
        }

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
            return std::nullopt;
        }

        pollfd pfd{fd_, POLLIN, 0};
        const int ready = ::poll(&pfd, 1, static_cast<int>(remaining.count()));
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("poll fehlgeschlagen: " +
                                     std::string(std::strerror(errno)));
        }
        if (ready == 0) {
            return std::nullopt;                     /* Zeitfenster abgelaufen */
        }

        char buffer[kReadChunk];
        const ssize_t n = ::read(fd_, buffer, sizeof buffer);
        if (n > 0) {
            pending_.append(buffer, static_cast<std::size_t>(n));
        } else if (n < 0 && errno != EAGAIN && errno != EINTR) {
            throw std::runtime_error("Lesen fehlgeschlagen: " +
                                     std::string(std::strerror(errno)));
        }
    }
}
