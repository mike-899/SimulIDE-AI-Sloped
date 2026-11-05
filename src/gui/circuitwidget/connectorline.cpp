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

#include "connectorline.h"

#include "circuit.h"
#include "circuitview.h"
#include "connector.h"
#include "node.h"
#include "thememanager.h"
#include "utils.h"

ConnectorLine::ConnectorLine( int x1, int y1, int x2, int y2, Connector* connector )
             : QGraphicsObject()
{
    setParent( connector );
    m_pConnector = connector;
    
    m_prevLine = 0l;
    m_nextLine = 0l;

    m_p1X = x1;
    m_p1Y = y1;
    m_p2X = x2;
    m_p2Y = y2;

    m_isBus  = false;
    m_moveP1 = false;
    m_moveP2 = false;
    m_moving = false;

    this->setFlag( QGraphicsItem::ItemIsSelectable, true );
    setAcceptHoverEvents(true);

    setCursor( Qt::CrossCursor );

    //setZValue( -0.5 );
    setZValue( 1 );

    updatePos();
}
ConnectorLine::~ConnectorLine(){}

// Handle sizes, in scene units
static const double kVertex = 3.0; // Half side of a corner handle
static const double kCenter = 3.2; // Radius of the segment center handle
static const double kGrab   = 4.0; // Half width of the clickable band

QRectF ConnectorLine::boundingRect() const
{
    QRectF r(QPointF(0, 0), QPointF(m_p2X - m_p1X, m_p2Y - m_p1Y));
    return r.normalized().adjusted(-7, -7, 7, 7);
}

void ConnectorLine::sSetP1( QPoint point )
{
    prepareGeometryChange();
    m_p1X = point.x();
    m_p1Y = point.y();
    updatePos();
}

void ConnectorLine::sSetP2( QPoint point )
{
    prepareGeometryChange();
    m_p2X = point.x();
    m_p2Y = point.y();
    updatePos();
}

void ConnectorLine::setP1( QPoint point )
{
    if( m_prevLine ) m_prevLine->sSetP2( point );
    sSetP1( point );
}

void ConnectorLine::setP2( QPoint point )
{
    if( m_nextLine ) m_nextLine->sSetP1( point );
    sSetP2( point );
}

void ConnectorLine::moveSimple( QPointF delta )
{
    bool deltaH  = fabs( delta.x() )> 0;
    bool deltaV  = fabs( delta.y() )> 0;
    
    prepareGeometryChange();

    m_p1X = m_p1X + delta.x();
    m_p1Y = m_p1Y + delta.y();
    m_p2Y = m_p2Y + delta.y();
    m_p2X = m_p2X + delta.x();

    bool isHoriz = ( dy() == 0 ) && ( dx() != 0 );
    
    if( m_prevLine && !(m_prevLine->isSelected()) )
    {
        m_prevLine->moveLine( delta.toPoint() );
        
        if( (  isHoriz && deltaV )
          ||( !isHoriz && deltaH ))
            m_prevLine->sSetP2( QPoint( m_p1X, m_p1Y) );
            
        m_prevLine->updatePos();
        m_prevLine->updatePrev();
    }
    if( m_nextLine && !(m_nextLine->isSelected()) ) 
    {
        m_nextLine->moveLine( delta.toPoint() ); 
        
        if( (  isHoriz && deltaV )
          ||( !isHoriz && deltaH ))
            m_nextLine->sSetP1( QPoint( m_p2X, m_p2Y) );
            
        m_nextLine->updatePos();
        m_nextLine->updateNext();
    }
    updatePos();
}

void ConnectorLine::move( QPointF delta )
{
    // If contiguous lines are also selected, just move line.
    bool moveSimple = false;
    /* !( ( m_prevLine && !(m_prevLine->isSelected()) )
                       ||( m_nextLine && !(m_nextLine->isSelected()) ));*/
                       
    if( Circuit::self()->pasting() || moveSimple )
    {
        prepareGeometryChange();
        m_p1Y = m_p1Y + delta.y();
        m_p2Y = m_p2Y + delta.y();
        m_p1X = m_p1X + delta.x();
        m_p2X = m_p2X + delta.x();
        updatePos();

        return;
    }
    int myindex = m_pConnector->lineList()->indexOf( this );
    if( ( myindex == 0 ) || ( myindex == m_pConnector->lineList()->size()-1 ) )
        return;    //avoid moving first or last line

    moveLine( delta.toPoint() );
    updatePos();
    updateLines();
    m_pConnector->refreshPointList();
}

