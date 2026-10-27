/***************************************************************************
 *   Copyright (C) 2012 by santiago González                               *
 *   santigoro@gmail.com                                                   *
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

#ifndef SERIALPORT_H
#define SERIALPORT_H

#include "component.h"
#include "e-element.h"

#include <QPointer>
#include <QtSerialPort/QSerialPort>

class LibraryItem;
class McuComponent;
class QSocketNotifier;
class QTcpServer;
class QTcpSocket;
class UartFeeder;

class MAINMODULE_EXPORT SerialPort : public Component, public eElement
{
    Q_OBJECT
    Q_PROPERTY( bool     Auto_Open     READ autoOpen  WRITE setAutoOpen  DESIGNABLE true  USER true )
    Q_PROPERTY(Link_Mode Mode READ mode WRITE setMode DESIGNABLE true USER true)
    Q_PROPERTY(
        int Tcp_Port READ tcpPort WRITE setTcpPort DESIGNABLE true USER true)
    Q_PROPERTY(QString Mcu READ mcuId WRITE setMcuId DESIGNABLE true USER true)
    Q_PROPERTY( int      Mcu_Uart      READ uart  WRITE setUart  DESIGNABLE true  USER true )
    Q_PROPERTY( QString  Port_Name     READ port  WRITE setPort  DESIGNABLE true  USER true )
    Q_PROPERTY( QSerialPort::BaudRate BaudRate READ baudRate WRITE setBaudRate  DESIGNABLE true  USER true )
    Q_PROPERTY( QSerialPort::DataBits DataBits READ dataBits WRITE setDataBits  DESIGNABLE true  USER true )
    Q_PROPERTY( QSerialPort::Parity   Parity   READ parity   WRITE setParity    DESIGNABLE true  USER true )
    Q_PROPERTY( QSerialPort::StopBits StopBits READ stopBits WRITE setStopBits  DESIGNABLE true  USER true )
    Q_PROPERTY( QSerialPort::FlowControl FlowControl READ flowControl WRITE setFlowControl  DESIGNABLE true  USER true )
    Q_ENUMS(Link_Mode)

  public:
    // Serial_Port: a real port of the host (QSerialPort)
    // TCP_Server: listen on 127.0.0.1:Tcp_Port (avrdude -P net:127.0.0.1:PORT)
    // PTY: create a pseudo terminal (Linux/macOS), e.g. /dev/pts/3
    enum Link_Mode { Serial_Port = 0, TCP_Server, PTY };

    SerialPort(QObject *parent, QString type, QString id);
    ~SerialPort();

    static Component *construct(QObject *parent, QString type, QString id);
    static LibraryItem *libraryItem();

    void updateStep();

    bool autoOpen()
    {
        return m_autoOpen; }
        void setAutoOpen( bool ao ) { m_autoOpen = ao; }

        int uart() { return m_uart+1; }
        void setUart( int uart );

        QString port(){return m_portName;}
        void setPort( QString name ){ m_portName = name;}

        QSerialPort::BaudRate baudRate() { return m_BaudRate; }
        void setBaudRate( QSerialPort::BaudRate br ) { m_BaudRate = br; }

        QSerialPort::DataBits dataBits(){ return m_dataBits; }
        void setDataBits( QSerialPort::DataBits db ){ m_dataBits = db; }

        QSerialPort::Parity parity() { return m_parity; }
        void setParity( QSerialPort::Parity par ) { m_parity = par; }

        QSerialPort::StopBits stopBits() { return m_stopBits; }
        void setStopBits( QSerialPort::StopBits sb ) { m_stopBits = sb; }

        QSerialPort::FlowControl flowControl() { return m_flowControl; }
        void setFlowControl( QSerialPort::FlowControl fc ) { m_flowControl = fc; }

        Link_Mode mode()
        {
            return m_mode;
        }
        void setMode(Link_Mode mode);

        int tcpPort()
        {
            return m_tcpPort;
        }
        void setTcpPort(int port);

        // Id of the MCU whose UART is linked (empty: active MCU at creation)
        QString mcuId();
        void setMcuId(QString id);

        bool isOpen();
        // Where the host connects: port name, "127.0.0.1:5333" or "/dev/pts/N"
        QString linkName();
        void openLink()
        {
            open();
        }
        void closeLink()
        {
            close();
        }

        virtual void paint( QPainter* p, const QStyleOptionGraphicsItem* option, QWidget* widget );

    public slots:
        void onbuttonclicked();
        void slotWriteData( int uart, int value );
        void slotClose();
        void slotAutoOpen();

    private slots:
        void readData();
        void newTcpConnection();
        void readTcp();
        void tcpDisconnected();
        void readPty();

      private:
        void open();
        void close();
        McuComponent *mcu();
        void bindMcu(McuComponent *mcu);
        void hostData(const QByteArray &data);
        bool openPty();
        void closePty();

        QPushButton* m_button;
        QGraphicsProxyWidget* m_proxy;

        QSerialPort* m_serial;

        int m_uart;
        QPointer<McuComponent> m_mcu;
        bool m_active;
        bool m_autoOpen;

        QString m_portName;
        QSerialPort::BaudRate    m_BaudRate;
        QSerialPort::DataBits    m_dataBits;
        QSerialPort::Parity      m_parity;
        QSerialPort::StopBits    m_stopBits;
        QSerialPort::FlowControl m_flowControl;

        Link_Mode m_mode;
        int m_tcpPort;
        QString m_mcuId;
        QTcpServer *m_server;
        QPointer<QTcpSocket> m_client;
        int m_ptyFd;
        int m_ptySlaveFd;
        QString m_ptyName;
        QSocketNotifier *m_ptyNotifier;
        UartFeeder *m_feeder;
};

#endif

