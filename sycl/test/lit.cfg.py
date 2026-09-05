# -*- Python -*-

import os
import platform
import re
import subprocess
import tempfile

import lit.formats
import lit.util

from lit.llvm import llvm_config

# Configuration file for the 'lit' test runner.

# name: The name of this test suite.
config.name = "SYCL"

# testFormat: The test format to use to interpret tests.
#
# For now we require '&&' between commands, until they get globally killed and
# the test runner updated.
config.test_format = lit.formats.ShTest()

# suffixes: A list of file extensions to treat as test files.
dump_only_tests = bool(lit_config.params.get("SYCL_LIB_DUMPS_ONLY", False))
if dump_only_tests:
    config.suffixes = [".dump"]  # Only run dump testing
else:
    config.suffixes = [
        ".c",
        ".cpp",
        ".dump",
        ".test",
    ]  # add .spv. Currently not clear what to do with those

# feature tests are considered not so lightweight, so, they are excluded by default
config.excludes = ["Inputs", "feature-tests"]

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# allow expanding substitutions that are based on other substitutions
config.recursiveExpansionLimit = 10

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(config.sycl_obj_root, "test")

# Propagate some variables from the host environment.
llvm_config.with_system_environment(
    ["PATH", "OCL_ICD_FILENAMES", "SYCL_DEVICE_ALLOWLIST", "SYCL_CONFIG_FILE_NAME"]
)

config.substitutions.append(("%python", '"%s"' % (sys.executable)))
config.substitutions.append(
    ("%dpclangxx", os.path.join(config.install_bin_dir, "dpclang++"))
)

# Propagate extra environment variables
if config.extra_environment:
    lit_config.note("Extra environment variables")
    for env_pair in config.extra_environment.split(","):
        [var, val] = env_pair.split("=")
        if val:
            llvm_config.with_environment(var, val)
            lit_config.note("\t" + var + "=" + val)
        else:
            lit_config.note("\tUnset " + var)
            llvm_config.with_environment(var, "")

# Tests are not compiled with -fsanitize=address (that would also instrument
# device code and change what the check_device_code tests match), so a host
# binary loading a sanitized libsycl needs the ASan runtime loaded first.
# Preload it, and ignore the leaks of the system tools a RUN line invokes, which
# get leak-checked as a side effect of being started by a preloaded process.
# A test that sets LD_PRELOAD itself must prepend %{asan_preload} to keep the
# runtime first; it expands to nothing in a regular build.
AsanPreload = ""
if "Address" in getattr(config, "llvm_use_sanitizer", ""):
    AsanRuntime = subprocess.check_output(
        [config.host_cxx, "-print-file-name=libclang_rt.asan-x86_64.so"],
        text=True,
    ).strip()
    if os.path.isfile(AsanRuntime):
        AsanPreload = AsanRuntime + ":"
        llvm_config.with_environment("LD_PRELOAD", AsanRuntime)
        llvm_config.with_environment(
            "LSAN_OPTIONS",
            "suppressions="
            + os.path.join(config.test_source_root, "lsan_suppressions.txt"),
        )
    else:
        lit_config.warning(
            "sanitized build, but no ASan runtime found: {}".format(AsanRuntime)
        )
config.substitutions.append(("%{asan_preload}", AsanPreload))

# UBSan prints a diagnostic and keeps going by default, so undefined behaviour in
# the runtime would leave the test passing with the report buried in output lit
# discards. Make it abort instead, so a finding fails the test that hit it.
if "Undefined" in getattr(config, "llvm_use_sanitizer", ""):
    llvm_config.with_environment("UBSAN_OPTIONS", "halt_on_error=1,print_stacktrace=1")

# If major release preview library is enabled we can enable the feature.
if config.sycl_preview_lib_enabled == "ON":
    config.available_features.add("preview-breaking-changes-supported")

# Configure LD_LIBRARY_PATH or corresponding os-specific alternatives
# Add 'libcxx' feature to filter out all SYCL abi tests when SYCL runtime
# is built with llvm libcxx. This feature is added for Linux only since MSVC
# CL compiler doesn't support to use llvm libcxx instead of MSVC STL.
if platform.system() == "Linux":
    config.available_features.add("linux")
    if config.sycl_use_libcxx == "ON":
        config.available_features.add("libcxx")
    llvm_config.with_system_environment("LD_LIBRARY_PATH")
    llvm_config.with_environment(
        "LD_LIBRARY_PATH", config.sycl_libs_dir, append_path=True
    )

