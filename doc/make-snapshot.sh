#! /bin/sh
#
# make-snapshot.sh <application> <filename>
#
# Make a snapshot from an application's start window and save it to a file.
# This needs X11 and ImageMagick's `import' tool.
#
# This script is very simple.  Some possible problems:
#
# o In case the program doesn't create a visible X11 window, you have to
#   abort the script with ^C.
#
# o It fails for intelligent applications like `firefox' in case they are
#   already run, since firefox replaces the process with a new window of the
#   already running instance.
#
# o It loops forever for programs like `k3b' which spawns itself, thus
#   having a different process ID for the visible window.


# This script uses ideas from
# http://blog.chewearn.com/2010/01/18/find-window-id-of-a-process-id-in-bash-script/.

if [ $# -ne 2 ]; then
  echo "Usage: $0 application imagename"
  exit 1
fi


find_WID()
{
  # Get all windows with the name $APP (ignoring case).
  xwininfo -root -tree 2>/dev/null \
  | grep -i $1 \
  | while read DATA; do
      # Extract Window ID.
      WID=`echo $DATA | awk '{print $1}'`

      # Check whether the window's PID is matching the application's PID.
      if [ `xprop -id $WID _NET_WM_PID | awk '{print $3}'` -eq $PID ]; then
        # Check whether window is displayed actually.
        if [ "`xwininfo -id $WID | grep 'IsViewable'`" != '' ]; then
          echo $WID
          return
        fi
      fi
    done
}


# Start program in background and get its process ID.
$1 &
PID=$!

sleep 1

# Get application name.
APP=`ps --no-header -o comm -p $PID`
if [ "$APP" == "" ]; then
  echo "Couldn't start application \`$1'"
  exit 1
fi

# Loop until program has displayed a window
# so that we can actually get the Windows ID.
while [ "$WID" == "" ]; do
  WID=`find_WID $APP`
  sleep 1
done

# Make snapshot.
import -silent -window $WID $2

kill $PID

# eof
