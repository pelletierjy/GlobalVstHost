spec:

&#x20; type: feature\_spec

&#x20; id: system-wide-vst-host

&#x20; title: System-Wide VST Audio Processor with Virtual Device

&#x20; description: >

&#x20;   A Windows application that creates a virtual audio output device,

&#x20;   routes all system audio through a VST plugin chain, and outputs to a

&#x20;   selected hardware device with low latency.



&#x20; goals:

&#x20;   - Provide a virtual audio output device recognized by Windows.

&#x20;   - Host VST2 and VST3 plugins for real-time processing.

&#x20;   - Route processed audio to a selected hardware output.

&#x20;   - Maintain low latency suitable for gaming and media playback.

&#x20;   - Provide a simple FXSound-style UI for non-technical users.



&#x20; non\_goals:

&#x20;   - No DAW-level routing or multi-track mixing.

&#x20;   - No macOS support in the initial release.

&#x20;   - No built-in DSP beyond plugin hosting.

&#x20;   - No ASIO multi-client support.



&#x20; requirements:

&#x20;   functional:

&#x20;     - Create a WASAPI-compatible virtual audio output device.

&#x20;     - Capture system audio from the virtual device.

&#x20;     - Implement a JUCE-based VST2/VST3 host.

&#x20;     - Support plugin scanning and loading.

&#x20;     - Support plugin chain ordering and bypass.

&#x20;     - Provide a UI for device selection and plugin management.

&#x20;     - Route processed audio to a hardware output device.

&#x20;     - Provide real-time input/output meters.

&#x20;     - Provide preset save/load functionality.



&#x20;   nonfunctional:

&#x20;     - Round-trip latency under 10 ms when possible.

&#x20;     - CPU usage under 5% during normal playback.

&#x20;     - Stable operation with long-running sessions.

&#x20;     - Graceful handling of sample rate mismatches.



&#x20; inputs:

&#x20;   - Windows WASAPI API

&#x20;   - JUCE framework

&#x20;   - VST2/VST3 plugin SDKs

&#x20;   - System audio stream from virtual device

&#x20;   - User-selected hardware output device



&#x20; outputs:

&#x20;   - Virtual audio device visible in Windows

&#x20;   - Real-time processed audio output

&#x20;   - UI with plugin chain and device controls

&#x20;   - Preset files for plugin configurations



&#x20; acceptance\_criteria:

&#x20;   - Windows detects the virtual audio device after installation.

&#x20;   - User can load ARC X or any VST plugin successfully.

&#x20;   - Audio flows through the plugin chain without dropouts.

&#x20;   - Latency remains acceptable for gaming (< 20 ms).

&#x20;   - UI allows selecting hardware output and managing plugins.

&#x20;   - Application runs for 12 hours without crashes or drift.



