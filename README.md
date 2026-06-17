# HCC2D CLI Encoder 0.9.0

Written by Marco Querini

Standalone distribution of the HCC2D single-file C reference encoder.

## Official links

- HCC2D website: `https://hcc2d.com/en/`
- HCC2D specification PDF: `https://hcc2d.com/hcc2d_specification_v0.9.0.pdf`
- HCC2D API: `https://hcc2d.com/en/api`
- HCC2D Decoder official availability:
  - Google Play: `https://play.google.com/store/apps/details?id=com.hcc2d.decoder`
  - Huawei AppGallery: `https://appgallery.cloud.huawei.com/marketshare/app/C117478101`
  - App Store for iPhone: `https://apps.apple.com/app/id6762202762`
- HCC2D Encoder official availability:
  - App Store for Mac: `https://apps.apple.com/app/id6767833986`

## Contents

- `single_file_c_hcc2d_encoder_v0.9.0.c`
- `LICENSE`
- `CHANGELOG.md`
- `Makefile`
- `SHA256SUMS.txt`

## Scope

This package contains a compact single-file C encoder intended to conform to:

- `HCC2D Code Specification version 0.9.0`
- Reference PDF: `https://hcc2d.com/hcc2d_specification_v0.9.0.pdf`

For normative format details, decoding assumptions, payload-wrapper rules, and
parameter-table definitions, refer to the specification PDF above.

The implementation supports:

- `qr`
- `hcc2d4`
- `hcc2d8`
- EC levels `L`, `M`, `Q`, `H`
- versions `1..40`
- auto-version selection with `0`
- raster PNG output
- optional `HCC2DF` payload wrapping

## Build

Prerequisites:

- C compiler such as `cc`
- zlib development package

Build:

```bash
make
```

Equivalent direct command:

```bash
cc -O2 -o hcc2d_encoder single_file_c_hcc2d_encoder_v0.9.0.c -lz
```

Install to the default local prefix:

```bash
make install
```

Install into a packaging or staging directory:

```bash
make install DESTDIR=/tmp/hcc2d-stage PREFIX=/usr
```

## Usage

Show help:

```bash
./hcc2d_encoder --help
```

Examples:

1. Standard QR Code from raw text bytes:

```bash
./hcc2d_encoder --text "https://hcc2d.com/spec" --mode qr --file-format raw qr_spec_url.png
```

- uses standard QR mode
- stores the text bytes directly, with no wrapper
- produces a black/white QR PNG
- good for short plain-text payloads such as URLs, IDs, or short labels
- this is the most sensible choice when you do not need filename metadata

2. HCC2D4 from raw text bytes:

```bash
./hcc2d_encoder --text "Batch=2026-06-16;Line=A3;Unit=004271" --mode hcc2d4 --file-format raw hcc2d4_label.png
```

- uses the 4-colour HCC2D family
- stores the payload bytes directly
- avoids the overhead of an `HCC2DF` wrapper
- good when the payload is already application-defined text or binary and you do not need an embedded filename
- usually preferable to `hcc2df` for short or medium payloads where every byte matters

3. HCC2D4 from a small file, wrapped in HCC2DF:

```bash
./hcc2d_encoder --input-file notes.txt --mode hcc2d4 --file-format hcc2df hcc2d4_notes.png
```

- reads the bytes of `notes.txt`
- wraps them in `HCC2DF`
- preserves the basename `notes.txt` inside the payload
- good when the receiver expects a file-oriented payload, not just anonymous bytes
- for small files, compression may not be applied because the wrapper only compresses when:
  - the original content is at least `128` bytes, and
  - the compressed result is strictly smaller than `90%` of the original size

4. HCC2D8 from a larger file with filename metadata and possible compression:

```bash
./hcc2d_encoder --input-file report.pdf --mode hcc2d8 --file-format hcc2df hcc2d8_report.png
```

