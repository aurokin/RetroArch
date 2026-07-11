# Agent-Control Interface

The `agent-control` branch extends RetroArch's shared command parser with
deterministic input, timing, state, memory, and replay operations. The commands
are transport-neutral: configured stdin and network command interfaces use the
same action table in `command.h`.

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