elif platform.system() == "Windows":
    config.available_features.add("windows")
    llvm_config.with_system_environment("LIB")
    llvm_config.with_environment("LIB", config.sycl_libs_dir, append_path=True)

elif platform.system() == "Darwin":
    # FIXME: surely there is a more elegant way to instantiate the Xcode directories.
    llvm_config.with_system_environment(["C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH"])
    llvm_config.with_environment(
        "CPLUS_INCLUDE_PATH",
        "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++/v1",
        append_path=True,
    )
    llvm_config.with_environment(
        "C_INCLUDE_PATH",
        "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/",
        append_path=True,
    )
    llvm_config.with_environment(
        "CPLUS_INCLUDE_PATH",
        "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/",
        append_path=True,
    )
    llvm_config.with_environment("DYLD_LIBRARY_PATH", config.sycl_libs_dir)

llvm_config.with_environment("PATH", config.sycl_tools_dir, append_path=True)

config.substitutions.append(("%threads_lib", config.sycl_threads_lib))
config.substitutions.append(("%sycl_libs_dir", config.sycl_libs_dir))
config.substitutions.append(("%sycl_include", config.sycl_include))
config.substitutions.append(("%sycl_source_dir", config.sycl_source_dir))
config.substitutions.append(("%llvm_main_include_dir", config.llvm_main_include_dir))
config.substitutions.append(("%opencl_libs_dir", config.opencl_libs_dir))
config.substitutions.append(("%level_zero_include_dir", config.level_zero_include_dir))
config.substitutions.append(("%opencl_include_dir", config.opencl_include_dir))
config.substitutions.append(("%cuda_toolkit_include", config.cuda_toolkit_include))
config.substitutions.append(("%sycl_tools_src_dir", config.sycl_tools_src_dir))
config.substitutions.append(("%llvm_build_lib_dir", config.llvm_build_lib_dir))
config.substitutions.append(("%llvm_build_bin_dir", config.llvm_build_bin_dir))
config.substitutions.append(("%test_include_path", config.test_include_path))

llvm_symbolizer = os.path.join(config.llvm_build_bin_dir, "llvm-symbolizer")
llvm_config.with_environment("LLVM_SYMBOLIZER_PATH", llvm_symbolizer)

sycl_host_only_options = "-std=c++17 -Xclang -fsycl-is-host"
for include_dir in [
    config.sycl_include,
    config.level_zero_include_dir,
    config.opencl_include_dir,
    config.sycl_include + "/sycl/",
]:
    if include_dir:
        sycl_host_only_options += " -isystem %s" % include_dir
config.substitutions.append(("%fsycl-host-only", sycl_host_only_options))

config.substitutions.append(
    ("%sycl_lib", " -lsycl9" if platform.system() == "Windows" else "-lsycl")
)

llvm_config.add_tool_substitutions(["llvm-spirv"], [config.sycl_tools_dir])

triple = lit_config.params.get("SYCL_TRIPLE", "spir64-unknown-unknown")
lit_config.note("Triple: {}".format(triple))
config.substitutions.append(("%sycl_triple", triple))

additional_flags = config.sycl_clang_extra_flags.split(" ")

# ThreadSanitizer needs the opposite treatment. clang only ever links the TSan
# runtime statically, so a sanitized libsycl.so keeps its __tsan_* references
# undefined and an uninstrumented test executable will not even link against it.
# Link the shared runtime into the test binaries instead, so it is their first
# DT_NEEDED entry and initializes before anything else.
#
# It must NOT be preloaded the way the ASan runtime is: every RUN line runs
# tools from this build, those are instrumented and carry the static runtime,
# and a second, shared runtime on top of that kills them - starting with the
# clang that lit itself probes for its resource dir. For the same reason it must
# not be passed as a driver *input* file either: that would break the RUN lines
# producing several outputs (-fsyclbin, -fsycl-link) and would put the runtime
# inside helper libraries a test builds only in order to preload them.
if "Thread" in getattr(config, "llvm_use_sanitizer", ""):
    TsanRuntime = subprocess.check_output(
        [config.host_cxx.strip(), "-print-file-name=libclang_rt.tsan-x86_64.so"],
        text=True,
    ).strip()
    if os.path.isfile(TsanRuntime):
        llvm_config.with_environment(
            "TSAN_OPTIONS",
            "halt_on_error=0 history_size=7 second_deadlock_stack=1 suppressions="
            + os.path.join(config.test_source_root, "tsan_suppressions.txt"),
        )
        TsanRuntimeDir = os.path.dirname(TsanRuntime)
        additional_flags += [
            "-Wl,-L" + TsanRuntimeDir,
            "-Wl,-l:" + os.path.basename(TsanRuntime),
            "-Wl,-rpath," + TsanRuntimeDir,
            # The link flags above are unused on the compile-only RUN lines.
            "-Wno-unused-command-line-argument",
        ]
        # The library reference counts as a driver input, which a handful of
        # tests cannot accommodate. They mark themselves UNSUPPORTED on this.
        config.available_features.add("tsan-runtime")
    else:
        lit_config.warning(
            "sanitized build, but no TSan runtime found: {}".format(TsanRuntime)
        )

