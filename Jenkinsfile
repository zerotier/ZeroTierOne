#!/usr/bin/env groovy
options {
    disableConcurrentBuilds()
}

def alpineStaticTask(distro, platform) {
    def myNode = {
        node ('linux-build') {
            checkout scm
            def runtime = docker.image("ztbuild/${distro}-${platform}:latest")
            runtime.inside {
                sh 'make -j8 ZT_STATIC=1 all'
                sh "mv zerotier-one zerotier-one-static-${platform}"
                archiveArtifacts artifacts: 'zerotier-one-*', fingerprint: true, onlyIfSuccessful: true
            }
        }
    }
    return myNode
}

def getTasks(axisDistro, axisPlatform, task) {
    def tasks = [:]
    for(int i=0; i< axisDistro.size(); i++) {
        def axisDistroValue = axisDistro[i]
        for(int j=0; j< axisPlatform.size(); j++) {
            def axisPlatformValue = axisPlatform[j]
            tasks["${axisDistroValue}/${axisPlatformValue}"] = task(axisDistroValue, axisPlatformValue)
        }
    }
    return tasks
}

pipeline {
    options {
        disableConcurrentBuilds()
    }
    
    agent none
    
    stages {
        stage ("Static Build") {
            steps {
                script {
                    def dist = ["alpine"]
                    def archs = ["aarch64", "amd64", "i386", "armhf", "armel", "ppc64le", "s390x"]
                    parallel getTasks(dist, archs, this.&alpineStaticTask)
                }
            }
        }
    }
}


mattermostSend color: "#00ff00", message: "${env.JOB_NAME} #${env.BUILD_NUMBER} Complete (<${env.BUILD_URL}|Show More...>)"
