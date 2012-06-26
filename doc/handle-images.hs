-- handle-images.hs
--
-- Copyright (C) 2012 by Werner Lemberg.
--
-- This file is part of the ttfautohint library, and may only be used,
-- modified, and distributed under the terms given in `COPYING'.  By
-- continuing to use, modify, or distribute this file you indicate
-- that you have read `COPYING' and understand and accept it fully.
--
-- The file `COPYING' mentioned in the previous paragraph is
-- distributed with the ttfautohint library.
--
--
-- This is a simple pandoc filter to support different image formats
-- for different backends.  It simply appends the suffix given as a
-- command line option to all pandoc images and checks whether the
-- image files actually exist.  For non-existent files the file name
-- doesn't get altered.
--
-- Command line examples:
--
--   % pandoc -t json foo \
--     | ./handle-images ".svg" \
--     | pandoc -f json -t html \
--     > foo.html
--
--   % pandoc -t json foo \
--     | ./handle-images ".pdf" \
--     | pandoc --latex-engine=pdflatex -f json -t latex \
--     > foo.tex


import Text.Pandoc
import System.Environment(getArgs)
import System.Directory(doesFileExist)


handleImage :: String
               -> Inline
               -> IO Inline
handleImage format
            (Image description (basename, title)) = do
  let
    filename = basename ++ format

  fileExists <- doesFileExist filename
  if fileExists
    then
      return $ Image description (filename, title)
    else
      return $ Image description (basename, title)
handleImage _ x = return x


main :: IO ()
main = do
  args <- getArgs
  toJsonFilter
    $ handleImage
    $ case args of
        [f] -> f
        _   -> "" -- default


-- end of handle-images.hs
