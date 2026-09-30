#include "boundary_fixture.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv)
{
    using namespace std::chrono_literals;
    if (argc != 2) {
        std::cerr << "Expected success, crash, timeout, incomplete, malformed or oversized.\n";
        return 2;
    }
    const std::string_view mode = argv[1];
    if (mode != "success" && mode != "crash" && mode != "timeout" &&
        mode != "incomplete" && mode != "malformed" && mode != "oversized") return 2;
#ifdef _WIN32
    // Suppress the intentional native failure's interactive Windows crash dialog.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    // The private fixture framing is byte-exact LF on every platform.
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 4;
#endif
    std::this_thread::sleep_for(300ms);
    if (mode == "success") {
        std::cout << sn022::complete_record;
        return 0;
    }
    if (mode == "malformed") {
        std::cout << sn022::complete_record << "trailing\n";
        return 0;
    }
    if (mode == "oversized") {
        std::cout << std::string(1024, 'x') << std::flush;
        return 0;
    }
    std::cout << sn022::partial_record << std::flush;
    if (mode == "incomplete") return 0;
    if (mode == "timeout") {
        std::this_thread::sleep_for(30s);
        return 0;
    }
#ifdef _WIN32
    RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    return 3;
#else
    std::abort();
#endif
}
