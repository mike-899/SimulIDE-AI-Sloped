/***************************************************************************
 *   Copyright (C) 2026 SimulIDE contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.  *
 *                                                                         *
 ***************************************************************************/

// Minimal STK500 version 1 host ("uploader"), written from Atmel application
// note AVR061 ("STK500 Communication Protocol"). It talks to a programmer
// firmware such as ArduinoISP over a byte stream (the simulated UART) and
// programs/verifies the flash of the target behind it.
//
// No Qt dependency: the caller moves bytes (popTx/rxByte) and time (tick).

#ifndef STK500CLIENT_H
#define STK500CLIENT_H

#include <deque>
#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

struct AvrIspDevice;

class Stk500Client
{
  public:
    enum State { Idle, Running, Done, Failed };

    struct Options {
        bool erase = true;
        bool verify = true;
        int lfuse = -1; // -1: leave unchanged
        int hfuse = -1;
        int efuse = -1;
        double timeoutMs = 3000; // Per command (sync uses a shorter one)
    };

    Stk500Client();

    // Parse Intel HEX text into a flash image (0xFF filled). Returns false on error.
    static bool parseHex(const std::string &text, std::vector<uint8_t> &flash, std::string &error);

    // Start programming flash image into a target of type dev.
    bool start(const std::vector<uint8_t> &flash, const AvrIspDevice *dev, const Options &opt);
    void abort(const std::string &why);

    void rxByte(uint8_t b);      // Byte received from the programmer
    bool popTx(uint8_t &b);      // Next byte to send to the programmer
    bool hasTx() const { return !m_tx.empty(); }
    void tick(double nowMs);     // Drive timeouts

    State state() const { return m_state; }
    int progress() const;        // 0..100
    const std::string &error() const { return m_error; }

    std::function<void(const std::string &)> onLog;

  private:
    struct Request {
        std::vector<uint8_t> cmd;
        size_t respLen;          // Data bytes between INSYNC and OK
        std::function<bool(const std::vector<uint8_t> &)> check;
        std::string what;
        bool sync = false;
    };

    void log(const std::string &s);
    void push(Request r);
    void sendCurrent(double nowMs);
    void buildPlan();

    State m_state;
    std::string m_error;
    Options m_opt;
    const AvrIspDevice *m_dev;
    std::vector<uint8_t> m_flash;
    uint32_t m_used; // Bytes of flash to program (page aligned)

    std::deque<Request> m_plan;
    size_t m_total;
    std::deque<uint8_t> m_tx;
    std::vector<uint8_t> m_rx;
    bool m_waiting;
    double m_now;
    double m_sentAt;
    int m_syncTries;
    double m_drainUntil;
    std::vector<uint8_t> m_readBack;
};

#endif
