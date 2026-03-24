#!/bin/bash
# Aria test harness
# Reads directives from the first 10 lines of each .aria file:
#   // test-files: file1.aria file2.aria    — compile these files together (in order)
#   // test-expect: expected output         — check stdout contains this text
#   // test-requires: feature               — skip test (external dependency)
#   // test-support: true                   — support file, don't run directly

COMPILER="${ARIA_COMPILER:-src/main}"
DIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
SKIP=0
CRASH=0
TOTAL=0
FAILURES=""

for f in "$DIR"/*.aria; do
    name=$(basename "$f" .aria)
    TOTAL=$((TOTAL + 1))

    # Read directives from first 10 lines
    test_files=""
    test_expect=""
    test_requires=""
    test_support=""
    while IFS= read -r line; do
        case "$line" in
            "// test-files: "*)    test_files="${line#// test-files: }" ;;
            "// test-expect: "*)   test_expect="${line#// test-expect: }" ;;
            "// test-requires: "*) test_requires="${line#// test-requires: }" ;;
            "// test-support: "*)  test_support="${line#// test-support: }" ;;
        esac
    done < <(head -10 "$f")

    # Skip support files
    if [ "$test_support" = "true" ]; then
        SKIP=$((SKIP + 1))
        continue
    fi

    # Skip tests with external requirements
    if [ -n "$test_requires" ]; then
        SKIP=$((SKIP + 1))
        continue
    fi

    # Determine build command
    if [ -n "$test_files" ]; then
        build_args=""
        first_file=""
        for tf in $test_files; do
            build_args="$build_args $DIR/$tf"
            if [ -z "$first_file" ]; then first_file="$DIR/$tf"; fi
        done
        # Compiler names output after the first file
        bin="${first_file%.aria}"
    else
        build_args="$f"
        bin="${f%.aria}"
    fi

    rm -f "$bin"

    # Build (capture output, allow failure)
    build_out=$($COMPILER build $build_args 2>&1 || true)

    if [ ! -f "$bin" ]; then
        FAIL=$((FAIL + 1))
        err=$(echo "$build_out" | grep "error\[" | head -1)
        FAILURES="${FAILURES}FAIL (build): ${name} — ${err}\n"
        continue
    fi

    # Run
    run_out=$("$bin" 2>&1 || true)
    run_exit=${PIPESTATUS[0]}

    # Check for crash (signal-based exit codes > 128)
    if [ "$run_exit" -gt 1 ] 2>/dev/null; then
        CRASH=$((CRASH + 1))
        FAILURES="${FAILURES}CRASH: ${name} (exit ${run_exit})\n"
        continue
    fi

    # Check expected output
    if [ -n "$test_expect" ]; then
        if echo "$run_out" | grep -qF "$test_expect"; then
            PASS=$((PASS + 1))
        else
            FAIL=$((FAIL + 1))
            got=$(echo "$run_out" | tail -1)
            FAILURES="${FAILURES}FAIL (output): ${name}\n  expected: ${test_expect}\n  got: ${got}\n"
        fi
    else
        PASS=$((PASS + 1))
    fi
done

echo ""
if [ -n "$FAILURES" ]; then
    echo -e "$FAILURES"
fi
echo "Results: $PASS pass, $FAIL fail, $CRASH crash, $SKIP skip / $TOTAL total"

if [ "$FAIL" -gt 0 ] || [ "$CRASH" -gt 0 ]; then
    exit 1
fi
