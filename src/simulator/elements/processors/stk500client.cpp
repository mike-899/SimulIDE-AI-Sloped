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

#include "stk500client.h"
#include "avrisp.h"

#include <sstream>
#include <stdio.h>

// AVR061 constants
enum {
    STK_OK = 0x10,
    STK_FAILED = 0x11,
    STK_NOSYNC = 0x15,
    STK_INSYNC = 0x14,
    CRC_EOP = 0x20,
    STK_GET_SYNC = 0x30,
    STK_GET_PARAMETER = 0x41,
    STK_SET_DEVICE = 0x42,
    STK_SET_DEVICE_EXT = 0x45,
    STK_ENTER_PROGMODE = 0x50,
    STK_LEAVE_PROGMODE = 0x51,
    STK_LOAD_ADDRESS = 0x55,
    STK_UNIVERSAL = 0x56,
    STK_PROG_PAGE = 0x64,
    STK_READ_PAGE = 0x74,
    STK_READ_SIGN = 0x75,
    STK_SW_MAJOR = 0x81,
    STK_SW_MINOR = 0x82,
};

static const double SYNC_TIMEOUT_MS = 300;
static const int SYNC_TRIES = 10;

static std::string hex2(uint8_t v)
{
    char b[8];
    snprintf(b, sizeof(b), "%02X", v);
    return b;
}

Stk500Client::Stk500Client()
{
    m_state = Idle;
    m_dev = nullptr;
    m_used = 0;
    m_total = 0;
    m_waiting = false;
    m_now = 0;
    m_sentAt = 0;
    m_syncTries = 0;
    m_drainUntil = 0;
}

bool Stk500Client::parseHex(const std::string &text, std::vector<uint8_t> &flash, std::string &error)
{
    std::istringstream in(text);
    std::string line;
    uint32_t base = 0;
    int lineN = 0;
    flash.clear();

    auto hexv = [](const std::string &s, size_t pos, int n, bool &ok) {
        uint32_t v = 0;
        for (int i = 0; i < n; i++) {
            char c = pos + i < s.size() ? s[pos + i] : 'x';
            int d = (c >= '0' && c <= '9')   ? c - '0'
                    : (c >= 'A' && c <= 'F') ? c - 'A' + 10
                    : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                                             : -1;
            if (d < 0) {
                ok = false;
                return 0u;
            }
            v = (v << 4) | d;
        }
        return v;
    };

    while (std::getline(in, line)) {
        lineN++;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty())
            continue;
        if (line[0] != ':') {
            error = "line " + std::to_string(lineN) + ": missing ':'";
            return false;
        }
        bool ok = true;
        uint32_t len = hexv(line, 1, 2, ok);
        uint32_t addr = hexv(line, 3, 4, ok);
        uint32_t type = hexv(line, 7, 2, ok);
        if (!ok || line.size() < 11 + len * 2) {
            error = "line " + std::to_string(lineN) + ": malformed record";
            return false;
        }
        uint8_t sum = len + (addr >> 8) + (addr & 0xFF) + type;
        std::vector<uint8_t> data(len);
        for (uint32_t i = 0; i < len; i++) {
            data[i] = hexv(line, 9 + i * 2, 2, ok);
            sum += data[i];
        }
        sum += hexv(line, 9 + len * 2, 2, ok);
        if (!ok || sum != 0) {
            error = "line " + std::to_string(lineN) + ": bad checksum";
            return false;
        }
        if (type == 0) {
            uint32_t a = base + addr;
            if (a >= 0x800000) // EEPROM/fuse segments of avr-objcopy output
                continue;
            if (flash.size() < a + len)
                flash.resize(a + len, 0xFF);
            for (uint32_t i = 0; i < len; i++)
                flash[a + i] = data[i];
        } else if (type == 1)
            break;
        else if (type == 2 && len == 2)
            base = ((data[0] << 8) | data[1]) << 4;
        else if (type == 4 && len == 2)
            base = ((data[0] << 8) | data[1]) << 16;
    }
    if (flash.empty()) {
        error = "no data records";
        return false;
    }
    return true;
}

void Stk500Client::log(const std::string &s)
{
    if (onLog)
        onLog(s);
}

void Stk500Client::push(Request r)
{
    r.cmd.push_back(CRC_EOP);
    m_plan.push_back(r);
}

