#!/bin/bash
# SPDX-License-Identifier: MIT

set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

IDS_FILE="${1:-$PROJECT_ROOT/data/arm.ids}"

if [ ! -f "$IDS_FILE" ]; then
    echo "Error: $IDS_FILE not found." >&2
    exit 1
fi

LC_ALL=C awk '
function fail(msg)
{
    printf "%s:%d: %s\n", FILENAME, FNR, msg > "/dev/stderr"
    errors++
}

function is_lower_hex(value, length_wanted,   i, c)
{
    if (length(value) != length_wanted) {
        return 0
    }

    for (i = 1; i <= length_wanted; i++) {
        c = substr(value, i, 1)
        if (c !~ /[0-9a-f]/) {
            return 0
        }
    }

    return 1
}

BEGIN {
    vendor = ""
    last_imp = ""
    last_part = ""
    has_version = 0
    vendors = 0
    parts = 0
    errors = 0
}

{
    line = $0

    if (line ~ /\r$/) {
        fail("CRLF line ending")
        next
    }

    if (line ~ /[ \t]$/) {
        fail("trailing whitespace")
        next
    }

    if (line == "") {
        next
    }

    if (substr(line, 1, 1) == "#") {
        if (line ~ /^# Version: [0-9][0-9][0-9][0-9]\.[0-9][0-9]$/) {
            has_version = 1
        }
        next
    }

    if (substr(line, 1, 1) == "\t") {
        if (vendor == "") {
            fail("part entry before any implementer")
            next
        }

        fields = split(line, field, "\t")
        if (fields != 3) {
            fail("part line must contain exactly two tabs")
            next
        }

        part = field[2]
        name = field[3]

        if (!is_lower_hex(part, 3)) {
            fail("part ID must be three lowercase hex digits")
            next
        }

        if (name == "" || name ~ /^[ \t]/) {
            fail("empty or malformed part name")
            next
        }

        if (last_part != "" && part <= last_part) {
            fail("part IDs must be strictly increasing (duplicate or out of order)")
        }

        last_part = part
        parts++
        next
    }

    if (length(line) < 5 || substr(line, 3, 2) != "  ") {
        fail("invalid implementer line")
        next
    }

    imp = substr(line, 1, 2)
    name = substr(line, 5)

    if (!is_lower_hex(imp, 2)) {
        fail("implementer ID must be two lowercase hex digits")
        next
    }

    if (name == "" || name ~ /^[ \t]/ || index(name, "\t") != 0) {
        fail("empty or malformed implementer name")
        next
    }

    last_part = ""

    if (last_imp != "" && imp <= last_imp) {
        # ARM is listed first; every other implementer is ascending.
        if (!(vendors == 1 && last_imp == "41" && imp < last_imp)) {
            fail("implementers must be strictly increasing (duplicate or out of order)")
            next
        }
    }

    last_imp = imp
    vendor = imp
    vendors++
}

END {
    if (!has_version) {
        printf "%s: missing or malformed \"# Version: YYYY.MM\"\n", FILENAME > "/dev/stderr"
        errors++
    }

    if (errors > 0) {
        printf "%s: %d error(s)\n", FILENAME, errors > "/dev/stderr"
        exit 1
    }

    printf "%s: OK (%d implementers, %d parts)\n", FILENAME, vendors, parts
}
' "$IDS_FILE"
