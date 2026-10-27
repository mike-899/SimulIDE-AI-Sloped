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

#include "avrisp.h"

#include <string.h>
#ifdef AVRISP_TRACE
#include <stdio.h>
#endif

// simavr
#include "avr_eeprom.h"
#include "sim_avr.h"

// Data from the "Memory Programming" chapters of the device datasheets.
// Fuse index: 0 = low, 1 = high, 2 = extended.
static const AvrIspDevice s_devices[] = {
    // name, signature, page words, EEPROM page, flash bytes, EEPROM bytes,
    // number of fuses, default fuses, EESAVE fuse/bit, RSTDISBL fuse/bit, RESET, SCK, MOSI, MISO
    {"attiny13", {0x1E, 0x90, 0x07}, 16, 4, 1024, 64, 2, {0x6A, 0xFF, 0xFF}, 0, 6, 1, 0, "PB5", "PB2", "PB0", "PB1"},
    {"attiny25", {0x1E, 0x91, 0x08}, 16, 4, 2048, 128, 3, {0x62, 0xDF, 0xFF}, 1, 3, 1, 7, "PB5", "PB2", "PB0", "PB1"},
    {"attiny45", {0x1E, 0x92, 0x06}, 32, 4, 4096, 256, 3, {0x62, 0xDF, 0xFF}, 1, 3, 1, 7, "PB5", "PB2", "PB0", "PB1"},
    {"attiny85", {0x1E, 0x93, 0x0B}, 32, 4, 8192, 512, 3, {0x62, 0xDF, 0xFF}, 1, 3, 1, 7, "PB5", "PB2", "PB0", "PB1"},
    {"attiny24", {0x1E, 0x91, 0x0B}, 16, 4, 2048, 128, 3, {0x62, 0xDF, 0xFF}, 1, 3, 1, 7, "PB3", "PA4", "PA6", "PA5"},
    {"attiny44", {0x1E, 0x92, 0x07}, 32, 4, 4096, 256, 3, {0x62, 0xDF, 0xFF}, 1, 3, 1, 7, "PB3", "PA4", "PA6", "PA5"},
    {"attiny84", {0x1E, 0x93, 0x0C}, 32, 4, 8192, 512, 3, {0x62, 0xDF, 0xFF}, 1, 3, 1, 7, "PB3", "PA4", "PA6", "PA5"},
    {"attiny2313", {0x1E, 0x91, 0x0A}, 16, 4, 2048, 128, 3, {0x64, 0xDF, 0xFF}, 1, 6, 1, 0, "PA2", "PB7", "PB5", "PB6"},
    {"attiny2313a", {0x1E, 0x91, 0x0A}, 16, 4, 2048, 128, 3, {0x64, 0xDF, 0xFF}, 1, 6, 1, 0, "PA2", "PB7", "PB5", "PB6"},
    {"attiny4313", {0x1E, 0x92, 0x0D}, 32, 4, 4096, 256, 3, {0x64, 0xDF, 0xFF}, 1, 6, 1, 0, "PA2", "PB7", "PB5", "PB6"},
    {"atmega8", {0x1E, 0x93, 0x07}, 32, 4, 8192, 512, 2, {0xE1, 0xD9, 0xFF}, 1, 3, 1, 7, "PC6", "PB5", "PB3", "PB4"},
    {"atmega48", {0x1E, 0x92, 0x05}, 32, 4, 4096, 256, 3, {0x62, 0xDF, 0xFF}, 1, 3, 1, 7, "PC6", "PB5", "PB3", "PB4"},
    {"atmega88", {0x1E, 0x93, 0x0A}, 32, 4, 8192, 512, 3, {0x62, 0xDF, 0xF9}, 1, 3, 1, 7, "PC6", "PB5", "PB3", "PB4"},
    {"atmega168", {0x1E, 0x94, 0x06}, 64, 4, 16384, 512, 3, {0x62, 0xDF, 0xF9}, 1, 3, 1, 7, "PC6", "PB5", "PB3", "PB4"},
    // SimulIDE's "atmega328" runs simavr's ATmega328P core: report the P signature
    {"atmega328", {0x1E, 0x95, 0x0F}, 64, 4, 32768, 1024, 3, {0x62, 0xD9, 0xFF}, 1, 3, 1, 7, "PC6", "PB5", "PB3", "PB4"},
    {"atmega328p", {0x1E, 0x95, 0x0F}, 64, 4, 32768, 1024, 3, {0x62, 0xD9, 0xFF}, 1, 3, 1, 7, "PC6", "PB5", "PB3", "PB4"},
    {"atmega16", {0x1E, 0x94, 0x03}, 64, 4, 16384, 512, 2, {0xE1, 0x99, 0xFF}, 1, 3, 0xFF, 0, "RST", "PB7", "PB5", "PB6"},
    {"atmega32", {0x1E, 0x95, 0x02}, 64, 4, 32768, 1024, 2, {0xE1, 0x99, 0xFF}, 1, 3, 0xFF, 0, "RST", "PB7", "PB5", "PB6"},
    {"atmega164", {0x1E, 0x94, 0x0A}, 64, 4, 16384, 512, 3, {0x62, 0x99, 0xFF}, 1, 3, 0xFF, 0, "RST", "PB7", "PB5", "PB6"},
    {"atmega324", {0x1E, 0x95, 0x08}, 64, 4, 32768, 1024, 3, {0x62, 0x99, 0xFF}, 1, 3, 0xFF, 0, "RST", "PB7", "PB5", "PB6"},
    {"atmega644", {0x1E, 0x96, 0x0A}, 128, 8, 65536, 2048, 3, {0x62, 0x99, 0xFF}, 1, 3, 0xFF, 0, "RST", "PB7", "PB5", "PB6"},
    {"atmega1284", {0x1E, 0x97, 0x05}, 128, 8, 131072, 4096, 3, {0x62, 0x99, 0xFF}, 1, 3, 0xFF, 0, "RST", "PB7", "PB5", "PB6"},
    {"atmega1280", {0x1E, 0x97, 0x03}, 128, 8, 131072, 4096, 3, {0x62, 0x99, 0xFF}, 1, 3, 0xFF, 0, "RST", "PB1", "PB2", "PB3"},
    {"atmega2560", {0x1E, 0x98, 0x01}, 128, 8, 262144, 4096, 3, {0x62, 0x99, 0xFF}, 1, 3, 0xFF, 0, "RST", "PB1", "PB2", "PB3"},
};

