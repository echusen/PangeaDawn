// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ADSEditorSubsystem.h"

#include "ADSDialogueDeveloperSettings.h"
#include "ADSDialogueFunctionLibrary.h"
#include "ADSDialoguePartecipantComponent.h"
#include "ADSDialogueSubsystem.h"
#include "Graph/ADSDialogue.h"
#include "Graph/ADSDialogueResponseNode.h"
#include "Graph/ADSStartDialogueNode.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Factories/SoundFactory.h"
#include "FileHelpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Graph/ADSGraphNode.h"
#include "HAL/PlatformFilemanager.h"
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
#include "AssetRegistry/AssetRegistryModule.h"
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

USoundWave* UADSEditorSubsystem::CreateSoundWaveAsset(const TArray<uint8>& AudioData, const FString& AssetName, const FString& PackagePath)
{
    if (AudioData.Num() == 0) {
        UE_LOG(LogTemp, Error, TEXT("Cannot create sound wave from empty audio data"));
        return nullptr;
    }

    // Create temporary file path
    const FString TempDir = FPaths::ProjectIntermediateDir() / TEXT("VoiceTTS");
    const FString TempFilePath = TempDir / (AssetName + TEXT(".mp3"));

    // Ensure directory exists
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*TempDir)) {
        PlatformFile.CreateDirectoryTree(*TempDir);
    }

    // Save MP3 data to temporary file
    if (!FFileHelper::SaveArrayToFile(AudioData, *TempFilePath)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to save audio data to temp file: %s"), *TempFilePath);
        return nullptr;
    }

    // Create the full package path
    const FString FullPackagePath = PackagePath + AssetName;

    // Ensure the target directory exists in the content browser
    const FString ContentPath = FPaths::ProjectContentDir() + PackagePath.RightChop(6); // Remove "/Game/"
    if (!PlatformFile.DirectoryExists(*ContentPath)) {
        PlatformFile.CreateDirectoryTree(*ContentPath);
    }

    // Import using AssetTools with automated import data
    IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

    UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
    ImportData->bReplaceExisting = true;
    ImportData->Filenames.Add(TempFilePath);
    ImportData->DestinationPath = FPackageName::GetLongPackagePath(FullPackagePath);

    TArray<UObject*> ImportedAssets = AssetTools.ImportAssetsAutomated(ImportData);

    USoundWave* ImportedSoundWave = nullptr;
    if (ImportedAssets.Num() > 0) {
        ImportedSoundWave = Cast<USoundWave>(ImportedAssets[0]);
    }

    if (ImportedSoundWave) {
        UE_LOG(LogTemp, Log, TEXT("Successfully imported TTS audio as USoundWave: %s"), *FullPackagePath);

        // Clean up temporary file
        PlatformFile.DeleteFile(*TempFilePath);

        // Configure the sound wave properties
        ImportedSoundWave->bLooping = false;
        ImportedSoundWave->Volume = 1.0f;
        ImportedSoundWave->Pitch = 1.0f;

        return ImportedSoundWave;
    } else {
        UE_LOG(LogTemp, Error, TEXT("Failed to import audio file as USoundWave: %s"), *TempFilePath);

        // Clean up temporary file on failure
        PlatformFile.DeleteFile(*TempFilePath);

        return nullptr;
    }

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

