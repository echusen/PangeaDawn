// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#include "AIToolsNotification.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

void FACFAIToolsNotification::Start(const FText& InMessage)
{
	if (NotificationItem.IsValid())
	{
		NotificationItem->ExpireAndFadeout();
		NotificationItem.Reset();
	}

	FNotificationInfo Info(InMessage);
	Info.bFireAndForget = false;
	Info.bUseThrobber = true;
	Info.bUseSuccessFailIcons = true;
	Info.FadeOutDuration = 0.5f;
	Info.ExpireDuration = 0.0f;

	NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
	if (NotificationItem.IsValid())
	{
		NotificationItem->SetCompletionState(SNotificationItem::CS_Pending);
	}
}

void FACFAIToolsNotification::Update(const FText& InMessage)
{
	if (NotificationItem.IsValid())
	{
		NotificationItem->SetText(InMessage);
	}
}

void FACFAIToolsNotification::Finish(const FText& InMessage, bool bSuccess)
{
	if (!NotificationItem.IsValid())
	{
		return;
	}

	NotificationItem->SetText(InMessage);
	NotificationItem->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	NotificationItem->SetExpireDuration(bSuccess ? 4.0f : 8.0f);
	NotificationItem->SetFadeOutDuration(0.5f);
	NotificationItem->ExpireAndFadeout();
	NotificationItem.Reset();
}
