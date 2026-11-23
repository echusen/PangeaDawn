// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSDialogueToolset.h"

#include "ADSDialogueDeveloperSettings.h"
#include "ADSDialogueFactory.h"
#include "ADSDialogueFunctionLibrary.h"
#include "ADSTypes.h"
#include "ADSVoiceConfigDataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "GameplayTagContainer.h"
#include "Graph/ADSDialogue.h"
#include "Graph/ADSGraphNode.h"
#include "Graph/ADSWorldDialogue.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace ADSDialogueToolset
{
    static UADSEditorSubsystem* GetSubsystem()
    {
        UADSEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UADSEditorSubsystem>() : nullptr;
        if (!Subsystem)
        {
            UKismetSystemLibrary::RaiseScriptError(TEXT("ADS editor subsystem is not available."));
        }
        return Subsystem;
    }

    static UADSDialogue* LoadDialogue(const FString& DialoguePath)
    {
        FString ObjectPath = DialoguePath.TrimStartAndEnd();
        if (ObjectPath.IsEmpty())
        {
            UKismetSystemLibrary::RaiseScriptError(TEXT("DialoguePath cannot be empty."));
            return nullptr;
        }
        if (!ObjectPath.Contains(TEXT(".")))
        {
            ObjectPath += TEXT(".") + FPackageName::GetShortName(ObjectPath);
        }

        UADSDialogue* Dialogue = LoadObject<UADSDialogue>(nullptr, *ObjectPath);
        if (!Dialogue)
        {
            UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("No ADS dialogue found at '%s'."), *DialoguePath));
        }
        return Dialogue;
    }

    static UADSVoiceConfigDataAsset* GetVoiceConfig()
    {
        UADSVoiceConfigDataAsset* VoiceConfig = GetDefault<UADSDialogueDeveloperSettings>()->GetVoiceConfigDataAsset();
        if (!VoiceConfig)
        {
            UKismetSystemLibrary::RaiseScriptError(TEXT("No Voice Config Data Asset is set in Project Settings > Ascent Dialogue Settings."));
        }
        return VoiceConfig;
    }

    static FADSVoiceCastingEntry MakeCastingEntry(const UADSVoiceConfigDataAsset& VoiceConfig, const FGameplayTag& Tag)
    {
        FADSVoiceCastingEntry Entry;
        Entry.ParticipantTag = Tag.ToString();
        Entry.ParticipantName = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(Tag);
        if (const FADSVoiceSettings* Settings = VoiceConfig.VoiceConfigs.Find(Tag))
        {
            Entry.bHasVoiceConfig = true;
            Entry.Personality = Settings->Personality;
            Entry.VoiceName = Settings->SelectedVoiceName;
            Entry.VoiceID = Settings->GetSelectedVoiceID();
        }
        return Entry;
    }

    struct FDialogueTarget
    {
        FString PackageName;
        FString AssetName;
        FString ObjectPath;
    };

    static bool ResolveTarget(const FString& DialoguePath, FDialogueTarget& OutTarget, FString& OutError)
    {
        FString PackageName = DialoguePath.TrimStartAndEnd();
        FString ObjectName;
        const int32 DotIndex = PackageName.Find(TEXT("."));
        if (DotIndex != INDEX_NONE)
        {
            ObjectName = PackageName.Mid(DotIndex + 1);
            PackageName.LeftInline(DotIndex);
        }

        FText Reason;
        if (PackageName.IsEmpty() || PackageName.Contains(TEXT("//")) || PackageName.Contains(TEXT(".."))
            || !FPackageName::IsValidLongPackageName(PackageName, false, &Reason))
        {
            OutError = FString::Printf(TEXT("'%s' is not a valid asset path%s%s. Use a path such as /Game/MyProject/Dialogues/DA_Guard."),
                *DialoguePath, Reason.IsEmpty() ? TEXT("") : TEXT(": "), *Reason.ToString());
            return false;
        }
        if (PackageName.StartsWith(TEXT("/Engine/")) || PackageName.StartsWith(TEXT("/Script/")) || PackageName.StartsWith(TEXT("/Temp/")))
        {
            OutError = FString::Printf(TEXT("'%s' is not project content. Save dialogues under /Game/ or a plugin's content folder."), *DialoguePath);
            return false;
        }
        FString Filename;
        if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension()))
        {
            OutError = FString::Printf(TEXT("'%s' is not under a mounted content root."), *DialoguePath);
            return false;
        }

        const FString AssetName = FPackageName::GetShortName(PackageName);
        if (!ObjectName.IsEmpty() && !ObjectName.Equals(AssetName))
        {
            OutError = FString::Printf(TEXT("'%s' names object '%s', but the asset in that package is '%s'."), *DialoguePath, *ObjectName, *AssetName);
            return false;
        }

        OutTarget.PackageName = PackageName;
        OutTarget.AssetName = AssetName;
        OutTarget.ObjectPath = PackageName + TEXT(".") + AssetName;
        return true;
    }

    static bool ResolveKind(const FString& DialogueKind, UClass*& OutClass, bool& bOutExplicit, FString& OutError)
    {
        const FString Kind = DialogueKind.TrimStartAndEnd();
        bOutExplicit = !Kind.IsEmpty();
        if (Kind.IsEmpty() || Kind.Equals(TEXT("Dialogue"), ESearchCase::IgnoreCase))
        {
            OutClass = UADSDialogue::StaticClass();
            return true;
        }
        if (Kind.Equals(TEXT("WorldDialogue"), ESearchCase::IgnoreCase))
        {
            OutClass = UADSWorldDialogue::StaticClass();
            return true;
        }
        OutError = FString::Printf(TEXT("DialogueKind '%s' is not valid. Use \"Dialogue\" or \"WorldDialogue\"."), *DialogueKind);
        return false;
    }

    static bool ResolveTag(const FString& Name, FGameplayTag& OutTag)
    {
        OutTag = FGameplayTag::RequestGameplayTag(FName(*Name.TrimStartAndEnd()), false);
        return OutTag.IsValid();
    }

    static bool ResolveParticipantTags(const TArray<FString>& Names, TArray<FGameplayTag>& OutTags, FString& OutError)
    {
        TArray<FString> Unknown;
        for (const FString& Name : Names)
        {
            FGameplayTag Tag;
            if (!ResolveTag(Name, Tag))
            {
                Unknown.AddUnique(Name);
            }
            else
            {
                OutTags.AddUnique(Tag);
            }
        }
        if (!Unknown.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Unregistered participant tag(s): %s. Use tags returned by TagList with root Character; never invent one."),
                *FString::Join(Unknown, TEXT(", ")));
            return false;
        }
        return true;
    }

    static bool ParseResponseMode(const FString& Name, EADSAIPlayerResponseMode& OutMode, FString& OutError)
    {
        const int64 Value = StaticEnum<EADSAIPlayerResponseMode>()->GetValueByNameString(Name.TrimStartAndEnd());
        if (Value == INDEX_NONE)
        {
            OutError = FString::Printf(TEXT("PlayerResponseMode '%s' is not valid. Use \"OptionButtons\", \"DialogueNodes\" or \"Mixed\"."), *Name);
            return false;
        }
        OutMode = static_cast<EADSAIPlayerResponseMode>(Value);
        return true;
    }

    static bool HasPlayerParticipant(const TArray<FGameplayTag>& Tags)
    {
        for (const FGameplayTag& Tag : Tags)
        {
            if (UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(Tag).ToLower().Contains(TEXT("player")))
            {
                return true;
            }
        }
        return false;
    }
}

