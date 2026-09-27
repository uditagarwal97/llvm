// Moving a sampled_image/unsampled_image must leave the moved-from object's
// destructor with nothing to do rather than dereferencing its null impl.
// RUN: %{build} -o %t.out
// RUN: %{run-unfiltered-devices} %t.out

#include <cassert>
#include <sycl/sycl.hpp>
#include <utility>

int main() {
  sycl::unsampled_image<1> UImg(sycl::image_format::r8g8b8a8_unorm,
                                sycl::range<1>{4});
  sycl::unsampled_image<1> UMoved(std::move(UImg));
  assert(UImg != UMoved && "move should have detached the source impl");

  std::shared_ptr<const void> HostPtr(new unsigned char[16],
                                      std::default_delete<unsigned char[]>());
  sycl::sampled_image<1> SImg(
      HostPtr, sycl::image_format::r8g8b8a8_unorm,
      sycl::image_sampler{sycl::addressing_mode::clamp,
                          sycl::coordinate_normalization_mode::unnormalized,
                          sycl::filtering_mode::nearest},
      sycl::range<1>{4});
  sycl::sampled_image<1> SMoved(std::move(SImg));
  assert(SImg != SMoved && "move should have detached the source impl");
  return 0;
}
