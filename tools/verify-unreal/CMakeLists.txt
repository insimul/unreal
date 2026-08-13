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
    "${RUNTIME_DIR}/Portable/InsimulMechanicHosts.cpp"
    "${RUNTIME_DIR}/Portable/InsimulMechanicSurface.cpp"
    "${RUNTIME_DIR}/Portable/InsimulModuleActivation.cpp"
    "${RUNTIME_DIR}/Portable/InsimulModulePacks.cpp"
    "${RUNTIME_DIR}/Portable/InsimulMechanicScenario.cpp"
)

# The data an EXPORTED game reads: the vendored rule packs, the activation table and
# the sample scene's script. The gates below read the SAME files a build ships, so a
# re-vendored table nothing here would notice cannot exist.
set(ACTIVATION_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../templates/project/Content/Data/insimul")

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
set(PROLOG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../conformance/prolog")
if(NOT EXISTS "${PROLOG_DIR}")
  set(PROLOG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core/conformance/prolog")
endif()
set(UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../conformance/ui")
if(NOT EXISTS "${UI_DIR}")
  set(UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core/conformance/ui")
endif()
set(ITEMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../conformance/items")
if(NOT EXISTS "${ITEMS_DIR}")
  set(ITEMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core/conformance/items")
endif()
# Committed cross-check artifacts (C++-produced envelope pinned to the TS check).
set(CROSSCHECK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cross-check")

# The default-UI portable slice. Kept apart from PORTABLE_SOURCES so the gates that
# have nothing to do with UMG do not recompile it.
set(UI_SOURCES
    "${RUNTIME_DIR}/Portable/InsimulUIRegistryModel.cpp"
    "${RUNTIME_DIR}/Portable/InsimulUIThemeTokens.cpp"
    "${RUNTIME_DIR}/Portable/InsimulLoadingViewModel.cpp"
    "${RUNTIME_DIR}/Portable/InsimulNotifications.cpp"
    "${RUNTIME_DIR}/Portable/InsimulUIPanelCatalog.cpp"
)

set(INCLUDE_DIRS
    "${RUNTIME_DIR}/Portable"
    "${RUNTIME_DIR}/Generated"
)

if (MSVC)
    set(WARN_FLAGS /W4)
else()
    set(WARN_FLAGS -Wall -Wextra)
endif()

# --require-binaries, in this harness's terms. The bridge-linked targets below are
# registered only when a libinsimulcore + libinsimul pair is found, and their absence
# is a configure-time WARNING so a clone with no insimul-native beside it still runs
# the toolchain-free gates. A warning that reads as a pass is how this repo once
# believed it had gates it did not have (RUNTIME_CORE_ADOPTION.md §6.4), so
# -DINSIMUL_REQUIRE_BINARIES=ON turns that warning into a hard configure failure. CI
# and anyone verifying an adoption claim should pass it.
option(INSIMUL_REQUIRE_BINARIES "Fail configuration when no libinsimulcore is found" OFF)

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

# ─────────────────────────────────────────────────────────────────────────────
# The band-120 mechanic host half (tasklist 146 US-1, RUNTIME_CORE_ADOPTION.md §12).
#
#   mechanic_hosts   always built. Drives the eight-interface mirror, the fallbacks
#                    core documents per interface, and the module surface's four
#                    states over stub callers — with no native library present.
#   mechanic_bridge  built ONLY with libinsimulcore (below). Asks `core.methods` of
#                    the artifact that ships and pins the answer, so a mechanic row
#                    ARRIVING fails as loudly as one disappearing.
# ─────────────────────────────────────────────────────────────────────────────

set(MECHANIC_SOURCES
    test_mechanic_hosts.cpp
    ${PORTABLE_SOURCES}
)

add_executable(insimul_verify_mechanic_hosts ${MECHANIC_SOURCES})
target_include_directories(insimul_verify_mechanic_hosts PRIVATE ${INCLUDE_DIRS})
target_compile_options(insimul_verify_mechanic_hosts PRIVATE ${WARN_FLAGS})
add_test(NAME mechanic_hosts COMMAND insimul_verify_mechanic_hosts)

# ─────────────────────────────────────────────────────────────────────────────
# Modules activated from the GENRE BUNDLE (tasklist 146 US-3, §14).
#
#   module_activation    always built. Drives the resolver, the pack consult, the
#                        host restriction and the scenario runner over the data an
#                        exported game reads, with no native library — every
#                        outcome, including the ones a healthy build never takes.
#   activation_witness   built ONLY with libinsimul (below). The only place "was
#                        this pack consulted" can actually be answered: a KB.
# ─────────────────────────────────────────────────────────────────────────────

add_executable(insimul_verify_module_activation
    test_module_activation.cpp
    ${PORTABLE_SOURCES}
)
target_include_directories(insimul_verify_module_activation PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_module_activation PRIVATE
    INSIMUL_ACTIVATION_DATA_DIR="${ACTIVATION_DATA_DIR}"
)
target_compile_options(insimul_verify_module_activation PRIVATE ${WARN_FLAGS})
add_test(NAME module_activation COMMAND insimul_verify_module_activation)

# ─────────────────────────────────────────────────────────────────────────────
# The default-UI registry, theme, loading view-model and the MODULE GATE
# (tasklist 190 US-1, docs/ui.md).
#
# Until this story the four UI host tests under Source/InsimulRuntime/Tests were
# compiled and run by NOTHING: the harness they name (run-ui-tests.sh) does not
# exist in this repository, so "the registry tests pass on the shared cases" was a
# claim with no gate behind it. This target is that gate. It reads the SAME two
# files a build ships — the vendored corpus every engine leg runs, and the panel
# catalog an exported game carries in Content/Data/insimul/ui — so a re-vendored
# corpus or a re-pointed panel that nothing here would notice cannot exist.
# ─────────────────────────────────────────────────────────────────────────────

add_executable(insimul_verify_ui_registry
    "${RUNTIME_DIR}/Tests/test_ui_registry.cpp"
    "${RUNTIME_DIR}/Portable/InsimulJson.cpp"
    "${RUNTIME_DIR}/Portable/InsimulModuleActivation.cpp"
    ${UI_SOURCES}
)
target_include_directories(insimul_verify_ui_registry PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_ui_registry PRIVATE
    INSIMUL_UI_DIR="${UI_DIR}"
    INSIMUL_ACTIVATION_DATA_DIR="${ACTIVATION_DATA_DIR}"
)
target_compile_options(insimul_verify_ui_registry PRIVATE ${WARN_FLAGS})
add_test(NAME ui_registry COMMAND insimul_verify_ui_registry)

# ─────────────────────────────────────────────────────────────────────────────
# The quest + trade panels (tasklist 190 US-2, docs/ui.md).
#
# These two sources existed before this story and were compiled by NOTHING — the
# runners they name (run-quest-ui-tests.sh, run-trade-ui-tests.sh) belong to the
# parent platform checkout and do not exist here, so "the shared view-model cases
# pass" was a claim with no gate behind it, exactly as ui_registry was. Both read
# the vendored corpus every engine leg runs.
# ─────────────────────────────────────────────────────────────────────────────

add_executable(insimul_verify_ui_quest_journal
    "${RUNTIME_DIR}/Tests/test_quest_journal.cpp"
    "${RUNTIME_DIR}/Portable/InsimulJson.cpp"
    "${RUNTIME_DIR}/Portable/InsimulQuestJournalModel.cpp"
    "${RUNTIME_DIR}/Portable/InsimulNotifications.cpp"
)
target_include_directories(insimul_verify_ui_quest_journal PRIVATE ${INCLUDE_DIRS})
target_compile_options(insimul_verify_ui_quest_journal PRIVATE ${WARN_FLAGS})
add_test(NAME ui_quest_journal COMMAND insimul_verify_ui_quest_journal "${UI_DIR}")

add_executable(insimul_verify_ui_trade
    "${RUNTIME_DIR}/Tests/test_trade.cpp"
    "${RUNTIME_DIR}/Portable/InsimulJson.cpp"
    "${RUNTIME_DIR}/Portable/InsimulTradeModel.cpp"
)
target_include_directories(insimul_verify_ui_trade PRIVATE ${INCLUDE_DIRS})
target_compile_options(insimul_verify_ui_trade PRIVATE ${WARN_FLAGS})
add_test(NAME ui_trade COMMAND insimul_verify_ui_trade "${UI_DIR}")

# ─────────────────────────────────────────────────────────────────────────────
# Shop + REPUTATION, EQUIPMENT, and the state-location invariant (190 US-2).
#
# The first two read the band-124 items corpora — a price is a function of the
# simulation rather than a shop table, and the world declares its own equipment
# slots — and diff the canonical projection of every case byte for byte. The third
# loads a REAL SaveFile fixture, runs the panels' ops through it, and asserts the
# change is in the save's canonical bytes, that a re-hydrate reproduces it, that
# everything outside currentState is untouched, and that the census is conserved:
# playthrough data lives in the save file and nowhere else.
# ─────────────────────────────────────────────────────────────────────────────

add_executable(insimul_verify_ui_state_binding
    "${RUNTIME_DIR}/Tests/test_ui_state_binding.cpp"
    "${RUNTIME_DIR}/Portable/InsimulJson.cpp"
    "${RUNTIME_DIR}/Portable/InsimulCanonicalJson.cpp"
    "${RUNTIME_DIR}/Portable/InsimulSha256.cpp"
    "${RUNTIME_DIR}/Portable/InsimulSaveSystem.cpp"
    "${RUNTIME_DIR}/Portable/InsimulQuestSystem.cpp"
    "${RUNTIME_DIR}/Portable/InsimulQuestJournalModel.cpp"
    "${RUNTIME_DIR}/Portable/InsimulTradeModel.cpp"
    "${RUNTIME_DIR}/Portable/InsimulTradePricing.cpp"
    "${RUNTIME_DIR}/Portable/InsimulEquipmentModel.cpp"
    "${RUNTIME_DIR}/Portable/InsimulUIStateBinding.cpp"
)
target_include_directories(insimul_verify_ui_state_binding PRIVATE ${INCLUDE_DIRS})
target_compile_definitions(insimul_verify_ui_state_binding PRIVATE
    INSIMUL_ITEMS_DIR="${ITEMS_DIR}"
    INSIMUL_FIXTURE_DIR="${FIXTURE_DIR}"
)
target_compile_options(insimul_verify_ui_state_binding PRIVATE ${WARN_FLAGS})
add_test(NAME ui_state_binding COMMAND insimul_verify_ui_state_binding)

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

  # ── the band-120 reachability MEASUREMENT (146 US-1) ──────────────────────
  add_executable(insimul_verify_mechanic_bridge
      ${MECHANIC_SOURCES}
      "${RUNTIME_DIR}/Private/Core/InsimulCoreBridge.cpp"
  )
  target_include_directories(insimul_verify_mechanic_bridge PRIVATE
      ${INCLUDE_DIRS}
      "${RUNTIME_DIR}/Private/Core"
      "${CORE_ABI_INCLUDE}"
  )
  target_compile_definitions(insimul_verify_mechanic_bridge PRIVATE
      INSIMUL_HAVE_CORE_BRIDGE=1
      INSIMUL_WITH_CORE=1
  )
  target_compile_options(insimul_verify_mechanic_bridge PRIVATE ${WARN_FLAGS})
  target_link_libraries(insimul_verify_mechanic_bridge PRIVATE "${CORE_LIB}" "${INSIMUL_LIB}")
  add_test(NAME mechanic_bridge COMMAND insimul_verify_mechanic_bridge --bridge)

  # ── the band-120 PREDICATE half, EXECUTED (146 US-2) ──────────────────────
  # Drives every vendored conformance/prolog case through insimul::InsimulKB —
  # the plugin's own wrapper over libinsimul, not a second reader — and diffs the
  # golden solution sets. This is the gate that stops `conformance/prolog/` from
  # being a checked-in file nothing runs; it needs only libinsimul, but it is
  # registered in this block so it shares the one discovery and the one
  # --require-binaries contract with the bridge legs.
  add_executable(insimul_verify_prolog_corpus
      test_prolog_corpus.cpp
      "${RUNTIME_DIR}/Portable/InsimulJson.cpp"
      "${RUNTIME_DIR}/Private/Prolog/InsimulKB.cpp"
  )
  target_include_directories(insimul_verify_prolog_corpus PRIVATE
      ${INCLUDE_DIRS}
      "${RUNTIME_DIR}/Private/Prolog"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../Source/ThirdParty/InsimulLibrary/include"
  )
  target_compile_definitions(insimul_verify_prolog_corpus PRIVATE
      INSIMUL_PROLOG_DIR="${PROLOG_DIR}"
  )
  target_compile_options(insimul_verify_prolog_corpus PRIVATE ${WARN_FLAGS})
  target_link_libraries(insimul_verify_prolog_corpus PRIVATE "${INSIMUL_LIB}")
  add_test(NAME prolog_corpus COMMAND insimul_verify_prolog_corpus)

  # ── an inactive module contributes NOTHING, witnessed in a KB (146 US-3) ──
  # The only place that claim can be proved: consults each genre's active packs
  # through the plugin's own ConsultActivePacks + insimul::InsimulKB and asks
  # current_predicate/1 for every pack the build carries. Also executes the sample
  # scene's scenario against the packs its genre activates.
  add_executable(insimul_verify_activation_witness
      test_activation_witness.cpp
      "${RUNTIME_DIR}/Portable/InsimulJson.cpp"
      "${RUNTIME_DIR}/Portable/InsimulModuleActivation.cpp"
      "${RUNTIME_DIR}/Portable/InsimulModulePacks.cpp"
      "${RUNTIME_DIR}/Portable/InsimulMechanicScenario.cpp"
      "${RUNTIME_DIR}/Private/Prolog/InsimulKB.cpp"
  )
  target_include_directories(insimul_verify_activation_witness PRIVATE
      ${INCLUDE_DIRS}
      "${RUNTIME_DIR}/Private/Prolog"
      "${CMAKE_CURRENT_SOURCE_DIR}/../../Source/ThirdParty/InsimulLibrary/include"
  )
  target_compile_definitions(insimul_verify_activation_witness PRIVATE
      INSIMUL_ACTIVATION_DATA_DIR="${ACTIVATION_DATA_DIR}"
  )
  target_compile_options(insimul_verify_activation_witness PRIVATE ${WARN_FLAGS})
  target_link_libraries(insimul_verify_activation_witness PRIVATE "${INSIMUL_LIB}")
  add_test(NAME activation_witness COMMAND insimul_verify_activation_witness)
else()
  # NOT a silent skip. The bridge is the whole point of the adoption, so its
  # absence is reported at configure time with the exact fix — but it is a
  # WARNING rather than an error because this repo is standalone by design and
  # must still configure and run its six pre-existing gates in a clone that has
  # no insimul-native beside it. radiant_source (the stub leg) still runs and
  # still gates the translation site.
  if(INSIMUL_REQUIRE_BINARIES)
    set(_missing_binaries_level FATAL_ERROR)
  else()
    set(_missing_binaries_level WARNING)
  endif()
  message(${_missing_binaries_level}
      "radiant_bridge + quest_parity_core + mechanic_bridge + prolog_corpus NOT built: no libinsimulcore.a + libinsimul.a found.\n"
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
