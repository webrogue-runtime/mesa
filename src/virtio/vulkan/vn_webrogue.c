#include "vn_webrogue.h"

#include "vn_device.h"
#include "vn_instance.h"
#include "vn_physical_device.h"
#include "venus-protocol/vn_protocol_driver_surface.h"
#include "venus-protocol/vn_protocol_driver_device.h"
#include "venus-protocol/vn_protocol_driver_queue.h"
#include "venus-protocol/vn_protocol_driver_transport.h"

struct vn_swapchain {
   struct vn_object_base base;
   VkSwapchainKHR handle;
};

VK_DEFINE_NONDISP_HANDLE_CASTS(vn_swapchain,
                               base.vk,
                               VkSwapchainKHR,
                               VK_OBJECT_TYPE_SWAPCHAIN_KHR)

VKAPI_ATTR VkResult VKAPI_CALL
vn_CreateSwapchainKHR(VkDevice device,
                      const VkSwapchainCreateInfoKHR *pCreateInfo,
                      const VkAllocationCallbacks *pAllocator,
                      VkSwapchainKHR *pSwapchain)
{
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.vk.alloc;
   struct vn_swapchain *chain =
      vk_zalloc(alloc, sizeof(*chain), VN_DEFAULT_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!chain)
      return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vn_object_base_init(&chain->base, VK_OBJECT_TYPE_SWAPCHAIN_KHR, &dev->base);
   vn_object_set_id(chain, vn_get_next_obj_id(), VK_OBJECT_TYPE_SWAPCHAIN_KHR);
   chain->handle = vn_swapchain_to_handle(chain);

   VkResult result = vn_call_vkCreateSwapchainKHR(
      dev->primary_ring, device, pCreateInfo, pAllocator, &chain->handle);
   if (result != VK_SUCCESS) {
      vn_object_base_fini(&chain->base);
      vk_free(alloc, chain);
      return vn_error(dev->instance, result);
   }

   *pSwapchain = chain->handle;
   return result;
}

