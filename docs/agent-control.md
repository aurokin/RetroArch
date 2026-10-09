# Agent-Control Interface

The `agent-control` branch extends RetroArch's shared command parser with
deterministic input, timing, state, memory, and replay operations. The commands
are transport-neutral: configured stdin and network command interfaces use the
same action table in `command.h`.

## Building

Linux recipe used for the known-good agent-control build:

```sh
./configure --disable-wayland --enable-x11 --enable-opengl --enable-vulkan \
  --enable-sdl2 --enable-alsa --enable-udev --enable-freetype --enable-zlib \
  --enable-ffmpeg
make -j4
```

- `--enable-vulkan` is required for the `headless_vk` context; it makes
  configure fail instead of silently omitting Vulkan when headers are missing.
- The command interface needs no flag: configure enables `HAVE_NETWORK_CMD`
  with networking and `HAVE_STDIN_CMD` when `fcntl` is available, and either
  one enables `HAVE_COMMAND`. Confirm all three in `config.mk` after configuring.
- The remaining flags are host-specific: they pin the windowing, audio, input,
  font, and media features of the reference build so its feature set does not
  drift with installed packages. Adjust them for the target host.

macOS needs a different pinned configure environment (Metal, MoltenVK, and
no SDL2). The parallel-n64 repository's `tools/adapters` directory has a build
helper for it.

## macOS configuration

Use `profiles/agent-control-macos.cfg` as the explicit base configuration for
Apple Silicon macOS builds with Cocoa input, CoreAudio3 and Metal/MoltenVK:

```sh
retroarch --config /absolute/path/to/profiles/agent-control-macos.cfg \
  --appendconfig '/absolute/path/to/session.cfg|/absolute/path/to/extra.cfg' \
  --libretro /absolute/path/to/core.dylib /absolute/path/to/content
```

The profile pins the platform drivers, synchronous Vulkan rendering, Metal
argument buffers and vsync. It enables stdin commands and keeps control active
when the window loses focus. Automatic core/content overrides and remaps are
disabled so ambient desktop settings cannot silently change the session. Saving
the active configuration on exit is disabled. App-bundle asset extraction is
also disabled: its completion callback saves a full config independently of the
exit setting. Agent sessions do not require desktop menu assets. The profile contains no content,
core options, personal paths or window preferences, and does not replace the
tracked example `retroarch.cfg`.

Configuration loads in this order: explicit base, generated session settings,
then explicit extra files. Later files override earlier files; duplicate keys
within one file use the first occurrence. `--appendconfig` accepts one `|`-joined
list, and repeating the option replaces its previous list. Use separate files
for overlays rather than concatenating duplicate keys into one file. Callers
must validate file readability because failed appended loads only log an error.

An existing `MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS` environment variable takes
precedence over `video_use_metal_arg_buffers`, even if its value is empty. Leave
it unset for the profile to control MoltenVK, or set it explicitly for a declared
runtime condition. Changes require a new frontend process. The parallel-n64
adapters accept this profile through `--base-config` or `RETROARCH_BASE_CONFIG`,
preserve explicit environment overrides, and snapshot optional extra config
into the session bundle. The generic Linux configuration is unchanged.

Run `python3 tests-other/test_agent_control_macos_profile.py` for the real config
parser composition check. Runtime qualification must additionally verify the
selected build's drivers, capture, frame/input control, state barriers and
teardown. Renderer-specific hi-res claims require the renderer's own fixtures.

## Offline command advertisement

`retroarch --verbose --command __LIST_SUPPORTED_COMMANDS__` prints the compiled
command table and exits 1 because the supplied command is intentionally invalid.
Validation rejects it before sending a UDP packet or loading content. This can
identify supported commands without an active emulator. Command-only failures
exit directly; they must not enter teardown of an uninitialized runtime.
The advertisement proves command availability, not successful runtime behavior.

## Stdin Transport

Build with stdin command support and set `stdin_cmd_enable = "true"` in the
active configuration. Commands are newline-delimited on stdin and replies are
written to stdout. The selected input driver must not claim stdin.

`--start-paused` loads content, pauses before its first core frame, and starts
the command-visible frame counter at zero.

## Command Contract

| Command | Contract |
|---------|----------|
| `PING` | Replies `PING OK`. |
| `GET_STATUS` | Reports content, pause, system, and command-visible frame state. |
| `SET_PAUSE ON\|OFF\|TOGGLE` | Changes pause state and reports `PAUSED` or `PLAYING`. |
| `STEP_FRAME <count>` | Accepts a positive frame request; the run loop executes it and pauses. |
| `SET_INPUT_PORT <port> <mask> [lx ly rx ry]` | Replaces one port's joypad and analog state. |
| `CLEAR_INPUT_PORT <port>` | Releases the input override and clears its state. |
| `GET_INPUT_PORT <port>` | Reports the current override state. |
| `LOAD_STATE_SLOT <slot>` | Queues a slot load and resets the command-visible frame counter. |
| `LOAD_STATE_SLOT_PAUSED <slot>` | Queues a slot load, resets frame counters, and pauses. |
| `WAIT_SAVE_STATE` | Waits for the pending save task and replies `DONE`. |
| `WAIT_LOAD_STATE` | Waits for the pending load task and replies `DONE`. |
| `READ_CORE_MEMORY <address> <count>` | Reads system-addressed core memory. |
| `WRITE_CORE_MEMORY <address> <bytes...>` | Writes system-addressed core memory. |
| `PLAY_REPLAY_SLOT <slot>` | Starts replay playback from a configured replay slot. |
| `SEEK_REPLAY <frame>` | Seeks active replay playback to a frame. |
| `RECORD_REPLAY_PATH <path>` | Starts an anchored BSV2 recording at the current state. |
| `PLAY_REPLAY_PATH <path>` | Restores and plays an anchored BSV2 recording. |
| `STOP_REPLAY` | Stops recording or playback and reports its frame count. |

`LOAD_STATE_SLOT` and `LOAD_STATE_SLOT_PAUSED` acknowledge queueing, not task
completion. Follow either with `WAIT_LOAD_STATE` when subsequent commands
require the loaded state. Apply the same rule to saves with `WAIT_SAVE_STATE`.
`WAIT_* DONE` means the blocking task queue was drained; it does not report the
underlying save or load result. Treat it as a sequencing barrier and verify
success through the operation's logs or resulting state.

`STEP_FRAME` acknowledges the accepted request before the requested frames are
complete. Poll `GET_STATUS` when a client needs to observe the resulting frame
count and paused state.

Anchored recording and playback require serialization support and at least one
completed core frame. Path-based replay commands, `SEEK_REPLAY`, and
`STOP_REPLAY` return `NO` when their prerequisites are not met. The legacy
`PLAY_REPLAY_SLOT` path may fail without a reply payload. `GET_STATUS` includes
the command-visible `frame=` counter.

The action table in `command.h` and implementations in `command.c` are the
source of truth for exact argument and reply formats.
