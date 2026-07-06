/*  RetroArch - A frontend for libretro.
 *
 *  Experimental Vulkan headless context for automated/eval runs.
 *
 *  This context uses VK_EXT_headless_surface so Vulkan hardware contexts can
 *  render without X11/Wayland/DRI3 presentation. It intentionally mirrors the
 *  minimal KHR_display Vulkan context shape while avoiding any native window or
 *  display connector dependency.
 */

#include <assert.h>
#include <compat/strl.h>
#include <retro_timers.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../frontend/frontend_driver.h"
#include "../../verbosity.h"
#include "../common/vulkan_common.h"

typedef struct
{
   gfx_ctx_vulkan_data_t vk;
   int swap_interval;
   unsigned width;
   unsigned height;
} headless_vk_ctx_data_t;

static void gfx_ctx_headless_vk_destroy(void *data)
{
   headless_vk_ctx_data_t *headless = (headless_vk_ctx_data_t*)data;
   if (!headless)
      return;

   vulkan_context_destroy(&headless->vk, true);
#ifdef HAVE_THREADS
   if (headless->vk.context.queue_lock)
      slock_free(headless->vk.context.queue_lock);
#endif
   free(headless);
}

static void gfx_ctx_headless_vk_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   headless_vk_ctx_data_t *headless = (headless_vk_ctx_data_t*)data;
   *width                          = headless->width;
   *height                         = headless->height;
}

static void *gfx_ctx_headless_vk_init(void *video_driver)
{
   headless_vk_ctx_data_t *headless = (headless_vk_ctx_data_t*)
      calloc(1, sizeof(*headless));
   if (!headless)
      return NULL;

   if (!vulkan_context_init(&headless->vk, VULKAN_WSI_HEADLESS))
   {
      RARCH_ERR("[Vulkan] Failed to create headless Vulkan context.\n");
      goto error;
   }

   frontend_driver_install_signal_handler();
   return headless;

error:
   gfx_ctx_headless_vk_destroy(headless);
   return NULL;
}

static void gfx_ctx_headless_vk_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   headless_vk_ctx_data_t *headless = (headless_vk_ctx_data_t*)data;
   *resize                         = (headless->vk.flags & VK_DATA_FLAG_NEED_NEW_SWAPCHAIN) ? true : false;

   if (headless->width != *width || headless->height != *height)
   {
      *width                       = headless->width;
      *height                      = headless->height;
      *resize                      = true;
   }

   if ((bool)frontend_driver_get_signal_handler_state())
      *quit                        = true;
}

static bool gfx_ctx_headless_vk_set_resize(void *data,
      unsigned width, unsigned height)
{
   headless_vk_ctx_data_t *headless = (headless_vk_ctx_data_t*)data;

   headless->width                  = width;
   headless->height                 = height;

   if (!vulkan_create_swapchain(&headless->vk, headless->width, headless->height,
            headless->swap_interval))
   {
      RARCH_ERR("[Vulkan] Failed to update headless swapchain.\n");
      return false;
   }

   if (headless->vk.flags & VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN)
      vulkan_acquire_next_image(&headless->vk);

   headless->vk.context.flags      |=  VK_CTX_FLAG_INVALID_SWAPCHAIN;
   headless->vk.flags              &= ~VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   return true;
}

static bool gfx_ctx_headless_vk_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   headless_vk_ctx_data_t *headless = (headless_vk_ctx_data_t*)data;

   if (!width)
      width                         = 640;
   if (!height)
      height                        = 480;

   if (!vulkan_surface_create(&headless->vk, VULKAN_WSI_HEADLESS, NULL, NULL,
            width, height, headless->swap_interval))
   {
      RARCH_ERR("[Vulkan] Failed to create headless surface.\n");
      gfx_ctx_headless_vk_destroy(data);
      return false;
   }

   headless->width                  = headless->vk.context.swapchain_width;
   headless->height                 = headless->vk.context.swapchain_height;

   RARCH_LOG("[Vulkan] Created headless Vulkan surface/swapchain: %ux%u.\n",
         headless->width, headless->height);
   return true;
}

static void gfx_ctx_headless_vk_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static enum gfx_ctx_api gfx_ctx_headless_vk_get_api(void *data)
{
   return GFX_CTX_VULKAN_API;
}

static bool gfx_ctx_headless_vk_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   return (api == GFX_CTX_VULKAN_API);
}

static void gfx_ctx_headless_vk_set_flags(void *data, uint32_t flags) { }
static bool gfx_ctx_headless_vk_has_focus(void *data) { return true; }
static bool gfx_ctx_headless_vk_suppress_screensaver(void *data, bool enable) { return false; }
static gfx_ctx_proc_t gfx_ctx_headless_vk_get_proc_address(const char *symbol) { return NULL; }

