#include "virtio/virtio-gpu/venus_hw.h"
#include <unistd.h>


#include <webroguegfx/webroguegfx.h>
#include "drm-uapi/virtgpu_drm.h"
#include "vn_renderer_internal.h"

#define WEBROGUE_PCI_VENDOR_ID 0x1af4
#define WEBROGUE_PCI_DEVICE_ID 0x1050
#define WEBROGUE_SYNC_WAIT_FLAG_ANY (1u << 0)

struct webrogue;

struct webrogue_shmem {
   struct vn_renderer_shmem base;
};

struct webrogue_bo {
   struct vn_renderer_bo base;

   bool mappable;
};

struct webrogue_sync {
   struct vn_renderer_sync base;
};

struct webrogue {
   struct vn_renderer base;

   struct vn_instance *instance;

   mtx_t sock_mutex;

   uint32_t max_timeline_count;

   struct {
      uint32_t id;
      uint32_t version;
      struct virgl_renderer_capset_venus data;
   } capset;

   struct util_sparse_array shmem_array;
   struct util_sparse_array bo_array;

   struct vn_renderer_shmem_cache shmem_cache;
};

static bool
webrogue_vcmd_get_capset(struct webrogue *webrogue,
                      uint32_t id,
                      uint32_t version,
                      void *capset,
                      size_t capset_size)
{
   uint32_t size = webroguegfx_vulkan_get_capset(id, version, capset,
                                                 capset_size);
   if (!size)
      return false;
   memset(capset + size, 0, capset_size - size);
   return true;
}

static int32_t
webrogue_vcmd_sync_wait(struct webrogue *webrogue,
                        uint32_t flags,
                        uint32_t timeout,
                        struct vn_renderer_sync *const *syncs,
                        const uint64_t *vals,
                        uint32_t count)
{
   uint32_t *buf = malloc(3 * count * sizeof(uint32_t));
   if (!buf && count)
      return (int32_t)VK_ERROR_OUT_OF_HOST_MEMORY;

   for (uint32_t i = 0; i < count; i++) {
      const uint64_t val = vals[i];
      buf[i * 3 + 0] = syncs[i]->sync_id;
      buf[i * 3 + 1] = (uint32_t)val;
      buf[i * 3 + 2] = (uint32_t)(val >> 32);
   }

   int32_t result = (int32_t)webroguegfx_vulkan_sync_wait(flags, timeout,
                                          (const uint8_t *)buf,
                                          count ? 3 * count * sizeof(uint32_t) : 0);
   free(buf);

   return result;
}

static void
webrogue_vcmd_submit_cmd2(struct webrogue *webrogue,
                       const struct vn_renderer_submit *submit)
{
   const uint32_t batch_count = submit->batch_count;
   if (!batch_count)
      return;

   size_t cs_size = 0;
   size_t sync_size = 0;
   for (uint32_t i = 0; i < batch_count; i++) {
      const struct vn_renderer_submit_batch *batch = &submit->batches[i];
      assert(batch->cs_size % sizeof(uint32_t) == 0);
      cs_size += batch->cs_size;
      sync_size += (sizeof(uint32_t) + sizeof(uint64_t)) * batch->sync_count;
   }

   /* flat batch headers: [cmd_offset, cmd_size, sync_offset,
    * sync_count, ring_idx] per batch, offsets in dwords relative to the
    * cs/syncs buffers */
   uint32_t *headers = malloc(5 * sizeof(uint32_t) * batch_count);
   uint8_t *cmds = malloc(cs_size);
   uint8_t *syncs = malloc(sync_size);
   if (!headers || (cs_size && !cmds) || (sync_size && !syncs)) {
      free(headers);
      free(cmds);
      free(syncs);
      return;
   }

   uint32_t cs_words = 0;
   uint32_t sync_words = 0;
   for (uint32_t i = 0; i < batch_count; i++) {
      const struct vn_renderer_submit_batch *batch = &submit->batches[i];
      uint32_t *h = headers + i * 5;
      h[0] = cs_words;
      h[1] = batch->cs_size / sizeof(uint32_t);
      h[2] = sync_words;
      h[3] = batch->sync_count;
      h[4] = batch->ring_idx;

      if (batch->cs_size)
         memcpy(cmds + cs_words * sizeof(uint32_t), batch->cs_data,
                batch->cs_size);
      cs_words += batch->cs_size / sizeof(uint32_t);

      for (uint32_t j = 0; j < batch->sync_count; j++) {
         const uint64_t val = batch->sync_values[j];
         uint32_t *s = (uint32_t *)(syncs + sync_words * sizeof(uint32_t));
         s[0] = batch->syncs[j]->sync_id;
         s[1] = (uint32_t)val;
         s[2] = (uint32_t)(val >> 32);
         sync_words += 3;
      }
   }

   webroguegfx_vulkan_submit_cmd((const uint8_t *)headers,
                                 5 * batch_count * sizeof(uint32_t),
                                 cmds, cs_size,
                                 syncs, sync_size);

   free(headers);
   free(cmds);
   free(syncs);
}

