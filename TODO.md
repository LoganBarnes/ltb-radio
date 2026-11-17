# TODO

Open work, roughly in priority order within each section.

## Build and CI

- [ ] **GitHub Actions builds.** Catch bugs that only show up in other builds and
      configurations. Examples so far: an `argc` used only by an `assert` failed Release builds
      under the strict flags, and a submodule commit that was never pushed broke every clean
      checkout. Build matrix:
  - macOS (AppleClang) and Ubuntu (GCC and Clang); Windows (MSVC) later.
  - Debug and Release, both with `LTB_RADIO_USE_STRICT_FLAGS=ON`.
  - A clean checkout without submodules, so ltb-vlk is downloaded at the pinned commit.
  - Run `ltb-radio-tests` with `ctest`. They need no GPU or HackRF.
  - Install the Vulkan SDK (or headers, loader and `glslc`), `pkg-config` and `libusb`.
  - Cache `CPM_SOURCE_CACHE` between runs.
- [ ] **Format and lint checks.** Fail CI on `clang-format` differences, and optionally run
      `clang-tidy` with the repo's `.clang-tidy`.

## Radios

- [ ] **ILS localizer/glideslope time-sharing.** A toggle in the ILS Indicator that alternates
      between the localizer and its paired glideslope (every 0.25–0.5 s) so both needles update
      live. Measure tone depths per dwell rather than with the long-running meters, and accept
      a short gap in the ident audio at each switch. libhackrf's sweep mode
      (`hackrf_start_rx_sweep`) may hop faster than retuning.
- [ ] **Stereo FM.** The 19 kHz pilot and 38 kHz stereo subcarrier are already visible in the
      FM Discriminator stage.
- [ ] **Station finder.** List strong carriers in the RF Input spectrum as clickable stations.
- [ ] **Waterfall view.** A scrolling spectrogram for spotting stations and fading over time.
- [ ] **Morse ident decoding.** Decode the 1020 Hz ident on VOR and ILS to confirm the station.
