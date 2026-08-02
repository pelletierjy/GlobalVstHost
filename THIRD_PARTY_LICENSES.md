# Third-Party Licenses

This document lists the third-party software and libraries used by **JyGlobalVST**
(Global VST Host) and their respective licenses.

## Steinberg ASIO SDK

- **Version:** 2.3 (or compatible)
- **License:** Dual-licensed under the Proprietary Steinberg ASIO License and
  the General Public License (GPL) Version 3.
- **Usage:** Mandatory audio device output support.
- **Obligations:** See `third_party/asiosdk/LICENSE.txt` for full terms. Before
  shipping a proprietary product, a signed License Agreement from Steinberg
  Media Technologies GmbH is required. The SDK must not be used to re-engineer
  or manipulate any Steinberg or third-party technology unless permitted by law.

## JUCE Framework

- **Version:** 8.0.4
- **License:** Dual-licensed under the GNU Affero General Public License v3.0
  (AGPL-3.0) and the commercial JUCE 8 End User Licence Agreement.
- **Usage:** UI framework, VST3 hosting, AudioProcessorGraph, audio device
  abstraction.
- **Obligations:** See `third_party/juce-src/LICENSE.md` for full terms.

## VST3 SDK (included via JUCE)

- **License:** Dual-licensed under the Proprietary Steinberg VST3 License and
  the GNU General Public License (GPL) Version 3.
- **Usage:** VST3 plugin format support.
- **Obligations:** See
  `third_party/juce-src/modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt`
  for full terms.

## nlohmann/json

- **Version:** 3.11.3
- **License:** MIT License
- **Copyright:** Copyright (c) 2013-2022 Niels Lohmann
- **Usage:** JSON serialization for presets, settings, scan cache, update
  manifests, and IPC envelopes.

## GoogleTest

- **Version:** 1.14.0
- **License:** BSD-3-Clause License
- **Copyright:** Copyright 2008, Google Inc.
- **Usage:** Unit and integration testing (build-time dependency only; not
  linked in release binaries).
