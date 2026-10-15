// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSEditorSubsystem.h"

#include "ADSDialogueDeveloperSettings.h"
#include "ADSDialogueFunctionLibrary.h"
#include "ADSDialoguePartecipantComponent.h"
#include "ADSDialogueSubsystem.h"
#include "ADSVoiceConfigDataAsset.h"
#include "ADSTypes.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Graph/ADSDialogue.h"
#include "Graph/ADSDialogueResponseNode.h"
#include "Graph/ADSStartDialogueNode.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "TimerManager.h"
#include "EditorAssetLibrary.h"
#include "Factories/SoundFactory.h"
#include "PropertyEditorModule.h"
#include "AudioDevice.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Graph/ADSGraphNode.h"
#include "HAL/PlatformFileManager.h"
#include "HttpModule.h"
#include "IAssetTools.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/Notifications/SNotificationList.h"
#include <HttpFwd.h>
#include <Interfaces/IHttpRequest.h>
#include <Kismet/GameplayStatics.h>
#include <Sound/SoundCue.h>
#include <Sound/SoundNodeWavePlayer.h>
#include <Sound/SoundWave.h>
#include "AGSGraph.h"
#include "EdNode_AGSGraphNode.h"
#include "EdNode_AGSGraphEdge.h"
#include "EdGraph_AGSGraph.h"
#include "AssetGraphSchema_AGSGraph.h"
#include "Graph/ADSDialogueNode.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "GameplayTagsManager.h"
#include "ScopedTransaction.h"
#include "Misc/MessageDialog.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/Skeleton.h"
#include "Curves/RichCurve.h"
#include "Engine/DataTable.h"
#include "ADSTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include <initializer_list>
#if WITH_ADS_METAHUMAN
#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceExportUtils.h"
#include "AudioDrivenAnimationConfig.h"
#include "AudioDrivenAnimationMood.h"
#endif // WITH_ADS_METAHUMAN

static FString SanitizeAssetName(const FString& Input, int32 MaxWords = 3, int32 MaxLen = 30)
{
    FString Result;
    int32 WordCount = 0;
    bool bPrevWasSpace = true;

    for (int32 i = 0; i < Input.Len(); ++i)
    {
        const TCHAR Ch = Input[i];
        if (FChar::IsWhitespace(Ch))
        {
            if (!bPrevWasSpace && WordCount < MaxWords)
            {
                Result += TEXT("_");
            }
            bPrevWasSpace = true;
            WordCount++;
            continue;
        }
        if (bPrevWasSpace)
        {
            bPrevWasSpace = false;
        }
        if (WordCount >= MaxWords) break;
        if (FChar::IsAlnum(Ch))
        {
            Result += Ch;
        }
    }
    if (Result.EndsWith(TEXT("_")))
    {
        Result.LeftChopInline(1);
    }
    if (Result.Len() > MaxLen)
    {
        Result.LeftInline(MaxLen);
    }
    return Result.IsEmpty() ? TEXT("FacialAnim") : Result;
}

UADSEditorSubsystem::UADSEditorSubsystem()
{
}

namespace ADSEditorAudioHelper
{
    FString PackageDirToContentDir(FString PackageDir)
    {
        PackageDir.ReplaceInline(TEXT("\\"), TEXT("/"));
        PackageDir.RemoveFromStart(TEXT("/Game/"));
        return FPaths::ProjectContentDir() / PackageDir;
    }

    void EnsureContentDirectoryExists(const FString& PackageDir)
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        const FString ContentPath = PackageDirToContentDir(PackageDir);
        if (!PlatformFile.DirectoryExists(*ContentPath))
        {
            PlatformFile.CreateDirectoryTree(*ContentPath);
        }
    }

    void RefreshSoundWaveForEditorPlayback(USoundWave* SoundWave)
    {
        if (!SoundWave)
        {
            return;
        }

        if (GEditor)
        {
            GEditor->ResetPreviewAudioComponent();
        }

        if (GEngine)
        {
            if (FAudioDevice* AudioDevice = GEngine->GetMainAudioDeviceRaw())
            {
                AudioDevice->StopAllSounds(true);
                AudioDevice->Flush(nullptr);
            }
        }

        SoundWave->InvalidateCompressedData(true, true);
        SoundWave->PostEditChange();
        SoundWave->MarkPackageDirty();
    }

    void AssignGeneratedAudioToNode(UADSGraphNode* DialogueNode, USoundWave* SoundWave)
    {
        if (!DialogueNode || !SoundWave)
        {
            return;
        }

        DialogueNode->Modify();
        DialogueNode->SetSoundToPlay(SoundWave);

        if (FProperty* SoundProp = UADSGraphNode::StaticClass()->FindPropertyByName(TEXT("SoundToPlay")))
        {
            FPropertyChangedEvent PropertyEvent(SoundProp, EPropertyChangeType::ValueSet);
            DialogueNode->PostEditChangeProperty(PropertyEvent);
        }

        DialogueNode->MarkPackageDirty();

        if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
        {
            TArray<UObject*> ObjectsToUpdate;
            ObjectsToUpdate.Add(DialogueNode);
            PropertyEditor->UpdatePropertyViews(ObjectsToUpdate);
        }
    }

    UADSDialogue* GetOwningDialogue(const UADSGraphNode* DialogueNode)
    {
        return DialogueNode ? Cast<UADSDialogue>(DialogueNode->GetGraph()) : nullptr;
    }

    bool PackageAssetExists(FString PackageDir, const FString& AssetName)
    {
        PackageDir.ReplaceInline(TEXT("\\"), TEXT("/"));
        if (!PackageDir.EndsWith(TEXT("/")))
        {
            PackageDir += TEXT("/");
        }

        const FString FullPackage = PackageDir + AssetName;
        if (FPackageName::DoesPackageExist(FullPackage))
        {
            return true;
        }

        const FString ObjectPath = FullPackage + TEXT(".") + AssetName;
        return FindObject<UObject>(nullptr, *ObjectPath) != nullptr;
    }

    FString MakeVersionedAssetName(const FString& PackageDir, const FString& BaseName)
    {
        if (BaseName.IsEmpty() || !PackageAssetExists(PackageDir, BaseName))
        {
            return BaseName;
        }

        for (int32 Version = 2; Version <= 999; ++Version)
        {
            const FString Candidate = FString::Printf(TEXT("%s_V%d"), *BaseName, Version);
            if (!PackageAssetExists(PackageDir, Candidate))
            {
                return Candidate;
            }
        }

        return FString::Printf(TEXT("%s_%s"), *BaseName, *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
    }
}

USoundWave* UADSEditorSubsystem::CreateSoundWaveAsset(const TArray<uint8>& AudioData, const FString& AssetName, const FString& PackagePath)
{
    if (AudioData.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("Cannot create sound wave from empty audio data"));
        return nullptr;
    }

    FString NormalizedPackagePath = PackagePath;
    NormalizedPackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!NormalizedPackagePath.EndsWith(TEXT("/")))
    {
        NormalizedPackagePath += TEXT("/");
    }

    const FString UniqueAssetName = ADSEditorAudioHelper::MakeVersionedAssetName(NormalizedPackagePath, AssetName);

    const FString TempDir = FPaths::ProjectIntermediateDir() / TEXT("VoiceTTS");
    const FString TempFilePath = TempDir / (UniqueAssetName + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".mp3"));

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*TempDir)) {
        PlatformFile.CreateDirectoryTree(*TempDir);
    }

    if (!FFileHelper::SaveArrayToFile(AudioData, *TempFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to save audio data to temp file: %s"), *TempFilePath);
        return nullptr;
    }

    const FString FullPackagePath = NormalizedPackagePath + UniqueAssetName;
    ADSEditorAudioHelper::EnsureContentDirectoryExists(NormalizedPackagePath);

    USoundFactory* SoundFactory = NewObject<USoundFactory>();
    SoundFactory->SuppressImportDialogs();
    SoundFactory->bAutoCreateCue = false;

    UPackage* Package = CreatePackage(*FullPackagePath);
    if (Package)
    {
        Package->FullyLoad();
    }

    bool bCancelled = false;
    UObject* ImportedObject = SoundFactory->ImportObject(
        USoundWave::StaticClass(),
        Package,
        FName(*UniqueAssetName),
        RF_Public | RF_Standalone,
        TempFilePath,
        nullptr,
        bCancelled);

    PlatformFile.DeleteFile(*TempFilePath);

    USoundWave* ImportedSoundWave = Cast<USoundWave>(ImportedObject);
    if (ImportedSoundWave)
    {
        FAssetRegistryModule::AssetCreated(ImportedSoundWave);
        ImportedSoundWave->bLooping = false;
        ImportedSoundWave->Volume = 1.0f;
        ImportedSoundWave->Pitch = 1.0f;
        ImportedSoundWave->MarkPackageDirty();
        ADSEditorAudioHelper::RefreshSoundWaveForEditorPlayback(ImportedSoundWave);
        UE_LOG(LogTemp, Log, TEXT("Successfully imported TTS audio as USoundWave: %s"), *FullPackagePath);
        return ImportedSoundWave;
    }

    UE_LOG(LogTemp, Error, TEXT("Failed to import audio file as USoundWave: %s"), *TempFilePath);
    return nullptr;
}

USoundCue* UADSEditorSubsystem::CreateSoundCueFromWave(USoundWave* SoundWave, const FString& CueName)
{
    if (!SoundWave) {
        UE_LOG(LogTemp, Error, TEXT("Cannot create SoundCue: SoundWave is null"));
        return nullptr;
    }

    // Get the package path from the SoundWave
    const FString SoundWavePackagePath = SoundWave->GetPackage()->GetName();
    const FString CuePackagePath = SoundWavePackagePath + TEXT("_Cue");

    // Create the sound cue asset
    IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

    USoundCue* SoundCue = Cast<USoundCue>(AssetTools.CreateAsset(CueName, FPackageName::GetLongPackagePath(CuePackagePath), USoundCue::StaticClass(), nullptr));

    if (!SoundCue) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create USoundCue asset: %s"), *CueName);
        return nullptr;
    }

    // Create a USoundNodeWavePlayer node
    USoundNodeWavePlayer* WavePlayer = NewObject<USoundNodeWavePlayer>(SoundCue);
    if (!WavePlayer) {
        UE_LOG(LogTemp, Error, TEXT("Failed to create USoundNodeWavePlayer"));
        return nullptr;
    }

    WavePlayer->SetSoundWave(SoundWave);

    // Set up the sound cue properties
    SoundCue->FirstNode = WavePlayer;
    SoundCue->MaxDistance = 10000.0f;
    SoundCue->VolumeMultiplier = 1.0f;
    SoundCue->PitchMultiplier = 1.0f;

    // Reconstruct the cue safely without calling LinkGraphNodesFromSoundNodes
    SoundCue->PostEditChange();
    SoundCue->MarkPackageDirty();

    UE_LOG(LogTemp, Log, TEXT("Successfully created USoundCue: %s"), *SoundCue->GetName());

    return SoundCue;
}

void UADSEditorSubsystem::ShowNotification(const FString& Message, bool bSuccess /*= true*/) const
{
    FNotificationInfo Info(FText::FromString(Message));
    Info.ExpireDuration = 7.0f;
    Info.bFireAndForget = true;

    if (!bSuccess) {
        Info.Image = FCoreStyle::Get().GetBrush("NotificationList.FailImage");
    } else {
        Info.Image = FCoreStyle::Get().GetBrush("NotificationList.SuccessImage");
    }

    FSlateNotificationManager::Get().AddNotification(Info);
}

USoundWave* UADSEditorSubsystem::CreateSoundWaveFromAudioData(const TArray<uint8>& AudioData, const FString& SoundName)
{
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString PackagePath = TEXT("/Game/") + Settings->GetDefaultOutputPath();
    return CreateSoundWaveAsset(AudioData, SoundName, PackagePath);
}

namespace {
/** Appends a query parameter to an endpoint that may or may not already carry a query string. */
FString AppendQueryParam(const FString& URL, const FString& Param)
{
    return URL + (URL.Contains(TEXT("?")) ? TEXT("&") : TEXT("?")) + Param;
}

/**
 * ElevenLabs allows several voices to share a display name, but the catalog is keyed by name.
 * Suffix the duplicates so every voice stays reachable from the picker.
 */
FString MakeUniqueVoiceName(const FString& VoiceName, const TMap<FString, FString>& ExistingNameToID)
{
    if (!ExistingNameToID.Contains(VoiceName)) {
        return VoiceName;
    }

    for (int32 Suffix = 2; Suffix < 100; ++Suffix) {
        const FString Candidate = FString::Printf(TEXT("%s (%d)"), *VoiceName, Suffix);
        if (!ExistingNameToID.Contains(Candidate)) {
            return Candidate;
        }
    }

    return VoiceName;
}
}

void UADSEditorSubsystem::RefreshVoicesFromAPI()
{
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString APIKey = Settings->GetTTSVoiceGenAPIKey();

    if (APIKey.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("No TTS Voice API key set - cannot refresh voices"));
        ShowNotification(TEXT("No TTS Voice API key set in Project Settings."), false);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Refreshing voices from TTS Voice API..."));

    if (UADSDialogueSubsystem* DialogueSubsystem = GEngine->GetEngineSubsystem<UADSDialogueSubsystem>()) {
        DialogueSubsystem->ClearVoiceCache();
    }

    FetchVoicePage(APIKey, FString(), MakeShared<TArray<FString>>(), MakeShared<TMap<FString, FString>>(), 0);
}

void UADSEditorSubsystem::FetchVoicePage(const FString& APIKey, const FString& NextPageToken, TSharedRef<TArray<FString>> AccumulatedNames,
    TSharedRef<TMap<FString, FString>> AccumulatedNameToID, int32 PageIndex)
{
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const int32 MaxVoices = FMath::Max(1, Settings->GetMaxVoicesToFetch());
    const int32 PageSize = FMath::Clamp(MaxVoices, 1, 100);

    FString URL = AppendQueryParam(Settings->GetTTSVoiceSelectionAPIEndPoint(), FString::Printf(TEXT("page_size=%d"), PageSize));
    if (!NextPageToken.IsEmpty()) {
        URL = AppendQueryParam(URL, TEXT("next_page_token=") + FGenericPlatformHttp::UrlEncode(NextPageToken));
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(URL);
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("xi-api-key"), APIKey);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    TWeakObjectPtr<UADSEditorSubsystem> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, APIKey, AccumulatedNames, AccumulatedNameToID, PageIndex, MaxVoices](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful) {
            UADSEditorSubsystem* Self = WeakThis.Get();
            if (!Self) {
                return;
            }

            if (!bWasSuccessful || !HttpResponse.IsValid() || HttpResponse->GetResponseCode() != 200) {
                const FString ErrorMsg = HttpResponse.IsValid()
                    ? FString::Printf(TEXT("HTTP %d: %s"), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString())
                    : TEXT("Request failed (no response from TTS Voice API).");
                UE_LOG(LogTemp, Error, TEXT("Failed to refresh voices from TTS Voice API: %s"), *ErrorMsg);
                Self->ShowNotification(ErrorMsg, false);
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());

            if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) {
                UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response from TTS Voice API"));
                Self->ShowNotification(TEXT("Failed to parse the TTS Voice API response."), false);
                return;
            }

            const TArray<TSharedPtr<FJsonValue>>* VoicesArray;
            if (!JsonObject->TryGetArrayField(TEXT("voices"), VoicesArray)) {
                UE_LOG(LogTemp, Error, TEXT("No 'voices' array found in API response"));
                Self->ShowNotification(TEXT("No 'voices' array in the TTS Voice API response."), false);
                return;
            }

            for (const TSharedPtr<FJsonValue>& VoiceValue : *VoicesArray) {
                const TSharedPtr<FJsonObject> VoiceObj = VoiceValue->AsObject();
                if (!VoiceObj.IsValid()) {
                    continue;
                }

                FString VoiceName;
                FString VoiceId;
                if (!VoiceObj->TryGetStringField(TEXT("name"), VoiceName) || !VoiceObj->TryGetStringField(TEXT("voice_id"), VoiceId)) {
                    continue;
                }

                const FString UniqueName = MakeUniqueVoiceName(VoiceName, *AccumulatedNameToID);
                AccumulatedNames->Add(UniqueName);
                AccumulatedNameToID->Add(UniqueName, VoiceId);
            }

            // v1 endpoints return the whole list at once and carry no pagination fields, so this stops after one page.
            bool bHasMore = false;
            JsonObject->TryGetBoolField(TEXT("has_more"), bHasMore);

            FString NewPageToken;
            JsonObject->TryGetStringField(TEXT("next_page_token"), NewPageToken);

            const bool bRoomLeft = AccumulatedNames->Num() < MaxVoices;
            const bool bMadeProgress = VoicesArray->Num() > 0;

            if (bHasMore && bRoomLeft && bMadeProgress && !NewPageToken.IsEmpty()) {
                Self->FetchVoicePage(APIKey, NewPageToken, AccumulatedNames, AccumulatedNameToID, PageIndex + 1);
                return;
            }

            Self->CommitVoiceCatalog(*AccumulatedNames, *AccumulatedNameToID, APIKey);
        });

    Request->ProcessRequest();
}

