ZeroTier HTML5 UI
======

This is the new (as of 1.0.3) ZeroTier One UI. It's implemented in HTML5 and React.

If you make changes to the .jsx files, type 'make'. You will need NodeJS, react-tools, and minify installed and available in your path.

For this to work, these files must be installed in the 'ui' subfolder of the ZeroTier home path. For development it's nice to symlink this to the 'ui' folder in your working directory. If the 'ui' subfolder is not present, the UI static files will not be served by the embedded web server.

Packaging for Mac and Windows is accomplished by way of the wrappers in ext/. For Mac this is done with a modified version of MacGap. Windows uses a custom project that embeds a web view.
