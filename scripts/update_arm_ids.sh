#!/bin/bash
# SPDX-License-Identifier: MIT
#
# Import ARM implementer/part IDs that are missing from data/arm.ids using the
# Linux kernel as the source of truth. Existing entries and their curated names
# are never overwritten.

set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

IDS_FILE="$PROJECT_ROOT/data/arm.ids"
LINUX_REF="${LINUX_REF:-master}"
LINUX_BASE_URL="${LINUX_BASE_URL:-https://raw.githubusercontent.com/torvalds/linux}"

SOURCE_FILE=""
REPORT_FILE=""

usage() {
    cat <<EOF
Usage: $0 [options]

Import missing ARM CPU IDs from the Linux kernel.

Options:
  --source FILE  read a local cputype.h instead of downloading it
  --ref REF      Linux ref to download (default: $LINUX_REF)
  --report FILE  write added entries as "implementer<TAB>part<TAB>name"
  -h, --help     show this help
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --source)
            SOURCE_FILE="${2:-}"
            [ -n "$SOURCE_FILE" ] || { echo "Error: --source needs a file." >&2; exit 2; }
            shift 2
            ;;
        --ref)
            LINUX_REF="${2:-}"
            [ -n "$LINUX_REF" ] || { echo "Error: --ref needs a value." >&2; exit 2; }
            shift 2
            ;;
        --report)
            REPORT_FILE="${2:-}"
            [ -n "$REPORT_FILE" ] || { echo "Error: --report needs a file." >&2; exit 2; }
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Error: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [ ! -f "$IDS_FILE" ]; then
    echo "Error: $IDS_FILE not found." >&2
    exit 1
fi

source_path=""
source_label=""
tmp_source=""
tmp_data="$(mktemp "${TMPDIR:-/tmp}/arm_ids.data.XXXXXX")"
tmp_report="$(mktemp "${TMPDIR:-/tmp}/arm_ids.report.XXXXXX")"

cleanup() {
    rm -f "$tmp_source" "$tmp_data" "$tmp_report"
}
trap cleanup EXIT

if [ -n "$SOURCE_FILE" ]; then
    if [ ! -f "$SOURCE_FILE" ]; then
        echo "Error: $SOURCE_FILE not found." >&2
        exit 1
    fi

    source_path="$SOURCE_FILE"
    source_label="$SOURCE_FILE"
else
    if ! command -v curl >/dev/null 2>&1; then
        echo "Error: curl not found. Use --source to import a local file." >&2
        exit 1
    fi

    tmp_source="$(mktemp "${TMPDIR:-/tmp}/arm_ids.cputype.XXXXXX")"
    source_url="$LINUX_BASE_URL/$LINUX_REF/arch/arm64/include/asm/cputype.h"

    echo "Downloading $source_url..."
    if ! curl -fsSL --retry 3 --retry-delay 2 "$source_url" -o "$tmp_source"; then
        echo "Error: failed to download $source_url." >&2
        exit 1
    fi

    source_path="$tmp_source"
    source_label="Linux kernel $LINUX_REF arch/arm64/include/asm/cputype.h"
fi

LC_ALL=C awk \
    -v report="$tmp_report" \
    -v version="$(date -u +%Y.%m)" \
    -v source="$source_label" '
function warn(msg)
{
    printf "warning: %s\n", msg > "/dev/stderr"
}

function fail(msg)
{
    printf "%s:%d: %s\n", FILENAME, FNR, msg > "/dev/stderr"
    bad = 1
}

function zero_pad(value, width,   out)
{
    out = tolower(value)

    while (length(out) < width) {
        out = "0" out
    }

    return out
}

function display_name(suffix,   count, tokens, i, token, out)
{
    count = split(suffix, tokens, "_")
    out = ""

    for (i = 1; i <= count; i++) {
        token = tokens[i]

        if (token in WORD) {
            token = WORD[token]
        } else if (token ~ /^[A-Z]+$/) {
            token = substr(token, 1, 1) tolower(substr(token, 2))
        }

        if (i == 1) {
            out = token
        } else {
            out = out "-" token
        }
    }

    return out
}

function vendor_name(prefix)
{
    if (prefix in VENDOR) {
        return VENDOR[prefix]
    }

    warn("unknown implementer prefix " prefix ", using it verbatim")
    return prefix
}

function sort_key(imp)
{
    # ARM is listed first; the remaining implementers are ascending.
    if (imp == "41") {
        return "00"
    }

    return imp
}

function add_part(imp, part, name,   key)
{
    key = imp SUBSEP part

    if (key in kernel_part) {
        if (kernel_part[key] != name) {
            duplicate_note[key] = kernel_part[key] SUBSEP name
        }
        return
    }

    kernel_part[key] = name
}

BEGIN {
    in_header = 1

    WORD["CORTEX"] = "Cortex"
    WORD["NEOVERSE"] = "Neoverse"
    WORD["THUNDERX"] = "ThunderX"
    WORD["THUNDERX2"] = "ThunderX2"
    WORD["OCTX2"] = "OcteonTX2"
    WORD["XGENE"] = "X-Gene"
    WORD["AEM"] = "AEM"

    VENDOR["ARM"] = "ARM"
    VENDOR["APM"] = "APM"
    VENDOR["CAVIUM"] = "Cavium"
    VENDOR["BRCM"] = "Broadcom"
    VENDOR["QCOM"] = "Qualcomm"
    VENDOR["NVIDIA"] = "NVIDIA"
    VENDOR["FUJITSU"] = "Fujitsu"
    VENDOR["HISI"] = "HiSilicon"
    VENDOR["APPLE"] = "Apple"
    VENDOR["AMPERE"] = "Ampere"
    VENDOR["MICROSOFT"] = "Microsoft"
}

