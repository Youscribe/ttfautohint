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
  create_actions();
  create_menus();

  read_settings();

  setUnifiedTitleAndToolBarOnMac(true);
}


void Main_GUI::close_event(QCloseEvent* event)
{
  write_settings();
  event->accept();
}


void Main_GUI::about()
{
  QMessageBox::about(this,
                     tr("About TTFautohint"),
                     tr("<b>TTFautohint</b> adds new auto-generated hints"
                        " to a TrueType font or TrueType collection."));
}


void Main_GUI::create_actions()
{
  exit_act = new QAction(tr("E&xit"), this);
  exit_act->setShortcuts(QKeySequence::Quit);
  connect(exit_act, SIGNAL(triggered()), this, SLOT(close()));

  about_act = new QAction(tr("&About"), this);
  connect(about_act, SIGNAL(triggered()), this, SLOT(about()));

  about_Qt_act = new QAction(tr("About &Qt"), this);
  connect(about_Qt_act, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}


void Main_GUI::create_menus()
{
  help_menu = menuBar()->addMenu(tr("&Help"));
  help_menu->addAction(about_act);
  help_menu->addAction(about_Qt_act);
}


void Main_GUI::read_settings()
{
  QSettings settings;
//  QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
//  QSize size = settings.value("size", QSize(400, 400)).toSize();
//  resize(size);
//  move(pos);
}


void Main_GUI::write_settings()
{
  QSettings settings;
//  settings.setValue("pos", pos());
//  settings.setValue("size", size());
}

// end of maingui.cpp