void ConnectorLine::moveLine( QPoint delta )
{
    prepareGeometryChange();

    if( /*( dy() == 0 ) &&*/ ( dx() != 0 ) )
    {
       m_p1Y = m_p1Y + delta.y();
       m_p2Y = m_p2Y + delta.y();
    }
    if( /*( dx() == 0 ) &&*/ ( dy() != 0 ) )
    {
       m_p1X = m_p1X + delta.x();
       m_p2X = m_p2X + delta.x();
    }
    //else return;                     //line is "0"
}

void ConnectorLine::updateLines()
{
    updatePrev();
    updateNext();
}

void ConnectorLine::updatePrev()
{
    if( m_prevLine ) m_prevLine->sSetP2( QPoint( m_p1X, m_p1Y) );
}

void ConnectorLine::updateNext()
{
    if( m_nextLine ) 
    {
        m_nextLine->sSetP1( QPoint( m_p2X, m_p2Y) );
        m_nextLine->updatePos();
    }
}

void ConnectorLine::updatePos()
{
    setPos( m_p1X, m_p1Y );
    
    update();
}

void ConnectorLine::setPrevLine( ConnectorLine* prevLine )
{
    m_prevLine = prevLine;
}

void ConnectorLine::setNextLine( ConnectorLine* nextLine )
{
    m_nextLine = nextLine;
}

void ConnectorLine::remove() 
{ 
    if( !isSelected() ) Circuit::self()->clearSelection();
    setSelected( true );
    Circuit::self()->removeItems();

    //m_pConnector->remove(); 
}

bool ConnectorLine::conSelected() const
{
    for (ConnectorLine *line : *m_pConnector->lineList())
        if (line->isSelected())
            return true;
    return false;
}

void ConnectorLine::selectWire()
{
    Circuit::self()->clearSelection();
    for (ConnectorLine *line : *m_pConnector->lineList())
        line->setSelected(true);
}

// What a press at scenePos grabs: an inner corner (moves freely, the
// neighbour segments stop being parallel) or the segment (moves parallel).
// The ends on pins are not movable.
ConnectorLine::DragMode ConnectorLine::hitTest(QPointF scenePos) const
{
    QList<ConnectorLine *> *lines = m_pConnector->lineList();
    int index = lines->indexOf(const_cast<ConnectorLine *>(this));
    double r  = kVertex + 2;

    QPointF d1 = scenePos - QPointF(m_p1X, m_p1Y);
    QPointF d2 = scenePos - QPointF(m_p2X, m_p2Y);
    if (index > 0 && fabs(d1.x()) <= r && fabs(d1.y()) <= r)
        return DragP1;
    if (index < lines->size() - 1 && fabs(d2.x()) <= r && fabs(d2.y()) <= r)
        return DragP2;
    return DragSegment;
}

bool ConnectorLine::connectAt(QPoint point1)
{
    Circuit *circ = Circuit::self();
    if (circ->is_constarted()) {
        Connector *con = circ->getNewConnector();
        if (con->isBus() != m_isBus)
            return false; // Bus with no-Bus
        if (con->enode() == m_pConnector->enode())
            return false; // Same net
    }
    int index;
    int myindex = m_pConnector->lineList()->indexOf(this);
    int last    = m_pConnector->lineList()->size() - 1;

    if (((dy() == 0 && abs(point1.x() - m_p2X) < 8) // Near the p2 corner
         || (dx() == 0 && abs(point1.y() - m_p2Y) < 8)) &&
        myindex != last) {
        point1 = p2();
        index  = myindex + 1;
    } else if (((dy() == 0 && abs(point1.x() - m_p1X) < 8) // Near the p1 corner
                || (dx() == 0 && abs(point1.y() - m_p1Y) < 8)) &&
               myindex != 0) {
        point1 = p1();
        index  = myindex;
    } else // Split this line in two
    {
        if (dy() == 0)
            point1.setY(m_p1Y);
        else
            point1.setX(m_p1X);

        index               = myindex + 1;
        ConnectorLine *line = new ConnectorLine(point1.x(), point1.y(), m_p2X,
                                                m_p2Y, m_pConnector);
        m_pConnector->addConLine(line, index);
    }
    QString type = QString("Node");
    QString id   = type + "-" + circ->newSceneId();

    Node *node = new Node(0, type, id); // Add the Node
    node->setPos(point1.x(), point1.y());
    circ->addItem(node);

    bool pauseSim = Simulator::self()->isRunning();
    if (pauseSim)
        Simulator::self()->pauseSim();

    m_pConnector->splitCon(index, node->getPin(0), node->getPin(2));
    node->getPin(1)->setEnode(m_pConnector->enode());

    if (circ->is_constarted())
        circ->closeconnector(node->getPin(1)); // Ends here
    else
        circ->newconnector(node->getPin(1)); // Starts here

    if (pauseSim)
        Simulator::self()->runContinuous();
    return true;
}

