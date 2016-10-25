#!/usr/bin/env groovy

slackSend "Building ${env.JOB_NAME} #${env.BUILD_NUMBER}"
parallel 'centos7': {
    node('centos7') {
        try {
            checkout scm

	        stage('Build Centos 7') {
                sh 'make -f make-linux.mk'
            }
        }
        catch (err) {
            currentBuild.result = "FAILURE"
            slackSend color: '#ff0000', message: "${env.JOB_NAME} broken on Centos 7 (<${env.BUILD_URL}|Open>)"

            throw err
        }
    }
}, 'android-ndk': {
    node('android-ndk') {
        try {
            checkout scm
	
            stage('Build Android NDK') { 
                sh "/android/android-ndk-r13/ndk-build -C $WORKSPACE/java ZT1=${WORKSPACE}"
            }
        }
        catch (err) {
            currentBuild.result = "FAILURE"
            slackSend color: '#ff0000', message: "${env.JOB_NAME} broken on Android NDK (<${env.BUILD_URL}|Open>)"

            throw err
        }
    }
}, 'macOS': {
    node('macOS') {
        try {
            checkout scm

            stage('Build macOS') {
                sh 'make -f make-mac.mk'
            }

            stage('Build macOS UI') {
                sh 'cd macui && xcodebuild -target "ZeroTier One" -configuration Debug'
            }
        }
        catch (err) {
            currentBuild.result = "FAILURE"
            slackSend color: '#ff0000', message: "${env.JOB_NAME} broken on macOS (<${env.BUILD_URL}|Open>)"

            throw err
        }
    }
}

slackSend "${env.JOB_NAME} #${env.BUILD_NUMBER} Complete (<${env.BUILD_URL}|Show More...>)"