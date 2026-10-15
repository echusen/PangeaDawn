// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Starts or stops the Yarin phone page from Project Settings. Safe to call repeatedly.
void SyncACFAssistantPhoneAccess();
// Also clears the tunnel request. Called by Close tunnel and on Editor exit.
void StopACFAssistantPhoneAccess();
// The tunnel lives from Open tunnel until Close tunnel or Editor exit, independent of the QR window.
void OpenACFAssistantPhoneTunnel();
void CloseACFAssistantPhoneTunnel();
bool IsACFAssistantPhoneTunnelOpen();
bool IsACFAssistantPhoneAccessListening();
// Terminates tunnel helpers (cloudflared, dumbpipe) left behind by a previous Editor session (crash, kill). Returns how many were closed.
int32 CleanupACFAssistantPhoneTunnels();
FString GetACFAssistantPhoneUrl();
void CopyACFAssistantPhoneUrl();
// Registers Yarin.ShowPhoneUrl and Yarin.ForgetPhone at module startup, so a lost phone can be revoked before any tunnel is opened.
void RegisterACFAssistantPhoneCommands();
void UnregisterACFAssistantPhoneCommands();
// Replaces this project's Iroh link and token. The phone that scanned the old QR code stops working until it scans the new one.
void ForgetACFAssistantPhonePairing();
// Opens the tunnel when "Open Phone Tunnel At Editor Startup" is on and Iroh is the transport. No-op otherwise.
void OpenACFAssistantPhoneTunnelAtStartup();
void ShowACFAssistantPhoneWindow();
