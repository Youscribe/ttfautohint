// ttlineedit.cpp

// Copyright (C) 2012 by Werner Lemberg.
//
// This file is part of the ttfautohint library, and may only be used,
// modified, and distributed under the terms given in `COPYING'.  By
// continuing to use, modify, or distribute this file you indicate that you
// have read `COPYING' and understand and accept it fully.
//
// The file `COPYING' mentioned in the previous paragraph is distributed
// with the ttfautohint library.


// Derived class `Tooltip_Line_Edit' is QLineEdit which displays a tooltip
// if the data in the field is wider than the field width.

#include <config.h>

#include "ttlineedit.h"

Tooltip_Line_Edit::Tooltip_Line_Edit(QWidget* parent)
: QLineEdit(parent)
{
  connect(this, SIGNAL(textChanged(QString)),
          this, SLOT(change_tooltip(QString)));
}


void Tooltip_Line_Edit::change_tooltip(QString tip)
{
  QFont font = this->font();
  QFontMetrics metrics(font);

  // get the (sum of the) left and right borders; this is a bit tricky
  // since Qt doesn't have methods to directly access those margin values
  int line_minwidth = minimumSizeHint().width();
  int char_maxwidth = metrics.maxWidth();
  int border = line_minwidth - char_maxwidth;

  int linewidth = this->width();
  int textwidth = metrics.width(tip);

  if (textwidth > linewidth - border)
    this->setToolTip(tip);
  else
    this->setToolTip("");
}

// end of ttlineedit.cpp
