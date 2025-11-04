using UnrealBuildTool;
using System.Collections.Generic;

public class LiveKitSandboxEditorTarget : TargetRules
{
    public LiveKitSandboxEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(new string[] { "LiveKitSandbox" });
    }
}
