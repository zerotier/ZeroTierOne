#!/usr/bin/env bash
#
# Build & install the pinned, version-sensitive OpenTelemetry / google-cloud-cpp
# dependencies (both fetched from upstream at pinned tags) into a prefix that the
# CMake build then finds via -DCMAKE_PREFIX_PATH. Replaces conda's role of pinning these to
# exact versions fleet-wide; everything else comes from the native package manager
# (Brewfile on macOS; the apt list in the controller Dockerfile on Linux).
#
# Both the controller and the daemon resolve opentelemetry-cpp from this prefix --
# the only difference is how much gets built (controlled by ZT_CONTROLLER_DEPS):
#
#   default:              opentelemetry-cpp 1.27.0 *API only* (header-only, quick) and
#                         NO google-cloud-cpp. For the daemon / most dev builds, which
#                         only need the OTel API. This is the common case.
#   ZT_CONTROLLER_DEPS=1: opentelemetry-cpp 1.27.0 (SDK + OTLP exporters, built against
#                         the native abseil/protobuf/grpc) + google-cloud-cpp 2.38.0.
#                         Only the hosted controller (-DZT1_CENTRAL_CONTROLLER=1) needs this.
#
# Usage:
#   scripts/bootstrap-deps.sh [PREFIX]                     # daemon / default: header-only OTel API (default PREFIX: <repo>/.deps)
#   ZT_CONTROLLER_DEPS=1 scripts/bootstrap-deps.sh [PREFIX] # hosted controller: full OTel + google-cloud-cpp
#
# Idempotent: a dependency is skipped if its CMake config already exists in PREFIX.
# Delete PREFIX to force a rebuild or to switch between api-only and full.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${1:-$REPO_ROOT/.deps}"
BUILD_ROOT="$REPO_ROOT/.deps-build"
OTEL_VERSION="v1.27.0"
GCLOUD_VERSION="v2.38.0"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
CONTROLLER_DEPS="${ZT_CONTROLLER_DEPS:-0}"   # 1 = full OTel SDK + OTLP exporters + google-cloud-cpp (hosted controller); default = header-only OTel API (daemon)

# Let CMake find Homebrew-provided deps (abseil/protobuf/grpc/...) on macOS.
PREFIX_PATH="$PREFIX"
if command -v brew >/dev/null 2>&1; then
    PREFIX_PATH="$PREFIX;$(brew --prefix)"
    # libpq is keg-only on macOS; add it so the controller's find_package(PostgreSQL) resolves.
    _libpq="$(brew --prefix libpq 2>/dev/null || true)"
    [ -n "$_libpq" ] && PREFIX_PATH="$PREFIX_PATH;$_libpq"
fi

mkdir -p "$PREFIX" "$BUILD_ROOT"
echo "==> Bootstrapping pinned deps into: $PREFIX"
echo "==> CMAKE_PREFIX_PATH: $PREFIX_PATH"

# --- opentelemetry-cpp (fetched) ------------------------------------------
# Cloned (like google-cloud-cpp), not vendored. A plain clone is enough: OTel
# auto-downloads opentelemetry-proto when building the OTLP exporters, so no
# submodules are needed.
OTEL_SRC="$BUILD_ROOT/opentelemetry-cpp-src"
if [ -d "$PREFIX/lib/cmake/opentelemetry-cpp" ] || [ -d "$PREFIX/lib64/cmake/opentelemetry-cpp" ]; then
    echo "==> opentelemetry-cpp already installed in PREFIX, skipping"
    echo "    (delete $PREFIX to switch between api-only and full)"
