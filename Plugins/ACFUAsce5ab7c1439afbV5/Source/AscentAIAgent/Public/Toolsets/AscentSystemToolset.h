// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "AscentSystemToolset.generated.h"

/// Editor Python escape hatch (loopback editor only).
UCLASS(BlueprintType, Hidden)
class ASCENTAIAGENT_API UAscentSystemToolset : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    /**
     * Executes Python inside the loopback UE editor (was ue_py_run).
     *
     * `print()` WORKS: stdout and stderr are redirected into the returned `output` field
     * (with `output_length`), so print your results and read them from the response. Do NOT
     * write results to a file — `open(` is a blocked token and the call will be refused.
     * Read `output_length`, NOT `output`, to tell whether anything was printed: a run that
     * printed nothing returns `output_length: 0` with `output` set to the sentinel string
     * "Executed successfully" (never an empty string). Do not mistake that sentinel for
     * your script's result.
     *
     * @param Code Python code to execute.
     * @param Mode Required execution mode: "read_only" (safe), "mutate_asset" (can modify assets), or "mutate_level" (can modify the level).
     * @param ExpectedPathsJson Optional JSON array of asset paths expected to be modified by mutate_asset code; pass an empty string to skip.
     * @return JSON reporting execution status, mode, and the script's captured `output`.
     */
    UFUNCTION(meta = (AICallable), Category = "AscentSystem")
    static FString PythonRun(const FString& Code, const FString& Mode, const FString& ExpectedPathsJson);

    /**
     * Starts a Live Coding compile of the C++ changes, the same as Ctrl+Alt+F11. The Editor always asks the user first, in every work mode.
     * Returns immediately while the compile runs; follow it with GetLiveCodingStatus and the LogLiveCoding log category.
     * @return JSON with "result": InProgress, CompileStillActive, NotStarted, Unavailable, or the final result.
     */
    UFUNCTION(meta = (AICallable, MutatingTool), Category = "AscentSystem")
    static FString TriggerLiveCoding();

    /**
     * Reports whether Live Coding is available and enabled in this Editor session and whether a compile is running.
     * @return JSON with "available", "enabled", "started", "compiling", and "message" when it cannot be used.
     */
    UFUNCTION(meta = (AICallable, ReadOnlyTool), Category = "AscentSystem")
    static FString GetLiveCodingStatus();
};
