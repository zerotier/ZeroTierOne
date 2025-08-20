#!/bin/bash

# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

set -e

function install_prometheus_cpp_client
{
  pushd third_party/prometheus-cpp
  git submodule update --recursive --init
  [[ -d _build ]] && rm -rf ./_build
  mkdir _build && cd _build
  cmake .. -DBUILD_SHARED_LIBS=ON -DUSE_THIRDPARTY_LIBRARIES=ON
  make -j $(nproc)
  sudo make install
  popd
}

function run_benchmarks
{
  docker run -d --rm -it -p 4317:4317 -p 4318:4318 -v \
    $(pwd)/examples/otlp:/cfg otel/opentelemetry-collector:0.109.0 \
    --config=/cfg/opentelemetry-collector-config/config.dev.yaml

  [ -z "${BENCHMARK_DIR}" ] && export BENCHMARK_DIR=$HOME/benchmark
  mkdir -p $BENCHMARK_DIR
  bazel $BAZEL_STARTUP_OPTIONS build $BAZEL_OPTIONS_ASYNC -c opt -- \
    $(bazel query 'attr("tags", "benchmark_result", ...)')
  echo ""
  echo "Benchmark results in $BENCHMARK_DIR:"
  (
    cd bazel-bin
    find . -name \*_result.json -exec bash -c \
      'echo "$@" && mkdir -p "$BENCHMARK_DIR/$(dirname "$@")" && \
       cp "$@" "$BENCHMARK_DIR/$@" && chmod +w "$BENCHMARK_DIR/$@"' _ {} \;
  )

  # collect benchmark results into one array
  pushd $BENCHMARK_DIR
  components=(api sdk exporters)
  for component in "${components[@]}"
  do
    out=$component-benchmark_result.json
    find ./$component -type f -name "*_result.json" -exec cat {} \; > $component_tmp_bench.json
    # Print each result in CI logs, so it can be inspected.
    echo "BENCHMARK result (begin)"
    cat $component_tmp_bench.json
    echo "BENCHMARK result (end)"
    cat $component_tmp_bench.json | docker run -i --rm itchyny/gojq:0.12.6 -s \
      '.[0].benchmarks = ([.[].benchmarks] | add) |
      if .[0].benchmarks == null then null else .[0] end' > $BENCHMARK_DIR/$out
  done

  mv *benchmark_result.json ${SRC_DIR}
  popd
  docker kill $(docker ps -q)
}

[ -z "${SRC_DIR}" ] && export SRC_DIR="`pwd`"
[ -z "${BUILD_DIR}" ] && export BUILD_DIR=$HOME/build
mkdir -p "${BUILD_DIR}"
[ -z "${PLUGIN_DIR}" ] && export PLUGIN_DIR=$HOME/plugin
mkdir -p "${PLUGIN_DIR}"
[ -z "${INSTALL_TEST_DIR}" ] && export INSTALL_TEST_DIR=$HOME/install_test
mkdir -p "${INSTALL_TEST_DIR}"

MAKE_COMMAND="make -k -j \$(nproc)"

echo "make command: ${MAKE_COMMAND}"

BAZEL_OPTIONS_DEFAULT="--copt=-DENABLE_METRICS_EXEMPLAR_PREVIEW"
BAZEL_OPTIONS="$BAZEL_OPTIONS_DEFAULT"

BAZEL_TEST_OPTIONS="$BAZEL_OPTIONS --test_output=errors"

BAZEL_OPTIONS_ASYNC="$BAZEL_OPTIONS --copt=-DENABLE_ASYNC_EXPORT"
BAZEL_TEST_OPTIONS_ASYNC="$BAZEL_OPTIONS_ASYNC --test_output=errors"

# https://github.com/bazelbuild/bazel/issues/4341
BAZEL_MACOS_OPTIONS="$BAZEL_OPTIONS_ASYNC --features=-supports_dynamic_linker"
BAZEL_MACOS_TEST_OPTIONS="$BAZEL_MACOS_OPTIONS --test_output=errors"

BAZEL_STARTUP_OPTIONS="--output_user_root=$HOME/.cache/bazel"

