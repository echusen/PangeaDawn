// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

class SMultiLineEditableTextBox;
class SEditableTextBox;
class SButton;
class UTexture2D;
class UObject;
struct FSlateBrush;
class UACFAIToolsSettings;
class UACFAIToolsSubsystem;

/** Severity of the inline status message shown in the window. */
enum class EACFAIToolsStatusLevel : uint8
{
	Info,
	Success,
	Error
};

/**
 * Editor panel that drives AI generation: prompt entry, provider/options selection,
 * a live preview of the last generated image, and buttons to generate images and
 * 3D meshes. Inline status text reports every error; long-running work also shows a
 * bottom-right throbber notification.
 */
class SACFAIToolsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFAIToolsWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SACFAIToolsWindow() override;

private:
	FString GetPromptText() const;
	FString GetFileName() const;

	UACFAIToolsSubsystem* GetSubsystem() const;

	FReply OnGenerateImageClicked();
	FReply OnTextToMeshClicked();
	FReply OnImageToMeshClicked();
	FReply OnOpenSettingsClicked();

	bool IsImageIdle() const;
	bool IsMeshIdle() const;
	bool HasGeneratedImage() const;

	// Subsystem event handlers.
	void HandleStatus(const FText& Status);
	void HandleImageGenerated(UTexture2D* Texture, bool bSuccess, const FString& Error);
	void HandleMeshGenerated(UObject* Mesh, bool bSuccess, const FString& Error);

	void SetPreviewTexture(UTexture2D* Texture);

	// --- Inline status / error message ---
	void SetStatus(const FText& Message, EACFAIToolsStatusLevel Level);
	FText GetStatusText() const { return StatusMessage; }
	FSlateColor GetStatusColor() const;
	EVisibility GetStatusVisibility() const;

	// --- Settings-backed options ---
	static UACFAIToolsSettings* MutableSettings();
	void SaveSettings();

	// Image provider selector
	TSharedRef<class SWidget> MakeComboItem(TSharedPtr<FString> Item);
	void OnImageProviderChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetImageProviderText() const;

	// 3D provider selector and capability gates for the active backend
	TArray<TSharedPtr<FString>> ThreeDProviderOptions;
	void OnThreeDProviderChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetThreeDProviderText() const;
	FText GetMeshOptionsTitle() const;
	bool IsTextToMeshEnabled() const;
	bool IsImageToMeshEnabled() const;

	ECheckBoxState IsGenerateTextures() const;
	void OnGenerateTexturesChanged(ECheckBoxState NewState);
	bool AreTexturesEnabled() const;

	ECheckBoxState IsEnablePBR() const;
	void OnEnablePBRChanged(ECheckBoxState NewState);

	ECheckBoxState IsAutoRefine() const;
	void OnAutoRefineChanged(ECheckBoxState NewState);

	ECheckBoxState IsRigHumanoid() const;
	void OnRigHumanoidChanged(ECheckBoxState NewState);

	TOptional<int32> GetTargetPolycount() const;
	void OnTargetPolycountChanged(int32 NewValue);

	void OnArtStyleChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetArtStyleText() const;
	void OnTopologyChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	FText GetTopologyText() const;

	TSharedPtr<SMultiLineEditableTextBox> PromptBox;
	TSharedPtr<SEditableTextBox> FileNameBox;

	TArray<TSharedPtr<FString>> ImageProviderOptions;
	TArray<TSharedPtr<FString>> ArtStyleOptions;
	TArray<TSharedPtr<FString>> TopologyOptions;

	/** Brush wrapping the last imported preview texture. */
	TSharedPtr<FSlateBrush> PreviewBrush;
	TWeakObjectPtr<UTexture2D> PreviewTexture;

	FText StatusMessage;
	EACFAIToolsStatusLevel StatusLevel = EACFAIToolsStatusLevel::Info;

	// Delegate handles for the subsystem native events.
	FDelegateHandle StatusHandle;
	FDelegateHandle ImageHandle;
	FDelegateHandle MeshHandle;
};