void ConnectorLine::mousePressEvent( QGraphicsSceneMouseEvent* event )
{
    Circuit *circ = Circuit::self();
    QPoint point  = togrid(event->scenePos()).toPoint();

    if (event->button() == Qt::MiddleButton) // Move the segment
    {
        event->accept();
        m_drag    = DragSegment;
        m_dragged = false;
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    event->accept();

    if (circ->is_constarted()) // Legacy click wiring: end here
    {
        if (!connectAt(point))
            event->ignore();
        return;
    }
    if (event->modifiers() & Qt::ControlModifier) // Toggle selection
    {
        bool sel = !conSelected();
        for (ConnectorLine *line : *m_pConnector->lineList())
            line->setSelected(sel);
        return;
    }
    if (event->modifiers() & Qt::ShiftModifier) // Branch: drag a new wire
    {
        bool horiz = dy() == 0; // Leave this wire at a right angle
        if (connectAt(point))
            circ->beginWireDrag(horiz ? 2 : 1);
        return;
    }
    if (!conSelected())
        selectWire();

    m_drag    = hitTest(event->scenePos());
    m_dragged = false;
}

void ConnectorLine::mouseMoveEvent( QGraphicsSceneMouseEvent* event )
{
    event->accept();
    if (m_drag == NoDrag || Circuit::self()->wireDragging())
        return;

    QPoint delta = togrid(event->scenePos()).toPoint() -
                   togrid(event->lastScenePos()).toPoint();
    if (delta.isNull())
        return;

    if (!m_dragged) // First real move: undo point
    {
        Circuit::self()->saveState();
        m_dragged = true;
    }
    if (m_drag == DragP1)
        setP1(p1() + delta);
    else if (m_drag == DragP2)
        setP2(p2() + delta);
    else
    {
        int myindex = m_pConnector->lineList()->indexOf( this );
        // An end segment drags a new stub out of its pin, so the pin end
        // stays put and the segment moves parallel.
        if( myindex == 0 )
            m_pConnector->addConLine( p1().x(), p1().y(), p1().x(), p1().y(), myindex );

        else if( myindex == m_pConnector->lineList()->size()-1 )
            m_pConnector->addConLine( p2().x(), p2().y(), p2().x(), p2().y(), myindex + 1 );

        moveLine( delta );
    }
    updatePos();
    updateLines();
    for (ConnectorLine *line : *m_pConnector->lineList())
        line->update();
}

void ConnectorLine::mouseReleaseEvent( QGraphicsSceneMouseEvent* event )
{
    event->accept();
    bool dragged = m_dragged;
    m_drag       = NoDrag;
    m_dragged    = false;
    m_moveP1     = false;
    m_moveP2     = false;
    m_moving     = false;

    if (dragged) {
        Connector *con = m_pConnector;
        con->remNullLines(); // May remove this line: touch nothing after
        Circuit::self()->setChanged();
    }
}

void ConnectorLine::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (Circuit::self()->is_constarted())
        return;
    if (!conSelected()) {
        setCursor(Qt::PointingHandCursor);
        return;
    }

    DragMode mode = hitTest(event->scenePos());
    if (mode != DragSegment)
        setCursor(Qt::SizeAllCursor);
    else if (isDiagonal())
        setCursor(Qt::SizeAllCursor);
    else if (dy() == 0)
        setCursor(Qt::SizeVerCursor);
    else
        setCursor(Qt::SizeHorCursor);
}

QVariant ConnectorLine::itemChange(GraphicsItemChange change,
                                   const QVariant &value)
{
    if (change == ItemSelectedHasChanged && m_pConnector) // Handles on/off
        for (ConnectorLine *line : *m_pConnector->lineList())
            if (line != this)
                line->update();
    return QGraphicsObject::itemChange(change, value);
}

