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

#ifndef UARTFEEDER_H
#define UARTFEEDER_H

#include "e-element.h"

#include <QByteArray>
#include <QMutex>
#include <QPointer>

class BaseProcessor;

// Delivers bytes from the host side (TCP, pty, serial port, embedded
// uploader) to a simulated UART, one byte per frame time of *simulated*
// time and from the simulation thread. Bursts (e.g. a 128 byte STK500
// page) would otherwise overflow the MCU's receive FIFO.
class UartFeeder : public eElement
{
  public:
    UartFeeder(std::string id);
    ~UartFeeder();

    void setTarget(BaseProcessor *proc, int uart);
    void setBaud(int baud);

    void push(const QByteArray &data); // Thread safe
    void clear();
    int pending();

    virtual void simuClockStep();

  private:
    QMutex m_mutex;
    QByteArray m_queue;
    QPointer<BaseProcessor> m_proc;
    int m_uart;
    uint64_t m_byteNs;
    uint64_t m_nextNs;
};

#endif
