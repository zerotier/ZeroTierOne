pipeline {
    options {
        disableConcurrentBuilds()
        preserveStashes(buildCount: 10)
        timestamps()
    }
    parameters {
        booleanParam(name: "BUILD_ALL", defaultValue: false, description: "Build all supported platform/architecture combos.  Defaults to x86/x64 only")
    }
    environment {
        PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
    }
    
    agent none
    
    stages {
        stage ("Build") {
            steps {
                script {
                    def tasks = [:]
                    tasks << buildStaticBinaries()
                    tasks << buildDebianNative()
                    tasks << buildCentosNative()
                    
                    parallel tasks
                }
            }
        }
        // stage ("Package Static") {
        //     steps {
        //         script {
        //             parallel packageStatic()
        //         }
        //     }
        // }
    }
}

def buildStaticBinaries() {
    def tasks = [:]
    def dist = ["alpine"]
    def archs = []
    if (params.BUILD_ALL == true) {
        archs = ["arm64", "amd64", "i386", "armhf", "armel", "ppc64le", "s390x"]
    } else {
        archs = ["amd64", "i386"]
    }

    tasks << getTasks(dist, archs, { distro, platform -> 
        def myNode = {
            node ('linux-build') {
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${platform}:latest")
                runtime.inside {
                    dir("build") {

                        def cmakeFlags = 'CMAKE_ARGS="-DBUILD_STATIC=1"'
                        if (platform == "i386") {
                            cmakeFlags = 'CMAKE_ARGS="-DBUILD_32BIT=1 -DBUILD_STATIC=1"'
                         }
                   
                        sh "${cmakeFlags} make"
                        dir("build") {
                            sh "mv zerotier zerotier-static-${platform}"
                            stash includes: 'zerotier-static-*', name: "static-${platform}"
                        }
                    }
                    cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
                }
            }
        }
        return myNode
    })
    
    return tasks
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

def packageStatic() {
    def tasks = [:]
    
    def centos6 = ["centos6"]
    def centos6Arch = ["i386", "amd64"]
    tasks << getTasks(centos6, centos6Arch, { distro, arch -> 
        def myNode = {
            node ('linux-build') {
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    dir("build") {
                        unstash "static-${arch}"
                        sh "mv zerotier-static-${arch} zerotier && chmod +x zerotier" 
                        sh "make redhat"
                        sh "mkdir -p ${distro}"
                        sh "cp -av `find ~/rpmbuild/ -type f -name \"*.rpm\"` ${distro}/"
                        archiveArtifacts artifacts: "${distro}/*.rpm", onlyIfSuccessful: true
                    }
                }
                cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
            }
        }
        return myNode
    })
    
    def centos7 = ["centos7"]
    def centos7Arch = ["i386"]
    tasks << getTasks(centos7, centos7Arch, { distro, arch -> 
        def myNode = {
            node ('linux-build') {
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    dir("build") {
                        unstash "static-${arch}"
                        sh "mv zerotier-static-${arch} zerotier && chmod +x zerotier" 
                        sh "make redhat"
                        sh "mkdir -p ${distro}"
                        sh "cp -av `find ~/rpmbuild/ -type f -name \"*.rpm\"` ${distro}/"
                        archiveArtifacts artifacts: "${distro}/*.rpm", onlyIfSuccessful: true
                    }
                }
                cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
            }
        }
        return myNode
    })
    
    if (params.BUILD_ALL == true) {
        def clefos7 = ["clefos"]
        def clefos7Arch = ["s390x"]
        tasks << getTasks(clefos7, clefos7Arch, { distro, arch -> 
            def myNode = {
                node ('linux-build') {
                    dir ("build") {
                        checkout scm
                    }
                    def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                    runtime.inside {
                        dir("build/") {
                            unstash "static-${arch}"
                            sh "mv zerotier-static-${arch} zerotier && chmod +x zerotier" 
                            sh "make redhat"
                            sh "mkdir -p ${distro}"
                            sh "cp -av `find ~/rpmbuild/ -type f -name \"*.rpm\"` ${distro}/"
                            archiveArtifacts artifacts: "${distro}/*.rpm", onlyIfSuccessful: true
                        }
                    }
                    cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
                }
            }
            return myNode
        })
    }

    def debianJessie = ["debian-jessie"]
    def debianJessieArchs = []
    if (params.BUILD_ALL == true) {
        debianJessieArch = ["armhf", "armel", "amd64", "i386"]
    } else {
        debianJessieArch = ["amd64", "i386"]
    }
    tasks << getTasks(debianJessie, debianJessieArch, { distro, arch -> 
        def myNode = {
            node ('linux-build') {
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    sh "ls -la ."
                    dir('build/') {
                        sh "ls -la ."
                        unstash "static-${arch}"
                        sh "pwd"
                        sh "mv zerotier-static-${arch} zerotier && chmod +x zerotier && file ./zerotier" 
                        sh "mv -f debian/rules.static debian/rules"
                        sh "make debian"
                    }
                    sh "mkdir -p ${distro}"
                    sh "mv *.deb ${distro}"
                    archiveArtifacts artifacts: "${distro}/*.deb", onlyIfSuccessful: true
                }
                cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
            }
        }
        return myNode
    })
    
    def ubuntuTrusty = ["ubuntu-trusty"]
    def ubuntuTrustyArch = []
    if (params.BUILD_ALL == true) {
        ubuntuTrustyArch = ["i386", "amd64", "armhf", "arm64", "ppc64le"]
    } else {
        ubuntuTrustyArch = ["i386", "amd64"]
    }
    tasks << getTasks(ubuntuTrusty, ubuntuTrustyArch, { distro, arch -> 
        def myNode = {
            node ('linux-build') {
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    sh "ls -la ."
                    dir('build/') {
                        sh "ls -la ."
                        unstash "static-${arch}"
                        sh "pwd"
                        sh "mv zerotier-static-${arch} zerotier && chmod +x zerotier && file ./zerotier" 
                        sh "mv -f debian/rules.static debian/rules"
                        sh "make debian"
                    }
                    sh "mkdir -p ${distro}"
                    sh "mv *.deb ${distro}"
                    archiveArtifacts artifacts: "${distro}/*.deb", onlyIfSuccessful: true
                }
                cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
            }
        }
        return myNode
    })
    
    def debianWheezy = ["debian-wheezy"]
    def debianWheezyArchs = []
    if (params.BUILD_ALL == true) {
        debianWheezyArchs = ["armhf", "armel", "amd64", "i386"]
    } else {
        debianWheezyArchs = ["amd64", "i386"]
    }
    tasks << getTasks(debianJessie, debianJessieArch, { distro, arch -> 
        def myNode = {
            node ('linux-build') {
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    dir('build/') {
                        unstash "static-${arch}"
                        sh "mv zerotier-static-${arch} zerotier && chmod +x zerotier && file ./zerotier" 
                        sh "mv -f debian/rules.wheezy.static debian/rules"
                        sh "mv -f debian/control.wheezy debian/control"
                        sh "make debian"
                    }
                    sh "mkdir -p ${distro}"
                    sh "mv *.deb ${distro}"
                    archiveArtifacts artifacts: "${distro}/*.deb", onlyIfSuccessful: true
                }
                cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
            }
        }
        return myNode
    })
    
    return tasks
}