void UADSEditorSubsystem::CommitVoiceCatalog(const TArray<FString>& VoiceNames, const TMap<FString, FString>& VoiceNameToID, const FString& APIKey)
{
    if (UADSDialogueSubsystem* DialogueSubsystem = GEngine->GetEngineSubsystem<UADSDialogueSubsystem>()) {
        DialogueSubsystem->UpdateVoiceCache(VoiceNames, VoiceNameToID, APIKey);
    }

    // Persist name→ID catalog on the VoiceConfig DataAsset so VoiceIDs survive editor restart
    const UADSDialogueDeveloperSettings* DialogueSettings = GetDefault<UADSDialogueDeveloperSettings>();
    if (UADSVoiceConfigDataAsset* VoiceConfig = DialogueSettings->GetVoiceConfigDataAsset()) {
        VoiceConfig->PersistVoiceCatalog(VoiceNames, VoiceNameToID);
        UEditorLoadingAndSavingUtils::SavePackages({ VoiceConfig->GetPackage() }, /*bOnlyDirty=*/true);
        UE_LOG(LogTemp, Log, TEXT("Persisted %d voices to VoiceConfig DataAsset"), VoiceNames.Num());
    } else {
        UE_LOG(LogTemp, Warning, TEXT("No VoiceConfig DataAsset set in Ascent Dialogue Settings — voice catalog not saved to disk"));
    }

    UE_LOG(LogTemp, Log, TEXT("Successfully refreshed %d voices from TTS Voice API"), VoiceNames.Num());
    UE_LOG(LogTemp, Log, TEXT("Reopen the Details Panel to see updated voice dropdown"));
    ShowNotification(FString::Printf(TEXT("%d voices refreshed!"), VoiceNames.Num()), true);
}

void UADSEditorSubsystem::ClearVoiceCache()
{
    if (UADSDialogueSubsystem* DialogueSubsystem = GEngine->GetEngineSubsystem<UADSDialogueSubsystem>()) {
        // Clear current cache
        DialogueSubsystem->ClearVoiceCache();
    }
    UE_LOG(LogTemp, Log, TEXT("TTS voice cache cleared"));
    ShowNotification("Voice cache cleared. Click 'Refresh Voices' to reload.", true);
}

void UADSEditorSubsystem::PreviewSelectedVoice(const FString& voiceId)
{
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString APIKey = Settings->GetTTSVoiceGenAPIKey();

    if (APIKey.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("TTS Voice API Key not set in Project Settings"));
        return;
    }

    if (voiceId.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("No Voice ID specified"));
        return;
    }

    // Make HTTP request to get voice preview
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Settings->GetTTSVoiceSelectionAPIEndPoint());
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("xi-api-key"), APIKey);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    Request->OnProcessRequestComplete().BindLambda([this, voiceId](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful) {
        if (bWasSuccessful && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == 200) {
            const FString ResponseString = HttpResponse->GetContentAsString();
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

            if (FJsonSerializer::Deserialize(Reader, JsonObject)) {
                // Find the voice and get preview URL
                const TArray<TSharedPtr<FJsonValue>>* VoicesArray;
                if (JsonObject->TryGetArrayField(TEXT("voices"), VoicesArray)) {
                    for (const auto& VoiceValue : *VoicesArray) {
                        const TSharedPtr<FJsonObject> VoiceObj = VoiceValue->AsObject();
                        FString VoiceIdFromAPI;
                        if (VoiceObj->TryGetStringField(TEXT("voice_id"), VoiceIdFromAPI) && VoiceIdFromAPI == voiceId) {
                            FString PreviewURL;
                            if (VoiceObj->TryGetStringField(TEXT("preview_url"), PreviewURL)) {
                                // Download and play preview audio
                                DownloadAndPlayPreviewAudio(PreviewURL, voiceId);
                            } else {
                                UE_LOG(LogTemp, Warning, TEXT("No preview URL found for voice ID: %s"), *voiceId);
                            }
                            return;
                        }
                    }
                    UE_LOG(LogTemp, Warning, TEXT("Voice ID not found: %s"), *voiceId);
                }
            }
        } else {
            UE_LOG(LogTemp, Error, TEXT("Failed to fetch voice data for preview"));
            ShowNotification("Failed to fetch voice data for preview", false);
        }
    });

    Request->ProcessRequest();
}

void UADSEditorSubsystem::DownloadAndPlayPreviewAudio(const FString& PreviewURL, const FString& voiceID)
{
    // Generate cache file path based on voice ID

    const FString CachedFilePath = FPaths::ProjectIntermediateDir() / TEXT("TTSCache") / FString::Printf(TEXT("Preview_%s.mp3"), *voiceID);

    // First check if USoundWave asset already exists in samples folder
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    FString AssetName = FString::Printf(TEXT("Preview_%s"), *voiceID);
    FString AssetPath = FString::Printf(TEXT("/Game/%s%s"), *Settings->GetPreviewSamplesPath(), *AssetName);

    if (UObject* ExistingAsset = StaticLoadObject(USoundWave::StaticClass(), nullptr, *AssetPath)) {
        USoundWave* ExistingSoundWave = Cast<USoundWave>(ExistingAsset);
        if (ExistingSoundWave) {
            UE_LOG(LogTemp, Log, TEXT("Playing existing preview asset: %s"), *AssetPath);

            // Play existing asset directly
            if (UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr) {
                UGameplayStatics::PlaySound2D(World, ExistingSoundWave);
            }

            ShowNotification("Playing voice preview..", true);
            return;
        }
    }

    // Check if cached preview file exists
    if (FPaths::FileExists(CachedFilePath)) {
        UE_LOG(LogTemp, Log, TEXT("Playing cached voice preview from: %s"), *CachedFilePath);

        // Play cached file directly in Unreal
        PlayAudioFileInUnreal(CachedFilePath);
        ShowNotification("Playing voice preview..", true);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Downloading voice preview from API..."));

    // Create HTTP request to download preview audio
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(PreviewURL);
    Request->SetVerb(TEXT("GET"));

    Request->OnProcessRequestComplete().BindLambda([this, CachedFilePath](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful) {
        if (bWasSuccessful && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == 200) {
            TArray<uint8> AudioData = HttpResponse->GetContent();
            UE_LOG(LogTemp, Log, TEXT("Downloaded preview audio: %d bytes"), AudioData.Num());

            // Ensure cache directory exists
            FString CacheDir = FPaths::GetPath(CachedFilePath);
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
            if (!PlatformFile.DirectoryExists(*CacheDir)) {
                PlatformFile.CreateDirectoryTree(*CacheDir);
            }

            // Save to cached file path
            if (FFileHelper::SaveArrayToFile(AudioData, *CachedFilePath)) {
                UE_LOG(LogTemp, Log, TEXT("Cached and playing voice preview from: %s"), *CachedFilePath);

                // Play downloaded file in Unreal
                PlayAudioFileInUnreal(CachedFilePath);

                // Show notification

                ShowNotification("Playing voice preview..", true);

            } else {
                UE_LOG(LogTemp, Error, TEXT("Failed to save preview audio to cache file"));
            }
        } else {
            UE_LOG(LogTemp, Error, TEXT("Failed to download preview audio"));
            ShowNotification("Failed to download preview audio", false);
        }
    });

    Request->ProcessRequest();
}

void UADSEditorSubsystem::PlayAudioFileInUnreal(const FString& FilePath)
{
    // Load audio data from file
    TArray<uint8> AudioData;
    if (!FFileHelper::LoadFileToArray(AudioData, *FilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to load audio file: %s"), *FilePath);
        return;
    }

    // Create asset name for preview
    const FString FileName = FPaths::GetBaseFilename(FilePath);
    const FString AssetName = FString::Printf(TEXT("Preview_%s"), *FileName);

    // Use configurable preview samples path
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString PackagePath = TEXT("/Game/") + Settings->GetPreviewSamplesPath();

    // Create USoundWave asset
    USoundWave* PreviewSoundWave = CreateSoundWaveAsset(AudioData, AssetName, PackagePath);

    if (PreviewSoundWave) {
        // Play in editor world
        if (const UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr) {
            UGameplayStatics::PlaySound2D(World, PreviewSoundWave);
            UE_LOG(LogTemp, Log, TEXT("Playing preview audio in Unreal Engine: %s"), *PreviewSoundWave->GetName());
        }
    } else {
        UE_LOG(LogTemp, Error, TEXT("Failed to create preview audio asset from: %s"), *FilePath);
    }
}

void UADSEditorSubsystem::GenerateTTSAudio(UADSGraphNode* dialogueNode)
{
    // Thin wrapper around the shared core: surface every result (success or failure) as a notification.
    RequestTTSForNode(dialogueNode, [this](bool bSuccess, const FString& Message) {
        ShowNotification(Message, bSuccess);
    });
}

void UADSEditorSubsystem::RequestTTSForNode(UADSGraphNode* DialogueNode, TFunction<void(bool, const FString&)> OnComplete)
{
    // Guarantee the callback is always invoked exactly once, even on early-out paths.
    auto Fail = [&OnComplete](const FString& Message) {
        UE_LOG(LogTemp, Warning, TEXT("Generate TTS: %s"), *Message);
        if (OnComplete) {
            OnComplete(false, Message);
        }
    };

    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    if (!Settings) {
        Fail(TEXT("Dialogue developer settings are unavailable."));
        return;
    }

    const FString APIKey = Settings->GetTTSVoiceGenAPIKey();
    if (APIKey.IsEmpty()) {
        Fail(TEXT("TTS API Key not set in Project Settings."));
        return;
    }

    if (!DialogueNode) {
        Fail(TEXT("Dialogue node is null - cannot generate TTS audio."));
        return;
    }

    FADSVoiceSettings VoiceConfig;
    if (!DialogueNode->TryGetParticipantVoiceConfig(VoiceConfig)) {
        Fail(TEXT("Dialogue node has no voice config - assign a participant voice first."));
        return;
    }

    const FString UseVoiceID = VoiceConfig.GetSelectedVoiceID();
    if (UseVoiceID.IsEmpty()) {
        Fail(TEXT("No Voice ID specified for this node's participant."));
        return;
    }

    const FText Text = DialogueNode->GetTextForAIGeneration();
    if (Text.IsEmpty()) {
        Fail(DialogueNode->GetUsesSeparateAIGenerationText()
            ? TEXT("Node has 'Use Separate AI Generation Text' enabled but AI Generation Text is empty.")
            : TEXT("Node has no dialogue text to convert to speech."));
        return;
    }

    const FString VoiceModel = Settings->GetTTSVoiceModel();
    if (VoiceModel.IsEmpty()) {
        Fail(TEXT("Voice model is empty - set 'Voice Generation Model' in Project Settings (e.g. eleven_v3)."));
        return;
    }
    if (!UADSDialogueDeveloperSettings::IsKnownVoiceModel(VoiceModel)) {
        UE_LOG(LogTemp, Warning, TEXT("Generate TTS: unknown voice model '%s' - sending it to the API anyway."), *VoiceModel);
    }

    // Create JSON payload
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetStringField(TEXT("text"), Text.ToString());
    JsonObject->SetStringField(TEXT("model_id"), VoiceModel);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    const FString finalEndpoint = Settings->GetTTSVoiceGenerationEndPoint() + "/" + UseVoiceID;

    // Make HTTP request to generate TTS
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(finalEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("xi-api-key"), APIKey);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(JsonString);

    TWeakObjectPtr<UADSGraphNode> WeakNode(DialogueNode);

    Request->OnProcessRequestComplete().BindLambda([this, Settings, WeakNode, OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
        auto Report = [&OnComplete](bool bSuccess, const FString& Message) {
            if (bSuccess) {
                UE_LOG(LogTemp, Log, TEXT("Generate TTS: %s"), *Message);
            } else {
                UE_LOG(LogTemp, Error, TEXT("Generate TTS: %s"), *Message);
            }
            if (OnComplete) {
                OnComplete(bSuccess, Message);
            }
        };

        if (!(bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)) {
            const FString ErrorMsg = Response.IsValid()
                ? FString::Printf(TEXT("HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString())
                : TEXT("Request failed (no response from TTS service).");
            Report(false, ErrorMsg);
            return;
        }

        UADSGraphNode* DialogueNode = WeakNode.Get();
        if (!DialogueNode) {
            Report(false, TEXT("Dialogue node was destroyed before TTS audio could be assigned."));
            return;
        }

        TArray<uint8> AudioData = Response->GetContent();
        if (AudioData.Num() == 0) {
            Report(false, TEXT("TTS service returned empty audio data."));
            return;
        }
        UE_LOG(LogTemp, Log, TEXT("Generated TTS audio: %d bytes"), AudioData.Num());

        const FGameplayTag PartecipantTag = DialogueNode->GetParticipantTag();
        FString ParticipantName = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(PartecipantTag);
        if (ParticipantName.IsEmpty() || ParticipantName == TEXT("None")) {
            ParticipantName = TEXT("Dialogue");
        }
        const FString TextSnippet = SanitizeAssetName(DialogueNode->GetDialogueText().ToString());
        const FString AssetName = FString::Printf(TEXT("TTS_%s_%s"), *ParticipantName, *TextSnippet);
        FString PackagePath = TEXT("/Game/") + Settings->GetFinalTTSPath();
        if (UADSDialogue* OwningDialogue = ADSEditorAudioHelper::GetOwningDialogue(DialogueNode))
        {
            PackagePath = OwningDialogue->ResolveVoiceOutputPackagePath();
        }

        USoundWave* NewSoundWave = CreateSoundWaveAsset(AudioData, AssetName, PackagePath);
        if (!NewSoundWave) {
            // Fallback: save to temp file so the generated audio isn't lost.
            const FString TempFilePath = FPaths::ProjectDir() / TEXT("Temp") / (AssetName + TEXT(".mp3"));
            if (FFileHelper::SaveArrayToFile(AudioData, *TempFilePath)) {
                UE_LOG(LogTemp, Log, TEXT("TTS audio saved to fallback file: %s"), *TempFilePath);
            }
            Report(false, FString::Printf(TEXT("Failed to import TTS audio as a SoundWave asset (%s)."), *AssetName));
            return;
        }

        if (Settings->GetAutoAssignGeneratedAudio()) {
            ADSEditorAudioHelper::AssignGeneratedAudioToNode(DialogueNode, NewSoundWave);
        }
        if (NewSoundWave->GetPackage()) {
            NewSoundWave->GetPackage()->SetDirtyFlag(true);
        }

        Report(true, FString::Printf(TEXT("TTS Audio Generated: %s"), *NewSoundWave->GetName()));
    });

    Request->ProcessRequest();
}

void UADSEditorSubsystem::GenerateTTSAudioForAllNodes(UADSDialogue* Dialogue, bool bForceRegenerate)
{
    if (!Dialogue) {
        ShowNotification(TEXT("No dialogue selected."), false);
        UE_LOG(LogTemp, Warning, TEXT("Generate All TTS: dialogue is null."));
        return;
    }

    if (bIsBatchGeneratingVoices) {
        ShowNotification(TEXT("Voice generation already in progress. Please wait for it to finish."), false);
        return;
    }

    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    if (!Settings || Settings->GetTTSVoiceGenAPIKey().IsEmpty()) {
        ShowNotification(TEXT("TTS API Key not set in Project Settings."), false);
        UE_LOG(LogTemp, Warning, TEXT("Generate All TTS: missing API key or settings."));
        return;
    }

    // Collect every dialogue node that actually has text to voice (skipping already-voiced ones
    // unless the caller forces regeneration).
    TArray<TWeakObjectPtr<UADSGraphNode>> NodesToProcess;
    const int32 Skipped = CollectVoiceNodes({ Dialogue }, NodesToProcess, !bForceRegenerate);

    if (NodesToProcess.Num() == 0) {
        const FString Msg = Skipped > 0
            ? FString::Printf(TEXT("All %d dialogue nodes already have a voice. Use 'Force' to regenerate."), Skipped)
            : FString(TEXT("No dialogue nodes with text were found to generate voices for."));
        ShowNotification(Msg, false);
        UE_LOG(LogTemp, Warning, TEXT("Generate All TTS: no eligible nodes in '%s' (%d already set)."), *Dialogue->GetName(), Skipped);
        return;
    }

    // Warn the user that this can be expensive (one API call per node) before proceeding.
    FString ConfirmMsg = FString::Printf(
        TEXT("Generate TTS voices for %d dialogue node(s)%s?\n\n"
             "This sends one request per node to the TTS service and may take a while "
             "and consume API credits.\n\n"),
        NodesToProcess.Num(), bForceRegenerate ? TEXT(" (FORCE: regenerating already-voiced nodes)") : TEXT(""));
    if (Skipped > 0) {
        ConfirmMsg += FString::Printf(TEXT("%d node(s) already voiced will be skipped.\n\n"), Skipped);
    }
    ConfirmMsg += TEXT("Continue?");

    if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmMsg)) != EAppReturnType::Yes) {
        return;
    }

    StartVoiceBatch(MoveTemp(NodesToProcess));
}

void UADSEditorSubsystem::GenerateAnimationsForAllNodes(UADSDialogue* Dialogue, bool bForceRegenerate)
{
    if (!Dialogue) {
        ShowNotification(TEXT("No dialogue selected."), false);
        UE_LOG(LogTemp, Warning, TEXT("Generate All Facial: dialogue is null."));
        return;
    }

    // Reuse the multi-dialogue batch path with a single-element selection.
    GenerateFacialAnimationForDialogues({ Dialogue }, bForceRegenerate);
}

int32 UADSEditorSubsystem::CollectVoiceNodes(const TArray<UADSDialogue*>& Dialogues, TArray<TWeakObjectPtr<UADSGraphNode>>& OutNodes, bool bSkipAlreadySet)
{
    int32 SkippedAlreadySet = 0;
    TSet<UADSGraphNode*> Seen;
    for (UADSDialogue* Dialogue : Dialogues) {
        if (!Dialogue) {
            continue;
        }
        for (UAGSGraphNode* Node : Dialogue->GetAllNodes()) {
            UADSGraphNode* DialogueNode = Cast<UADSGraphNode>(Node);
            if (!DialogueNode || DialogueNode->GetTextForAIGeneration().IsEmpty() || Seen.Contains(DialogueNode)) {
                continue;
            }
            Seen.Add(DialogueNode);
            if (bSkipAlreadySet && DialogueNode->GetSoundToPlay() != nullptr) {
                SkippedAlreadySet++;
                continue;
            }
            OutNodes.Add(DialogueNode);
        }
    }
    return SkippedAlreadySet;
}

void UADSEditorSubsystem::GenerateTTSForDialogues(const TArray<UADSDialogue*>& Dialogues, bool bForceRegenerate)
{
    if (bIsBatchGeneratingVoices) {
        ShowNotification(TEXT("Voice generation already in progress. Please wait for it to finish."), false);
        return;
    }

    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    if (!Settings || Settings->GetTTSVoiceGenAPIKey().IsEmpty()) {
        ShowNotification(TEXT("TTS API Key not set in Project Settings."), false);
        UE_LOG(LogTemp, Warning, TEXT("Generate TTS (selection): missing API key or settings."));
        return;
    }

    int32 ValidDialogues = 0;
    for (const UADSDialogue* Dialogue : Dialogues) {
        if (Dialogue) {
            ValidDialogues++;
        }
    }

    if (ValidDialogues == 0) {
        ShowNotification(TEXT("No valid dialogues selected."), false);
        return;
    }

    TArray<TWeakObjectPtr<UADSGraphNode>> NodesToProcess;
    const int32 Skipped = CollectVoiceNodes(Dialogues, NodesToProcess, !bForceRegenerate);

    if (NodesToProcess.Num() == 0) {
        const FString Msg = Skipped > 0
            ? FString::Printf(TEXT("All %d dialogue nodes in the selection already have a voice. Use 'Force' to regenerate."), Skipped)
            : FString(TEXT("No dialogue nodes with text were found in the selected dialogues."));
        ShowNotification(Msg, false);
        UE_LOG(LogTemp, Warning, TEXT("Generate TTS (selection): no eligible nodes (%d already set)."), Skipped);
        return;
    }

    FString ConfirmMsg = FString::Printf(
        TEXT("Generate TTS voices for %d dialogue node(s) across %d selected dialogue(s)%s?\n\n"
             "This sends one request per node to the TTS service and may take a while "
             "and consume API credits.\n\n"),
        NodesToProcess.Num(), ValidDialogues, bForceRegenerate ? TEXT(" (FORCE: regenerating already-voiced nodes)") : TEXT(""));
    if (Skipped > 0) {
        ConfirmMsg += FString::Printf(TEXT("%d node(s) already voiced will be skipped.\n\n"), Skipped);
    }
    ConfirmMsg += TEXT("Continue?");

    if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmMsg)) != EAppReturnType::Yes) {
        return;
    }

    StartVoiceBatch(MoveTemp(NodesToProcess));
}

