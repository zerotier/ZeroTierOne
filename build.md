# ZeroTier Build and Project Information

## Build and Platform Notes

### Basic Build Instructions

To build on Mac and Linux just type `make`. On FreeBSD and OpenBSD `gmake` (GNU make) is required and can be installed from packages or ports. For Windows there is a Visual Studio solution in `windows/`.

### Platform Requirements

#### Mac
- Xcode command line tools for macOS 10.13 or newer are required.
- Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.

#### Linux
- The minimum compiler versions required are GCC/G++ 8.x or CLANG/CLANG++ 5.x.
- Linux makefiles automatically detect and prefer clang/clang++ if present as it produces smaller and slightly faster binaries in most cases. You can override by supplying CC and CXX variables on the make command line.
- Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.

#### Windows
- Visual Studio 2022 on Windows 10 or newer.
- Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.

#### FreeBSD
- GNU make is required. Type `gmake` to build.
- `binutils` is required. Type `pkg install binutils` to install.
- Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.

#### OpenBSD
- There is a limit of four network memberships on OpenBSD as there are only four tap devices (`/dev/tap0` through `/dev/tap3`).
- GNU make is required. Type `gmake` to build.
- Rust for x86_64 and ARM64 targets *if SSO is enabled in the build*.

### Testing

Typing `make selftest` will build a *zerotier-selftest* binary which unit tests various internals and reports on a few aspects of the build environment. It's a good idea to try this on novel platforms or architectures.

## Running ZeroTier

Running *zerotier-one* with `-h` option will show help.

On Linux and BSD, if you built from source, you can start the service with:

    sudo ./zerotier-one -d

On most distributions, macOS, and Windows, the installer will start the service and set it up to start on boot.

A home folder for your system will automatically be created.

The service is controlled via the JSON API, which by default is available at `127.0.0.1:9993`. It also listens on `0.0.0.0:9993` which is only usable if `allowManagementFrom` is properly configured in `local.conf`. We include a *zerotier-cli* command line utility to make API calls for standard things like joining and leaving networks. The *authtoken.secret* file in the home folder contains the secret token for accessing this API. See [service/README.md](service/README.md) for API documentation.

## Directory Structure

### Home Folder Locations

ZeroTier stores its configuration and state files in platform-specific locations:

* **Linux**: `/var/lib/zerotier-one`
* **FreeBSD** / **OpenBSD**: `/var/db/zerotier-one`
* **Mac**: `/Library/Application Support/ZeroTier/One`
* **Windows**: `\ProgramData\ZeroTier\One` (That's the default. The base 'shared app data' folder might be different if Windows is installed with a non-standard drive letter assignment or layout.)

### Project Structure

The repository is organized as follows:

- **node/** - Core ZeroTier networking code
- **osdep/** - Operating system dependent code  
- **service/** - Service implementation and API
- **controller/** - Network controller implementation
- **ext/** - External code included for build convenience (retains original licenses)
- **nonfree/** - Source available (non-free) portions
- **windows/** - Windows-specific Visual Studio solution files

### Build System

The project supports various build targets and platforms:
- Standard build: `make`
- Self-test build: `make selftest`
- Platform-specific requirements:
  - FreeBSD/OpenBSD: Use `gmake` (GNU make)
  - Windows: Visual Studio solution in `windows/`

### License Structure

- **node/**, **osdep/**, **service/**: Licensed under MPL-2.0 (see LICENSE-MPL.txt)
- **nonfree/**: Non-free "source available" code (see nonfree/LICENSE.md)
- **ext/**: External code with original licenses retained