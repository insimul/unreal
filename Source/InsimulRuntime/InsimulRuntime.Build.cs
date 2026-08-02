// Copyright 2024 Insimul. All Rights Reserved.

using UnrealBuildTool;

public class InsimulRuntime : ModuleRules
{
    public InsimulRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "HTTP",
            "Json",
            "JsonUtilities",
            "WebSockets",
            "UMG",
            "Slate",
            "SlateCore",
            "AudioCapture",
            "AudioMixer"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            // Native Prolog core (libinsimul C ABI). Private: the InsimulKB
            // wrapper (Private/Prolog) is the only consumer of insimul.h.
            "InsimulLibrary",
            // `@insimul/core` across the C ABI (libinsimulcore). Private: the
            // FInsimulCoreBridge wrapper (Private/Core) is the only consumer of
            // insimulcore.h. The module defines INSIMUL_WITH_CORE per platform;
            // where no build exists it is 0 and the bridge compiles to its
            // unavailable stub (RUNTIME_CORE_ADOPTION.md §4.7.2).
            "InsimulCoreLibrary"
        });
    }
}
