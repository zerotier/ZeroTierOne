# CMake build script for libzerotiercore.a

cmake_minimum_required(VERSION 3.15)
project(zerotier-one LANGUAGES CXX C)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ZT1_CENTRAL_CONTROLLER "Build with ZeroTier Central Controller support" OFF)
option(ADDRESS_SANITIZER "Build with Address Sanitizer enabled (only for x86_64/arm64)" OFF)
option(EXT_OSDEP "Build with external osdep feature" OFF)
option(ZT_IA32 "Force a 32-bit (i386) build on x86 hosts mismarked as i386" OFF)

# Feature toggles mirrored from make-linux.mk's ZT_* switches.
option(ZT_TRACE "Enable tracing (-DZT_TRACE)" OFF)
option(ZT_RULES_ENGINE_DEBUGGING "Enable rules-engine debugging" OFF)
option(ZT_DEBUG_TRACE "Enable debug tracing" OFF)
option(ZT_USE_TEST_TAP "Build with the test tap device" OFF)
option(ZT_VAULT_SUPPORT "Enable HashiCorp Vault support (links libcurl)" OFF)
option(ZT_STATIC "Link the executables statically" OFF)
option(ZT_QNAP "Build for QNAP (embedded)" OFF)
option(ZT_UBIQUITI "Build for Ubiquiti (embedded)" OFF)
option(ZT_SYNOLOGY "Build for Synology (embedded)" OFF)

set(PROJ_DIR ${PROJECT_SOURCE_DIR})

# make builds release by default and switches to debug flags under ZT_DEBUG=1;
# mirror that with CMAKE_BUILD_TYPE, defaulting to Release when unset.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ---------------------------------------------------------------------------
# CPU architecture detection and architecture-specific flags
#
# Translated from make-linux.mk. CMAKE_SYSTEM_PROCESSOR is CMake's canonical
# target-processor variable: it reflects the host for native builds and is set
# by the toolchain file when cross-compiling. ZT_ARCHITECTURE mirrors the
# numeric arch codes used by the Makefile and is exported to the binary as
# ZT_BUILD_ARCHITECTURE (read by one.cpp via version.h).
#
# Note: node/Constants.hpp already auto-defines ZT_NO_TYPE_PUNNING for every
# non-x86 architecture, and AES.hpp auto-defines ZT_AES_NEON whenever ARM
# crypto extensions are present (-march=armv8-a+crypto). The explicit -D flags
# below are therefore belt-and-suspenders that keep this a faithful mirror of
# make-linux.mk.
# ---------------------------------------------------------------------------
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" CPU_ARCHITECTURE)
# On the Visual Studio generators CMAKE_SYSTEM_PROCESSOR reflects the *host*, not the
# target. The target architecture is CMAKE_GENERATOR_PLATFORM (-A Win32/x64/ARM64, set
# by the windows-* presets); normalize it to the canonical names the matrix matches on.
if(MSVC AND CMAKE_GENERATOR_PLATFORM)
    string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" _zt_genplat)
    if(_zt_genplat STREQUAL "win32")
        set(CPU_ARCHITECTURE "x86")
    elseif(_zt_genplat STREQUAL "x64")
        set(CPU_ARCHITECTURE "amd64")
    elseif(_zt_genplat STREQUAL "arm64")
        set(CPU_ARCHITECTURE "arm64")
    endif()
    message(STATUS "MSVC target platform (CMAKE_GENERATOR_PLATFORM): ${CMAKE_GENERATOR_PLATFORM} -> ${CPU_ARCHITECTURE}")
endif()
message(STATUS "Target CPU (CMAKE_SYSTEM_PROCESSOR): ${CMAKE_SYSTEM_PROCESSOR}")

# ZeroTier architecture code (see node/Constants.hpp); 999 = unknown.
set(ZT_ARCHITECTURE 999)
# Crypto-assembly gates, also consumed by node/CMakeLists.txt to select sources.
set(ZT_USE_X64_ASM_SALSA FALSE)
set(ZT_USE_X64_ASM_ED25519 FALSE)
set(ZT_USE_ARM32_NEON_ASM_CRYPTO FALSE)
set(ZT_SSO_SUPPORTED FALSE)