void UADSEditorSubsystem::StartVoiceBatch(TArray<TWeakObjectPtr<UADSGraphNode>> NodesToProcess)
{
    if (NodesToProcess.Num() == 0) {
        return;
    }

    // Persistent "In Progress" notification with throbber, updated as the batch runs.
    FNotificationInfo ProgressInfo(FText::FromString(TEXT("Generating voices... (0 / ") + FString::FromInt(NodesToProcess.Num()) + TEXT(")")));
    ProgressInfo.bFireAndForget = false;
    ProgressInfo.bUseThrobber = true;
    ProgressInfo.FadeOutDuration = 0.5f;
    ProgressInfo.ExpireDuration = 0.0f;
    TSharedPtr<SNotificationItem> ProgressNotification = FSlateNotificationManager::Get().AddNotification(ProgressInfo);
    if (ProgressNotification.IsValid()) {
        ProgressNotification->SetCompletionState(SNotificationItem::CS_Pending);
    }

    bIsBatchGeneratingVoices = true;

    // Shared state carried across the sequential async chain. The Step closure is stored here
    // and captures the state weakly to avoid a reference cycle; the per-request completion
    // callbacks capture the state strongly, which keeps it alive across each async HTTP wait.
    struct FBatchState
    {
        TArray<TWeakObjectPtr<UADSGraphNode>> Nodes;
        int32 Index = 0;
        int32 SuccessCount = 0;
        int32 FailCount = 0;
        int32 SkippedAnswerCount = 0; // answer (button) nodes without a voice config — not an error
        TSharedPtr<SNotificationItem> Notification;
        TFunction<void()> Step;
    };

    TSharedRef<FBatchState> State = MakeShared<FBatchState>();
    State->Nodes = MoveTemp(NodesToProcess);
    State->Notification = ProgressNotification;

    TWeakPtr<FBatchState> WeakState = State;

    // Processes exactly one node, then schedules itself again from the completion callback.
    State->Step = [this, WeakState]() {
        TSharedPtr<FBatchState> S = WeakState.Pin();
        if (!S.IsValid()) {
            return;
        }

        const int32 Total = S->Nodes.Num();

        if (S->Index >= Total) {
            // Batch finished: report a summary and clear the in-progress state.
            bIsBatchGeneratingVoices = false;

            const bool bAllOk = (S->FailCount == 0);
            FString Summary = FString::Printf(
                TEXT("Voice generation complete: %d succeeded, %d failed (of %d)."),
                S->SuccessCount, S->FailCount, Total);
            if (S->SkippedAnswerCount > 0) {
                Summary += FString::Printf(TEXT(" %d answer node(s) without voice config skipped."), S->SkippedAnswerCount);
            }

            if (S->Notification.IsValid()) {
                S->Notification->SetCompletionState(bAllOk ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
                S->Notification->SetText(FText::FromString(Summary));
                S->Notification->ExpireAndFadeout();
            }
            // Separate persistent notification so the summary stays readable after the throbber fades.
            ShowNotification(Summary, bAllOk);
            UE_LOG(LogTemp, Log, TEXT("Generate All TTS: %s"), *Summary);
            return;
        }

        if (S->Notification.IsValid()) {
            S->Notification->SetText(FText::FromString(
                FString::Printf(TEXT("Generating voices... (%d / %d)"), S->Index + 1, Total)));
        }

        UADSGraphNode* Node = S->Nodes[S->Index].Get();
        if (!Node) {
            // Node went away between collection and processing - count as failure and continue.
            S->FailCount++;
            const FString Msg = FString::Printf(TEXT("Voice batch (%d/%d): node was destroyed before processing — skipped."), S->Index + 1, Total);
            UE_LOG(LogTemp, Warning, TEXT("Generate All TTS: %s"), *Msg);
            ShowNotification(Msg, false);
            S->Index++;
            S->Step();
            return;
        }

        // Answer (button) nodes without a voice config are not an error: they are
        // player choice buttons and simply don't need audio. Skip them silently.
        if (Node->IsA<UADSDialogueResponseNode>()) {
            FADSVoiceSettings AnswerVoiceCfg;
            if (!Node->TryGetParticipantVoiceConfig(AnswerVoiceCfg) || AnswerVoiceCfg.GetSelectedVoiceID().IsEmpty()) {
                UE_LOG(LogTemp, Log, TEXT("Generate All TTS: answer node (%d/%d) has no voice config — skipped (not an error)."), S->Index + 1, Total);
                S->SkippedAnswerCount++;
                S->Index++;
                S->Step();
                return;
            }
        }

        // Identify the node in error messages (participant + text snippet)
        FString NodeLabel = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(Node->GetParticipantTag());
        const FString NodeText = Node->GetDialogueText().ToString().Left(40);
        if (NodeLabel.IsEmpty() || NodeLabel == TEXT("None")) {
            NodeLabel = TEXT("Unknown");
        }

        // Strong capture of S keeps the batch state alive while the request is in flight.
        RequestTTSForNode(Node, [this, S, NodeLabel, NodeText](bool bSuccess, const FString& Message) {
            if (bSuccess) {
                S->SuccessCount++;
            } else {
                S->FailCount++;
                const FString Msg = FString::Printf(TEXT("Voice batch (%d/%d) failed — %s: \"%s...\"\n%s"),
                    S->Index + 1, S->Nodes.Num(), *NodeLabel, *NodeText, *Message);
                UE_LOG(LogTemp, Warning, TEXT("Generate All TTS: %s"), *Msg);
                ShowNotification(Msg, false);
            }
            S->Index++;
            S->Step();
        });
    };

    State->Step();
}

void UADSEditorSubsystem::FetchVoicesAsync() const
{
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString APIKey = Settings->GetTTSVoiceGenAPIKey();

    if (APIKey.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("No TTS Voice API key set - cannot fetch voices"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Starting async voice fetch from TTS Voice API..."));

    // Create HTTP request
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Settings->GetTTSVoiceSelectionAPIEndPoint());
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("xi-api-key"), APIKey);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful) {
        if (bWasSuccessful && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == 200) {
            FString ResponseString = HttpResponse->GetContentAsString();
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

            if (FJsonSerializer::Deserialize(Reader, JsonObject)) {
                // Get access to the global cache (same statics as in GetAvailableVoiceNames)
                const TArray<TSharedPtr<FJsonValue>>* VoicesArray;
                if (JsonObject->TryGetArrayField(TEXT("voices"), VoicesArray)) {

                    UE_LOG(LogTemp, Log, TEXT("Successfully fetched %d voices from TTS Voice API"), VoicesArray->Num());

                    // Note: The cache update needs to happen in GetAvailableVoiceNames function
                    // since that's where the static variables are declared
                    // For now, just log success - user will need to reopen Details Panel to see voices

                } else {
                    UE_LOG(LogTemp, Error, TEXT("No 'voices' array found in API response"));
                }
            } else {
                UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response from TTS Voice API"));
            }
        } else {
            const FString ErrorMsg = HttpResponse.IsValid()
                ? FString::Printf(TEXT("HTTP %d: %s"), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString())
                : TEXT("Request failed (no response from TTS Voice API).");
            UE_LOG(LogTemp, Error, TEXT("Failed to fetch voices from TTS Voice API: %s"), *ErrorMsg);
        }
    });

    // Start the async request
    Request->ProcessRequest();
}

void UADSEditorSubsystem::FetchVoicesFromAPI(TArray<FString>& OutVoiceNames, TMap<FString, FString>& OutVoiceNameToID) const
{
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString APIKey = Settings->GetTTSVoiceGenAPIKey();

    if (APIKey.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("No TTS Voice API key set - cannot fetch voices"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Fetching voices from TTS Voice API..."));

    // Create HTTP request
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();

    Request->SetURL(Settings->GetTTSVoiceSelectionAPIEndPoint());
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("xi-api-key"), APIKey);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    // Keep request alive with shared pointer
    TSharedPtr<IHttpRequest> RequestPtr = Request;

    Request->OnProcessRequestComplete().BindLambda([&OutVoiceNames, &OutVoiceNameToID, RequestPtr](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful) {
        if (bWasSuccessful && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == 200) {
            FString ResponseString = HttpResponse->GetContentAsString();
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

            if (FJsonSerializer::Deserialize(Reader, JsonObject)) {
                const TArray<TSharedPtr<FJsonValue>>* VoicesArray;
                if (JsonObject->TryGetArrayField(TEXT("voices"), VoicesArray)) {
                    for (const auto& VoiceValue : *VoicesArray) {
                        const TSharedPtr<FJsonObject> VoiceObj = VoiceValue->AsObject();
                        if (VoiceObj.IsValid()) {
                            FString VoiceName;
                            FString VoiceId;

                            if (VoiceObj->TryGetStringField(TEXT("name"), VoiceName) && VoiceObj->TryGetStringField(TEXT("voice_id"), VoiceId)) {

                                OutVoiceNames.Add(VoiceName);
                                OutVoiceNameToID.Add(VoiceName, VoiceId);
                            }
                        }
                    }
                    UE_LOG(LogTemp, Log, TEXT("Successfully fetched %d voices from TTS Voice API"), OutVoiceNames.Num());
                } else {
                    UE_LOG(LogTemp, Error, TEXT("No 'voices' array found in API response"));
                }
            } else {
                UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response from TTS Voice API"));
            }
        } else {
            const FString ErrorMsg = HttpResponse.IsValid()
                ? FString::Printf(TEXT("HTTP %d: %s"), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString())
                : TEXT("Request failed (no response from TTS Voice API).");
            UE_LOG(LogTemp, Error, TEXT("Failed to fetch voices from TTS Voice API: %s"), *ErrorMsg);
        }
    });

    // Start the request
    Request->ProcessRequest();

    // For now, provide some fallback voices so the dropdown isn't empty while request processes
    if (OutVoiceNames.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("Using fallback voices while API request processes..."));
        OutVoiceNames.Add(TEXT("Loading voices from TTS..."));
        OutVoiceNameToID.Add(TEXT("Loading voices from TTS..."), TEXT(""));
    }
}

// ============================================================================
// Facial Animation Generation (MetaHuman Animator)
// ============================================================================

