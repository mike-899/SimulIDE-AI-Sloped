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

#include "keypad.h"
#include "circuit.h"
#include "itemlibrary.h"

static const char* KeyPad_properties[] = {
    QT_TRANSLATE_NOOP("App::Property","Key Labels")
};

Component* KeyPad::construct( QObject* parent, QString type, QString id )
{ return new KeyPad( parent, type, id ); }

LibraryItem* KeyPad::libraryItem()
{
    return new LibraryItem(
            tr( "KeyPad" ),
            tr( "Switches" ),
            "keypad.png",
            "KeyPad",
            KeyPad::construct);
}

KeyPad::KeyPad( QObject* parent, QString type, QString id )
      : Component( parent, type, id )
      , eElement( id.toStdString() )
{
    Q_UNUSED( KeyPad_properties );
    
    setLabelPos(-8,-16, 0);
    
    m_keyLabels = "123456789*0#";
    m_rows = 4;
    m_cols = 3;
    setupButtons();
}
KeyPad::~KeyPad(){}

void KeyPad::attach()
{
    for( int row=0; row<m_rows; row++ )
    {
        Pin* rowPin = m_pin[row];
        eNode* rowNode = rowPin->getEnode();
        
        for( int col=0; col<m_cols; col++ )
        {
            Pin* colPin = m_pin[m_rows+col];
            eNode* colNode = colPin->getEnode();
            
            PushBase* button = m_buttons.at( row*m_cols+col );
            
            ePin* epin0 = button->getEpin( 0 );
            epin0->setEnode( rowNode );
            
            ePin* epin1 = button->getEpin( 1 );
            epin1->setEnode( colNode );
        }
    }
}

void KeyPad::setupButtons()
{
    bool pauseSim = Simulator::self()->isRunning();
    if( pauseSim ) Simulator::self()->pauseSim();

    // Membrane keypad: keys on a 24px pitch, all row/col pins on the ribbon
    // connector below the card (R1..Rn then C1..Cn, like real keypads)
    const int numPins  = m_rows + m_cols;
    const double cardW = m_cols * 24 + 8;
    int x0 = 8 * qRound(((cardW - 12) / 2 - (numPins - 1) * 4) / 8.0);
    m_area =
        QRectF(-6, -6, cardW, m_rows * 24 + 22)
            .united(QRectF(x0 - 6, m_rows * 24, (numPins - 1) * 8 + 12, 16));

    for( PushBase* button : m_buttons ) 
    {
       m_buttons.removeOne( button );
       Circuit::self()->removeComp( button );
    }
    
    for( Pin* pin : m_pin ) 
    {
        if( pin->isConnected() ) pin->connector()->remove();
        if( pin->scene() ) Circuit::self()->removeItem( pin );
        delete pin;
    }
    m_pin.resize(numPins);

    for (int i = 0; i < numPins; i++) {
        QString pinId = m_id + "-Pin" + QString::number(i);
        QPoint pinPos = QPoint(x0 + i * 8, m_rows * 24 + 24);
        m_pin[i]      = new Pin(270, pinPos, pinId, 0, this);
    }

    int labelMax = m_keyLabels.size()-1;

    for (int row = 0; row < m_rows; row++) {
        for( int col=0; col<m_cols; col++ )
        {
            QString butId =
                m_id + "button" + QString::number(row) + QString::number(col);
            PushBase* button = new PushBase( this, "PushBase", butId );
            button->SetupButton();
            button->setParentItem( this );
            button->setPos(QPointF(0, 0));
            button->setFlag( QGraphicsItem::ItemIsSelectable, false );
            m_buttons.append( button );

            // Invisible click area: the key itself is painted by KeyPad
            QPushButton *pb = button->button();
            pb->setMaximumSize(20, 20);
            pb->setGeometry(col * 24, row * 24, 20, 20);
            pb->setAttribute(Qt::WA_TranslucentBackground);
            pb->setStyleSheet("QPushButton{background:transparent;"
                              "border:none;color:transparent;}");
            pb->setCursor(Qt::PointingHandCursor);

            int pos = row*m_cols+col;
            QString buttonLabel = "";
            if( pos <= labelMax ) buttonLabel = m_keyLabels.mid( pos, 1 );
            button->setKey(buttonLabel);
        }
    }
    if( pauseSim ) Simulator::self()->resumeSim();
    Circuit::self()->update();
}

