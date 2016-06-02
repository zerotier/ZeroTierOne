Manual Pages and Other Documentation
=====

To build the Linux/Unix manual pages type "./build.sh." You'll need NodeJS since if it's not here already the build script will pull [marked-man](https://github.com/kapouer/marked-man), which is a MarkDown to ROFF/man converter. We pre-build and ship the compiled man pages, so running build.sh is not essential unless the source MarkDown files have changed.
