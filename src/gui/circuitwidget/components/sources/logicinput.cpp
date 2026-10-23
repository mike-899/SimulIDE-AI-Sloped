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

#include "connector.h"
#include "circuit.h"
#include "itemlibrary.h"
#include "logicinput.h"

static const char* LogicInput_properties[] = {
    QT_TRANSLATE_NOOP("App::Property","Voltage"),
    QT_TRANSLATE_NOOP("App::Property","Show Volt")
};


Component* LogicInput::construct( QObject* parent, QString type, QString id )
{ return new LogicInput( parent, type, id ); }

LibraryItem* LogicInput::libraryItem()
{
    return new LibraryItem(
        tr( "Fixed Volt." ),
        tr( "Sources" ),
        "voltage.png",
        "Fixed Voltage",
        LogicInput::construct );
}

LogicInput::LogicInput( QObject* parent, QString type, QString id )
          : Component( parent, type, id )
          , eElement( id.toStdString() )
{
    Q_UNUSED( LogicInput_properties );

    m_area = QRect( -10, -10, 20, 20 );
    
    setLabelPos(-64,-24 );
    
    m_changed = false;

    QString nodid = id;
    nodid.append(QString("-outnod"));
    QPoint nodpos = QPoint(16,0);
    m_outpin = new Pin( 0, nodpos, nodid, 0, this);

    nodid.append(QString("-eSource"));
    m_out = new eSource( nodid.toStdString(), m_outpin );
    
    m_unit = "V";
    setVolt(5.0);
    setValLabelPos(-16, 8 , 0 ); // x, y, rot 
    setShowVal( true );

    m_button = new QPushButton( );
    m_button->setFixedSize(26, 18);
    m_button->setCheckable( true );
    m_button->setText("OFF");
    m_button->setCursor(Qt::PointingHandCursor);
    // Rocker style power switch: grey OFF / green ON, readable in both themes
    m_button->setStyleSheet(
        "QPushButton{background:#5f6368;color:#eeeeee;border:1px solid #202124;"
        "border-radius:3px;font-size:8px;font-weight:bold;padding:0;}"
        "QPushButton:checked{background:#2e9d3e;color:white;}");

    m_proxy = Circuit::self()->addWidget( m_button );
    m_proxy->setParentItem( this );
    m_proxy->setPos(QPoint(-38, -9));

    Simulator::self()->addToUpdateList( this );

    connect( m_button, SIGNAL( clicked() ),
             this,     SLOT  ( onbuttonclicked() ));
}

LogicInput::~LogicInput() 
{
    //delete m_out;
}

void LogicInput::onbuttonclicked()
{
    m_out->setOut( m_button->isChecked() );
    m_button->setText(m_button->isChecked() ? "ON" : "OFF");
    m_changed = true;
    //qDebug() << "LogicInput::onbuttonclicked" ;
    //update();
}

void LogicInput::updateStep()
{
    if( m_changed ) 
    {
        m_out->stampOutput();
        m_changed = false;
        update();
    }
}

double LogicInput::volt()  
{ 
    return m_value; 
}

void LogicInput::setVolt( double v )
{
    Component::setValue( v );       // Takes care about units multiplier
    m_voltHight = m_value*m_unitMult;
    m_out->setVoltHigh( m_voltHight );
    m_changed = true;
    //update();
}

void LogicInput::setUnit( QString un ) 
{
    Component::setUnit( un );
    m_voltHight = m_value*m_unitMult;
    m_out->setVoltHigh( m_voltHight );
    m_changed = true;
}

void LogicInput::remove()
{
    if( m_outpin->isConnected() ) m_outpin->connector()->remove();
    delete m_out;
    
    Simulator::self()->remFromUpdateList( this );
    
    Component::remove();
}


void LogicInput::paint( QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget )
{
    Component::paint( p, option, widget );

    // Output terminal: red (live) when ON, dark grey when OFF
    p->setRenderHint(QPainter::Antialiasing);
    bool on = m_out->out();
    p->setPen(QPen(on ? QColor(120, 0, 0) : QColor(30, 30, 30), 1));
    p->setBrush(on ? QColor(230, 40, 40) : QColor(90, 90, 95));
    p->drawRoundedRect(QRectF(-8, -8, 16, 16), 3, 3);
    p->setBrush(on ? QColor(255, 150, 140) : QColor(140, 140, 145));
    p->setPen(Qt::NoPen);
    p->drawEllipse(QPointF(0, 0), 3, 3);
}

#include "moc_logicinput.cpp"

