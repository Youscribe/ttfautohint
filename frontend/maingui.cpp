// maingui.cpp

// Copyright (C) 2012 by Werner Lemberg.
//
// This file is part of the ttfautohint library, and may only be used,
// modified, and distributed under the terms given in `COPYING'.  By
// continuing to use, modify, or distribute this file you indicate that you
// have read `COPYING' and understand and accept it fully.
//
// The file `COPYING' mentioned in the previous paragraph is distributed
// with the ttfautohint library.


#include <QtGui>

#include "maingui.h"

Main_GUI::Main_GUI(int range_min,
                   int range_max,
                   bool ignore,
                   bool pre,
                   int fallback)
: hinting_range_min(range_min),
  hinting_range_max(range_max),
  ignore_permissions(ignore),
  pre_hinting(pre),
  latin_fallback(fallback)
{
  createActions();
  createMenus();

  readSettings();

  setUnifiedTitleAndToolBarOnMac(true);
}


void Main_GUI::closeEvent(QCloseEvent *event)
{
  writeSettings();
  event->accept();
}


void Main_GUI::about()
{
  QMessageBox::about(this,
                     tr("About TTFautohint"),
                     tr("<b>TTFautohint</b> adds new auto-generated hints"
                        " to a TrueType font or TrueType collection."));
}


void Main_GUI::createActions()
{
  exitAct = new QAction(tr("E&xit"), this);
  exitAct->setShortcuts(QKeySequence::Quit);
  exitAct->setStatusTip(tr("Exit the application"));
  connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

  aboutAct = new QAction(tr("&About"), this);
  aboutAct->setStatusTip(tr("Show the application's About box"));
  connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

  aboutQtAct = new QAction(tr("About &Qt"), this);
  aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
  connect(aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}


void Main_GUI::createMenus()
{
  helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(aboutAct);
  helpMenu->addAction(aboutQtAct);
}


void Main_GUI::readSettings()
{
  QSettings settings;
//  QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
//  QSize size = settings.value("size", QSize(400, 400)).toSize();
//  resize(size);
//  move(pos);
}


void Main_GUI::writeSettings()
{
  QSettings settings;
//  settings.setValue("pos", pos());
//  settings.setValue("size", size());
}

// end of maingui.cpp