def buildDebianNative() {
    def tasks = [:]
    def buster = ["debian-buster", "debian-stretch", "debian-bullseye", "debian-sid"]
    def busterArchs = []
    if (params.BUILD_ALL) {
        busterArchs = ["s390x", "ppc64le", "i386", "armhf", "armel", "arm64", "amd64"]
    } else {
        busterArchs = ["amd64", "i386"]
    }
    
    def build = { distro, arch -> 
        def myNode = {
            node ('linux-build') {
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    def cmakeFlags = ""
                    if (arch == "i386") {
                        cmakeFlags = 'CMAKE_ARGS="-DBUILD_32BIT=1"'
                    }
                   
                    sh 'whoami'
                    dir("build") {
                        sh "${cmakeFlags} make -j4"
                    }
                    // sh "mkdir -p ${distro}"
                    // sh "mv *.deb ${distro}"
                    // archiveArtifacts artifacts: "${distro}/*.deb", onlyIfSuccessful: true
                    cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
                }
            }
        }
        return myNode
    }
    
    tasks << getTasks(buster, busterArchs, build)
    
    // bash is broken when running under QEMU-s390x on Xenial
    def xenial = ["ubuntu-xenial"]
    def xenialArchs = []
    if (params.BUILD_ALL == true) {
        xenialArchs = ["i386", "amd64", "armhf", "arm64", "ppc64le"]
    } else {
        xenialArchs = ["i386", "amd64"]
    }
    tasks << getTasks(xenial, xenialArchs, build)
    
    def ubuntu = ["ubuntu-bionic", "ubuntu-eoan"]
    def ubuntuArchs = []
    if (params.BUILD_ALL == true) {
        ubuntuArchs = ["i386", "amd64", "armhf", "arm64", "ppc64le", "s390x"]
    } else {
        ubuntuArchs = ["i386", "amd64"]
    }
    tasks << getTasks(ubuntu, ubuntuArchs, build)
    
    def ubuntuFocal = ["ubuntu-focal"]
    def ubuntuFocalArchs = []
    if (params.BUILD_ALL == true) {
        ubuntuFocalArchs = ["amd64", "armhf", "arm64", "ppc64le", "s390x"]
    } else {
        ubuntuFocalArchs = ["amd64"]
    }
    tasks << getTasks(ubuntuFocal, ubuntuFocalArchs, build)

    def kali = ["kali-rolling"]
    def kaliArchs = ["amd64"]
    tasks << getTasks(kali, kaliArchs, build)
    
    return tasks
}

def buildCentosNative() {
    def tasks = [:]
    
    def build = { distro, arch -> 
        def myNode = {
            node ('linux-build') {
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    dir("build") {
                        if (distro == 'centos7') {
                            sh 'source scl_source enable devtoolset-8 llvm-toolset-7 && make'
                        } else {
                            sh 'make'
                        }
                        // sh 'make redhat'
                        // sh "mkdir -p ${distro}"
                        // sh "cp -av `find ~/rpmbuild/ -type f -name \"*.rpm\"` ${distro}/"
                        // archiveArtifacts artifacts: "${distro}/*.rpm", onlyIfSuccessful: true
                    }
                    
                    cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
                }
            }
        }
        return myNode
    }
    
    def centos8 = ["centos8"]
    def centos8Archs = []
    if (params.BUILD_ALL == true) {
        centos8Archs = ["amd64", "arm64", "ppc64le"]
    } else {
        centos8Archs = ["amd64"]
    }
    tasks << getTasks(centos8, centos8Archs, build)
    
    def centos7 = ["centos7"]
    def centos7Archs = ["amd64"]
    tasks << getTasks(centos7, centos7Archs, build)
    
    return tasks
}