else
    # Re-clone if the existing checkout isn't the pinned tag (handles a version bump).
    if [ -d "$OTEL_SRC/.git" ] && [ "$(git -C "$OTEL_SRC" describe --tags --exact-match 2>/dev/null)" != "$OTEL_VERSION" ]; then
        echo "==> opentelemetry-cpp clone is not $OTEL_VERSION; re-fetching"
        rm -rf "$OTEL_SRC"
    fi
    if [ ! -d "$OTEL_SRC/.git" ]; then
        echo "==> Fetching opentelemetry-cpp $OTEL_VERSION"
        git clone --depth 1 --branch "$OTEL_VERSION" \
            https://github.com/open-telemetry/opentelemetry-cpp.git "$OTEL_SRC"
    fi
    if [ "$CONTROLLER_DEPS" = "1" ]; then
        # Full build: SDK + OTLP gRPC/HTTP exporters, against the native
        # abseil/protobuf/grpc (USE_THIRDPARTY_LIBRARIES=OFF) so it stays
        # ABI-coherent with google-cloud-cpp. Prometheus exporter OFF (unused).
        echo "==> Building opentelemetry-cpp (full: SDK + OTLP exporters)"
        cmake -S "$OTEL_SRC" -B "$BUILD_ROOT/otel" -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="$PREFIX" \
            -DCMAKE_PREFIX_PATH="$PREFIX_PATH" \
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
            -DBUILD_TESTING=OFF \
            -DOPENTELEMETRY_INSTALL=ON \
            -DWITH_BENCHMARK=OFF -DWITH_EXAMPLES=OFF -DWITH_FUNC_TESTS=OFF \
            -DUSE_THIRDPARTY_LIBRARIES=OFF \
            -DWITH_ABSEIL=ON \
            -DWITH_API_ONLY=OFF \
            -DWITH_OTLP_GRPC=ON -DWITH_OTLP_HTTP=ON -DWITH_PROMETHEUS=OFF
    else
        # Header-only API only (default): quick, no SDK/exporters, no abseil/protobuf/grpc.
        echo "==> Building opentelemetry-cpp (API only)"
        cmake -S "$OTEL_SRC" -B "$BUILD_ROOT/otel" -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX="$PREFIX" \
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
            -DBUILD_TESTING=OFF \
            -DOPENTELEMETRY_INSTALL=ON \
            -DWITH_API_ONLY=ON
    fi
    cmake --build "$BUILD_ROOT/otel" -j"$JOBS"
    cmake --install "$BUILD_ROOT/otel"
fi

# --- google-cloud-cpp 2.38.0 (fetched) ------------------------------------
if [ "$CONTROLLER_DEPS" != "1" ]; then
    echo "==> Default (API-only) mode: skipping google-cloud-cpp (only the controller needs it; set ZT_CONTROLLER_DEPS=1 to build it)"
elif [ -d "$PREFIX/lib/cmake/google_cloud_cpp_pubsub" ] || [ -d "$PREFIX/lib64/cmake/google_cloud_cpp_pubsub" ]; then
    echo "==> google-cloud-cpp already installed in PREFIX, skipping"
else
    echo "==> Fetching & building google-cloud-cpp $GCLOUD_VERSION (bigtable, pubsub)"
    GCLOUD_SRC="$BUILD_ROOT/google-cloud-cpp-src"
    # Re-clone if the existing checkout isn't the pinned tag (handles a version bump).
    if [ -d "$GCLOUD_SRC/.git" ] && [ "$(git -C "$GCLOUD_SRC" describe --tags --exact-match 2>/dev/null)" != "$GCLOUD_VERSION" ]; then
        echo "==> google-cloud-cpp clone is not $GCLOUD_VERSION; re-fetching"
        rm -rf "$GCLOUD_SRC"
    fi
    if [ ! -d "$GCLOUD_SRC/.git" ]; then
        git clone --depth 1 --branch "$GCLOUD_VERSION" \
            https://github.com/googleapis/google-cloud-cpp.git "$GCLOUD_SRC"
    fi
    cmake -S "$GCLOUD_SRC" -B "$BUILD_ROOT/google-cloud-cpp" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_PREFIX_PATH="$PREFIX_PATH" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DBUILD_TESTING=OFF \
        -DGOOGLE_CLOUD_CPP_ENABLE=bigtable,pubsub
    cmake --build "$BUILD_ROOT/google-cloud-cpp" -j"$JOBS"
    cmake --install "$BUILD_ROOT/google-cloud-cpp"
fi

echo ""
if [ "$CONTROLLER_DEPS" = "1" ]; then
    echo "==> Done. Build the controller with:"
    echo "    cmake -DCMAKE_BUILD_TYPE=Release -DZT1_CENTRAL_CONTROLLER=1 \\"
    echo "          -DCMAKE_PREFIX_PATH=\"$PREFIX_PATH\" -S . -B build"
else
    echo "==> Done (API-only). Build the daemon with:"
    echo "    cmake -DCMAKE_BUILD_TYPE=Release \\"
    echo "          -DCMAKE_PREFIX_PATH=\"$PREFIX_PATH\" -S . -B build"
fi
echo "    cmake --build build -j$JOBS"
