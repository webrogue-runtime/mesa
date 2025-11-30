files_libplatform_virtgpu_webrogue = files(
  'WebrogueVirtGpuBlob.cpp',
  'WebrogueVirtGpuBlobMapping.cpp',
  'WebrogueVirtGpuDevice.cpp',
)

libplatform_virtgpu_webrogue = static_library(
   'platform_virtgpu_webrogue',
   files_libplatform_virtgpu_webrogue,
   cpp_args: gfxstream_guest_args,
   include_directories: [inc_platform_virtgpu, inc_src, inc_include],
   dependencies: dep_webroguegfx,
)
