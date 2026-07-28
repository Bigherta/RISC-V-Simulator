#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CPU_SIM="$SCRIPT_DIR/build/cpu_sim"
DATA_DIR="$SCRIPT_DIR/data"
TIMEOUT=120

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

if [ ! -x "$CPU_SIM" ]; then
    echo -e "${RED}Error: cpu_sim not found at $CPU_SIM${NC}"
    exit 1
fi

echo "=========================================="
echo "  CPU Simulator Test Suite"
echo "=========================================="
echo ""

passed=0
failed=0
timeout_count=0
total=0

while IFS= read -r -d '' c_file; do
    data_file="${c_file%.c}.data"
    if [ ! -f "$data_file" ]; then
        continue
    fi

    expected=$(grep -Po 'return.*//\s*\K\d+' "$c_file")
    if [ -z "$expected" ]; then
        echo -e "${YELLOW}SKIP: $c_file (no return comment found)${NC}"
        continue
    fi

    total=$((total + 1))
    rel_path="${c_file#$DATA_DIR/}"
    printf "TEST [%s]: " "$rel_path"

    out_file="${c_file%.c}.out"
    full_output=$(timeout $TIMEOUT "$CPU_SIM" < "$data_file" 2>/dev/null)
    exit_code=$?
    actual=$(echo "$full_output" | tail -1)

    if [ $exit_code -eq 124 ]; then
        echo -e "${YELLOW}TIMEOUT (>${TIMEOUT}s)${NC}"
        { echo "$full_output"; echo "TIMEOUT (>${TIMEOUT}s)"; } > "$out_file"
        timeout_count=$((timeout_count + 1))
    elif [ "$actual" = "$expected" ]; then
        echo -e "${GREEN}PASS${NC} (expected=$expected, got=$actual)"
        passed=$((passed + 1))
    else
        echo -e "${RED}FAIL${NC} (expected=$expected, got=$actual)"
        echo "$full_output" > "$out_file"
        failed=$((failed + 1))
    fi
done < <(find "$DATA_DIR" -name '*.c' -print0)

echo ""
echo "=========================================="
echo "  Results: $passed passed, $failed failed, $timeout_count timeout (total $total)"
echo "=========================================="