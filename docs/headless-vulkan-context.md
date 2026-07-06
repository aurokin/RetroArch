# Experimental headless Vulkan context

Status: spike-only. This is intended for automated eval runners on render-only or connectorless Vulkan devices, especially Intel B50 SR-IOV VFs on Bront (`saur` / `tortle`). It is not planned for merge at this time.

## Context driver

Set:

```ini
video_driver = "vulkan"
video_context_driver = "headless_vk"
```

The `headless_vk` context creates a `VK_EXT_headless_surface` instead of an X11, Wayland, or KHR-display surface. This avoids the X11/DRI3 presentation requirement that fails when the real desktop is on virtio but the desired Vulkan device is a connectorless B50 VF.

## Why presentation is skipped

The initial prototype created a headless surface and used RetroArch's normal swapchain/present path. On `saur` with the B50 VF, that reached hardware Vulkan successfully but crashed inside Mesa's Intel Vulkan driver on `vkQueuePresentKHR`:

```text
SIGSEGV in /lib/x86_64-linux-gnu/libvulkan_intel.so
#8  vulkan_present()
#9  gfx_ctx_headless_vk_swap_buffers()
```

For eval automation we do not need compositor presentation. We need submitted GPU work plus screenshot/readback evidence. Therefore `headless_vk` deliberately does **not** call `vulkan_present()` / `vkQueuePresentKHR` in `swap_buffers`; it waits for the queue, recycles RetroArch frame fences/semaphores, and leaves the acquired swapchain image available for subsequent frames/readback.

This is intentional, not a temporary fallback to software rendering.

## Verified behavior

Validated on `saur` with an Intel Arc Pro B50 SR-IOV VF:

```text
[Vulkan] Found GPU #0: "Intel(R) Arc(tm) Pro B50 Graphics (BMG G21)".
[Vulkan] Found GPU #1: "llvmpipe (LLVM 20.1.2, 256 bits)".
[Vulkan] Using GPU #0: "Intel(R) Arc(tm) Pro B50 Graphics (BMG G21)".
[Vulkan] Created headless Vulkan surface/swapchain: 640x480.
```

When combined with the `agent-control` branch and the `parallel-n64` adapter override, the Paper Mario title-screen scenario completed its command sequence and produced a capture/evidence bundle without `DISPLAY`, `WAYLAND_DISPLAY`, DRI3, or lavapipe.

## Known limitations

- This path is for automated runs, not visible desktop presentation.
- It depends on command/readback workflows for evidence.
- Screenshot hashes can differ from headed/X11 baselines; semantic traces should be used to decide whether a new headless baseline should be minted.
- The branch is intentionally isolated; no merge into mainline is currently planned.
