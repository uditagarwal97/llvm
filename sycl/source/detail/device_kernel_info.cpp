//==---------------------- device_kernel_info.cpp ----------------------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include <detail/device_kernel_info.hpp>
#include <detail/program_manager/program_manager.hpp>

#ifdef __has_include
#if __has_include(<cxxabi.h>)
#define __SYCL_ENABLE_GNU_DEMANGLING
#include <cstdlib>
#include <cxxabi.h>
#include <memory>
#endif
#endif

namespace sycl {
inline namespace _V1 {
namespace detail {

DeviceKernelInfo::DeviceKernelInfo(const CompileTimeKernelInfoTy &Info,
                                   std::optional<sycl::kernel_id> KernelID)
    : CompileTimeKernelInfoTy{Info}, MKernelID{std::move(KernelID)} {}

template <typename OtherTy>
inline constexpr bool operator==(const CompileTimeKernelInfoTy &LHS,
                                 const OtherTy &RHS) {
  // TODO replace with std::tie(...) == std::tie(...) once there is
  // implicit conversion from detail to std string_view.
  return std::string_view{LHS.Name} == std::string_view{RHS.Name} &&
         LHS.NumParams == RHS.NumParams && LHS.IsESIMD == RHS.IsESIMD &&
         std::string_view{LHS.FileName} == std::string_view{RHS.FileName} &&
         std::string_view{LHS.FunctionName} ==
             std::string_view{RHS.FunctionName} &&
         LHS.LineNumber == RHS.LineNumber &&
         LHS.ColumnNumber == RHS.ColumnNumber &&
         LHS.KernelSize == RHS.KernelSize &&
         // TODO This check fails with test_handler CTS due to what appears to
         // be a test bug. Disable it for now as a workaround.
         // See https://github.com/intel/llvm/issues/20134 for more info.
         // LHS.ParamDescGetter == RHS.ParamDescGetter &&
         LHS.HasSpecialCaptures == RHS.HasSpecialCaptures;
}

void DeviceKernelInfo::setCompileTimeInfoIfNeeded(
    const CompileTimeKernelInfoTy &Info) {
  // Called under ProgramManager::m_DeviceKernelInfoMapMutex, which protects the
  // map, not this object: the fields below are read from the kernel submission
  // path (KernelData, CGExecKernel, ...) through a reference that has already
  // escaped that mutex. So this lazy publication must stay write-once, and must
  // not rewrite a field that readers already rely on.
  if (isCompileTimeInfoSet()) {
    assert(Info == *this);
    return;
  }

  // `Name` is deliberately not copied. It is set when the map entry is created,
  // before this object is reachable, and points into the device image, while
  // `Info.Name` points at an equivalent string literal in the caller's
  // translation unit - equivalent because this entry was found by looking the
  // map up with `Info.Name` as the key. Readers that only know the kernel name
  // (ProgramManager::getDeviceKernelInfo(std::string_view),
  // tryGetDeviceKernelInfo) read `Name` without any lock and may be holding a
  // detail::string_view into it, so swapping its (pointer, length) pair under
  // them buys nothing.
  NumParams = Info.NumParams;
  IsESIMD = Info.IsESIMD;
  FileName = Info.FileName;
  FunctionName = Info.FunctionName;
  LineNumber = Info.LineNumber;
  ColumnNumber = Info.ColumnNumber;
  KernelSize = Info.KernelSize;
  HasSpecialCaptures = Info.HasSpecialCaptures;
  // Written last: this is what isCompileTimeInfoSet() tests.
  ParamDescGetter = Info.ParamDescGetter;

  assert(isCompileTimeInfoSet());
  // Also guards against a field being added to CompileTimeKernelInfoTy (and to
  // operator== above) but not to the list of fields published here.
  assert(Info == *this);
}

void DeviceKernelInfo::setImplicitLocalArgPos(int Pos) {
  assert(!MImplicitLocalArgPos.has_value() || MImplicitLocalArgPos == Pos);
  MImplicitLocalArgPos = Pos;
}

void DeviceKernelInfo::setWorkGroupDynamicLocalMem() {
  MWorkGroupDynamicLocalMem = true;
}

std::string_view DeviceKernelInfo::getDemangledName() const {
  std::call_once(MDemangledNameInitFlag, [&]() {
#ifdef __SYCL_ENABLE_GNU_DEMANGLING
    int Status = -1; // some arbitrary value to eliminate the compiler warning
    char *Demangled =
        abi::__cxa_demangle(Name.data(), nullptr, nullptr, &Status);
    if (Status == 0 && Demangled) {
      std::unique_ptr<char, void (*)(void *)> Guard(Demangled, std::free);
      MDemangledName = std::string(Guard.get());
    } else {
      MDemangledName = std::string(Name);
    }
#else
    MDemangledName = std::string(Name);
#endif
  });
  return MDemangledName;
}

} // namespace detail
} // namespace _V1
} // namespace sycl
