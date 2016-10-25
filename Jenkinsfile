#!/usr/bin/env groovy

parallel 'centos7': {
    node('centos7') {
    	stage('Checkout Centos 7') {
            checkout scm
        }

	    stage('Build Centos 7') {
            sh 'make -f make-linux.mk'
        }
    }
}, 'android-ndk': {
    node('android-ndk') {
	    stage('Checkout Android NDK') {
            checkout scm
        }
	
        stage "Build Android NDK" { 
            sh '/android/android-ndk-r13/ndk-build -C $WORKSPACE/java ZT1=$WORKSPACE'
        }
    }
}
