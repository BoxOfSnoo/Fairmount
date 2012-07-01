#!/bin/sh

# Script to generate a "Fancy" .dmg
# Is passed a version number and a pre-existing "Image Directory"

VERSION=$1
SOURCE=$2

TITLE="Fairmount-${VERSION}"
APPNAME="Fairmount.app"

TMPDMG=temp.dmg

rm -f $TMPDMG

hdiutil create -srcfolder "${SOURCE}" -volname "${TITLE}" -fs HFS+ \
      -fsargs "-c c=64,a=16,e=16" -format UDRW $TMPDMG

hdiutil attach -readwrite -noverify -noautoopen "$TMPDMG"


sleep 2

osascript <<EOF
  tell application "Finder"
     tell disk "${TITLE}"
       open
       tell container window
         set current view to icon view
         set toolbar visible to false
         set statusbar visible to false
         -- bounds are stupidly x, y, x+h, y+w
         set bounds to {100, 100, 800, 500}
       end tell
       close
       set opts to the icon view options of container window
       tell opts
          set icon size to 128
          set arrangement to not arranged
       end tell
       set background picture of opts to file ".bg:dmg-background.tiff"
       set position of item "${APPNAME}" to {210, 190}
       set position of item "Applications" to {490, 190}
       update without registering applications
       tell container window
         open
         set the_window_id to id
       end tell
       update without registering applications
     end tell
     -- bounds are stupidly x, y, x+h, y+w
     set bounds of window id the_window_id to {100, 100, 800, 500}
     delay 5
     eject "${TITLE}"
  end tell
EOF

sync
sync

rm -rf "${TITLE}.dmg"
hdiutil convert $TMPDMG -format UDBZ -o "${TITLE}.dmg"

rm -f $TMPDMG

