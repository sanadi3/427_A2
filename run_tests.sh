#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$ROOT_DIR/src"
TEST_DIR="$ROOT_DIR/test-cases"
OUT_DIR="${TMPDIR:-/tmp}/mysh-test-outputs"

cleanup_build() {
  make -C "$SRC_DIR" clean >/dev/null 2>&1 || true
}
trap cleanup_build EXIT

if [[ ! -d "$SRC_DIR" || ! -d "$TEST_DIR" ]]; then
  echo "error: expected src/ and test-cases/ under $ROOT_DIR"
  exit 1
fi

mkdir -p "$OUT_DIR"

echo "[1/2] Building mysh"
if ! make -C "$SRC_DIR" clean mysh >/dev/null; then
  echo "build failed"
  exit 1
fi

pass=0
fail=0
skip=0

printf "\n[2/2] Running tests\n"

shopt -s nullglob
pushd "$TEST_DIR" >/dev/null || exit 1
for input in T_*.txt; do
  name="$(basename "$input")"
  [[ "$name" == *_result* ]] && continue

  base="${name%.txt}"
  expected_files=("${base}"_result*.txt)

  if (( ${#expected_files[@]} == 0 )); then
    echo "SKIP $name (no expected output)"
    ((skip++))
    continue
  fi

  out_file="$OUT_DIR/${base}.out"
  "$SRC_DIR/mysh" < "$input" > "$out_file"

  matched=0
  for expected in "${expected_files[@]}"; do
    if diff -q "$expected" "$out_file" >/dev/null; then
      matched=1
      break
    fi
  done

  if (( matched )); then
    echo "PASS $name"
    ((pass++))
  else
    echo "FAIL $name"
    echo "  output: $out_file"
    ((fail++))
  fi
done
popd >/dev/null || exit 1

printf "\nSummary: %d passed, %d failed, %d skipped\n" "$pass" "$fail" "$skip"

if (( fail > 0 )); then
  exit 1
fi

exit 0
