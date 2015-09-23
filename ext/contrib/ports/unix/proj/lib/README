This directory contains an example of how to compile lwIP as a self
initialising shared library on Linux.

Some brief instructions:
 
* Compile the code:

 > make clean all

 This should produce liblwip4unixlib.so.  This is the shared library.

* Link an application against the shared library

 If you're using gcc you can do this by including -llwip4unixlib in
your link command. 

* Run your application
 
 Ensure that LD_LIBRARY_PATH includes the directory that contains
liblwip4unixlib.so (ie. this directory)



If you are unsure about shared libraries and libraries on linux in
general, you might find this HOWTO useful:

<http://www.tldp.org/HOWTO/Program-Library-HOWTO/>



Kieran Mansley, October 2002.