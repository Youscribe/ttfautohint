// lineedit.h

// Copyright (C) 2012 by Werner Lemberg.
//
// This file is part of the ttfautohint library, and may only be used,
// modified, and distributed under the terms given in `COPYING'.  By
// continuing to use, modify, or distribute this file you indicate that you
// have read `COPYING' and understand and accept it fully.
//
// The file `COPYING' mentioned in the previous paragraph is distributed
// with the ttfautohint library.


#ifndef __LINEEDIT_H__
#define __LINEEDIT_H__

#include <config.h>

#include <QtGui>

class Line_Edit
: public QLineEdit
{
  Q_OBJECT

public:
  Line_Edit(QWidget* = 0);

  void dragEnterEvent(QDragEnterEvent*);
  void dropEvent(QDropEvent*);
};


#endif // __LINEEDIT_H__

// end of lineedit.h