FADSDialogueWritingBrief UADSDialogueToolset::GetWritingBrief(const FString& DialoguePath, const FString& Description)
{
    FADSDialogueWritingBrief Brief;
    UADSEditorSubsystem* Subsystem = ADSDialogueToolset::GetSubsystem();
    UADSDialogue* Dialogue = Subsystem ? ADSDialogueToolset::LoadDialogue(DialoguePath) : nullptr;
    if (!Dialogue)
    {
        return Brief;
    }

    FString Error;
    if (!Subsystem->BuildDialogueWritingBrief(Dialogue, Description, Brief, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
    }
    return Brief;
}

FString UADSDialogueToolset::ExportText(const FString& DialoguePath)
{
    UADSEditorSubsystem* Subsystem = ADSDialogueToolset::GetSubsystem();
    UADSDialogue* Dialogue = Subsystem ? ADSDialogueToolset::LoadDialogue(DialoguePath) : nullptr;
    return Dialogue ? Subsystem->ExportDialogueToText(Dialogue) : FString();
}

FADSDialogueTextValidation UADSDialogueToolset::ValidateText(const FString& DialoguePath, const FString& Text)
{
    UADSEditorSubsystem* Subsystem = ADSDialogueToolset::GetSubsystem();
    UADSDialogue* Dialogue = Subsystem ? ADSDialogueToolset::LoadDialogue(DialoguePath) : nullptr;
    return Dialogue ? Subsystem->ValidateDialogueText(Dialogue, Text) : FADSDialogueTextValidation();
}

FADSDialogueTextValidation UADSDialogueToolset::PreviewText(const FString& DialoguePath, const FString& Text)
{
    UADSEditorSubsystem* Subsystem = ADSDialogueToolset::GetSubsystem();
    UADSDialogue* Dialogue = Subsystem ? ADSDialogueToolset::LoadDialogue(DialoguePath) : nullptr;
    if (!Dialogue)
    {
        return FADSDialogueTextValidation();
    }

    const FADSDialogueTextValidation Validation = Subsystem->ValidateDialogueText(Dialogue, Text);
    if (!Validation.bValid)
    {
        UKismetSystemLibrary::RaiseScriptError(TEXT("Dialogue text is not valid: ") + FString::Join(Validation.Errors, TEXT(" ")));
        return Validation;
    }

    Subsystem->ShowGeneratedDialoguePreview(Dialogue, Text, TEXT("Yarin"));
    return Validation;
}

FString UADSDialogueToolset::GenerateVoices(const FString& DialoguePath, bool bForceRegenerate)
{
    UADSEditorSubsystem* Subsystem = ADSDialogueToolset::GetSubsystem();
    UADSDialogue* Dialogue = Subsystem ? ADSDialogueToolset::LoadDialogue(DialoguePath) : nullptr;
    if (!Dialogue)
    {
        return FString();
    }

    if (Subsystem->IsBatchGeneratingVoices())
    {
        UKismetSystemLibrary::RaiseScriptError(TEXT("A voice generation batch is already running."));
        return FString();
    }

    Subsystem->GenerateTTSAudioForAllNodes(Dialogue, bForceRegenerate);
    return FString::Printf(TEXT("Voice generation started for '%s'. Progress and results appear as editor notifications."), *Dialogue->GetName());
}

FADSVoiceCasting UADSDialogueToolset::GetVoiceCasting(const FString& DialoguePath)
{
    FADSVoiceCasting Casting;
    UADSEditorSubsystem* Subsystem = ADSDialogueToolset::GetSubsystem();
    UADSDialogue* Dialogue = Subsystem ? ADSDialogueToolset::LoadDialogue(DialoguePath) : nullptr;
    UADSVoiceConfigDataAsset* VoiceConfig = Dialogue ? ADSDialogueToolset::GetVoiceConfig() : nullptr;
    if (!VoiceConfig)
    {
        return Casting;
    }

    if (!VoiceConfig->HasPersistedVoiceCatalog())
    {
        Subsystem->RefreshVoicesFromAPI();
        UKismetSystemLibrary::RaiseScriptError(TEXT("No voices are cached yet. 'Refresh Voices' has been started; call GetVoiceCasting again once the editor reports the voices were refreshed."));
        return Casting;
    }
    Casting.AvailableVoiceNames = VoiceConfig->GetCachedVoiceNames();

    TArray<FGameplayTag> Tags = Dialogue->AIParticipantTags;
    for (UAGSGraphNode* Node : Dialogue->GetAllNodes())
    {
        if (const UADSGraphNode* DialogueNode = Cast<UADSGraphNode>(Node))
        {
            if (DialogueNode->GetParticipantTag().IsValid())
            {
                Tags.AddUnique(DialogueNode->GetParticipantTag());
            }
        }
    }

    for (const FGameplayTag& Tag : Tags)
    {
        Casting.Participants.Add(ADSDialogueToolset::MakeCastingEntry(*VoiceConfig, Tag));
    }
    return Casting;
}

FADSVoiceCastingEntry UADSDialogueToolset::SetParticipantVoice(const FString& ParticipantTag, const FString& Voice)
{
    UADSVoiceConfigDataAsset* VoiceConfig = ADSDialogueToolset::GetVoiceConfig();
    if (!VoiceConfig)
    {
        return FADSVoiceCastingEntry();
    }

    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*ParticipantTag.TrimStartAndEnd()), false);
    if (!Tag.IsValid())
    {
        UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("'%s' is not a registered gameplay tag."), *ParticipantTag));
        return FADSVoiceCastingEntry();
    }

    const FString Requested = Voice.TrimStartAndEnd();
    FString VoiceName;
    FString VoiceID;
    if (VoiceConfig->TryGetCachedVoiceIDByName(Requested, VoiceID))
    {
        VoiceName = Requested;
    }
    else if (VoiceConfig->TryGetVoiceNameByID(Requested, VoiceName))
    {
        VoiceID = Requested;
    }
    else if (!Requested.IsEmpty() && !Requested.Contains(TEXT(" ")))
    {
        VoiceID = Requested;
    }
    else
    {
        UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("'%s' is not a cached voice name. Use a name from GetVoiceCasting or a raw voice id."), *Voice));
        return FADSVoiceCastingEntry();
    }

    VoiceConfig->Modify();
    FADSVoiceSettings& Settings = VoiceConfig->VoiceConfigs.FindOrAdd(Tag);
    Settings.VoiceID = VoiceID;
    Settings.SelectedVoiceName = VoiceName;
    VoiceConfig->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SavePackages({ VoiceConfig->GetPackage() }, /*bOnlyDirty=*/true))
    {
        UKismetSystemLibrary::RaiseScriptError(FString::Printf(
            TEXT("Failed to save voice assignment for '%s' to ADS Voice Config '%s'; the change remains in memory."),
            *ParticipantTag, *VoiceConfig->GetPathName()));
        return FADSVoiceCastingEntry();
    }

    return ADSDialogueToolset::MakeCastingEntry(*VoiceConfig, Tag);
}

