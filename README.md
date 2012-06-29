# Fairmount

This is Fairmount, a tool which allows fair use of DVD content.

It is a program which mounts the contents of a DVD on a Mac OS X
machine as though it was a normal file system, allowing the user to
examine the contents.

It does not itself supply DVD decryption code, but instead requires
that VLC be installed and makes use of the libraries provided by it.

Fairmount was originally developed by Metakine, but it appears to have
vanished from their web site. However, it was released under the GPL,
so others may continue to develop it.

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

**Note**: The sources contain a `dsa_keys/dsa_pub.pem` file,
apparently needed by Sparkle. It was grabbed from an old Fairmount
binary. This will need to get replaced in order for Sparkle to be
aimed at a new source of updates, but for now it is there just so that
things will build at all.
