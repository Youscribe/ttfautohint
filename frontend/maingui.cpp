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
  create_layout();
  create_connections();
  create_actions();
  create_menus();

  read_settings();

  setUnifiedTitleAndToolBarOnMac(true);
}


void
Main_GUI::close_event(QCloseEvent* event)
{
  write_settings();
  event->accept();
}


void
Main_GUI::about()
{
  QMessageBox::about(this,
                     tr("About TTFautohint"),
                     tr("<b>TTFautohint</b> adds new auto-generated hints"
                        " to a TrueType font or TrueType collection."));
}


void
Main_GUI::browse_input()
{
  // XXX remember last directory
  QString file = QFileDialog::getOpenFileName(
                   this,
                   tr("Open Input File"),
                   QDir::toNativeSeparators(QDir::homePath()),
                   "");
  if (!file.isEmpty())
    input_line->setText(file);
}


void
Main_GUI::browse_output()
{
  // XXX remember last directory
  QString file = QFileDialog::getOpenFileName(
                   this,
                   tr("Open Output File"),
                   QDir::toNativeSeparators(QDir::homePath()),
                   "");
  if (!file.isEmpty())
    output_line->setText(file);
}


void
Main_GUI::check_run()
{
  if (input_line->text().isEmpty() || output_line->text().isEmpty())
    run_button->setEnabled(false);
  else
    run_button->setEnabled(true);
}


void
Main_GUI::run()
{
}


void
Main_GUI::create_layout()
{
  // file stuff
  QLabel* input_label = new QLabel(tr("Input File:"));
  input_line = new QLineEdit;
  input_button = new QPushButton(tr("Browse..."));

  QLabel* output_label = new QLabel(tr("Output File:"));
  output_line = new QLineEdit;
  output_button = new QPushButton(tr("Browse..."));

  QGridLayout* file_layout = new QGridLayout;
  file_layout->addWidget(input_label, 0, 0);
  file_layout->addWidget(input_line, 0, 1);
  file_layout->addWidget(input_button, 0, 2);
  file_layout->addWidget(output_label, 1, 0);
  file_layout->addWidget(output_line, 1, 1);
  file_layout->addWidget(output_button, 1, 2);

  // minmax controls
  QLabel* min_label = new QLabel(tr("Minimum:"));
  min_box = new QSpinBox;
  min_box->setRange(2, 10000);
  min_box->setValue(hinting_range_min);

  QLabel* max_label = new QLabel(tr("Maximum:"));
  max_box = new QSpinBox;
  max_box->setRange(2, 10000);
  max_box->setValue(hinting_range_max);

  QGridLayout* minmax_layout = new QGridLayout;
  minmax_layout->addWidget(min_label, 0, 0);
  minmax_layout->addWidget(min_box, 0, 1);
  minmax_layout->addWidget(max_label, 1, 0);
  minmax_layout->addWidget(max_box, 1, 1);

  // hinting and fallback controls
  QLabel* hinting_label = new QLabel(tr("Hinting Range") + " ");
  QLabel* fallback_label = new QLabel(tr("Fallback Script:"));
  fallback_box = new QComboBox;
  fallback_box->insertItem(0, tr("Latin"));

  QHBoxLayout* hint_fallback_layout = new QHBoxLayout;
  hint_fallback_layout->addWidget(hinting_label);
  hint_fallback_layout->addLayout(minmax_layout);
  hint_fallback_layout->addStretch(1);
  hint_fallback_layout->addWidget(fallback_label);
  hint_fallback_layout->addWidget(fallback_box);
  hint_fallback_layout->addStretch(2);

  // flags
  pre_box = new QCheckBox(tr("Pre-hinting"), this);
  ignore_box = new QCheckBox(tr("Ignore Permissions"), this);

  QHBoxLayout* flags_layout = new QHBoxLayout;
  flags_layout->addWidget(pre_box);
  flags_layout->addStretch(1);
  flags_layout->addWidget(ignore_box);
  flags_layout->addStretch(2);

  // running
  run_button = new QPushButton(tr("Run"));
  run_button->setEnabled(false);

  QHBoxLayout* running_layout = new QHBoxLayout;
  running_layout->addStretch(1);
  running_layout->addWidget(run_button);
  running_layout->addStretch(1);

  // the whole gui
  QVBoxLayout* gui_layout = new QVBoxLayout;
  gui_layout->addSpacing(10); // XXX urgh, pixels...
  gui_layout->addLayout(file_layout);
  gui_layout->addSpacing(20); // XXX urgh, pixels...
  gui_layout->addLayout(hint_fallback_layout);
  gui_layout->addSpacing(20); // XXX urgh, pixels...
  gui_layout->addLayout(flags_layout);
  gui_layout->addSpacing(20); // XXX urgh, pixels...
  gui_layout->addLayout(running_layout);
  gui_layout->addSpacing(10); // XXX urgh, pixels...

  // create dummy widget to register layout
  QWidget* main_widget = new QWidget;
  main_widget->setLayout(gui_layout);
  setCentralWidget(main_widget);
  setWindowTitle("TTFautohint");
}


void
Main_GUI::create_connections()
{
  connect(input_button, SIGNAL(clicked()), this,
          SLOT(browse_input()));
  connect(output_button, SIGNAL(clicked()), this,
          SLOT(browse_output()));

  connect(input_line, SIGNAL(textChanged(QString)), this,
          SLOT(check_run()));
  connect(output_line, SIGNAL(textChanged(QString)), this,
          SLOT(check_run()));

  connect(run_button, SIGNAL(clicked()), this,
          SLOT(run()));
}


void
Main_GUI::create_actions()
{
  exit_act = new QAction(tr("E&xit"), this);
  exit_act->setShortcuts(QKeySequence::Quit);
  connect(exit_act, SIGNAL(triggered()), this, SLOT(close()));

  about_act = new QAction(tr("&About"), this);
  connect(about_act, SIGNAL(triggered()), this, SLOT(about()));

  about_Qt_act = new QAction(tr("About &Qt"), this);
  connect(about_Qt_act, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
}


void
Main_GUI::create_menus()
{
  help_menu = menuBar()->addMenu(tr("&Help"));
  help_menu->addAction(about_act);
  help_menu->addAction(about_Qt_act);
}


void
Main_GUI::read_settings()
{
  QSettings settings;
//  QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
//  QSize size = settings.value("size", QSize(400, 400)).toSize();
//  resize(size);
//  move(pos);
}


void
Main_GUI::write_settings()
{
  QSettings settings;
//  settings.setValue("pos", pos());
//  settings.setValue("size", size());
}

// end of maingui.cpp