bool Stk500Client::start(const std::vector<uint8_t> &flash, const AvrIspDevice *dev, const Options &opt)
{
    m_plan.clear();
    m_tx.clear();
    m_rx.clear();
    m_error.clear();
    m_dev = dev;
    m_opt = opt;
    m_flash = flash;
    m_waiting = false;
    m_syncTries = 0;
    m_drainUntil = 0;

    if (!dev) {
        m_state = Failed;
        m_error = "unknown target device";
        return false;
    }
    if (flash.size() > dev->flashSize) {
        m_state = Failed;
        m_error = "image (" + std::to_string(flash.size()) + " bytes) does not fit in " + dev->name;
        return false;
    }
    uint32_t page = dev->pageWords * 2;
    m_used = ((flash.size() + page - 1) / page) * page;
    m_flash.resize(m_used, 0xFF);

    buildPlan();
    m_total = m_plan.size();
    m_state = Running;
    log(std::string("Programming ") + dev->name + ": " + std::to_string(flash.size()) + " bytes");
    return true;
}

void Stk500Client::buildPlan()
{
    const AvrIspDevice *d = m_dev;
    uint32_t page = d->pageWords * 2;

    Request r;
    r.cmd = {STK_GET_SYNC};
    r.respLen = 0;
    r.what = "sync";
    r.sync = true;
    push(r);

    r = Request();
    r.cmd = {STK_GET_PARAMETER, STK_SW_MAJOR};
    r.respLen = 1;
    r.what = "get version";
    r.check = [this](const std::vector<uint8_t> &v) {
        log("Programmer firmware version " + std::to_string(v[0]));
        return true;
    };
    push(r);

    // SET_DEVICE: 20 parameter bytes (AVR061 table "Parameters")
    r = Request();
    r.cmd = {STK_SET_DEVICE,
             0x00, // device code (not used by ArduinoISP)
             0x00, // revision
             0x00, // progtype: both parallel and serial
             0x01, // parmode: full parallel interface
             0x01, // polling supported
             0x01, // self timed
             0x01, // lock bytes
             d->nFuses,
             0xFF, 0xFF, // flash poll values
             0xFF, 0xFF, // EEPROM poll values
             uint8_t(page >> 8), uint8_t(page),
             uint8_t(d->eeSize >> 8), uint8_t(d->eeSize),
             uint8_t(d->flashSize >> 24), uint8_t(d->flashSize >> 16),
             uint8_t(d->flashSize >> 8), uint8_t(d->flashSize)};
    r.respLen = 0;
    r.what = "set device";
    push(r);

    r = Request();
    r.cmd = {STK_SET_DEVICE_EXT, 5, d->eePage, 0xD7, 0xA0, 0x00};
    r.respLen = 0;
    r.what = "set device ext";
    push(r);

    r = Request();
    r.cmd = {STK_ENTER_PROGMODE};
    r.respLen = 0;
    r.what = "enter programming mode";
    push(r);

    r = Request();
    r.cmd = {STK_READ_SIGN};
    r.respLen = 3;
    r.what = "read signature";
    r.check = [this](const std::vector<uint8_t> &v) {
        log("Device signature = 0x" + hex2(v[0]) + hex2(v[1]) + hex2(v[2]));
        if (v[0] != m_dev->sig[0] || v[1] != m_dev->sig[1] || v[2] != m_dev->sig[2]) {
            m_error = std::string("signature mismatch: expected 0x") + hex2(m_dev->sig[0]) + hex2(m_dev->sig[1]) +
                      hex2(m_dev->sig[2]) + " (" + m_dev->name + "). Check wiring and target device.";
            return false;
        }
        return true;
    };
    push(r);

    if (m_opt.erase) {
        r = Request();
        r.cmd = {STK_UNIVERSAL, 0xAC, 0x80, 0x00, 0x00};
        r.respLen = 1;
        r.what = "chip erase";
        r.check = [this](const std::vector<uint8_t> &) {
            log("Chip erased");
            return true;
        };
        push(r);
    }

    const uint8_t fuseCmd[3] = {0xA0, 0xA8, 0xA4};
    const int fuseVal[3] = {m_opt.lfuse, m_opt.hfuse, m_opt.efuse};
    for (int i = 0; i < 3; i++) {
        if (fuseVal[i] < 0 || i >= d->nFuses)
            continue;
        r = Request();
        r.cmd = {STK_UNIVERSAL, 0xAC, fuseCmd[i], 0x00, uint8_t(fuseVal[i])};
        r.respLen = 1;
        r.what = "write fuse";
        push(r);
    }

    // Program non blank pages
    std::vector<uint32_t> pages;
    for (uint32_t a = 0; a < m_used; a += page) {
        bool blank = true;
        for (uint32_t i = 0; i < page && blank; i++)
            blank = m_flash[a + i] == 0xFF;
        if (!blank || !m_opt.erase)
            pages.push_back(a);
    }
    for (uint32_t a : pages) {
        uint32_t w = a / 2;
        r = Request();
        r.cmd = {STK_LOAD_ADDRESS, uint8_t(w), uint8_t(w >> 8)};
        r.respLen = 0;
        r.what = "load address";
        push(r);

        r = Request();
        r.cmd = {STK_PROG_PAGE, uint8_t(page >> 8), uint8_t(page), 'F'};
        r.cmd.insert(r.cmd.end(), m_flash.begin() + a, m_flash.begin() + a + page);
        r.respLen = 0;
        r.what = "program page";
        push(r);
    }
    if (m_opt.verify) {
        for (uint32_t a : pages) {
            uint32_t w = a / 2;
            r = Request();
            r.cmd = {STK_LOAD_ADDRESS, uint8_t(w), uint8_t(w >> 8)};
            r.respLen = 0;
            r.what = "load address";
            push(r);

            r = Request();
            r.cmd = {STK_READ_PAGE, uint8_t(page >> 8), uint8_t(page), 'F'};
            r.respLen = page;
            r.what = "read page";
            r.check = [this, a, page](const std::vector<uint8_t> &v) {
                for (uint32_t i = 0; i < page; i++) {
                    if (v[i] != m_flash[a + i]) {
                        char b[96];
                        snprintf(b, sizeof(b), "verification error at 0x%04X: device 0x%02X != file 0x%02X",
                                 a + i, v[i], m_flash[a + i]);
                        m_error = b;
                        return false;
                    }
                }
                return true;
            };
            push(r);
        }
    }
    r = Request();
    r.cmd = {STK_LEAVE_PROGMODE};
    r.respLen = 0;
    r.what = "leave programming mode";
    push(r);
}

