#!/bin/sh

# This script was used to clean up the tabs and other glitches in the
# sources.

# It converts tabs to spaces with four column tab stops, removes
# trailing whitepace, fixes a file with \r\n DOS-style line endings,
# and cuts the length of separator comments down below 80 columns.

# I did this with great reluctance, but there were too many places
# where spaces were mixed badly with tabs, and was generally too hard
# to look at the code in emacs or on github or in the shell.

for i in *.{cpp,h,m,mm,c,html,plist}
do
    dos2unix $i
    cat $i | expand -t 4 | sed -E 's/[      ]+$//' >$i.NEW
    sed 's|^//----------------------------------------------------------------------------------$|//-------------------------------------------------------------------------|' <$i.NEW >$i
    #mv $i.NEW $i
    rm $i.NEW
done

