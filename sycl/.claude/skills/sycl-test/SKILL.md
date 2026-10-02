---
name: sycl-test
description: Add or review a SYCL test in this repo. Picks the right tier (gtest unit test in sycl/unittests, LIT test in sycl/test, E2E test in sycl/test-e2e), enforces the process that makes a test trustworthy (search existing coverage, write, prove it can fail), and lists the repo-specific traps CI lints and the UR mock will catch. Use whenever asked to add, fix, extend, or review a SYCL test, or when a runtime/header change needs coverage.
---

# SYCL tests: add or review

The sibling tests and headers are the best reference for mechanics; read them. This file
covers only what they do not tell you: which tier, what process, and which traps.

## Tier

| The claim is about... | Tier |
|---|---|
| A kernel's result on a device, data movement, backend behaviour, interop, any `__SYCL_DEVICE_ONLY__` branch | **E2E** `sycl/test-e2e/` (the only tier that runs device code) |
| Host runtime behaviour toward the backend: which UR calls, flags, wait-lists, caching, lifetime, injected UR errors, `errc`, env/config, thread-safety | **Unit** `sycl/unittests/` (gtest + `UrMock`, host compiler, no hardware; cannot see kernel results) |
| Headers at compile time: `static_assert`, traits, a diagnostic that must or must not fire, device IR, warning cleanliness, ABI, feature macros | **LIT** `sycl/test/` (`check-sycl`; never runs device code) |

Host-computable header logic (`id`, `range`) may be a LIT compile+run test only if host and
device paths are identical; otherwise E2E with the host result as oracle.

## Process

1. **Search before writing.** One grep of the tier's tree for the API, UR entry point, or
   operator, then decide; don't audit the whole suite. If coverage exists, say so and stop, or
   extend that file with one representative case per new code path.
2. **Write.** Copy the nearest sibling's header/fixture shape. No license or copyright
   header in tests. Open with a 1–3 line comment saying what is checked and why, with the
   GitHub issue link if there is one; otherwise comment only what is not obvious from the code.
   LLVM naming (`PascalCase` locals, `camelCase` functions, `sycl::` only); the file's own
   idiom wins. Always run the build's `clang-format -i` on every file you touched.
   Never write absolute paths or Intel-internal links into a test. A JIRA id may appear as
   plain text (no link) only when the user explicitly asks for it.
3. **Prove it can fail.** Run it, then flip an expected value (or break the callback / CHECK
   line) and confirm it fails. Restore. A test that cannot fail is not done.
4. **Run targets** (`<build>` = DPC++ build dir, run from repo root):
   unit `ninja -C <build> check-sycl-unittests` (sets the mock library path and config env;
   never run binaries by hand) · LIT `<build>/bin/llvm-lit -v sycl/test/<path>.cpp` ·
   E2E `<build>/bin/llvm-lit -v --param sycl_devices="level_zero:gpu" sycl/test-e2e/<path>.cpp`
   (devices via `<build>/bin/sycl-ls`; never blank `ONEAPI_DEVICE_SELECTOR`).
5. **Report** in one line: tier and why; then the command, its result, and that the mutation
   check fired; any gates the test needs.

## Traps (CI lints or runtime behaviour that siblings do not explain)

**E2E**
  Use `<sycl/detail/core.hpp>` plus fine-grained headers (`<sycl/usm.hpp>`, `<sycl/builtins.hpp>`).
- Every `XFAIL:` must be followed on the next line by `// XFAIL-TRACKER: <GitHub issue URL | PROJ-123>`;
  every `UNSUPPORTED:` (including `UNSUPPORTED: true`) by `// UNSUPPORTED-TRACKER: <id>` or
  `// UNSUPPORTED-INTENDED: <reason>`. Flaky ⇒ UNSUPPORTED + tracker, not XFAIL.
- `%{build}` already injects `-fsycl-targets` and `-Werror`; `%{run}` runs once per device
  with `ONEAPI_DEVICE_SELECTOR` set, so use `sycl::queue Q;`, never a hard-coded selector.
  Prefer `REQUIRES: aspect-<x>` or a runtime `Dev.has(aspect::x)` skip over `REQUIRES: gpu`.
  Compile-time gates use `target-*`; runtime-only XFAIL includes `run-mode`.
