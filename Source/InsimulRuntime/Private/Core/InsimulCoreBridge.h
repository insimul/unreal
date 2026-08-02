// Copyright 2024 Insimul. All Rights Reserved.
//
// FInsimulCoreBridge — a plain C++ RAII wrapper around the libinsimulcore C ABI
// (insimulcore.h). It is to libinsimulcore exactly what insimul::InsimulKB
// (Private/Prolog/InsimulKB.h) is to libinsimul, and is deliberately shaped the
// same way: create in the constructor, non-throwing, LastError() carries the
// reason a call returned nothing, move-only, no engine types beyond the ABI.
//
// DELIBERATELY UE-FREE: this header and its .cpp use only the C++ standard
// library and the extern "C" ABI. No CoreMinimal.h, no UObject, no FString.
// That is what lets tools/verify-unreal link and drive it under plain clang++
// (target `radiant_bridge`) — the first automated gate in this repo to link a
// native library at all (RUNTIME_CORE_ADOPTION.md §6.5).
//
// THIS IS THE ONLY FILE IN THE PLUGIN THAT INCLUDES insimulcore.h. Everything
// above it speaks insimul::ICoreCaller (JSON in, JSON out); everything below it
// is QuickJS and the vendored `@insimul/core` bundle. It marshals bytes and
// NOTHING else — no argument building, no result interpretation. Those live in
// Portable/InsimulRadiantSource.cpp, the single translation site, so there can
// never be two conversions that disagree.
//
// THREAD AFFINITY: one handle is owned by one thread, exactly like a libinsimul
// KB (QuickJS is single-threaded and the bundle's Prolog seam calls straight
// into libinsimul). The game-thread assertion lives in the UE shell
// (Public/InsimulRadiantSource.h), which is where UE's IsInGameThread() is
// available — the same split UInsimulPrologSubsystem uses over InsimulKB.
//
// BUILD-TIME AVAILABILITY: compiled against the ABI only when INSIMUL_WITH_CORE
// is defined non-zero by the ThirdParty/InsimulCoreLibrary module. On a platform
// with no libinsimulcore build (consoles, iOS, Android — §4.7.2) the .cpp
// compiles to a stub whose IsAvailable() is false and whose LastError() says
// why, so the plugin still builds and every adopted call site degrades to its
// pre-adoption path. Absence of the library is NOT a build error.

#pragma once

#include "../../Portable/InsimulCoreCaller.h" // insimul::ICoreCaller

#include <string>

// Opaque C ABI handle (from ThirdParty/InsimulCoreLibrary/include/insimulcore.h).
// Forward-declared so this header pulls in neither the C ABI header nor any UE
// header — only the .cpp includes "insimulcore.h".
struct insimul_core;

namespace insimul {

class FInsimulCoreBridge : public ICoreCaller {
public:
	/**
	 * Start a core runtime: instantiate QuickJS and evaluate the vendored
	 * `@insimul/core` bundle. This is the expensive call (a few ms) — create ONE
	 * per game instance and keep it, as you would a libinsimul KB. Never throws;
	 * check IsAvailable() and read LastError() when it is false.
	 */
	FInsimulCoreBridge();
	~FInsimulCoreBridge() override;

	FInsimulCoreBridge(FInsimulCoreBridge&& Other) noexcept;
	FInsimulCoreBridge& operator=(FInsimulCoreBridge&& Other) noexcept;
	FInsimulCoreBridge(const FInsimulCoreBridge&) = delete;
	FInsimulCoreBridge& operator=(const FInsimulCoreBridge&) = delete;

	bool IsAvailable() const override { return Handle != nullptr; }

	/**
	 * One JSON encode + decode per call, plus core's own work. Call it when a
	 * decision is needed (a director tick, a craft attempt) — NEVER from Tick.
	 * See insimulcore.h, "THE ONE HARD RULE".
	 */
	bool Call(const std::string& Method, const std::string& ArgsJson, std::string& OutJson) override;

	std::string LastError() const override { return LastErrorText; }

	/** "<abi> (quickjs <pin>, core <commit>)", or "" without the library. */
	std::string Version() const override;

	/** False when this build has no libinsimulcore to link against at all. */
	static bool IsCompiledIn();

private:
	insimul_core* Handle = nullptr;
	std::string LastErrorText;
};

} // namespace insimul