if [[ "${BUILD_TYPE}" =~ ^(Debug|Release|RelWithDebInfo|MinSizeRel)$ ]]; then
  CMAKE_OPTIONS=(-DCMAKE_BUILD_TYPE=${BUILD_TYPE})
else
  CMAKE_OPTIONS=(-DCMAKE_BUILD_TYPE=Debug)
fi

if [ -n "${CXX_STANDARD}" ]; then
  CMAKE_OPTIONS+=("-DCMAKE_CXX_STANDARD=${CXX_STANDARD}")
else
  CMAKE_OPTIONS+=("-DCMAKE_CXX_STANDARD=14")
fi

CMAKE_OPTIONS+=("-DCMAKE_CXX_STANDARD_REQUIRED=ON")
CMAKE_OPTIONS+=("-DCMAKE_CXX_EXTENSIONS=OFF")

if [ -n "$CMAKE_TOOLCHAIN_FILE" ]; then
  echo "CMAKE_TOOLCHAIN_FILE is set to: $CMAKE_TOOLCHAIN_FILE"
  CMAKE_OPTIONS+=("-DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE")
fi

echo "CMAKE_OPTIONS:" "${CMAKE_OPTIONS[@]}"

export CTEST_OUTPUT_ON_FAILURE=1

if [[ "$1" == "cmake.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_PROMETHEUS=ON \
        -DWITH_ZIPKIN=ON \
        -DWITH_ELASTICSEARCH=ON \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        "${SRC_DIR}"
  make -j $(nproc)
  make test
  exit 0
elif [[ "$1" == "cmake.maintainer.sync.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_PROMETHEUS=ON \
        -DWITH_EXAMPLES=ON \
        -DWITH_EXAMPLES_HTTP=ON \
        -DWITH_ZIPKIN=ON \
        -DBUILD_W3CTRACECONTEXT_TEST=ON \
        -DWITH_ELASTICSEARCH=ON \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DWITH_ASYNC_EXPORT_PREVIEW=OFF \
        -DOTELCPP_MAINTAINER_MODE=ON \
        -DWITH_NO_DEPRECATED_CODE=ON \
        -DWITH_OTLP_HTTP_COMPRESSION=ON \
        -DWITH_OTLP_RETRY_PREVIEW=ON \
        -DWITH_THREAD_INSTRUMENTATION_PREVIEW=ON \
        "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  make test
  exit 0
elif [[ "$1" == "cmake.maintainer.async.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_PROMETHEUS=ON \
        -DWITH_EXAMPLES=ON \
        -DWITH_EXAMPLES_HTTP=ON \
        -DWITH_ZIPKIN=ON \
        -DBUILD_W3CTRACECONTEXT_TEST=ON \
        -DWITH_ELASTICSEARCH=ON \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DOTELCPP_MAINTAINER_MODE=ON \
        -DWITH_NO_DEPRECATED_CODE=ON \
        -DWITH_OTLP_HTTP_COMPRESSION=ON \
        -DWITH_OTLP_RETRY_PREVIEW=ON \
        -DWITH_THREAD_INSTRUMENTATION_PREVIEW=ON \
        "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  make test
  exit 0
elif [[ "$1" == "cmake.maintainer.cpp11.async.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_CXX_STANDARD=11 \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_PROMETHEUS=ON \
        -DWITH_EXAMPLES=ON \
        -DWITH_EXAMPLES_HTTP=ON \
        -DWITH_ZIPKIN=ON \
        -DBUILD_W3CTRACECONTEXT_TEST=ON \
        -DWITH_ELASTICSEARCH=ON \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DOTELCPP_MAINTAINER_MODE=ON \
        -DWITH_NO_DEPRECATED_CODE=ON \
        -DWITH_OTLP_HTTP_COMPRESSION=ON \
        -DWITH_OTLP_RETRY_PREVIEW=ON \
        -DWITH_THREAD_INSTRUMENTATION_PREVIEW=ON \
        "${SRC_DIR}"
  make -k -j $(nproc)
  make test
  exit 0
elif [[ "$1" == "cmake.maintainer.abiv2.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_PROMETHEUS=ON \
        -DWITH_EXAMPLES=ON \
        -DWITH_EXAMPLES_HTTP=ON \
        -DWITH_ZIPKIN=ON \
        -DBUILD_W3CTRACECONTEXT_TEST=ON \
        -DWITH_ELASTICSEARCH=ON \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DWITH_ASYNC_EXPORT_PREVIEW=OFF \
        -DOTELCPP_MAINTAINER_MODE=ON \
        -DWITH_NO_DEPRECATED_CODE=ON \
        -DWITH_ABI_VERSION_1=OFF \
        -DWITH_ABI_VERSION_2=ON \
        -DWITH_OTLP_HTTP_COMPRESSION=ON \
        -DWITH_OTLP_RETRY_PREVIEW=ON \
        -DWITH_THREAD_INSTRUMENTATION_PREVIEW=ON \
        "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  make test
  exit 0
elif [[ "$1" == "cmake.with_async_export.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_PROMETHEUS=ON \
        -DWITH_ZIPKIN=ON \
        -DWITH_ELASTICSEARCH=ON \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        "${SRC_DIR}"
  make -j $(nproc)
  make test
  exit 0
elif [[ "$1" == "cmake.opentracing_shim.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}" \
        -DCMAKE_CXX_FLAGS="-Werror -Wno-error=redundant-move $CXXFLAGS" \
        -DWITH_OPENTRACING=ON \
        "${SRC_DIR}"
  make -j $(nproc)
  make test
  exit 0
elif [[ "$1" == "cmake.opentracing_shim.install.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  rm -rf ${INSTALL_TEST_DIR}/*
  cmake "${CMAKE_OPTIONS[@]}" \
        -DCMAKE_CXX_FLAGS="-Werror -Wno-error=redundant-move $CXXFLAGS" \
        -DWITH_OPENTRACING=ON \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_TEST_DIR} \
        "${SRC_DIR}"
  make -j $(nproc)
  make test
  make install
  export LD_LIBRARY_PATH="${INSTALL_TEST_DIR}/lib:$LD_LIBRARY_PATH"
  CMAKE_OPTIONS_STRING=$(IFS=" "; echo "${CMAKE_OPTIONS[*]}")
  EXPECTED_COMPONENTS=(
    "api"
    "sdk"
    "ext_common"
    "exporters_in_memory"
    "exporters_ostream"
    "shims_opentracing"
  )
  EXPECTED_COMPONENTS_STRING=$(IFS=\;; echo "${EXPECTED_COMPONENTS[*]}")
  mkdir -p "${BUILD_DIR}/install_test"
  cd "${BUILD_DIR}/install_test"
  cmake "${CMAKE_OPTIONS[@]}" \
        "-DCMAKE_PREFIX_PATH=${INSTALL_TEST_DIR}" \
        "-DINSTALL_TEST_CMAKE_OPTIONS=${CMAKE_OPTIONS_STRING}" \
        "-DINSTALL_TEST_COMPONENTS=${EXPECTED_COMPONENTS_STRING}" \
        -S "${SRC_DIR}/install/test/cmake"
  ctest --output-on-failure
  exit 0
elif [[ "$1" == "cmake.c++20.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DWITH_STL=CXX20 \
        "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  make test
  exit 0
elif [[ "$1" == "cmake.c++23.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DWITH_STL=CXX23 \
        "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  make test
  exit 0
elif [[ "$1" == "cmake.c++14.stl.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DWITH_STL=CXX14 \
        "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  make test
  exit 0
elif [[ "$1" == "cmake.c++17.stl.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DWITH_STL=CXX17 \
        "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  make test
  exit 0
elif [[ "$1" == "cmake.c++20.stl.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DWITH_STL=CXX20 \
        "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  make test
  exit 0
elif [[ "$1" == "cmake.c++23.stl.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DWITH_STL=CXX23 \
        "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  make test
  exit 0
elif [[ "$1" == "cmake.legacy.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  export BUILD_ROOT="${BUILD_DIR}"
  ${SRC_DIR}/tools/build-benchmark.sh
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        "${SRC_DIR}"
  make -j $(nproc)
  make test
  exit 0
elif [[ "$1" == "cmake.legacy.exporter.otprotocol.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  export BUILD_ROOT="${BUILD_DIR}"
  ${SRC_DIR}/tools/build-benchmark.sh
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_CXX_STANDARD=11 \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        "${SRC_DIR}"
  grpc_cpp_plugin=`which grpc_cpp_plugin`
  proto_make_file="CMakeFiles/opentelemetry_proto.dir/build.make"
  sed -i "s~gRPC_CPP_PLUGIN_EXECUTABLE-NOTFOUND~$grpc_cpp_plugin~" ${proto_make_file} #fixme
  make -j $(nproc)
  cd exporters/otlp && make test
  exit 0
elif [[ "$1" == "cmake.exporter.otprotocol.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_OTLP_GRPC_SSL_MTLS_PREVIEW=ON \
        -DWITH_OTLP_RETRY_PREVIEW=ON \
        "${SRC_DIR}"
  grpc_cpp_plugin=`which grpc_cpp_plugin`
  proto_make_file="CMakeFiles/opentelemetry_proto.dir/build.make"
  sed -i "s~gRPC_CPP_PLUGIN_EXECUTABLE-NOTFOUND~$grpc_cpp_plugin~" ${proto_make_file} #fixme
  make -j $(nproc)
  cd exporters/otlp && make test
  exit 0
elif [[ "$1" == "cmake.exporter.otprotocol.shared_libs.with_static_grpc.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_FILE=ON \
        -DBUILD_SHARED_LIBS=ON \
        "${SRC_DIR}"
  grpc_cpp_plugin=`which grpc_cpp_plugin`
  proto_make_file="CMakeFiles/opentelemetry_proto.dir/build.make"
  sed -i "s~gRPC_CPP_PLUGIN_EXECUTABLE-NOTFOUND~$grpc_cpp_plugin~" ${proto_make_file} #fixme
  make -j $(nproc)
  cd exporters/otlp && make test
  exit 0
elif [[ "$1" == "cmake.exporter.otprotocol.with_async_export.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        "${SRC_DIR}"
  grpc_cpp_plugin=`which grpc_cpp_plugin`
  proto_make_file="CMakeFiles/opentelemetry_proto.dir/build.make"
  sed -i "s~gRPC_CPP_PLUGIN_EXECUTABLE-NOTFOUND~$grpc_cpp_plugin~" ${proto_make_file} #fixme
  make -j $(nproc)
  cd exporters/otlp && make test
  exit 0
elif [[ "$1" == "cmake.w3c.trace-context.build-server" ]]; then
  echo "Building w3c trace context test server"
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
          -DBUILD_W3CTRACECONTEXT_TEST=ON \
          -DCMAKE_CXX_STANDARD=${CXX_STANDARD} \
          "${SRC_DIR}"
  eval "$MAKE_COMMAND"
  exit 0
elif [[ "$1" == "cmake.do_not_install.test" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DOPENTELEMETRY_INSTALL=OFF \
        "${SRC_DIR}"
  grpc_cpp_plugin=`which grpc_cpp_plugin`
  proto_make_file="CMakeFiles/opentelemetry_proto.dir/build.make"
  sed -i "s~gRPC_CPP_PLUGIN_EXECUTABLE-NOTFOUND~$grpc_cpp_plugin~" ${proto_make_file} #fixme
  make -j $(nproc)
  cd exporters/otlp && make test
  exit 0
elif [[ "$1" == "cmake.install.test" ]]; then
  if [[ -n "${BUILD_SHARED_LIBS}" && "${BUILD_SHARED_LIBS}" == "ON" ]]; then
    CMAKE_OPTIONS+=("-DBUILD_SHARED_LIBS=ON")
    echo "BUILD_SHARED_LIBS is set to: ON"
  else
    CMAKE_OPTIONS+=("-DBUILD_SHARED_LIBS=OFF")
    echo "BUILD_SHARED_LIBS is set to: OFF"
  fi
  CMAKE_OPTIONS+=("-DCMAKE_POSITION_INDEPENDENT_CODE=ON")

  cd "${BUILD_DIR}"
  rm -rf *
  rm -rf ${INSTALL_TEST_DIR}/*

  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_TEST_DIR} \
        -DWITH_ABI_VERSION_1=OFF \
        -DWITH_ABI_VERSION_2=ON \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DWITH_THREAD_INSTRUMENTATION_PREVIEW=ON \
        -DWITH_OTLP_GRPC_SSL_MTLS_PREVIEW=ON \
        -DWITH_OTLP_RETRY_PREVIEW=ON \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_OTLP_HTTP_COMPRESSION=ON \
        -DWITH_HTTP_CLIENT_CURL=ON \
        -DWITH_PROMETHEUS=ON \
        -DWITH_ZIPKIN=ON \
        -DWITH_ELASTICSEARCH=ON \
        -DWITH_EXAMPLES=ON \
        -DWITH_EXAMPLES_HTTP=ON \
        -DBUILD_W3CTRACECONTEXT_TEST=ON \
        -DOPENTELEMETRY_INSTALL=ON \
        "${SRC_DIR}"

  make -j $(nproc)
  make test
  make install
  export LD_LIBRARY_PATH="${INSTALL_TEST_DIR}/lib:$LD_LIBRARY_PATH"

  CMAKE_OPTIONS_STRING=$(IFS=" "; echo "${CMAKE_OPTIONS[*]}")

  EXPECTED_COMPONENTS=(
    "api"
    "sdk"
    "ext_common"
    "ext_http_curl"
    "exporters_in_memory"
    "exporters_ostream"
    "exporters_otlp_common"
    "exporters_otlp_file"
    "exporters_otlp_grpc"
    "exporters_otlp_http"
    "exporters_prometheus"
    "exporters_elasticsearch"
    "exporters_zipkin"
  )
  EXPECTED_COMPONENTS_STRING=$(IFS=\;; echo "${EXPECTED_COMPONENTS[*]}")
  mkdir -p "${BUILD_DIR}/install_test"
  cd "${BUILD_DIR}/install_test"
  cmake  "${CMAKE_OPTIONS[@]}" \
         "-DCMAKE_PREFIX_PATH=${INSTALL_TEST_DIR}" \
         "-DINSTALL_TEST_CMAKE_OPTIONS=${CMAKE_OPTIONS_STRING}" \
         "-DINSTALL_TEST_COMPONENTS=${EXPECTED_COMPONENTS_STRING}" \
         -S "${SRC_DIR}/install/test/cmake"
  ctest --output-on-failure
  exit 0
elif [[ "$1" == "cmake.fetch_content.test" ]]; then
  if [[ -n "${BUILD_SHARED_LIBS}" && "${BUILD_SHARED_LIBS}" == "ON" ]]; then
    CMAKE_OPTIONS+=("-DBUILD_SHARED_LIBS=ON")
    echo "BUILD_SHARED_LIBS is set to: ON"
  else
    CMAKE_OPTIONS+=("-DBUILD_SHARED_LIBS=OFF")
    echo "BUILD_SHARED_LIBS is set to: OFF"
  fi
  CMAKE_OPTIONS+=("-DCMAKE_POSITION_INDEPENDENT_CODE=ON")

  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_INSTALL_PREFIX=${INSTALL_TEST_DIR} \
        -DWITH_ABI_VERSION_1=OFF \
        -DWITH_ABI_VERSION_2=ON \
        -DWITH_METRICS_EXEMPLAR_PREVIEW=ON \
        -DWITH_ASYNC_EXPORT_PREVIEW=ON \
        -DWITH_THREAD_INSTRUMENTATION_PREVIEW=ON \
        -DWITH_OTLP_GRPC_SSL_MTLS_PREVIEW=ON \
        -DWITH_OTLP_RETRY_PREVIEW=ON \
        -DWITH_OTLP_GRPC=ON \
        -DWITH_OTLP_HTTP=ON \
        -DWITH_OTLP_FILE=ON \
        -DWITH_OTLP_HTTP_COMPRESSION=ON \
        -DWITH_HTTP_CLIENT_CURL=ON \
        -DWITH_PROMETHEUS=ON \
        -DWITH_ZIPKIN=ON \
        -DWITH_ELASTICSEARCH=ON \
        -DWITH_EXAMPLES=ON \
        -DWITH_EXAMPLES_HTTP=ON \
        -DBUILD_W3CTRACECONTEXT_TEST=ON \
        -DOPENTELEMETRY_INSTALL=OFF \
        -DOPENTELEMETRY_CPP_SRC_DIR="${SRC_DIR}" \
        "${SRC_DIR}/install/test/cmake/fetch_content_test"
  make -j $(nproc)
  make test
  exit 0

elif [[ "$1" == "cmake.test_example_plugin" ]]; then
  # Build the plugin
  cd "${BUILD_DIR}"
  rm -rf *
  cat <<EOF > export.map
{
  global:
    OpenTelemetryMakeFactoryImpl;
  local: *;
};
EOF

  LINKER_FLAGS="\
    -static-libstdc++ \
    -static-libgcc \
    -Wl,--version-script=${PWD}/export.map \
  "
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        -DCMAKE_EXE_LINKER_FLAGS="$LINKER_FLAGS" \
        -DCMAKE_SHARED_LINKER_FLAGS="$LINKER_FLAGS" \
        "${SRC_DIR}"
  make example_plugin
  cp examples/plugin/plugin/libexample_plugin.so ${PLUGIN_DIR}

  # Verify we can load the plugin
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_CXX_FLAGS="-Werror $CXXFLAGS" \
        "${SRC_DIR}"
  make load_plugin_example
  examples/plugin/load/load_plugin_example ${PLUGIN_DIR}/libexample_plugin.so /dev/null
  exit 0
elif [[ "$1" == "bazel.no_bzlmod.test" ]]; then
  bazel $BAZEL_STARTUP_OPTIONS build --enable_bzlmod=false $BAZEL_OPTIONS //...
  bazel $BAZEL_STARTUP_OPTIONS test --enable_bzlmod=false $BAZEL_TEST_OPTIONS //...
  exit 0
elif [[ "$1" == "bazel.test" ]]; then
  bazel $BAZEL_STARTUP_OPTIONS build $BAZEL_OPTIONS //...
  bazel $BAZEL_STARTUP_OPTIONS test $BAZEL_TEST_OPTIONS //...
  exit 0
elif [[ "$1" == "bazel.with_async_export.test" ]]; then
  bazel $BAZEL_STARTUP_OPTIONS build $BAZEL_OPTIONS_ASYNC //...
  bazel $BAZEL_STARTUP_OPTIONS test $BAZEL_TEST_OPTIONS_ASYNC //...
  exit 0
elif [[ "$1" == "bazel.benchmark" ]]; then
  run_benchmarks
  exit 0
elif [[ "$1" == "bazel.macos.test" ]]; then
  bazel $BAZEL_STARTUP_OPTIONS build $BAZEL_MACOS_OPTIONS -- //...
  bazel $BAZEL_STARTUP_OPTIONS test $BAZEL_MACOS_TEST_OPTIONS -- //...
  exit 0
elif [[ "$1" == "bazel.legacy.test" ]]; then
  # we uses C++ future and async() function to test the Prometheus Exporter functionality,
  # that make this test always fail. ignore Prometheus exporter here.
  bazel $BAZEL_STARTUP_OPTIONS build $BAZEL_OPTIONS_ASYNC -- //... -//exporters/otlp/... -//exporters/prometheus/...
  bazel $BAZEL_STARTUP_OPTIONS test $BAZEL_TEST_OPTIONS_ASYNC -- //... -//exporters/otlp/... -//exporters/prometheus/...
  exit 0
elif [[ "$1" == "bazel.noexcept" ]]; then
  # there are some exceptions and error handling code from the Prometheus Client
  # as well as Opentracing shim (due to some third party code in its Opentracing dependency)
  # that make this test always fail. Ignore these packages in the noexcept test here.
  bazel $BAZEL_STARTUP_OPTIONS build --copt=-fno-exceptions $BAZEL_OPTIONS_ASYNC -- //... -//exporters/prometheus/... -//examples/prometheus/... -//opentracing-shim/...
  bazel $BAZEL_STARTUP_OPTIONS test --copt=-fno-exceptions $BAZEL_TEST_OPTIONS_ASYNC -- //... -//exporters/prometheus/... -//examples/prometheus/... -//opentracing-shim/...
  exit 0
elif [[ "$1" == "bazel.nortti" ]]; then
  # there are some exceptions and error handling code from the Prometheus Client
  # that make this test always fail. Ignore these packages in the nortti test here.
  bazel $BAZEL_STARTUP_OPTIONS build --cxxopt=-fno-rtti $BAZEL_OPTIONS_ASYNC -- //... -//exporters/prometheus/...
  bazel $BAZEL_STARTUP_OPTIONS test --cxxopt=-fno-rtti $BAZEL_TEST_OPTIONS_ASYNC -- //... -//exporters/prometheus/...
  exit 0
elif [[ "$1" == "bazel.asan" ]]; then
  bazel $BAZEL_STARTUP_OPTIONS test --config=asan $BAZEL_TEST_OPTIONS_ASYNC //...
  exit 0
elif [[ "$1" == "bazel.tsan" ]]; then
# TODO - potential race condition in Civetweb server used by prometheus-cpp during shutdown
# https://github.com/civetweb/civetweb/issues/861, so removing prometheus from the test
  bazel $BAZEL_STARTUP_OPTIONS test --config=tsan $BAZEL_TEST_OPTIONS_ASYNC  -- //... -//exporters/prometheus/...
  exit 0
elif [[ "$1" == "bazel.valgrind" ]]; then
  bazel $BAZEL_STARTUP_OPTIONS build $BAZEL_OPTIONS_ASYNC //...
  bazel $BAZEL_STARTUP_OPTIONS test --run_under="/usr/bin/valgrind --leak-check=full --error-exitcode=1 --errors-for-leak-kinds=definite --suppressions=\"${SRC_DIR}/ci/valgrind-suppressions\"" $BAZEL_TEST_OPTIONS_ASYNC //...
  exit 0
elif [[ "$1" == "bazel.e2e" ]]; then
  cd examples/e2e
  bazel $BAZEL_STARTUP_OPTIONS build $BAZEL_OPTIONS_DEFAULT //...
  exit 0
elif [[ "$1" == "benchmark" ]]; then
  [ -z "${BENCHMARK_DIR}" ] && export BENCHMARK_DIR=$HOME/benchmark
  bazel $BAZEL_STARTUP_OPTIONS build $BAZEL_OPTIONS_ASYNC -c opt -- \
    $(bazel query 'attr("tags", "benchmark_result", ...)')
  echo ""
  echo "Benchmark results in $BENCHMARK_DIR:"
  (
    cd bazel-bin
    find . -name \*_result.json -exec bash -c \
      'echo "$@" && mkdir -p "$BENCHMARK_DIR/$(dirname "$@")" && \
       cp "$@" "$BENCHMARK_DIR/$@" && chmod +w "$BENCHMARK_DIR/$@"' _ {} \;
  )
  exit 0
elif [[ "$1" == "format" ]]; then
  tools/format.sh
  CHANGED="$(git ls-files --modified)"
  if [[ ! -z "$CHANGED" ]]; then
    echo "The following files have changes:"
    echo "$CHANGED"
    git diff
    exit 1
  fi
  exit 0
elif [[ "$1" == "code.coverage" ]]; then
  cd "${BUILD_DIR}"
  rm -rf *
  cmake "${CMAKE_OPTIONS[@]}"  \
        -DCMAKE_CXX_FLAGS="-Werror --coverage $CXXFLAGS" \
        "${SRC_DIR}"
  make
  make test
  lcov --directory $PWD --capture --output-file coverage.info
  # removing test http server coverage from the total coverage. We don't use this server completely.
  lcov --remove coverage.info '*/ext/http/server/*'> tmp_coverage.info 2>/dev/null
  cp tmp_coverage.info coverage.info
  exit 0
elif [[ "$1" == "third_party.tags" ]]; then
  echo "gRPC=v1.49.2" > third_party_release
  echo "abseil=20240116.1" >> third_party_release
  git submodule foreach --quiet 'echo "$name=$(git describe --tags HEAD)"' | sed 's:.*/::' >> third_party_release
  exit 0
fi

echo "Invalid do_ci.sh target, see ci/README.md for valid targets."
exit 1