if(CPU_ARCHITECTURE STREQUAL "x86_64" OR CPU_ARCHITECTURE STREQUAL "amd64")
    set(ZT_ARCHITECTURE 2)
    set(ZT_USE_X64_ASM_SALSA TRUE)
    # x64 ed25519 asm (qhasm): Linux/BSD only here, matching make-mac.mk which omits it on
    # macOS (macOS uses the portable Ed25519). NB: it *does* assemble on macOS and the .s
    # export macOS-underscored symbols, so this is a parity choice with make-mac.mk, not a
    # technical limitation.
    if(NOT APPLE)
        set(ZT_USE_X64_ASM_ED25519 TRUE)
    endif()
    set(ZT_SSO_SUPPORTED TRUE)
    # macOS x86_64 has SSE2 by default; skip the explicit flag (it would also leak into
    # the arm64 slice of a universal build). See the aarch64 branch note below.
    if(NOT MSVC AND NOT APPLE)
        add_compile_options(-msse -msse2)
    endif()
elseif(CPU_ARCHITECTURE STREQUAL "e2k" OR CPU_ARCHITECTURE STREQUAL "e2k64")
    # Elbrus 2000: x86-compatible arch code, but no x86 crypto assembly.
    set(ZT_ARCHITECTURE 2)
elseif(CPU_ARCHITECTURE STREQUAL "i386" OR CPU_ARCHITECTURE STREQUAL "i486"
        OR CPU_ARCHITECTURE STREQUAL "i586" OR CPU_ARCHITECTURE STREQUAL "i686"
        OR CPU_ARCHITECTURE STREQUAL "x86")
    set(ZT_ARCHITECTURE 1)
    set(ZT_SSO_SUPPORTED TRUE)
elseif(CPU_ARCHITECTURE STREQUAL "aarch64" OR CPU_ARCHITECTURE STREQUAL "arm64")
    set(ZT_ARCHITECTURE 4)
    set(ZT_SSO_SUPPORTED TRUE)
    # On Apple, omit the explicit arch defines/flags and let Constants.hpp / AES.hpp
    # self-detect from compiler builtins (per -arch slice) -- this is what make-mac.mk
    # does, and it's required for universal (arm64 + x86_64) builds, where a global
    # -march=armv8-a+crypto or -DZT_ARCH_ARM_HAS_NEON would wrongly hit the x86_64 slice.
    if(NOT APPLE)
        add_definitions(-DZT_NO_TYPE_PUNNING -DZT_ARCH_ARM_HAS_NEON)
        if(NOT MSVC)
            add_compile_options(-march=armv8-a+crypto -mtune=generic -mstrict-align)
        endif()
    endif()
elseif(CPU_ARCHITECTURE MATCHES "^(arm|armel|armhf|armv6|armv6l|armv6k|armv6kz|armv6zk|armv7|armv7l|armv7hl|armv7ve)$")
    set(ZT_ARCHITECTURE 3)
    set(ZT_USE_ARM32_NEON_ASM_CRYPTO TRUE)
    add_definitions(-DZT_NO_TYPE_PUNNING)
    if(CPU_ARCHITECTURE STREQUAL "armhf")
        set(ZT_SSO_SUPPORTED TRUE)
    endif()
elseif(CPU_ARCHITECTURE STREQUAL "powerpc64le")
    set(ZT_ARCHITECTURE 8)
    add_definitions(-DZT_NO_TYPE_PUNNING)
elseif(CPU_ARCHITECTURE STREQUAL "powerpc")
    set(ZT_ARCHITECTURE 8)
    add_definitions(-DZT_NO_TYPE_PUNNING -DZT_NO_CAPABILITIES)
elseif(CPU_ARCHITECTURE STREQUAL "ppc64le" OR CPU_ARCHITECTURE STREQUAL "ppc64el")
    set(ZT_ARCHITECTURE 8)
elseif(CPU_ARCHITECTURE STREQUAL "mips" OR CPU_ARCHITECTURE STREQUAL "mipsel")
    set(ZT_ARCHITECTURE 5)
    add_definitions(-DZT_NO_TYPE_PUNNING)
elseif(CPU_ARCHITECTURE STREQUAL "mips64" OR CPU_ARCHITECTURE STREQUAL "mips64el")
    set(ZT_ARCHITECTURE 6)
    add_definitions(-DZT_NO_TYPE_PUNNING)
elseif(CPU_ARCHITECTURE STREQUAL "s390x")
    set(ZT_ARCHITECTURE 16)
elseif(CPU_ARCHITECTURE STREQUAL "riscv64")
    set(ZT_ARCHITECTURE 0)
elseif(CPU_ARCHITECTURE STREQUAL "loongarch64")
    set(ZT_ARCHITECTURE 17)
    add_definitions(-DZT_NO_TYPE_PUNNING)
endif()

# Embedded platform markers (make-linux.mk); these also disable SSO.
if(ZT_QNAP)
    add_definitions(-D__QNAP__)
    set(ZT_EMBEDDED TRUE)
