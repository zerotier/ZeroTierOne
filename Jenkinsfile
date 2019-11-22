#!/usr/bin/env groovy

node('master') {
    checkout scm
    
    def changelog = getChangeLog currentBuild

    mattermostSend "Building ${env.JOB_NAME} #${env.BUILD_NUMBER} \n Change Log: \n ${changelog}"
}

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
            mattermostSend color: '#ff0000', message: "${env.JOB_NAME} broken on Centos 7 (<${env.BUILD_URL}|Open>)"

            throw err
        }
    }
// }, 'android-ndk': {
//     node('android-ndk') {
//         try {
//             checkout scm
	
//             stage('Build Android NDK') { 
//                 sh "/android/android-ndk-r15b/ndk-build -C $WORKSPACE/java ZT1=${WORKSPACE}"
//             }
//         }
//         catch (err) {
//             currentBuild.result = "FAILURE"
//             mattermostSend color: '#ff0000', message: "${env.JOB_NAME} broken on Android NDK (<${env.BUILD_URL}|Open>)"

//             throw err
//         }
//     }
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
            mattermostSend color: '#ff0000', message: "${env.JOB_NAME} broken on macOS (<${env.BUILD_URL}|Open>)"

            throw err
        }
    }
}, 'windows': {
    node('windows') {
        try {
            checkout scm
            
            stage('Build Windows') {
                bat '''CALL "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community\\VC\\Auxiliary\\Build\\vcvars32.bat" x64
msbuild windows\\ZeroTierOne.sln
'''
            }
        }
        catch (err) {
            currentBuild.result = "FAILURE"
            mattermostSend color: '#ff0000', message: "${env.JOB_NAME} broken on Windows (<${env.BUILD_URL}|Open>)"

            throw err
        }
    }
}

mattermostSend color: "#00ff00", message: "${env.JOB_NAME} #${env.BUILD_NUMBER} Complete (<${env.BUILD_URL}|Show More...>)"