#if WITH_ADS_METAHUMAN
bool UADSEditorSubsystem::GenerateFacialAnimationForNode(UADSGraphNode* DialogueNode, FString& OutMessage)
{
    if (!DialogueNode)
    {
        OutMessage = TEXT("No dialogue node selected");
        return false;
    }

    USoundWave* SoundWave = Cast<USoundWave>(DialogueNode->GetSoundToPlay());
    if (!SoundWave)
    {
        OutMessage = TEXT("Node has no SoundWave assigned. Generate TTS audio first.");
        return false;
    }

    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    FString OutputPath = TEXT("/Game/") + Settings->GetFacialAnimationOutputPath();
    if (UADSDialogue* OwningDialogue = ADSEditorAudioHelper::GetOwningDialogue(DialogueNode))
    {
        OutputPath = OwningDialogue->ResolveAnimationOutputPackagePath();
    }

    ADSEditorAudioHelper::EnsureContentDirectoryExists(OutputPath);

    FString ParticipantName = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(DialogueNode->GetParticipantTag());
    if (ParticipantName.IsEmpty() || ParticipantName == TEXT("None"))
    {
        ParticipantName = TEXT("Dialogue");
    }

    const FString TextSnippet = SanitizeAssetName(DialogueNode->GetDialogueText().ToString());
    const FString BaseName = FString::Printf(TEXT("%s_%s"), *ParticipantName, *TextSnippet);

    UMetaHumanPerformance* Performance = Cast<UMetaHumanPerformance>(
        Settings->GetDefaultMetaHumanPerformance().TryLoad());

    const bool bIsReused = (Performance != nullptr);
    if (!Performance)
    {
        Performance = NewObject<UMetaHumanPerformance>(
            GetTransientPackage(), NAME_None, RF_Transient);
    }

    Performance->InputType = EDataInputType::Audio;
    Performance->Audio = SoundWave;
    Performance->bGenerateBlinks = DialogueNode->GetGenerateBlink();

    static const TMap<EADSFacialAnimMood, EAudioDrivenAnimationMood> MoodMapping = {
        { EADSFacialAnimMood::AutoDetect,  EAudioDrivenAnimationMood::AutoDetect },
        { EADSFacialAnimMood::Neutral,     EAudioDrivenAnimationMood::Neutral },
        { EADSFacialAnimMood::Happiness,   EAudioDrivenAnimationMood::Happiness },
        { EADSFacialAnimMood::Sadness,     EAudioDrivenAnimationMood::Sadness },
        { EADSFacialAnimMood::Disgust,     EAudioDrivenAnimationMood::Disgust },
        { EADSFacialAnimMood::Anger,       EAudioDrivenAnimationMood::Anger },
        { EADSFacialAnimMood::Surprise,    EAudioDrivenAnimationMood::Surprise },
        { EADSFacialAnimMood::Fear,        EAudioDrivenAnimationMood::Fear },
        { EADSFacialAnimMood::Confidence,  EAudioDrivenAnimationMood::Confidence },
        { EADSFacialAnimMood::Excitement,  EAudioDrivenAnimationMood::Excitement },
        { EADSFacialAnimMood::Boredom,     EAudioDrivenAnimationMood::Boredom },
        { EADSFacialAnimMood::Playfulness, EAudioDrivenAnimationMood::Playfulness },
        { EADSFacialAnimMood::Confusion,   EAudioDrivenAnimationMood::Confusion },
    };

    FAudioDrivenAnimationSolveOverrides& SolveOverrides = Performance->AudioDrivenAnimationSolveOverrides;
    const EAudioDrivenAnimationMood* FoundMood = MoodMapping.Find(DialogueNode->GetFacialAnimMood());
    SolveOverrides.Mood = FoundMood ? *FoundMood : EAudioDrivenAnimationMood::AutoDetect;
    SolveOverrides.MoodIntensity = DialogueNode->GetFacialAnimMoodIntensity();

    USkeletalMesh* VisMesh = Cast<USkeletalMesh>(Settings->GetFacialAnimVisualizationMesh().TryLoad());
    if (VisMesh)
    {
        Performance->VisualizationObject = VisMesh;
    }

    // Trigger internal frame range recalculation by simulating Audio property change
    {
        FProperty* AudioProp = UMetaHumanPerformance::StaticClass()->FindPropertyByName(
            GET_MEMBER_NAME_CHECKED(UMetaHumanPerformance, Audio));
        FPropertyChangedEvent AudioChangedEvent(AudioProp);
        Performance->PostEditChangeProperty(AudioChangedEvent);
    }

    UE_LOG(LogTemp, Log, TEXT("MetaHuman Animator: Audio duration=%.2fs, FrameRange=[%u, %u], Audio=%s, VisMesh=%s"),
        SoundWave->Duration,
        Performance->StartFrameToProcess,
        Performance->EndFrameToProcess,
        *SoundWave->GetName(),
        VisMesh ? *VisMesh->GetName() : TEXT("null"));

    Performance->SetBlockingProcessing(true);

    if (!Performance->CanProcess())
    {
        const FText Reason = Performance->GetCannotProcessTooltipText();
        OutMessage = FString::Printf(TEXT("MetaHuman Animator cannot process: %s"), *Reason.ToString());
        UE_LOG(LogTemp, Error, TEXT("MetaHuman Animator CanProcess() == false: %s"), *Reason.ToString());
        return false;
    }

    const EStartPipelineErrorType StartError = Performance->StartPipeline(true);

    if (StartError != EStartPipelineErrorType::None)
    {
        OutMessage = TEXT("MetaHuman Animator: Pipeline failed");
        UE_LOG(LogTemp, Error, TEXT("MetaHuman Animator StartPipeline failed with error %d"), static_cast<int32>(StartError));
        return false;
    }

    if (!Performance->ContainsAnimationDataType(EFrameAnimationDataType::Face))
    {
        OutMessage = TEXT("MetaHuman Animator: No animation data generated");
        return false;
    }

    if (bIsReused)
    {
        Performance->MarkPackageDirty();
    }

    UMetaHumanPerformanceExportAnimationSettings* ExportSettings =
        NewObject<UMetaHumanPerformanceExportAnimationSettings>();

    ExportSettings->bShowExportDialog = false;
    ExportSettings->bAutoSaveAnimSequence = true;
    ExportSettings->PackagePath = OutputPath;
    const FString SequenceName = ADSEditorAudioHelper::MakeVersionedAssetName(
        OutputPath, FString::Printf(TEXT("FacialAnim_%s"), *BaseName));
    ExportSettings->AssetName = SequenceName;

    if (VisMesh)
    {
        ExportSettings->TargetSkeletonOrSkeletalMesh = VisMesh;
    }

    UAnimSequence* ExportedSequence = UMetaHumanPerformanceExportUtils::ExportAnimationSequence(Performance, ExportSettings);

    if (ExportedSequence)
    {
        DialogueNode->SetFacialAnimation(nullptr);

        const FString MontageName = SequenceName + TEXT("_Montage");
        UAnimMontage* Montage = CreateMontageWrappingSequence(ExportedSequence, MontageName, OutputPath);

        if (Montage)
        {
            DialogueNode->SetFacialAnimation(Montage);
        }
        // Mark the dialogue (the node's owning package) dirty so "Save All" picks up the change.
        DialogueNode->MarkPackageDirty();

        OutMessage = FString::Printf(TEXT("Facial Animation Generated: %s"), *MontageName);
        UE_LOG(LogTemp, Log, TEXT("MetaHuman Animator: Generated facial animation %s"), *MontageName);

        // Optionally bake an ARKit blendshape-curve animation from the MetaHuman control curves.
        if (Settings->GetExportARKitCurves())
        {
            FString ARKitPath;
            if (UADSDialogue* OwningDialogue = ADSEditorAudioHelper::GetOwningDialogue(DialogueNode))
            {
                ARKitPath = OwningDialogue->ResolveARKitOutputPackagePath();
            }
            ConvertMetaHumanAnimToARKit(ExportedSequence, BaseName, ARKitPath);
        }

        return true;
    }

    OutMessage = TEXT("MetaHuman Animator: Export failed");
    return false;
}

void UADSEditorSubsystem::GenerateFacialAnimation(UADSGraphNode* DialogueNode)
{
    // Show a pending notification, run the (blocking) core, then resolve the notification.
    FNotificationInfo LoadingInfo(FText::FromString(TEXT("MetaHuman Animator: Generating facial animation...")));
    LoadingInfo.bFireAndForget = false;
    LoadingInfo.bUseThrobber = true;
    LoadingInfo.FadeOutDuration = 0.5f;
    LoadingInfo.ExpireDuration = 0.0f;
    TSharedPtr<SNotificationItem> LoadingNotification = FSlateNotificationManager::Get().AddNotification(LoadingInfo);
    if (LoadingNotification.IsValid())
    {
        LoadingNotification->SetCompletionState(SNotificationItem::CS_Pending);
    }

    FString Message;
    const bool bSuccess = GenerateFacialAnimationForNode(DialogueNode, Message);

    if (LoadingNotification.IsValid())
    {
        LoadingNotification->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
        LoadingNotification->SetText(FText::FromString(Message));
        LoadingNotification->ExpireAndFadeout();
    }
}

void UADSEditorSubsystem::GenerateFacialAnimationForDialogues(const TArray<UADSDialogue*>& Dialogues, bool bForceRegenerate)
{
    if (bIsBatchGeneratingFacialAnims)
    {
        ShowNotification(TEXT("Facial animation generation already in progress. Please wait for it to finish."), false);
        return;
    }

    // Collect every node that already has a SoundWave assigned (facial anim is audio-driven).
    // Nodes that already have a facial animation are skipped unless regeneration is forced.
    TArray<TWeakObjectPtr<UADSGraphNode>> NodesToProcess;
    int32 ValidDialogues = 0;
    int32 NodesMissingAudio = 0;
    int32 SkippedAlreadySet = 0;
    {
        TSet<UADSGraphNode*> Seen;
        for (UADSDialogue* Dialogue : Dialogues)
        {
            if (!Dialogue)
            {
                continue;
            }
            ValidDialogues++;
            for (UAGSGraphNode* Node : Dialogue->GetAllNodes())
            {
                UADSGraphNode* DialogueNode = Cast<UADSGraphNode>(Node);
                if (!DialogueNode || Seen.Contains(DialogueNode))
                {
                    continue;
                }
                Seen.Add(DialogueNode);
                if (Cast<USoundWave>(DialogueNode->GetSoundToPlay()))
                {
                    if (!bForceRegenerate && DialogueNode->GetFacialAnimation() != nullptr)
                    {
                        SkippedAlreadySet++;
                        continue;
                    }
                    NodesToProcess.Add(DialogueNode);
                }
                else if (!DialogueNode->GetDialogueText().IsEmpty())
                {
                    NodesMissingAudio++;
                }
            }
        }
    }

    if (ValidDialogues == 0)
    {
        ShowNotification(TEXT("No valid dialogues selected."), false);
        return;
    }

    if (NodesToProcess.Num() == 0)
    {
        const FString Msg = SkippedAlreadySet > 0
            ? FString::Printf(TEXT("All %d eligible node(s) already have a facial animation. Use 'Force' to regenerate."), SkippedAlreadySet)
            : FString(TEXT("No nodes with a SoundWave were found. Generate TTS audio first."));
        ShowNotification(Msg, false);
        UE_LOG(LogTemp, Warning, TEXT("Generate Facial (selection): no eligible nodes (%d missing audio, %d already set)."), NodesMissingAudio, SkippedAlreadySet);
        return;
    }

    FString ConfirmMsg = FString::Printf(
        TEXT("Generate MetaHuman facial animation for %d node(s) across %d selected dialogue(s)%s?\n\n"
             "Each node is processed sequentially (the MetaHuman pipeline is blocking) and the "
             "editor may be unresponsive while it runs.\n\n"),
        NodesToProcess.Num(), ValidDialogues, bForceRegenerate ? TEXT(" (FORCE: regenerating existing animations)") : TEXT(""));
    if (SkippedAlreadySet > 0)
    {
        ConfirmMsg += FString::Printf(TEXT("%d node(s) already animated will be skipped.\n\n"), SkippedAlreadySet);
    }
    if (NodesMissingAudio > 0)
    {
        ConfirmMsg += FString::Printf(TEXT("%d node(s) without audio will be skipped.\n\n"), NodesMissingAudio);
    }
    ConfirmMsg += TEXT("Continue?");

    if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmMsg)) != EAppReturnType::Yes)
    {
        return;
    }

    // Progress notification, advanced one node per editor tick so the UI stays responsive
    // between (blocking) solves.
    FNotificationInfo ProgressInfo(FText::FromString(
        FString::Printf(TEXT("Generating facial animations... (0 / %d)"), NodesToProcess.Num())));
    ProgressInfo.bFireAndForget = false;
    ProgressInfo.bUseThrobber = true;
    ProgressInfo.FadeOutDuration = 0.5f;
    ProgressInfo.ExpireDuration = 0.0f;
    TSharedPtr<SNotificationItem> ProgressNotification = FSlateNotificationManager::Get().AddNotification(ProgressInfo);
    if (ProgressNotification.IsValid())
    {
        ProgressNotification->SetCompletionState(SNotificationItem::CS_Pending);
    }

    bIsBatchGeneratingFacialAnims = true;

    struct FFacialBatchState
    {
        TArray<TWeakObjectPtr<UADSGraphNode>> Nodes;
        int32 Index = 0;
        int32 SuccessCount = 0;
        int32 FailCount = 0;
        TSharedPtr<SNotificationItem> Notification;
        TFunction<void()> Step;
    };

    TSharedRef<FFacialBatchState> State = MakeShared<FFacialBatchState>();
    State->Nodes = MoveTemp(NodesToProcess);
    State->Notification = ProgressNotification;

    TWeakPtr<FFacialBatchState> WeakState = State;

    State->Step = [this, WeakState]()
    {
        TSharedPtr<FFacialBatchState> S = WeakState.Pin();
        if (!S.IsValid())
        {
            return;
        }

        const int32 Total = S->Nodes.Num();

        if (S->Index >= Total)
        {
            bIsBatchGeneratingFacialAnims = false;

            const bool bAllOk = (S->FailCount == 0);
            const FString Summary = FString::Printf(
                TEXT("Facial animation complete: %d succeeded, %d failed (of %d)."),
                S->SuccessCount, S->FailCount, Total);

            if (S->Notification.IsValid())
            {
                S->Notification->SetCompletionState(bAllOk ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
                S->Notification->SetText(FText::FromString(Summary));
                S->Notification->ExpireAndFadeout();
            }
            UE_LOG(LogTemp, Log, TEXT("Generate Facial (selection): %s"), *Summary);
            return;
        }

        if (S->Notification.IsValid())
        {
            S->Notification->SetText(FText::FromString(
                FString::Printf(TEXT("Generating facial animations... (%d / %d)"), S->Index + 1, Total)));
        }

        UADSGraphNode* Node = S->Nodes[S->Index].Get();
        FString Message;
        const bool bSuccess = Node ? GenerateFacialAnimationForNode(Node, Message) : false;
        if (bSuccess)
        {
            S->SuccessCount++;
        }
        else
        {
            S->FailCount++;
            UE_LOG(LogTemp, Warning, TEXT("Generate Facial (selection): node %d failed: %s"), S->Index, *Message);
        }
        S->Index++;

        // Defer the next (blocking) node to the next tick so the progress notification repaints.
        // The timer lambda captures the shared state strongly so it stays alive across ticks
        // (the kick-off TSharedRef in GenerateFacialAnimationForDialogues is gone by now). Once
        // the batch finishes it stops rescheduling, so the last lambda releases the final ref.
        if (GEditor)
        {
            GEditor->GetTimerManager()->SetTimerForNextTick([S]()
            {
                S->Step();
            });
        }
        else
        {
            S->Step();
        }
    };

    State->Step();
}
#else
void UADSEditorSubsystem::GenerateFacialAnimation(UADSGraphNode* DialogueNode)
{
    ShowNotification(TEXT("Facial animation generation requires the MetaHuman plugin, which is only available on Windows/Linux."), false);
}

void UADSEditorSubsystem::GenerateFacialAnimationForDialogues(const TArray<UADSDialogue*>& Dialogues, bool bForceRegenerate)
{
    ShowNotification(TEXT("Facial animation generation requires the MetaHuman plugin, which is only available on Windows/Linux."), false);
}
#endif // WITH_ADS_METAHUMAN

// ============================================================================
// ARKit Blendshape Curve Generation
// ============================================================================

namespace ADSARKitHelper
{
    /**
     * Builds the built-in default MetaHuman -> ARKit mapping.
     * Source control names are best-effort MetaHuman face-board raw controls; they are prefixed with
     * Prefix (default "CTRL_expressions_"). Names that do not exist in the source animation are simply
     * skipped during the bake, and the whole mapping can be overridden via a DataTable in project settings.
     */
    static void BuildDefaultARKitMapping(const FString& Prefix, TArray<FADSARKitCurveMapping>& Out)
    {
        auto Make = [&Prefix](const TCHAR* ARKitName, std::initializer_list<TPair<const TCHAR*, float>> Contributions)
        {
            FADSARKitCurveMapping Entry;
            Entry.ARKitCurveName = FName(ARKitName);
            for (const TPair<const TCHAR*, float>& C : Contributions)
            {
                FADSARKitControlContribution Contribution;
                Contribution.SourceControlCurve = FName(*(Prefix + C.Key));
                Contribution.Weight = C.Value;
                Entry.Sources.Add(Contribution);
            }
            return Entry;
        };

        // Brows
        Out.Add(Make(TEXT("browDownLeft"), { { TEXT("browDownL"), 1.f } }));
        Out.Add(Make(TEXT("browDownRight"), { { TEXT("browDownR"), 1.f } }));
        Out.Add(Make(TEXT("browInnerUp"), { { TEXT("browRaiseInL"), 0.5f }, { TEXT("browRaiseInR"), 0.5f } }));
        Out.Add(Make(TEXT("browOuterUpLeft"), { { TEXT("browRaiseOuterL"), 1.f } }));
        Out.Add(Make(TEXT("browOuterUpRight"), { { TEXT("browRaiseOuterR"), 1.f } }));

        // Cheeks
        Out.Add(Make(TEXT("cheekPuff"), { { TEXT("mouthCheekBlowL"), 0.5f }, { TEXT("mouthCheekBlowR"), 0.5f } }));
        Out.Add(Make(TEXT("cheekSquintLeft"), { { TEXT("eyeCheekRaiseL"), 1.f } }));
        Out.Add(Make(TEXT("cheekSquintRight"), { { TEXT("eyeCheekRaiseR"), 1.f } }));

        // Eyes
        Out.Add(Make(TEXT("eyeBlinkLeft"), { { TEXT("eyeBlinkL"), 1.f } }));
        Out.Add(Make(TEXT("eyeBlinkRight"), { { TEXT("eyeBlinkR"), 1.f } }));
        Out.Add(Make(TEXT("eyeLookDownLeft"), { { TEXT("eyeLookDownL"), 1.f } }));
        Out.Add(Make(TEXT("eyeLookDownRight"), { { TEXT("eyeLookDownR"), 1.f } }));
        Out.Add(Make(TEXT("eyeLookInLeft"), { { TEXT("eyeLookRightL"), 1.f } }));
        Out.Add(Make(TEXT("eyeLookInRight"), { { TEXT("eyeLookLeftR"), 1.f } }));
        Out.Add(Make(TEXT("eyeLookOutLeft"), { { TEXT("eyeLookLeftL"), 1.f } }));
        Out.Add(Make(TEXT("eyeLookOutRight"), { { TEXT("eyeLookRightR"), 1.f } }));
        Out.Add(Make(TEXT("eyeLookUpLeft"), { { TEXT("eyeLookUpL"), 1.f } }));
        Out.Add(Make(TEXT("eyeLookUpRight"), { { TEXT("eyeLookUpR"), 1.f } }));
        Out.Add(Make(TEXT("eyeSquintLeft"), { { TEXT("eyeSquintInnerL"), 1.f } }));
        Out.Add(Make(TEXT("eyeSquintRight"), { { TEXT("eyeSquintInnerR"), 1.f } }));
        Out.Add(Make(TEXT("eyeWideLeft"), { { TEXT("eyeWidenL"), 1.f } }));
        Out.Add(Make(TEXT("eyeWideRight"), { { TEXT("eyeWidenR"), 1.f } }));

        // Jaw
        Out.Add(Make(TEXT("jawForward"), { { TEXT("jawFwd"), 1.f } }));
        Out.Add(Make(TEXT("jawLeft"), { { TEXT("jawLeft"), 1.f } }));
        Out.Add(Make(TEXT("jawRight"), { { TEXT("jawRight"), 1.f } }));
        Out.Add(Make(TEXT("jawOpen"), { { TEXT("jawOpen"), 1.f } }));

        // Mouth
        Out.Add(Make(TEXT("mouthClose"), { { TEXT("mouthClose"), 1.f } }));
        Out.Add(Make(TEXT("mouthDimpleLeft"), { { TEXT("mouthDimpleL"), 1.f } }));
        Out.Add(Make(TEXT("mouthDimpleRight"), { { TEXT("mouthDimpleR"), 1.f } }));
        Out.Add(Make(TEXT("mouthFrownLeft"), { { TEXT("mouthCornerDepressL"), 1.f } }));
        Out.Add(Make(TEXT("mouthFrownRight"), { { TEXT("mouthCornerDepressR"), 1.f } }));
        Out.Add(Make(TEXT("mouthFunnel"), { { TEXT("mouthFunnelUL"), 0.25f }, { TEXT("mouthFunnelUR"), 0.25f }, { TEXT("mouthFunnelDL"), 0.25f }, { TEXT("mouthFunnelDR"), 0.25f } }));
        Out.Add(Make(TEXT("mouthLeft"), { { TEXT("mouthLeft"), 1.f } }));
        Out.Add(Make(TEXT("mouthRight"), { { TEXT("mouthRight"), 1.f } }));
        Out.Add(Make(TEXT("mouthLowerDownLeft"), { { TEXT("mouthLowerLipDepressL"), 1.f } }));
        Out.Add(Make(TEXT("mouthLowerDownRight"), { { TEXT("mouthLowerLipDepressR"), 1.f } }));
        Out.Add(Make(TEXT("mouthPressLeft"), { { TEXT("mouthPressUL"), 0.5f }, { TEXT("mouthPressDL"), 0.5f } }));
        Out.Add(Make(TEXT("mouthPressRight"), { { TEXT("mouthPressUR"), 0.5f }, { TEXT("mouthPressDR"), 0.5f } }));
        Out.Add(Make(TEXT("mouthPucker"), { { TEXT("mouthLipsPurseUL"), 0.25f }, { TEXT("mouthLipsPurseUR"), 0.25f }, { TEXT("mouthLipsPurseDL"), 0.25f }, { TEXT("mouthLipsPurseDR"), 0.25f } }));
        Out.Add(Make(TEXT("mouthRollLower"), { { TEXT("mouthLipsTowardsDL"), 0.5f }, { TEXT("mouthLipsTowardsDR"), 0.5f } }));
        Out.Add(Make(TEXT("mouthRollUpper"), { { TEXT("mouthLipsTowardsUL"), 0.5f }, { TEXT("mouthLipsTowardsUR"), 0.5f } }));
        Out.Add(Make(TEXT("mouthShrugLower"), { { TEXT("mouthShrugLower"), 1.f } }));
        Out.Add(Make(TEXT("mouthShrugUpper"), { { TEXT("mouthShrugUpper"), 1.f } }));
        Out.Add(Make(TEXT("mouthSmileLeft"), { { TEXT("mouthCornerPullL"), 1.f } }));
        Out.Add(Make(TEXT("mouthSmileRight"), { { TEXT("mouthCornerPullR"), 1.f } }));
        Out.Add(Make(TEXT("mouthStretchLeft"), { { TEXT("mouthStretchL"), 1.f } }));
        Out.Add(Make(TEXT("mouthStretchRight"), { { TEXT("mouthStretchR"), 1.f } }));
        Out.Add(Make(TEXT("mouthUpperUpLeft"), { { TEXT("mouthUpperLipRaiseL"), 1.f } }));
        Out.Add(Make(TEXT("mouthUpperUpRight"), { { TEXT("mouthUpperLipRaiseR"), 1.f } }));

        // Nose
        Out.Add(Make(TEXT("noseSneerLeft"), { { TEXT("noseWrinkleL"), 1.f } }));
        Out.Add(Make(TEXT("noseSneerRight"), { { TEXT("noseWrinkleR"), 1.f } }));

        // Tongue
        Out.Add(Make(TEXT("tongueOut"), { { TEXT("tongueOut"), 1.f } }));
    }
}

