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

// Serial (ISP) programming mode of an AVR target, implemented from the
// "Serial Programming" chapter of the AVR datasheets (ATtiny13/x4/x5,
// ATmega48/88/168/328, ...) and Atmel application note AVR910.
//
// While RESET is held low the core is halted and the device listens on
// SCK/MOSI (SPI mode 0, MSB first) for 4 byte instructions and answers on
// MISO. The decoder acts directly on simavr's storage: avr->flash,
// avr->fuse[], avr->lockbits and the EEPROM (via AVR_IOCTL_EEPROM_*).
//
// This class only models the protocol. Electrical glue (which pins carry
// SCK/MOSI/MISO/RESET, driving MISO) lives in the MCU component.

#ifndef AVRISP_H
#define AVRISP_H

#include <stdint.h>
#include <vector>

struct avr_t;

struct AvrIspDevice {
    const char *name;   // simavr / SimulIDE device name
    uint8_t sig[3];     // Signature bytes
    uint16_t pageWords; // Flash page size in words
    uint8_t eePage;     // EEPROM page size in bytes
    uint32_t flashSize; // Flash size in bytes
    uint16_t eeSize;    // EEPROM size in bytes
    uint8_t nFuses;     // 1..3 (low, high, extended)
    uint8_t fuse[3];    // Factory default fuse values
    uint8_t eesaveFuse; // Fuse byte index holding EESAVE (0xFF: none)
    uint8_t eesaveBit;
    uint8_t rstdisblFuse; // Fuse byte index holding RSTDISBL (0xFF: none)
    uint8_t rstdisblBit;
    const char *rst; // Pin ids (SimulIDE package ids) of the ISP signals
    const char *sck;
    const char *mosi;
    const char *miso;
};

class AvrIsp
{
  public:
    enum Signal { None = 0, Reset, Sck, Mosi, Miso };

    AvrIsp();

    // Look up ISP data for a device name ("attiny85", "atmega328", ...).
    static const AvrIspDevice *device(const char *name);

    // Attach to a simavr core. Initializes default fuses/lock if needed.
    void setCpu(avr_t *cpu);
    avr_t *cpu()
    {
        return m_cpu;
    }
    const AvrIspDevice *dev()
    {
        return m_dev;
    }

    // RESET pin went low (true) or high (false).
    void setReset(bool low);
    // Clock edge on SCK. mosi is the MOSI level at this moment.
    void sckEdge(bool level, bool mosi);

    bool inReset() const
    {
        return m_inReset;
    }
    // True once the programmer started talking: MISO must be driven.
    bool misoEnabled() const
    {
        return m_inReset && m_misoEn;
    }
    bool misoBit() const
    {
        return m_misoBit;
    }
    // Programming Enable was accepted.
    bool progEnabled() const
    {
        return m_progEn;
    }
    // Something in non-volatile memory changed while in reset.
    bool memChanged() const
    {
        return m_memChanged;
    }
    // Number of flash pages written since reset went low.
    int pagesWritten() const
    {
        return m_pagesWritten;
    }

    // RSTDISBL fuse programmed: reset pin acts as GPIO.
    bool resetDisabled() const;

    // Execute one complete 4 byte instruction as if it was clocked in;
    // returns the byte shifted out while the 4th byte is received.
    uint8_t instruction(const uint8_t in[4]);

  private:
    void byteDone(uint8_t in);
    uint8_t readResult(const uint8_t *c); // Data out during the 4th byte
    void execute(const uint8_t *c);       // Side effects after the 4th byte
    uint8_t readEeprom(uint32_t addr);
    void writeEeprom(uint32_t addr, uint8_t val);
    void chipErase();
    bool locked() const;     // LB1 programmed: further programming disabled
    bool readLocked() const; // LB1+LB2 programmed: verification disabled

    avr_t *m_cpu;
    const AvrIspDevice *m_dev;
    AvrIspDevice m_generic; // Used when the device is not in the table

    bool m_inReset;
    bool m_progEn;
    bool m_misoEn;
    bool m_misoBit;
    bool m_sck;
    bool m_memChanged;
    int m_pagesWritten;

    uint8_t m_inShift;
    uint8_t m_outShift;
    int m_bitCount;
    int m_byteIdx;
    uint8_t m_cmd[4];
    uint8_t m_extAddr;

    std::vector<uint16_t> m_pageBuf; // Flash page buffer (words)
    std::vector<int16_t> m_eeBuf;    // EEPROM page buffer (-1: not loaded)
};

#endif