endif()
if(ZT_UBIQUITI)
    add_definitions(-D__UBIQUITI__)
    set(ZT_EMBEDDED TRUE)
endif()
if(ZT_SYNOLOGY)
    add_definitions(-D__SYNOLOGY__)
    set(ZT_EMBEDDED TRUE)
endif()

# SSO is unavailable for external-osdep and embedded builds (make-linux.mk).
if(EXT_OSDEP OR ZT_EMBEDDED)
    set(ZT_SSO_SUPPORTED FALSE)
endif()

# Fail if the architecture could not be determined.
if(ZT_ARCHITECTURE EQUAL 999)
    message(FATAL_ERROR
        "Unsupported/undetected CPU architecture '${CMAKE_SYSTEM_PROCESSOR}'. "
        "Add a case for it above (mirror make-linux.mk).")
endif()
message(STATUS "ZeroTier architecture code (ZT_BUILD_ARCHITECTURE): ${ZT_ARCHITECTURE}")

# Intel 32-bit override: some images mismark x86_64 as i386. Force 32-bit and
# disable x86-64 crypto assembly.
if(ZT_IA32)
    add_compile_options(-m32)
    add_link_options(-m32)
    set(ZT_USE_X64_ASM_SALSA FALSE)
    set(ZT_USE_X64_ASM_ED25519 FALSE)
endif()

# "ARM32 hell": conservative per-board flags. Mirrors make-linux.mk, which
# branches on the Debian architecture (host-based dpkg probe, as in the
# Makefile) and the EXT_OSDEP feature. NEON crypto assembly ends up disabled
# on arm32 except in the EXT_OSDEP path.
if(ZT_ARCHITECTURE EQUAL 3)
    execute_process(
        COMMAND dpkg --print-architecture
        OUTPUT_VARIABLE ZT_DPKG_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(ZT_DPKG_ARCH STREQUAL "armel")
        add_compile_options(-march=armv5t -mfloat-abi=soft -msoft-float -mno-unaligned-access -marm)
        set(ZT_USE_ARM32_NEON_ASM_CRYPTO FALSE)
    elseif(NOT EXT_OSDEP)
        add_compile_options(-mfloat-abi=hard -march=armv6zk -marm -mfpu=vfp -mno-unaligned-access -mtp=cp15 -mcpu=arm1176jzf-s)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
        set(ZT_USE_ARM32_NEON_ASM_CRYPTO FALSE)
    else()
        add_definitions(-DZT_NO_PEER_METRICS)
    endif()
endif()

# MSVC cannot assemble the GNU .s crypto sources; use the SSE-intrinsic Salsa20
# instead (matches windows/ZeroTierOne.vcxproj). The x64 default ISA already
# includes SSE2, so the -msse/-march/-m32 options above (guarded if(NOT MSVC))
# aren't needed here.
if(MSVC)
    set(ZT_USE_X64_ASM_SALSA FALSE)
    set(ZT_USE_X64_ASM_ED25519 FALSE)
    set(ZT_USE_ARM32_NEON_ASM_CRYPTO FALSE)
    # SSE-intrinsic Salsa20 on Intel targets only (x86 = 1, x64 = 2); ARM64 (4) uses
    # the portable path. Matches windows/ZeroTierOne.vcxproj, which defines
    # ZT_SALSA20_SSE for both Win32 and x64 with the *default* ISA -- MSVC's SSE/SSE2
    # intrinsics are always available, so no /arch flag is needed (and 32-bit MSVC
    # already defaults to /arch:SSE2).
    if(ZT_ARCHITECTURE EQUAL 1 OR ZT_ARCHITECTURE EQUAL 2)
        add_definitions(-DZT_SALSA20_SSE)
    endif()
endif()

# Single sign-on (OIDC) support.
if(ZT_SSO_SUPPORTED)
    add_definitions(-DZT_SSO_SUPPORTED=1)
endif()

# A universal macOS build can't put the per-arch crypto .s asm in the (fat) core target --
# it would be assembled for the wrong slice. Mirror make-mac.mk: keep the x64 Salsa20/12
# asm but build it as a separate x86_64-only object (node/CMakeLists.txt) and define
# ZT_USE_X64_ASM_SALSA2012 so Packet.cpp uses it -- its `&& ZT_ARCH_X64` guard keeps it off
# the arm64 slice. The ed25519/arm32 asm aren't used on macOS, so drop those.
list(LENGTH CMAKE_OSX_ARCHITECTURES _zt_osx_arch_count)
if(APPLE AND _zt_osx_arch_count GREATER 1)
    set(ZT_USE_X64_ASM_SALSA FALSE)              # not added to the fat core target...
    set(ZT_MACOS_UNIVERSAL_X64_SALSA_ASM TRUE)   # ...built x86_64-only and linked in instead
    add_definitions(-DZT_USE_X64_ASM_SALSA2012)
    set(ZT_USE_X64_ASM_ED25519 FALSE)
    set(ZT_USE_ARM32_NEON_ASM_CRYPTO FALSE)
endif()

# Faster crypto assembly. These -D gates pair with the source selection in
# node/CMakeLists.txt, which reads the ZT_USE_*_ASM_* variables set above.
if(ZT_USE_X64_ASM_SALSA)
    add_definitions(-DZT_USE_X64_ASM_SALSA2012)
endif()
if(ZT_USE_X64_ASM_ED25519)
    add_definitions(-DZT_USE_FAST_X64_ED25519)
endif()
if(ZT_USE_ARM32_NEON_ASM_CRYPTO)
    add_definitions(-DZT_USE_ARM32_NEON_ASM_SALSA2012)
endif()

# Build platform code reported into the binary (read by one.cpp via version.h).
# Codes match the other build systems: make-linux.mk = 1, Windows .vcxproj = 2,
# make-mac.mk = 3, root Makefile FreeBSD = 7 / OpenBSD = 9. NetBSD has no
# assigned code anywhere, so it falls back to version.h's default of 0.
if(APPLE)
    set(ZT_BUILD_PLATFORM 3)
elseif(WIN32)
    set(ZT_BUILD_PLATFORM 2)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(ZT_BUILD_PLATFORM 7)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
    set(ZT_BUILD_PLATFORM 9)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ZT_BUILD_PLATFORM 1)
