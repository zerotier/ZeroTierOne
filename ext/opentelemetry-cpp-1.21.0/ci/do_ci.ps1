# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

$ErrorActionPreference = "Stop";
trap { $host.SetShouldExit(1) }

$action = $args[0]

$nproc = (Get-ComputerInfo).CsNumberOfLogicalProcessors

$SRC_DIR = (Get-Item -Path ".\").FullName

# Workaround https://github.com/bazelbuild/bazel/issues/18683
$BAZEL_STARTUP_OPTIONS = "--output_base=C:\O"
$BAZEL_OPTIONS = "--copt=-DENABLE_ASYNC_EXPORT --compilation_mode=dbg"
$BAZEL_TEST_OPTIONS = "$BAZEL_OPTIONS --test_output=errors"

if (!(test-path build)) {
  mkdir build
}
$BUILD_DIR = Join-Path "$SRC_DIR" "build"

if (!(test-path install_test)) {
  mkdir install_test
}
$INSTALL_TEST_DIR = Join-Path "$SRC_DIR" "install_test"

if (!(test-path plugin)) {
  mkdir plugin
}
$PLUGIN_DIR = Join-Path "$SRC_DIR" "plugin"

$VCPKG_DIR = Join-Path "$SRC_DIR" "tools/vcpkg"

$Env:CTEST_OUTPUT_ON_FAILURE = "1"

switch ($action) {
  "bazel.build" {
    bazel $BAZEL_STARTUP_OPTIONS build $BAZEL_OPTIONS --action_env=VCPKG_DIR=$VCPKG_DIR --deleted_packages=opentracing-shim -- //...
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.dll.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      -DOPENTELEMETRY_BUILD_DLL=1 `
     "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    $env:PATH = "$BUILD_DIR\ext\src\dll\Debug;$env:PATH"
    examples\simple\Debug\example_simple.exe
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    examples\metrics_simple\Debug\metrics_ostream_example.exe
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    examples\logs_simple\Debug\example_logs_simple.exe
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.dll.cxx20.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DCMAKE_CXX_STANDARD=20 `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      -DOPENTELEMETRY_BUILD_DLL=1 `
     "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    $env:PATH = "$BUILD_DIR\ext\src\dll\Debug;$env:PATH"
    examples\simple\Debug\example_simple.exe
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    examples\metrics_simple\Debug\metrics_ostream_example.exe
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    examples\logs_simple\Debug\example_logs_simple.exe
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.maintainer.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DWITH_OTLP_GRPC=ON `
      -DWITH_OTLP_HTTP=ON `
      -DWITH_OTLP_RETRY_PREVIEW=ON `
      -DOTELCPP_MAINTAINER_MODE=ON `
      -DWITH_NO_DEPRECATED_CODE=ON `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.maintainer.cxx20.stl.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DWITH_STL=CXX20 `
      -DCMAKE_CXX_STANDARD=20 `
      -DWITH_OTLP_GRPC=ON `
      -DWITH_OTLP_HTTP=ON `
      -DWITH_OTLP_RETRY_PREVIEW=ON `
      -DOTELCPP_MAINTAINER_MODE=ON `
      -DWITH_NO_DEPRECATED_CODE=ON `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.maintainer.abiv2.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DWITH_OTLP_GRPC=ON `
      -DWITH_OTLP_HTTP=ON `
      -DWITH_OTLP_RETRY_PREVIEW=ON `
      -DOTELCPP_MAINTAINER_MODE=ON `
      -DWITH_NO_DEPRECATED_CODE=ON `
      -DWITH_ABI_VERSION_1=OFF `
      -DWITH_ABI_VERSION_2=ON `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.with_async_export.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      -DWITH_ASYNC_EXPORT_PREVIEW=ON `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.exporter.otprotocol.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      -DWITH_OTLP_GRPC=ON `
      -DWITH_OTLP_HTTP=ON `
      -DWITH_OTLP_RETRY_PREVIEW=ON `
      -DWITH_OTPROTCOL=ON `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.exporter.otprotocol.dll.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      -DOPENTELEMETRY_BUILD_DLL=1 `
      -DWITH_OTPROTCOL=ON `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.exporter.otprotocol.with_async_export.test" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      -DWITH_ASYNC_EXPORT_PREVIEW=ON `
      -DWITH_OTPROTCOL=ON `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.build_example_plugin" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      -DWITH_ASYNC_EXPORT_PREVIEW=ON `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cp examples/plugin/plugin/Debug/example_plugin.dll ${PLUGIN_DIR}
  }
  "cmake.test_example_plugin" {
    cd "$BUILD_DIR"
    cmake $SRC_DIR `
      -DVCPKG_TARGET_TRIPLET=x64-windows `
      -DWITH_ASYNC_EXPORT_PREVIEW=ON `
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
    cp examples/plugin/plugin/Debug/example_plugin.dll ${PLUGIN_DIR}
    $config = New-TemporaryFile
    examples/plugin/load/Debug/load_plugin_example.exe ${PLUGIN_DIR}/example_plugin.dll $config
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }
  }
  "cmake.install.test" {
    Remove-Item -Recurse -Force "$BUILD_DIR\*"
    Remove-Item -Recurse -Force "$INSTALL_TEST_DIR\*"
    cd "$BUILD_DIR"

    if (Test-Path Env:\CXX_STANDARD) {
        $CXX_STANDARD = [int](Get-Item Env:\CXX_STANDARD).Value
    } else {
        $CXX_STANDARD = 14
    }
    if (-not $CXX_STANDARD) {
        $CXX_STANDARD = 14
    }
    Write-Host "Using CXX_STANDARD: $CXX_STANDARD"
      
    $CMAKE_OPTIONS = @(
    "-DCMAKE_CXX_STANDARD=$CXX_STANDARD",
    "-DCMAKE_CXX_STANDARD_REQUIRED=ON",
    "-DCMAKE_CXX_EXTENSIONS=OFF",
    "-DVCPKG_TARGET_TRIPLET=x64-windows",
    "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    )

    cmake $SRC_DIR `
      $CMAKE_OPTIONS `
      "-DCMAKE_INSTALL_PREFIX=$INSTALL_TEST_DIR" `
      -DWITH_ABI_VERSION_1=OFF `
      -DWITH_ABI_VERSION_2=ON `
      -DWITH_THREAD_INSTRUMENTATION_PREVIEW=ON `
      -DWITH_METRICS_EXEMPLAR_PREVIEW=ON `
      -DWITH_ASYNC_EXPORT_PREVIEW=ON `
      -DWITH_OTLP_GRPC_SSL_MTLS_PREVIEW=ON `
      -DWITH_OTLP_RETRY_PREVIEW=ON `
      -DWITH_OTLP_GRPC=ON `
      -DWITH_OTLP_HTTP=ON `
      -DWITH_OTLP_FILE=ON `
      -DWITH_OTLP_HTTP_COMPRESSION=ON `
      -DWITH_HTTP_CLIENT_CURL=ON `
      -DWITH_PROMETHEUS=ON `
      -DWITH_ZIPKIN=ON `
      -DWITH_ELASTICSEARCH=ON `
      -DWITH_ETW=ON `
      -DWITH_EXAMPLES=ON `
      -DWITH_EXAMPLES_HTTP=ON `
      -DBUILD_W3CTRACECONTEXT_TEST=ON `
      -DOPENTELEMETRY_INSTALL=ON

    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    cmake --build . --target install
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    $env:PATH = "$INSTALL_TEST_DIR\bin;$env:PATH"

    $CMAKE_OPTIONS_STRING = $CMAKE_OPTIONS -join " "
   
    $EXPECTED_COMPONENTS = @(
      "api",
      "sdk",
      "ext_common",
      "ext_http_curl",
      "exporters_in_memory",
      "exporters_ostream",
      "exporters_otlp_common",
      "exporters_otlp_file",
      "exporters_otlp_grpc",
      "exporters_otlp_http",
      "exporters_prometheus",
      "exporters_elasticsearch",
      "exporters_zipkin",
      "exporters_etw"
    )
    $EXPECTED_COMPONENTS_STRING = $EXPECTED_COMPONENTS -join ";"

    mkdir "$BUILD_DIR\install_test"
    cd "$BUILD_DIR\install_test"

    cmake $CMAKE_OPTIONS `
          "-DCMAKE_PREFIX_PATH=$INSTALL_TEST_DIR" `
          "-DINSTALL_TEST_CMAKE_OPTIONS=$CMAKE_OPTIONS_STRING" `
          "-DINSTALL_TEST_COMPONENTS=$EXPECTED_COMPONENTS_STRING" `
          -S "$SRC_DIR\install\test\cmake"
          
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    ctest -C Debug --output-on-failure
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    exit 0
  }
  "cmake.dll.install.test" {
    cd "$BUILD_DIR"
    Remove-Item -Recurse -Force "$BUILD_DIR\*"
    Remove-Item -Recurse -Force "$INSTALL_TEST_DIR\*"

    $CMAKE_OPTIONS = @(
    "-DCMAKE_CXX_STANDARD=17",
    "-DVCPKG_TARGET_TRIPLET=x64-windows",
    "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake"
    )

    cmake $SRC_DIR `
      $CMAKE_OPTIONS `
      "-DCMAKE_INSTALL_PREFIX=$INSTALL_TEST_DIR" `
      -DOPENTELEMETRY_BUILD_DLL=1 `
      -DWITH_ABI_VERSION_1=ON `
      -DWITH_ABI_VERSION_2=OFF `
      -DWITH_THREAD_INSTRUMENTATION_PREVIEW=ON `
      -DWITH_METRICS_EXEMPLAR_PREVIEW=ON `
      -DWITH_ASYNC_EXPORT_PREVIEW=ON `
      -DWITH_ETW=ON `
      -DOPENTELEMETRY_INSTALL=ON `
      -DWITH_OTLP_GRPC_SSL_MTLS_PREVIEW=OFF `
      -DWITH_OTLP_RETRY_PREVIEW=OFF `
      -DWITH_OTLP_GRPC=OFF `
      -DWITH_OTLP_HTTP=OFF `
      -DWITH_OTLP_FILE=OFF `
      -DWITH_OTLP_HTTP_COMPRESSION=OFF `
      -DWITH_HTTP_CLIENT_CURL=OFF `
      -DWITH_PROMETHEUS=OFF `
      -DWITH_ZIPKIN=OFF `
      -DWITH_ELASTICSEARCH=OFF `
      -DWITH_EXAMPLES=OFF `

    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    cmake --build . -j $nproc
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    ctest -C Debug
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    cmake --build . --target install
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    $env:PATH = "$INSTALL_TEST_DIR\bin;$env:PATH"

    echo "$env:PATH" 

    $CMAKE_OPTIONS_STRING = $CMAKE_OPTIONS -join " "

    $EXPECTED_COMPONENTS = @(
      "api",
      "sdk",
      "ext_common",
      "exporters_in_memory",
      "exporters_ostream",
      "exporters_etw",
      "ext_dll" 
    )
    $EXPECTED_COMPONENTS_STRING = $EXPECTED_COMPONENTS -join ";"

    mkdir "$BUILD_DIR\install_test"
    cd "$BUILD_DIR\install_test"

    cmake $CMAKE_OPTIONS `
          "-DCMAKE_PREFIX_PATH=$INSTALL_TEST_DIR" `
          "-DINSTALL_TEST_CMAKE_OPTIONS=$CMAKE_OPTIONS_STRING" `
          "-DINSTALL_TEST_COMPONENTS=$EXPECTED_COMPONENTS_STRING" `
          -S "$SRC_DIR\install\test\cmake"
          
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    ctest -C Debug --output-on-failure
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
      exit $exit
    }

    exit 0
  }
  default {
    echo "unknown action: $action"
    exit 1
  }
}
