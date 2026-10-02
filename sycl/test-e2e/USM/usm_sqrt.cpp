// RUN: %{build} -o %t.out
// RUN: %{run} %t.out
// XFAIL: level_zero

#include <sycl/sycl.hpp>

#include <cmath>
#include <cstdlib>

int main() {
  sycl::queue Q{sycl::gpu_selector_v};
  constexpr size_t N = 1024;
  float *Data = sycl::malloc_shared<float>(N, Q);
  for (size_t I = 0; I < N; ++I)
    Data[I] = static_cast<float>(I) * 0.1f;

  Q.parallel_for(sycl::range<1>{N},
                 [=](sycl::id<1> Id) { Data[Id] = sycl::sqrt(Data[Id]); });

  for (size_t I = 0; I < N; ++I) {
    if (Data[I] != std::sqrt(static_cast<float>(I) * 0.1f))
      exit(1);
  }
  sycl::free(Data, Q);
  return 0;
}
