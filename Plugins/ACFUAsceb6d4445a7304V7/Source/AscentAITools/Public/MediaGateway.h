// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Fired once for every credential Set or Clear that changes the stored values. */
DECLARE_MULTICAST_DELEGATE(FACFMediaGatewayChangedDelegate);

/**
 * The Yarin sign-in credential the 3D backends need to bill generation to the user's Yarin credit.
 *
 * AscentACFAssistant owns the sign-in and fills this holder at the existing credential events (a minted or
 * refreshed gateway key sets it, sign-out, forget and an expired sign-in clear it). The 3D clients in this
 * module read it through ACF3D::ResolveEndpoints. The holder knows nothing about the assistant module, so
 * AscentAITools keeps no dependency on it.
 *
 * A gateway URL or key that could not survive a URL or an Authorization header is refused: printable ASCII
 * only, capped length, and https unless a loopback address. The rules mirror the Yarin assistant's own checks
 * for the same values.
 */
class ASCENTAITOOLS_API FACFMediaGateway
{
public:
	/** The gateway base URL (for example https://yarin.example.com/litellm/v1) and the account's virtual key. */
	struct FCredential
	{
		FString BaseUrl;
		FString Key;
	};

	/** Stores a credential. Returns false (and stores nothing) when either value is unusable. */
	static bool Set(const FString& GatewayBaseUrl, const FString& GatewayKey);

	/** Forgets the credential. The 3D backends then behave as when signed out of Yarin. */
	static void Clear();

	/** True when a usable credential is stored. */
	static bool IsPresent();

	/** A copy of the stored credential, empty when none is present. */
	static FCredential GetCredential();

	/** The origin of the gateway base URL plus /media3d, where provider traffic billed to Yarin goes. Empty when not present. */
	static FString GetMediaRoot();

	// The same shape rules the Yarin assistant applies to its gateway URL and key.
	static bool IsUsableGatewayUrl(const FString& Url);
	static bool IsUsableGatewayKey(const FString& Key);

	/**
	 * Fired on the thread that changed the credential, after the new value is already readable, so a
	 * subscriber can react to sign in and sign out without polling. Only changes broadcast: a Set with the
	 * values already stored and a Clear with nothing stored stay silent.
	 */
	static FACFMediaGatewayChangedDelegate& OnChanged();
};
