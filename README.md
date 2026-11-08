<h3 align="center">textfetch</h3>

<p align="center">
    <strong>
    Another fetch tool for UNIX systems, written just for fun.
    </strong>
</p>
<p align="center">
    <a href="https://github.com/b1sted/textfetch/actions/workflows/tests.yml">
        <img src="https://img.shields.io/github/actions/workflow/status/b1sted/textfetch/tests.yml?style=flat-square&label=CI" alt="CI Status">
    </a>
    <a href="https://github.com/b1sted/textfetch/releases/latest">
        <img src="https://img.shields.io/github/v/release/b1sted/textfetch?style=flat-square&label=Release" alt="Release Version">
    </a>
    <a href="#license">
        <img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License" />
    </a>
</p>
<p align="center">
    <a href="#features">Features</a> •
    <a href="#output">Output</a> •
    <a href="#battery-health">Battery health</a> •
    <a href="#supported-platforms">Supported Platforms</a> •
    <a href="#build-and-run">Build and run</a> •
    <a href="#license">License</a>
</p>
<hr>

A minimal, fast system information tool with no third-party dependencies. Designed to cleanly output hardware and software specifications natively using platform interfaces such as `sysfs`, `procfs`, `sysctl`, and IOKit.

<br>
<p align="center">
  <img src=".github/assets/demo.png" alt="textfetch demo" width="600"/>
  <br>
  <em>textfetch v1.2.2 on macOS</em>
</p>
<br>

## Features

- **CPU**: Topology (physical core and logical thread counts), frequency, and model identification.
- **GPU**: Vendor and device detection via DRM/sysfs (Linux), IOKit (macOS), and the system Vulkan driver (Android).
- **Power**: Battery capacity, charging status, health status, and model (on Linux and macOS).
- **Memory & Storage**: RAM/Swap usage and mounted filesystems with usage, size, and filesystem type.
- **Software**: OS/distribution, kernel, uptime, shell, locale, process and thread counts, and device model.
- **Zero bloat**: No third-party dependencies. Written in C99 with POSIX.1-2008 and platform APIs. Android/Termux uses the system Vulkan loader already present on the device.

## Output

A typical Linux run:

```text
user@host
---------
OS: Arch Linux x86_64
Device: Example Laptop
Kernel: 6.12.1-arch1-1
Uptime: 3 days, 00:28:03
Processes: 2644 (7890 threads)
Shell: bash 5.2.37
Locale: en_US.UTF-8
CPU: AMD Ryzen 7 7840H (8C / 16T) @ 3.801 GHz
GPU: Advanced Micro Devices, Inc. [AMD/ATI] Phoenix1 [0x15BF]
RAM: 11.48 GiB / 27.09 GiB (42%)
Swap: 0.14 GiB / 32.00 GiB (0%)
Disk (/): 136.98 GiB / 240.00 GiB (57%) [btrfs]
Battery (L21M4PD6): 100% (Full, Health: Good)
```

