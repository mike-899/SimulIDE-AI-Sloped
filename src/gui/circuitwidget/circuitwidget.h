/***************************************************************************
 *   Copyright (C) 2010 by santiago González                               *
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

#ifndef CIRCUITWIDGET_H
#define CIRCUITWIDGET_H

#include <QtWidgets>

#include "circuitview.h"
#include "plotterwidget.h"
#include "terminalwidget.h"

class Component;

class MAINMODULE_EXPORT CircuitWidget : public QWidget
{
    Q_OBJECT

    public:
        CircuitWidget( QWidget *parent );
        ~CircuitWidget();

 static CircuitWidget* self() { return m_pSelf; }

        void clear();

        void createActions();

        void setRate( int rate );
        void powerCircOn();
        void powerCircOff();
        void powerCircDebug( bool run );

        QLabel *rateLabel()
        {
            return m_rateLabel;
        }
        CircuitView *view()
        {
            return &m_circView;
        }

        // Simulation state as shown in the UI: "Off", "On", "Debug", "Paused"
        QString simState();

        // Actions, placed in the main window top bar / toolbar / menu
        QAction *newCircAct;
        QAction *openCircAct;
        QAction *saveCircAct;
        QAction *saveCircAsAct;
        QAction *powerCircAct;
        QAction *pauseSimAct;
        QAction *infoAct;
        QAction *aboutAct;
        QAction *aboutQtAct;

        QAction *copyAct;
        QAction *pasteAct;
        QAction *deleteAct;
        QAction *undoAct;
        QAction *redoAct;
        QAction *rotateCWAct;
        QAction *rotateCCWAct;
        QAction *hFlipAct;
        QAction *vFlipAct;
        QAction *zoomInAct;
        QAction *zoomOutAct;
        QAction *zoomFitAct;
        QAction *zoomResetAct;
        QAction *importCircAct;
        QAction *saveImageAct;
        QAction *createSubcircAct;
        QAction *bomAct;

      public slots:
        bool newCircuit();
        void openCirc();
        void loadCirc( QString path );
        void saveCirc();
        bool saveCircAs();
        void powerCirc();
        void pauseSim();
        void openInfo();
        void about();

        // Edit operations on the current selection
        void copySelection();
        void pasteClipboard();
        void deleteSelection();
        void undo();
        void redo();
        void rotateSelectionCW();
        void rotateSelectionCCW();
        void hFlipSelection();
        void vFlipSelection();
        void importCirc();
        void createSubcircuit();
        void bom();

      signals:
        void dataAvailable( int uart, const QByteArray &data );
        void simStateChanged(const QString &state);
        void rateChanged(int rate); // Real speed %, < 0 on error

      private:
        static CircuitWidget *m_pSelf;

        QList<Component *> selectedComponents();

        QVBoxLayout    m_verticalLayout;
        QHBoxLayout    m_horizontLayout;
        CircuitView    m_circView;

        PlotterWidget     m_plotter;

        QLabel*  m_rateLabel;

        QString m_curCirc;
        QString m_lastCircDir;
};

#endif
