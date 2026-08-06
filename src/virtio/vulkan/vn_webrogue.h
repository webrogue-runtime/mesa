
#ifndef VN_WEBROGUE_H
#define VN_WEBROGUE_H

#include "vn_common.h"

struct vn_surface {
   struct vn_object_base base;
};
VK_DEFINE_NONDISP_HANDLE_CASTS(vn_surface,
                                base.vk,
                                VkSurfaceKHR,
                                VK_OBJECT_TYPE_SURFACE_KHR)

#endif /* VN_WEBROGUE_H */