AvrIsp::AvrIsp()
{
    m_cpu = nullptr;
    m_dev = nullptr;
    m_inReset = false;
    m_progEn = false;
    m_misoEn = false;
    m_misoBit = false;
    m_sck = false;
    m_memChanged = false;
    m_pagesWritten = 0;
    m_inShift = 0;
    m_outShift = 0;
    m_bitCount = 0;
    m_byteIdx = 0;
    m_extAddr = 0;
    memset(m_cmd, 0, sizeof(m_cmd));
    memset(&m_generic, 0, sizeof(m_generic));
}

const AvrIspDevice *AvrIsp::device(const char *name)
{
    if (!name)
        return nullptr;
    for (const AvrIspDevice &d : s_devices)
        if (strcmp(d.name, name) == 0)
            return &d;
    return nullptr;
}

void AvrIsp::setCpu(avr_t *cpu)
{
    m_cpu = cpu;
    m_dev = nullptr;
    if (!cpu)
        return;

    m_dev = device(cpu->mmcu);
    if (!m_dev) // Unknown device: use what simavr knows
    {
        m_generic.name = cpu->mmcu;
        memcpy(m_generic.sig, cpu->signature, 3);
        m_generic.pageWords = 64;
        m_generic.eePage = 4;
        m_generic.flashSize = cpu->flashend + 1;
        m_generic.eeSize = cpu->e2end + 1;
        m_generic.nFuses = 3;
        memcpy(m_generic.fuse, cpu->fuse, 3);
        m_generic.eesaveFuse = 0xFF;
        m_generic.rstdisblFuse = 0xFF;
        m_dev = &m_generic;
    }
    // Start from the datasheet factory defaults (simavr's header derived
    // defaults are not always right). Called once, when the core is created.
    if (m_dev != &m_generic)
        memcpy(cpu->fuse, m_dev->fuse, 3);
    else if (cpu->fuse[0] == 0 && cpu->fuse[1] == 0 && cpu->fuse[2] == 0)
        memcpy(cpu->fuse, m_dev->fuse, 3);

    m_pageBuf.assign(m_dev->pageWords, 0xFFFF);
    m_eeBuf.assign(m_dev->eePage, -1);
}

bool AvrIsp::resetDisabled() const
{
    if (!m_cpu || !m_dev || m_dev->rstdisblFuse > 2)
        return false;
    return (m_cpu->fuse[m_dev->rstdisblFuse] & (1 << m_dev->rstdisblBit)) == 0;
}

