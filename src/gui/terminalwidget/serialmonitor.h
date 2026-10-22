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

#ifndef SERIALMONITOR_H
#define SERIALMONITOR_H

#include <QtWidgets>

class BaseProcessor;
class OutPanelText;

// Serial monitor docked at the bottom of the code panel (Tinkercad style).
// It follows whatever processor (MCU) is currently active in the circuit
// and shows the bytes it sends through the selected UART; text typed in
// the input line is sent to the MCU.
class MAINMODULE_EXPORT SerialMonitor : public QWidget
{
    Q_OBJECT

    public:
        SerialMonitor( QWidget* parent );
        ~SerialMonitor();

    public slots:
        void clear();

    private slots:
        void poll();
        void uartOut( int uart, int value );
        void sendText();

    private:
        void attach( BaseProcessor* proc );

        QPointer<QObject> m_proc;

        OutPanelText* m_output;
        QLineEdit*    m_input;
        QPushButton*  m_sendButton;
        QSpinBox*     m_uartBox;
        QCheckBox*    m_addCrBox;
        QCheckBox*    m_asciiBox;
        QLabel*       m_status;

        QTimer m_timer;
};

#endif
