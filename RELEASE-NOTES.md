ZeroTier Release Notes
======

## 2026-05-20 -- Version 1.16.2
 
  * Fixed line endings for the zttap300.inf for the Windows ARM x64 install.
  * Internal updates to the central controller (CV1) including schema changes and a multi-architecture Docker build pipeline.
  * Increased `ZT_MAX_NETWORK_SPECIALISTS` from 256 to 512 to accommodate users with large networks that exceeded the previous limit. The network config dictionary capacity was raised correspondingly.
  * Resolved intermittent multi-minute delays when leaving one network and joining another under Windows.
  * Compiler warning cleanup across the codebase, covering GCC 14, Clang 18, and Clang 21.

## 2025-12-22 -- Version 1.16.1
 
  * Metrics collection is now disabled by default. It can be enabled via the `enableMetrics` setting in `local.conf`.
  * Fix for an issue where metrics were not being recorded while running in daemon mode.
  * Fix debug dumpfile being written to the root directory.
  * Minor bug fixes in Mac and BSD tun/tap code.

## 2025-08-21 -- Version 1.16.0

  * License Changes
    * The core (`node/` and `include/`) and the service (`service/` and `osdep/`) are now under the Mozilla Public License (MPL).
    * The network controller (`controller/`) is now under a commercial source-available license.
    * Use `make ZT_NONFREE=1` to build non-MPL components.
    * Building with `ZT_NONFREE=1` changes the license of the resulting executable to a proprietary commercial license vs. MPL.
  * Default binary builds no longer contain the controller.
  * Network-Specific Relays (preview / beta)
    * It is now possible to designate one or more nodes as network-specific relays to be used in preference to roots for relayed traffic between members of a network. These nodes need not necessarily be members of the network.
    * "Moons" are now considered even more extra *deprecated* and should not be used in new deployments.
    * We will announce support for network-specific relays once we have worked with some users to test and performed more internal validation.
  * HELLO packet encryption is now available by enabling the `encryptedHelloEnabled` settting in `local.conf`.
    * HELLO packets contain no data, only public keys and very basic meta-data like protocol version information.
    * Most users won't care about this, but if you require this for e.g. compliance reasons you can enable. This adds a small amount of CPU and bandwidth overhead to the HELLO sign-on process.
  * Small Fixes
    * Code has been reformatted using `clang-format` with a `.clang-format` definition in the repo. Typing `make clang-format` executes this against all main core and service C++ files.
    * Bridges are no longer counted toward multicast limits.
    * A flow designation issue in bridged traffic under multipath scenarios has been fixed.
    * Library version updates for OIDC and other features.
    * Antiquated and unused software update code removed for precautionary reasons.
    * Compiler warnings removed through removing use of deprecated functions on some platforms.
    * Other minor bug fixes.

  * Merged PRs:
    * PR #2495: Hosted controller changes for cv1 os/arch reporting
    * PR #2493: Add a warning about missing DNS functionality on Linux
    * PR #2491: Fix: standardize bond link selection method JSON field naming
    * PR #2489: Fix link select field in bond CLI
    * PR #2487: Windows installer fixes
    * PR #2486: Add 'apt update' to validation action
    * PR #2482: Add OpenTelemetry support in Central Controllers
    * PR #2481: Rust warning removal, clippy allows, update library versions
    * PR #2477: Don't count bridges towards multicast limit. Send to all bridges
    * PR #2475: clang-format
    * PR #2474: Hello encryption
    * PR #2452: CV2 db schema support
    * PR #2451: Bump crossbeam-channel from 0.5.13 to 0.5.15 in /rustybits
    * PR #2450: Bump tokio from 1.42.0 to 1.43.1 in /rustybits
    * PR #2449: Bump openssl from 0.10.70 to 0.10.72 in /rustybits
    * PR #2445: Allow setting local.conf content from Docker environment variable
    * PR #2444: Temporal sdk fix
    * PR #2443: Fix AuthInfo Provider not being set
    * PR #2442: Bump ring from 0.17.8 to 0.17.13 in /rustybits
    * PR #2441: Run CI on pull requests
    * PR #2438: Add custom control plane for third-party device vendors
    * PR #2432: Update upload-artifact action to use v4
    * PR #2430: Bump openssl from 0.10.68 to 0.10.70 in /rustybits
    * PR #2427: Fix active backup link selection
    * PR #2417: Update rust dependencies
    * PR #2409: Bump rustls from 0.23.15 to 0.23.18 in /rustybits
    * PR #2405: Build fix for OpenBSD - See ticket #2397