void UADSEditorSubsystem::GetARKitCurveMappings(TArray<FADSARKitCurveMapping>& OutMappings) const
{
    OutMappings.Reset();

    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();

    if (UDataTable* Table = Cast<UDataTable>(Settings->GetARKitMappingTable().TryLoad()))
    {
        TArray<FADSARKitCurveMapping*> Rows;
        Table->GetAllRows<FADSARKitCurveMapping>(TEXT("ADSARKitMapping"), Rows);
        for (const FADSARKitCurveMapping* Row : Rows)
        {
            if (Row && !Row->ARKitCurveName.IsNone())
            {
                OutMappings.Add(*Row);
            }
        }
    }

    if (OutMappings.Num() == 0)
    {
        ADSARKitHelper::BuildDefaultARKitMapping(Settings->GetARKitSourceControlPrefix(), OutMappings);
    }
}

UAnimMontage* UADSEditorSubsystem::CreateMontageWrappingSequence(UAnimSequence* Sequence, const FString& MontageName, const FString& PackagePath)
{
    if (!Sequence)
    {
        return nullptr;
    }

    FString NormalizedPackagePath = PackagePath;
    NormalizedPackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!NormalizedPackagePath.EndsWith(TEXT("/")))
    {
        NormalizedPackagePath += TEXT("/");
    }

    const FString UniqueMontageName = ADSEditorAudioHelper::MakeVersionedAssetName(NormalizedPackagePath, MontageName);
    const FString MontagePackagePath = NormalizedPackagePath + UniqueMontageName;
    UPackage* MontagePackage = CreatePackage(*MontagePackagePath);
    MontagePackage->FullyLoad();

    UAnimMontage* Montage = NewObject<UAnimMontage>(
        MontagePackage, FName(*UniqueMontageName), RF_Public | RF_Standalone);
    Montage->SetSkeleton(Sequence->GetSkeleton());

    Montage->SlotAnimTracks.Empty();
    FSlotAnimationTrack& SlotTrack = Montage->SlotAnimTracks.AddDefaulted_GetRef();
    SlotTrack.SlotName = FName("DefaultSlot");

    FAnimSegment& Segment = SlotTrack.AnimTrack.AnimSegments.AddDefaulted_GetRef();
    Segment.SetAnimReference(Sequence, true);
    Segment.AnimStartTime = 0.0f;
    Segment.AnimEndTime = Sequence->GetPlayLength();
    Segment.AnimPlayRate = 1.0f;
    Segment.StartPos = 0.0f;
    Segment.LoopingCount = 1;

    Montage->CompositeSections.Empty();
    FCompositeSection& Section = Montage->CompositeSections.AddDefaulted_GetRef();
    Section.SectionName = FName("Default");

    Montage->InvalidateRecursiveAsset();
    Montage->PostEditChange();
    Montage->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Montage);

    return Montage;
}

UAnimSequence* UADSEditorSubsystem::ConvertMetaHumanAnimToARKit(UAnimSequence* SourceSequence, const FString& BaseName, const FString& PackagePathOverride)
{
    if (!SourceSequence)
    {
        ShowNotification(TEXT("ARKit export: source animation is null"), false);
        return nullptr;
    }

    const IAnimationDataModel* SrcModel = SourceSequence->GetDataModel();
    if (!SrcModel)
    {
        ShowNotification(TEXT("ARKit export: source animation has no data model"), false);
        return nullptr;
    }

    // Index the source control curves by name for fast lookup.
    TMap<FName, const FRichCurve*> SourceCurves;
    for (const FFloatCurve& FloatCurve : SrcModel->GetCurveData().FloatCurves)
    {
        SourceCurves.Add(FloatCurve.GetName(), &FloatCurve.FloatCurve);
    }

    if (SourceCurves.Num() == 0)
    {
        ShowNotification(TEXT("ARKit export: source animation contains no control curves to remap."), false);
        return nullptr;
    }

    TArray<FADSARKitCurveMapping> Mappings;
    GetARKitCurveMappings(Mappings);
    if (Mappings.Num() == 0)
    {
        ShowNotification(TEXT("ARKit export: no curve mapping available."), false);
        return nullptr;
    }

    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();

    USkeleton* TargetSkeleton = Cast<USkeleton>(Settings->GetARKitTargetSkeleton().TryLoad());
    if (!TargetSkeleton)
    {
        TargetSkeleton = SourceSequence->GetSkeleton();
    }
    if (!TargetSkeleton)
    {
        ShowNotification(TEXT("ARKit export: no target skeleton available."), false);
        return nullptr;
    }

    // Prepare the output package/asset.
    FString OutputPath = PackagePathOverride;
    if (OutputPath.IsEmpty())
    {
        OutputPath = TEXT("/Game/") + Settings->GetARKitAnimationOutputPath();
    }
    if (!OutputPath.EndsWith(TEXT("/")))
    {
        OutputPath += TEXT("/");
    }

    ADSEditorAudioHelper::EnsureContentDirectoryExists(OutputPath);

    const FString AssetName = ADSEditorAudioHelper::MakeVersionedAssetName(
        OutputPath, FString::Printf(TEXT("ARKitAnim_%s"), *BaseName));
    const FString FullPackagePath = OutputPath + AssetName;

    UPackage* Package = CreatePackage(*FullPackagePath);
    Package->FullyLoad();

    UAnimSequence* NewSeq = NewObject<UAnimSequence>(Package, FName(*AssetName), RF_Public | RF_Standalone);
    NewSeq->SetSkeleton(TargetSkeleton);

    const FFrameRate FrameRate = SrcModel->GetFrameRate();
    const int32 NumberOfFrames = SrcModel->GetNumberOfFrames();
    const int32 NumberOfKeys = SrcModel->GetNumberOfKeys();

    int32 MatchedContributions = 0;

    IAnimationDataController& Controller = NewSeq->GetController();
    Controller.OpenBracket(FText::FromString(TEXT("Generate ARKit Curves")));
    Controller.InitializeModel();
    Controller.SetFrameRate(FrameRate);
    Controller.SetNumberOfFrames(FFrameNumber(NumberOfFrames));

    for (const FADSARKitCurveMapping& Mapping : Mappings)
    {
        if (Mapping.ARKitCurveName.IsNone())
        {
            continue;
        }

        const FAnimationCurveIdentifier CurveId(Mapping.ARKitCurveName, ERawCurveTrackTypes::RCT_Float);
        Controller.AddCurve(CurveId, AACF_Editable);

        TArray<FRichCurveKey> Keys;
        Keys.Reserve(FMath::Max(NumberOfKeys, 1));

        for (int32 KeyIndex = 0; KeyIndex < NumberOfKeys; ++KeyIndex)
        {
            const float Time = (float)FrameRate.AsSeconds(FFrameNumber(KeyIndex));
            float Value = 0.0f;
            for (const FADSARKitControlContribution& Source : Mapping.Sources)
            {
                if (const FRichCurve* const* Found = SourceCurves.Find(Source.SourceControlCurve))
                {
                    Value += (*Found)->Eval(Time) * Source.Weight;
                    ++MatchedContributions;
                }
            }
            Value = FMath::Clamp(Value, 0.0f, 1.0f);
            Keys.Add(FRichCurveKey(Time, Value));
        }

        Controller.SetCurveKeys(CurveId, Keys);
    }

    Controller.NotifyPopulated();
    Controller.CloseBracket();

    NewSeq->PostEditChange();
    NewSeq->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewSeq);

    if (MatchedContributions == 0)
    {
        ShowNotification(
            TEXT("ARKit export: no source control curves matched the mapping. "
                 "Check the 'ARKit Source Control Prefix' / mapping table in Ascent Dialogue Settings."),
            false);
        UE_LOG(LogTemp, Warning, TEXT("ARKit export: 0 source control curves matched. Generated empty ARKit animation %s"), *AssetName);
    }
    else
    {
        ShowNotification(FString::Printf(TEXT("ARKit Animation Generated: %s"), *AssetName), true);
        UE_LOG(LogTemp, Log, TEXT("ARKit export: generated %s (%d ARKit curves, %d matched contributions)"),
            *AssetName, Mappings.Num(), MatchedContributions);
    }

    if (Settings->GetCreateARKitMontage())
    {
        const FString MontageName = AssetName + TEXT("_Montage");
        CreateMontageWrappingSequence(NewSeq, MontageName, OutputPath);
    }

    return NewSeq;
}

void UADSEditorSubsystem::GenerateARKitAnimationForNode(UADSGraphNode* DialogueNode)
{
    if (!DialogueNode)
    {
        ShowNotification(TEXT("No dialogue node selected"), false);
        return;
    }

    UAnimMontage* FacialMontage = DialogueNode->GetFacialAnimation();
    if (!FacialMontage)
    {
        ShowNotification(TEXT("This node has no facial animation yet. Click 'Generate Facial Animation' first."), false);
        return;
    }

    // Pull the underlying AnimSequence (MetaHuman control curves) out of the facial montage.
    UAnimSequence* SourceSequence = nullptr;
    for (const FSlotAnimationTrack& SlotTrack : FacialMontage->SlotAnimTracks)
    {
        for (const FAnimSegment& Segment : SlotTrack.AnimTrack.AnimSegments)
        {
            if (UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference()))
            {
                SourceSequence = Sequence;
                break;
            }
        }
        if (SourceSequence)
        {
            break;
        }
    }

    if (!SourceSequence)
    {
        ShowNotification(TEXT("The facial animation montage contains no AnimSequence to convert."), false);
        return;
    }

    FString ParticipantName = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(DialogueNode->GetParticipantTag());
    if (ParticipantName.IsEmpty() || ParticipantName == TEXT("None"))
    {
        ParticipantName = TEXT("Dialogue");
    }

    const FString TextSnippet = SanitizeAssetName(DialogueNode->GetDialogueText().ToString());
    const FString BaseName = FString::Printf(TEXT("%s_%s"), *ParticipantName, *TextSnippet);

    FString ARKitPath;
    if (UADSDialogue* OwningDialogue = ADSEditorAudioHelper::GetOwningDialogue(DialogueNode))
    {
        ARKitPath = OwningDialogue->ResolveARKitOutputPackagePath();
    }
    ConvertMetaHumanAnimToARKit(SourceSequence, BaseName, ARKitPath);
}

// ============================================================================
// Dialogue Export/Import
// ============================================================================

namespace ADSExportHelper
{
    /**
     * Recursively traverses dialogue nodes in DFS order and appends screenplay-formatted text.
     * Response nodes (player choices) are output as "(Option N)" variants.
     */
    static void ExportNodeRecursive(UADSGraphNode* Node, TSet<UAGSGraphNode*>& Visited, FString& Output)
    {
        if (!Node || Visited.Contains(Node))
        {
            return;
        }
        Visited.Add(Node);

        // Get participant name from the tag
        FString ParticipantName = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(Node->GetParticipantTag());
        if (ParticipantName.IsEmpty() || ParticipantName == TEXT("None"))
        {
            ParticipantName = TEXT("NARRATOR");
        }

        const FString DialogueText = Node->GetDialogueText().ToString();
        const FGuid NodeId = Node->GetNodeId();

        // Gather response children vs regular dialogue children
        TArray<UADSDialogueResponseNode*> ResponseChildren;
        TArray<UADSGraphNode*> RegularChildren;

        for (UAGSGraphNode* Child : Node->ChildrenNodes)
        {
            if (UADSDialogueResponseNode* Response = Cast<UADSDialogueResponseNode>(Child))
            {
                ResponseChildren.Add(Response);
            }
            else if (UADSGraphNode* RegularChild = Cast<UADSGraphNode>(Child))
            {
                RegularChildren.Add(RegularChild);
            }
        }

        // Option numbering must follow left-to-right graph layout
        ResponseChildren.Sort([](const UADSDialogueResponseNode& A, const UADSDialogueResponseNode& B)
        {
            const FVector2D PosA = A.GetNodePosition();
            const FVector2D PosB = B.GetNodePosition();
            if (!FMath::IsNearlyEqual(PosA.X, PosB.X))
            {
                return PosA.X < PosB.X;
            }
            if (!FMath::IsNearlyEqual(PosA.Y, PosB.Y))
            {
                return PosA.Y < PosB.Y;
            }
            return A.GetNodeId() < B.GetNodeId();
        });

        // Output current node: [Node:GUID] \n ParticipantName: \n Text \n\n
        Output += FString::Printf(TEXT("[Node:%s]\n"), *NodeId.ToString());
        Output += FString::Printf(TEXT("%s:\n"), *ParticipantName);
        Output += FString::Printf(TEXT("%s\n\n"), *DialogueText);

        // If there are response (option) children, output each one
        if (ResponseChildren.Num() > 0)
        {
            for (int32 i = 0; i < ResponseChildren.Num(); i++)
            {
                UADSDialogueResponseNode* Response = ResponseChildren[i];
                if (Visited.Contains(Response))
                {
                    continue;
                }
                Visited.Add(Response);

                FString ResponseParticipant = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(Response->GetParticipantTag());
                if (ResponseParticipant.IsEmpty() || ResponseParticipant == TEXT("None"))
                {
                    ResponseParticipant = TEXT("PLAYER");
                }

                const FString ResponseText = Response->GetDialogueText().ToString();

                Output += FString::Printf(TEXT("[Node:%s]\n"), *Response->GetNodeId().ToString());
                Output += FString::Printf(TEXT("%s (Option %d):\n"), *ResponseParticipant, i + 1);
                Output += FString::Printf(TEXT("%s\n\n"), *ResponseText);

                // Follow each response's children
                for (UAGSGraphNode* ResponseChild : Response->ChildrenNodes)
                {
                    UADSGraphNode* NextNode = Cast<UADSGraphNode>(ResponseChild);
                    if (NextNode)
                    {
                        ExportNodeRecursive(NextNode, Visited, Output);
                    }
                }
            }
        }
        else
        {
            // Follow regular children
            for (UADSGraphNode* Child : RegularChildren)
            {
                ExportNodeRecursive(Child, Visited, Output);
            }
        }
    }
}

