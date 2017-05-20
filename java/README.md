ZeroTier One SDK - Android JNI Wrapper
=====


Building
-----

Reqires:

* JDK
* ANT
* Android SDK
* Android NDK
* XCode (if building on OS X)

Required Environment Variables:

* JAVA_HOME - Path to the Java SDK's home folder
* NDK\_BUILD\_LOC - Path do the ndk-build script in the Android NDK
* ANDROID\_PLATFORM - path to the directory android.jar lives (on Windows: C:\Users\<username>\AppData\Local\Android\sdk\platforms\android-21)
* ZT - Path to ZeroTier's source folder

Make sure to check your OS version in CMakeLists.txt if your are on OS X.
