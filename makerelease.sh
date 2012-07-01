#!/bin/bash

echo "*** Making Release for Fairmount... ***"

VERSION=$(fgrep APP_VERSION Fairmount.xcodeproj/project.pbxproj | \
    head -1 | sed -E 's/.*= ([0-9.]+);/\1/')
IMAGEDIR="IMAGE"

echo "*** Version is: $VERSION ***"

echo "*** Building Release ***"

# Clean old build directories
[ -d build ] && chmod -R u+w build
rm -rf build

# Build
xcodebuild -configuration Release

echo "*** Staging Image ***"

# Create IMAGE directories
[ -d $IMAGEDIR ] && chmod -R u+w $IMAGEDIR
rm -rf $IMAGEDIR
mkdir $IMAGEDIR

# Create IMAGE content
cp -R build/Release/Fairmount.app $IMAGEDIR/Fairmount.app
ln -s /Applications $IMAGEDIR/Applications
mkdir $IMAGEDIR/.bg
cp Packaging/dmg-background.tiff $IMAGEDIR/.bg/
chmod -R go-w $IMAGEDIR

echo "*** Building Fairmount-${VERSION}.dmg ***"

sh Packaging/makedmg.sh $VERSION $IMAGEDIR

echo "*** Completed ***"
