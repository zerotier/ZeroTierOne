#include "LinuxDropPrivileges.hpp"
#include <linux/capability.h>
#include <linux/securebits.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

namespace ZeroTier {

#ifndef PR_CAP_AMBIENT
// if we are on old libc, dropPrivileges is nop
void dropPrivileges(std::string homeDir) {}

#else

const char* TARGET_USER_NAME = "zerotier-one";

struct cap_header_struct {
    __u32 version;
    int pid;
};

struct cap_data_struct {
    __u32 effective;
    __u32 permitted;
    __u32 inheritable;
};

// libc doesn't export capset, it is instead located in libcap
// We ignore libcap and call it manually.

int capset(cap_header_struct* hdrp, cap_data_struct* datap) {
    return syscall(SYS_capset, hdrp, datap);
}

void notDropping(std::string homeDir) {
    struct stat buf;
    if (lstat(homeDir.c_str(), &buf) < 0) {
        if (buf.st_uid != 0 || buf.st_gid != 0) {
            fprintf(stderr, "ERROR: failed to drop privileges. Refusing to run as root, because %s was already used in nonprivileged mode.\n", homeDir.c_str());
            exit(1);
        }
    }
    fprintf(stderr, "WARNING: failed to drop privileges, running as root\n");
}

int setCapabilities(int flags) {
    cap_header_struct capheader = {_LINUX_CAPABILITY_VERSION_1, 0};
    cap_data_struct capdata;
    capdata.inheritable = capdata.permitted = capdata.effective = flags;
    return capset(&capheader, &capdata);
}

void createOwnedHomedir(std::string homeDir, struct passwd* targetUser) {
    struct stat buf;
    if (lstat(homeDir.c_str(), &buf) < 0) {
        if (errno == ENOENT) {
            mkdir(homeDir.c_str(), 0755);
        } else {
            perror("cannot access home directory");
            exit(1);
        }
    }

    if (buf.st_uid != 0 || buf.st_gid != 0) {
        // should be already owned by zerotier-one
        if (targetUser->pw_uid != buf.st_uid) {
            fprintf(stderr, "ERROR: %s not owned by zerotier-one or root\n", homeDir.c_str());
            exit(1);
        }
        return;
    }

    // Change homedir owner to zerotier-one user. This is safe, because this directory is writable only by root, so no one could have created malicious hardlink.
    long p = (long)fork();
    int exitcode = -1;
    if (p > 0) {
        waitpid(p, &exitcode, 0);
    } else if (p == 0) {
        std::string ownerString = std::to_string(targetUser->pw_uid) + ":" + std::to_string(targetUser->pw_gid);
        execlp("chown", "chown", "-R", ownerString.c_str(), "--", homeDir.c_str(), NULL);
        _exit(-1);
    }

    if (exitcode != 0) {
        fprintf(stderr, "failed to change owner of %s to %s\n", homeDir.c_str(), targetUser->pw_name);
        exit(1);
    }
}

void dropPrivileges(std::string homeDir) {
    // dropPrivileges switches to zerotier-one user while retaining CAP_NET_ADMIN
    // and CAP_NET_RAW capabilities.
    struct passwd* targetUser = getpwnam(TARGET_USER_NAME);
    if (targetUser == NULL) {
        // zerotier-one user not configured by package
        return;
    }

    if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_IS_SET, CAP_NET_RAW, 0, 0) < 0) {
        // Kernel has no support for ambient capabilities.
        notDropping(homeDir);
        return;
    }

    if (prctl(PR_SET_SECUREBITS, SECBIT_KEEP_CAPS | SECBIT_NOROOT) < 0) {
        notDropping(homeDir);
        return;
    }

    createOwnedHomedir(homeDir, targetUser);

    if (setCapabilities((1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW) | (1 << CAP_SETUID) | (1 << CAP_SETGID)) < 0) {
        fprintf(stderr, "ERROR: failed to set capabilities (not running as real root?)\n");
        exit(1);
    }

    int oldDumpable = prctl(PR_GET_DUMPABLE);

    if (prctl(PR_SET_DUMPABLE, 0) < 0) {
        // Disable ptracing. Otherwise there is a small window when previous
        // compromised ZeroTier process could ptrace us, when we still have CAP_SETUID.
        // (this is mitigated anyway on most distros by ptrace_scope=1)
        perror("prctl(PR_SET_DUMPABLE)");
        exit(1);
    }

    if (setgid(targetUser->pw_gid) < 0) {
        perror("setgid");
        exit(1);
    }
    if (setuid(targetUser->pw_uid) < 0) {
        perror("setuid");
        exit(1);
    }

    if (setCapabilities((1 << CAP_NET_ADMIN) | (1 << CAP_NET_RAW)) < 0) {
        perror("could not drop capabilities after setuid");
        exit(1);
    }

    if (prctl(PR_SET_DUMPABLE, oldDumpable) < 0) {
        perror("could not restore dumpable flag");
        exit(1);
    }

    if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_NET_ADMIN, 0, 0) < 0) {
        perror("could not raise ambient CAP_NET_ADMIN");
        exit(1);
    }

    if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_NET_RAW, 0, 0) < 0) {
        perror("could not raise ambient CAP_NET_RAW");
        exit(1);
    }
}

#endif
}
