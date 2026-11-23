// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SNotificationItem;

/**
 * Small helper that wraps an editor toast notification (bottom-right of the editor)
 * showing a spinning throbber while an AI generation task is running, and turning
 * into a success/failure message when it completes.
 */
class ASCENTAITOOLS_API FACFAIToolsNotification
{
public:
	/** Starts a pending (throbber) notification in the bottom-right corner. */
	void Start(const FText& InMessage);

	/** Updates the running text (e.g. with progress percentage). */
	void Update(const FText& InMessage);

	/** Closes the notification with a success state. */
	void Finish(const FText& InMessage, bool bSuccess);

	bool IsActive() const { return NotificationItem.IsValid(); }

private:
	TSharedPtr<SNotificationItem> NotificationItem;
};
