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

#include <config.h>

#include <QtGui>
#include "lineedit.h"

#include <stdio.h>
#include <ttfautohint.h>

class QAction;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFile;
class QLabel;
class QLocale;
class QMenu;
class QPushButton;
class QSpinBox;

class Line_Edit;

class Main_GUI
: public QMainWindow
{
  Q_OBJECT

public:
  Main_GUI(int, int, int,
           bool, bool, bool,
           int, bool, bool, bool,
           bool, bool, int, bool);

protected:
  void closeEvent(QCloseEvent*);

private slots:
  void about();
  void browse_input();
  void browse_output();
  void check_min();
  void check_max();
  void check_limit();
  void check_no_limit();
  void check_no_increase();
  void absolute_input();
  void absolute_output();
  void check_run();
  void run();

private:
  int hinting_range_min;
  int hinting_range_max;
  int hinting_limit;
  int gray_strong_stem_width;
  int gdi_cleartype_strong_stem_width;
  int dw_cleartype_strong_stem_width;
  int increase_x_height;
  int ignore_restrictions;
  int windows_compatibility;
  int pre_hinting;
  int hint_with_components;
  int no_info;
  int latin_fallback;
  int symbol;

  void create_layout();

  void create_connections();
  void create_actions();
  void create_menus();
  void create_status_bar();
  void read_settings();
  void write_settings();

  int check_filenames(const QString&, const QString&);
  int open_files(const QString&, FILE**, const QString&, FILE**);
  int handle_error(TA_Error, const unsigned char*, QString);

  QMenu* file_menu;
  QMenu* help_menu;

  Line_Edit* input_line;
  QPushButton* input_button;

  Line_Edit* output_line;
  QPushButton* output_button;

  QSpinBox* min_box;
  QSpinBox* max_box;

  QCheckBox* gray_box;
  QCheckBox* gdi_box;
  QCheckBox* dw_box;

  QComboBox* fallback_box;

  QLabel* limit_label;
  QSpinBox* limit_box;
  QCheckBox* no_limit_box;

  QLabel* increase_label;
  QSpinBox* increase_box;
  QCheckBox* no_increase_box;

  QCheckBox* wincomp_box;
  QCheckBox* pre_box;
  QCheckBox* hint_box;
  QCheckBox* symbol_box;
  QCheckBox* info_box;

  QPushButton* run_button;

  QAction* exit_act;
  QAction* about_act;
  QAction* about_Qt_act;

  QLocale* locale;
};

#endif // __MAINGUI_H__

// end of maingui.h