double KeyPad::rows()
{
    return m_rows;
}

void KeyPad::setRows( double rows )
{
    m_rows = rows;
    setupButtons();
}

double KeyPad::cols()
{
    return m_cols;
}

void KeyPad::setCols( double cols )
{
    m_cols = cols;
    setupButtons();
}

QString KeyPad::keyLabels()
{
    return m_keyLabels;
}
void KeyPad::setKeyLabels( QString keyLabels )
{
    m_keyLabels = keyLabels;
    
    int labelMax = m_keyLabels.size()-1;
    
    for( int row=0; row<m_rows; row++ )
    {
        for( int col=0; col<m_cols; col++ )
        {
            PushBase* button = m_buttons.at( row*m_cols+col );
            
            int pos = row*m_cols+col;
            QString buttonLabel = "";
            if( pos <= labelMax ) buttonLabel = m_keyLabels.mid( pos, 1 );
            button->setKey( buttonLabel );
        }
    }
}

void KeyPad::remove()
{
    for( PushBase* button : m_buttons ) 
        Circuit::self()->removeComp( button );

    Component::remove();
}

void KeyPad::paint( QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget )
{
    Component::paint( p, option, widget );

    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    const int numPins  = m_rows + m_cols;
    const double cardW = m_cols * 24 + 8;
    const double cardH = m_rows * 24 + 8;
    double px0         = m_pin[0]->pos().x();
    double pinsW       = (numPins - 1) * 8;

    // Ribbon cable + connector
    p->setPen(QPen(QColor(90, 79, 48), 0.6));
    p->setBrush(QColor(176, 156, 100));
    p->drawRect(QRectF(px0 - 3, cardH - 8, pinsW + 6, 12));
    p->setPen(QPen(QColor(120, 100, 60), 1.2));
    for (int i = 0; i < numPins; i++)
        p->drawLine(QPointF(px0 + i * 8, cardH - 6),
                    QPointF(px0 + i * 8, cardH + 4));
    p->setPen(QPen(Qt::black, 0.6));
    p->setBrush(QColor(30, 30, 30));
    p->drawRoundedRect(QRectF(px0 - 5, cardH + 3, pinsW + 10, 7), 1, 1);

    // Membrane card
    QPen cardPen(isSelected() ? QColor(60, 130, 230) : QColor(Qt::black),
                 isSelected() ? 1.5 : 0.8);
    p->setPen(cardPen);
    p->setBrush(QColor(29, 29, 29));
    p->drawRoundedRect(QRectF(-6, -6, cardW, cardH), 3, 3);

    // Keys
    QFont f = p->font();
    f.setPixelSize(11);
    f.setBold(true);
    p->setFont(f);
    for (int row = 0; row < m_rows; row++) {
        for (int col = 0; col < m_cols; col++) {
            int pos = row * m_cols + col;
            if (pos >= m_buttons.size())
                break;
            QString label = m_keyLabels.mid(pos, 1);
            bool letter   = !label.isEmpty() && label.at(0).isLetter();
            bool pressed  = m_buttons.at(pos)->button()->isChecked();

            QRectF kr(col * 24, row * 24, 20, 20);
            if (pressed)
                kr.translate(0.5, 0.8);
            QColor kc = letter ? QColor(198, 40, 40) : QColor(242, 242, 242);
            if (pressed)
                kc = kc.darker(135);

            QLinearGradient g(kr.topLeft(), kr.bottomLeft());
            g.setColorAt(0, kc.lighter(pressed ? 100 : 110));
            g.setColorAt(1, kc.darker(115));
            p->setPen(QPen(QColor(90, 90, 90), 0.5));
            p->setBrush(g);
            p->drawRoundedRect(kr, 2, 2);
            if (pressed) // Pressed key highlight
            {
                p->setPen(QPen(QColor(60, 160, 255), 1.2));
                p->setBrush(Qt::NoBrush);
                p->drawRoundedRect(kr.adjusted(0.6, 0.6, -0.6, -0.6), 2, 2);
            }
            p->setPen(letter ? Qt::white : QColor(34, 34, 34));
            p->drawText(kr, Qt::AlignCenter, label);
        }
    }
    p->restore();
}

#include "moc_keypad.cpp"