else()
    set(ZT_BUILD_PLATFORM 0)
endif()
add_definitions(-DZT_BUILD_PLATFORM=${ZT_BUILD_PLATFORM} -DZT_BUILD_ARCHITECTURE=${ZT_ARCHITECTURE})

# Address Sanitizer (only meaningful on x86_64/arm64).
if(ADDRESS_SANITIZER AND NOT MSVC AND (ZT_ARCHITECTURE EQUAL 2 OR ZT_ARCHITECTURE EQUAL 4))
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif()

# ---------------------------------------------------------------------------
# C++ language standard
#
# make-linux.mk compiles everything as C++17 (controller and daemon alike), so
# we do the same. Extensions stay ON, which yields -std=gnu++17 -- a superset of
# the Makefile's -std=c++17, so anything that builds under make also builds here.
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to conform to" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED True CACHE BOOL "C++ standard required" FORCE)
set(CMAKE_CXX_EXTENSIONS ON CACHE BOOL "Enable compiler-specific extensions" FORCE)

# ---------------------------------------------------------------------------
# Global compile definitions
# ---------------------------------------------------------------------------
add_definitions(-DCMAKE_BUILD)

# The Central Controller is not supported on Windows (no libpq / redis / google-
# cloud-cpp path there); the daemon is the only supported Windows build.
if(WIN32 AND ZT1_CENTRAL_CONTROLLER)
    message(FATAL_ERROR
        "ZT1_CENTRAL_CONTROLLER is not supported on Windows -- build the daemon "
        "(configure without -DZT1_CENTRAL_CONTROLLER).")
endif()

# ZT_NONFREE pulls in the bundled FileDB-based network controller -- the controller
# that ships in official daemon builds. Mirrors make-linux.mk, where ZT_CONTROLLER=1
# implies ZT_NONFREE=1; ZT1_CENTRAL_CONTROLLER is a superset that adds the Postgres/
# PubSub/BigTable/Redis backends on top (handled inside nonfree/controller).
option(ZT_NONFREE "Build the bundled FileDB network controller into the daemon (required by ZT1_CENTRAL_CONTROLLER)" ON)
if(ZT1_CENTRAL_CONTROLLER)
    set(ZT_NONFREE ON)
endif()
if(ZT_NONFREE)
    add_definitions(-DZT_NONFREE_CONTROLLER=1)
endif()

if(ZT1_CENTRAL_CONTROLLER)
    add_definitions(
        -DZT_CONTROLLER_USE_LIBPQ=1
        -DZT1_CENTRAL_CONTROLLER=1
        -DZT_OPENTELEMETRY_ENABLED=1
        -DZT_NO_PEER_METRICS=1
    )
    # The controller installs a fatal-signal handler (one.cpp) that prints a backtrace
    # via backtrace()/backtrace_symbols_fd() on SIGSEGV/SIGABRT/etc. Keep debug info even
    # in Release (-O3 -g; no runtime cost) and export the executable's dynamic symbols
    # (-rdynamic) so the backtrace resolves function names instead of bare addresses.
    # Controller-only; not built on Windows (enforced above).
    if(NOT MSVC)
        add_compile_options(-g)
        add_link_options(-rdynamic)
    endif()