void UADSEditorSubsystem::RefreshVoicesFromAPI()
{
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString APIKey = Settings->GetTTSVoiceGenAPIKey();

    if (APIKey.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("No TTS Voice API key set - cannot refresh voices"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Refreshing voices from TTS Voice API..."));

    // Clear current cache
    if (UADSDialogueSubsystem* DialogueSubsystem = GEngine->GetEngineSubsystem<UADSDialogueSubsystem>()) {
        // Clear current cache
        DialogueSubsystem->ClearVoiceCache();
    }

    // Create HTTP request
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Settings->GetTTSVoiceSelectionAPIEndPoint());
    Request->SetVerb(TEXT("GET"));
    Request->SetHeader(TEXT("xi-api-key"), APIKey);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    Request->OnProcessRequestComplete().BindLambda([this, APIKey](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful) {
        if (bWasSuccessful && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == 200) {
            FString ResponseString = HttpResponse->GetContentAsString();
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

            if (FJsonSerializer::Deserialize(Reader, JsonObject)) {
                const TArray<TSharedPtr<FJsonValue>>* VoicesArray;
                if (JsonObject->TryGetArrayField(TEXT("voices"), VoicesArray)) {
                    // Populate cache with real API data
                    TArray<FString> NewVoiceNames;
                    TMap<FString, FString> NewVoiceNameToID;

                    for (const auto& VoiceValue : *VoicesArray) {
                        const TSharedPtr<FJsonObject> VoiceObj = VoiceValue->AsObject();
                        if (VoiceObj.IsValid()) {
                            FString VoiceName;
                            FString VoiceId;

                            if (VoiceObj->TryGetStringField(TEXT("name"), VoiceName) && VoiceObj->TryGetStringField(TEXT("voice_id"), VoiceId)) {

                                NewVoiceNames.Add(VoiceName);
                                NewVoiceNameToID.Add(VoiceName, VoiceId);
                            }
                        }
                    }

                    if (UADSDialogueSubsystem* DialogueSubsystem = GEngine->GetEngineSubsystem<UADSDialogueSubsystem>()) {
                        // Clear current cache
                        DialogueSubsystem->UpdateVoiceCache(NewVoiceNames, NewVoiceNameToID, APIKey);
                    }

                    UE_LOG(LogTemp, Log, TEXT("Successfully refreshed %d voices from TTS Voice API"), NewVoiceNames.Num());
                    UE_LOG(LogTemp, Log, TEXT("Reopen the Details Panel to see updated voice dropdown"));
                    ShowNotification("Voice Refreshed!", true);

                } else {
                    UE_LOG(LogTemp, Error, TEXT("No 'voices' array found in API response"));
                }
            } else {
                UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response from TTS Voice API"));
            }
        } else {
            const FString ErrorMsg = HttpResponse.IsValid() ? FString::Printf(TEXT("HTTP %d"), HttpResponse->GetResponseCode()) : TEXT("Request failed");
            UE_LOG(LogTemp, Error, TEXT("Failed to refresh voices from TTS Voice API: %s"), *ErrorMsg);
            ShowNotification(ErrorMsg, false);
        }
    });

    // Start the async request
    Request->ProcessRequest();
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
    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString APIKey = Settings->GetTTSVoiceGenAPIKey();

    if (APIKey.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("TTS API Key not set in Project Settings"));
        return;
    }

    if (!dialogueNode) {
        UE_LOG(LogTemp, Warning, TEXT("Dialogue node is null - cannot generate TTS audio"));
        return;
    }

    FADSVoiceSettings VoiceConfig;
    if (!dialogueNode->TryGetParticipantVoiceConfig(VoiceConfig)) {
        UE_LOG(LogTemp, Warning, TEXT("Dialogue node has no voice config - cannot generate TTS audio"));
        return;
    }

    const FString UseVoiceID = VoiceConfig.GetSelectedVoiceID();
    if (UseVoiceID.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("No Voice ID specified"));
        return;
    }

    const FText Text = dialogueNode->GetDialogueText();
    if (Text.IsEmpty()) {
        UE_LOG(LogTemp, Warning, TEXT("No text to convert to speech"));
        return;
    }

    // Create JSON payload
    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetStringField(TEXT("text"), Text.ToString());
    JsonObject->SetStringField(TEXT("model_id"), Settings->GetTTSVoiceModel());

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

    Request->OnProcessRequestComplete().BindLambda([this, Settings, dialogueNode](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful) {
        if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200) {
            TArray<uint8> AudioData = Response->GetContent();
            UE_LOG(LogTemp, Log, TEXT("Generated TTS audio: %d bytes"), AudioData.Num());

            // Log successful TTS generation
            UE_LOG(LogTemp, Log, TEXT("Successfully generated TTS audio from TTS Voice API"));

            const FGameplayTag PartecipantTag = dialogueNode->GetParticipantTag();
            // Create USoundWave asset automatically

            FString ParticipantName = UADSDialogueFunctionLibrary::ExtractLastStringFromGameplayTag(PartecipantTag); 
            if (ParticipantName.IsEmpty() || ParticipantName == TEXT("None")) {
                ParticipantName = TEXT("Dialogue");
            }
            const FString TextSnippet = SanitizeAssetName(dialogueNode->GetDialogueText().ToString());
            const FString AssetName = FString::Printf(TEXT("TTS_%s_%s"), *ParticipantName, *TextSnippet);
            const FString PackagePath = TEXT("/Game/") + Settings->GetFinalTTSPath();

            USoundWave* NewSoundWave = CreateSoundWaveAsset(AudioData, AssetName, PackagePath);
            if (NewSoundWave) {
                // Automatically assign the generated sound to this node if enabled
                if (Settings->GetAutoAssignGeneratedAudio() && dialogueNode) {
                    dialogueNode->SetSoundToPlay(NewSoundWave);
                }

                UE_LOG(LogTemp, Log, TEXT("Successfully created TTS audio assets: %s"), *NewSoundWave->GetName());

                const FString successMsg = FString::Printf(TEXT("TTS Audio Generated: %s"), *AssetName);

                // Mark the package as dirty so it shows up for saving
                if (NewSoundWave->GetPackage()) {
                    NewSoundWave->GetPackage()->SetDirtyFlag(true);
                }
                ShowNotification(successMsg, true);
            } else {
                UE_LOG(LogTemp, Error, TEXT("Failed to create USoundWave asset from TTS audio"));

                // Fallback: save to temp file
                FString TempFilePath = FPaths::ProjectDir() / TEXT("Temp") / TEXT("_TTS.mp3");
                if (FFileHelper::SaveArrayToFile(AudioData, *TempFilePath)) {
                    UE_LOG(LogTemp, Log, TEXT("TTS audio saved to: %s"), *TempFilePath);
                }
            }

        } else {
            FString ErrorMsg = Response.IsValid() ? FString::Printf(TEXT("HTTP %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString()) : TEXT("Request failed");
            UE_LOG(LogTemp, Error, TEXT("Failed to generate TTS audio: %s"), *ErrorMsg);

            ShowNotification(ErrorMsg, false);
        }
    });

    Request->ProcessRequest();
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
            const FString ErrorMsg = HttpResponse.IsValid() ? FString::Printf(TEXT("HTTP %d"), HttpResponse->GetResponseCode()) : TEXT("Request failed");
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
            FString ErrorMsg = HttpResponse.IsValid() ? FString::Printf(TEXT("HTTP %d"), HttpResponse->GetResponseCode()) : TEXT("Request failed");
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
void UADSEditorSubsystem::GenerateFacialAnimation(UADSGraphNode* DialogueNode)
{
    if (!DialogueNode)
    {
        ShowNotification(TEXT("No dialogue node selected"), false);
        return;
    }

    USoundWave* SoundWave = Cast<USoundWave>(DialogueNode->GetSoundToPlay());
    if (!SoundWave)
    {
        ShowNotification(TEXT("Node has no SoundWave assigned. Generate TTS audio first."), false);
        return;
    }

    const UADSDialogueDeveloperSettings* Settings = GetDefault<UADSDialogueDeveloperSettings>();
    const FString OutputPath = TEXT("/Game/") + Settings->GetFacialAnimationOutputPath();

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const FString ContentPath = FPaths::ProjectContentDir() + Settings->GetFacialAnimationOutputPath();
    if (!PlatformFile.DirectoryExists(*ContentPath))
    {
        PlatformFile.CreateDirectoryTree(*ContentPath);
    }

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
        Performance->VisualizationMesh = VisMesh;
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
        ShowNotification(FString::Printf(TEXT("MetaHuman Animator cannot process: %s"), *Reason.ToString()), false);
        UE_LOG(LogTemp, Error, TEXT("MetaHuman Animator CanProcess() == false: %s"), *Reason.ToString());
        return;
    }

    // Show loading notification with throbber
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

    const EStartPipelineErrorType StartError = Performance->StartPipeline(true);

    if (StartError != EStartPipelineErrorType::None)
    {
        if (LoadingNotification.IsValid())
        {
            LoadingNotification->SetCompletionState(SNotificationItem::CS_Fail);
            LoadingNotification->SetText(FText::FromString(TEXT("MetaHuman Animator: Pipeline failed")));
            LoadingNotification->ExpireAndFadeout();
        }
        UE_LOG(LogTemp, Error, TEXT("MetaHuman Animator StartPipeline failed with error %d"), static_cast<int32>(StartError));
        return;
    }

    if (!Performance->ContainsAnimationData())
    {
        if (LoadingNotification.IsValid())
        {
            LoadingNotification->SetCompletionState(SNotificationItem::CS_Fail);
            LoadingNotification->SetText(FText::FromString(TEXT("MetaHuman Animator: No animation data generated")));
            LoadingNotification->ExpireAndFadeout();
        }
        return;
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
    ExportSettings->AssetName = FString::Printf(TEXT("FacialAnim_%s"), *BaseName);

    if (VisMesh)
    {
        ExportSettings->TargetSkeletonOrSkeletalMesh = VisMesh;
    }

    UAnimSequence* ExportedSequence = UMetaHumanPerformanceExportUtils::ExportAnimationSequence(Performance, ExportSettings);

    if (ExportedSequence)
    {
        DialogueNode->SetFacialAnimation(nullptr);

        const FString MontageName = FString::Printf(TEXT("FacialAnim_%s_Montage"), *BaseName);
        const FString MontagePackagePath = OutputPath + MontageName;
        UPackage* MontagePackage = CreatePackage(*MontagePackagePath);
        MontagePackage->FullyLoad();

        UAnimMontage* Montage = NewObject<UAnimMontage>(
            MontagePackage, FName(*MontageName), RF_Public | RF_Standalone);
        Montage->SetSkeleton(ExportedSequence->GetSkeleton());

        Montage->SlotAnimTracks.Empty();
        FSlotAnimationTrack& SlotTrack = Montage->SlotAnimTracks.AddDefaulted_GetRef();
        SlotTrack.SlotName = FName("DefaultSlot");

        FAnimSegment& Segment = SlotTrack.AnimTrack.AnimSegments.AddDefaulted_GetRef();
        Segment.SetAnimReference(ExportedSequence, true);
        Segment.AnimStartTime = 0.0f;
        Segment.AnimEndTime = ExportedSequence->GetPlayLength();
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

        DialogueNode->SetFacialAnimation(Montage);

        if (LoadingNotification.IsValid())
        {
            LoadingNotification->SetCompletionState(SNotificationItem::CS_Success);
            LoadingNotification->SetText(FText::FromString(FString::Printf(TEXT("Facial Animation Generated: %s"), *MontageName)));
            LoadingNotification->ExpireAndFadeout();
        }
        UE_LOG(LogTemp, Log, TEXT("MetaHuman Animator: Generated facial animation %s"), *MontageName);
    }
    else
    {
        if (LoadingNotification.IsValid())
        {
            LoadingNotification->SetCompletionState(SNotificationItem::CS_Fail);
            LoadingNotification->SetText(FText::FromString(TEXT("MetaHuman Animator: Export failed")));
            LoadingNotification->ExpireAndFadeout();
        }
    }
}
#else
void UADSEditorSubsystem::GenerateFacialAnimation(UADSGraphNode* DialogueNode)
{
    ShowNotification(TEXT("Facial animation generation requires the MetaHuman plugin, which is only available on Windows/Linux."), false);
}
#endif // WITH_ADS_METAHUMAN

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

    /** Connect output pin of FromNode to input pin of ToNode */
    static void ConnectEditorNodes(UEdNode_AGSGraphNode* FromNode, UEdNode_AGSGraphNode* ToNode)
    {
        if (FromNode && ToNode && FromNode->Pins.Num() >= 2 && ToNode->Pins.Num() >= 2)
        {
            UEdGraphPin* OutputPin = FromNode->Pins[1]; // Output
            UEdGraphPin* InputPin = ToNode->Pins[0];    // Input
            OutputPin->MakeLinkTo(InputPin);
        }
    }
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
    // 2. Read and parse the screenplay file
    // ----------------------------------------------------------------
    FString FileContent;
    if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
    {
        ShowNotification(FString::Printf(TEXT("Failed to read file: %s"), *FilePath), false);
        return;
    }

    TArray<ADSImportHelper::FParsedDialogueEntry> ParsedEntries = ADSImportHelper::ParseScreenplay(FileContent);

    if (ParsedEntries.Num() == 0)
    {
        ShowNotification(TEXT("No dialogue entries found in the file. Check the screenplay format."), false);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Parsed %d dialogue entries from: %s"), ParsedEntries.Num(), *FilePath);

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
    if (bUseUpdateMode)
    {
        const int32 NewEntries = ParsedEntries.Num() - MatchedGuidCount;
        FString ConfirmMsg = FString::Printf(
            TEXT("Update Mode\n\n"
                 "Found %d entries matching existing nodes (will update text and participant).\n"
                 "%d entries have no matching node (will be skipped).\n\n"
                 "File: %s\n\nContinue?"),
            MatchedGuidCount, NewEntries, *FPaths::GetCleanFilename(FilePath));

        if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(ConfirmMsg)) != EAppReturnType::Yes)
        {
            return;
        }
    }
    else
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
                 "File: %s\n\nContinue?"),
            RegularEntries, OptionEntries, *FPaths::GetCleanFilename(FilePath));

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
            RuntimeNode->GenerateNewID();
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
            RegularCount, OptionCount, *FPaths::GetCleanFilename(FilePath));
        ShowNotification(ResultMsg, true);
        UE_LOG(LogTemp, Log, TEXT("%s"), *ResultMsg);
    }

#endif
}
