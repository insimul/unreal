# Insimul Unreal runtime — host-test harness (tools/verify-unreal)
#
# Compiles the PORTABLE (std-only) slice of the InsimulRuntime module — the
# parts that carry cross-runtime semantics — and runs host tests against the
# golden fixtures in packages/core/conformance. UE-coupled files (the UStruct
# boundary layer) are syntax-gated (#if WITH_ENGINE) and intentionally excluded
# here; they are verified by a human Unreal build.
#
#   cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
#
# or just run the built binary directly: ./build/insimul_verify_unreal

cmake_minimum_required(VERSION 3.16)
project(insimul_verify_unreal CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Portable runtime-core sources (must stay UE-free).
set(RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Source/InsimulRuntime")
set(PORTABLE_SOURCES
    "${RUNTIME_DIR}/Portable/InsimulJson.cpp"
    "${RUNTIME_DIR}/Portable/InsimulRadiantSource.cpp"
    "${RUNTIME_DIR}/Portable/InsimulWorldSource.cpp"
    "${RUNTIME_DIR}/Portable/InsimulSha256.cpp"
    "${RUNTIME_DIR}/Portable/InsimulCanonicalJson.cpp"
    "${RUNTIME_DIR}/Portable/InsimulSaveSystem.cpp"
    "${RUNTIME_DIR}/Portable/InsimulQuestSystem.cpp"
    "${RUNTIME_DIR}/Portable/InsimulContentLibrary.cpp"
    "${RUNTIME_DIR}/Portable/InsimulBootstrap.cpp"
)

# Golden fixtures shipped by packages/core (semantics authority).
set(FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../conformance/saves")
if(NOT EXISTS "${FIXTURE_DIR}")
  set(FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core/conformance/saves")
endif()
set(QUESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../conformance/quests")
if(NOT EXISTS "${QUESTS_DIR}")
  set(QUESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core/conformance/quests")
endif()
set(CONTENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../conformance/content")
if(NOT EXISTS "${CONTENT_DIR}")
  set(CONTENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core/conformance/content")
endif()
set(RADIANT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../conformance/radiant")
if(NOT EXISTS "${RADIANT_DIR}")
  set(RADIANT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core/conformance/radiant")
endif()
# Committed cross-check artifacts (C++-produced envelope pinned to the TS check).
set(CROSSCHECK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cross-check")

set(INCLUDE_DIRS
    "${RUNTIME_DIR}/Portable"
    "${RUNTIME_DIR}/Generated"
)

if (MSVC)
    set(WARN_FLAGS /W4)
else()
    set(WARN_FLAGS -Wall -Wextra)
endif()

enable_testing()

# ── World source host tests (US-XC1) ──
add_executable(insimul_verify_unreal
    test_world_source.cpp
    ${PORTABLE_SOURCES}
)
target_include_directories(insimul_verify_unreal PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_unreal PRIVATE
    INSIMUL_FIXTURE_DIR="${FIXTURE_DIR}"
)
target_compile_options(insimul_verify_unreal PRIVATE ${WARN_FLAGS})
add_test(NAME world_source COMMAND insimul_verify_unreal)

# ── Save system host tests (US-XC2) ──
add_executable(insimul_verify_save
    test_save_system.cpp
    ${PORTABLE_SOURCES}
)
target_include_directories(insimul_verify_save PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_save PRIVATE
    INSIMUL_FIXTURE_DIR="${FIXTURE_DIR}"
    INSIMUL_CROSSCHECK_DIR="${CROSSCHECK_DIR}"
    INSIMUL_GEN_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_options(insimul_verify_save PRIVATE ${WARN_FLAGS})
add_test(NAME save_system COMMAND insimul_verify_save)

# ── Quest system host tests (US-XC3) ──
add_executable(insimul_verify_quest
    test_quest_system.cpp
    ${PORTABLE_SOURCES}
)
target_include_directories(insimul_verify_quest PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_quest PRIVATE
    INSIMUL_FIXTURE_DIR="${FIXTURE_DIR}"
    INSIMUL_QUESTS_DIR="${QUESTS_DIR}"
)
target_compile_options(insimul_verify_quest PRIVATE ${WARN_FLAGS})
add_test(NAME quest_system COMMAND insimul_verify_quest)

# ── Bootstrap / full-loop host tests (US-XC4) ──
add_executable(insimul_verify_bootstrap
    test_bootstrap.cpp
    ${PORTABLE_SOURCES}
)
target_include_directories(insimul_verify_bootstrap PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_bootstrap PRIVATE
    INSIMUL_FIXTURE_DIR="${FIXTURE_DIR}"
)
target_compile_options(insimul_verify_bootstrap PRIVATE ${WARN_FLAGS})
add_test(NAME bootstrap COMMAND insimul_verify_bootstrap)

# ── Content-library import host tests (US-IM1) ──
add_executable(insimul_verify_content_library
    test_content_library.cpp
    ${PORTABLE_SOURCES}
)
target_include_directories(insimul_verify_content_library PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_content_library PRIVATE
    INSIMUL_CONTENT_DIR="${CONTENT_DIR}"
)
target_compile_options(insimul_verify_content_library PRIVATE ${WARN_FLAGS})
add_test(NAME content_library COMMAND insimul_verify_content_library)

# ── Content-library round-trip parity host tests (US-IM2) ──
add_executable(insimul_verify_content_roundtrip
    test_content_roundtrip.cpp
    ${PORTABLE_SOURCES}
)
target_include_directories(insimul_verify_content_roundtrip PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_content_roundtrip PRIVATE
    INSIMUL_CONTENT_DIR="${CONTENT_DIR}"
)
target_compile_options(insimul_verify_content_roundtrip PRIVATE ${WARN_FLAGS})
add_test(NAME content_roundtrip COMMAND insimul_verify_content_roundtrip)

# ─────────────────────────────────────────────────────────────────────────────
# Radiant quest GENERATION — the first adopted slice of @insimul/core
# (tasklist 99, RUNTIME_CORE_ADOPTION.md §5). Two targets over ONE source file
# and ONE comparison implementation, so the legs cannot drift apart:
#
#   radiant_source       always built. Drives insimul::FRadiantSource over a
#                        recording stub caller: gates the translation site (the
#                        request document byte-for-byte, the decode, the
#                        fallback) with no native library present.
#   radiant_source_none  the same binary, --source none: this plugin's
#                        pre-adoption behaviour, classified AGREE/GAIN/REGRESSION.
#   radiant_bridge       built ONLY when an insimul-native checkout is visible.
#                        Drives the FULL stack — libinsimulcore (QuickJS + the
#                        vendored @insimul/core bundle) over the natively linked
#                        libinsimul — and must match all 11 corpus vectors.
#
# radiant_bridge is the FIRST target in this harness to link a native library at
# all (§6.5). It links the PREBUILT archives rather than recompiling QuickJS, so
# it gates the artifact that actually ships.
# ─────────────────────────────────────────────────────────────────────────────

set(RADIANT_SOURCES
    test_radiant.cpp
    ${PORTABLE_SOURCES}
)

add_executable(insimul_verify_radiant_source ${RADIANT_SOURCES})
target_include_directories(insimul_verify_radiant_source PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_radiant_source PRIVATE
    INSIMUL_RADIANT_DIR="${RADIANT_DIR}"
    INSIMUL_RADIANT_DEFAULT_SOURCE="stub"
)
target_compile_options(insimul_verify_radiant_source PRIVATE ${WARN_FLAGS})
add_test(NAME radiant_source COMMAND insimul_verify_radiant_source --source stub)
add_test(NAME radiant_source_none COMMAND insimul_verify_radiant_source --source none)

# ── discover libinsimulcore + libinsimul (vendored-first, then sibling) ──────
# Mirrors the corpus lookup above and Godot's run_radiant_tests.sh. Point it
# somewhere explicit with either:
#   -DINSIMUL_NATIVE_DIST=<dir>   a packaged dist:  <dir>/include + <dir>/lib
#   -DINSIMUL_NATIVE_DIR=<dir>    a native checkout: <dir>/corebridge/include
#                                                  + <dir>/build
set(INSIMUL_NATIVE_DIST "$ENV{INSIMUL_NATIVE_DIST}" CACHE PATH "packaged insimul-native dist dir")
set(INSIMUL_NATIVE_DIR "$ENV{INSIMUL_NATIVE_DIR}" CACHE PATH "insimul-native checkout")

set(CORE_LIB "")
set(INSIMUL_LIB "")
if(INSIMUL_NATIVE_DIST AND EXISTS "${INSIMUL_NATIVE_DIST}/lib")
  set(_native_candidates "${INSIMUL_NATIVE_DIST}|lib")
else()
  set(_native_candidates "")
endif()
foreach(_root
    "${INSIMUL_NATIVE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../native"      # <worktrees>/unreal + <worktrees>/native
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../native")  # <project>/engines/unreal layout
  if(_root)
    list(APPEND _native_candidates "${_root}|build")
  endif()
endforeach()

foreach(_candidate IN LISTS _native_candidates)
  string(REPLACE "|" ";" _parts "${_candidate}")
  list(GET _parts 0 _dir)
  list(GET _parts 1 _libsub)
  if(EXISTS "${_dir}/${_libsub}/libinsimulcore.a" AND EXISTS "${_dir}/${_libsub}/libinsimul.a")
    get_filename_component(CORE_LIB "${_dir}/${_libsub}/libinsimulcore.a" ABSOLUTE)
    get_filename_component(INSIMUL_LIB "${_dir}/${_libsub}/libinsimul.a" ABSOLUTE)
    break()
  endif()
endforeach()

# The ABI header is vendored in this repo (byte-identical to the shipping one),
# so the gate compiles against exactly what the plugin compiles against.
set(CORE_ABI_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../Source/ThirdParty/InsimulCoreLibrary/include")

if(CORE_LIB)
  message(STATUS "radiant_bridge: linking ${CORE_LIB}")
  add_executable(insimul_verify_radiant_bridge
      ${RADIANT_SOURCES}
      "${RUNTIME_DIR}/Private/Core/InsimulCoreBridge.cpp"
  )
  target_include_directories(insimul_verify_radiant_bridge PRIVATE
      ${INCLUDE_DIRS}
      "${RUNTIME_DIR}/Private/Core"
      "${CORE_ABI_INCLUDE}"
  )
  target_compile_definitions(insimul_verify_radiant_bridge PRIVATE
      INSIMUL_RADIANT_DIR="${RADIANT_DIR}"
      INSIMUL_RADIANT_DEFAULT_SOURCE="core"
      INSIMUL_HAVE_CORE_BRIDGE=1
      INSIMUL_WITH_CORE=1
  )
  target_compile_options(insimul_verify_radiant_bridge PRIVATE ${WARN_FLAGS})
  # Order matters for static archives: libinsimulcore.a calls into libinsimul.a.
  target_link_libraries(insimul_verify_radiant_bridge PRIVATE "${CORE_LIB}" "${INSIMUL_LIB}")
  add_test(NAME radiant_bridge COMMAND insimul_verify_radiant_bridge --source core)

  # ── the three-leg quest parity diff (US-3) ────────────────────────────────
  add_executable(insimul_verify_quest_parity_core
      test_quest_parity.cpp
      ${PORTABLE_SOURCES}
      "${RUNTIME_DIR}/Private/Core/InsimulCoreBridge.cpp"
  )
  target_include_directories(insimul_verify_quest_parity_core PRIVATE
      ${INCLUDE_DIRS}
      "${RUNTIME_DIR}/Private/Core"
      "${CORE_ABI_INCLUDE}"
  )
  target_compile_definitions(insimul_verify_quest_parity_core PRIVATE
      INSIMUL_QUESTS_DIR="${QUESTS_DIR}"
      INSIMUL_HAVE_CORE_BRIDGE=1
      INSIMUL_WITH_CORE=1
  )
  target_compile_options(insimul_verify_quest_parity_core PRIVATE ${WARN_FLAGS})
  target_link_libraries(insimul_verify_quest_parity_core PRIVATE "${CORE_LIB}" "${INSIMUL_LIB}")
  add_test(NAME quest_parity_core COMMAND insimul_verify_quest_parity_core --core)
else()
  # NOT a silent skip. The bridge is the whole point of the adoption, so its
  # absence is reported at configure time with the exact fix — but it is a
  # WARNING rather than an error because this repo is standalone by design and
  # must still configure and run its six pre-existing gates in a clone that has
  # no insimul-native beside it. radiant_source (the stub leg) still runs and
  # still gates the translation site.
  message(WARNING
      "radiant_bridge + quest_parity_core NOT built: no libinsimulcore.a + libinsimul.a found.\n"
      "  The full-stack radiant gate (11 corpus vectors through core's real\n"
      "  TypeScript) and the three-leg quest parity diff will NOT run. The stub\n"
      "  leg (radiant_source) and the two-leg quest_parity still do.\n"
      "  To build them, in an insimul-native checkout:\n"
      "      cmake -S . -B build && cmake --build build\n"
      "  then re-configure with:\n"
      "      cmake -S tools/verify-unreal -B build -DINSIMUL_NATIVE_DIR=/path/to/native\n"
      "  Searched: ${_native_candidates}")
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Quest parity — the two-implementation diff US-3's third criterion asks for
# (RUNTIME_CORE_ADOPTION.md §10.3). FInsimulQuestSystem is a hand-port of core's
# quest hydration and radiant tick; both are pinned by conformance/quests/.
#
#   quest_parity       always built. Legs: corpus + the hand-port, plus the
#                      classifier self-test. Gates the corpus floors and proves
#                      all five verdicts are reachable with no native library.
#   quest_parity_core  built with libinsimulcore (above). Adds core's leg and
#                      is where the classification is actually produced.
# ─────────────────────────────────────────────────────────────────────────────

add_executable(insimul_verify_quest_parity
    test_quest_parity.cpp
    ${PORTABLE_SOURCES}
)
target_include_directories(insimul_verify_quest_parity PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_quest_parity PRIVATE
    INSIMUL_QUESTS_DIR="${QUESTS_DIR}"
)
target_compile_options(insimul_verify_quest_parity PRIVATE ${WARN_FLAGS})
add_test(NAME quest_parity COMMAND insimul_verify_quest_parity)

# ─────────────────────────────────────────────────────────────────────────────
# The vendored-corpus drift guard (US-3 criterion 4). Every gate above reads
# conformance/, and until this story nothing checked that conformance/ was still
# the corpus it claims to mirror — it had rotted to 41 of core's 76 Prolog cases.
# --check needs no core checkout, so it runs here; add --core for the real
# byte-for-byte diff (see conformance/VENDORED.md).
# ─────────────────────────────────────────────────────────────────────────────

find_program(NODE_EXECUTABLE NAMES node nodejs)
if(NODE_EXECUTABLE)
  add_test(NAME corpus_manifest
      COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../vendor-conformance.mjs" --check)
else()
  message(WARNING
      "corpus_manifest NOT registered: no `node` on PATH, so the vendored corpus\n"
      "  drift guard will not run. Install Node and re-configure, or run it by\n"
      "  hand:  node tools/vendor-conformance.mjs --check")
endif()
