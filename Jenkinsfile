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
                    tasks << buildMacOS()
                    tasks << buildWindows()
                    // tasks << buildFreeBSD()

                    parallel tasks
                }
            }
        }
        stage ("Package Static") {
            steps {
                script {
                    parallel packageStatic()
                }
            }
        }
    }
}

def buildMacOS() {
    def tasks = [:]
    tasks << getTasks(['mac'],['amd64'], {unused1, unused2 ->
        def myNode = {
            env.PATH = env.PATH + ":/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
            node ('mac') {
                dir("build") {
                    checkout scm
                    sh 'make'
                }
                cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
            }
        }
        return myNode
    })
    return tasks
}

def buildWindows() {
    def tasks = [:]
    tasks << getTasks(['windows'], ['amd64', 'i386'], { unused1, platform ->
        def myNode = {
            node ('windows') {
                env.SHELL = 'C:/Windows/System32/cmd.exe'
                dir ("build") {
                    checkout scm
                    
                    dir ("build") {
                        withEnv(["PATH=C:\\TDM-GCC-64\\bin;C:\\WINDOWS;C:\\Windows\\system32;C:\\CMake\\bin;C:\\Go\\bin"]) {
                            def cmakeFlags = ""
                            if (platform == "i386") {
                                cmakeFlags = '-DBUILD_32BIT=1'
                            }
                            bat """
                            cmake -G"MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ${cmakeFlags} ..
                            mingw32-make -j8
                            """
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

def buildFreeBSD() {
    def tasks = [:]
    tasks << getTasks(['freebsd12'], ['amd64'], { unused1, unused2 ->
        def myNode = {
            node ('freebsd12') {
                dir('build') {
                    checkout scm
                    sh 'make setup'
                    dir('build') {
                        sh 'make -j4'
                    }
                }
                cleanWs deleteDirs: true, disableDeferredWipeout: true, notFailBuild: true
            }
        }
        return myNode
    })
    return tasks
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
                env.PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
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
        
    def centos7 = ["centos7"]
    def centos7Arch = ["i386"]
    tasks << getTasks(centos7, centos7Arch, { distro, arch -> 
        def myNode = {
            node ('linux-build') {
                env.PATH = env.PATH + ":/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    dir("build") {
                        unstash "static-${arch}"
                        sh "mkdir -p build"
                        sh "mv zerotier-static-${arch} build/zerotier && chmod +x build/zerotier" 
                        sh 'CMAKE_ARGS="-DBUILD_32BIT=1 -DPACKAGE_STATIC=1 -DZT_PACKAGE_FORMAT=RPM" make setup'
                        dir("build") {
                            sh 'make package'
                        }
                        sh "mkdir -p ${distro}"
                        sh "cp -av build/*.rpm ${distro}/"
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
                    env.PATH = env.PATH + ":/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
                    dir ("build") {
                        checkout scm
                    }
                    def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                    runtime.inside {
                        dir("build/") {
                            unstash "static-${arch}"
                            sh "mkdir -p build"
                            sh "mv zerotier-static-${arch} build/zerotier && chmod +x build/zerotier" 
                            sh 'CMAKE_ARGS="-DPACKAGE_STATIC=1 -DZT_PACKAGE_FORMAT=RPM" make setup'
                            dir("build") {
                                sh 'make package'
                            }
                            sh "mkdir -p ${distro}"
                            sh "cp -av build/*.rpm ${distro}/"
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
                env.PATH = env.PATH + ":/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    dir('build/') {
                        unstash "static-${arch}"
                        sh "mkdir -p build"
                        sh "mv zerotier-static-${arch} build/zerotier && chmod +x build/zerotier" 
                        sh 'CMAKE_ARGS="-DPACKAGE_STATIC=1 -DZT_PACKAGE_FORMAT=DEB" make setup'
                        dir("build") {
                            sh 'make package'
                        }
                        sh "mkdir -p ${distro}"
                        sh "cp -av build/*.deb ${distro}/"
                        archiveArtifacts artifacts: "${distro}/*.deb", onlyIfSuccessful: true
                    }
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
                env.PATH = env.PATH + ":/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    dir('build/') {
                        unstash "static-${arch}"
                        sh "mkdir -p build"
                        sh "mv zerotier-static-${arch} build/zerotier && chmod +x build/zerotier" 
                        sh 'CMAKE_ARGS="-DPACKAGE_STATIC=1 -DZT_PACKAGE_FORMAT=DEB" make setup'
                        dir("build") {
                            sh 'make package'
                        }
                        sh "mkdir -p ${distro}"
                        sh "cp -av build/*.deb ${distro}/"
                        archiveArtifacts artifacts: "${distro}/*.deb", onlyIfSuccessful: true
                    }
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
                env.PATH = env.PATH + ":/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    dir('build/') {
                        unstash "static-${arch}"
                        sh "mkdir -p build"
                        sh "mv zerotier-static-${arch} build/zerotier && chmod +x build/zerotier" 
                        sh 'CMAKE_ARGS="-DPACKAGE_STATIC=1 -DZT_PACKAGE_FORMAT=DEB" make setup'
                        dir("build") {
                            sh 'make package'
                        }
                        sh "mkdir -p ${distro}"
                        sh "cp -av build/*.deb ${distro}/"
                        archiveArtifacts artifacts: "${distro}/*.deb", onlyIfSuccessful: true
                    }
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
    def buster = ["debian-buster", /*"debian-stretch",*/ "debian-bullseye", "debian-sid"]
    def busterArchs = []
    if (params.BUILD_ALL) {
        busterArchs = ["s390x", "ppc64le", "i386", "armhf", "armel", "arm64", "amd64"]
    } else {
        busterArchs = ["amd64", "i386"]
    }
    
    def build = { distro, arch -> 
        def myNode = {
            node ('linux-build') {
                env.PATH = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    def cmakeFlags = 'CMAKE_ARGS="-DZT_PACKAGE_FORMAT=DEB"'
                    if (arch == "i386") {
                        cmakeFlags = 'CMAKE_ARGS="-DBUILD_32BIT=1 -DZT_PACKAGE_FORMAT=DEB"'
                    }
                   
                    sh 'whoami'
                    dir("build") {
                        sh "${cmakeFlags} make setup"
                        dir("build") {
                            sh "make package -j4"
                        }
                    }
                    sh "mkdir -p ${distro}"
                    sh "mv build/build/*.deb ${distro}"
                    archiveArtifacts artifacts: "${distro}/*.deb", onlyIfSuccessful: true
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
    //tasks << getTasks(xenial, xenialArchs, build)
    
    def ubuntu = ["ubuntu-bionic", "ubuntu-eoan"]
    def ubuntuArchs = []
    if (params.BUILD_ALL == true) {
        ubuntuArchs = ["i386", "amd64", "armhf", "arm64", "ppc64le", "s390x"]
    } else {
        ubuntuArchs = ["i386" /*, "amd64"*/]
    }
    tasks << getTasks(ubuntu, ubuntuArchs, build)
    
    def ubuntuFocal = ["ubuntu-focal"]
    def ubuntuFocalArchs = []
    if (params.BUILD_ALL == true) {
        ubuntuFocalArchs = ["amd64", "arm64", "ppc64le", "s390x"]
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
                env.PATH = env.PATH + ":/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/go/bin:/home/jenkins-build/go/bin"
                dir ("build") {
                    checkout scm
                }
                def runtime = docker.image("ztbuild/${distro}-${arch}:latest")
                runtime.inside {
                    dir("build") {
                        if (distro == 'centos7' && arch == 'amd64') {
                            sh 'source scl_source enable devtoolset-8 llvm-toolset-7 && CMAKE_ARGS="-DZT_PACKAGE_FORMAT=RPM" make setup'
                        } else {
                            sh 'CMAKE_ARGS="-DZT_PACKAGE_FORMAT=RPM" make setup'
                        }
                        dir ("build") {
                            if (distro == 'centos7' && arch == 'amd64') {
                                sh 'source scl_source enable devtoolset-8 llvm-toolset-7 && make package -j4'
                            } else {
                                sh 'make package -j4'
                            }
                        }
                    }
                    sh "mkdir -p ${distro}"
                    sh "cp -av build/build/*.rpm ${distro}/"
                    archiveArtifacts artifacts: "${distro}/*.rpm", onlyIfSuccessful: true
                    
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