endif()

# ---------------------------------------------------------------------------
# Compiler and linker flags (translated from make-linux.mk)
#
# CMAKE_BUILD_TYPE supplies -O3 -DNDEBUG for Release and -g for Debug; the lines
# below add the remaining flags make uses so the two builds match. make applies
# these to ZeroTier's own objects only; here they are global, so bundled
# third-party targets see them too (harmless -- we set no -Werror).
# ---------------------------------------------------------------------------

# Warnings: every configuration, matching make.
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wno-deprecated)
endif()

# Position independence: PIC for all libraries, PIE for the executables (paired
# with -pie below). Equivalent to make's -fPIC -fPIE hardening, applied the
# idiomatic CMake way so -fPIE does NOT leak into bundled shared libraries
# (e.g. redis++), which fails to link on aarch64. (No-op on MSVC/Windows.)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    # Static CRT to match the vcxproj's /MT (/MTd for Debug). CMP0091 (NEW via
    # cmake_minimum_required >= 3.15) routes this instead of patching CMAKE_*_FLAGS.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # /MP parallel compile, /EHsc C++ exceptions, /utf-8 source + exec charset.
    add_compile_options(/MP /EHsc /utf-8)
    # Always-on Windows defines from the vcxproj.
    add_compile_definitions(WIN32 NOMINMAX FD_SETSIZE=1024 _CRT_SECURE_NO_WARNINGS)
else()
    # Release adds a stack protector; debug uses -O1 globally (with -O2 for the hot
    # crypto sources, set per-file in node/CMakeLists.txt).
    add_compile_options($<$<NOT:$<CONFIG:Debug>>:-fstack-protector>)
    add_compile_options($<$<CONFIG:Debug>:-O1>)
endif()

# make's debug build also defines these (ZT_DEBUG implies ZT_TRACE).
add_compile_definitions($<$<CONFIG:Debug>:ZT_DEBUG> $<$<CONFIG:Debug>:ZT_TRACE>)

# Stop libstdc++ from pulling in its mt/pool/extptr/debug allocators (GNU
# libstdc++ only; no-op on macOS/libc++, irrelevant to MSVC's STL).
if(NOT MSVC)
    add_definitions(-D_MT_ALLOCATOR_H -D_POOL_ALLOCATOR_H -D_EXTPTR_ALLOCATOR_H -D_DEBUG_ALLOCATOR_H)
endif()

# GNU-ld hardening (Linux/BSD only -- not understood by the macOS or MSVC linkers).
# make applies -Wl,-z,noexecstack to every build and -pie -Wl,-z,relro,-z,now to
# release, and disables self-update on Linux/BSD (left to package management).
if(UNIX AND NOT APPLE)
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,-z,noexecstack")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -pie -Wl,-z,relro,-z,now")
    add_compile_definitions(ZT_SOFTWARE_UPDATE_DEFAULT="disable")
elseif(WIN32)
    # Windows daemon defines from the vcxproj. ZT_EXPORT takes the dllexport branch in
    # include/ZeroTierOne.h, which suppresses its `#pragma comment(lib, "ZeroTierOne_x64.lib")`
    # auto-link of the prebuilt SDK static library -- we build the core in-tree, not against
    # the SDK lib. Self-update default: "apply" on release, "disable" on debug.
    add_compile_definitions(STATICLIB ZT_SSO_ENABLED=1 ZT_USE_MINIUPNPC MINIUPNP_STATICLIB ZT_EXPORT)
    add_compile_definitions(ZT_SOFTWARE_UPDATE_DEFAULT="$<IF:$<CONFIG:Debug>,disable,apply>")
endif()

# Optional feature toggles (mirror make-linux.mk's ZT_* switches).
if(ZT_TRACE)
    add_definitions(-DZT_TRACE)
endif()
if(ZT_RULES_ENGINE_DEBUGGING)
    add_definitions(-DZT_RULES_ENGINE_DEBUGGING)
endif()
if(ZT_DEBUG_TRACE)
    add_definitions(-DZT_DEBUG_TRACE)
endif()
if(ZT_USE_TEST_TAP)
    add_definitions(-DZT_USE_TEST_TAP)
endif()
if(ZT_VAULT_SUPPORT)
    add_definitions(-DZT_VAULT_SUPPORT=1)
endif()
if(ZT_STATIC)
    string(APPEND CMAKE_EXE_LINKER_FLAGS " -static")
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(FetchContent)
include(ExternalProject)

