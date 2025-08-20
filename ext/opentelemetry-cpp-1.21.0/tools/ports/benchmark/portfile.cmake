if (VCPKG_PLATFORM_TOOLSET STREQUAL "v140")
  set(CMAKE_C_COMPILER cl.exe)
  set(CMAKE_CXX_COMPILER cl.exe)
  set(MSVC_TOOLSET_VERSION 140)
else()
  # Make sure vs2019 compiled binaries are compat with vs2017
  set(VCPKG_CXX_FLAGS "/Zc:__cplusplus /d2FH4-")
  set(VCPKG_C_FLAGS "/Zc:__cplusplus /d2FH4-")
  set(PREFER PREFER_NINJA)
endif()

#https://github.com/google/benchmark/issues/661
vcpkg_fail_port_install(ON_TARGET "uwp") 

vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO google/benchmark
    REF c05843a9f622db08ad59804c190f98879b76beba # v1.5.3
    SHA512 4e4b793ff6d7a4b12ce4b98cf5faa6d5d27a0a1d7be630c0c4a2a4bca5168fd94585e5e3e1a9c43603e42145ddcded8e5a3688528bf073a068f53054b86ce774
    HEAD_REF master
    PATCHES "version.patch"
)

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
    ${PREFER}
    OPTIONS
        -DBENCHMARK_ENABLE_TESTING=OFF
        -DCMAKE_DEBUG_POSTFIX=d
)

vcpkg_install_cmake()

vcpkg_copy_pdbs()

vcpkg_fixup_cmake_targets(CONFIG_PATH lib/cmake/benchmark)

vcpkg_fixup_pkgconfig(SYSTEM_LIBRARIES pthread)

file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/include)
file(REMOVE_RECURSE ${CURRENT_PACKAGES_DIR}/debug/share)

# Handle copyright
file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/${PORT} RENAME copyright)
