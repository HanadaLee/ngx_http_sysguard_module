#!/usr/bin/env bash
set -euo pipefail

module_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
nginx_source="$module_root/.ci/nginx"
test_output="$module_root/.ci/test-output"
link="${TEST_MODULE_LINK:-static}"
sanitize="${TEST_SANITIZE:-disabled}"
expr="${TEST_EXPR:-disabled}"
mkdir -p "$test_output"
exec > >(tee "$test_output/run.log") 2>&1
trap 'rc=$?; printf "%s\n" "$rc" > "$test_output/exit-code"; chmod -R a+rX "$test_output"; exit "$rc"' EXIT

[[ "$(id -u)" == 0 ]] || { echo 'Run as root for isolated proc fixture tests' >&2; exit 1; }
case "$link" in static|dynamic) ;; *) exit 1 ;; esac
case "$sanitize" in enabled|disabled) ;; *) exit 1 ;; esac
case "$expr" in enabled|disabled) ;; *) exit 1 ;; esac

for dependency in nginx nginx-tests; do
    path="$module_root/.ci/$dependency"
    printf '%s %s\n' "$dependency" "$(git -c safe.directory="$path" -C "$path" rev-parse HEAD)"
done > "$test_output/dependencies.txt"

args=(--prefix="$nginx_source/test-prefix" --with-debug)
if [[ "$link" == dynamic ]]; then
    args+=(--with-compat --add-dynamic-module="$module_root")
else
    args+=(--add-module="$module_root")
fi
if [[ "$expr" == enabled ]]; then
    path="$module_root/.ci/ngx_expr_module"
    args+=(--with-http_json_module --add-module="$path")
    printf 'ngx_expr_module %s\n' "$(git -c safe.directory="$path" -C "$path" rev-parse HEAD)" >> "$test_output/dependencies.txt"
fi
if [[ "$sanitize" == enabled ]]; then
    args+=(--with-cc-opt='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie')
    args+=(--with-ld-opt='-fsanitize=address,undefined -no-pie')
    export ASAN_OPTIONS=detect_leaks=0:abort_on_error=1
    export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
fi

cd "$nginx_source"
configure=auto/configure
[[ ! -f configure ]] || configure=./configure
if ! sh "$configure" "${args[@]}" > "$test_output/configure.log" 2>&1; then
    cat "$test_output/configure.log"
    exit 1
fi
if ! make -j"${TEST_BUILD_JOBS:-4}" > "$test_output/build.log" 2>&1; then
    tail -100 "$test_output/build.log"
    exit 1
fi

export TEST_NGINX_BINARY="$nginx_source/objs/nginx"
export PERL5LIB="$module_root/.ci/nginx-tests/lib"
export TEST_NGINX_GLOBALS='user root;'
export TEST_NGINX_LEAVE=1
export TMPDIR="$test_output/tmp"
mkdir -p "$TMPDIR"
if [[ "$link" == dynamic ]]; then
    export TEST_NGINX_GLOBALS="user root; load_module $nginx_source/objs/ngx_http_sysguard_module.so;"
fi
"$TEST_NGINX_BINARY" -V > "$test_output/nginx-build.txt" 2>&1
sha256sum "$TEST_NGINX_BINARY" > "$test_output/nginx.sha256.txt"
unset HTTP_PROXY HTTPS_PROXY ALL_PROXY http_proxy https_proxy all_proxy
export NO_PROXY='*' no_proxy='*'

cd "$module_root"
tests=()
for file in t/*.t; do
    case "$file" in t/sysguard_system.t|t/sysguard_cpu.t) continue ;; esac
    tests+=("$file")
done
status=0
printf 'suite\texit_code\n' > "$test_output/results.tsv"
if prove -v "${tests[@]}" > "$test_output/regressions.log" 2>&1; then
    rc=0
else
    rc=$?
    status=$rc
fi
cat "$test_output/regressions.log"
printf 'regressions\t%s\n' "$rc" >> "$test_output/results.tsv"
if bash util/proc-tests.sh > "$test_output/system.log" 2>&1; then
    rc=0
else
    rc=$?
    status=$rc
fi
cat "$test_output/system.log"
printf 'system\t%s\n' "$rc" >> "$test_output/results.tsv"
exit "$status"
