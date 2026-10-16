#pragma once

#include "ANSNavPageWidget.h"
#include "ANSUIPlayerSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

// Module-private adapter: existing feature data widgets are content, while ANS
// owns the page, Back handling, input requests and stack lifetime.
namespace MiningScreenHost
{
    inline void Refresh(UUserWidget* Content, AActor* Actor, const TCHAR* FunctionName)
    {
        UANSNavPageWidget* Page = Content ? Content->GetTypedOuter<UANSNavPageWidget>() : nullptr;
        if (!Page || !Page->IsActivated()) return;
        if (UFunction* Function = Content->FindFunction(FunctionName))
        {
            struct FContext { AActor* Actor; APawn* Pawn; } Context{ Actor, Content->GetOwningPlayerPawn() };
            Content->ProcessEvent(Function, &Context);
        }
    }

    inline void Close(UUserWidget* Content)
    {
        if (Content)
        {
            if (UANSNavPageWidget* Page = Content->GetTypedOuter<UANSNavPageWidget>()) Page->RemoveSelf();
            else Content->RemoveFromParent();
        }
    }

    inline UUserWidget* Open(APlayerController* PC, const TCHAR* Tag)
    {
        UANSUIPlayerSubsystem* UI = PC && PC->IsLocalController() && PC->GetGameInstance()
            ? PC->GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>() : nullptr;
        UANSNavPageWidget* Page = UI ? Cast<UANSNavPageWidget>(UI->SpawnWidgetByTag(FGameplayTag::RequestGameplayTag(Tag))) : nullptr;
        if (!Page) return nullptr;
        UUserWidget* Content = Cast<UUserWidget>(Page->GetWidgetFromName(TEXT("ScreenContent")));
        if (!Content)
        {
            UE_LOG(LogTemp, Error, TEXT("Pangea mining page %s has no ScreenContent widget"), *GetNameSafe(Page));
            Page->RemoveSelf();
        }
        return Content;
    }
}