void Stk500Client::sendCurrent(double nowMs)
{
    if (m_plan.empty())
        return;
    m_rx.clear();
    for (uint8_t b : m_plan.front().cmd)
        m_tx.push_back(b);
    m_waiting = true;
    m_sentAt = nowMs;
}

void Stk500Client::abort(const std::string &why)
{
    if (m_state != Running)
        return;
    m_state = Failed;
    m_error = why;
    m_plan.clear();
    m_tx.clear();
    log("Error: " + why);
}

int Stk500Client::progress() const
{
    if (m_state == Done)
        return 100;
    if (!m_total)
        return 0;
    return int(100 * (m_total - m_plan.size()) / m_total);
}

bool Stk500Client::popTx(uint8_t &b)
{
    if (m_tx.empty())
        return false;
    b = m_tx.front();
    m_tx.pop_front();
    return true;
}

void Stk500Client::tick(double nowMs)
{
    m_now = nowMs;
    if (m_state != Running)
        return;
    if (!m_waiting) {
        if (nowMs < m_drainUntil)
            return;
        sendCurrent(nowMs);
        return;
    }
    if (!m_tx.empty()) // Still sending: restart timeout
    {
        m_sentAt = nowMs;
        return;
    }
    const Request &r = m_plan.front();
    double timeout = r.sync ? SYNC_TIMEOUT_MS : m_opt.timeoutMs;
    if (nowMs - m_sentAt < timeout)
        return;

    if (r.sync && ++m_syncTries < SYNC_TRIES) {
        log("Not in sync, retrying...");
        sendCurrent(nowMs);
        return;
    }
    abort("timeout waiting for response to '" + r.what + "' (is ArduinoISP running at the right baud rate?)");
}

void Stk500Client::rxByte(uint8_t b)
{
    if (m_state != Running || !m_waiting || m_plan.empty())
        return;
    const Request r = m_plan.front();

    if (m_rx.empty() && b != STK_INSYNC) {
        if (r.sync)
            return; // Garbage before sync: ignore
        abort("programmer not in sync during '" + r.what + "' (got 0x" + hex2(b) + ")");
        return;
    }
    m_rx.push_back(b);
    if (m_rx.size() < r.respLen + 2)
        return;

    if (m_rx.back() != STK_OK) {
        abort("'" + r.what + "' failed (status 0x" + hex2(m_rx.back()) + ")");
        return;
    }
    std::vector<uint8_t> data(m_rx.begin() + 1, m_rx.end() - 1);
    if (r.check && !r.check(data)) {
        abort(m_error.empty() ? "'" + r.what + "' check failed" : m_error);
        return;
    }
    m_plan.pop_front();
    m_waiting = false;
    m_rx.clear();

    if (m_plan.empty()) {
        m_state = Done;
        log(m_opt.verify ? "Flash written and verified" : "Flash written");
        return;
    }
    if (r.sync) {
        // Retried syncs may still be answered: let them drain first
        log("In sync with programmer");
        m_drainUntil = m_now + SYNC_TIMEOUT_MS;
        return;
    }
    sendCurrent(m_now);
}
