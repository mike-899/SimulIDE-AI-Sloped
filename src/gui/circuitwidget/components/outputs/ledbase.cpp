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

#include "ledbase.h"

#include "connector.h"
#include "partart.h"
#include "pin.h"
#include "simulator.h"

#include <QPainterPath>

static const char* LedBase_properties[] = {
    QT_TRANSLATE_NOOP("App::Property","MaxCurrent"),
    QT_TRANSLATE_NOOP("App::Property","Grounded")
};

LedBase::LedBase( QObject* parent, QString type, QString id )
       : Component( parent, type, id )
       , eLed( id.toStdString() )
{
    Q_UNUSED( LedBase_properties );
    
    m_overCurrent = false;
    m_grounded = false;
    m_ground   = 0l;
    m_scrEnode = 0l;
    m_counter = 0;
    m_bright = 0;
    
    m_color = QColor( Qt::black );
    setColor( yellow );
    
    m_valLabel->setEnabled( false );
    m_valLabel->setVisible( false );

    Simulator::self()->addToUpdateList( this );
}
LedBase::~LedBase()
{ 
}

void LedBase::updateStep()
{
    uint bright = m_bright;
    eLed::updateBright();
    
    if( m_bright > 255+75 )
    {
        m_bright = 255+75;
        m_counter++;
        
        if( m_counter > 4 )
        {
            m_counter = 0;
            m_overCurrent = !m_overCurrent;
            update();
        }
    }
    else if( m_overCurrent )
    {
        m_overCurrent = false;
        m_counter = 0;
        update();
    }
    else if( bright != m_bright ) update();
}

bool LedBase::grounded()
{
    return m_grounded;
}

void LedBase::setGrounded( bool grounded )
{
    if( grounded == m_grounded ) return;
    
    bool pauseSim = Simulator::self()->isRunning();
    if( pauseSim )  Simulator::self()->pauseSim();

    if( grounded )
    {
        Pin* pin1 = (static_cast<Pin*>(m_ePin[1]));
        if( m_ePin[1]->isConnected() ) pin1->connector()->remove();
        pin1->setEnabled( false );
        pin1->setVisible( false );
        
        QString nodid = m_id;
        nodid.append(QString("Gnod-eSource"));
        
        m_ground = new eSource( nodid.toStdString(), m_ePin[1] );
        
        m_scrEnode = new eNode( nodid+"scr" );
        m_scrEnode->setNodeNumber(0);
        Simulator::self()->remFromEnodeList( m_scrEnode, /*delete=*/ false );
        
        m_ePin[1]->setEnode( m_scrEnode );
    }
    else
    {
        Pin* pin1 = (static_cast<Pin*>(m_ePin[1]));

        pin1->setEnabled( true );
        pin1->setVisible( true );
        
        delete m_ground;

        m_ground = 0l;
        m_scrEnode = 0l;
        
        m_ePin[1]->setEnode( 0l );
    }
    m_grounded = grounded;

    if( pauseSim ) Simulator::self()->runContinuous();
}

void LedBase::remove()
{
    if( m_ground ) delete m_ground;
    
    Simulator::self()->remFromUpdateList( this ); 
    
    Component::remove();
}

void LedBase::drawArtGlow(QPainter *p, QColor color)
{
    // Dome of mit/led.svg inside its art rect
    QRectF ar = PartArt::artRect(m_type);
    if (ar.isNull())
        return;
    QPointF c(ar.x() + ar.width() * 0.5, ar.y() + ar.height() * 0.24);
    double rad = ar.width() * 0.42;

    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);

    QColor base; // Tint for non red leds
    if (m_ledColor == yellow)
        base = QColor(240, 220, 40);
    else if (m_ledColor == green)
        base = QColor(40, 200, 60);
    else if (m_ledColor == blue)
        base = QColor(40, 110, 240);
    else if (m_ledColor == orange)
        base = QColor(255, 140, 20);
    else if (m_ledColor == purple)
        base = QColor(170, 60, 220);
    if (base.isValid()) {
        base.setAlpha(170);
        p->setBrush(base);
        double w = ar.width();
        QPainterPath dome; // Rounded top lens + rim
        dome.addRoundedRect(QRectF(ar.x() + w * 0.06,
                                   ar.y() + ar.height() * 0.01, w * 0.88,
                                   ar.height() * 0.52),
                            w * 0.44, w * 0.44);
        dome.addRect(QRectF(ar.x() + w * 0.06, ar.y() + ar.height() * 0.30,
                            w * 0.88, ar.height() * 0.23));
        dome.addRect(
            QRectF(ar.x(), ar.y() + ar.height() * 0.48, w, ar.height() * 0.08));
        dome.setFillRule(Qt::WindingFill);
        p->drawPath(dome);
    }
    int bright = m_overCurrent ? 255 : (int)m_bright;
    if (bright > 25) {
        QColor glow = m_overCurrent ? QColor(Qt::white) : color;
        double gr   = rad * (1.4 + bright / 255.0);
        QRadialGradient grad(c, gr);
        QColor c0 = glow;
        c0.setAlpha(qMin(255, 80 + bright));
        QColor c1 = glow;
        c1.setAlpha(bright / 3);
        QColor c2 = glow;
        c2.setAlpha(0);
        grad.setColorAt(0.0, QColor(255, 255, 255, qMin(255, bright)));
        grad.setColorAt(0.25, c0);
        grad.setColorAt(0.6, c1);
        grad.setColorAt(1.0, c2);
        p->setBrush(grad);
        p->drawEllipse(c, gr, gr);
    }
    p->restore();
}

void LedBase::paint( QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget )
{
    Component::paint( p, option, widget );

    QPen pen(Qt::black, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    
    QColor color;

    if( m_overCurrent )                               // Max Current
    {
        //m_bright = 0;
        p->setBrush( Qt::white );
        color = QColor( Qt::white );
        pen.setColor( color );
    }
    else
    {
        int overBight = 100;
        
        if( m_bright > 25 )
        {
            m_bright += 15;                          // Set a Minimun Bright
            
            if( m_bright > 255 ) 
            {
                overBight += m_bright-255;
                m_bright = 255;
            }
        }
        
        color = QColor( m_bright, m_bright, overBight ); // Default = yellow
        
        if     ( m_ledColor == red )    color = QColor( m_bright,  m_bright/3, overBight );
        else if( m_ledColor == green )  color = QColor( overBight, m_bright,   m_bright*2/3 );
        else if( m_ledColor == blue )   color = QColor( overBight, m_bright/2, m_bright );
        else if( m_ledColor == orange ) color = QColor( m_bright,  m_bright*2/3, overBight );
        else if( m_ledColor == purple ) color = QColor( m_bright,  overBight,  m_bright*2/3 );
    }
    if (artActive()) // Realistic part art
    {
        paintArt(p);
        drawArtGlow(p, color);
        return;
    }
    p->setPen(pen);
    drawBackground( p );
    
    pen.setColor( color );
    pen.setWidth(2.5);
    p->setPen(pen);
    p->setBrush( color );

    drawForeground( p );
}

#include "moc_ledbase.cpp"
