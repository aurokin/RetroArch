# Headless Vulkan Context

The `headless_vk` context supports Vulkan rendering on systems without a
display server or connector. It is explicit-only and cannot be selected by
automatic context fallback.

## Configuration

```ini
video_driver = "vulkan"
video_context_driver = "headless_vk"
```

The context requires `VK_EXT_headless_surface`. It creates a headless surface
and swapchain while leaving normal Vulkan device selection unchanged.

## Presentation Model

`headless_vk` deliberately skips `vkQueuePresentKHR`. Its swap operation waits
for submitted work, recycles RetroArch's frame synchronization objects, and
keeps the acquired image available for capture or readback. This is a hardware
Vulkan path, not a software-rendering fallback.

## Constraints

- It produces no visible desktop presentation.
- Validation and automation must use capture or readback output.
- Captures can differ from headed contexts because presentation paths differ.
- The selected Vulkan implementation must expose `VK_EXT_headless_surface`.
