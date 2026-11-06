# lwIP

The core of the lwIP TCP/IP stack, unmodified, for the ESP32-C3's Wi-Fi
gateway (experiment E-09, SN-268; decision DE9b). It ends the stations' TCP
connections; `src/emu/wifi_gateway.cpp` carries them on with the computer's
sockets.

| Item | Value |
|---|---|
| Version | 2.2.1, tag `STABLE-2_2_1_RELEASE` of https://github.com/lwip-tcpip/lwip |
| Archive | `https://github.com/lwip-tcpip/lwip/archive/refs/tags/STABLE-2_2_1_RELEASE.tar.gz`, downloaded 2026-11-06, SHA-256 `ce0b7461c0ad9602c376f0bf07c5eb7253b48c7bf66f011c6bf3e2a96731c539` (GitHub publishes no checksum for tag archives; this is the hash of the file used) |
| Copied | `COPYING`; `src/core/*.c`; `src/core/ipv4/*.c`; `src/include` without `lwip/apps` and `compat` |
| Modifications | None. The options (`lwipopts.h`) and the compiler glue (`arch/cc.h`, `arch/bpstruct.h`, `arch/epstruct.h`) are SimNodus's own, in `src/emu/lwip_port` (MIT) |
| License | BSD-3-Clause, [COPYING](COPYING); Copyright (c) 2001-2004 Swedish Institute of Computer Science and the lwIP contributors |

The Windows package installs `COPYING` as `licenses/lwip-BSD-3-Clause.txt`
and names lwIP in `THIRD-PARTY-NOTICES.txt`, as the license asks for binary
distributions.