void UADSEditorSubsystem::ExportDialogueToFile(UADSDialogue* Dialogue)
{
    if (!Dialogue)
    {
        ShowNotification(TEXT("No dialogue selected"), false);
        return;
    }

#if WITH_EDITORONLY_DATA
    FString Directory = Dialogue->ExportImportDirectory.Path;
    if (Directory.IsEmpty())
    {
        ShowNotification(TEXT("Please set the Export/Import Directory on the Dialogue asset first."), false);
        return;
    }

    // Resolve relative paths
    if (FPaths::IsRelative(Directory))
    {
        Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Directory);
    }

    // Ensure directory exists
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*Directory))
    {
        PlatformFile.CreateDirectoryTree(*Directory);
    }

    // Generate file path
    const FString DialogueName = Dialogue->GetName();
    const FString FilePath = FPaths::Combine(Directory, DialogueName + TEXT(".txt"));

    // Confirmation dialog
    const FString ExportConfirmMsg = FString::Printf(
        TEXT("Export dialogue '%s' to:\n%s\n\nAny existing file will be overwritten."),
        *DialogueName, *FilePath);

    if (FMessageDialog::Open(EAppMsgType::OkCancel, FText::FromString(ExportConfirmMsg)) != EAppReturnType::Ok)
    {
        return;
    }

    const FString Output = ExportDialogueToText(Dialogue);

    // Save to file (UTF-8 without BOM for maximum compatibility)
    if (FFileHelper::SaveStringToFile(Output, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        const FString SuccessMsg = FString::Printf(TEXT("Dialogue exported to: %s"), *FilePath);
        ShowNotification(SuccessMsg, true);
        UE_LOG(LogTemp, Log, TEXT("%s"), *SuccessMsg);
    }
    else
    {
        ShowNotification(FString::Printf(TEXT("Failed to save file: %s"), *FilePath), false);
    }
#endif
}

FString UADSEditorSubsystem::ExportDialogueToText(UADSDialogue* Dialogue) const
{
    if (!Dialogue)
    {
        return FString();
    }

    const FString DialogueName = Dialogue->GetName();

    // Build screenplay header
    FString Output;
    Output += TEXT("# ============================================\n");
    Output += TEXT("# Ascent Dialogue System - Screenplay Export\n");
    Output += FString::Printf(TEXT("# Dialogue: %s\n"), *DialogueName);
    Output += FString::Printf(TEXT("# Exported: %s\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")));
    Output += TEXT("# ============================================\n");
    Output += TEXT("#\n");
    Output += TEXT("# FORMAT:\n");
    Output += TEXT("#   [Node:GUID]          <- Do NOT modify this line\n");
    Output += TEXT("#   PARTICIPANT NAME:     <- Speaker name\n");
    Output += TEXT("#   Dialogue text here    <- Editable text\n");
    Output += TEXT("#\n");
    Output += TEXT("# For player choices:\n");
    Output += TEXT("#   PARTICIPANT (Option N):\n");
    Output += TEXT("#   Choice text here\n");
    Output += TEXT("#\n");
    Output += TEXT("# Lines starting with # are comments (ignored during import)\n");
    Output += TEXT("# You may freely edit dialogue text. Do NOT change [Node:...] lines.\n");
    Output += TEXT("# ============================================\n\n");

    // DFS traversal from root nodes
    TSet<UAGSGraphNode*> Visited;
    for (UAGSGraphNode* Root : Dialogue->RootNodes)
    {
        UADSGraphNode* DialogueRoot = Cast<UADSGraphNode>(Root);
        if (DialogueRoot)
        {
            ADSExportHelper::ExportNodeRecursive(DialogueRoot, Visited, Output);
        }
    }

    return Output;
}

// ============================================================================
// Import Helper Functions
// ============================================================================

namespace ADSImportHelper
{
    /** Parsed entry from a screenplay file */
    struct FParsedDialogueEntry
    {
        FString ParticipantName;
        FString Text;
        int32 OptionNumber = 0; // 0 = regular dialogue, >0 = player choice
        FGuid NodeId;           // Optional - from [Node:GUID] tags in exported files
        bool bHasNodeId = false;
    };

    /** Parse a screenplay-formatted text file into dialogue entries */
    static TArray<FParsedDialogueEntry> ParseScreenplay(const FString& FileContent)
    {
        TArray<FParsedDialogueEntry> Entries;
        TArray<FString> Lines;
        FileContent.ParseIntoArrayLines(Lines);

        int32 CurrentEntry = -1;
        bool bReadingText = false;

        // [Node:GUID] lines appear BEFORE the participant header,
        // so we store them as "pending" and assign to the next entry
        FGuid PendingGuid;
        bool bHasPendingGuid = false;

        for (const FString& Line : Lines)
        {
            FString TrimmedStart = Line.TrimStart();

            // Skip comment lines
            if (TrimmedStart.StartsWith(TEXT("#")))
            {
                continue;
            }

            // Capture [Node:GUID] lines (store as pending for the next entry)
            if (TrimmedStart.StartsWith(TEXT("[Node:")) && TrimmedStart.TrimEnd().EndsWith(TEXT("]")))
            {
                FString GuidStr = TrimmedStart.TrimEnd();
                GuidStr = GuidStr.Mid(6, GuidStr.Len() - 7);
                bHasPendingGuid = FGuid::Parse(GuidStr, PendingGuid);
                continue;
            }

            // Check for header line: "Name:" or "Name (Option N):"
            bool bIsHeader = false;
            FString Trimmed = Line.TrimEnd();
            if (Trimmed.EndsWith(TEXT(":")) && Trimmed.Len() > 1)
            {
                FString Content = Trimmed.LeftChop(1).TrimEnd();

                if (!Content.IsEmpty())
                {
                    // Check for "(Option N)" pattern
                    int32 OptionNum = 0;
                    int32 OpenParen = Content.Find(TEXT("(Option "), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
                    if (OpenParen != INDEX_NONE)
                    {
                        int32 CloseParen = Content.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, OpenParen);
                        if (CloseParen != INDEX_NONE && CloseParen == Content.Len() - 1)
                        {
                            FString OptionStr = Content.Mid(OpenParen + 8, CloseParen - OpenParen - 8);
                            OptionNum = FCString::Atoi(*OptionStr);
                            Content = Content.Left(OpenParen).TrimEnd();
                        }
                    }

                    // Trim previous entry's text
                    if (CurrentEntry >= 0)
                    {
                        Entries[CurrentEntry].Text = Entries[CurrentEntry].Text.TrimStartAndEnd();
                    }

                    // Create new entry
                    FParsedDialogueEntry NewEntry;
                    NewEntry.ParticipantName = Content;
                    NewEntry.OptionNumber = OptionNum;

                    // Assign pending GUID if available
                    if (bHasPendingGuid)
                    {
                        NewEntry.NodeId = PendingGuid;
                        NewEntry.bHasNodeId = true;
                        bHasPendingGuid = false;
                    }

                    Entries.Add(NewEntry);
                    CurrentEntry = Entries.Num() - 1;
                    bReadingText = true;
                    bIsHeader = true;
                }
            }

            // Accumulate text lines (if not a header)
            if (!bIsHeader && bReadingText && CurrentEntry >= 0)
            {
                FParsedDialogueEntry& Entry = Entries[CurrentEntry];
                if (!Entry.Text.IsEmpty())
                {
                    Entry.Text += TEXT("\n");
                }
                Entry.Text += Line;
            }
        }

        // Trim last entry
        if (CurrentEntry >= 0)
        {
            Entries[CurrentEntry].Text = Entries[CurrentEntry].Text.TrimStartAndEnd();
        }

        return Entries;
    }

    /** Try to find a GameplayTag matching a participant name */
    static FGameplayTag FindTagForParticipant(const FString& ParticipantName, const FGameplayTagContainer& AllTags)
    {
        if (ParticipantName.IsEmpty())
        {
            return FGameplayTag();
        }

        // Try direct match: Character.Name
        FGameplayTag DirectTag = FGameplayTag::RequestGameplayTag(
            FName(*(TEXT("Character.") + ParticipantName)), false);
        if (DirectTag.IsValid())
        {
            return DirectTag;
        }

        // Search all tags for one ending with .ParticipantName (case-insensitive)
        for (const FGameplayTag& Tag : AllTags)
        {
            FString TagString = Tag.ToString();
            if (TagString.EndsWith(TEXT(".") + ParticipantName, ESearchCase::IgnoreCase))
            {
                return Tag;
            }
        }

        // Try matching just the last segment of any tag
        for (const FGameplayTag& Tag : AllTags)
        {
            FString TagString = Tag.ToString();
            FString LastSegment;
            TagString.Split(TEXT("."), nullptr, &LastSegment, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
            if (LastSegment.Equals(ParticipantName, ESearchCase::IgnoreCase))
            {
                return Tag;
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("Import: No GameplayTag found for participant '%s'. Expected tag like 'Character.%s'."),
            *ParticipantName, *ParticipantName);

        return FGameplayTag();
    }

    /** Create an editor graph node wrapping a runtime node */
    static UEdNode_AGSGraphNode* CreateEditorNode(UEdGraph_AGSGraph* EdGraph, UAGSGraphNode* RuntimeNode, float PosX, float PosY)
    {
        UEdNode_AGSGraphNode* EdNode = NewObject<UEdNode_AGSGraphNode>(EdGraph);
        EdNode->AGSGraphNode = RuntimeNode;

        EdNode->Rename(nullptr, EdGraph);
        EdGraph->AddNode(EdNode, true, false);

        EdNode->CreateNewGuid();
        EdNode->PostPlacedNewNode();
        EdNode->AllocateDefaultPins();

        EdNode->NodePosX = PosX;
        EdNode->NodePosY = PosY;

        RuntimeNode->SetFlags(RF_Transactional);
        EdNode->SetFlags(RF_Transactional);
        RuntimeNode->SetNodePosition(FVector2D(PosX, PosY));

        return EdNode;
    }

    /** Connect nodes through the graph schema so a UADSGraphEdge transition is created. */
    static bool ConnectEditorNodes(UEdNode_AGSGraphNode* FromNode, UEdNode_AGSGraphNode* ToNode)
    {
        if (!FromNode || !ToNode || FromNode->Pins.Num() < 2 || ToNode->Pins.Num() < 2)
        {
            return false;
        }

        const UEdGraphSchema* Schema = FromNode->GetSchema();
        if (!Schema || !Schema->TryCreateConnection(FromNode->Pins[1], ToNode->Pins[0]))
        {
            UE_LOG(LogTemp, Warning, TEXT("Import: Failed to create transition from '%s' to '%s'."),
                *FromNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
                *ToNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
            return false;
        }

        FromNode->NodeConnectionListChanged();
        ToNode->NodeConnectionListChanged();
        return true;
    }
}

FADSDialogueTextValidation UADSEditorSubsystem::ValidateDialogueText(UADSDialogue* Dialogue, const FString& ScreenplayText) const
{
    FADSDialogueTextValidation Result;

    const TArray<ADSImportHelper::FParsedDialogueEntry> Entries = ADSImportHelper::ParseScreenplay(ScreenplayText);
    Result.EntryCount = Entries.Num();
    if (Entries.Num() == 0)
    {
        Result.Errors.Add(TEXT("No dialogue entries found. Each entry is a 'NAME:' line followed by its text, separated by a blank line."));
        return Result;
    }

    TArray<FString> AllowedNames;
    if (Dialogue)
    {
        for (const FGameplayTag& Tag : Dialogue->AIParticipantTags)
        {
            const FString Name = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(Tag);
            if (!Name.IsEmpty() && Name != TEXT("None"))
            {
                AllowedNames.Add(Name);
            }
        }
    }

    FGameplayTagContainer AllTags;
    UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, true);

    TSet<FString> ReportedNames;
    for (int32 Index = 0; Index < Entries.Num(); ++Index)
    {
        const ADSImportHelper::FParsedDialogueEntry& Entry = Entries[Index];
        const FString Where = FString::Printf(TEXT("Entry %d (%s)"), Index + 1, *Entry.ParticipantName);

        if (Entry.Text.IsEmpty())
        {
            Result.Errors.Add(Where + TEXT(": has no dialogue text under the speaker line."));
        }

        if (Entry.bHasNodeId && Dialogue && Dialogue->GetNodeById(Entry.NodeId))
        {
            Result.bUpdatesExistingNodes = true;
        }

        if (!ReportedNames.Contains(Entry.ParticipantName))
        {
            ReportedNames.Add(Entry.ParticipantName);
            if (AllowedNames.Num() > 0
                && !Entry.ParticipantName.Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase)
                && !Entry.ParticipantName.Equals(TEXT("NARRATOR"), ESearchCase::IgnoreCase)
                && !AllowedNames.ContainsByPredicate(
                    [&Entry](const FString& Name) { return Name.Equals(Entry.ParticipantName, ESearchCase::IgnoreCase); }))
            {
                Result.Warnings.Add(FString::Printf(TEXT("Speaker '%s' is not one of the dialogue's AI participants (%s)."),
                    *Entry.ParticipantName, *FString::Join(AllowedNames, TEXT(", "))));
            }
            if (!ADSImportHelper::FindTagForParticipant(Entry.ParticipantName, AllTags).IsValid())
            {
                Result.Warnings.Add(FString::Printf(TEXT("Speaker '%s' does not match any GameplayTag; its nodes will be imported without a participant."),
                    *Entry.ParticipantName));
            }
        }

        if (Entry.OptionNumber > 0)
        {
            ++Result.OptionCount;
        }
    }

    if (Result.bUpdatesExistingNodes)
    {
        for (int32 Index = 0; Index < Entries.Num(); ++Index)
        {
            const ADSImportHelper::FParsedDialogueEntry& Entry = Entries[Index];
            if (!Entry.bHasNodeId || !Dialogue->GetNodeById(Entry.NodeId))
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Entry %d (%s): %s; update-mode importer would skip this entry. Restore its matching [Node:GUID] or use a screenplay without existing node GUIDs to create a new graph."),
                    Index + 1, *Entry.ParticipantName,
                    Entry.bHasNodeId ? TEXT("node GUID does not match an existing node") : TEXT("missing [Node:GUID]")));
            }
        }
    }

    Result.bValid = Result.Errors.Num() == 0;
    return Result;
}

void UADSEditorSubsystem::ImportDialogueFromFile(UADSDialogue* Dialogue)
{
    if (!Dialogue)
    {
        ShowNotification(TEXT("No dialogue selected"), false);
        return;
    }

#if WITH_EDITORONLY_DATA

    // ----------------------------------------------------------------
    // 1. Open file dialog to select the screenplay file
    // ----------------------------------------------------------------
    FString InitialDirectory = Dialogue->ExportImportDirectory.Path;
    if (!InitialDirectory.IsEmpty() && FPaths::IsRelative(InitialDirectory))
    {
        InitialDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), InitialDirectory);
    }

    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        ShowNotification(TEXT("Desktop platform not available"), false);
        return;
    }

    TArray<FString> OpenFilenames;
    const bool bOpened = DesktopPlatform->OpenFileDialog(
        FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
        TEXT("Select Screenplay File to Import"),
        InitialDirectory,
        TEXT(""),
        TEXT("Text Files (*.txt)|*.txt|All Files (*.*)|*.*"),
        EFileDialogFlags::None,
        OpenFilenames
    );

    if (!bOpened || OpenFilenames.Num() == 0)
    {
        return; // User cancelled
    }

    const FString FilePath = OpenFilenames[0];

    // ----------------------------------------------------------------
    // 2. Read the screenplay file and delegate to the text-based importer
    // ----------------------------------------------------------------
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
    {
        ShowNotification(FString::Printf(TEXT("Failed to read file: %s"), *FilePath), false);
        return;
    }

    ImportDialogueFromText(Dialogue, FileContent, FPaths::GetCleanFilename(FilePath), /*bConfirm*/ true);

#endif
}