void ConnectorLine::contextMenuEvent( QGraphicsSceneContextMenuEvent* event )
{
    if( Circuit::self()->is_constarted() ) return;

    if( m_pConnector->endPin() )
    {
       event->accept();
       QMenu menu;

       QAction* removeAction = menu.addAction( tr("Remove") );
       connect(removeAction, SIGNAL(triggered()), this, SLOT(remove()));

       menu.exec(event->screenPos());

       //qDebug() << "ConnectorLine::contextMenuEvent\n" << m_pConnector->pointList();
    }
}

void ConnectorLine::setIsBus( bool bus )
{
    m_isBus = bus;
}

void ConnectorLine::setConnector( Connector* con ) { m_pConnector = con; }

QPoint ConnectorLine::p1() { return QPoint( m_p1X, m_p1Y ); }
QPoint ConnectorLine::p2() { return QPoint( m_p2X, m_p2Y ); }

int ConnectorLine::dx() { return (m_p2X - m_p1X);}
int ConnectorLine::dy() { return (m_p2Y - m_p1Y);}

bool ConnectorLine::isDiagonal()
{
    return ( abs(m_p2X - m_p1X)>0 && abs(m_p2Y - m_p1Y)>0 );
}

Connector* ConnectorLine::connector(){ return m_pConnector; }

QPainterPath ConnectorLine::shape() const
{
    QPainterPath line;
    line.moveTo(0, 0);
    line.lineTo(m_p2X - m_p1X, m_p2Y - m_p1Y);

    QPainterPathStroker stroker; // A comfortable band to click on
    stroker.setWidth(2 * kGrab);
    stroker.setCapStyle(Qt::RoundCap);
    QPainterPath path = stroker.createStroke(line);

    if (conSelected()) // Handles stick out of the band
    {
        double r = kVertex + 2;
        path.addRect(QRectF(-r, -r, 2 * r, 2 * r));
        path.addRect(
            QRectF(m_p2X - m_p1X - r, m_p2Y - m_p1Y - r, 2 * r, 2 * r));
    }
    return path;
}

void ConnectorLine::paint( QPainter* p, const QStyleOptionGraphicsItem* option, QWidget* widget )
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    bool wireSel = conSelected();
    QColor sel   = ThemeManager::self()->wireSelected();
    QColor color;
    if (wireSel)
        color = sel;
    else if (!m_isBus && Circuit::self()->animate()) {
        if( m_pConnector->getVolt() > 2.5 ) color = QColor( 200, 50, 50 );
        else
            color = QColor(50, 50, 200);
    } else if (m_isBus)
        color = Qt::darkGreen;
    else
        color = ThemeManager::self()->wireColor();

    if (wireSel) // Halo: tells a selected wire apart in every color mode
    {
        QColor halo = sel;
        halo.setAlpha(60);
        p->setPen(QPen(halo, 7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawLine(0, 0, dx(), dy());
    }
    QPen pen(color, m_isBus ? 3 : 2, Qt::SolidLine, Qt::RoundCap,
             Qt::RoundJoin);
    p->setPen( pen );
    p->drawLine(0, 0, dx(), dy());

    if (!wireSel || Circuit::self()->is_constarted())
        return;

    // Handles: every corner (square) and the center of the segment (dot).
    // Ends on pins are drawn as small rings: they don't move.
    p->setRenderHint(QPainter::Antialiasing);
    QList<ConnectorLine *> *lines = m_pConnector->lineList();
    int index                     = lines->indexOf(this);
    QColor fill =
        ThemeManager::self()->isDark() ? QColor(30, 30, 36) : Qt::white;

    auto vertex = [&](QPointF at, bool fixed) {
        p->setPen(QPen(sel, 1.2));
        p->setBrush(fill);
        if (fixed)
            p->drawEllipse(at, kVertex - 0.8, kVertex - 0.8);
        else
            p->drawRect(QRectF(at.x() - kVertex, at.y() - kVertex, 2 * kVertex,
                               2 * kVertex));
    };
    vertex(QPointF(0, 0), index == 0);
    vertex(QPointF(dx(), dy()), index == lines->size() - 1);

    if (abs(dx()) + abs(dy()) >= 4 * kVertex + 2 * kCenter) {
        p->setPen(QPen(fill, 1.2));
        p->setBrush(sel);
        p->drawEllipse(QPointF(dx() / 2.0, dy() / 2.0), kCenter, kCenter);
    }
}
