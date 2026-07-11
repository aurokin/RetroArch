# Agent Instructions

## Scope
- Work on the `agent-control` branch unless the task explicitly targets upstream synchronization.
- Keep control, replay, capture, and headless transport generic across libretro cores.
- Do not add game-, core-, renderer-, host-, or fleet-specific policy here.

## References
| Need | File |
|------|------|
| Project overview | `README.md` |
| Contribution conventions | `CONTRIBUTING.md` |
| Security policy | `SECURITY.md` |
| Deterministic command contract | `docs/agent-control.md` |
| Headless Vulkan contract | `docs/headless-vulkan-context.md` |

## Commands
| Task | Command |
|------|---------|
| Configure | `./configure` |
| Build | `make -j4` |
| Clean | `make clean` |
| Check patch hygiene | `git diff --check` |

## Working Rules
- Preserve upstream behavior when the added interfaces are unused.
- Keep command syntax and replies documented alongside changes to `command.c` or `command.h`.
- Build after source changes; consumer repositories own content-specific integration tests.
- Do not commit generated binaries, runtime configuration dumps, captures, or local paths.
