// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "devices/serial_port.hpp"

#include <algorithm>
#include <cstring>
#include <deque>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace simnodus::devices {

namespace {

// Sends back what it receives.
class LoopbackPort : public SerialPort {
public:
    std::size_t read(std::uint8_t* buffer, std::size_t size) override
    {
        std::size_t count = 0;
        while (count < size && !bytes_.empty()) {
            buffer[count++] = bytes_.front();
            bytes_.pop_front();
        }
        return count;
    }

    bool write(const std::uint8_t* bytes, std::size_t size) override
    {
        bytes_.insert(bytes_.end(), bytes, bytes + size);
        return true;
    }

private:
    std::deque<std::uint8_t> bytes_;
};

// "COM3" before "COM10": by the text before the number, then by the number.
bool port_order(const std::string& a, const std::string& b)
{
    const auto split = [](const std::string& s) {
        std::size_t digits = s.size();
        while (digits > 0 && s[digits - 1] >= '0' && s[digits - 1] <= '9') {
            --digits;
        }
        return std::make_pair(s.substr(0, digits), digits < s.size() ? std::stoul(s.substr(digits)) : 0ul);
    };
    return split(a) < split(b);
}

#ifdef _WIN32

std::string system_message(DWORD code)
{
    char* text = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        code, 0, reinterpret_cast<char*>(&text), 0, nullptr);
    std::string message = text ? text : "error " + std::to_string(code);
    LocalFree(text);
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == '.')) {
        message.pop_back();
    }
    return message;
}

class WindowsPort : public SerialPort {
public:
    explicit WindowsPort(HANDLE handle) : handle_(handle) {}
    ~WindowsPort() override { CloseHandle(handle_); }

    std::size_t read(std::uint8_t* buffer, std::size_t size) override
    {
        DWORD count = 0;
        if (!ReadFile(handle_, buffer, static_cast<DWORD>(size), &count, nullptr)) {
            return 0;
        }
        return count;
    }

    bool write(const std::uint8_t* bytes, std::size_t size) override
    {
        DWORD count = 0;
        return WriteFile(handle_, bytes, static_cast<DWORD>(size), &count, nullptr) && count == size;
    }

    bool modem_status(bool& dsr, bool& cts) override
    {
        DWORD status = 0;
        if (!GetCommModemStatus(handle_, &status)) {
            return false;
        }
        dsr = (status & MS_DSR_ON) != 0;
        cts = (status & MS_CTS_ON) != 0;
        return true;
    }

private:
    HANDLE handle_;
};

#else

std::string system_message(int code)
{
    return std::strerror(code);
}

speed_t speed_of(unsigned baud, bool& known)
{
    known = true;
    switch (baud) {
    case 300: return B300;
    case 1200: return B1200;
    case 2400: return B2400;
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
#ifdef B460800
    case 460800: return B460800;
#endif
#ifdef B921600
    case 921600: return B921600;
#endif
    default: known = false; return B9600;
    }
}

class PosixPort : public SerialPort {
public:
    explicit PosixPort(int fd) : fd_(fd) {}
    ~PosixPort() override { ::close(fd_); }

    std::size_t read(std::uint8_t* buffer, std::size_t size) override
    {
        const auto count = ::read(fd_, buffer, size);
        return count > 0 ? static_cast<std::size_t>(count) : 0;
    }

    bool write(const std::uint8_t* bytes, std::size_t size) override
    {
        std::size_t done = 0;
        for (int tries = 0; done < size && tries < 100; ++tries) {
            const auto count = ::write(fd_, bytes + done, size - done);
            if (count > 0) {
                done += static_cast<std::size_t>(count);
            } else if (count < 0 && errno != EAGAIN && errno != EINTR) {
                return false;
            } else {
                ::usleep(1000);
            }
        }
        return done == size;
    }

    bool modem_status(bool& dsr, bool& cts) override
    {
        int status = 0;
        if (::ioctl(fd_, TIOCMGET, &status) != 0) {
            return false;
        }
        dsr = (status & TIOCM_DSR) != 0;
        cts = (status & TIOCM_CTS) != 0;
        return true;
    }

private:
    int fd_;
};

#endif

} // namespace

