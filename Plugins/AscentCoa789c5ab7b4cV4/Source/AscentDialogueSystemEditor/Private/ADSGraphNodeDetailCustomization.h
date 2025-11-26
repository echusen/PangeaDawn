#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class UADSGraphNode;

/**
 * Custom detail panel for ADSGraphNode to add  TTS buttons
 */
class FADSGraphNodeDetailCustomization : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    TWeakObjectPtr<UADSGraphNode> CachedNode;
};


class FADSAIVoiceGeneratorComponentDetails : public IDetailCustomization
{
public:
    /** Factory method */
    static TSharedRef<IDetailCustomization> MakeInstance();

    /** Called to customize the details panel */
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    /** Cached component being customized */
    TWeakObjectPtr<class UADSVoiceConfigDataAsset> CachedComponent;
};