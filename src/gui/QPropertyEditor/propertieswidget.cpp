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

#include "propertieswidget.h"

#include "mainwindow.h"
#include "thememanager.h"

#include <QToolButton>

PropertiesWidget* PropertiesWidget::m_pSelf = 0l;

PropertiesWidget::PropertiesWidget( QWidget *parent ) 
                :QWidget( parent )
{
    m_pSelf   = this;
    
    createWidgets();
}

PropertiesWidget::~PropertiesWidget()
{
}

void PropertiesWidget::createWidgets()
{
    QGridLayout* widgetLayout = new QGridLayout( this );
    widgetLayout->setSpacing(0);
    widgetLayout->setContentsMargins(0, 0, 0, 0);
    widgetLayout->setObjectName("widgetLayout");

    m_properties = new  QPropertyEditorWidget( this );
    widgetLayout->addWidget(m_properties, 0, 0);

    // Collapsible help (collapsed by default so properties get the space)
    QToolButton *helpToggle = new QToolButton(this);
    helpToggle->setObjectName("propertiesHelpToggle");
    helpToggle->setText(tr("Help"));
    helpToggle->setIcon(ThemeManager::icon("help"));
    helpToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    helpToggle->setCheckable(true);
    helpToggle->setAutoRaise(true);
    widgetLayout->addWidget(helpToggle, 1, 0, Qt::AlignLeft);

    m_help = new QPlainTextEdit(this);
    m_help->setObjectName("propertiesHelp");
    m_help->setReadOnly( true );
    m_help->setMaximumHeight(110);
    m_help->setVisible(false);
    widgetLayout->addWidget(m_help, 2, 0);

    connect(helpToggle, &QToolButton::toggled, m_help, &QWidget::setVisible);
}

void PropertiesWidget::setHelpText( QString* text )
{
    if( !text ) return;
    m_help->clear();
    m_help->appendPlainText( *text );
}

#include  "moc_propertieswidget.cpp"
