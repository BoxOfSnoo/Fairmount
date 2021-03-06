# Fairmount

This is Fairmount, a tool which allows fair use of DVD content.

It is a program which mounts the contents of a DVD on a Mac OS X
machine as though it was a normal file system, allowing the user to
examine the contents.

It does not itself supply DVD reading code, but instead requires
that a DVD library from videolan.org be installed separately.

Fairmount was originally developed by Metakine, but it appears to have
vanished from their web site. However, it was released under the GPL,
so others may continue to develop it.

## Getting Binaries

For now, a zip file of the .app for OS X 10.6 and later (Intel only)
is located at: <https://github.com/pmetzger/Fairmount/downloads>

You must also install the package found at:
<http://download.videolan.org/libdvdcss/last/macosx/libdvdcss.pkg> --
this will place the file libdvdcss.2.dylib into /usr/local/lib/ where the
program will find it.

If you don't install the library, the program will prompt you to do so.

\[In the unlikely event you want to use with a different version
of the library, the program first searches in
`~/Library/Application\ Support/Fairmount/libdvdcss.dylib` and then in
`~/Library/Application\ Support/Fairmount/libdvdcss.2.dylib` \]

The version number has been bumped to 1.1.2, but other than a few
fixes (including finding the needed library in a new place), there's
no new functionality.

Automatic updates are intentionally broken as there is not yet
anywhere to update from.

## How To Build

Fairmount uses the "Sparkle" framework for automatic updates. Said
updates are currently broken, of course, but you can't build without
it.

Therefore:

1. Get the Sparkle framework from: <http://sparkle.andymatuschak.org/>
   (Sources to Sparkle, if you want to find them, are at:
   <https://github.com/andymatuschak/Sparkle> but the binaries are all
   you need to build.)
2. Place the Sparkle.framework file in the Sparkle/ directory in the
   checked out sources

Then tell XCode to build the project.

Alternatively, from the top level of the sources, you can run

    sh makerelease.sh

and the script will build everything, including a release .dmg file.

**Note**: The sources contain a `dsa_keys/dsa_pub.pem` file,
apparently needed by Sparkle. It was grabbed from an old Fairmount
binary. This will need to get replaced in order for Sparkle to be
aimed at a new source of updates, but for now it is there just so that
things will build at all.

## Source files overview

This overview is taken from the original Help.html file included with
the project resources.

<ul>
    <li><i>Types.h</i>:<br/>
        Specifies some data types used throughout the code.
    </li>

    <li><i>Decryption.[h,m]</i>:<br/>
        Provides the dvdcss API. <b>dvdcss is not included with Fairmount</b>,
        it is rather loaded from the user's disk and must be separately installed.
    </li>

    <li><i>Fairmount.[h,mm]</i>:<br/>
        This contains the main classes for Fairmount, the Fairmount class
        which registers itself with the Disk Arbitration framework to
        receive notifications of disks appearing and disappearing, and the
        DVDServer class which do the heavywork and contains the web
        server object.
    </li>

    <li><i>Overlay.[h,m]</i>:<br/>
        Contains the code neccessary to show the progress's overlay
        Apple's style.
    </li>

    <li><i>socketWrap.[h,c]</i>:<br/>
        Provides a simple API to hide socket usage details.
    </li>

    <li><i>FileSize.[h,c]</i>:<br/>
        Function that provides the byte size of block devices.
    </li>

    <li><i>ListFiles.[h,cpp]</i>:<br/>
        Provides the file's list (size, start and end) of the ISO9660
        filesystem of the dvd. Used by CSSFileAccess to ensure
        decryption is only applied on VOB files.
    </li>

    <li><i>HTTPServer.[h,cpp]</i>:<br/>
        A quite minimal web server able to serve large files.
    </li>

    <li><i>IFileAccess.h</i>:<br/>
        Provides an interface to all data access done by the web server.
        By using this interfaces, it is easy to test different methods of
        accessing data.
    </li>

    <li><i>FileAccess.cpp</i>:<br/>
        A dumb file access module used for debugging purpose.
    </li>

    <li><i>CSSFileAccess.cpp</i>:<br/>
        This module does the decryption on the VOB files, and slices requests
        on VOB boundaries.
    </li>

    <li><i>CachedFileAccess.cpp</i>:<br/ >
        This module works on top of another to provide threaded
        read-ahead cache.
    </li>
</ul>
