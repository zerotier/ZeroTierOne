ZeroTier Official Release Steps
======

This is mostly for ZeroTier internal use, but others who want to do builds might find it helpful.

Note: Many of these steps will require GPG and other signing keys that are kept in cold storage and must be mounted.

# Bumping the Version and Preparing Installers

The version must be incremented in all of the following files:

    /version.h
    /zerotier-one.spec
    /debian/changelog
    /ext/installfiles/mac/ZeroTier One.pkgproj
    /ext/installfiles/windows/ZeroTier One.aip
    /windows/WinUI/AboutView.xaml

The final .AIP file can only be edited on Windows with [Advanced Installer Enterprise](http://www.advancedinstaller.com/). In addition to incrementing the version be sure that a new product code is generated. (The "upgrade code" GUID on the other hand must never change.)

# Building for Supported Platforms

## Macintosh

Mac's easy. Just type:

    make official

You will need [Packages](http://s.sudre.free.fr/Software/Packages/about.html) and our release signing key in the keychain.

## Windows

First load the Visual Studio solution and rebuild the UI and ZeroTier One in both x64 and i386 `Release` mode. Then load [Advanced Installer Enterprise](http://www.advancedinstaller.com/), check that the version is correct, and build. The build will fail if any build artifacts are missing, and Windows must have our product singing key (from DigiCert) available to sign the resulting MSI file. The MSI must then be tested on at least a few different CLEAN Windows VMs to ensure that the installer is valid and properly signed.