| Field | Description |
| --- | --- |
| `user@host` | Current username and hostname. |
| `OS` | Distribution or operating system, version, and machine architecture. |
| `Device` | System or device model. Omitted when the platform exposes no model name. |
| `Kernel` | Kernel release string. |
| `Uptime` | Time elapsed since boot. |
| `Processes` | Process count and total thread count reported by the kernel; the thread count is omitted when unavailable. |
| `Shell` | Active interactive shell and version, resolved through the ancestor process chain. Falls back to `$SHELL` when no shell can be identified or queried. |
| `Locale` | Active locale, preferring `LC_ALL`, `LC_CTYPE`, and `LANG`; on macOS CoreFoundation provides the graphical-session fallback. |
| `CPU` | Model, physical cores (`C`), logical threads (`T`), and the highest available frequency. Missing topology or frequency details are omitted. |
| `GPU` | GPU vendor and device name, with the PCI device ID on Linux. Linux names are resolved from `pci.ids`; the vendor falls back to `Generic GPU` when no matching entry is found. |
| `RAM`, `Swap` | Used / total and percentage used. A system without swap shows `Unused`. |
| `Disk (mount)` | Used / total, percentage used, and filesystem type. Each physical filesystem is shown once; pseudo, loop, FUSE, and EROFS mounts are skipped on Linux. |
| `Battery (model)` | Charge percentage, power state, and health label. See [Battery health](#battery-health). |

The `Processes` line includes the total number of threads in parentheses, so it is normally greater than or equal to the process count. If the two samples are inconsistent or the thread counter is unavailable, only the process count is shown. On Linux the thread total comes from the kernel task counter in `/proc/loadavg`; when that file is restricted (as on Android), the `sysinfo()` task counter is used instead. On macOS it is the sum of the thread counts reported for each process, with inaccessible processes contributing one thread as a lower bound.

Fields are omitted when the underlying platform API does not expose the data. Run with `--verbose` to print diagnostics explaining why a value could not be read.

On Linux, install `pciutils` or `hwdata` if the `GPU` line falls back to `Generic GPU`; `textfetch` reads `pci.ids` from the standard system locations.

## Battery health

The `Battery` line shows the current charge level, power state, and a health label. Charge and health are different measurements: a battery can be charged to 100% while its health is `Fair` or `Poor`, because health describes how much capacity the battery has left compared with when it was new.

The power state may be `Charging`, `Discharging`, `Full`, `Not Charging`, or `Unknown`, depending on the platform and battery driver. On Linux it is read directly from `sysfs`; on macOS it is derived from IOKit power-source flags.

On Linux, `textfetch` estimates health from the ratio of the current full-charge capacity (`energy_full` or `charge_full`) to the original design capacity (`energy_full_design` or `charge_full_design`):

| Health | Meaning |
| --- | --- |
| `Good` | The battery still holds at least 80% of its design capacity. |
| `Fair` | The battery holds between 60% and 79% of its design capacity. |
| `Poor` | The battery holds less than 60% of its design capacity. |
| `Unknown` | The kernel does not expose the counters needed to estimate health. |

On macOS, the label comes from IOKit's `kIOPSBatteryHealthKey`, so it reflects Apple's own battery health classification.

Linux battery health is an estimate based on the capacity counters exposed by the kernel; firmware or vendor utilities may report a slightly different value.

## Supported Platforms

`textfetch` natively supports the following architectures and operating systems out-of-the-box:

- **macOS**: Intel (x86_64) on Mac OS X 10.7+ (legacy build) or 10.9+ (modern build), and Apple Silicon (ARM64) on macOS 11.0+.
- **Linux**: x86_64 and ARM64. CI builds both on Ubuntu; unit tests run on x86_64.
- **Android**: Termux (`aarch64`) and other Android ABIs supported by the NDK.
- **IoT/Embedded**: Builds for the Luckfox Pico Mini; CI produces the binary but does not run tests.

## Build and run

### Compilation from source
Requires a C99 compiler with a POSIX.1-2008 environment, GNU `make`, and the platform-specific APIs used for hardware discovery: Linux `sysfs`/`procfs`, macOS IOKit/CoreFoundation, and Android Bionic system properties. On macOS, install the Xcode Command Line Tools. On Android/Termux, only `clang` and `make` are needed; GPU detection loads the system Vulkan loader at runtime.

```bash
make
```

### Installation

```bash
sudo make install
```

`PREFIX` defaults to `/usr/local`; `DESTDIR` is supported for packaging:

```bash
make install PREFIX="$HOME/.local"
make install DESTDIR=/tmp/package PREFIX=/usr
```

### Usage

```bash
./bin/textfetch
```

Run `./bin/textfetch --help` for the full option list:

| Option | Description |
| --- | --- |
| `-h`, `--human-readable` | Print sizes in human-readable format (default) |
| `-k`, `--kibibytes` | Force kibibytes |
| `-m`, `--mebibytes` | Force mebibytes |
| `-g`, `--gibibytes` | Force gibibytes |
| `-v`, `--verbose` | Explain what is being done |
| `--no-color` | Disable color output |
| `--help` | Display help and exit |
| `-V`, `--version` | Output version information and exit |

Note that `-h` selects human-readable sizes; use `--help` (there is no short form) to print usage.

### Formatting and color

Sizes use binary units (`KiB`, `MiB`, `GiB`, and so on). By default they scale automatically based on the largest value in each line. `-k`, `-m`, and `-g` force a fixed unit, while `-h` restores automatic scaling.

Color is enabled only when standard output is a terminal. Set `NO_COLOR` to any value or pass `--no-color` to disable ANSI styling. Verbose diagnostics are written to standard error, so `--verbose` does not contaminate piped output.

`textfetch` exits with status `0` on success and `1` when an unknown option is supplied.

### Testing

The project ships with a self-contained test suite that runs under AddressSanitizer and UndefinedBehaviorSanitizer (leak detection is enabled on Linux; Apple's ASan runtime does not support it):

```bash
make test
```

Static analysis and formatting checks are available through `clang-format`, `clang-tidy`, and `cppcheck`:

```bash
make lint
```

`make lint` checks formatting first, then runs both static analyzers. Both `make test` and `make lint` are executed by the `Tests and Linters` GitHub Actions workflow.

Additional developer targets include `make dev`, `make leaks`, `make format`, `make format-check`, and `make clean`.

### Pre-built binaries
Pre-compiled tarballs for macOS (Intel legacy 10.7+, Intel modern 10.9+, and Apple Silicon), Linux (x86_64 and ARM64), the Luckfox Pico Mini, and Android/Termux (ARM64) are available in the [Releases](https://github.com/b1sted/textfetch/releases) section.

## License

Distributed under the **MIT License**.

You are free to use, modify, distribute, and sell this software. The only requirement is to preserve the copyright notice and the license file in any substantial portion of the software.

Full text of the license: [LICENSE](./LICENSE).
