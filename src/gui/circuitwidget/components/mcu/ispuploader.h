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

#ifndef ISPUPLOADER_H
#define ISPUPLOADER_H

#include "stk500client.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

class McuComponent;
class UartFeeder;

// Built-in "avrdude -c stk500v1": sends a .hex through the UART of a
// simulated MCU running a STK500v1 programmer firmware (ArduinoISP), which
// programs the target MCU wired to its ISP pins.
class IspUploader : public QObject
{
    Q_OBJECT
  public:
    IspUploader(McuComponent *programmer, QObject *parent = 0l);
    ~IspUploader();

    // Returns false (and sets error()) if it can not start.
    bool start(const QString &hexFile, const QString &targetDevice,
               const Stk500Client::Options &opt = Stk500Client::Options());

    bool running()
    {
        return m_client.state() == Stk500Client::Running;
    }
    bool ok()
    {
        return m_client.state() == Stk500Client::Done;
    }
    QString error()
    {
        return m_error;
    }
    QString log()
    {
        return m_log.join("\n");
    }

    // List of devices known by the ISP model (for UIs)
    static QStringList devices();
    // Guess the target: another AVR in the circuit with ISP support
    static QString guessTarget(McuComponent *programmer);

    // Interactive version (context menu)
    static void runDialog(McuComponent *programmer);

  signals:
    void finished(bool ok);
    void progress(int percent);

  private slots:
    void pump();
    void uartOut(int uart, int value);

  private:
    void finish();

    QPointer<McuComponent> m_programmer;
    UartFeeder *m_feeder;
    Stk500Client m_client;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QStringList m_log;
    QString m_error;
    int m_lastProgress;
};

#endif
