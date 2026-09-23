// RUN: %clangxx -fsycl-device-only -fsyntax-only -Xclang -verify -Xclang -verify-ignore-unexpected=note %s
// RUN: %clangxx %fsycl-host-only -fsyntax-only -Xclang -verify -Xclang -verify-ignore-unexpected=note %s
//
// sycl::ext::oneapi::experimental::printf is annotated with the 'format'
// attribute, so an invalid format specifier is diagnosed at compile time
// instead of aborting the device runtime (CMPLRLLVM-77469).

#include <sycl/detail/core.hpp>
#include <sycl/ext/oneapi/experimental/builtins.hpp>

using sycl::ext::oneapi::experimental::printf;

void check() {
  // Invalid conversion specifier.
  printf("%@\n", 1); // expected-warning{{invalid conversion specifier '@'}}

  // The specifiers from the bug report: both abort the device runtime.
  printf("%ls\n", "wide");     // expected-warning{{specifies type 'wchar_t *'}}
  printf("%Ls\n", "not wide"); // expected-warning{{length modifier 'L'}}

  // Argument type mismatches.
  printf("%d\n", 1.0);  // expected-warning{{argument has type 'double'}}
  printf("%s\n", 1);    // expected-warning{{argument has type 'int'}}
  printf("%d %d\n", 1); // expected-warning{{more '%' conversions than data}}

  // Valid: no diagnostic.
  printf("%d %s %f %p %c %%\n", 1, "str", 1.0, (void *)0, 'c');
  printf("%lu\n", 1ul);

#ifdef __SYCL_DEVICE_ONLY__
  // OpenCL C vector specifiers are valid in device code and must not warn.
  using ocl_int4 = int __attribute__((ext_vector_type(4)));
  using ocl_float4 = float __attribute__((ext_vector_type(4)));
  printf("%v4d\n", ocl_int4{1, 2, 3, 4});
  printf("%v4hld\n", ocl_int4{1, 2, 3, 4});
  printf("%v4hlf\n", ocl_float4{1.f, 2.f, 3.f, 4.f});
#endif
}