static VkResult
webrogue_sync_write(struct vn_renderer *renderer,
                 struct vn_renderer_sync *_sync,
                 uint64_t val)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;
   struct webrogue_sync *sync = (struct webrogue_sync *)_sync;

   mtx_lock(&webrogue->sock_mutex);
   webroguegfx_vulkan_sync_write(sync->base.sync_id, val);
   mtx_unlock(&webrogue->sock_mutex);

   return VK_SUCCESS;
}

static VkResult
webrogue_sync_read(struct vn_renderer *renderer,
                struct vn_renderer_sync *_sync,
                uint64_t *val)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;
   struct webrogue_sync *sync = (struct webrogue_sync *)_sync;

   mtx_lock(&webrogue->sock_mutex);
   *val = webroguegfx_vulkan_sync_read(sync->base.sync_id);
   mtx_unlock(&webrogue->sock_mutex);

   return VK_SUCCESS;
}

static VkResult
webrogue_sync_reset(struct vn_renderer *renderer,
                 struct vn_renderer_sync *sync,
                 uint64_t initial_val)
{
   /* same as write */
   return webrogue_sync_write(renderer, sync, initial_val);
}

static void
webrogue_sync_destroy(struct vn_renderer *renderer,
                   struct vn_renderer_sync *_sync)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;
   struct webrogue_sync *sync = (struct webrogue_sync *)_sync;

   mtx_lock(&webrogue->sock_mutex);
   webroguegfx_vulkan_sync_unref(sync->base.sync_id);
   mtx_unlock(&webrogue->sock_mutex);

   free(sync);
}

static VkResult
webrogue_sync_create(struct vn_renderer *renderer,
                  uint64_t initial_val,
                  uint32_t flags,
                  struct vn_renderer_sync **out_sync)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;

   struct webrogue_sync *sync = calloc(1, sizeof(*sync));
   if (!sync)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   mtx_lock(&webrogue->sock_mutex);
   sync->base.sync_id = webroguegfx_vulkan_sync_create(initial_val);
   mtx_unlock(&webrogue->sock_mutex);

   *out_sync = &sync->base;
   return VK_SUCCESS;
}

static void
webrogue_bo_invalidate(struct vn_renderer *renderer,
                    struct vn_renderer_bo *bo,
                    VkDeviceSize offset,
                    VkDeviceSize size)
{
   /* nop */
}

static void
webrogue_bo_flush(struct vn_renderer *renderer,
               struct vn_renderer_bo *bo,
               VkDeviceSize offset,
               VkDeviceSize size)
{
   /* nop */
}

static void *
webrogue_bo_map(struct vn_renderer *renderer,
             struct vn_renderer_bo *_bo,
             void *placed_addr)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;
   struct webrogue_bo *bo = (struct webrogue_bo *)_bo;
   const bool mappable = bo->mappable;

   /* not thread-safe but is fine */
   if (!bo->base.mmap_ptr && mappable) {
      VkDeviceSize alloc_size = align(bo->base.mmap_size, 16 * 1024);
      void *ptr = aligned_alloc(16*1024, alloc_size);
      if (ptr == NULL) {
         vn_log(webrogue->instance, "failed to allocate %zu bytes", bo->base.mmap_size);
      } else {
         webroguegfx_vulkan_register_blob(bo->base.res_id, ptr, alloc_size);
         bo->base.mmap_ptr = ptr;
      }
   }

   return bo->base.mmap_ptr;
}

static int
webrogue_bo_export_dma_buf(struct vn_renderer *renderer,
                        struct vn_renderer_bo *_bo)
{
   return -1;
}

static bool
webrogue_bo_destroy(struct vn_renderer *renderer, struct vn_renderer_bo *_bo)
{
   struct webrogue_bo *bo = (struct webrogue_bo *)_bo;

   webroguegfx_vulkan_resource_unref(bo->base.res_id);

   /* unregister then free, so the shadow mapping doesn't dangle on freed memory */
   if (bo->base.mmap_ptr)
      free(bo->base.mmap_ptr);

   return true;
}

static VkResult
webrogue_bo_create_from_device_memory(
   struct vn_renderer *renderer,
   VkDeviceSize size,
   vn_object_id mem_id,
   VkMemoryPropertyFlags flags,
   VkExternalMemoryHandleTypeFlags external_handles,
   struct vn_renderer_bo **out_bo)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;

   uint32_t res_id = webroguegfx_vulkan_create_blob(NULL, size, mem_id);
   if (!res_id)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   struct webrogue_bo *bo = util_sparse_array_get(&webrogue->bo_array, res_id);
   *bo = (struct webrogue_bo){
      .base = {
         .refcount = VN_REFCOUNT_INIT(1),
         .res_id = res_id,
         .mmap_size = size,
      },
      .mappable = !!(flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
   };

   *out_bo = &bo->base;

   return VK_SUCCESS;
}

