// maingui.h

// Copyright (C) 2012 by Werner Lemberg.
//
// This file is part of the ttfautohint library, and may only be used,
// modified, and distributed under the terms given in `COPYING'.  By
// continuing to use, modify, or distribute this file you indicate that you
// have read `COPYING' and understand and accept it fully.
//
// The file `COPYING' mentioned in the previous paragraph is distributed
// with the ttfautohint library.


#ifndef __MAINGUI_H__
#define __MAINGUI_H__

#include <QMainWindow>

class QAction;
class QMenu;
class QPlainTextEdit;

class Main_GUI
: public QMainWindow
{
  Q_OBJECT

public:
  Main_GUI(int, int, bool, bool, int);

protected:
  void close_event(QCloseEvent *event);

private slots:
  void about();

private:
  int hinting_range_min;
  int hinting_range_max;
  int ignore_permissions;
  int pre_hinting;
  int latin_fallback;

  void create_actions();
  void create_menus();
  void read_settings();
  void write_settings();

  QMenu *help_menu;
  QAction *exit_act;
  QAction *about_act;
  QAction *about_Qt_act;
};

#endif /* __MAINGUI_H__ */

// end of maingui.h
