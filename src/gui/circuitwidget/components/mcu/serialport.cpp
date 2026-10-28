/***************************************************************************
 *   Copyright (C) 2018 by santiago González                               *
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

#include <QSocketNotifier>
#include <QTcpServer>
#include <QTcpSocket>
#include <iostream>

#ifdef Q_OS_UNIX
#    include <fcntl.h>
#    include <stdlib.h>
#    include <termios.h>
#    include <unistd.h>
#endif

#include "baseprocessor.h"
#include "circuit.h"
#include "circuitwidget.h"
#include "itemlibrary.h"
#include "mcucomponent.h"
#include "serialport.h"
#include "uartfeeder.h"
#include "utils.h"

static const char *SerialPort_properties[] = {
    QT_TRANSLATE_NOOP("App::Property", "Steps"),
    QT_TRANSLATE_NOOP("App::Property", "Port Name"),
    QT_TRANSLATE_NOOP("App::Property", "BaudRate"),
    QT_TRANSLATE_NOOP("App::Property", "DataBits"),
    QT_TRANSLATE_NOOP("App::Property", "Parity"),
    QT_TRANSLATE_NOOP("App::Property", "StopBits"),
    QT_TRANSLATE_NOOP("App::Property", "FlowControl"),
    QT_TRANSLATE_NOOP("App::Property", "Mode"),
    QT_TRANSLATE_NOOP("App::Property", "Tcp Port"),
    QT_TRANSLATE_NOOP("App::Property", "Mcu")
};

Component* SerialPort::construct( QObject* parent, QString type, QString id )
{
    return new SerialPort( parent, type, id );
}

LibraryItem* SerialPort::libraryItem()
{
    return new LibraryItem(
        "SerialPort",
        "",
        "SerialPort.png",
        "SerialPort",
        SerialPort::construct );
}

SerialPort::SerialPort( QObject* parent, QString type, QString id )
      : Component( parent, type, id )
      , eElement( (id+"-eElement").toStdString() )
{
    Q_UNUSED( SerialPort_properties );

    m_area = QRect( -34, -12, 100, 24 );
    setLabelPos(-34,-20 );

    m_serial = new QSerialPort( this );

    m_autoOpen = false;
    m_active = false;
    m_uart = 0;
    m_BaudRate    = QSerialPort::Baud9600;
    m_dataBits    = QSerialPort::Data8;
    m_parity      = QSerialPort::NoParity;
    m_stopBits    = QSerialPort::OneStop;
    m_flowControl = QSerialPort::NoFlowControl;

    m_mode        = Serial_Port;
    m_tcpPort     = 5333;
    m_server      = 0l;
    m_ptyFd       = -1;
    m_ptySlaveFd  = -1;
    m_ptyNotifier = 0l;
    m_feeder      = new UartFeeder((id + "-feeder").toStdString());

    m_button = new QPushButton( );
    m_button->setMaximumSize( 32,16 );
    m_button->setGeometry(-36,-16,32,16);
    m_button->setCheckable( true );
    m_button->setText( "Open" );

    m_proxy = Circuit::self()->addWidget( m_button );
    m_proxy->setParentItem( this );
    m_proxy->setPos( QPoint(-32, -8) );

    connect( m_button, SIGNAL( clicked() ),
                 this, SLOT(   onbuttonclicked() ));

    // Bound to the MCU that is active when the port is created (see Mcu
    // property)
    bindMcu(McuComponent::self());

    connect( m_serial, SIGNAL( readyRead()),
                 this, SLOT(   readData()) );

    Simulator::self()->addToUpdateList( this );
}

SerialPort::~SerialPort()
{
    close();
    Simulator::self()->remFromUpdateList( this );
    delete m_feeder;
}

void SerialPort::bindMcu(McuComponent *mcu)
{
    if (mcu == m_mcu)
        return;
    if (m_mcu) {
        disconnect(m_mcu, 0l, this, 0l);
        if (m_mcu->processor())
            disconnect(m_mcu->processor(), 0l, this, 0l);
    }
    m_mcu = mcu;
    if (!mcu)
        return;

    connect(mcu, SIGNAL(closeSerials()), this, SLOT(slotClose()));
    connect(mcu, SIGNAL(openSerials()), this, SLOT(slotAutoOpen()));
    if (mcu->processor())
        connect(mcu->processor(), SIGNAL(uartDataOut(int, int)), this,
                SLOT(slotWriteData(int, int)));
}

McuComponent *SerialPort::mcu()
{
    if (!m_mcuId.isEmpty()) {
        McuComponent *found = McuComponent::findMcu(m_mcuId);
        if (found)
            bindMcu(found);
    }
    if (!m_mcu)
        bindMcu(McuComponent::self());
    return m_mcu;
}

QString SerialPort::mcuId()
{
    if (m_mcu)
        return m_mcu->itemID();
    return m_mcuId;
}

void SerialPort::setMcuId(QString id)
{
    m_mcuId             = id;
    McuComponent *found = McuComponent::findMcu(id);
    if (found)
        bindMcu(found);
}

void SerialPort::setMode(Link_Mode mode)
{
    if (mode == m_mode)
        return;
    bool wasOpen = isOpen();
    if (wasOpen)
        close();
    m_mode = mode;
    if (wasOpen)
        open();
    update();
}

void SerialPort::setTcpPort(int port)
{
    if (port < 1)
        port = 1;
    if (port > 65535)
        port = 65535;
    m_tcpPort = port;
}

bool SerialPort::isOpen()
{
    if (m_mode == TCP_Server)
        return m_server && m_server->isListening();
    if (m_mode == PTY)
        return m_ptyFd >= 0;
    return m_serial->isOpen();
}

QString SerialPort::linkName()
{
    if (m_mode == TCP_Server)
        return "127.0.0.1:" + QString::number(m_tcpPort);
    if (m_mode == PTY)
        return m_ptyName;
    return m_portName;
}

void SerialPort::hostData(const QByteArray &data)
{
    m_active        = !m_active;
    McuComponent *m = mcu();
    if (!m || !m->processor())
        return;

    // Paced at the configured baud rate, from the simulation thread
    m_feeder->setTarget(m->processor(), m_uart);
    m_feeder->setBaud(m_BaudRate);
    m_feeder->push(data);
}

void SerialPort::newTcpConnection()
{
    QTcpSocket *sock = m_server->nextPendingConnection();
    if (!sock)
        return;
    if (m_client) // One host at a time: the newest wins
    {
        m_client->disconnect(this);
        m_client->abort();
        m_client->deleteLater();
    }
    m_client = sock;
    m_feeder->clear();
    connect(sock, SIGNAL(readyRead()), this, SLOT(readTcp()));
    connect(sock, SIGNAL(disconnected()), this, SLOT(tcpDisconnected()));
    std::cout << "SerialPort " << m_id.toStdString() << ": host connected from "
              << sock->peerAddress().toString().toStdString() << std::endl;
    update();
}

void SerialPort::readTcp()
{
    if (m_client)
        hostData(m_client->readAll());
}

void SerialPort::tcpDisconnected()
{
    std::cout << "SerialPort " << m_id.toStdString() << ": host disconnected"
              << std::endl;
    if (m_client)
        m_client->deleteLater();
    m_client = 0l;
    update();
}

bool SerialPort::openPty()
{
#ifdef Q_OS_UNIX
    int fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (fd < 0 || grantpt(fd) < 0 || unlockpt(fd) < 0) {
        if (fd >= 0)
            ::close(fd);
        return false;
    }
    m_ptyName = QString::fromLatin1(ptsname(fd));

    // Raw mode on the device side, as a real serial port
    struct termios t;
    m_ptySlaveFd = ::open(ptsname(fd), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_ptySlaveFd >= 0 && tcgetattr(m_ptySlaveFd, &t) == 0) {
        cfmakeraw(&t);
        tcsetattr(m_ptySlaveFd, TCSANOW, &t);
    }
    // Keeping the slave open avoids EIO on the master when the host closes it
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    m_ptyFd       = fd;
    m_ptyNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_ptyNotifier, SIGNAL(activated(int)), this, SLOT(readPty()));
    return true;
#else
    return false;
#endif
}

void SerialPort::closePty()
{
#ifdef Q_OS_UNIX
    if (m_ptyNotifier)
        delete m_ptyNotifier;
    m_ptyNotifier = 0l;
    if (m_ptyFd >= 0)
        ::close(m_ptyFd);
    if (m_ptySlaveFd >= 0)
        ::close(m_ptySlaveFd);
    m_ptyFd = m_ptySlaveFd = -1;
    m_ptyName.clear();
#endif
}

void SerialPort::readPty()
{
#ifdef Q_OS_UNIX
    char buf[512];
    QByteArray data;
    for (;;) {
        ssize_t n = ::read(m_ptyFd, buf, sizeof(buf));
        if (n <= 0)
            break;
        data.append(buf, n);
    }
    if (!data.isEmpty())
        hostData(data);
#endif
}

void SerialPort::updateStep()
{
    update();
}

void SerialPort::slotAutoOpen()
{
    if( m_autoOpen ) open();
}

void SerialPort::open()
{
    if (isOpen())
        close();
    mcu();
    m_feeder->clear();

    if (m_mode == TCP_Server) {
        if (!m_server) {
            m_server = new QTcpServer(this);
            connect(m_server, SIGNAL(newConnection()), this,
                    SLOT(newTcpConnection()));
        }
        if (m_server->listen(QHostAddress::LocalHost, m_tcpPort)) {
            std::cout << "SerialPort " << m_id.toStdString()
                      << ": listening on 127.0.0.1:" << m_tcpPort
                      << "  (avrdude -P net:127.0.0.1:" << m_tcpPort << ")"
                      << std::endl;
            m_button->setText("Close");
            m_button->setChecked(true);
        } else {
            m_button->setChecked(false);
            MessageBoxNB("Error", tr("Cannot listen on TCP port %1:\n%2.")
                                      .arg(m_tcpPort)
                                      .arg(m_server->errorString()));
        }
        m_active = false;
        update();
        return;
    }
    if (m_mode == PTY) {
        if (openPty()) {
            std::cout << "SerialPort " << m_id.toStdString()
                      << ": pseudo terminal " << m_ptyName.toStdString()
                      << std::endl;
            m_button->setText("Close");
            m_button->setChecked(true);
            setToolTip(m_ptyName);
        } else {
            m_button->setChecked(false);
            MessageBoxNB("Error",
                         tr("Cannot create a pseudo terminal on this system."));
        }
        m_active = false;
        update();
        return;
    }

    m_serial->setPortName( m_portName );
    m_serial->setBaudRate( m_BaudRate );
    m_serial->setDataBits( m_dataBits );
    m_serial->setParity(   m_parity );
    m_serial->setStopBits( m_stopBits );
    m_serial->setFlowControl( m_flowControl );

    if( m_serial->open(QIODevice::ReadWrite) )
    {
        qDebug()<<"Connected to" << m_portName;
        m_button->setText( "Close" );
    }
    else
    {
        m_button->setChecked( false );
        MessageBoxNB( "Error", tr("Cannot Open Port %1:\n%2.").arg(m_portName).arg(m_serial->errorString()) );
    }
    m_active = false;
    update();
}

void SerialPort::close()
{
    if( m_serial->isOpen() ) m_serial->close();
    if (m_client) {
        m_client->disconnect(this);
        m_client->abort();
        m_client->deleteLater();
        m_client = 0l;
    }
    if (m_server)
        m_server->close();
    closePty();
    m_feeder->clear();
    m_button->setText( "Open" );
    m_active = false;
    update();
    //ui->openButton->setEnabled( true );
    //ui->closeButton->setEnabled( false );
}

void SerialPort::readData()
{
    QByteArray data = m_serial->readAll();
    update();
    //qDebug()<<"SerialPort::readData" << data;
    hostData(data);
}

void SerialPort::slotWriteData( int uart, int value )
{
    if( uart != m_uart ) return;

    QByteArray ba;
    ba.resize(1);
    ba[0] = value;

    if (m_mode == TCP_Server) {
        if (m_client)
            m_client->write(ba);
        m_active = !m_active;
    }
#ifdef Q_OS_UNIX
    else if (m_mode == PTY) {
        if (m_ptyFd >= 0 && ::write(m_ptyFd, ba.constData(), 1) == 1)
            m_active = !m_active;
    }
#endif
    else if (m_serial->isOpen()) {
        m_serial->write( ba );
        m_active = !m_active;
        update();
        //qDebug() << "SerialPort::slotWriteData"<<value;
    }
}

void SerialPort::slotClose()
{
    close();
    Circuit::self()->removeComp( this );
}

void SerialPort::setUart( int uart )
{
    if      ( uart<1 ) uart = 1;
    else if ( uart>6 ) uart = 6;
    m_uart = uart-1;
    update();
}

void SerialPort::onbuttonclicked()
{
    if( m_button->isChecked() ) open();
    else                        close();
}

void SerialPort::paint( QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget )
{
    Component::paint( p, option, widget );
    
    p->setBrush( Qt::darkBlue );
    p->drawRoundedRect( m_area, 4, 4 );
    
    p->setBrush( Qt::white );
    QPen pen = p->pen();
    pen.setWidth( 0 );
    pen.setColor( QColor( 250, 210, 150 ) );
    p->setPen(pen);
    p->drawText( 10, 5, "Uart"+QString::number(m_uart+1) );

    if (m_mode != Serial_Port) // Show where the host connects
    {
        QString link =
            (m_mode == TCP_Server) ? "tcp " + linkName() : "pty " + linkName();
        if (!isOpen())
            link = (m_mode == TCP_Server) ? "tcp :" + QString::number(m_tcpPort)
                                          : "pty";
        QFont font = p->font();
        font.setPixelSize(8);
        p->setFont(font);
        pen.setColor(QColor(120, 120, 120));
        p->setPen(pen);
        p->drawText(-34, 22, link);
        pen.setColor(QColor(250, 210, 150));
        p->setPen(pen);
    }

    if (isOpen()) {
        if( m_active ) p->setBrush( Qt::yellow );
        else           p->setBrush( Qt::red );
        m_active = false;
    } else
        p->setBrush(Qt::black);
    p->drawEllipse( 48, -6, 12, 12);
    //p->drawRoundedRect( QRectF( -114, -79, 345, 185 ), 3, 3 );

}

#include "moc_serialport.cpp"