- One unique `%t<name>.out` per build line. Float compares need tolerance (`ulp_utils.hpp`,
  `DeviceLib/math_utils.hpp`). `wait()` before reading USM. `assert`/nonzero return, not `exit(1)`.
- Leak check: `%{l0_leak_check} %{run} %t.out 2>&1 | FileCheck %s --implicit-check-not=LEAK`.
- Build and run happen separately, possibly on different machines and OSes: no absolute
  paths, no assumptions about the build machine. The compiler may be `clang-cl` (Windows) or
  `clang++` (Linux), so express flags through LIT substitutions (`%O0`, `%debug_option`,
  `%fPIC`, `%shared_lib`, `%if cl_options %{/clang:-x%} %else %{-x%}`), never raw GCC-only
  flags. No Linux- or Windows-specific shell commands in RUN lines; if unavoidable, gate with
  `%if linux`/`%if windows` and put non-binary run-stage steps under `%{run-aux}`.
- Graph tests follow the existing structure: shared body in `Graph/Inputs/<name>.cpp` using
  `graph_common.hpp`, thin wrappers in `Graph/Explicit/` and `Graph/RecordReplay/` that define
  `GRAPH_E2E_EXPLICIT` / `GRAPH_E2E_RECORD_REPLAY` and include the body.

**Unit**
- `sycl::unittest::UrMock<> Mock;` first in the test, one per test; it loads only the mock
  adapter (one platform, one fake GPU). Needed only when overriding or inspecting UR calls.
- The mock's defaults (`urDeviceGetInfo`, `urDeviceGet`, ...) are `replace` callbacks, so a bare
  `set_replace_callback` of yours discards every canned answer. Patch fixed-size values with
  `set_after_callback`; for strings or size changes (the default asserts the buffer size),
  `replace` and forward other queries to `sycl::unittest::MockAdapter::mock_urDeviceGetInfo(pParams)`.
  Info queries are two-phase: write `**P.ppPropSizeRet` when `*P.ppPropValue` is null, copy otherwise.
- `~UrMock` resets callbacks, not your statics; reset counters at the top of each test.
- `OBJECT` linking (the default) statically links the runtime objects, so `<detail/*.hpp>` and
  non-exported internals are usable. New files go in the directory's `add_sycl_unittest(...)` list.
- Kernels without a device compiler: `#include <helpers/TestKernel.hpp>`, or
  `MOCK_INTEGRATION_HEADER(K)` + `MockDeviceImage`/`MockDeviceImageArray` (see `SYCL2020/KernelBundle.cpp`).

**LIT**
- IR checks need `-fsycl-device-only -S -emit-llvm`; without the first flag you get host IR.
  Pipe with `-o -` or write to `%t`; a literal output path races under parallel lit.
- Match one property with `{{.*}}` and `[[VAR:...]]` captures around a `SYCL_EXTERNAL` free
  function; full `CHECK-NEXT` dumps of mangled lambda names break on any codegen change.
  Builtins appear mangled (`@{{.*}}__spirv_AtomicLoad{{.*}}(`); autogenerate full-body checks
  with `llvm/utils/update_cc_test_checks.py --clang <build>/bin/clang++`.
- `-Xclang -verify` fails on any unexpected note: add `-Xclang -verify-ignore-unexpected=note,warning`;
  positive files need `// expected-no-diagnostics`; header-located diagnostics use `@*:*`.
- `%fsycl-host-only` has no `-fsycl`: kernels are not compiled, device diagnostics will not fire.
- `REQUIRES: linux` + `UNSUPPORTED: libcxx` only for libstdc++ layout/mangling tests (`abi/`,
  `gdb/`); plain IR checks need no OS gate.

## Review

Flag: wrong tier (device behaviour asserted by a LIT run test, runtime logic in E2E, kernel
results in a unit test); vacuous (cannot fail, no mutation evidence); redundant with a sibling; any trap above; noise (license header, missing or verbose comments, `cl::sycl::`,
unused includes, absolute paths, Intel-internal links, unrequested JIRA ids, not
clang-formatted). Otherwise approve with the tier justification.