# OpenSSL is not used by the C++ daemon: the control plane is plain-HTTP cpp-httplib
# (HTTPLIB_USE_OPENSSL_IF_AVAILABLE=OFF) and SSO/OIDC TLS lives in rustybits (native-tls).
# It IS needed for the controller (libpq/redis TLS) and on Linux/BSD (rustybits' native-tls
# links system OpenSSL there). macOS/Windows daemons need none -- which is what lets a
# universal macOS build avoid a fat OpenSSL.
if(ZT1_CENTRAL_CONTROLLER OR (UNIX AND NOT APPLE))
    set(ZT_NEED_OPENSSL TRUE)
endif()
if(ZT_NEED_OPENSSL)
    find_package(OpenSSL REQUIRED)
endif()
find_package(nlohmann_json REQUIRED)
# inja: found if installed (Homebrew), otherwise fetched (Debian has no package).
include(cmake/inja.cmake)

# Threads (prefer pthreads)
set(THREADS_PREFER_PTHREAD_FLAG TRUE CACHE INTERNAL "Use pthreads" FORCE)
find_package(Threads REQUIRED)
if(CMAKE_USE_PTHREADS_INIT)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# OpenTelemetry: the controller build needs the full SDK + exporters; everyone
# else only needs the API.
if(ZT1_CENTRAL_CONTROLLER)
    find_package(PostgreSQL REQUIRED)
    # opentelemetry-cpp + google-cloud-cpp come from scripts/bootstrap-deps.sh. Find
    # them non-fatally so we can emit an actionable error (vs CMake's default) if the
    # prefix wasn't provided. A wrong-mode prefix (api-only OTel) also lands here,
    # since the missing sdk/exporter components leave opentelemetry-cpp_FOUND false.
    find_package(opentelemetry-cpp QUIET COMPONENTS api sdk exporters_otlp_grpc exporters_otlp_http)
    find_package(google_cloud_cpp_bigtable QUIET)
    find_package(google_cloud_cpp_pubsub QUIET)
    if(NOT opentelemetry-cpp_FOUND OR NOT google_cloud_cpp_bigtable_FOUND OR NOT google_cloud_cpp_pubsub_FOUND)
        message(FATAL_ERROR [=[
Controller dependencies (opentelemetry-cpp and/or google-cloud-cpp) were not found on
CMAKE_PREFIX_PATH. Build them, then re-run CMake pointing at the prefix:

    ZT_CONTROLLER_DEPS=1 scripts/bootstrap-deps.sh   # builds OTel + google-cloud-cpp into ./.deps
    cmake -DZT1_CENTRAL_CONTROLLER=1 -DCMAKE_PREFIX_PATH="<prefix>" -S . -B build

bootstrap-deps.sh prints the exact -DCMAKE_PREFIX_PATH to use when it finishes.
See nonfree/controller/README_CENTRAL_CONTROLLER.md.
]=])
    endif()
else()
    # Daemon/non-controller: only the OTel API (header-only), from the same bootstrap prefix.
    find_package(opentelemetry-cpp QUIET COMPONENTS api)
    if(NOT opentelemetry-cpp_FOUND)
        message(FATAL_ERROR [=[
opentelemetry-cpp (API) was not found on CMAKE_PREFIX_PATH. Build the header-only OTel
API, then re-run CMake pointing at the prefix:

    scripts/bootstrap-deps.sh                      # quick, header-only (this is the default)
    cmake -DCMAKE_PREFIX_PATH="<prefix>" -S . -B build

bootstrap-deps.sh prints the exact -DCMAKE_PREFIX_PATH to use when it finishes.
]=])
    endif()
endif()

# Bundled / fetched dependencies. redis-plus-plus is controller-only (the daemon
# never links it -- all redis++ references sit behind ZT1_CENTRAL_CONTROLLER), so
# only pull it in for the controller build. This also keeps it out of the Windows
# daemon build, where the controller is unsupported.
include(cmake/cpp-httplib.cmake)
include(cmake/miniupnpc.cmake)
if(ZT1_CENTRAL_CONTROLLER)
    include(cmake/redis-plus-plus.cmake)
endif()

