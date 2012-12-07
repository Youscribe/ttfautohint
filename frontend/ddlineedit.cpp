// ddlineedit.cpp

// Copyright (C) 2012 by Werner Lemberg.
//
// This file is part of the ttfautohint library, and may only be used,
// modified, and distributed under the terms given in `COPYING'.  By
// continuing to use, modify, or distribute this file you indicate that you
// have read `COPYING' and understand and accept it fully.
//
// The file `COPYING' mentioned in the previous paragraph is distributed
// with the ttfautohint library.


// Derived class `Drag_Drop_Line_Edit' is QLineEdit
// which accepts drag and drop.

#include <config.h>

#include "ddlineedit.h"

Drag_Drop_Line_Edit::Drag_Drop_Line_Edit(QWidget* parent)
: QLineEdit(parent)
{
  // empty
}

 
// XXX: There are no standardized MIME types for TTFs and TTCs
//      which work everywhere.  So we rely on the extension.

void
Drag_Drop_Line_Edit::dragEnterEvent(QDragEnterEvent* event)
{
  QList<QUrl> url_list;
  QString file_name;

  if (event->mimeData()->hasUrls())
  {
    url_list = event->mimeData()->urls();

    // if just text was dropped, url_list is empty
    if (url_list.size())
    {
      file_name = url_list[0].toLocalFile();

      if (file_name.endsWith(".ttf")
          || file_name.endsWith(".TTF")
          || file_name.endsWith(".ttc")
          || file_name.endsWith(".TTC"))
        event->acceptProposedAction();
    }
  }
}


void
Drag_Drop_Line_Edit::dropEvent(QDropEvent* event)
{
  QList<QUrl> url_list;
  QString file_name;
  QFileInfo info;

  if (event->mimeData()->hasUrls())
  {
    url_list = event->mimeData()->urls();

    // if just text was dropped, url_list is empty
    if (url_list.size())
    {
      file_name = url_list[0].toLocalFile();

      // check whether `file_name' is valid
      info.setFile(file_name);
      if (info.isFile())
        setText(file_name);
    }
  }

  event->acceptProposedAction();
}

// end of ddlineedit.cpp
