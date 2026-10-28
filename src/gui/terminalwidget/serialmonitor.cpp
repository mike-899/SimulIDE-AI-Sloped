/***************************************************************************
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

#include "serialmonitor.h"

#include "baseprocessor.h"
#include "mcucomponent.h"
#include "outpaneltext.h"
#include "thememanager.h"

SerialMonitor::SerialMonitor(QWidget *parent) : QWidget(parent), m_timer(this)
{
    setObjectName("serialMonitor");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_output = new OutPanelText(this);
    m_output->setObjectName("serialMonitorOutput");
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(2000);
    layout->addWidget(m_output, 1);

    QHBoxLayout *inLayout = new QHBoxLayout();
    inLayout->setSpacing(4);

    m_input = new QLineEdit(this);
    m_input->setObjectName("serialMonitorInput");
    m_input->setPlaceholderText(tr("Send to MCU..."));
    inLayout->addWidget(m_input, 1);

    m_sendButton = new QPushButton(tr("Send"), this);
    m_sendButton->setObjectName("serialMonitorSend");
    inLayout->addWidget(m_sendButton);
    layout->addLayout(inLayout);

    QHBoxLayout *optLayout = new QHBoxLayout();
    optLayout->setSpacing(8);

    m_uartBox = new QSpinBox(this);
    m_uartBox->setObjectName("serialMonitorUart");
    m_uartBox->setPrefix("Uart ");
    m_uartBox->setRange(1, 6);
    optLayout->addWidget(m_uartBox);

    m_addCrBox = new QCheckBox(tr("Add CR"), this);
    m_addCrBox->setObjectName("serialMonitorAddCr");
    optLayout->addWidget(m_addCrBox);

    m_asciiBox = new QCheckBox(tr("ASCII"), this);
    m_asciiBox->setObjectName("serialMonitorAscii");
    m_asciiBox->setChecked(true);
    m_asciiBox->setToolTip(
        tr("Print received bytes as ASCII (unchecked: numeric values)"));
    optLayout->addWidget(m_asciiBox);

    optLayout->addStretch();

    m_status = new QLabel(this);
    m_status->setObjectName("serialMonitorStatus");
    optLayout->addWidget(m_status);

    QToolButton *clearButton = new QToolButton(this);
    clearButton->setObjectName("serialMonitorClear");
    clearButton->setIcon(ThemeManager::icon("delete"));
    clearButton->setToolTip(tr("Clear"));
    clearButton->setAutoRaise(true);
    optLayout->addWidget(clearButton);
    layout->addLayout(optLayout);

    connect(m_input, SIGNAL(returnPressed()), this, SLOT(sendText()));
    connect(m_sendButton, SIGNAL(clicked()), this, SLOT(sendText()));
    connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));

    // Follow the active processor; also flushes buffered output.
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(poll()));
    m_timer.start(100);
    poll();
}
SerialMonitor::~SerialMonitor() {}

void SerialMonitor::attach(BaseProcessor *proc)
{
    if (m_proc)
        disconnect(m_proc, 0, this, 0);
    m_proc = proc;

    if (proc) {
        connect(proc, SIGNAL(uartDataOut(int, int)), this,
                SLOT(uartOut(int, int)));
        McuComponent *mcu = proc->component();
        QString name      = mcu ? mcu->itemID() : tr("MCU");
        m_status->setText(tr("Connected: %1").arg(name));
        if (!m_output->toPlainText().isEmpty())
            m_output->appendText("\n--- " + name + " ---\n");
    } else
        m_status->setText(tr("No MCU"));

    m_input->setEnabled(proc != 0l);
    m_sendButton->setEnabled(proc != 0l);
}

void SerialMonitor::poll()
{
    BaseProcessor *proc = BaseProcessor::self();
    if ((QObject *)proc != m_proc.data() || (!proc && m_input->isEnabled()))
        attach(proc);

    if (isVisible())
        m_output->step();
}

void SerialMonitor::uartOut(int uart, int value)
{
    if (uart != m_uartBox->value() - 1)
        return;

    if (m_asciiBox->isChecked())
        m_output->appendText(QString(QChar(value)));
    else
        m_output->appendText(QString::number(value) + " ");
}

void SerialMonitor::sendText()
{
    BaseProcessor *proc = BaseProcessor::self();
    if (!proc)
        return;

    int uart         = m_uartBox->value() - 1;
    QByteArray array = m_input->text().toLocal8Bit();

    for (int i = 0; i < array.size(); i++)
        proc->uartIn(uart, array.at(i));
    if (m_addCrBox->isChecked())
        proc->uartIn(uart, 13);

    m_input->clear();
}

void SerialMonitor::setUart(int uart)
{
    if (uart < 1)
        uart = 1;
    else if (uart > 6)
        uart = 6;
    m_uartBox->setValue(uart);
}

void SerialMonitor::clear()
{
    m_output->clear();
}

#include "moc_serialmonitor.cpp"
