// Copyright 2024 Insimul. All Rights Reserved.
//
// Implementation of the plain C++ RAII wrapper over the libinsimulcore C ABI.
// UE-FREE: only <insimulcore.h> (the extern "C" ABI) and the standard library.
//
// Two builds of this file exist, selected by INSIMUL_WITH_CORE:
//   1  the real bridge (desktop: Mac / Linux / Win64);
//   0  an unavailable stub that still compiles and links, so a console/mobile
//      target builds and falls back (RUNTIME_CORE_ADOPTION.md §4.7.2).
// The stub is not a courtesy — it is what keeps "the plugin degrades cleanly
// when the bridge is absent" true at COMPILE time rather than by convention.

#include "InsimulCoreBridge.h"

#if !defined(INSIMUL_WITH_CORE)
// Standalone / host-harness builds define this on the command line. Default to
// present: a build that meant to exclude the bridge says so explicitly, and a
// silent default of "absent" would turn a link error into a mysteriously empty
// quest list.
#define INSIMUL_WITH_CORE 1
#endif

#if INSIMUL_WITH_CORE
extern "C" {
#include "insimulcore.h"
}
#endif

namespace insimul {

bool FInsimulCoreBridge::IsCompiledIn() {
#if INSIMUL_WITH_CORE
	return true;
#else
	return false;
#endif
}

#if INSIMUL_WITH_CORE

FInsimulCoreBridge::FInsimulCoreBridge() {
	Handle = insimul_core_create();
	if (Handle == nullptr) {
		// A null handle means the runtime could not be created or the vendored
		// bundle failed to evaluate. There is no handle to ask, so the ABI's
		// NULL-safe last_error is the only source.
		const char* Error = insimul_core_last_error(nullptr);
		LastErrorText = (Error != nullptr && *Error != '\0')
			? Error
			: "insimul_core_create() failed — the core runtime could not start";
	}
}

FInsimulCoreBridge::~FInsimulCoreBridge() {
	// Deterministic release. Unlike a GC'd host we know exactly when the runtime
	// dies, and every handle taken must have an explicit release site — see
	// RUNTIME_CORE_ADOPTION.md §1.4.
	insimul_core_destroy(Handle);
	Handle = nullptr;
}

bool FInsimulCoreBridge::Call(const std::string& Method, const std::string& ArgsJson, std::string& OutJson) {
	OutJson.clear();
	if (Handle == nullptr) {
		LastErrorText = "core bridge is not available";
		return false;
	}
	// Returned strings are owned by the handle and valid only until the next
	// call on it, so copy into OutJson before doing anything else.
	const char* Result = insimul_core_call(Handle, Method.c_str(), ArgsJson.empty() ? nullptr : ArgsJson.c_str());
	if (Result == nullptr) {
		const char* Error = insimul_core_last_error(Handle);
		LastErrorText = (Error != nullptr && *Error != '\0') ? Error : (Method + " failed");
		return false;
	}
	OutJson.assign(Result);
	LastErrorText.clear();
	return true;
}

std::string FInsimulCoreBridge::Version() const {
	const char* V = insimul_core_version();
	return V != nullptr ? std::string(V) : std::string();
}

#else // !INSIMUL_WITH_CORE — no libinsimulcore for this platform.

namespace {
const char* const kNoBridge =
	"this build has no libinsimulcore (no build exists for this platform) — "
	"radiant generation falls back to its pre-adoption behaviour";
} // namespace

FInsimulCoreBridge::FInsimulCoreBridge() : LastErrorText(kNoBridge) {}

FInsimulCoreBridge::~FInsimulCoreBridge() = default;

bool FInsimulCoreBridge::Call(const std::string&, const std::string&, std::string& OutJson) {
	OutJson.clear();
	LastErrorText = kNoBridge;
	return false;
}

std::string FInsimulCoreBridge::Version() const {
	return std::string();
}

#endif // INSIMUL_WITH_CORE

FInsimulCoreBridge::FInsimulCoreBridge(FInsimulCoreBridge&& Other) noexcept
	: Handle(Other.Handle), LastErrorText(std::move(Other.LastErrorText)) {
	Other.Handle = nullptr;
}

FInsimulCoreBridge& FInsimulCoreBridge::operator=(FInsimulCoreBridge&& Other) noexcept {
	if (this != &Other) {
#if INSIMUL_WITH_CORE
		insimul_core_destroy(Handle);
#endif
		Handle = Other.Handle;
		LastErrorText = std::move(Other.LastErrorText);
		Other.Handle = nullptr;
	}
	return *this;
}

} // namespace insimul
