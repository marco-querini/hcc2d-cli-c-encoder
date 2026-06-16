# Changelog

## 0.9.0

- packaged standalone single-file C reference encoder
- aligned filename and internal version strings to `0.9.0`
- set CLI default scale to `12`
- enforce minimum scale of `6` pixels per module
- tightened `HCC2DF` validation:
  - require successful `compress2(...)` before storing compressed content
  - require valid UTF-8 filename bytes
- added explicit Apache 2.0 license link in source header
- added explicit reference to the `HCC2D Code Specification v0.9.0` PDF
- completed a specification-conformance review against `HCC2D Code Specification v0.9.0`
  with the following checklist areas verified:
  - encoding parameters
  - palette Model 1 / Model 2 definitions
  - payload framing
  - version selection
  - HCC2D codeword organization
  - plane construction
  - common mask selection on inverted plane 0
  - inner-matrix construction
  - function-module rendering
  - Color Palette Pattern formulas
  - rendered output coordinates and quiet zone
  - decoder-relevant structural rules
  - end-to-end encoding procedure
  - optional HCC2DF wrapper structure and compression rule