void UADSEditorSubsystem::ImportDialogueFromText(UADSDialogue* Dialogue, const FString& ScreenplayText, const FString& SourceName, bool bConfirm)
{
    if (!Dialogue)
    {
        ShowNotification(TEXT("No dialogue selected"), false);
        return;
    }

#if WITH_EDITORONLY_DATA
    // ----------------------------------------------------------------
    // Parse the screenplay text
    // ----------------------------------------------------------------
    TArray<ADSImportHelper::FParsedDialogueEntry> ParsedEntries = ADSImportHelper::ParseScreenplay(ScreenplayText);

    if (ParsedEntries.Num() == 0)
    {
        ShowNotification(TEXT("No dialogue entries found in the text. Check the screenplay format."), false);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Parsed %d dialogue entries from: %s"), ParsedEntries.Num(), *SourceName);

    // ----------------------------------------------------------------
    // 3. Cache all existing GameplayTags for participant matching
    // ----------------------------------------------------------------
    FGameplayTagContainer AllTags;
    UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, true);

    // ----------------------------------------------------------------
    // 4. Determine import mode: UPDATE (file has GUIDs matching existing nodes) or CREATE (plain screenplay)
    // ----------------------------------------------------------------
    int32 MatchedGuidCount = 0;
    int32 TotalWithGuids = 0;

    for (const ADSImportHelper::FParsedDialogueEntry& Entry : ParsedEntries)
    {
        if (Entry.bHasNodeId)
        {
            TotalWithGuids++;
            if (Dialogue->GetNodeById(Entry.NodeId) != nullptr)
            {
                MatchedGuidCount++;
            }
        }
    }

    const bool bUseUpdateMode = (MatchedGuidCount > 0);

    // ----------------------------------------------------------------
    // 5. Confirmation dialog
    // ----------------------------------------------------------------
    if (bConfirm && bUseUpdateMode)
    {
        const int32 NewEntries = ParsedEntries.Num() - MatchedGuidCount;
        FString ConfirmMsg = FString::Printf(
            TEXT("Update Mode\n\n"
                 "Found %d entries matching existing nodes (will update text and participant).\n"
                 "%d entries have no matching node (will be skipped).\n\n"
                 "Source: %s\n\nContinue?"),
            MatchedGuidCount, NewEntries, *SourceName);

        if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmMsg)) != EAppReturnType::Yes)
        {
            return;
        }
    }
    else if (bConfirm)
    {
        int32 RegularEntries = 0;
        int32 OptionEntries = 0;
        for (const auto& E : ParsedEntries)
        {
            if (E.OptionNumber > 0) OptionEntries++;
            else RegularEntries++;
        }

        FString ConfirmMsg = FString::Printf(
            TEXT("Create Mode - New Dialogue Graph\n\n"
                 "This will REPLACE the current dialogue with:\n"
                 "  %d dialogue nodes\n"
                 "  %d response options\n\n"
                 "Source: %s\n\nContinue?"),
            RegularEntries, OptionEntries, *SourceName);

        if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmMsg)) != EAppReturnType::Yes)
        {
            return;
        }
    }

    // ================================================================
    // UPDATE MODE: Update text/tags on existing nodes matched by GUID
    // ================================================================
    if (bUseUpdateMode)
    {
        const FScopedTransaction Transaction(FText::FromString(TEXT("Update Dialogue from Screenplay")));
        Dialogue->Modify();

        int32 UpdatedCount = 0;
        int32 SkippedCount = 0;

        for (const ADSImportHelper::FParsedDialogueEntry& Entry : ParsedEntries)
        {
            if (!Entry.bHasNodeId)
            {
                SkippedCount++;
                continue;
            }

            UAGSGraphNode* Node = Dialogue->GetNodeById(Entry.NodeId);
            if (!Node)
            {
                SkippedCount++;
                UE_LOG(LogTemp, Warning, TEXT("Import: Node ID not found: %s (participant: %s)"),
                    *Entry.NodeId.ToString(), *Entry.ParticipantName);
                continue;
            }

            UADSGraphNode* DialogueNode = Cast<UADSGraphNode>(Node);
            if (DialogueNode)
            {
                DialogueNode->Modify();
                DialogueNode->SetText(FText::FromString(Entry.Text));

                FGameplayTag Tag = ADSImportHelper::FindTagForParticipant(Entry.ParticipantName, AllTags);
                if (Tag.IsValid())
                {
                    DialogueNode->SetParticipantTag(Tag);
                }
                UpdatedCount++;
            }
        }

        Dialogue->MarkPackageDirty();

        FString ResultMsg = FString::Printf(TEXT("Updated %d dialogue entries."), UpdatedCount);
        if (SkippedCount > 0)
        {
            ResultMsg += FString::Printf(TEXT(" (%d entries skipped - no matching node)"), SkippedCount);
        }
        ShowNotification(ResultMsg, true);
        UE_LOG(LogTemp, Log, TEXT("%s"), *ResultMsg);
    }
    // ================================================================
    // CREATE MODE: Build entire dialogue graph from screenplay
    // ================================================================
    else
    {
        const FScopedTransaction Transaction(FText::FromString(TEXT("Import Dialogue from Screenplay")));
        Dialogue->Modify();

        // Ensure EdGraph exists
        UEdGraph_AGSGraph* EdGraph = Cast<UEdGraph_AGSGraph>(Dialogue->EdGraph);
        if (!EdGraph)
        {
            Dialogue->EdGraph = NewObject<UEdGraph_AGSGraph>(Dialogue, UEdGraph_AGSGraph::StaticClass(), NAME_None, RF_Transactional);
            Dialogue->EdGraph->Schema = UAssetGraphSchema_AGSGraph::StaticClass();
            EdGraph = Cast<UEdGraph_AGSGraph>(Dialogue->EdGraph);
        }

        EdGraph->Modify();

        // Clear existing graph nodes
        {
            TArray<UEdGraphNode*> NodesToRemove;
            for (UEdGraphNode* Node : EdGraph->Nodes)
            {
                NodesToRemove.Add(Node);
            }
            for (UEdGraphNode* Node : NodesToRemove)
            {
                EdGraph->RemoveNode(Node);
            }
        }

        Dialogue->AllNodes.Empty();
        Dialogue->RootNodes.Empty();

        // Create nodes from parsed entries
        struct FCreatedNodeInfo
        {
            UEdNode_AGSGraphNode* EdNode = nullptr;
            bool bIsOption = false;
        };

        TArray<FCreatedNodeInfo> CreatedNodes;
        bool bFirstRegularNode = true;
        float PosY = 0.0f;
        bool bPrevWasOption = false;
        float OptionGroupStartY = 0.0f;
        TSet<FGuid> ImportedNodeIds;

        for (int32 i = 0; i < ParsedEntries.Num(); i++)
        {
            const ADSImportHelper::FParsedDialogueEntry& Entry = ParsedEntries[i];

            // Determine the runtime node type
            UADSGraphNode* RuntimeNode = nullptr;
            const bool bIsOption = (Entry.OptionNumber > 0);

            if (bFirstRegularNode && !bIsOption)
            {
                RuntimeNode = NewObject<UADSStartDialogueNode>(Dialogue);
                bFirstRegularNode = false;
            }
            else if (bIsOption)
            {
                RuntimeNode = NewObject<UADSDialogueResponseNode>(Dialogue);
            }
            else
            {
                RuntimeNode = NewObject<UADSDialogueNode>(Dialogue);
                bFirstRegularNode = false;
            }

            // Configure the runtime node
            if (Entry.bHasNodeId && Entry.NodeId.IsValid() && !ImportedNodeIds.Contains(Entry.NodeId))
            {
                RuntimeNode->SetNodeId(Entry.NodeId);
                ImportedNodeIds.Add(Entry.NodeId);
            }
            else
            {
                RuntimeNode->GenerateNewID();
                if (Entry.bHasNodeId)
                {
                    UE_LOG(LogTemp, Warning, TEXT("Import: Could not preserve node ID '%s' for participant '%s'; generated a new ID."),
                        *Entry.NodeId.ToString(), *Entry.ParticipantName);
                }
            }
            RuntimeNode->Graph = Dialogue;
            RuntimeNode->SetText(FText::FromString(Entry.Text));

            // Match participant tag
            FGameplayTag ParticipantTag = ADSImportHelper::FindTagForParticipant(Entry.ParticipantName, AllTags);
            if (ParticipantTag.IsValid())
            {
                RuntimeNode->SetParticipantTag(ParticipantTag);
            }

            // Calculate position for layout
            float PosX = 0.0f;

            if (bIsOption)
            {
                if (!bPrevWasOption)
                {
                    OptionGroupStartY = PosY;
                }
                PosX = (Entry.OptionNumber - 1) * 400.0f;
                bPrevWasOption = true;
            }
            else
            {
                if (bPrevWasOption)
                {
                    PosY += 300.0f;
                }
                PosX = 0.0f;
                bPrevWasOption = false;
            }

            // Create the editor graph node
            UEdNode_AGSGraphNode* EdNode = ADSImportHelper::CreateEditorNode(
                EdGraph, RuntimeNode, PosX, bIsOption ? OptionGroupStartY : PosY);

            FCreatedNodeInfo Info;
            Info.EdNode = EdNode;
            Info.bIsOption = bIsOption;
            CreatedNodes.Add(Info);

            if (!bIsOption)
            {
                PosY += 300.0f;
            }
        }

        // Connect nodes via pins
        UEdNode_AGSGraphNode* LastRegularNode = nullptr;
        TArray<UEdNode_AGSGraphNode*> PendingOptionNodes;

        for (int32 i = 0; i < CreatedNodes.Num(); i++)
        {
            const FCreatedNodeInfo& Info = CreatedNodes[i];

            if (Info.bIsOption)
            {
                if (LastRegularNode)
                {
                    ADSImportHelper::ConnectEditorNodes(LastRegularNode, Info.EdNode);
                }
                PendingOptionNodes.Add(Info.EdNode);
            }
            else
            {
                if (PendingOptionNodes.Num() > 0)
                {
                    for (UEdNode_AGSGraphNode* OptionNode : PendingOptionNodes)
                    {
                        ADSImportHelper::ConnectEditorNodes(OptionNode, Info.EdNode);
                    }
                    PendingOptionNodes.Empty();
                }
                else if (LastRegularNode)
                {
                    ADSImportHelper::ConnectEditorNodes(LastRegularNode, Info.EdNode);
                }

                LastRegularNode = Info.EdNode;
            }
        }

        // Rebuild the runtime graph from the EdGraph
        EdGraph->RebuildAGSGraph();
        EdGraph->NotifyGraphChanged();

        Dialogue->MarkPackageDirty();

        // Report results
        int32 RegularCount = 0;
        int32 OptionCount = 0;
        for (const FCreatedNodeInfo& Info : CreatedNodes)
        {
            if (Info.bIsOption) OptionCount++;
            else RegularCount++;
        }

        const FString ResultMsg = FString::Printf(
            TEXT("Created dialogue graph: %d dialogue nodes, %d response options from '%s'"),
            RegularCount, OptionCount, *SourceName);
        ShowNotification(ResultMsg, true);
        UE_LOG(LogTemp, Log, TEXT("%s"), *ResultMsg);
    }

#endif
}