# ---------------------------------------------------------------------------
# rustybits (built via cargo, linked as a static library)
# ---------------------------------------------------------------------------
if(WIN32)
    # cargo builds for the host triple by default; cross-target the selected arch
    # explicitly so an x64 host can build x86/ARM64. The lib then lands under
    # target/<triple>/release. (The cbindgen header always goes to target/rustybits.h
    # per rustybits/build.rs, regardless of --target.) Requires the Rust target to be
    # installed: rustup target add {i686,aarch64}-pc-windows-msvc.
    if(ZT_ARCHITECTURE EQUAL 1)
        set(RUSTYBITS_TRIPLE i686-pc-windows-msvc)
    elseif(ZT_ARCHITECTURE EQUAL 4)
        set(RUSTYBITS_TRIPLE aarch64-pc-windows-msvc)
    else()
        set(RUSTYBITS_TRIPLE x86_64-pc-windows-msvc)
    endif()
    set(RUSTYBITS_LIB ${PROJ_DIR}/rustybits/target/${RUSTYBITS_TRIPLE}/release/rustybits.lib)
    set(RUSTYBITS_BUILD_COMMAND cd ${PROJ_DIR}/rustybits && cargo build --release --target ${RUSTYBITS_TRIPLE})
elseif(APPLE)
    # Honor CMAKE_OSX_ARCHITECTURES so rustybits matches the C/C++ slices. cargo builds
    # one target triple at a time, so a universal (multi-arch) build builds each arch and
    # lipo's them into one fat archive. Unset = native host build. Cross / universal builds
    # need the Rust target(s): rustup target add {x86_64,aarch64}-apple-darwin.
    set(_rb_triples "")
    foreach(_a IN LISTS CMAKE_OSX_ARCHITECTURES)
        if(_a STREQUAL "x86_64")
            list(APPEND _rb_triples x86_64-apple-darwin)
        elseif(_a STREQUAL "arm64")
            list(APPEND _rb_triples aarch64-apple-darwin)
        endif()
    endforeach()
    list(LENGTH _rb_triples _rb_n)
    if(_rb_n GREATER 1)
        # Universal: build each arch, then lipo into a single fat static lib.
        set(RUSTYBITS_LIB ${PROJ_DIR}/rustybits/target/zt-universal/librustybits.a)
        set(RUSTYBITS_BUILD_COMMAND "")
        set(_rb_lipo_inputs "")
        foreach(_t IN LISTS _rb_triples)
            if(RUSTYBITS_BUILD_COMMAND)
                list(APPEND RUSTYBITS_BUILD_COMMAND COMMAND)
            endif()
            list(APPEND RUSTYBITS_BUILD_COMMAND cd ${PROJ_DIR}/rustybits && cargo build --release --target ${_t})
            list(APPEND _rb_lipo_inputs ${PROJ_DIR}/rustybits/target/${_t}/release/librustybits.a)
        endforeach()
        list(APPEND RUSTYBITS_BUILD_COMMAND
            COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJ_DIR}/rustybits/target/zt-universal
            COMMAND lipo -create -output ${RUSTYBITS_LIB} ${_rb_lipo_inputs})
    elseif(_rb_n EQUAL 1)
        # Single explicit arch (e.g. cross-compiling x86_64 on an arm64 host).
        set(RUSTYBITS_LIB ${PROJ_DIR}/rustybits/target/${_rb_triples}/release/librustybits.a)
        set(RUSTYBITS_BUILD_COMMAND cd ${PROJ_DIR}/rustybits && cargo build --release --target ${_rb_triples})
    else()
        # No explicit arch: native host build.
        set(RUSTYBITS_LIB ${PROJ_DIR}/rustybits/target/release/librustybits.a)
        set(RUSTYBITS_BUILD_COMMAND cd ${PROJ_DIR}/rustybits && cargo build --release)
    endif()
else()
    # Linux / BSD: native host build.
    set(RUSTYBITS_LIB ${PROJ_DIR}/rustybits/target/release/librustybits.a)
    set(RUSTYBITS_BUILD_COMMAND cd ${PROJ_DIR}/rustybits && cargo build --release)
endif()
set(RUSTYBITS_INCLUDE_DIR ${PROJ_DIR}/rustybits/target)

ExternalProject_Add(
    rustybits_build
    DOWNLOAD_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${RUSTYBITS_BUILD_COMMAND}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${RUSTYBITS_LIB} ${RUSTYBITS_INCLUDE_DIR}/rustybits.h
)

add_library(rustybits STATIC IMPORTED GLOBAL)
set_property(TARGET rustybits PROPERTY IMPORTED_LOCATION ${RUSTYBITS_LIB})
add_dependencies(rustybits rustybits_build)

# Make `make clean` also clean rustybits. CMake's clean target can only delete
# files (it can't run `cargo clean`), but removing the target dir is exactly
# what `cargo clean` does.
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${PROJ_DIR}/rustybits/target)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
add_subdirectory(ext)
add_subdirectory(node)
add_subdirectory(osdep)
add_subdirectory(service)
# The bundled network controller is built only for ZT_NONFREE (official) builds.
if(ZT_NONFREE)
    add_subdirectory(nonfree)
