#!/usr/bin/env groovy

parallel 'centos7': {
    node('centos7') {
        checkout scm

	    stage('Build Centos 7') {
            sh 'make -f make-linux.mk'
        }
    }
}, 'android-ndk': {
    node('android-ndk') {
        checkout scm
	
        stage('Build Android NDK') { 
            sh '/android/android-ndk-r13/ndk-build -C $WORKSPACE/java ZT1=$WORKSPACE'
        }
    }
}, 'macOS': {
    node('macOS') {
        checkout scm

        stage('Build macOS') {
            sh 'make -f make-mac.mk'
        }

        stage('Build macOS UI') {
            dir('$WORKSPACE/macui') {
                sh 'xcodebuild -scheme "ZeroTier One" -configuration Debug'
            }
        }
    }
}