- reads raw bytes from `report.pdf`
- wraps them in `HCC2DF`
- preserves the basename `report.pdf` inside the wrapped payload
- uses the 8-colour HCC2D family for higher capacity per module
- this is the sensible path when you are encoding a real file and want the payload to carry file identity
- if the file is large enough and compresses well, the encoder may store zlib-compressed content automatically inside `HCC2DF`
- if compression does not help enough, the encoder falls back to raw file bytes inside the wrapper

5. Force a specific version and error-correction level:

```bash
./hcc2d_encoder --text "Inspection record #004271 / line A3 / station 12" --mode hcc2d4 --file-format raw --ec-level H --version 5 forced_v5_h.png
```

- forces version `5`
- forces error-correction level `H`
- fails if the framed payload does not fit that exact symbol size
- useful when you need a predictable printed size or want to test a specific version/EC combination

6. Increase raster scale for decoder testing:

```bash
./hcc2d_encoder --text "Synthetic decoder check payload" --mode hcc2d4 --file-format raw --scale 12 --quiet-zone 4 scaled.png
```

- keeps the logical symbol unchanged
- changes only the rendered output size
- useful for generating synthetic decoder test images with comfortable module size

### Choosing `raw` vs `hcc2df`

Use `raw` when:

- the payload is already your final byte sequence
- you do not need embedded filename metadata
- you want to minimize wrapper overhead
- you are encoding short text, IDs, labels, or already-structured binary

Use `hcc2df` when:

- you are encoding an actual file
- you want the payload to preserve the file basename
- you want optional automatic compression when it materially reduces size
- you want a receiver to reconstruct a file-oriented payload rather than anonymous bytes

## Parameters

### Required inputs

- `output.png`
  - destination PNG path
- one of:
  - `--text STRING`
  - `--input-file PATH`

Exactly one payload source is required.

### Payload source parameters

- `--text STRING`
  - encodes the UTF-8 bytes of the provided string exactly as given
- `--input-file PATH`
  - reads raw bytes from the specified file

### Payload format parameters

- `--file-format hcc2df`
  - wraps the source bytes in an `HCC2DF` container
  - includes filename metadata
  - may apply zlib compression when the wrapper rule allows it
  - this is the default
- `--file-format raw`
  - encodes the source bytes directly with no `HCC2DF` wrapper

### Symbol parameters

- `--mode qr`
  - generates a standard black/white QR Code
- `--mode hcc2d4`
  - generates a 4-colour HCC2D symbol
  - 2 bits per data module
  - this is the default
- `--mode hcc2d8`
  - generates an 8-colour HCC2D symbol
  - 3 bits per data module

- `--ec-level L|M|Q|H`
  - error-correction level
  - default: `Q`

- `--version N`
  - `0` means auto-select the smallest fitting version
  - `1..40` forces a specific version if the payload fits
  - default: `0`

### Rendering parameters

- `--scale PX`
  - output raster size in pixels per module
  - default: `12`
  - minimum allowed value: `6`

- `--quiet-zone N`
  - white margin around the rendered symbol, measured in modules
  - default: `4`

- `--palette model1|model2`
  - ignored for `--mode qr`
  - `model1`: screen-oriented palette
  - `model2`: print/CMYK-oriented palette
  - default: `model1`

## Defaults

- mode: `hcc2d4`
- EC level: `Q`
- version: `0` (smallest fitting)
- scale: `12` px/module (default)
- scale must be at least `6` px/module
- quiet zone: `4` modules
- palette: `model1`
- file format: `hcc2df`

## Validation notes

This package was reviewed against the HCC2D 0.9.0 specification with checks on:

- payload framing
- HCC2D plane construction
- shared mask selection on inverted plane 0
- function-module rendering
- Color Palette Pattern formulas
- HCC2DF wrapper structure and compression rule

## License

This package is distributed under Apache License 2.0. See `LICENSE`.

## APT repository

This repository also contains scaffolding for a signed self-hosted APT
repository for Debian and Ubuntu. See `apt-repo/README.md`.
