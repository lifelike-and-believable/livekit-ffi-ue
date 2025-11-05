using UnrealBuildTool;
using System.IO;

public class LiveKitBridge : ModuleRules
{
    public LiveKitBridge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Projects" });

        string ThirdPartyBase = Path.Combine(ModuleDirectory, "ThirdParty", "livekit_ffi");
        string IncludePath = Path.Combine(ThirdPartyBase, "include");
        PublicIncludePaths.Add(IncludePath);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string LibPath = Path.Combine(ThirdPartyBase, "lib", "Win64", "Release");
            // Link against the import library produced by the Rust cdylib build
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "livekit_ffi.dll.lib"));

            // Delay-load the DLL and ensure it's staged with the build
            PublicDelayLoadDLLs.Add("livekit_ffi.dll");
            RuntimeDependencies.Add("$(BinaryOutputDir)/livekit_ffi.dll");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string LibPath = Path.Combine(ThirdPartyBase, "lib", "Mac", "Release");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "liblivekit_ffi.a"));
        }
        else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
            string LibPath = Path.Combine(ThirdPartyBase, "lib", "Linux", "Release");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "liblivekit_ffi.a"));
            PublicSystemLibraries.AddRange(new string[] { "pthread", "dl" });
        }
    }
}