endif()

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
set(LINKED_LIBRARIES
    prometheus-cpp-lite
    zerotier-service
    zerotier-osdep
    zerotier-core
    Threads::Threads
    nlohmann_json::nlohmann_json
    opentelemetry-cpp::api
    rustybits
)
# OpenSSL only where it's actually needed (controller / Linux rustybits); the macOS/
# Windows daemon links none. See the find_package(OpenSSL) note above.
if(ZT_NEED_OPENSSL)
    list(APPEND LINKED_LIBRARIES OpenSSL::Crypto OpenSSL::SSL)
endif()

# The bundled FileDB network controller (only built when ZT_NONFREE; central
# backends are layered in under ZT1_CENTRAL_CONTROLLER inside the target).
if(ZT_NONFREE)
    list(APPEND LINKED_LIBRARIES zerotier-controller)
endif()

# Controller-only OpenTelemetry exporters.
if(ZT1_CENTRAL_CONTROLLER)
    list(APPEND LINKED_LIBRARIES
        opentelemetry-cpp::sdk
        opentelemetry-cpp::trace
        opentelemetry-cpp::proto_grpc
        opentelemetry-cpp::otlp_grpc_client
        opentelemetry-cpp::otlp_grpc_exporter
        opentelemetry-cpp::otlp_grpc_log_record_exporter
        opentelemetry-cpp::otlp_grpc_metrics_exporter
        opentelemetry-cpp::otlp_http_exporter
        opentelemetry-cpp::otlp_http_log_record_exporter
        opentelemetry-cpp::otlp_http_metric_exporter
    )
endif()

# HashiCorp Vault support links libcurl (make-linux.mk).
if(ZT_VAULT_SUPPORT)
    find_package(CURL REQUIRED)
    list(APPEND LINKED_LIBRARIES CURL::libcurl)
endif()

# Apple system frameworks.
if(APPLE)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(SECURITY_LIBRARY Security)
    find_library(SYSTEM_CONFIGURATION_LIBRARY SystemConfiguration)
    find_library(CARBON_LIBRARY Carbon)
    find_library(CORESERVICES_LIBRARY CoreServices)
    list(APPEND LINKED_LIBRARIES
        ${COREFOUNDATION_LIBRARY}
        ${SECURITY_LIBRARY}
        ${CARBON_LIBRARY}
        ${SYSTEM_CONFIGURATION_LIBRARY}
        ${CORESERVICES_LIBRARY}
    )
endif()

# Windows system libraries (from windows/ZeroTierOne/ZeroTierOne.vcxproj).
if(WIN32)
    list(APPEND LINKED_LIBRARIES
        ws2_32 wsock32 Iphlpapi Rpcrt4 bcrypt crypt32 ncrypt ntdll secur32 userenv wbemuuid)
endif()

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
add_executable(zerotier-one
    one.cpp
    ext/http-parser/http_parser.c
)
# On Windows one.cpp pulls in the service wrapper and includes it as
# "windows/ZeroTierOne/...", so compile those sources in and add the repo root to
# the include path. (The SDK and TAP driver stay in the MSBuild solution.)
if(WIN32)
    target_sources(zerotier-one PRIVATE
        windows/ZeroTierOne/ServiceBase.cpp
        windows/ZeroTierOne/ServiceInstaller.cpp
        windows/ZeroTierOne/ZeroTierOneService.cpp
    )
    target_include_directories(zerotier-one PRIVATE ${PROJ_DIR})
endif()
target_link_libraries(zerotier-one ${LINKED_LIBRARIES})

# The Windows .vcxproj doesn't build the selftest; skip it on Windows too.
if(NOT WIN32)
    add_executable(zerotier-selftest
        selftest.cpp
    )
    target_link_libraries(zerotier-selftest
        zerotier-core
        zerotier-osdep
        Threads::Threads
        nlohmann_json::nlohmann_json
        prometheus-cpp-lite
    )
endif()

# This Windows regression uses a process-local registry sandbox, not live NRPT.
if(WIN32)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(zerotier-windows-dns-test
            windows/WinDNSHelperTest.cpp
            osdep/WinDNSHelper.cpp
        )
        target_link_libraries(zerotier-windows-dns-test PRIVATE zerotier-core ws2_32 advapi32 ole32)
        add_test(NAME windows-dns-registry COMMAND zerotier-windows-dns-test)
        add_test(NAME windows-dns-registry-existing COMMAND zerotier-windows-dns-test --existing)
    endif()
endif()
