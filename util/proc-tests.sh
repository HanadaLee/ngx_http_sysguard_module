#!/usr/bin/env bash
set -euo pipefail

# Run only in a new mount namespace; never replace the host's proc files.
if [[ "${1:-}" != --isolated ]]; then
    exec unshare --mount --fork bash "$0" --isolated "$(readlink /proc/self/ns/mnt)"
fi
if [[ "$(readlink /proc/self/ns/mnt)" == "${2:-}" ]]; then
    echo 'proc fixture tests require an isolated mount namespace' >&2
    exit 1
fi
mount --make-rprivate /

module_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export TEST_SYSGUARD_FIXTURES="$(mktemp -d)"
trap 'rm -f "$TEST_SYSGUARD_FIXTURES/meminfo" "$TEST_SYSGUARD_FIXTURES/stat"; rmdir "$TEST_SYSGUARD_FIXTURES"' EXIT
touch "$TEST_SYSGUARD_FIXTURES/meminfo" "$TEST_SYSGUARD_FIXTURES/stat"
mount --bind "$TEST_SYSGUARD_FIXTURES/meminfo" /proc/meminfo
mount --bind "$TEST_SYSGUARD_FIXTURES/stat" /proc/stat
prove -v "$module_root/t/sysguard_system.t"
