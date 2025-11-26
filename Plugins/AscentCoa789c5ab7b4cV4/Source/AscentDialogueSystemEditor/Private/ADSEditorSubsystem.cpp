// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2025. All Rights Reserved.

#include "ADSEditorSubsystem.h"

#include "ADSDialogueDeveloperSettings.h"
#include "ADSDialogueFunctionLibrary.h"
#include "ADSDialoguePartecipantComponent.h"
#include "ADSDialogueSubsystem.h"
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
            if (ParticipantName == TEXT("None")) {
                ParticipantName = TEXT("Dialogue");
            }
            const FString AssetName = FString::Printf(TEXT("TTS_%s_%s"), *ParticipantName, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
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