bool AvrIsp::locked() const
{
    return m_cpu && (m_cpu->lockbits & 0x01) == 0;
}

bool AvrIsp::readLocked() const
{
    return m_cpu && (m_cpu->lockbits & 0x03) == 0;
}

void AvrIsp::setReset(bool low)
{
    if (low == m_inReset)
        return;
    m_inReset = low;
    m_progEn = false;
    m_misoEn = false;
    m_misoBit = false;
    m_bitCount = 0;
    m_byteIdx = 0;
    m_inShift = 0;
    m_outShift = 0;
    m_extAddr = 0;
    if (low) {
        m_memChanged = false;
        m_pagesWritten = 0;
        m_pageBuf.assign(m_pageBuf.size(), 0xFFFF);
        m_eeBuf.assign(m_eeBuf.size(), -1);
    }
}

void AvrIsp::sckEdge(bool level, bool mosi)
{
    if (level == m_sck)
        return;
    m_sck = level;
    if (!m_inReset || !m_cpu)
        return;

    if (level) // Rising edge: sample MOSI
    {
        m_inShift = (m_inShift << 1) | (mosi ? 1 : 0);
        if (++m_bitCount == 8) {
            m_bitCount = 0;
            byteDone(m_inShift);
            m_misoEn = true;
            m_misoBit = m_outShift & 0x80; // MSB ready for next byte
        }
    } else if (m_bitCount > 0) // Falling edge: shift out next bit
    {
        m_misoBit = (m_outShift >> (7 - m_bitCount)) & 1;
    }
}

void AvrIsp::byteDone(uint8_t in)
{
#ifdef AVRISP_TRACE
    fprintf(stderr, "[isp] byte %d: %02X\n", m_byteIdx, in);
#endif
    m_cmd[m_byteIdx] = in;
    m_outShift = in; // The device echoes the previous byte

    if (m_byteIdx == 2)
        m_outShift = readResult(m_cmd);

    if (++m_byteIdx == 4) {
        m_byteIdx = 0;
#ifdef AVRISP_TRACE
        fprintf(stderr, "[isp] %02X %02X %02X %02X -> %02X\n", m_cmd[0], m_cmd[1], m_cmd[2], m_cmd[3], m_outShift);
#endif
        execute(m_cmd);
    }
}

uint8_t AvrIsp::instruction(const uint8_t in[4])
{
    uint8_t out = readResult(in);
    execute(in);
    return out;
}

uint8_t AvrIsp::readResult(const uint8_t *c)
{
    if (!m_cpu || !m_dev)
        return 0;

    if (c[0] == 0xAC && c[1] == 0x53)
        return c[2]; // Programming Enable: echo
    if (!m_progEn)
        return c[2];

    uint32_t addr = ((uint32_t)c[1] << 8) | c[2];
    switch (c[0]) {
    case 0xF0:
        return 0x00; // Poll RDY/BSY: always ready
    case 0x20:       // Read program memory, low byte
    case 0x28:       // Read program memory, high byte
    {
        if (readLocked())
            return 0xFF;
        uint32_t byteAddr = (((uint32_t)m_extAddr << 16) | addr) * 2 + (c[0] == 0x28 ? 1 : 0);
        if (byteAddr > m_cpu->flashend)
            return 0xFF;
        return m_cpu->flash[byteAddr];
    }
    case 0xA0: // Read EEPROM memory
        if (readLocked())
            return 0xFF;
        return readEeprom(addr);
    case 0x30: // Read signature byte
        return (c[2] & 3) < 3 ? m_dev->sig[c[2] & 3] : 0xFF;
    case 0x38: // Read calibration byte
        return 0x80;
    case 0x50:
        if (c[1] == 0x00)
            return m_cpu->fuse[0]; // Low fuse
        if (c[1] == 0x08)
            return m_dev->nFuses > 2 ? m_cpu->fuse[2] : 0xFF; // Extended fuse
        return 0xFF;
    case 0x58:
        if (c[1] == 0x00)
            return m_cpu->lockbits; // Lock bits
        if (c[1] == 0x08)
            return m_dev->nFuses > 1 ? m_cpu->fuse[1] : 0xFF; // High fuse
        return 0xFF;
    default:
        return c[2];
    }
}