VKAPI_ATTR void VKAPI_CALL
vn_DestroySwapchainKHR(VkDevice device,
                       VkSwapchainKHR swapchain,
                       const VkAllocationCallbacks *pAllocator)
{
   struct vn_device *dev = vn_device_from_handle(device);
   struct vn_swapchain *chain = vn_swapchain_from_handle(swapchain);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &dev->base.vk.alloc;
   if (!chain)
      return;

   struct vn_ring_submit_command submit;
   vn_submit_vkDestroySwapchainKHR(dev->primary_ring, 0, device, swapchain,
                                   pAllocator, &submit);
   if (submit.ring_seqno_valid)
      vn_ring_wait_seqno(dev->primary_ring, submit.ring_seqno);

   vn_object_base_fini(&chain->base);
   vk_free(alloc, chain);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_AcquireNextImage2KHR(VkDevice device,
                        const VkAcquireNextImageInfoKHR *pAcquireInfo,
                        uint32_t *pImageIndex)
{
   struct vn_device *dev = vn_device_from_handle(device);
   return vn_call_vkAcquireNextImage2KHR(dev->primary_ring, device,
                                         pAcquireInfo, pImageIndex);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetSwapchainImagesKHR(VkDevice device,
                         VkSwapchainKHR swapchain,
                         uint32_t *pSwapchainImageCount,
                         VkImage *pSwapchainImages)
{
   struct vn_device *dev = vn_device_from_handle(device);
   const VkAllocationCallbacks *alloc = &dev->base.vk.alloc;
   struct vn_image **images = NULL;
   uint32_t image_count = pSwapchainImages ? *pSwapchainImageCount : 0;

   if (pSwapchainImages && image_count) {
      images = vk_zalloc(alloc, sizeof(*images) * image_count,
                         VN_DEFAULT_ALIGN, VK_SYSTEM_ALLOCATION_SCOPE_COMMAND);
      if (!images)
         return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);

      for (uint32_t i = 0; i < image_count; i++) {
         images[i] = vk_zalloc(alloc, sizeof(*images[i]), VN_DEFAULT_ALIGN,
                               VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
         if (!images[i]) {
            for (uint32_t j = 0; j < i; j++) {
               vk_image_finish(&images[j]->base.vk);
               vk_free(alloc, images[j]);
            }
            vk_free(alloc, images);
            return vn_error(dev->instance, VK_ERROR_OUT_OF_HOST_MEMORY);
         }

         vk_object_base_init(&dev->base.vk, &images[i]->base.vk.base,
                             VK_OBJECT_TYPE_IMAGE);
         vn_object_set_id(images[i], vn_get_next_obj_id(),
                          VK_OBJECT_TYPE_IMAGE);
         pSwapchainImages[i] = vn_image_to_handle(images[i]);
      }
   }

   VkResult result = vn_call_vkGetSwapchainImagesKHR(
      dev->primary_ring, device, swapchain, pSwapchainImageCount,
      pSwapchainImages);
   if (result != VK_SUCCESS && result != VK_INCOMPLETE && images) {
      for (uint32_t i = 0; i < image_count; i++) {
         vk_image_finish(&images[i]->base.vk);
         vk_free(alloc, images[i]);
      }
   }
   vk_free(alloc, images);
   return result;
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo)
{
   VK_FROM_HANDLE(vk_queue, queue_vk, queue);
   struct vn_device *dev = vn_device_from_vk(queue_vk->base.device);
   return vn_call_vkQueuePresentKHR(dev->primary_ring, queue, pPresentInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                      uint32_t queueFamilyIndex,
                                      VkSurfaceKHR surface,
                                      VkBool32 *pSupported)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);
   struct vn_ring *ring = physical_dev->instance->ring.ring;

   return vn_call_vkGetPhysicalDeviceSurfaceSupportKHR(
      ring, physicalDevice, queueFamilyIndex, surface, pSupported);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                       VkSurfaceKHR surface,
                                       uint32_t *pSurfaceFormatCount,
                                       VkSurfaceFormatKHR *pSurfaceFormats)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);
   struct vn_ring *ring = physical_dev->instance->ring.ring;

   return vn_call_vkGetPhysicalDeviceSurfaceFormatsKHR(
      ring, physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetPhysicalDeviceSurfaceFormats2KHR(VkPhysicalDevice physicalDevice,
                                        const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
                                        uint32_t *pSurfaceFormatCount,
                                        VkSurfaceFormat2KHR *pSurfaceFormats)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);
   struct vn_ring *ring = physical_dev->instance->ring.ring;

   return vn_call_vkGetPhysicalDeviceSurfaceFormats2KHR(
      ring, physicalDevice, pSurfaceInfo, pSurfaceFormatCount, pSurfaceFormats);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetPhysicalDeviceSurfaceCapabilitiesKHR(
   VkPhysicalDevice physicalDevice,
   VkSurfaceKHR surface,
   VkSurfaceCapabilitiesKHR *pSurfaceCapabilities)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);
   struct vn_ring *ring = physical_dev->instance->ring.ring;

   return vn_call_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      ring, physicalDevice, surface, pSurfaceCapabilities);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetPhysicalDeviceSurfaceCapabilities2KHR(
   VkPhysicalDevice physicalDevice,
   const VkPhysicalDeviceSurfaceInfo2KHR *pSurfaceInfo,
   VkSurfaceCapabilities2KHR *pSurfaceCapabilities)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);
   struct vn_ring *ring = physical_dev->instance->ring.ring;

   return vn_call_vkGetPhysicalDeviceSurfaceCapabilities2KHR(
      ring, physicalDevice, pSurfaceInfo, pSurfaceCapabilities);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_GetPhysicalDeviceSurfacePresentModesKHR(
   VkPhysicalDevice physicalDevice,
   VkSurfaceKHR surface,
   uint32_t *pPresentModeCount,
   VkPresentModeKHR *pPresentModes)
{
   struct vn_physical_device *physical_dev =
      vn_physical_device_from_handle(physicalDevice);
   struct vn_ring *ring = physical_dev->instance->ring.ring;

   return vn_call_vkGetPhysicalDeviceSurfacePresentModesKHR(
      ring, physicalDevice, surface, pPresentModeCount, pPresentModes);
}

VKAPI_ATTR VkResult VKAPI_CALL
vn_CreateSurfaceWEBROGUE(VkInstance _instance,
                         const VkSurfaceCreateInfoWEBROGUE *pCreateInfo,
                         const VkAllocationCallbacks *pAllocator,
                         VkSurfaceKHR *pSurface)
{
   struct vn_instance *instance = vn_instance_from_handle(_instance);
   const VkAllocationCallbacks *alloc =
      pAllocator ? pAllocator : &instance->base.vk.alloc;

   struct vn_surface *surface =
      vk_zalloc(alloc, sizeof(*surface), VN_DEFAULT_ALIGN,
                VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
   if (!surface)
      return vn_error(instance, VK_ERROR_OUT_OF_HOST_MEMORY);

   vk_object_base_instance_init(&instance->base.vk, &surface->base.vk,
                                VK_OBJECT_TYPE_SURFACE_KHR);
   vn_object_set_id(surface, vn_get_next_obj_id(),
                    VK_OBJECT_TYPE_SURFACE_KHR);

   VkSurfaceKHR surface_handle = vn_surface_to_handle(surface);
   const VkResult result =
      vn_call_vkCreateSurfaceWEBROGUE(instance->ring.ring, _instance,
                                      pCreateInfo, NULL, &surface_handle);
   if (result != VK_SUCCESS) {
      vn_object_base_fini(&surface->base);
      vk_free(alloc, surface);
      return vn_error(instance, result);
   }

   *pSurface = surface_handle;

   return VK_SUCCESS;
}
