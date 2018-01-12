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
    /ext/installfiles/windows/chocolatey/zerotier-one.nuspec
    /ext/installfiles/windows/ZeroTier One.aip
    /windows/WinUI/AboutView.xaml

The final .AIP file can only be edited on Windows with [Advanced Installer Enterprise](http://www.advancedinstaller.com/). In addition to incrementing the version be sure that a new product code is generated. (The "upgrade code" GUID on the other hand must never change.)

# Building for Supported Platforms

## Macintosh

Mac's easy. Just type:

    make official

You will need [Packages](http://s.sudre.free.fr/Software/Packages/about.html) and our release signing key in the keychain.

## Linux

See `LinuxBuild` environment on `linux-build` VM and use: `chroots/mount-build.sh`, `chroots/build.sh`, and the scripts in `build/` to make APT and RPM repositories.

## Windows

First load the Visual Studio solution and rebuild the UI and ZeroTier One in both x64 and i386 `Release` mode. Then load [Advanced Installer Enterprise](http://www.advancedinstaller.com/), check that the version is correct, and build. The build will fail if any build artifacts are missing, and Windows must have our product singing key (from DigiCert) available to sign the resulting MSI file. The MSI must then be tested on at least a few different CLEAN Windows VMs to ensure that the installer is valid and properly signed.

*After the MSI is published to download.zerotier.com in the proper RELEASE/#.#.#/dist subfolder for its version* the Chocolatey package must be rebuilt and published. Open a command prompt, change to `ext/installfiles/windows/chocolatey`, and type `choco pack`. Then use `choco push` to push it to Chocolatey (API key required).

    choco pack
    choco push zerotier-one.#.#.#.nupkg -s https://chocolatey.org/

Note that this does not cover rebuilding the drivers or their containing MSI projects, as this is typically not necessary and they are shipped in binary form in the repository for convenience.

## iOS, Android

... no docs here yet since this is done entirely out of band with regular installs.
