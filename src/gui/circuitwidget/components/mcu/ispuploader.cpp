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

#include "ispuploader.h"

#include "avrisp.h"
#include "baseprocessor.h"
#include "circuitwidget.h"
#include "mcucomponent.h"
#include "simulator.h"
#include "uartfeeder.h"
#include "utils.h"

#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <iostream>

IspUploader::IspUploader(McuComponent *programmer, QObject *parent) :
    QObject(parent)
{
    m_programmer   = programmer;
    m_feeder       = 0l;
    m_lastProgress = -1;

    m_client.onLog = [this](const std::string &s) {
        QString line = QString::fromStdString(s);
        m_log.append(line);
        std::cout << "ISP: " << s << std::endl;
    };
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(pump()));
}

IspUploader::~IspUploader()
{
    if (m_feeder) {
        bool pause = Simulator::self()->isRunning();
        if (pause)
            Simulator::self()->pauseSim();
        delete m_feeder;
        if (pause)
            Simulator::self()->resumeSim();
    }
}

QStringList IspUploader::devices()
{
    static const char *names[] = {
        "attiny13",   "attiny25",  "attiny45",   "attiny85",   "attiny24",
        "attiny44",   "attiny84",  "attiny2313", "attiny4313", "atmega8",
        "atmega48",   "atmega88",  "atmega168",  "atmega328",  "atmega16",
        "atmega32",   "atmega164", "atmega324",  "atmega644",  "atmega1284",
        "atmega1280", "atmega2560"
    };
    QStringList list;
    for (const char *n : names)
        list.append(n);
    return list;
}

QString IspUploader::guessTarget(McuComponent *programmer)
{
    for (McuComponent *mcu : McuComponent::mcuList()) {
        if (mcu == programmer)
            continue;
        if (AvrIsp::device(mcu->device().toLatin1().constData()))
            return mcu->device();
    }
    return "attiny85";
}

bool IspUploader::start(const QString &hexFile, const QString &targetDevice,
                        const Stk500Client::Options &opt)
{
    m_log.clear();
    m_error.clear();

    if (!m_programmer || !m_programmer->processor()) {
        m_error = tr("No programmer MCU");
        return false;
    }
    BaseProcessor *proc = m_programmer->processor();
    if (!proc->getLoadStatus()) {
        m_error =
            tr("The programmer MCU has no firmware: load ArduinoISP first");
        return false;
    }
    const AvrIspDevice *dev =
        AvrIsp::device(targetDevice.toLatin1().constData());
    if (!dev) {
        m_error = tr("Unknown target device: %1").arg(targetDevice);
        return false;
    }
    QFile file(hexFile);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        m_error = tr("Cannot read %1").arg(hexFile);
        return false;
    }
    std::vector<uint8_t> image;
    std::string err;
    if (!Stk500Client::parseHex(file.readAll().toStdString(), image, err)) {
        m_error = tr("Invalid HEX file %1: %2")
                      .arg(hexFile)
                      .arg(QString::fromStdString(err));
        return false;
    }
    if (!Simulator::self()->isRunning() && !Simulator::self()->isPaused()) {
        m_error = tr("Start the simulation first");
        return false;
    }

    // The feeder lives in the simulation clock list: add it while paused
    bool pause = Simulator::self()->isRunning();
    if (pause)
        Simulator::self()->pauseSim();
    if (!m_feeder)
        m_feeder = new UartFeeder("IspUploader-feeder");
    m_feeder->setTarget(proc, 0);
    m_feeder->setBaud(19200);
    m_feeder->clear();
    if (pause)
        Simulator::self()->resumeSim();

    connect(proc, SIGNAL(uartDataOut(int, int)), this, SLOT(uartOut(int, int)),
            Qt::UniqueConnection);

    if (!m_client.start(image, dev, opt)) {
        m_error = QString::fromStdString(m_client.error());
        return false;
    }
    m_clock.start();
    m_timer.start(2);
    return true;
}

void IspUploader::uartOut(int uart, int value)
{
    if (uart != 0)
        return;
    m_client.rxByte(value & 0xFF);
}

void IspUploader::pump()
{
    if (!m_programmer || !Simulator::self()->isRunning()) {
        if (!Simulator::self()->isPaused() || !m_programmer)
            m_client.abort("simulation stopped");
    }
    // Time base: simulated time, so a slow simulation does not time out
    m_client.tick(Simulator::self()->circTime() / 1e6);

    QByteArray out;
    uint8_t b;
    while (m_client.popTx(b))
        out.append((char)b);
    if (!out.isEmpty() && m_feeder)
        m_feeder->push(out);

    int p = m_client.progress();
    if (p != m_lastProgress) {
        m_lastProgress = p;
        emit progress(p);
    }
    if (m_client.state() != Stk500Client::Running)
        finish();
}

void IspUploader::finish()
{
    m_timer.stop();
    if (m_programmer && m_programmer->processor())
        disconnect(m_programmer->processor(), SIGNAL(uartDataOut(int, int)),
                   this, SLOT(uartOut(int, int)));

    if (!ok())
        m_error = QString::fromStdString(m_client.error());
    emit finished(ok());
}

void IspUploader::runDialog(McuComponent *programmer)
{
    static QString lastDir;

    QString hex = QFileDialog::getOpenFileName(
        0l, tr("Program target via ISP: firmware"), lastDir,
        tr("Hex Files (*.hex);;All files (*.*)"));
    if (hex.isEmpty())
        return;
    lastDir = QFileInfo(hex).absolutePath();

    QStringList devs = devices();
    int current      = devs.indexOf(guessTarget(programmer));
    bool ok          = false;
    QString target   = QInputDialog::getItem(
        0l, tr("Program target via ISP"),
        tr("Target device (wired to the programmer's ISP pins):"), devs,
        current < 0 ? 0 : current, false, &ok);
    if (!ok)
        return;

    if (!Simulator::self()->isRunning())
        CircuitWidget::self()->powerCircOn();

    IspUploader *up = new IspUploader(programmer);
    connect(up, &IspUploader::finished, [up](bool success) {
        if (success)
            MessageBoxNB(tr("ISP programming"), up->log());
        else
            MessageBoxNB(tr("ISP programming failed"),
                         up->log() + "\n\n" + up->error());
        up->deleteLater();
    });
    if (!up->start(hex, target)) {
        MessageBoxNB(tr("ISP programming failed"), up->error());
        up->deleteLater();
    }
}

#include "moc_ispuploader.cpp"
