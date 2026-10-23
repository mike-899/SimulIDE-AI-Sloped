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

#include "voltsource.h"
#include "connector.h"
#include "circuit.h"
#include "itemlibrary.h"


Component* VoltSource::construct( QObject* parent, QString type, QString id )
{
    return new VoltSource( parent, type, id );
}

LibraryItem* VoltSource::libraryItem()
{
    return new LibraryItem(
        tr( "Volt. Source" ),
        tr( "Sources" ),
        "voltsource.png",
        "Voltage Source",
        VoltSource::construct );
}

VoltSource::VoltSource( QObject* parent, QString type, QString id )
          : Component( parent, type, id )
          , eElement( id.toStdString() )
{
    m_area = QRect( -44, -68, 56, 80 );

    m_voltHight = 5.0;
    
    m_changed = false;

    m_voltw.setFixedSize( 46,70 );
    
    m_proxy = Circuit::self()->addWidget( &m_voltw );
    m_proxy->setParentItem( this );
    m_proxy->setPos( QPoint(-39, -63) );
    //m_proxy->setFlag(QGraphicsItem::ItemNegativeZStacksBehindParent, true );

    m_button = m_voltw.pushButton;
    m_dial   = m_voltw.dial;
    m_dial->setMaximum( 1000 );

    m_button->setText(QString("OFF"));

    // Transparent widget over the painted case: LCD style output button
    m_voltw.setAttribute(Qt::WA_TranslucentBackground);
    m_voltw.setStyleSheet("VoltWidget{background:transparent;}");
    m_button->setFixedSize(42, 17);
    m_button->setStyleSheet(
        "QPushButton{background:#0b1a0b;color:#a0a8a0;border:1px solid black;"
        "border-radius:2px;padding:0;font-family:monospace;font-weight:bold;"
        "font-size:10px;}"
        "QPushButton:checked{color:#5cff6a;}");
    m_button->setToolTip(tr("Output ON/OFF"));
    m_button->setCursor(Qt::PointingHandCursor);
    m_dial->setCursor(Qt::PointingHandCursor);

    QString nodid = id;
    nodid.append(QString("-outPin"));
    QPoint nodpos = QPoint(16,0);
    outpin = new Pin( 0, nodpos, nodid, 0, this );

    nodid.append("-eSource");
    m_out = new eSource( nodid.toStdString(), outpin );
    
    m_out->setVoltHigh( 0 );
    m_out->setVoltLow( 0 );
    
    m_unit = "V";
    setVolt(5.0);
    voltChanged( 0 );
    setValLabelPos(-26, 10 , 0 ); // x, y, rot 
    setShowVal( true );
    
    setLabelPos(-40,-80, 0);
    
    Simulator::self()->addToUpdateList( this );

    connect( m_button, SIGNAL( clicked()),
             this,     SLOT  ( onbuttonclicked()) );

    connect( m_dial,   SIGNAL( valueChanged(int) ),
             this,     SLOT  ( voltChanged(int)) );
}

VoltSource::~VoltSource() 
{ 
}

void VoltSource::updateStep()
{
    if( m_changed ) 
    {
        m_out->setVoltHigh( m_voltOut );
        m_out->stampOutput();
        updateButton();
        m_changed = false;
    }
}

void VoltSource::updateButton()
{
    QString msg;
    bool checked = m_button->isChecked();
    
    if( checked )
        msg = QString("%1 V").arg(float(int(m_voltOut*100))/100);
    else
        msg = QString("OFF");

    m_button->setText( msg );
}

void VoltSource::onbuttonclicked()
{
    m_out->setOut( m_button->isChecked() );
    updateButton();
    m_changed = true;
}

void VoltSource::voltChanged( int val )
{
    m_voltOut = double( m_voltHight*val/1000 );
    m_changed = true;
}

void VoltSource::setVolt( double v )            // Sets the Maximum Volt
{
    Component::setValue( v );       // Takes care about units multiplier
    m_voltHight = m_value*m_unitMult;
    voltChanged( m_dial->value() );
}

void VoltSource::setUnit( QString un ) 
{
    Component::setUnit( un );
    m_voltHight = m_value*m_unitMult;
    voltChanged( m_dial->value() );
}

void VoltSource::remove()
{
    if ( outpin->isConnected() ) outpin->connector()->remove();
    
    Simulator::self()->remFromUpdateList( this );
    delete m_out;
    
    Component::remove();
}

void VoltSource::paint( QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget )
{
    Component::paint( p, option, widget );

    // Bench power-supply module: dark case, knob (dial) and LCD (button)
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    QRectF caseR(-43, -67, 54, 78);
    QLinearGradient g(caseR.topLeft(), caseR.bottomLeft());
    g.setColorAt(0, QColor(78, 84, 92));
    g.setColorAt(1, QColor(38, 42, 47));
    p->setPen(QPen(isSelected() ? QColor(60, 130, 230) : QColor(21, 24, 27),
                   isSelected() ? 1.6 : 1));
    p->setBrush(g);
    p->drawRoundedRect(caseR, 4, 4);

    p->setPen(Qt::NoPen); // Knob bezel
    p->setBrush(QColor(25, 27, 30));
    p->drawEllipse(QPointF(-16, -37), 21, 21);

    QRadialGradient post(QPointF(7, -1), 4); // Red output post
    post.setColorAt(0, QColor(255, 120, 110));
    post.setColorAt(1, QColor(170, 20, 20));
    p->setBrush(post);
    p->setPen(QPen(QColor(90, 0, 0), 0.6));
    p->drawEllipse(QPointF(8, 0), 3.2, 3.2);
    p->restore();
}

#include "moc_voltsource.cpp"
