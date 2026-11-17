# CLAUDE.md

Project conventions, layout and build commands are shared with other tools in
`.github/copilot-instructions.md`; keep that file up to date rather than duplicating it here.

@.github/copilot-instructions.md

## Claude-specific notes

- Build and run the tests after changes; strict flags turn every warning into an error.
- `radio-app` is a GUI app. It can't be driven from the terminal, so verify visual changes by
  asking the user to run it (it's usually launched from CLion). Don't kill a running `radio-app`;
  it's likely the user's.
- Running the app writes window layout and open views to
  `~/Library/Application Support/ltb-radio/imgui.ini`.
