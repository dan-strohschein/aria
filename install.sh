#!/bin/sh
# Aria installer — https://aria-lang.com
# Usage: curl -sSL https://aria-lang.com/install.sh | sh
#
# Installs the Aria compiler, runtime, and standard library.
# Supports: macOS ARM64, Linux x86_64, Windows x86_64 (via WSL)

set -e

REPO="dan-strohschein/aria"
INSTALL_DIR="/usr/local/lib/aria"
BIN_DIR="/usr/local/bin"
BINARY_NAME="aria"

# --- Helpers ---

info() {
    printf '\033[1;34m%s\033[0m\n' "$1"
}

success() {
    printf '\033[1;32m%s\033[0m\n' "$1"
}

error() {
    printf '\033[1;31merror: %s\033[0m\n' "$1" >&2
    exit 1
}

# --- Detect platform ---

detect_platform() {
    OS="$(uname -s)"
    ARCH="$(uname -m)"

    case "$OS" in
        Darwin)  PLATFORM_OS="darwin" ;;
        Linux)   PLATFORM_OS="linux" ;;
        MINGW*|MSYS*|CYGWIN*)
            error "Native Windows is not supported by this installer. Use WSL or download from GitHub Releases."
            ;;
        *)
            error "Unsupported operating system: $OS"
            ;;
    esac

    case "$ARCH" in
        arm64|aarch64) PLATFORM_ARCH="arm64" ;;
        x86_64|amd64)  PLATFORM_ARCH="amd64" ;;
        *)
            error "Unsupported architecture: $ARCH"
            ;;
    esac

    # Check available binaries — only darwin-arm64 and linux-amd64 currently
    case "${PLATFORM_OS}-${PLATFORM_ARCH}" in
        darwin-arm64)  ;;
        linux-amd64)   ;;
        darwin-amd64)
            error "macOS x86_64 binaries are not yet available. Use Rosetta 2 on Apple Silicon or build from source."
            ;;
        linux-arm64)
            error "Linux ARM64 binaries are not yet available. Build from source: https://github.com/$REPO#building"
            ;;
        *)
            error "No prebuilt binary for ${PLATFORM_OS}-${PLATFORM_ARCH}. Build from source: https://github.com/$REPO#building"
            ;;
    esac

    PLATFORM="${PLATFORM_OS}-${PLATFORM_ARCH}"
}

# --- Fetch latest version ---

fetch_latest_version() {
    # Allow override: ARIA_VERSION=v0.1.1-beta curl ... | sh
    if [ -n "${ARIA_VERSION:-}" ]; then
        VERSION="$ARIA_VERSION"
        return
    fi

    if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
        error "curl or wget is required"
    fi

    # Try stable release first, then fall back to pre-release
    for ENDPOINT in "releases/latest" "releases"; do
        if command -v curl >/dev/null 2>&1; then
            VERSION="$(curl -sS "https://api.github.com/repos/$REPO/$ENDPOINT" \
                | grep '"tag_name"' | head -1 | sed 's/.*"tag_name": *"//;s/".*//')"
        else
            VERSION="$(wget -qO- "https://api.github.com/repos/$REPO/$ENDPOINT" \
                | grep '"tag_name"' | head -1 | sed 's/.*"tag_name": *"//;s/".*//')"
        fi
        if [ -n "$VERSION" ]; then return; fi
    done

    if [ -z "$VERSION" ]; then
        error "Could not determine latest version. Set ARIA_VERSION or check https://github.com/$REPO/releases"
    fi
}

# --- Download and install ---