void AvrIsp::execute(const uint8_t *c)
{
    if (!m_cpu || !m_dev)
        return;

    if (c[0] == 0xAC && c[1] == 0x53) {
        m_progEn = true;
        return;
    }
    if (!m_progEn)
        return;

    uint32_t addr = ((uint32_t)c[1] << 8) | c[2];
    uint32_t pageMask = m_dev->pageWords - 1;

    switch (c[0]) {
    case 0xAC:
        switch (c[1] & 0xE0) {
        case 0x80:
            chipErase();
            break;
        case 0xA0:
            // Write fuses: 0xA0 low, 0xA8 high, 0xA4 extended
            if (c[1] == 0xA0)
                m_cpu->fuse[0] = c[3];
            else if (c[1] == 0xA8 && m_dev->nFuses > 1)
                m_cpu->fuse[1] = c[3];
            else if (c[1] == 0xA4 && m_dev->nFuses > 2)
                m_cpu->fuse[2] = c[3];
            m_memChanged = true;
            break;
        case 0xE0: // Write lock bits: bits can only be programmed (1 -> 0)
            m_cpu->lockbits &= (c[3] | 0xFC);
            m_memChanged = true;
            break;
        }
        break;
    case 0x4D: // Load extended address byte
        m_extAddr = c[2];
        break;
    case 0x40: // Load program memory page, low byte
    case 0x48: // Load program memory page, high byte
    {
        uint32_t w = addr & pageMask;
        uint16_t &word = m_pageBuf[w];
        if (c[0] == 0x40)
            word = (word & 0xFF00) | c[3];
        else
            word = (word & 0x00FF) | ((uint16_t)c[3] << 8);
        break;
    }
    case 0x4C: // Write program memory page
    {
        if (locked())
            break;
        uint32_t base = ((((uint32_t)m_extAddr << 16) | addr) & ~pageMask) * 2;
        for (uint32_t i = 0; i < m_dev->pageWords; i++) {
            uint32_t b = base + i * 2;
            if (b + 1 > m_cpu->flashend)
                break;
            // Flash cells can only be programmed (1 -> 0): erase first
            m_cpu->flash[b] &= m_pageBuf[i] & 0xFF;
            m_cpu->flash[b + 1] &= m_pageBuf[i] >> 8;
        }
        m_pageBuf.assign(m_pageBuf.size(), 0xFFFF);
        m_memChanged = true;
        m_pagesWritten++;
        break;
    }
    case 0xC0: // Write EEPROM memory (byte)
        if (!locked())
            writeEeprom(addr, c[3]);
        break;
    case 0xC1: // Load EEPROM memory page
        m_eeBuf[c[2] & (m_dev->eePage - 1)] = c[3];
        break;
    case 0xC2: // Write EEPROM memory page
    {
        if (!locked()) {
            uint32_t base = addr & ~(uint32_t)(m_dev->eePage - 1);
            for (int i = 0; i < m_dev->eePage; i++)
                if (m_eeBuf[i] >= 0)
                    writeEeprom(base + i, m_eeBuf[i]);
        }
        m_eeBuf.assign(m_eeBuf.size(), -1);
        break;
    }
    default:
        break;
    }
}

uint8_t AvrIsp::readEeprom(uint32_t addr)
{
    if (addr > m_cpu->e2end)
        return 0xFF;
    avr_eeprom_desc_t ee;
    ee.ee = 0;
    ee.offset = addr;
    ee.size = 1;
    if (avr_ioctl(m_cpu, AVR_IOCTL_EEPROM_GET, &ee) < 0 || !ee.ee)
        return 0xFF;
    return ee.ee[0];
}

void AvrIsp::writeEeprom(uint32_t addr, uint8_t val)
{
    if (addr > m_cpu->e2end)
        return;
    avr_eeprom_desc_t ee;
    ee.ee = &val;
    ee.offset = addr;
    ee.size = 1;
    avr_ioctl(m_cpu, AVR_IOCTL_EEPROM_SET, &ee);
    m_memChanged = true;
}

void AvrIsp::chipErase()
{
    memset(m_cpu->flash, 0xFF, m_cpu->flashend + 1);

    bool eesave = false;
    if (m_dev->eesaveFuse <= 2)
        eesave = (m_cpu->fuse[m_dev->eesaveFuse] & (1 << m_dev->eesaveBit)) == 0;

    if (!eesave && m_cpu->e2end) {
        std::vector<uint8_t> ff(m_cpu->e2end + 1, 0xFF);
        avr_eeprom_desc_t ee;
        ee.ee = ff.data();
        ee.offset = 0;
        ee.size = m_cpu->e2end + 1;
        avr_ioctl(m_cpu, AVR_IOCTL_EEPROM_SET, &ee);
    }
    m_cpu->lockbits = 0xFF;
    m_memChanged = true;
}
