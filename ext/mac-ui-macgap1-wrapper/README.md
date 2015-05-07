#MacGap

The MacGap project provides HTML/JS/CSS developers an Xcode project for developing Native OSX Apps that run in OSX's WebView and take advantage of WebKit technologies.

The project exposes a JavaScript API for OS X integration, such as displaying native OS X 10.9 notifications. The MacGap project is extremely lightweight and nimble; a blank application is about 980KB.

### Features:
* tiny compiled app sizes
* Mac App Store compatible
* access to many Mac OS X-specific features

##Pre-requisites

MacGap works on OSX 10.6 and later.

Generate apps with the [macgap generator](http://github.com/maccman/macgap-rb), no compile necessary.

    gem install macgap

    macgap new myapp
    macgap build myapp

##API

MacGap exposes an object called `macgap` inside JavaScript. You can use it to alter the Dock icon and display Growl notifications, amongst other things. The API is documented in the WIKI on GitHub: https://github.com/maccman/macgap/wiki


##Attributes

MacGap was forked/ported from Phonegap-mac. It's under the same license (MIT).

##Custom Build

To build, make sure you have installed the latest Mac OSX Core Library. Download at [http://developer.apple.com/](http://developer.apple.com/).

Just clone the repository and build in Xcode. The file `public/index.html` is loaded on startup.
