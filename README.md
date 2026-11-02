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
    <a href="#supported-platforms">Supported Platforms</a> •
    <a href="#build-and-run">Build and run</a> •
    <a href="#license">License</a>
</p>
<hr>

A minimal, fast system information tool with no third-party dependencies. Designed to cleanly output hardware and software specifications natively using platform interfaces such as `sysfs`, `procfs`, `sysctl`, and IOKit.

<br>
<p align="center">
  <img src=".github/assets/demo.png" alt="textfetch demo" width="600"/>
</p>
<br>

## Features

- **CPU**: Topology (physical core count), frequency, and model identification.
- **GPU**: Vendor and device detection via DRM/sysfs (Linux), IOKit (macOS), and EGL/GLES (Android).
- **Power**: Battery capacity, charging status, health status, and model (on Linux and macOS).
- **Memory & Storage**: RAM/Swap usage and mounted filesystems with usage, size, and filesystem type.
- **Software**: OS/distribution, kernel, uptime, shell, locale, process count, and device model.
- **Zero bloat**: No third-party dependencies. Written in C99 with POSIX.1-2008 and platform APIs. Android/Termux requires `mesa` and `mesa-dev` for GPU detection.

## Supported Platforms

`textfetch` natively supports the following architectures and operating systems out-of-the-box:

- **macOS**: Intel (x86_64) on Mac OS X 10.7+ (legacy build) or 10.9+ (modern build), and Apple Silicon (ARM64) on macOS 11.0+.
- **Linux**: x86_64 and ARM64. CI builds both on Ubuntu; unit tests run on x86_64.
- **Android**: Termux (`aarch64`). Requires `mesa` and `mesa-dev` for GPU detection.
- **IoT/Embedded**: Builds for the Luckfox Pico Mini; CI produces the binary but does not run tests.

## Build and run

### Compilation from source
Requires a C99 compiler with a POSIX.1-2008 environment, GNU `make`, and the platform-specific APIs used for hardware discovery: Linux `sysfs`/`procfs`, macOS IOKit/CoreFoundation, and Android Bionic system properties. On macOS, install the Xcode Command Line Tools. On Android/Termux, install `mesa` and `mesa-dev`.

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
