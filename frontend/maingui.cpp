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


#include <string.h>
#include <stdio.h>
#include <errno.h>

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

  // XXX register translations somewhere and loop over them
  if (QLocale::system().name() == "en_US")
    locale = new QLocale;
  else
    locale = new QLocale(QLocale::C);
}


// overloading

void
Main_GUI::closeEvent(QCloseEvent* event)
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
Main_GUI::check_min()
{
  int min = min_box->value();
  int max = max_box->value();
  if (min > max)
    max_box->setValue(min);
}


void
Main_GUI::check_max()
{
  int min = min_box->value();
  int max = max_box->value();
  if (max < min)
    min_box->setValue(max);
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
Main_GUI::absolute_input()
{
  if (!input_line->text().isEmpty()
      && QDir::isRelativePath(input_line->text()))
  {
    QDir cur_path(QDir::currentPath() + "/" + input_line->text());
    input_line->setText(cur_path.absolutePath());
  }
}


void
Main_GUI::absolute_output()
{
  if (!output_line->text().isEmpty()
      && QDir::isRelativePath(output_line->text()))
  {
    QDir cur_path(QDir::currentPath() + "/" + output_line->text());
    output_line->setText(cur_path.absolutePath());
  }
}


int
Main_GUI::check_filenames(const QFile& in_file,
                          const QString& in_name,
                          const QFile& out_file,
                          const QString& out_name)
{
  if (!in_file.exists())
  {
    QMessageBox::warning(
      this,
      "TTFautohint",
      tr("The file ")
        + locale->quoteString(in_name)
        + tr(" cannot be found."),
      QMessageBox::Ok,
      QMessageBox::Ok);
    return 0;
  }

  if (in_name == out_name)
  {
    QMessageBox::warning(
      this,
      "TTFautohint",
      tr("Input and output file names must be different."),
      QMessageBox::Ok,
      QMessageBox::Ok);
    return 0;
  }

  if (out_file.exists())
  {
    int ret = QMessageBox::warning(
                this,
                "TTFautohint",
                tr("The file ")
                  + locale->quoteString(out_name)
                  + tr(" already exists.\n")
                  + tr("Overwrite?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
    if (ret == QMessageBox::No)
      return 0;
  }

  return 1;
}


int
Main_GUI::open_files(QFile& in_file,
                     const QString& in_name,
                     FILE** in,
                     QFile& out_file,
                     const QString& out_name,
                     FILE** out)
{
  if (!in_file.open(QIODevice::ReadOnly))
  {
    QMessageBox::warning(
      this,
      "TTFautohint",
      tr("The following error occurred while opening input font ")
        + locale->quoteString(in_name)
        + tr(":\n")
        + in_file.errorString(),
      QMessageBox::Ok,
      QMessageBox::Ok);
    return 0;
  }

  if (!out_file.open(QIODevice::WriteOnly))
  {
    QMessageBox::warning(
      this,
      "TTFautohint",
      tr("The following error occurred while opening output font ")
        + locale->quoteString(in_name)
        + tr(":\n")
        + out_file.errorString(),
      QMessageBox::Ok,
      QMessageBox::Ok);
    return 0;
  }

  const int buf_len = 1024;
  char buf[buf_len];

  *in = fdopen(in_file.handle(), "rb");
  if (!*in)
  {
    strerror_r(errno, buf, buf_len);
    QMessageBox::warning(
      this,
      "TTFautohint",
      tr("The following error occurred while opening input font ")
        + locale->quoteString(in_name)
        + tr(":\n")
        + QString::fromLocal8Bit(buf),
      QMessageBox::Ok,
      QMessageBox::Ok);
    return 0;
  }

  *out = fdopen(out_file.handle(), "wb");
  if (!*out)
  {
    strerror_r(errno, buf, buf_len);
    QMessageBox::warning(
      this,
      "TTFautohint",
      tr("The following error occurred while opening output font ")
        + locale->quoteString(out_name)
        + tr(":\n")
        + QString::fromLocal8Bit(buf),
      QMessageBox::Ok,
      QMessageBox::Ok);
    return 0;
  }

  return 1;
}


void
Main_GUI::run()
{
  QFile in_file(QDir::fromNativeSeparators(input_line->text()));
  QString in_name = QDir::toNativeSeparators(in_file.fileName());

  QFile out_file(QDir::fromNativeSeparators(output_line->text()));
  QString out_name = QDir::toNativeSeparators(out_file.fileName());

  if (!check_filenames(in_file, in_name, out_file, out_name))
    return;

  // we need C file descriptors
  FILE* in;
  FILE* out;
  if (!open_files(in_file, in_name, &in, out_file, out_name, &out))
    return;
}


void
Main_GUI::create_layout()
{
  // file stuff
  QCompleter* completer = new QCompleter(this);
  QFileSystemModel* model = new QFileSystemModel(completer);
  model->setRootPath(QDir::rootPath());
  completer->setModel(model);

  QLabel* input_label = new QLabel(tr("&Input File:"));
  input_line = new QLineEdit;
  input_button = new QPushButton(tr("Browse..."));
  input_label->setBuddy(input_line);
  input_line->setCompleter(completer);

  QLabel* output_label = new QLabel(tr("&Output File:"));
  output_line = new QLineEdit;
  output_button = new QPushButton(tr("Browse..."));
  output_label->setBuddy(output_line);
  output_line->setCompleter(completer);

  QGridLayout* file_layout = new QGridLayout;
  file_layout->addWidget(input_label, 0, 0);
  file_layout->addWidget(input_line, 0, 1);
  file_layout->addWidget(input_button, 0, 2);
  file_layout->addWidget(output_label, 1, 0);
  file_layout->addWidget(output_line, 1, 1);
  file_layout->addWidget(output_button, 1, 2);

  // minmax controls
  QLabel* min_label = new QLabel(tr("Mi&nimum:"));
  min_box = new QSpinBox;
  min_label->setBuddy(min_box);
  min_box->setRange(2, 10000);
  min_box->setValue(hinting_range_min);

  QLabel* max_label = new QLabel(tr("Ma&ximum:"));
  max_box = new QSpinBox;
  max_label->setBuddy(max_box);
  max_box->setRange(2, 10000);
  max_box->setValue(hinting_range_max);

  QGridLayout* minmax_layout = new QGridLayout;
  minmax_layout->addWidget(min_label, 0, 0);
  minmax_layout->addWidget(min_box, 0, 1);
  minmax_layout->addWidget(max_label, 1, 0);
  minmax_layout->addWidget(max_box, 1, 1);

  // hinting and fallback controls
  QLabel* hinting_label = new QLabel(tr("Hinting Range") + " ");
  QLabel* fallback_label = new QLabel(tr("F&allback Script:"));
  fallback_box = new QComboBox;
  fallback_label->setBuddy(fallback_box);
  fallback_box->insertItem(0, tr("Latin"));

  QHBoxLayout* hint_fallback_layout = new QHBoxLayout;
  hint_fallback_layout->addWidget(hinting_label);
  hint_fallback_layout->addLayout(minmax_layout);
  hint_fallback_layout->addStretch(1);
  hint_fallback_layout->addWidget(fallback_label);
  hint_fallback_layout->addWidget(fallback_box);
  hint_fallback_layout->addStretch(2);

  // flags
  pre_box = new QCheckBox(tr("Pr&e-hinting"), this);
  ignore_box = new QCheckBox(tr("I&gnore Permissions"), this);

  QHBoxLayout* flags_layout = new QHBoxLayout;
  flags_layout->addWidget(pre_box);
  flags_layout->addStretch(1);
  flags_layout->addWidget(ignore_box);
  flags_layout->addStretch(2);

  // running
  run_button = new QPushButton(tr("&Run"));
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

  connect(input_line, SIGNAL(editingFinished()), this,
          SLOT(absolute_input()));
  connect(output_line, SIGNAL(editingFinished()), this,
          SLOT(absolute_output()));

  connect(min_box, SIGNAL(valueChanged(int)), this,
          SLOT(check_min()));
  connect(max_box, SIGNAL(valueChanged(int)), this,
          SLOT(check_max()));

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
  file_menu = menuBar()->addMenu(tr("&File"));
  file_menu->addAction(exit_act);

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