static void
webrogue_shmem_destroy_now(struct vn_renderer *renderer,
                        struct vn_renderer_shmem *_shmem)
{
   struct webrogue_shmem *shmem = (struct webrogue_shmem *)_shmem;

   webroguegfx_vulkan_resource_unref(shmem->base.res_id);
}

static void
webrogue_shmem_destroy(struct vn_renderer *renderer,
                    struct vn_renderer_shmem *shmem)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;

   if (vn_renderer_shmem_cache_add(&webrogue->shmem_cache, shmem))
      return;

   webrogue_shmem_destroy_now(&webrogue->base, shmem);
}

static struct vn_renderer_shmem *
webrogue_shmem_create(struct vn_renderer *renderer, size_t size)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;

   struct vn_renderer_shmem *cached_shmem =
      vn_renderer_shmem_cache_get(&webrogue->shmem_cache, size);
   if (cached_shmem) {
      cached_shmem->refcount = VN_REFCOUNT_INIT(1);
      return cached_shmem;
   }

   size_t size_aligned = align(size, 16 * 1024);
   void *ptr = aligned_alloc(16*1024, size_aligned);
   if (!ptr)
      return NULL;

   uint32_t res_id = webroguegfx_vulkan_create_blob(ptr, size_aligned, 0);
   if (!res_id) {
      free(ptr);
      return NULL;
   }

   struct webrogue_shmem *shmem =
      util_sparse_array_get(&webrogue->shmem_array, res_id);
   *shmem = (struct webrogue_shmem){
      .base = {
         .refcount = VN_REFCOUNT_INIT(1),
         .res_id = res_id,
         .mmap_size = size,
         .mmap_ptr = ptr,
      },
   };

   return &shmem->base;
}

static uint32_t
timeout_to_wait_timeout(uint64_t timeout)
{
   const uint64_t ns_per_ms = 1000000;
   const uint64_t ms = (timeout + ns_per_ms - 1) / ns_per_ms;
   if (!ms && timeout)
      return UINT32_MAX;
   return ms <= UINT32_MAX ? (uint32_t)ms : UINT32_MAX;
}

static VkResult
webrogue_wait(struct vn_renderer *renderer, const struct vn_renderer_wait *wait)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;
   const uint32_t flags = wait->wait_any ? WEBROGUE_SYNC_WAIT_FLAG_ANY : 0;
   const uint32_t timeout = timeout_to_wait_timeout(wait->timeout);
   mtx_lock(&webrogue->sock_mutex);
   const int32_t result =
      webrogue_vcmd_sync_wait(webrogue, flags, timeout, wait->syncs,
                              wait->sync_values, wait->sync_count);
   mtx_unlock(&webrogue->sock_mutex);

   return (VkResult)result;
}

static VkResult
webrogue_submit(struct vn_renderer *renderer,
             const struct vn_renderer_submit *submit)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;

   mtx_lock(&webrogue->sock_mutex);
   webrogue_vcmd_submit_cmd2(webrogue, submit);
   mtx_unlock(&webrogue->sock_mutex);

   return VK_SUCCESS;
}

static void
webrogue_init_renderer_info(struct webrogue *webrogue)
{
   struct vn_renderer_info *info = &webrogue->base.info;

   info->pci.vendor_id = WEBROGUE_PCI_VENDOR_ID;
   info->pci.device_id = WEBROGUE_PCI_DEVICE_ID;

   info->has_dma_buf_import = false;
   info->has_external_sync = false;
   info->has_implicit_fencing = false;
   info->has_guest_vram = false;
   info->has_sync_transport = true;

   const struct virgl_renderer_capset_venus *capset = &webrogue->capset.data;
   info->wire_format_version = capset->wire_format_version;
   info->vk_xml_version = capset->vk_xml_version;
   info->vk_ext_command_serialization_spec_version =
      capset->vk_ext_command_serialization_spec_version;
   info->vk_mesa_venus_protocol_spec_version =
      capset->vk_mesa_venus_protocol_spec_version;
   assert(capset->supports_blob_id_0);

   /* ensure vk_extension_mask is large enough to hold all capset masks */
   STATIC_ASSERT(sizeof(info->vk_extension_mask) >=
                 sizeof(capset->vk_extension_mask1));
   memcpy(info->vk_extension_mask, capset->vk_extension_mask1,
          sizeof(capset->vk_extension_mask1));

   assert(capset->allow_vk_wait_syncs);

   assert(capset->supports_multiple_timelines);
   info->max_timeline_count = webrogue->max_timeline_count;
}

