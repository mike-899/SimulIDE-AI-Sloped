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

#include "uartfeeder.h"

#include "baseprocessor.h"
#include "simulator.h"

UartFeeder::UartFeeder(std::string id) : eElement(id)
{
    m_uart   = 0;
    m_nextNs = 0;
    setBaud(19200);
    Simulator::self()->addToSimuClockList(this);
}
UartFeeder::~UartFeeder()
{
    Simulator::self()->remFromSimuClockList(this);
}

void UartFeeder::setTarget(BaseProcessor *proc, int uart)
{
    QMutexLocker lock(&m_mutex);
    m_proc = proc;
    m_uart = uart;
}

void UartFeeder::setBaud(int baud)
{
    if (baud < 300)
        baud = 300;
    // 1 start + 8 data + 1 stop bits, plus a little margin
    m_byteNs = (uint64_t)(10.5 * 1e9 / baud);
}

void UartFeeder::push(const QByteArray &data)
{
    QMutexLocker lock(&m_mutex);
    m_queue.append(data);
}

void UartFeeder::clear()
{
    QMutexLocker lock(&m_mutex);
    m_queue.clear();
}

int UartFeeder::pending()
{
    QMutexLocker lock(&m_mutex);
    return m_queue.size();
}

void UartFeeder::simuClockStep()
{
    uint64_t now = Simulator::self()->circTime();
    if (m_nextNs > now + 2 * m_byteNs)
        m_nextNs = now; // Simulation restarted
    if (now < m_nextNs)
        return;

    uint8_t byte;
    BaseProcessor *proc;
    {
        QMutexLocker lock(&m_mutex);
        if (m_queue.isEmpty() || !m_proc)
            return;
        byte = m_queue.at(0);
        m_queue.remove(0, 1);
        proc = m_proc;
    }
    proc->uartIn(m_uart, byte);
    m_nextNs = now + m_byteNs;
}
