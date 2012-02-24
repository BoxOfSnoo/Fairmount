#!/bin/bash

YELLOW="[1;33m"
NORM="[0m"

cecho()
{
  echo "${YELLOW}$1${NORM}"
}

cecho "Making package for Fairmount...";

#create Package directories
rm -rf PACKAGE;
mkdir PACKAGE;
mkdir PACKAGE/Fairmount\ Source;

#check test for version
cecho "Make sure theses lines use the correct version:";
cat Info.plist | tr -d \\0 | grep -A 1 Version;

#build
chmod -R u+w build;
rm -rf build;
xcodebuild -configuration Release;
chmod -R u+w build;

#copy app
cp -R build/Release/Fairmount.app PACKAGE/Fairmount.app;

#copy source
cp *.c *.cpp *.rtf *.h *.m *.mm *.png *.icns *.html *.plist *.txt *.sh PACKAGE/Fairmount\ Source/.
cp -R MainMenu.nib *.xcodeproj PACKAGE/Fairmount\ Source/.

#remove extra files
cd PACKAGE;
find . -name "*svn*" -print0 | xargs -0 rm -rf;
find . -name ".DS_Store" -print0 | xargs -0 rm -rf;
find . -name "*.mode1" -print0 | xargs -0 rm -rf;
find . -name "*.pbxuser" -print0 | xargs -0 rm -rf;

cecho "Fairmount packaged.";
