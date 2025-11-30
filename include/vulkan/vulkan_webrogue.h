#ifndef VULKAN_WEBROGUE_H_
#define VULKAN_WEBROGUE_H_ 1

/*
** Copyright 2015-2025 The Khronos Group Inc.
**
** SPDX-License-Identifier: Apache-2.0
*/

/*
** This header is generated from the Khronos Vulkan XML API Registry.
**
*/


#ifdef __cplusplus
extern "C" {
#endif



// VK_WEBROGUE_surface is a preprocessor guard. Do not pass it to API calls.
#define VK_WEBROGUE_surface 1
#define VK_WEBROGUE_SURFACE_SPEC_VERSION  1
#define VK_WEBROGUE_SURFACE_EXTENSION_NAME "VK_WEBROGUE_surface"
typedef VkFlags VkSurfaceCreateFlagsWEBROGUE;
typedef struct VkSurfaceCreateInfoWEBROGUE {
    VkStructureType                 sType;
    const void*                     pNext;
    VkSurfaceCreateFlagsWEBROGUE    flags;
    void*                           window;
} VkSurfaceCreateInfoWEBROGUE;

typedef VkResult (VKAPI_PTR *PFN_vkCreateSurfaceWEBROGUE)(VkInstance instance, const VkSurfaceCreateInfoWEBROGUE* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);

#ifndef VK_NO_PROTOTYPES
#ifndef VK_ONLY_EXPORTED_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSurfaceWEBROGUE(
    VkInstance                                  instance,
    const VkSurfaceCreateInfoWEBROGUE*          pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface);
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif
