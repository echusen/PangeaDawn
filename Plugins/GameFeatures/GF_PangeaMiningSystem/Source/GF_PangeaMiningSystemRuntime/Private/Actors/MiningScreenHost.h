#pragma once

#include "CoreMinimal.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"

#include "ANSNavPageWidget.h"
#include "ANSUIPlayerSubsystem.h"

// Module-private adapter: existing feature data widgets are content, while ANS
// owns the page, Back handling, input requests and stack lifetime.
namespace MiningScreenHost
{
	/**
	 * Calls the named function on Content with (Actor, owning player pawn) so the widget re-reads its data.
	 * Does nothing unless Content sits inside an activated ANS page.
	 */
inline void Refresh(UUserWidget* Content, AActor* Actor, const TCHAR* FunctionName)
{
	UANSNavPageWidget* const Page = Content != nullptr ? Content->GetTypedOuter<UANSNavPageWidget>() : nullptr;
	if (Page == nullptr || !Page->IsActivated())
	{
		return;
	}

	if (UFunction* Function = Content->FindFunction(FunctionName))
	{
			// Parameter block for ProcessEvent: the layout must match the widget function's (Actor, Pawn) parameters.
		struct FContext
		{
			AActor* Actor;
			APawn* Pawn;
		};

		FContext Context{ Actor, Content->GetOwningPlayerPawn() };
		Content->ProcessEvent(Function, &Context);
	}
}

	/** Removes the ANS page that hosts Content. When Content has no hosting page, removes Content from its parent. */
inline void Close(UUserWidget* Content)
{
	if (Content != nullptr)
	{
		if (UANSNavPageWidget* Page = Content->GetTypedOuter<UANSNavPageWidget>())
		{
			Page->RemoveSelf();
		}
		else
		{
			Content->RemoveFromParent();
		}
	}
}

	/**
	 * Spawns the ANS page registered under Tag for a local player controller and returns its "ScreenContent" widget.
	 * Returns null when the controller is not local, the page cannot be spawned, or the page has no ScreenContent
	 * widget. In the last case the page is removed again.
	 */
inline UUserWidget* Open(APlayerController* PC, const TCHAR* Tag)
{
	UANSUIPlayerSubsystem* const UI =
		PC != nullptr && PC->IsLocalController() && PC->GetGameInstance() != nullptr ? PC->GetGameInstance()->GetSubsystem<UANSUIPlayerSubsystem>() : nullptr;
	UANSNavPageWidget* const Page = UI != nullptr ? Cast<UANSNavPageWidget>(UI->SpawnWidgetByTag(FGameplayTag::RequestGameplayTag(Tag))) : nullptr;
	if (Page == nullptr)
	{
		return nullptr;
	}

	UUserWidget* const Content = Cast<UUserWidget>(Page->GetWidgetFromName(TEXT("ScreenContent")));
	if (Content == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("Pangea mining page %s has no ScreenContent widget"), *GetNameSafe(Page));
		Page->RemoveSelf();
	}

	return Content;
}
} // namespace MiningScreenHost