----

# 1.14

## 2024-10-23 -- Version 1.14.2

  * Fix for missing entitlement on macOS Sequoia.
  * Fix for a problem correctly parsing local.conf to enable low bandwidth mode.
  * Increment versions of some dependent libraries.
  * Other fixes.

## 2024-09-12 -- Version 1.14.1

  * Multithreaded packet I/O support! Currently this is just for Linux and must
    be enabled in local.conf. It will likely make the largest difference on small
    multi-core devices where CPU is a bottleneck and high throughput is desired.
    It may be enabled by default in the future but we want it to be thoroughly
    tested. It's a little harder than it seems at first glance due to the need
    to keep packets in sequence and balance load.
  * Several multipath bug fixes.
  * Updated the versions on a number of libraries related to OIDC support and HTTP.
  * MacOS .app now shows the correct version in its Info.plist manifest.
  * Sanitize MAC addresses in JSON format rules parser.
  * Some basic information about the platform (OS, CPU architecture) is now reported
    to network controllers when networks are joined so it can be displayed to
    network admins and in the future used in policy checking and inventory operations.

## 2024-05-02 -- Version 1.14.0

  * Linux I/O performance improvements under heavy load
  * Improvements to multipath
  * Fix for port rebinding "coma" bug after periods offline (some laptop users)
  * Fixed a rules engine quirk/ambiguity (GitHub Issue #2200)
  * Controller API enhancements: node names and other node meta-data
  * Other bug fixes

----

# 1.12 and Older

## 2023-09-12 -- Version 1.12.2

  * More improvements to macOS full tunnel mode.
  * Faster recovery after changes to physical network settings.

## 2023-08-25 -- Version 1.12.1

  * Minor release to fix a port binding issue in Linux.
  * Update Debian dependencies.
  * No changes for other platforms.

## 2023-08-23 -- Version 1.12.0

  * Experimental Windows ARM64 support
  * Fix numerous sleep/wake issues on macOS and other platforms
  * Faster recovery after changes to physical network settings
  * Prometheus compatible metrics support!
  * Fix full tunnel mode on recent macOS versions
  * Numerous macOS DNS fixes
  * 10-30% speed improvement on Linux

## 2023-03-23 -- Version 1.10.6

  * Prevent binding temporary ipv6 addresses on macos (#1910)
  * Prevent path-learning loops (#1914)
  * Prevent infinite loop of UAC prompts in tray app

## 2023-03-10 -- Version 1.10.5

 * Fix for high CPU usage bug on Windows

## 2023-03-07 -- Version 1.10.4

 * SECURITY FIX (Windows): this version fixes a file permission problem on
   Windows that could allow non-privileged users on a Windows system to read
   privileged files in the ZeroTier service's working directory. This could
   allow an unprivileged local Windows user to administrate the local ZeroTier
   instance without appropriate local permissions. This issue is not remotely
   exploitable unless a remote user can read arbitrary local files, and does
   not impact other operating systems.
 * Fix a bug in the handling of multiple IP address assignments to virtual
   interfaces on macOS.

## 2023-02-15 -- Version 1.10.3

 * Fix for duplicate paths in client. Could cause connectivity issues. Affects all platforms.
 * Fix for Ethernet Tap MTU setting, would not properly apply on Linux.
 * Fix default route bugs (macOS.)
 * Enable Ping automatically for ZeroTier Adapters (Windows.)
 * SSO updates and minor bugfixes.
 * Add low-bandwidth mode.
 * Add forceTcpRelay mode (optionally enabled.)
 * Fix bug that prevented setting of custom TCP relay address.
 * Build script improvements and bug fixes.

## 2022-11-01 -- Version 1.10.2

 * Fix another SSO "stuck client" issue in zeroidc.
 * Expose root-reported external IP/port information via the local JSON API for better diagnostics.
 * Multipath: CLI output improvement for inspecting bonds
 * Multipath: balance-aware mode
 * Multipath: Custom policies
 * Multipath: Link quality measurement improvements

Note that releases are coming few and far between because most of our dev effort is going into version 2.

## 2022-06-27 -- Version 1.10.1

 * Fix an issue that could cause SSO clients to get "stuck" on stale auth URLs.
 * A few other SSO related bug fixes.

## 2022-06-07 -- Version 1.10.0

 * Fix formatting problem in `zerotier-cli` when using SSO networks.
 * Fix a few other minor bugs in SSO signin to prepare for general availability.
 * Remove requirement for webview in desktop UI and instead just make everything available via the tray pulldown/menu. Use [libui-ng](https://github.com/libui-ng/libui-ng) for minor prompt dialogs. Saves space and eliminates installation headaches on Windows.
 * Fix SSO "spam" bug in desktop UI.
 * Use system default browser for SSO login so all your plugins, MFA devices, password managers, etc. will work as you have them configured.
 * Minor fix for bonding/multipath.

## 2022-05-10 -- Version 1.8.10

 * Fixed a bug preventing SSO sign-on on Windows.

## 2022-04-25 -- Version 1.8.9

 * Fixed a long-standing and strange bug that was causing sporadic "phantom" packet authentication failures. Not a security problem but could be behind sporadic reports of link failures under some conditions.
 * Fixed a memory leak in SSO/OIDC support.
 * Fixed SSO/OIDC display error on CLI.
 * Fixed a bug causing nodes to sometimes fail to push certs to each other (primarily affects SSO/OIDC use cases).
 * Fixed a deadlock bug on leaving SSO/OIDC managed networks.
 * Added some new Linux distributions to the build subsystem.

## 2022-04-11 -- Version 1.8.8

 * Fix a local privilege escalation bug in the Windows installer.
 * Dependency fix for some Ubuntu versions.
 * No changes for other platforms. Windows upgrade recommended, everyone else optional.

## 2022-03-30 -- Version 1.8.7

 * Fix for dependency installations in Windows MSI package.
 * Fix for desktop UI setup when run by a non-super-user.
 * Bug fix in local OIDC / SSO support for auth0 and other providers.
 * Other minor fixes for e.g. old Linux distributions.

## 2022-03-04 -- Version 1.8.6

 * Fixed an issue that could cause the UI to be non-responsive if not joined to any networks.
 * Fix dependency issues in Debian and RedHat packages for some distributions (Fedora, Mint).
 * Bumped the peer cache serialization version to prevent "coma" issues on upgrade due to changes in path logic behaving badly with old values.

## 2022-02-22 -- Version 1.8.5

 * Plumbing under the hood for endpoint device SSO support.
 * Fix in LinuxEthernetTap to tap device support on very old (2.6) Linux kernels.
 * Fix an issue that could cause self-hosted roots ("moons") to fail to assist peers in making direct links. (GitHub issue #1512)
 * Merge a series of changes by Joseph Henry (of ZeroTier) that should fix some edge cases where ZeroTier would "forget" valid paths.
 * Minor multipath improvements for automatic path negotiation.

## 2021-11-30 -- Version 1.8.4

 * Fixed an ugly font problem on some older macOS versions.
 * Fixed a bug that could cause the desktop tray app control panel to stop opening after a while on Windows.
 * Fixed a possible double "release" in macOS tray app code that crashed on older macOS versions.
 * Fixed installation on 32-bit Windows 10.
 * Fixed a build flags issue that could cause ZeroTier to crash on older ARM32 CPUs.

## 2021-11-15 -- Version 1.8.3

 * Remove problematic spinlock, which was only used on x86_64 anyway. Just use pthread always.
 * Fix fd leak on MacOS that caused non-responsiveness after some time.
 * Fix Debian install scripts to set /usr/sbin/nologin as shell on service user.
 * Fix regression that could prevent managed routes from being deleted.
 * DesktopUI: Remove NSDate:now() call, now works on MacOS 10.13 or newer!

## 2021-11-08 -- Version 1.8.2

 * Fix multicast on linux.
 * Fix a bug that could cause the tap adapter to have the wrong MAC on Linux.
 * Update build flags to possibly support MacOS older than 10.14, but more work needs to be done. It may not work yet.
 * Fix path variable setting on Windows.

## 2021-10-28 -- Version 1.8.1

 * Fix numerous UI issues from 1.8.0 (never fully released).
 * Remove support for REALLY ancient 1.1.6 or earlier network controllers.
 * MacOS IPv6 no longer binds to temporary addresses as these can cause interruptions if they expire.
 * Added additional hardening against address impersonation on networks (also in 1.6.6).
 * Fix an issue that could cause clobbering of MacOS IP route settings on restart.

 * NOTE: Windows 7 is no longer supported! Windows 7 users will have to use version 1.6.5 or earlier.

## 2021-09-15 -- Version 1.8.0 (preview release only)

 * A *completely* rewritten desktop UI for Mac and Windows!
 * Implement a workaround for one potential source of a "coma" bug, which can occur if buggy NATs/routers stop allowing the service to communicate on a given port. ZeroTier now reassigns a new secondary port if it's offline for a while unless a secondary port is manually specified in local.conf. Working around crummy buggy routers is an ongoing effort.
 * Fix for MacOS MTU capping issue on feth devices
 * Fix for mistakenly using v6 source addresses for v4 routes on some platforms
 * Stop binding to temporary IPv6 addresses
 * Set MAC address before bringing up Linux TAP link
 * Check if DNS servers need to be applied on macOS
 * Upgrade json.hpp dependency to version 3.10.2

## 2021-09-21 -- Version 1.6.6

 * Backport COM hash check mitigation against network member impersonation.

## 2021-04-13 -- Version 1.6.5

 * Fix a bug in potential network path filtering that could in some circumstances lead to "software laser" effects.
 * Fix a printf overflow in zerotier-cli (not exploitable or a security risk)
 * Windows now looks up the name of ZeroTier devices instead of relying on them having "ZeroTier" in them.

## 2021-02-15 -- Version 1.6.4

 * The groundhog saw his shadow, which meant that the "connection coma" bug still wasn't gone. We think we found it this time.

## 2021-02-02 -- Version 1.6.3

 * Likely fix for GitHub issue #1334, an issue that could cause ZeroTier to
   go into a "coma" on some networks.
 * Also groundhog day

## 2020-11-30 -- Version 1.6.2

 * Fix an ARM hardware AES crypto issue (not an exploitable vulnerability).
 * Fix a Linux network leave hang due to a mutex deadlock.

## 2020-11-24 -- Version 1.6.1

This release fixes some minor bugs and other issues in 1.6.0.

 * Fixed a bug that caused IP addresses in the 203.0.0.0/8 block to be miscategorized as not being in global scope.
 * Changed Linux builds to (hopefully) fix LXC and SELinux issues.
 * Fixed unaligned memory access that caused crash on FreeBSD systems on the ARM architecture.
 * Merged CLI options for controlling bonded devices into the beta multipath code.
 * Updated Windows driver with Microsoft cross-signing to fix issues on some Windows systems.

## 2020-11-19 -- Version 1.6.0

Version 1.6.0 is a major release that incorporates back-ported features from the 2.0 branch, which is still under development. It also fixes a number of issues.

New features and improvements (including those listed under 1.5.0):

 * **Apple Silicon** (MacOS ARM64) native support via universal binary. ZeroTier now requires the very latest Xcode to build.
 * **Linux performance improvements** for up to 25% faster tun/tap I/O performance on multi-core systems.
 * **Multipath support** with modes modeled after the Linux kernel's bonding driver. This includes active-passive and active-active modes with fast failover and load balancing. See section 2.1.5 of the manual.
 * **DNS configuration** push from network controllers to end nodes, with locally configurable permissions for whether or not push is allowed.
 * **AES-GMAC-SIV** encryption mode, which is both somewhat more secure and significantly faster than the old Salsa20/12-Poly1305 mode on hardware that supports AES acceleration. This includes virtually all X86-64 chips and most ARM64. This mode is based on AES-SIV and has been audited by Trail of Bits to ensure that it is equivalent security-wise.

Bug fixes:

 * **Managed route assignment fixes** to eliminate missing routes on Linux and what we believe to be the source of sporadic high CPU usage on MacOS.
 * **Hang on shutdown** issues should be fixed.
 * **Sporadic multicast outages** should be fixed.

Known remaining issues:

 * AES hardware acceleration is not yet supported on 32-bit ARM, PowerPC (32 or 64), or MIPS (32 or 64) systems. Currently supported are X86-64 and ARM64/AARCH64 with crypto extensions.

# 2020-10-05 -- Version 1.5.0 (actually 1.6.0-beta1)

Version 1.6.0 (1.5.0 is a beta!) is a significant release that incorporates a number of back-ported fixes and features from the ZeroTier 2.0 tree.

Major new features are:

 * **Multipath support** with modes modeled after the Linux kernel's bonding driver. This includes active-passive and active-active modes with fast failover and load balancing. See section 2.1.5 of the manual.
 * **DNS configuration** push from network controllers to end nodes, with locally configurable permissions for whether or not push is allowed.
 * **AES-GMAC-SIV** encryption mode, which is both somewhat more secure and significantly faster than the old Salsa20/12-Poly1305 mode on hardware that supports AES acceleration. This includes virtually all X86-64 chips and most ARM64. This mode is based on AES-SIV and has been audited by Trail of Bits to ensure that it is equivalent security-wise.

Known issues that are not yet fixed in this beta:

 * Some Mac users have reported periods of 100% CPU in kernel_task and connection instability after leaving networks that have been joined for a period of time, or needing to kill ZeroTier and restart it to finish leaving a network. This doesn't appear to affect all users and we haven't diagnosed the root cause yet.
 * The service sometimes hangs on shutdown requiring a kill -9. This also does not affect all systems or users.
 * AES hardware acceleration is not yet supported on 32-bit ARM, PowerPC (32 or 64), or MIPS (32 or 64) systems. Currently supported are X86-64 and ARM64/AARCH64 with crypto extensions.
 * Some users have reported multicast/broadcast outages on networks lasting up to 30 seconds. Still investigating.

We're trying to fix all these issues before the 1.6.0 release. Stay tuned.

## 2019-08-30 -- Version 1.4.6

 * Update default root list to latest
 * ARM32 platform build and flag fixes
 * Add a clarification line to LICENSE.txt
 * Fix license message in CLI
 * Windows service now looks for service command line arguments
 * Fixed a bug that could cause excessive queued multicasts

## 2019-08-23 -- Version 1.4.4

 * Change license from GPL3 to BSL 1.1, see LICENSE.txt
 * Fix an issue with the "ipauth" rule and auto-generated unforgeable IPv6 addresses
 * Fix socket/bind errors setting IPs and routes on Linux

## 2019-08-12 -- Version 1.4.2

 * Fix high CPU use bug on some platforms
 * Fix issues with PostgreSQL controller DB (only affects Central)
 * Restore backward compatibility with MacOS versions prior to 10.13

## 2019-07-29 -- Version 1.4.0

### Major Changes

 * Mac version no longer requires a kernel extension, instead making use of the [feth interfaces](https://apple.stackexchange.com/questions/337715/fake-ethernet-interfaces-feth-if-fake-anyone-ever-seen-this).
 * Added support for concurrent multipath (multiple paths at once) with traffic weighting by link quality and faster recovery from lost links.
 * Added under-the-hood support for QoS (not yet exposed) that will eventually be configurable via our rules engine.

### Minor Changes and Bug Fixes

 * Experimental controller DB driver for [LF](https://github.com/zerotier/lf) to store network controller data (LFDB.cpp / LFDB.hpp).
 * Modified credential push and direct path push timings and algorithms to somewhat reduce "chattiness" of the protocol when idle. More radical background overhead reductions will have to wait for the 2.x line.
 * Removed our beta/half-baked integration of Central with the Windows UI. We're going to do a whole new UI of some kind in the future at least for Windows and Mac.
 * Fixed stack overflow issues on Linux versions using musl libc.
 * Fixed some alignment problems reported on ARM and ARM64, but some reports we could not reproduce so please report any issues with exact chip, OS/distro, and ZeroTier version in use.
 * Fixed numerous other small issues and bugs such as ARM alignment issues causing crashes on some devices.
 * Windows now sets the adapter name such that it is consistent in both the Windows UI and command line utilities.

## 2018-07-27 -- Version 1.2.12

 * Fixed a bug that caused exits to take a long time on Mac due to huge numbers of redundant attempts to delete managed routes.
 * Fixed a socket limit problem on Windows that caused the ZeroTier service to run out of sockets, causing the UI and CLI to be unable to access the API.
 * Fixed a threading bug in the ZeroTier Core, albeit one that never manifested on the regular ZeroTier One service/client.
 * Fixed a bug that could cause the service to crash if an authorized local client accessed an invalid URL via the control API. (Not exploitable since you needed admin access anyway.)

## 2018-05-08 -- Version 1.2.10

 * Fix bug loading `moons.d/` files for federated root operation.
 * Fix compile problem with ZT_DEBUG on some versions of `clang`
 * Fix slow network startup bug related to loading of `networks.d/` cache files

## 2018-04-27 -- Version 1.2.8

 * Linux version once again builds with PIE (position independent executable) flags
 * Fixed bug in zerotier-idtool file sign and verify
 * Fixed minor OSX app typo
 * Merged alpha NetBSD support (mostly untested, so YMMV)
 * Merged several minor typo and one-liner bug fixes

## 2018-04-17 -- Version 1.2.6

 * Features and Core Improvements
    * Path selection has been overhauled to improve path stability, simplify code, and prepare for multi-path and trunking in the next major release.
    * This version introduces remote tracing for remote diagnostics. Network controllers can set a node (usually the controller itself) to receive remote tracing events from all members of the network or from select members. Events are only sent if they pertain to a given network for security reasons.
    * Multicast replication can now be done by designated multicast replicators on a network (flagged as such at the controller) rather than by the sender. Most users won't want this, but it's useful for specialized use cases on hub-and-spoke networks and for low-power devices.
    * Cryptographic performance improvements on several platforms.
    * Multithreaded performance improvements throughout the code base, including the use of an inline lightweight spinlock for low-contention resources.
 * Bugs fixed
    * Disappearing routes on Mac (GitHub issue #600)
    * Route flapping and path instability in some dual-stack V4/V6 networks
    * Blacklist (in local.conf) doesn't work reliably (GitHub issue #656)
    * Connection instabilities due to unsigned integer overflows in timing comparisons (use int64_t instead of uint64_t)
    * Binaries don't run on some older or lower-end 32-bit ARM chips (build problem)
    * ARM NEON crypto code crashes (build problem)
    * Fixed some lock ordering issues revealed by "valgrind" tool
    * The "zerotier-idtool" command could not be accessed from "zerotier-one" via command line switch
    * Leaking sockets on some platforms when uPnP/NAT-PMP is enabled
    * Fixed two very rare multithreading issues that were only observed on certain systems
 * Platform-Specific Changes
    * MacOS
        * Installer now loads the kernel extension right away so that High Sierra users will see the prompt to authorize it. This is done in the "Security & Privacy" preference pane and must be done directly on the console (not via remote desktop). On High Sierra and newer kexts must be authorized at the console via security settings system preferences pane.
    * Windows
        * The Windows installer should now install the driver without requiring a special prompt in most cases. This should make it easier for our packages to be accepted into and updated in the Chocolatey repository and should make it easier to perform remote installs across groups of machines using IT management and provisioning tools.
        * The Windows official packages are now signed with an EV certificate (with hardware key).
        * The Windows UI can now log into ZeroTier Central and join networks via the Central API.
        * The `zerotier-idtool` command should now work on Windows without ugly hacks.
        * Upgraded the installer version.
        * Made a few changes to hopefully fix sporadic "will not uninstall" problems, though we cannot duplicate these issues ourselves.
    * Linux
        * Device names are now generated deterministically based on network IDs for all newly joined networks.
    * Android
        * Multicast now works on Android in most cases! Android apps can send and receive multicast and subscribe to multicast group IPs. Note that in some cases the app must bind to the specific correct interface for this to work.
        * IPv6 can be disabled in UI for cases where it causes problems.

## 2017-04-20 -- Version 1.2.4

 * Managed routes are now only bifurcated for the default route. This is a change in behavior, though few people will probably notice. Bifurcating all managed routes was causing more trouble than it was worth for most users.
 * Up to 2X crypto speedup on x86-64 (except Windows, which will take some porting) and 32-bit ARM platforms due to integration of fast assembly language implementations of Salsa20/12 from the [supercop](http://bench.cr.yp.to/supercop.html) code base. These were written by Daniel J. Bernstein and are in the public domain. My MacBook Pro (Core i5 2.8ghz) now does almost 1.5GiB/sec Salsa20/12 per core and a Raspberry Pi got a 2X boost. 64-bit ARM support and Windows support will take some work but should not be too hard.
 * Refactored code that manages credentials to greatly reduce memory use in most cases. This may also result in a small performance improvement.
 * Reworked and simplified path selection and priority logic to fix path instability and dead path persistence edge cases. There have been some sporadic reports of persistent path instabilities and dead paths hanging around that take minutes to resolve. These have proven difficult to reproduce in house, but hopefully this will fix them. In any case it seems to speed up path establishment in our tests and it makes the code simpler and more readable.
 * Eliminated some unused cruft from the code around path management and in the peer class.
 * Fixed an issue causing build problems on some MIPS architecture systems.
 * Fixed Windows forgetting routes on sleep/wake or in some other circumstances. (GitHub issue #465)

## 2017-03-17 -- Version 1.2.2

 * A bug causing unreliable multicast propagation (GitHub issue #461).
 * A crash in ARM binaries due to a build chain and flags problem.
 * A bug in the network controller preventing members from being listed (GitHub issue #460).

## 2017-03-14 -- Version 1.2.0

Version 1.2.0 is a major milestone release representing almost nine months of work. It includes our rules engine for distributed network packet filtering and security monitoring, federated roots, and many other architectural and UI improvements and bug fixes.
