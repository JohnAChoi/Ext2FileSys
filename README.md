# Ext2FileSys
An Ext2 file system shell. 

This is a basic Ext2 file system shell.

Linux is required to build and run this program. It should work on most versions of Ubuntu. 
To compile, simply use the makefile. "make debug" will turn on the debug trace, in case you're interested.

The included mydisk file is an Ext2 formatted virtual disk. It contains a few files in a few directories, 
which can be used to test the functionality of the shell. The file sizes are large enough to show that the 
shell is capable of reading up to doubly-indirect blocks.

Known bugs:

Symbolic links are not printed correctly within the shell. Symbolic links themselves are created correctly, though.
