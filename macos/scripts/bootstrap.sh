#!/bin/bash
# Install only under this checkout; no Homebrew, sudo, or system modifications.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
[[ "$(uname -m)" == arm64 ]] || { echo 'This build targets Apple Silicon.' >&2; exit 1; }
xcrun --sdk macosx --show-sdk-version
mkdir -p work/tooling work/deps
[[ -x work/tooling/venv/bin/python3 ]] || python3 -m venv work/tooling/venv
work/tooling/venv/bin/pip install cmake==3.31.6 ninja==1.11.1.3
ARCHIVE=work/deps/openssl-3.5.4.tar.gz
[[ -f "$ARCHIVE" ]] || curl -fL --retry 2 https://www.openssl.org/source/openssl-3.5.4.tar.gz -o "$ARCHIVE"
python3 - <<'PY'
from pathlib import Path
import hashlib, tarfile
p=Path('work/deps/openssl-3.5.4.tar.gz')
assert hashlib.sha256(p.read_bytes()).hexdigest()=='967311f84955316969bdb1d8d4b983718ef42338639c621ec4c34fddef355e99', 'OpenSSL checksum mismatch'
if not Path('work/deps/openssl-3.5.4/Configure').exists():
    with tarfile.open(p) as archive: archive.extractall('work/deps')
PY
cd work/deps/openssl-3.5.4
./Configure darwin64-arm64-cc no-shared --prefix="$ROOT/work/deps/openssl"
make -j "${BUILD_JOBS:-8}"
make install_sw

cd "$ROOT"
ARCHIVE=work/deps/openh264-2.6.0.tar.gz
[[ -f "$ARCHIVE" ]] || curl -fL --retry 2 https://codeload.github.com/cisco/openh264/tar.gz/refs/tags/v2.6.0 -o "$ARCHIVE"
python3 - <<'PYCODE'
from pathlib import Path
import hashlib, tarfile
p=Path('work/deps/openh264-2.6.0.tar.gz')
assert hashlib.sha256(p.read_bytes()).hexdigest()=='558544ad358283a7ab2930d69a9ceddf913f4a51ee9bf1bfb9e377322af81a69', 'OpenH264 checksum mismatch'
if not Path('work/deps/openh264-2.6.0/Makefile').exists():
    with tarfile.open(p) as archive: archive.extractall('work/deps')
PYCODE
make -C work/deps/openh264-2.6.0 -j "${BUILD_JOBS:-8}" OS=darwin ARCH=arm64 PREFIX="$ROOT/work/deps/openh264" install-static