if config.cuda == "ON":
    config.available_features.add("cuda")

if config.hip == "ON":
    config.available_features.add("hip")

if config.opencl == "ON":
    config.available_features.add("opencl")

if config.level_zero == "ON":
    config.available_features.add("level_zero")

if config.native_cpu == "ON":
    config.available_features.add("native_cpu")

if config.native_cpu_ock == "ON":
    config.available_features.add("native_cpu_ock")

if "nvptx64-nvidia-cuda" in triple:
    llvm_config.with_system_environment("CUDA_PATH")
    config.available_features.add("cuda")

if "amdgcn-amd-amdhsa" in triple:
    llvm_config.with_system_environment("ROCM_PATH")
    config.available_features.add("hip")
    # For AMD the specific GPU has to be specified with --offload-arch
    if not any([f.startswith("--offload-arch") for f in additional_flags]):
        # If the offload arch wasn't specified in SYCL_CLANG_EXTRA_FLAGS,
        # hardcode it to gfx90a, this is fine because only compiler tests
        additional_flags += [
            "-Xsycl-target-backend=amdgcn-amd-amdhsa",
            "--offload-arch=gfx90a",
        ]

config.sycl_headers_filter = lit_config.params.get("SYCL_HEADERS_FILTER", None)
if config.sycl_headers_filter is not None:
    lit_config.note(
        "SYCL_HEADERS_FILTER param is set to '{}', it will impact amount of tests discovered within self-contained-headers sub-suite".format(
            config.sycl_headers_filter
        )
    )

# Disable the UR logger callback sink during test runs as output to SYCL RT can interfere with some tests relying on standard input/output
llvm_config.with_environment("UR_LOG_CALLBACK", "disabled")

# Dump-only tests do not have clang available
if not dump_only_tests:
    llvm_config.use_clang(additional_flags=additional_flags)

    # Detect libstdc++ behavior that %clangxx picks up on this host, so tests
    # can be gated on actual libstdc++ properties instead of a nonexistent
    # "gcc11"-style feature. Modeled after libcxx's compilerMacros() in
    # libcxx/utils/libcxx/test/dsl.py.
    def get_predefined_macros(include, std=None):
        probe = tempfile.NamedTemporaryFile(suffix=".cpp", delete=False)
        try:
            probe.write(("#include <%s>\n" % include).encode())
            probe.close()
            cmd = [config.clang, "-dM", "-E", probe.name]
            if std:
                cmd.insert(1, "-std=%s" % std)
            return subprocess.check_output(cmd, stderr=subprocess.DEVNULL).decode()
        except (subprocess.CalledProcessError, OSError):
            return ""
        finally:
            os.unlink(probe.name)

    if platform.system() == "Linux":
        match = re.search(
            r"#define _GLIBCXX_RELEASE (\d+)", get_predefined_macros("version")
        )
        if match and int(match.group(1)) >= 11:
            config.available_features.add("glibcxx-ge-11")

        # negative_test.cpp expects __cpp_lib_span-gated diagnostics; older
        # libstdc++ versions don't provide <span>, so __cpp_lib_span stays
        # undefined even with -std=c++20 and the expected-error annotations
        # under "#ifdef __cpp_lib_span" never get compiled in.
        if "#define __cpp_lib_span" in get_predefined_macros("span", std="c++20"):
            config.available_features.add("cpp_lib_span")

# Set timeout for test = 10 mins
try:
    import psutil

    config.maxIndividualTestTime = 600
except ImportError:
    pass
