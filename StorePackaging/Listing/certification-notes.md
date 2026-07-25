# Notes for certification

Paste the block below into Partner Center → your submission → **Notes for certification**.

Its job is to answer, before a reviewer has to ask: how do you test an audio plugin host on
a clean machine with no plugins installed, and why does an app that never records you ask
for microphone permission. Those are the two most likely reasons this submission stalls
(Store Policy 10.3 "Product is Testable", and reviewer scrutiny of the `microphone`
capability under 10.6).

Keep it factual and short. Reviewers read many of these.

---

## Text to paste

```text
WHAT THIS APP DOES

Global VST Host captures the audio Windows is already playing (WASAPI loopback),
runs it through an ordered chain of audio effects, and outputs it to an audio
device you select. It installs no driver and requires no reboot.

HOW TO TEST IT WITHOUT INSTALLING ANY PLUGINS

No third-party plugins are needed. Two effects are built into the app and appear
in the effect list immediately, with no scanning, download, or file selection:

  1. "Night-time"      - a stereo compressor/limiter that evens out loud and
                         quiet passages so volume stays consistent at low
                         listening levels.
  2. "EQ (Bass Boost)" - a multi-band equalizer with a dedicated bass boost.

Suggested 2-minute test:

  1. Launch the app. It runs in the system tray - click the tray icon to open
     the window. (It has no taskbar window by design.)
  2. Start playing any audio in another app (browser video, music player).
  3. In Global VST Host, select an audio output device (for example your
     speakers or headphones).
  4. Add "EQ (Bass Boost)" to the chain and raise the Bass Boost amount. The
     audio you are already playing audibly gains low end in real time.
  5. Add "Night-time" and play content with both loud and quiet passages. Loud
     parts are pulled down and quiet parts brought up.
  6. Toggle each effect's bypass button. Audio returns to unprocessed
     immediately, with no dropout.

The app is fully functional and demonstrable with zero third-party plugins
installed.

ABOUT THE MICROPHONE CAPABILITY

The app declares the "microphone" device capability because it captures audio.
No audio is ever written to disk or transmitted in any mode - it is processed in
memory in real time and discarded.

There are two capture sources, chosen by the user:

  1. System audio via WASAPI loopback (the default and primary use case). This
     reads the audio the system is already sending to its OUTPUT device. No
     microphone is involved, but Windows still requires the microphone
     capability for audio capture, which is why the microphone privacy
     indicator appears.

  2. A hardware input device the user selects from the input list, which
     enumerates WASAPI capture endpoints. If the user selects a microphone, the
     app processes that microphone's audio in real time, by explicit user
     choice. This is disclosed in the privacy policy.

The app never records, stores, or transmits captured audio in either mode.

THIRD-PARTY VST3 PLUGIN SUPPORT

Loading user-installed VST3 plugins is the app's described primary purpose and
is stated at the beginning of the Store description.

  - The app does not download, install, or distribute any plugin, and performs
    no network communication of any kind. The shipped binary imports no HTTP,
    TLS, or socket library.
  - It only enumerates and loads VST3 plugins ALREADY installed on the machine
    by the user, from the standard location
    (C:\Program Files\Common Files\VST3) or a folder the user selects.
  - Each plugin is loaded and processed inside a structured-exception guard, so
    a faulty plugin is bypassed and reported in the UI rather than crashing the
    application.

NO DRIVER DEPENDENCY

The app depends on no third-party driver and no NT service. System audio capture
uses the WASAPI loopback interface built into Windows.

PRIVACY

No personal information is collected, stored off-device, or transmitted. No
telemetry, analytics, or crash reporting. Privacy policy: <PRIVACY POLICY URL>

SYSTEM REQUIREMENTS

Windows 10 version 1909 (build 18363) or later, x64. No login or server is
required to test the app.
```

---

## Before pasting

- [ ] Replace `<PRIVACY POLICY URL>` with the live URL.
- [ ] Re-verify the tray interaction wording in step 1 against the shipped build — if the
      app opens a window on launch, simplify that step. It should describe what a reviewer
      actually sees, not what the spec intends.
- [ ] Walk the 6-step test yourself on the **installed MSIX package** (not a `build/` run)
      before submitting. If any step does not behave as written, fix the text, not the
      reviewer's expectations. A test script that does not reproduce is worse than none.