static void gfx_ctx_headless_vk_set_swap_interval(void *data,
      int swap_interval)
{
   headless_vk_ctx_data_t *headless = (headless_vk_ctx_data_t*)data;

   if (headless->swap_interval != swap_interval)
   {
      headless->swap_interval = swap_interval;
      if (headless->vk.swapchain)
         headless->vk.flags  |= VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   }
}

static void gfx_ctx_headless_vk_swap_buffers(void *data)
{
   headless_vk_ctx_data_t *headless = (headless_vk_ctx_data_t*)data;
   gfx_ctx_vulkan_data_t *vk        = &headless->vk;
   vulkan_context_t *ctx            = &vk->context;
   unsigned index;

   if (ctx->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
   {
      /* Intel's Mesa driver can create a VK_EXT_headless_surface swapchain,
       * but vkQueuePresentKHR() on that surface currently crashes on B50 SR-IOV
       * VFs. For the eval path we only need the submitted render work and
       * readback, not compositor presentation, so keep the initially acquired
       * image and recycle RetroArch's frame fences without presenting. */
      if (vk->swapchain == VK_NULL_HANDLE)
         retro_sleep(10);
      else
      {
#ifdef HAVE_THREADS
         slock_lock(ctx->queue_lock);
#endif
         vkQueueWaitIdle(ctx->queue);
#ifdef HAVE_THREADS
         slock_unlock(ctx->queue_lock);
#endif

         if (ctx->swapchain_semaphores[ctx->current_swapchain_index]
               != VK_NULL_HANDLE)
         {
            vkDestroySemaphore(ctx->device,
                  ctx->swapchain_semaphores[ctx->current_swapchain_index], NULL);
            ctx->swapchain_semaphores[ctx->current_swapchain_index] = VK_NULL_HANDLE;
         }
      }
   }

   ctx->current_frame_index =
      (ctx->current_frame_index + 1) % ctx->num_swapchain_images;
   index = ctx->current_frame_index;

   if (ctx->swapchain_fences[index] != VK_NULL_HANDLE)
   {
      if (ctx->swapchain_fences_signalled[index])
         vkWaitForFences(ctx->device, 1,
               &ctx->swapchain_fences[index], true, UINT64_MAX);
      vkResetFences(ctx->device, 1, &ctx->swapchain_fences[index]);
   }
   else
   {
      VkFenceCreateInfo fence_info;
      fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fence_info.pNext = NULL;
      fence_info.flags = 0;
      vkCreateFence(ctx->device, &fence_info, NULL,
            &ctx->swapchain_fences[index]);
   }
   ctx->swapchain_fences_signalled[index] = false;

   if (ctx->swapchain_wait_semaphores[index] != VK_NULL_HANDLE)
   {
      assert(ctx->num_recycled_acquire_semaphores < VULKAN_MAX_SWAPCHAIN_IMAGES);
      ctx->swapchain_recycled_semaphores[ctx->num_recycled_acquire_semaphores++] =
         ctx->swapchain_wait_semaphores[index];
      ctx->swapchain_wait_semaphores[index] = VK_NULL_HANDLE;
   }
}

static uint32_t gfx_ctx_headless_vk_get_flags(void *data)
{
   uint32_t flags = 0;

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static void *gfx_ctx_headless_vk_get_context_data(void *data)
{
   headless_vk_ctx_data_t *headless = (headless_vk_ctx_data_t*)data;
   return &headless->vk.context;
}

const gfx_ctx_driver_t gfx_ctx_headless_vk = {
   gfx_ctx_headless_vk_init,
   gfx_ctx_headless_vk_destroy,
   gfx_ctx_headless_vk_get_api,
   gfx_ctx_headless_vk_bind_api,
   gfx_ctx_headless_vk_set_swap_interval,
   gfx_ctx_headless_vk_set_video_mode,
   gfx_ctx_headless_vk_get_video_size,
   NULL,                                        /* get_refresh_rate */
   NULL,                                        /* get_video_output_size */
   NULL,                                        /* get_video_output_prev */
   NULL,                                        /* get_video_output_next */
   NULL,                                        /* get_metrics */
   NULL,
   NULL,                                        /* update_title */
   gfx_ctx_headless_vk_check_window,
   gfx_ctx_headless_vk_set_resize,
   gfx_ctx_headless_vk_has_focus,
   gfx_ctx_headless_vk_suppress_screensaver,
   false,                                       /* has_windowed */
   gfx_ctx_headless_vk_swap_buffers,
   gfx_ctx_headless_vk_input_driver,
   gfx_ctx_headless_vk_get_proc_address,
   NULL,
   NULL,
   NULL,
   "headless_vk",
   gfx_ctx_headless_vk_get_flags,
   gfx_ctx_headless_vk_set_flags,
   NULL,
   gfx_ctx_headless_vk_get_context_data,
   NULL,                                        /* make_current */
   NULL,                                        /* create_surface */
   NULL                                         /* destroy_surface */
};