std::unique_ptr<SerialPort> open_serial_port(const SerialSettings& settings, std::string& error)
{
    if (settings.name == loopback_port) {
        return std::make_unique<LoopbackPort>();
    }
    if (settings.name.empty()) {
        error = "no port chosen";
        return nullptr;
    }
#ifdef _WIN32
    // "\\.\COM10": ports above COM9 need the device namespace; it works for all.
    const std::string path = settings.name.rfind("\\\\.\\", 0) == 0 ? settings.name : "\\\\.\\" + settings.name;
    HANDLE handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = system_message(GetLastError());
        return nullptr;
    }
    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    GetCommState(handle, &dcb);
    dcb.BaudRate = settings.baud;
    dcb.ByteSize = static_cast<BYTE>(settings.data_bits);
    dcb.Parity = settings.parity == 'E' ? EVENPARITY : settings.parity == 'O' ? ODDPARITY : NOPARITY;
    dcb.StopBits = settings.stop_bits == 2 ? TWOSTOPBITS : ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = settings.parity != 'N';
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE; // many adapters and boards expect DTR on
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = FALSE;
    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = MAXDWORD; // with zero totals: return at once with what is there
    timeouts.WriteTotalTimeoutConstant = 200;
    if (!SetCommState(handle, &dcb) || !SetCommTimeouts(handle, &timeouts)) {
        error = system_message(GetLastError());
        CloseHandle(handle);
        return nullptr;
    }
    SetupComm(handle, 65536, 65536);
    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return std::make_unique<WindowsPort>(handle);
#else
    const int fd = ::open(settings.name.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        error = system_message(errno);
        return nullptr;
    }
    termios t{};
    bool known = false;
    const speed_t speed = speed_of(settings.baud, known);
    if (!known) {
        ::close(fd);
        error = "the computer does not offer " + std::to_string(settings.baud) + " baud";
        return nullptr;
    }
    if (tcgetattr(fd, &t) != 0) {
        error = system_message(errno);
        ::close(fd);
        return nullptr;
    }
    cfmakeraw(&t);
    cfsetispeed(&t, speed);
    cfsetospeed(&t, speed);
    t.c_cflag &= ~static_cast<tcflag_t>(CSIZE | PARENB | PARODD | CSTOPB | CRTSCTS);
    t.c_cflag |= CLOCAL | CREAD;
    t.c_cflag |= settings.data_bits == 5 ? CS5 : settings.data_bits == 6 ? CS6 : settings.data_bits == 7 ? CS7 : CS8;
    if (settings.parity != 'N') {
        t.c_cflag |= PARENB | (settings.parity == 'O' ? PARODD : 0);
    }
    if (settings.stop_bits == 2) {
        t.c_cflag |= CSTOPB;
    }
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &t) != 0) {
        error = system_message(errno);
        ::close(fd);
        return nullptr;
    }
    tcflush(fd, TCIOFLUSH);
    return std::make_unique<PosixPort>(fd);
#endif
}

std::vector<std::string> serial_port_names()
{
    std::vector<std::string> names;
#ifdef _WIN32
    // The ports Windows lists for its serial devices (USB adapters included).
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        for (DWORD i = 0;; ++i) {
            char value_name[256];
            DWORD name_size = sizeof(value_name);
            char data[256];
            DWORD data_size = sizeof(data) - 1;
            DWORD type = 0;
            const LONG result = RegEnumValueA(key, i, value_name, &name_size, nullptr, &type,
                reinterpret_cast<BYTE*>(data), &data_size);
            if (result != ERROR_SUCCESS) {
                break;
            }
            if (type == REG_SZ) {
                data[data_size] = '\0';
                names.emplace_back(data);
            }
        }
        RegCloseKey(key);
    }
#else
    // USB adapters and boards, and the serial ports with a device behind them.
    if (DIR* dir = ::opendir("/dev")) {
        while (const dirent* entry = ::readdir(dir)) {
            const std::string name = entry->d_name;
            const bool usb = name.rfind("ttyUSB", 0) == 0 || name.rfind("ttyACM", 0) == 0 || name.rfind("cu.", 0) == 0;
            bool serial = false;
            if (name.rfind("ttyS", 0) == 0) {
                const std::string device = "/sys/class/tty/" + name + "/device/driver";
                serial = ::access(device.c_str(), F_OK) == 0;
            }
            if (usb || serial) {
                names.push_back("/dev/" + name);
            }
        }
        ::closedir(dir);
    }
#endif
    names.erase(std::remove_if(names.begin(), names.end(), [](const std::string& n) { return n.empty(); }), names.end());
    std::sort(names.begin(), names.end(), port_order);
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

} // namespace simnodus::devices
