// Copyright 2024 Insimul. All Rights Reserved.
//
// ThirdParty module publishing the prebuilt libinsimulcore runtime — the C ABI
// that lets this plugin run `@insimul/core`'s TypeScript (RUNTIME_CORE_ADOPTION.md
// §4). It is deliberately the SAME shape as the sibling InsimulLibrary module
// (libinsimul / Trealla), because it is the same kind of thing: an opaque handle,
// JSON in / JSON out, no engine or JS types across the boundary.
//
//   Source/ThirdParty/InsimulCoreLibrary/
//     InsimulCoreLibrary.Build.cs   (this file)
//     include/insimulcore.h         the stable extern "C" C ABI (5 functions)
//     lib/Mac/libinsimulcore.dylib
//     lib/Linux/libinsimulcore.so
//     lib/Win64/insimulcore.dll  insimulcore.lib
//     VERSION                       abi + quickjs pin + the vendored core commit
//
// The binaries under lib/ are staged from insimul-native/dist/<platform>/ at
// package time; this module only declares where each build target finds them.
//
// PLATFORM MATRIX (RUNTIME_CORE_ADOPTION.md §4.7.2). libinsimulcore is built for
// desktop only — macOS, Linux and Win64. Consoles, iOS and Android have NO build,
// and QuickJS on a console platform is a porting question nobody has asked yet.
// On those targets this module publishes nothing and leaves INSIMUL_WITH_CORE
// undefined, so InsimulRuntime compiles without the bridge and every adopted
// call site degrades to its pre-adoption path (see Portable/InsimulRadiantSource.h,
// ERadiantSource::None). That degradation is a REQUIREMENT, not a nicety: it is
// what keeps a console build of the plugin shippable.

using System.IO;
using UnrealBuildTool;

public class InsimulCoreLibrary : ModuleRules
{
    public InsimulCoreLibrary(ReadOnlyTargetRules Target) : base(Target)
    {
        // Prebuilt native library — no UE source is compiled in this module.
        Type = ModuleType.External;

        // Consumers #include "insimulcore.h" (extern "C", leaks no engine types).
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

        string LibDir = Path.Combine(ModuleDirectory, "lib");

        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string Dylib = Path.Combine(LibDir, "Mac", "libinsimulcore.dylib");
            PublicAdditionalLibraries.Add(Dylib);
            RuntimeDependencies.Add(Dylib);
            PublicDefinitions.Add("INSIMUL_WITH_CORE=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string So = Path.Combine(LibDir, "Linux", "libinsimulcore.so");
            PublicAdditionalLibraries.Add(So);
            RuntimeDependencies.Add(So);
            PublicDefinitions.Add("INSIMUL_WITH_CORE=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Link against the import lib; delay-load + stage the runtime DLL.
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "Win64", "insimulcore.lib"));
            PublicDelayLoadDLLs.Add("insimulcore.dll");
            RuntimeDependencies.Add(Path.Combine(LibDir, "Win64", "insimulcore.dll"));
            PublicDefinitions.Add("INSIMUL_WITH_CORE=1");
        }
        else
        {
            // No libinsimulcore for this platform. INSIMUL_WITH_CORE stays
            // undefined and FInsimulCoreBridge compiles to its unavailable stub.
            PublicDefinitions.Add("INSIMUL_WITH_CORE=0");
        }
    }
}