void UADSEditorSubsystem::RegenerateDialogueTransitions(UADSDialogue* Dialogue)
{
    if (!Dialogue)
    {
        ShowNotification(TEXT("No dialogue selected"), false);
        return;
    }

#if WITH_EDITORONLY_DATA
    UEdGraph_AGSGraph* EdGraph = Cast<UEdGraph_AGSGraph>(Dialogue->EdGraph);
    if (!EdGraph)
    {
        ShowNotification(TEXT("This dialogue has no editor graph to regenerate."), false);
        return;
    }

    const FScopedTransaction Transaction(FText::FromString(TEXT("Regenerate Dialogue Transitions")));
    Dialogue->Modify();
    EdGraph->Modify();

    auto GetSafeOutputPin = [](UEdNode_AGSGraphNode* EdNode) -> UEdGraphPin*
    {
        return (EdNode && EdNode->Pins.Num() >= 2) ? EdNode->Pins[1] : nullptr;
    };
    auto GetSafeInputPin = [](UEdNode_AGSGraphNode* EdNode) -> UEdGraphPin*
    {
        return (EdNode && EdNode->Pins.Num() >= 2) ? EdNode->Pins[0] : nullptr;
    };

    TMap<UAGSGraphNode*, UEdNode_AGSGraphNode*> RuntimeToEd;
    TSet<TPair<UEdNode_AGSGraphNode*, UEdNode_AGSGraphNode*>> ExistingEdges;
    TSet<TPair<UEdNode_AGSGraphNode*, UEdNode_AGSGraphNode*>> PendingConnections;
    TArray<TPair<UEdGraphPin*, UEdGraphPin*>> DirectPinLinks;

    for (UEdGraphNode* GraphNode : EdGraph->Nodes)
    {
        if (UEdNode_AGSGraphNode* EdNode = Cast<UEdNode_AGSGraphNode>(GraphNode))
        {
            if (EdNode->AGSGraphNode)
            {
                EdNode->AGSGraphNode->Graph = Dialogue;
                RuntimeToEd.Add(EdNode->AGSGraphNode, EdNode);
            }
        }
        else if (UEdNode_AGSGraphEdge* EdgeNode = Cast<UEdNode_AGSGraphEdge>(GraphNode))
        {
            // MinimalAPI: GetStartNode/GetEndNode are not exported from AGSGraphEditor.
            UEdNode_AGSGraphNode* StartNode = (EdgeNode->Pins.Num() >= 1 && EdgeNode->Pins[0]->LinkedTo.Num() > 0)
                ? Cast<UEdNode_AGSGraphNode>(EdgeNode->Pins[0]->LinkedTo[0]->GetOwningNode())
                : nullptr;
            UEdNode_AGSGraphNode* EndNode = (EdgeNode->Pins.Num() >= 2 && EdgeNode->Pins[1]->LinkedTo.Num() > 0)
                ? Cast<UEdNode_AGSGraphNode>(EdgeNode->Pins[1]->LinkedTo[0]->GetOwningNode())
                : nullptr;
            if (StartNode && EndNode)
            {
                ExistingEdges.Add(TPair<UEdNode_AGSGraphNode*, UEdNode_AGSGraphNode*>(StartNode, EndNode));
            }
        }
    }

    auto QueueConnection = [&](UEdNode_AGSGraphNode* FromNode, UEdNode_AGSGraphNode* ToNode)
    {
        if (!FromNode || !ToNode || FromNode == ToNode)
        {
            return;
        }

        const TPair<UEdNode_AGSGraphNode*, UEdNode_AGSGraphNode*> Pair(FromNode, ToNode);
        if (!ExistingEdges.Contains(Pair))
        {
            PendingConnections.Add(Pair);
        }
    };

    for (const TPair<UAGSGraphNode*, UEdNode_AGSGraphNode*>& Mapping : RuntimeToEd)
    {
        UEdNode_AGSGraphNode* FromNode = Mapping.Value;
        UEdGraphPin* OutputPin = GetSafeOutputPin(FromNode);
        if (!OutputPin)
        {
            continue;
        }

        for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
        {
            if (!LinkedPin)
            {
                continue;
            }

            if (UEdNode_AGSGraphNode* ToNode = Cast<UEdNode_AGSGraphNode>(LinkedPin->GetOwningNode()))
            {
                QueueConnection(FromNode, ToNode);
                DirectPinLinks.Add(TPair<UEdGraphPin*, UEdGraphPin*>(OutputPin, LinkedPin));
            }
        }
    }

    for (UAGSGraphNode* RuntimeNode : Dialogue->AllNodes)
    {
        UEdNode_AGSGraphNode* FromNode = RuntimeToEd.FindRef(RuntimeNode);
        if (!FromNode)
        {
            continue;
        }

        for (UAGSGraphNode* ChildNode : RuntimeNode->ChildrenNodes)
        {
            QueueConnection(FromNode, RuntimeToEd.FindRef(ChildNode));
        }
    }

    for (const TPair<UEdGraphPin*, UEdGraphPin*>& Link : DirectPinLinks)
    {
        if (Link.Key && Link.Value)
        {
            Link.Key->Modify();
            Link.Value->Modify();
            Link.Key->BreakLinkTo(Link.Value);
        }
    }

    int32 CreatedCount = 0;
    int32 FailedCount = 0;
    for (const TPair<UEdNode_AGSGraphNode*, UEdNode_AGSGraphNode*>& Pair : PendingConnections)
    {
        UEdNode_AGSGraphNode* FromNode = Pair.Key;
        UEdNode_AGSGraphNode* ToNode = Pair.Value;
        UEdGraphPin* OutputPin = GetSafeOutputPin(FromNode);
        UEdGraphPin* InputPin = GetSafeInputPin(ToNode);
        if (!OutputPin || !InputPin)
        {
            FailedCount++;
            continue;
        }

        const UEdGraphSchema* Schema = FromNode->GetSchema();
        if (Schema && Schema->TryCreateConnection(OutputPin, InputPin))
        {
            FromNode->NodeConnectionListChanged();
            ToNode->NodeConnectionListChanged();
            CreatedCount++;
        }
        else
        {
            FailedCount++;
            UE_LOG(LogTemp, Warning, TEXT("Regenerate: Failed to create transition from '%s' to '%s'."),
                *FromNode->GetNodeTitle(ENodeTitleType::ListView).ToString(),
                *ToNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
        }
    }

    EdGraph->RebuildAGSGraph();
    EdGraph->NotifyGraphChanged();
    Dialogue->MarkPackageDirty();

    FString ResultMsg;
    if (CreatedCount == 0 && FailedCount == 0)
    {
        ResultMsg = TEXT("All dialogue transitions are already valid.");
    }
    else
    {
        ResultMsg = FString::Printf(TEXT("Regenerated %d dialogue transition(s)."), CreatedCount);
        if (FailedCount > 0)
        {
            ResultMsg += FString::Printf(TEXT(" %d failed."), FailedCount);
        }
    }
    ShowNotification(ResultMsg, FailedCount == 0);
    UE_LOG(LogTemp, Log, TEXT("%s"), *ResultMsg);
#endif
}

// ============================================================================
// AI Dialogue Generation (OpenAI-compatible)
// ============================================================================

bool UADSEditorSubsystem::BuildDialogueWritingBrief(UADSDialogue* Dialogue, const FString& DescriptionOverride, FADSDialogueWritingBrief& OutBrief, FString& OutError) const
{
    OutBrief = FADSDialogueWritingBrief();
    if (!Dialogue)
    {
        OutError = TEXT("No dialogue selected");
        return false;
    }

    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();

    const FString Description = DescriptionOverride.IsEmpty() ? Dialogue->AIConversationDescription : DescriptionOverride;
    if (Description.IsEmpty())
    {
        OutError = TEXT("Please fill the 'AI Conversation Description' field on the dialogue asset first.");
        return false;
    }

    // ----------------------------------------------------------------
    // Build participant list from AIParticipantTags on the dialogue asset.
    // Personalities are looked up from the Voice Config data asset when available.
    // ----------------------------------------------------------------
    if (Dialogue->AIParticipantTags.Num() == 0)
    {
        OutError = TEXT("No AI Participant Tags set on this dialogue. Add at least one character tag (e.g. Character.Guard) in the Yarin section.");
        return false;
    }

    UADSVoiceConfigDataAsset* VoiceConfig = Settings->GetVoiceConfigDataAsset();
    if (!VoiceConfig)
    {
        OutError = TEXT("No Voice Config Data Asset set in Project Settings > Ascent Dialogue Settings. "
                              "Character personalities cannot be sent to the AI.");
        return false;
    }

    FString ParticipantsBlock;
    TArray<FString> ParticipantNames;
    FString PlayerName; // name of the character tag that contains "Player"

    for (const FGameplayTag& Tag : Dialogue->AIParticipantTags)
    {
        const FString Name = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(Tag);
        if (Name.IsEmpty() || Name == TEXT("None"))
        {
            const FString Msg = FString::Printf(TEXT("AI Generation: skipping tag '%s' — could not extract a valid character name."), *Tag.ToString());
            UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
            OutBrief.Warnings.Add(Msg);
            continue;
        }

        ParticipantNames.Add(Name);

        // Detect player character (tag leaf contains "player", case-insensitive)
        if (PlayerName.IsEmpty() && Name.ToLower().Contains(TEXT("player")))
        {
            PlayerName = Name;
        }

        // Personality is always read from the Voice Config Data Asset
        const FADSVoiceSettings* FoundSettings = VoiceConfig->VoiceConfigs.Find(Tag);
        const FString Personality = (FoundSettings && !FoundSettings->Personality.IsEmpty())
            ? FoundSettings->Personality
            : TEXT("(no personality set in Voice Config for this character)");

        ParticipantsBlock += FString::Printf(TEXT("- %s: %s\n"), *Name, *Personality);
    }

    if (ParticipantNames.Num() == 0)
    {
        OutError = TEXT("AI Participant Tags are set but no valid character names could be resolved. Check the tags.");
        return false;
    }

    // ----------------------------------------------------------------
    // Build the dialogue context
    // ----------------------------------------------------------------
    FString DialogueContext;
    DialogueContext += FString::Printf(TEXT("Dialogue asset name: %s\n"), *Dialogue->GetName());
    if (Dialogue->GetDialogueTag().IsValid())
    {
        DialogueContext += FString::Printf(TEXT("Dialogue tag: %s\n"), *Dialogue->GetDialogueTag().ToString());
    }
    if (Dialogue->GetDefaultParticipantTag().IsValid())
    {
        const FString DefaultName = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(Dialogue->GetDefaultParticipantTag());
        DialogueContext += FString::Printf(TEXT("Primary speaker: %s\n"), *DefaultName);
    }

    // ----------------------------------------------------------------
    // Build the prompt
    // ----------------------------------------------------------------
    const EADSAIPlayerResponseMode ResponseMode = Dialogue->AIPlayerResponseMode;
    const FString ParticipantNamesCsv = FString::Join(ParticipantNames, TEXT(", "));

    // The name used for the player character in the screenplay (empty = no player in this dialogue)
    const FString EffectivePlayerName = PlayerName.IsEmpty() ? TEXT("Player") : PlayerName;

    // Build NPC name list (everyone except the player)
    TArray<FString> NpcNames;
    if (!PlayerName.IsEmpty())
    {
        NpcNames = ParticipantNames.FilterByPredicate(
            [&PlayerName](const FString& N){ return !N.Equals(PlayerName, ESearchCase::IgnoreCase); });
    }
    const FString NpcNamesCsv = FString::Join(NpcNames, TEXT(", "));

    FString& SystemPrompt = OutBrief.Instructions;
    SystemPrompt += TEXT("You are a professional video game dialogue writer.\n");
    SystemPrompt += TEXT("Generate a complete dialogue as a screenplay using EXACTLY the following plain-text format and nothing else:\n\n");
    SystemPrompt += TEXT("CHARACTER_NAME:\n");
    SystemPrompt += TEXT("Line of dialogue text.\n\n");

    // ---- BRANCHING RULE (applies to ALL modes) ----
    SystemPrompt += FString::Printf(
        TEXT("BRANCHING RULE (mandatory in every mode):\n"
             "NEVER invent branches or multiple options on your own.\n"
             "Add multiple %s options (branching) ONLY when the conversation description below\n"
             "EXPLICITLY states that the player must choose between alternatives\n"
             "(e.g. 'the player can decide whether to help or refuse').\n"
             "When branching IS described, use the numbered option format:\n"
             "%s (Option 1):\nFirst choice text.\n\n"
             "%s (Option 2):\nSecond choice text.\n\n"
             "When branching is NOT described, write exactly ONE %s response per beat.\n\n"),
        *EffectivePlayerName, *EffectivePlayerName, *EffectivePlayerName, *EffectivePlayerName);

    // ---- NODE TYPE per mode ----
    if (ResponseMode == EADSAIPlayerResponseMode::OptionButtons)
    {
        SystemPrompt += FString::Printf(
            TEXT("NODE TYPE — Player Answers mode:\n"
                 "Every %s line (whether a single response or a branch) MUST use the numbered option format:\n"
                 "%s (Option 1):\nLine of dialogue.\n\n"
                 "Even when there is only one response, still write it as '%s (Option 1):'.\n"
                 "NEVER write a plain '%s:' line.\n\n"),
            *EffectivePlayerName, *EffectivePlayerName, *EffectivePlayerName, *EffectivePlayerName);
    }
    else if (ResponseMode == EADSAIPlayerResponseMode::DialogueNodes)
    {
        SystemPrompt += FString::Printf(
            TEXT("NODE TYPE — Player Nodes mode:\n"
                 "Every %s line MUST use the plain dialogue format:\n"
                 "%s:\nLine of dialogue.\n\n"
                 "NEVER use the '%s (Option N):' format — not even for branches.\n"
                 "When branching is described, write each option as a separate plain '%s:' block.\n\n"),
            *EffectivePlayerName, *EffectivePlayerName, *EffectivePlayerName, *EffectivePlayerName);
    }
    else // Mixed
    {
        SystemPrompt += FString::Printf(
            TEXT("NODE TYPE — Mixed mode:\n"
                 "For each %s beat, choose the most appropriate node type:\n"
                 "  - Use '%s (Option N):' when the beat is a selectable choice for the player.\n"
                 "  - Use plain '%s:' when the beat is a natural dialogue line.\n"
                 "The choice of node type is independent of branching — a single option button\n"
                 "('%s (Option 1):') is valid when the beat calls for a button but no real branch.\n\n"),
            *EffectivePlayerName, *EffectivePlayerName, *EffectivePlayerName, *EffectivePlayerName);
    }

    // ---- Hard rules ----
    SystemPrompt += TEXT("RULES:\n");
    SystemPrompt += TEXT("- Use ONLY the following character names as speakers: ") + ParticipantNamesCsv + TEXT(". Do NOT invent any other character names.\n");
    if (NpcNames.Num() > 0)
    {
        SystemPrompt += FString::Printf(
            TEXT("- %s are NPCs: they NEVER use the '(Option N)' format. Only %s can have option buttons.\n"),
            *NpcNamesCsv, *EffectivePlayerName);
    }
    SystemPrompt += TEXT("- The speaker name line MUST end with a colon ':'.\n");
    SystemPrompt += TEXT("- Put the dialogue text on the line(s) directly beneath the speaker line.\n");
    SystemPrompt += TEXT("- Separate each entry with a single blank line.\n");
    SystemPrompt += TEXT("- Do NOT include any markdown, headers, numbering, stage directions in brackets, or commentary.\n");
    SystemPrompt += TEXT("- Keep each line of dialogue natural, in-character and concise.\n");

    FString& UserMessage = OutBrief.Request;
    UserMessage += FString::Printf(
        TEXT("Target dialogue length: approximately %d speaker turns total (all characters combined). "
             "Adjust naturally if the description requires slightly more or fewer beats.\n\n"),
        Dialogue->AIDialogueTreeLength);
    UserMessage += TEXT("Conversation description / constraints:\n");
    UserMessage += Description + TEXT("\n\n");
    UserMessage += TEXT("Dialogue context:\n");
    UserMessage += DialogueContext + TEXT("\n");
    UserMessage += TEXT("Participants and their personalities:\n");
    UserMessage += ParticipantsBlock;

    OutBrief.ParticipantNames = ParticipantNames;
    OutBrief.PlayerName = EffectivePlayerName;
    return true;
}

void UADSEditorSubsystem::GenerateDialogueWithAI(UADSDialogue* Dialogue)
{
#if WITH_EDITORONLY_DATA
    if (!Dialogue)
    {
        ShowNotification(TEXT("No dialogue selected"), false);
        return;
    }

    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();

    const FString APIKey = Settings->GetAIAPIKey();
    if (APIKey.IsEmpty())
    {
        ShowNotification(TEXT("AI API Key is not set. Configure it in Project Settings > Ascent Dialogue Settings > Conversational AI."), false);
        return;
    }

    FADSDialogueWritingBrief Brief;
    FString BriefError;
    if (!BuildDialogueWritingBrief(Dialogue, FString(), Brief, BriefError))
    {
        ShowNotification(BriefError, false);
        return;
    }
    for (const FString& Warning : Brief.Warnings)
    {
        ShowNotification(Warning, false);
    }
    const FString& SystemPrompt = Brief.Instructions;
    const FString& UserMessage = Brief.Request;

    const FString Model = Settings->GetAIModel();
    // Allow a larger response budget than a single line, since we are generating a whole dialogue
    const int32 MaxTok = FMath::Max(Settings->GetMaxTokens(), 1024);

    const FString Payload = Settings->ProcessPayloadTemplate(SystemPrompt, UserMessage, Model, MaxTok);

    // Validate JSON payload before sending
    {
        TSharedPtr<FJsonObject> ValidationJson;
        TSharedRef<TJsonReader<>> ValidationReader = TJsonReaderFactory<>::Create(Payload);
        if (!FJsonSerializer::Deserialize(ValidationReader, ValidationJson))
        {
            ShowNotification(TEXT("Generated AI payload is invalid JSON. Check the AI Payload Template in settings."), false);
            UE_LOG(LogTemp, Error, TEXT("Invalid AI payload: %s"), *Payload);
            return;
        }
    }

    const FString APIEndpoint = Settings->GetAIAPIEndpoint();
    if (APIEndpoint.IsEmpty())
    {
        ShowNotification(TEXT("AI API Endpoint is not set. Configure it in Project Settings > Ascent Dialogue Settings > Conversational AI."), false);
        return;
    }

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(APIEndpoint);
    Request->SetVerb(TEXT("POST"));

    const TMap<FString, FString> Headers = Settings->ProcessHeadersTemplate(APIKey);
    for (const auto& HeaderPair : Headers)
    {
        Request->SetHeader(HeaderPair.Key, HeaderPair.Value);
    }
    Request->SetContentAsString(Payload);

    // Loading notification with throbber
    FNotificationInfo LoadingInfo(FText::FromString(TEXT("Generating dialogue with AI...")));
    LoadingInfo.bFireAndForget = false;
    LoadingInfo.bUseThrobber = true;
    LoadingInfo.FadeOutDuration = 0.5f;
    LoadingInfo.ExpireDuration = 0.0f;
    TSharedPtr<SNotificationItem> LoadingNotification = FSlateNotificationManager::Get().AddNotification(LoadingInfo);
    if (LoadingNotification.IsValid())
    {
        LoadingNotification->SetCompletionState(SNotificationItem::CS_Pending);
    }

    TWeakObjectPtr<UADSDialogue> WeakDialogue(Dialogue);

    Request->OnProcessRequestComplete().BindLambda(
        [this, WeakDialogue, LoadingNotification](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            // Dismiss the loading throbber and also spawn a persistent error notification widget.
            auto FailNotify = [this, LoadingNotification](const FString& Msg)
            {
                if (LoadingNotification.IsValid())
                {
                    LoadingNotification->SetCompletionState(SNotificationItem::CS_Fail);
                    LoadingNotification->SetText(FText::FromString(Msg));
                    LoadingNotification->ExpireAndFadeout();
                }
                // Spawn a separate error notification with a readable duration so the
                // user is guaranteed to see the message even if the throbber fades fast.
                ShowNotification(Msg, false);
            };

            if (!bWasSuccessful || !Response.IsValid())
            {
                FailNotify(TEXT("AI request failed (connection error)"));
                return;
            }

            const int32 Code = Response->GetResponseCode();
            const FString Content = Response->GetContentAsString();
            if (Code != 200)
            {
                FailNotify(FString::Printf(TEXT("AI request failed (HTTP %d)"), Code));
                UE_LOG(LogTemp, Error, TEXT("AI generation failed %d: %s"), Code, *Content);
                return;
            }

            TSharedPtr<FJsonObject> Json;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
            if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
            {
                FailNotify(TEXT("Failed to parse AI response JSON"));
                return;
            }

            FString GeneratedText;
            const TArray<TSharedPtr<FJsonValue>>* Choices;
            if (Json->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
            {
                const TSharedPtr<FJsonObject> FirstChoice = (*Choices)[0]->AsObject();
                if (FirstChoice.IsValid())
                {
                    const TSharedPtr<FJsonObject> Message = FirstChoice->GetObjectField(TEXT("message"));
                    if (Message.IsValid())
                    {
                        GeneratedText = Message->GetStringField(TEXT("content"));
                    }
                }
            }

            if (GeneratedText.TrimStartAndEnd().IsEmpty())
            {
                FailNotify(TEXT("AI response did not contain any dialogue text"));
                return;
            }

            if (LoadingNotification.IsValid())
            {
                LoadingNotification->SetCompletionState(SNotificationItem::CS_Success);
                LoadingNotification->SetText(FText::FromString(TEXT("AI dialogue generated")));
                LoadingNotification->ExpireAndFadeout();
            }

            if (!WeakDialogue.IsValid())
            {
                FailNotify(TEXT("Dialogue asset was unloaded before the AI response arrived. Please try again."));
                return;
            }

            ShowGeneratedDialoguePreview(WeakDialogue.Get(), GeneratedText);
        });

    Request->ProcessRequest();
#else
    ShowNotification(TEXT("AI dialogue generation is editor-only."), false);
#endif
}

void UADSEditorSubsystem::ShowGeneratedDialoguePreview(UADSDialogue* Dialogue, const FString& GeneratedText, const FString& SourceName)
{
#if WITH_EDITORONLY_DATA
    TWeakObjectPtr<UADSDialogue> WeakDialogue(Dialogue);

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(FString::Printf(TEXT("%s Dialogue - Preview"), *SourceName)))
        .ClientSize(FVector2D(720.f, 620.f))
        .SupportsMaximize(true)
        .SupportsMinimize(false);

    TSharedRef<SMultiLineEditableTextBox> TextBox = SNew(SMultiLineEditableTextBox)
        .Text(FText::FromString(GeneratedText))
        .AutoWrapText(true);

    UADSEditorSubsystem* Self = this;
    TWeakPtr<SWindow> WeakWindow = Window;

    Window->SetContent(
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(8.f)
            [SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Review and edit the dialogue below, then click 'Import Dialogue' to build the graph.\nText with [Node:GUID] lines matching existing nodes only updates those nodes; otherwise importing REPLACES the current dialogue graph.")))
                    .AutoWrapText(true)]

        + SVerticalBox::Slot()
            .FillHeight(1.f)
            .Padding(8.f)
            [SNew(SScrollBox)
                + SScrollBox::Slot()
                    [TextBox]]

        + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(8.f)
            [SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4.f, 0.f)
                        [SNew(SButton)
                                .Text(FText::FromString(TEXT("Import Dialogue")))
                                .ToolTipText(FText::FromString(TEXT("Build the dialogue graph from the text above. This replaces the current dialogue.")))
                                .OnClicked_Lambda([Self, WeakDialogue, TextBox, WeakWindow, SourceName]() {
                                    if (WeakDialogue.IsValid())
                                    {
                                        const FString EditedText = TextBox->GetText().ToString();
                                        Self->ImportDialogueFromText(WeakDialogue.Get(), EditedText, SourceName, /*bConfirm*/ false);
                                    }
                                    if (TSharedPtr<SWindow> Win = WeakWindow.Pin())
                                    {
                                        Win->RequestDestroyWindow();
                                    }
                                    return FReply::Handled();
                                })]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4.f, 0.f)
                        [SNew(SButton)
                                .Text(FText::FromString(TEXT("Cancel")))
                                .OnClicked_Lambda([WeakWindow]() {
                                    if (TSharedPtr<SWindow> Win = WeakWindow.Pin())
                                    {
                                        Win->RequestDestroyWindow();
                                    }
                                    return FReply::Handled();
                                })]]);

    FSlateApplication::Get().AddWindow(Window);
#endif
}