FADSDialogueSaveResult UADSDialogueToolset::SaveDialogue(const FString& DialoguePath, const TArray<FString>& ParticipantTags,
    const FString& ConversationDescription, const FString& PlayerResponseMode,
    int32 TreeLength, const FString& DialogueKind)
{
    FADSDialogueSaveResult Result;
    FString Error;

    ADSDialogueToolset::FDialogueTarget Target;
    if (!ADSDialogueToolset::ResolveTarget(DialoguePath, Target, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return Result;
    }

    UClass* RequestedClass = nullptr;
    bool bKindGiven = false;
    if (!ADSDialogueToolset::ResolveKind(DialogueKind, RequestedClass, bKindGiven, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return Result;
    }

    TArray<FGameplayTag> NewParticipants;
    if (!ADSDialogueToolset::ResolveParticipantTags(ParticipantTags, NewParticipants, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return Result;
    }

    EADSAIPlayerResponseMode NewMode = EADSAIPlayerResponseMode::Mixed;
    const bool bSetMode = !PlayerResponseMode.TrimStartAndEnd().IsEmpty();
    if (bSetMode && !ADSDialogueToolset::ParseResponseMode(PlayerResponseMode, NewMode, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return Result;
    }

    if (TreeLength < 0)
    {
        UKismetSystemLibrary::RaiseScriptError(TEXT("TreeLength cannot be negative. Pass 0 to leave it unchanged."));
        return Result;
    }

    UObject* Existing = FindObject<UObject>(nullptr, *Target.ObjectPath);
    if (!Existing && FPackageName::DoesPackageExist(Target.PackageName))
    {
        Existing = LoadObject<UObject>(nullptr, *Target.ObjectPath);
        if (!Existing)
        {
            UKismetSystemLibrary::RaiseScriptError(FString::Printf(
                TEXT("Package '%s' already exists but holds no asset named '%s'. Pick another path."), *Target.PackageName, *Target.AssetName));
            return Result;
        }
    }

    UADSDialogue* Dialogue = nullptr;
    if (Existing)
    {
        Dialogue = Cast<UADSDialogue>(Existing);
        if (!Dialogue)
        {
            UKismetSystemLibrary::RaiseScriptError(FString::Printf(
                TEXT("'%s' already holds a %s, not an ADS dialogue."), *Target.ObjectPath, *Existing->GetClass()->GetName()));
            return Result;
        }
        if (bKindGiven && Dialogue->GetClass() != RequestedClass)
        {
            UKismetSystemLibrary::RaiseScriptError(FString::Printf(
                TEXT("'%s' is a %s; its kind cannot change to %s."), *Target.ObjectPath, *Dialogue->GetClass()->GetName(), *RequestedClass->GetName()));
            return Result;
        }
    }

    UClass* EffectiveClass = Dialogue ? Dialogue->GetClass() : RequestedClass;
    if (bSetMode && EffectiveClass->IsChildOf(UADSWorldDialogue::StaticClass()) && NewMode != EADSAIPlayerResponseMode::DialogueNodes)
    {
        UKismetSystemLibrary::RaiseScriptError(TEXT("World dialogues never use button-answer nodes, so PlayerResponseMode must be \"DialogueNodes\" for them."));
        return Result;
    }

    const bool bCreated = Dialogue == nullptr;
    if (bCreated)
    {
        UPackage* Package = CreatePackage(*Target.PackageName);
        UADSDialogueFactory* Factory = NewObject<UADSDialogueFactory>();
        Dialogue = Cast<UADSDialogue>(Factory->FactoryCreateNew(
            RequestedClass, Package, FName(*Target.AssetName), RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
        if (!Dialogue)
        {
            UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("Could not create a dialogue at '%s'."), *Target.ObjectPath));
            return Result;
        }
    }
    else
    {
        Dialogue->Modify();
    }

    if (!NewParticipants.IsEmpty())
    {
        Dialogue->AIParticipantTags = NewParticipants;
    }
    if (!ConversationDescription.TrimStartAndEnd().IsEmpty())
    {
        Dialogue->AIConversationDescription = ConversationDescription.TrimStartAndEnd();
    }
    if (bSetMode)
    {
        Dialogue->AIPlayerResponseMode = NewMode;
    }
    if (TreeLength > 0)
    {
        Dialogue->AIDialogueTreeLength = TreeLength;
    }

    Dialogue->MarkPackageDirty();
    if (bCreated)
    {
        FAssetRegistryModule::AssetCreated(Dialogue);
    }
    if (!UEditorLoadingAndSavingUtils::SavePackages({ Dialogue->GetPackage() }, /*bOnlyDirty=*/false))
    {
        UKismetSystemLibrary::RaiseScriptError(FString::Printf(
            TEXT("Failed to save '%s'; the %s remains in memory."), *Target.ObjectPath, bCreated ? TEXT("new dialogue") : TEXT("change")));
        return Result;
    }

    Result.ObjectPath = Target.ObjectPath;
    Result.bCreated = bCreated;
    Result.DialogueClass = Dialogue->GetClass()->GetName();
    for (const FGameplayTag& Tag : Dialogue->AIParticipantTags)
    {
        Result.ParticipantTags.Add(Tag.ToString());
    }
    Result.ConversationDescription = Dialogue->AIConversationDescription;
    Result.PlayerResponseMode = StaticEnum<EADSAIPlayerResponseMode>()->GetNameStringByValue(static_cast<int64>(Dialogue->AIPlayerResponseMode));
    Result.TreeLength = Dialogue->AIDialogueTreeLength;

    if (Dialogue->AIParticipantTags.IsEmpty())
    {
        Result.Warnings.Add(TEXT("No participant tags yet: GetWritingBrief refuses until at least one speaker is set."));
    }
    else if (!ADSDialogueToolset::HasPlayerParticipant(Dialogue->AIParticipantTags))
    {
        Result.Warnings.Add(TEXT("No participant tag has \"Player\" in its last segment, so the player gets no response options."));
    }
    for (const FGameplayTag& Tag : Dialogue->AIParticipantTags)
    {
        if (!Tag.ToString().StartsWith(TEXT("Character.")))
        {
            Result.Warnings.Add(FString::Printf(TEXT("'%s' is not under Character.*, so the Voice Config cannot give it a voice."), *Tag.ToString()));
        }
    }
    if (Dialogue->AIConversationDescription.IsEmpty())
    {
        Result.Warnings.Add(TEXT("No conversation description: pass one to GetWritingBrief or set it here."));
    }
    return Result;
}
