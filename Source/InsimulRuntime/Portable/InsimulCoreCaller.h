// Copyright 2024 Insimul. All Rights Reserved.
//
// ICoreCaller — the transport seam to `@insimul/core` (RUNTIME_CORE_ADOPTION.md
// §4). JSON in, JSON out, and nothing else: this is the whole of "no engine type
// crosses into core" at the transport level.
//
// It is deliberately NOT radiant-specific. Radiant generation is only the first
// adopted slice; every later one (crafting, the language stack, quest
// orchestration) speaks the same seam, so adopting more of core adds a method
// name to the bundle's table and a translation site above this interface — never
// a second transport. That is Decision 1's corollary: do not invent a second
// mechanism.
//
// Two implementations exist:
//   - insimul::FInsimulCoreBridge (Private/Core/InsimulCoreBridge.h) over the
//     libinsimulcore C ABI — what ships;
//   - a recording stub in tools/verify-unreal, so the translation site above can
//     be host-tested with no native library present.
//
// std-only, no CoreMinimal.h — it lives in Portable/ so both of those, and the
// adapters above it, compile under plain clang++.

#pragma once

#include <string>

namespace insimul {

class ICoreCaller {
public:
	virtual ~ICoreCaller() = default;

	/**
	 * True when the runtime behind this caller started. A false here is not a
	 * per-call error — it means the build has no libinsimulcore (or the bundle
	 * failed to evaluate), and callers should fall back rather than retry.
	 */
	virtual bool IsAvailable() const = 0;

	/**
	 * Call an adopted core method. `ArgsJson` is a JSON object string ("{}" for
	 * no arguments). Returns false on failure with LastError() set; OutJson is
	 * only meaningful on true.
	 *
	 * COST: one JSON encode + decode per call, plus core's own work. Fine at
	 * gameplay-event rate, fatal per frame — see insimulcore.h's "ONE HARD RULE".
	 */
	virtual bool Call(const std::string& Method, const std::string& ArgsJson, std::string& OutJson) = 0;

	/** Reason for the most recent failure on this caller, or "" if none. */
	virtual std::string LastError() const = 0;

	/**
	 * The transport's version stamp — for the bridge, libinsimulcore's
	 * "<abi> (quickjs <pin>, core <commit>)". It is NOT a core method: the core
	 * commit baked into the binary is a property of the transport, and it is the
	 * string to quote in a bug report about core behaviour (§4.7.3).
	 */
	virtual std::string Version() const = 0;
};

} // namespace insimul