FNR == NR {
    if ($1 != "#define" || $3 !~ /^0x[0-9A-Fa-f]+$/) {
        next
    }

    macro = $2
    hex = substr($3, 3)
    pos = index(macro, "_CPU_IMP_")

    if (pos > 0) {
        prefix = substr(macro, pos + 9)
        implementer[prefix] = zero_pad(hex, 2)

        if (!(prefix in VENDOR)) {
            VENDOR[prefix] = vendor_name(prefix)
        }

        next
    }

    pos = index(macro, "_CPU_PART_")

    if (pos > 0) {
        prefix = substr(macro, 1, pos - 1)
        suffix = substr(macro, pos + 10)

        if (!(prefix in implementer)) {
            warn("no implementer define for " prefix ", skipping " macro)
            next
        }

        add_part(implementer[prefix], zero_pad(hex, 3), display_name(suffix))
    }

    next
}

{
    line = $0

    if (line ~ /^#/ || line == "") {
        if (in_header) {
            header[++header_count] = line
        }
        next
    }

    in_header = 0

    if (substr(line, 1, 1) == "\t") {
        if (current == "") {
            fail("part entry before any implementer")
            next
        }

        fields = split(line, field, "\t")
        if (fields != 3) {
            fail("part line must contain exactly two tabs")
            next
        }

        key = current SUBSEP field[2]
        if (key in existing_part) {
            warn("duplicate " current "/" field[2] " in " FILENAME)
            next
        }

        existing_part[key] = field[3]
        next
    }

    if (length(line) < 5 || substr(line, 3, 2) != "  ") {
        fail("invalid implementer line")
        next
    }

    current = substr(line, 1, 2)
    vendor[current] = substr(line, 5)

    if (!(current in seen_vendor)) {
        seen_vendor[current] = 1
        vendor_list[++vendor_count] = current
    }
}

END {
    if (bad) {
        exit 1
    }

    for (key in existing_part) {
        merged[key] = existing_part[key]
    }

    for (key in kernel_part) {
        if (key in existing_part) {
            continue
        }

        merged[key] = kernel_part[key]
        added++
        split(key, parts, SUBSEP)
        printf "%s\t%s\t%s\n", parts[1], parts[2], kernel_part[key] >> report

        if (!(parts[1] in seen_vendor)) {
            seen_vendor[parts[1]] = 1
            vendor_list[++vendor_count] = parts[1]
            vendor[parts[1]] = VENDOR[parts[1]]
        }
    }

    for (key in duplicate_note) {
        if (key in existing_part) {
            continue
        }

        split(key, dup_parts, SUBSEP)
        split(duplicate_note[key], dup_names, SUBSEP)
        warn("duplicate " dup_parts[1] "/" dup_parts[2] ": keeping \"" dup_names[1] "\", ignoring \"" dup_names[2] "\"")
    }

    for (i = 2; i <= vendor_count; i++) {
        value = vendor_list[i]
        j = i - 1

        while (j >= 1 && sort_key(vendor_list[j]) > sort_key(value)) {
            vendor_list[j + 1] = vendor_list[j]
            j--
        }

        vendor_list[j + 1] = value
    }

    for (i = 1; i <= header_count; i++) {
        if (added > 0 && header[i] ~ /^# Version: /) {
            print "# Version: " version
        } else {
            print header[i]
        }
    }

    for (v = 1; v <= vendor_count; v++) {
        imp = vendor_list[v]

        if (v > 1) {
            print ""
        }

        print imp "  " vendor[imp]

        part_count = 0
        for (key in merged) {
            split(key, parts, SUBSEP)
            if (parts[1] == imp) {
                part_list[++part_count] = parts[2]
            }
        }

        for (i = 2; i <= part_count; i++) {
            value = part_list[i]
            j = i - 1

            while (j >= 1 && part_list[j] > value) {
                part_list[j + 1] = part_list[j]
                j--
            }

            part_list[j + 1] = value
        }

        for (i = 1; i <= part_count; i++) {
            print "\t" part_list[i] "\t" merged[imp SUBSEP part_list[i]]
        }

        split("", part_list)
    }

    printf "Imported %d new part(s) from %s\n", added, source > "/dev/stderr"
}
' "$source_path" "$IDS_FILE" > "$tmp_data"

if [ -s "$tmp_report" ]; then
    LC_ALL=C sort -t $'\t' -k1,1 -k2,2 -o "$tmp_report" "$tmp_report"
fi

if [ ! -s "$tmp_report" ]; then
    echo "$IDS_FILE is already up to date."
    exit 0
fi

if [ -n "$REPORT_FILE" ]; then
    install -m 644 "$tmp_report" "$REPORT_FILE"
fi

install -m 644 "$tmp_data" "$IDS_FILE"

added="$(wc -l < "$tmp_report" | tr -d ' ')"
echo "Updated $IDS_FILE with $added new part(s)."
echo "Run 'make arm-ids' to regenerate include/arm_ids.h."
