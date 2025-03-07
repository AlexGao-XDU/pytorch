#include <ATen/native/vulkan/ops/Common.h>
#include <torch/library.h>

namespace at {
namespace native {
namespace vulkan {
namespace ops {
namespace {

using namespace api::utils;

Tensor select_depth(const Tensor& input_arg, uint32_t index) {
  api::Context* const context = api::context();

  const Tensor input = input_arg.is_vulkan() ? input_arg : input_arg.vulkan();
  const vTensor& v_input = convert(input);
  const IntArrayRef v_input_sizes = v_input.sizes();

  vTensor v_output{
      context,
      {v_input_sizes[1], v_input_sizes[2]},
      input_arg.scalar_type(),
  };

  const struct Block final {
    uvec3 size; // output texture size
    uint32_t index;
  } block{v_output.extents(), index};

  api::UniformParamsBuffer params(context, block);
  api::PipelineBarrier pipeline_barrier{};

  context->submit_compute_job(
      // shader descriptor
      VK_KERNEL(select_depth),
      // pipeline barrier
      pipeline_barrier,
      // global work group size
      v_output.extents(),
      // local work group size
      adaptive_work_group_size(v_output.extents()),
      // fence handle
      VK_NULL_HANDLE,
      // shader arguments
      v_output.image(
          pipeline_barrier,
          api::PipelineStage::COMPUTE,
          api::MemoryAccessType::WRITE),
      v_input.image(pipeline_barrier, api::PipelineStage::COMPUTE),
      // params buffer
      params.buffer());

  return convert(v_output);
}

Tensor select_height(const Tensor& input_arg, uint32_t index) {
  api::Context* const context = api::context();

  const Tensor input = input_arg.is_vulkan() ? input_arg : input_arg.vulkan();
  const vTensor& v_input = convert(input);
  const IntArrayRef v_input_sizes = v_input.sizes();

  vTensor v_output{
      context,
      {v_input_sizes[0], v_input_sizes[2]},
      input_arg.scalar_type(),
  };

  const struct Block final {
    uvec3 size; // output texture size
    uint32_t index;
  } block{v_output.extents(), index};

  // Input tensor is a (c, h, w)
  // Output tensor is a (c, w)
  // In shader, the input texture's coordinate is (w, h, c)
  // In shader, the output texture's coordinate is (w, c, 1)
  uint32_t w = v_output.extents().data[0u];
  uint32_t c = v_output.extents().data[1u];

  // Encoding of c-channel is packed into texel, hence we only call ceil(c/4)
  // times to minimize invocation and read.
  // For the last dimension, it is the selected height. Shader will do a direct
  // lookup based on block.index.
  uvec3 global_workgroup_size{w, api::utils::div_up(c, 4u), 1};

  api::UniformParamsBuffer params(context, block);
  api::PipelineBarrier pipeline_barrier{};

  context->submit_compute_job(
      // shader descriptor
      VK_KERNEL(select_height),
      // pipeline barrier
      pipeline_barrier,
      // global work group size
      global_workgroup_size,
      // local work group size
      adaptive_work_group_size(global_workgroup_size),
      // fence handle
      VK_NULL_HANDLE,
      // shader arguments
      v_output.image(
          pipeline_barrier,
          api::PipelineStage::COMPUTE,
          api::MemoryAccessType::WRITE),
      v_input.image(pipeline_barrier, api::PipelineStage::COMPUTE),
      // params buffer
      params.buffer());

  return convert(v_output);
}

Tensor select_width(const Tensor& input_arg, uint32_t index) {
  api::Context* const context = api::context();

  const Tensor input = input_arg.is_vulkan() ? input_arg : input_arg.vulkan();
  const vTensor& v_input = convert(input);
  const IntArrayRef v_input_sizes = v_input.sizes();

  vTensor v_output{
      context,
      {v_input_sizes[0], v_input_sizes[1]},
      input_arg.scalar_type(),
  };

  const struct Block final {
    uvec3 size; // output texture size
    uint32_t index;
  } block{v_output.extents(), index};

  // Input tensor is a (c, h, w)
  // Output tensor is a (c, h)
  // In shader, the input texture's coordinate is (w, h, c)
  // In shader, the output texture's coordinate is (h, c, 1)
  uint32_t h = v_output.extents().data[0u];
  uint32_t c = v_output.extents().data[1u];

  // Encoding of c-channel is packed into texel, hence we only call ceil(c/4)
  // times to minimize invocation and read.
  // For the last dimension, it is the selected width. Shader will do a direct
  // lookup based on block.index.
  uvec3 global_workgroup_size{h, api::utils::div_up(c, 4u), 1};

  api::UniformParamsBuffer params(context, block);
  api::PipelineBarrier pipeline_barrier{};

  context->submit_compute_job(
      // shader descriptor
      VK_KERNEL(select_width),
      // pipeline barrier
      pipeline_barrier,
      // global work group size
      global_workgroup_size,
      // local work group size
      adaptive_work_group_size(global_workgroup_size),
      // fence handle
      VK_NULL_HANDLE,
      // shader arguments
      v_output.image(
          pipeline_barrier,
          api::PipelineStage::COMPUTE,
          api::MemoryAccessType::WRITE),
      v_input.image(pipeline_barrier, api::PipelineStage::COMPUTE),
      // params buffer
      params.buffer());

  return convert(v_output);
}

Tensor select(const Tensor& self, int64_t dim, int64_t index) {
  TORCH_CHECK(self.dim() == 3, "Vulkan select only supports 3d tensors!");
  TORCH_CHECK(
      0 <= dim && dim <= 2,
      "Vulkan select only supports one of the dim (0, 1, 2)");

  const int64_t size = self.size(dim);

  if (index < -size || index >= size) {
    TORCH_CHECK_INDEX(
        false,
        "select(): index ",
        index,
        " out of range for tensor of size ",
        self.sizes(),
        " at dimension ",
        dim);
  }
  if (index < 0) {
    index += size;
  }

  if (dim == 0) {
    return select_depth(self, index);
  } else if (dim == 1) {
    return select_height(self, index);
  } else {
    return select_width(self, index);
  }
}

#ifdef USE_VULKAN_API

TORCH_LIBRARY_IMPL(aten, Vulkan, m) {
  m.impl(TORCH_SELECTIVE_NAME("aten::select.int"), TORCH_FN(select));
}

#endif /* USE_VULKAN_API */

} // namespace
} // namespace ops
} // namespace vulkan
} // namespace native
} // namespace at