download_and_install() {
    ASSET_NAME="aria-${VERSION}-${PLATFORM}.tar.gz"
    URL="https://github.com/$REPO/releases/download/${VERSION}/${ASSET_NAME}"
    SHA_URL="https://github.com/$REPO/releases/download/${VERSION}/SHA256SUMS.txt"

    TMPDIR="$(mktemp -d)"
    trap 'rm -rf "$TMPDIR"' EXIT

    info "Downloading aria $VERSION for $PLATFORM..."

    if command -v curl >/dev/null 2>&1; then
        curl -sSL --fail -o "$TMPDIR/$ASSET_NAME" "$URL" || error "Download failed. Check that $VERSION has a binary for $PLATFORM."
        curl -sSL --fail -o "$TMPDIR/SHA256SUMS.txt" "$SHA_URL" 2>/dev/null || true
    else
        wget -q -O "$TMPDIR/$ASSET_NAME" "$URL" || error "Download failed. Check that $VERSION has a binary for $PLATFORM."
        wget -q -O "$TMPDIR/SHA256SUMS.txt" "$SHA_URL" 2>/dev/null || true
    fi

    # Verify checksum if available
    if [ -f "$TMPDIR/SHA256SUMS.txt" ]; then
        EXPECTED="$(grep "$ASSET_NAME" "$TMPDIR/SHA256SUMS.txt" | awk '{print $1}')"
        if [ -n "$EXPECTED" ]; then
            if command -v sha256sum >/dev/null 2>&1; then
                ACTUAL="$(sha256sum "$TMPDIR/$ASSET_NAME" | awk '{print $1}')"
            elif command -v shasum >/dev/null 2>&1; then
                ACTUAL="$(shasum -a 256 "$TMPDIR/$ASSET_NAME" | awk '{print $1}')"
            else
                ACTUAL=""
            fi

            if [ -n "$ACTUAL" ]; then
                if [ "$ACTUAL" != "$EXPECTED" ]; then
                    error "Checksum mismatch! Expected $EXPECTED, got $ACTUAL"
                fi
                info "Checksum verified."
            fi
        fi
    fi

    # Extract tarball
    info "Installing to $INSTALL_DIR..."
    tar -xzf "$TMPDIR/$ASSET_NAME" -C "$TMPDIR"

    # Create install directory and copy files
    if [ -w "$(dirname "$INSTALL_DIR")" ]; then
        mkdir -p "$INSTALL_DIR"
        cp "$TMPDIR/aria/bin/aria" "$INSTALL_DIR/" 2>/dev/null || cp "$TMPDIR/aria/aria" "$INSTALL_DIR/" 2>/dev/null || true
        cp -r "$TMPDIR/aria/lib" "$INSTALL_DIR/"
        cp -r "$TMPDIR/aria/runtime" "$INSTALL_DIR/"
        chmod +x "$INSTALL_DIR/aria"
        # Create symlink in PATH
        ln -sf "$INSTALL_DIR/aria" "$BIN_DIR/$BINARY_NAME"
    else
        info "Requires sudo for $INSTALL_DIR..."
        sudo mkdir -p "$INSTALL_DIR"
        sudo cp "$TMPDIR/aria/bin/aria" "$INSTALL_DIR/" 2>/dev/null || sudo cp "$TMPDIR/aria/aria" "$INSTALL_DIR/" 2>/dev/null || true
        sudo cp -r "$TMPDIR/aria/lib" "$INSTALL_DIR/"
        sudo cp -r "$TMPDIR/aria/runtime" "$INSTALL_DIR/"
        sudo chmod +x "$INSTALL_DIR/aria"
        sudo ln -sf "$INSTALL_DIR/aria" "$BIN_DIR/$BINARY_NAME"
    fi
}

# --- Verify installation ---

verify() {
    if command -v aria >/dev/null 2>&1; then
        success "Aria $VERSION installed successfully!"
        echo ""
        echo "  Installed to: $INSTALL_DIR"
        echo "  Standard library: $INSTALL_DIR/lib/"
        echo "  Runtime: $INSTALL_DIR/runtime/"
        echo ""
        echo "  Get started:"
        echo "    aria --version"
        echo "    echo 'entry { println(\"Hello, Aria!\") }' > hello.aria"
        echo "    aria run hello.aria"
        echo ""
        echo "  Docs:   https://aria-lang.com/docs/getting-started"
        echo "  GitHub: https://github.com/$REPO"
        echo ""
    else
        success "Aria installed to $INSTALL_DIR"
        echo ""
        echo "  If 'aria' is not in your PATH, add this to your shell profile:"
        echo "    export PATH=\"$BIN_DIR:\$PATH\""
        echo ""
    fi
}

# --- Main ---

main() {
    info "Aria Installer"
    echo ""
    detect_platform
    fetch_latest_version
    download_and_install
    verify
}

main