static void
webrogue_destroy(struct vn_renderer *renderer,
              const VkAllocationCallbacks *alloc)
{
   struct webrogue *webrogue = (struct webrogue *)renderer;

   vn_renderer_shmem_cache_fini(&webrogue->shmem_cache);

   mtx_destroy(&webrogue->sock_mutex);
   util_sparse_array_finish(&webrogue->shmem_array);
   util_sparse_array_finish(&webrogue->bo_array);

   vk_free(alloc, webrogue);
}

static VkResult
webrogue_init_capset(struct webrogue *webrogue)
{
   webrogue->capset.id = VIRTGPU_DRM_CAPSET_VENUS;
   webrogue->capset.version = 0;

   if (!webrogue_vcmd_get_capset(webrogue, webrogue->capset.id, webrogue->capset.version,
                              &webrogue->capset.data,
                              sizeof(webrogue->capset.data))) {
      vn_log(webrogue->instance, "no venus capset");
      return VK_ERROR_INITIALIZATION_FAILED;
   }

   return VK_SUCCESS;
}

static VkResult
webrogue_init_params(struct webrogue *webrogue)
{
   uint32_t val = webroguegfx_vulkan_get_max_timeline_count();
   if (!val) {
      vn_log(webrogue->instance, "no timeline support");
      return VK_ERROR_INITIALIZATION_FAILED;
   }
   webrogue->max_timeline_count = val;

   return VK_SUCCESS;
}

static VkResult
webrogue_init(struct webrogue *webrogue)
{
   util_sparse_array_init(&webrogue->shmem_array, sizeof(struct webrogue_shmem),
                          1024);
   util_sparse_array_init(&webrogue->bo_array, sizeof(struct webrogue_bo), 1024);

   mtx_init(&webrogue->sock_mutex, mtx_plain);

   webroguegfx_vulkan_create_renderer("venus", sizeof("venus") - 1);

   VkResult result = webrogue_init_params(webrogue);
   if (result == VK_SUCCESS)
      result = webrogue_init_capset(webrogue);
   if (result != VK_SUCCESS)
      return result;

   assert(webrogue->capset.data.supports_blob_id_0);

   vn_renderer_shmem_cache_init(&webrogue->shmem_cache, &webrogue->base,
                                webrogue_shmem_destroy_now);

   webroguegfx_vulkan_context_init(webrogue->capset.id);

   webrogue_init_renderer_info(webrogue);

   webrogue->base.ops.destroy = webrogue_destroy;
   webrogue->base.ops.submit = webrogue_submit;
   webrogue->base.ops.wait = webrogue_wait;

   webrogue->base.shmem_ops.create = webrogue_shmem_create;
   webrogue->base.shmem_ops.destroy = webrogue_shmem_destroy;

   webrogue->base.bo_ops.create_from_device_memory =
      webrogue_bo_create_from_device_memory;
   webrogue->base.bo_ops.create_from_dma_buf = NULL;
   webrogue->base.bo_ops.destroy = webrogue_bo_destroy;
   webrogue->base.bo_ops.export_dma_buf = webrogue_bo_export_dma_buf;
   webrogue->base.bo_ops.export_sync_file =
      vn_renderer_bo_export_sync_file_internal;
   webrogue->base.bo_ops.map = webrogue_bo_map;
   webrogue->base.bo_ops.flush = webrogue_bo_flush;
   webrogue->base.bo_ops.invalidate = webrogue_bo_invalidate;

   webrogue->base.sync_ops.create = webrogue_sync_create;
   webrogue->base.sync_ops.create_from_syncobj = NULL;
   webrogue->base.sync_ops.destroy = webrogue_sync_destroy;
   webrogue->base.sync_ops.export_syncobj = NULL;
   webrogue->base.sync_ops.reset = webrogue_sync_reset;
   webrogue->base.sync_ops.read = webrogue_sync_read;
   webrogue->base.sync_ops.write = webrogue_sync_write;

   return VK_SUCCESS;
}

VkResult
vn_renderer_create_webrogue(struct vn_instance *instance,
                            const VkAllocationCallbacks *alloc,
                            struct vn_renderer **renderer)
{
   struct webrogue *webrogue = vk_zalloc(alloc, sizeof(*webrogue), VN_DEFAULT_ALIGN,
                                   VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
   if (!webrogue)
      return VK_ERROR_OUT_OF_HOST_MEMORY;

   webrogue->instance = instance;

   VkResult result = webrogue_init(webrogue);
   if (result != VK_SUCCESS) {
      webrogue_destroy(&webrogue->base, alloc);
      return result;
   }

   *renderer = &webrogue->base;

   return VK_SUCCESS;
}
