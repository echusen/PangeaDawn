// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantPhoneAccess.h"
#include "ACFAssistantClientInternal.h"
#include "ACFAssistantMesh.h"
#include "ACFAssistantOwnPlugin.h"
#include "ACFAssistantSettings.h"
#include "AscentACFAssistant.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Http.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Char.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "ThirdParty/qrcodegen.h"
#include "Serialization/JsonSerializer.h"
#if PLATFORM_MAC || PLATFORM_LINUX
#include <sys/stat.h>
#endif
#include "Serialization/JsonWriter.h"
#include "Misc/Guid.h"
#include "Dom/JsonObject.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include <bcrypt.h>
#include <dpapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	constexpr int32 MaxConnections = 8;
	constexpr int32 MaxRequestBytes = 64 * 1024;
	constexpr int32 MaxResponseBytes = 16 * 1024 * 1024;
	constexpr int32 MaxSendBytesPerTick = 64 * 1024;
	constexpr double ConnectionTimeoutSeconds = 8.0;

	struct FPhoneConnection
	{
		FSocket* Socket = nullptr;
		TArray<uint8> Buffer;
		TArray<uint8> Response;
		int32 SendOffset = 0;
		bool bRequestHandled = false;
		bool bResponseOverflow = false;
		double AcceptedAt = 0.0;
	};

	FSocket* ListenSocket = nullptr;
	TArray<FPhoneConnection> Connections;
	FTSTicker::FDelegateHandle Ticker;
	IConsoleObject* ShowUrlCommand = nullptr;
	IConsoleObject* ForgetPhoneCommand = nullptr;
	FString AccessToken;
	FString PublicHttpsOrigin;
	int32 BoundPort = 0;
	FProcHandle TunnelProcess;
	// Windows job object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE. The tunnel process lives inside it, so the kernel
	// terminates cloudflared the moment the Editor process ends for any reason, including crashes and task-manager kills.
	void* TunnelJob = nullptr;
	void* TunnelReadPipe = nullptr;
	void* TunnelWritePipe = nullptr;
	FString TunnelLog;
	TSharedPtr<SWindow> PhoneWindow;
	// Set by Open tunnel, cleared by Close tunnel and Editor exit. The QR window only shows it; closing the window leaves it running.
	bool bTunnelRequested = false;
	bool bTunnelDownloadInFlight = false;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> TunnelDownload;
	// Which helper the running tunnel was started with. Changing the transport in Project Settings restarts it.
	EACFAssistantPhoneTransport ActiveTransport = EACFAssistantPhoneTransport::Cloudflare;
	// Iroh only: the short ticket dumbpipe prints once its endpoint is online. Empty until then.
	FString IrohTicket;
	// Iroh only: this project's dumbpipe endpoint secret (64 hex chars), handed to dumbpipe through IROH_SECRET so the Iroh
	// address is the same after every Editor restart. Never logged, never printed.
	FString IrohSecretHex;
	// Iroh only: Windows tar.exe unpacking the dumbpipe zip. Asynchronous so the Editor never blocks on it.
	FProcHandle ExtractProcess;
	FString ExtractTempDir;

	void DestroySocket(FSocket*& Socket)
	{
		if (!Socket) { return; }
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		Socket->Close();
		if (Sockets) { Sockets->DestroySocket(Socket); }
		Socket = nullptr;
	}

	FString NewAccessToken()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
	}

	FString RandomHex(int32 NumBytes)
	{
#if PLATFORM_WINDOWS
		TArray<uint8> Bytes;
		Bytes.SetNumZeroed(NumBytes);
		if (::BCryptGenRandom(nullptr, Bytes.GetData(), static_cast<ULONG>(NumBytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0)
		{
			return BytesToHex(Bytes.GetData(), NumBytes).ToLower();
		}
#endif
		return FString();
	}

	bool IsLowerHex(const FString& Text, int32 Length)
	{
		if (Text.Len() != Length) { return false; }
		for (const TCHAR Char : Text)
		{
			if (!((Char >= TEXT('0') && Char <= TEXT('9')) || (Char >= TEXT('a') && Char <= TEXT('f')))) { return false; }
		}
		return true;
	}

	// The Iroh pairing (this project's Iroh identity and access token) lives with the project, so the same project keeps the same
	// link across Editor restarts and a phone that scanned it once reconnects by itself.
	FString PairingFilePath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Yarin/PhonePairing.dat"));
	}

	// The pairing is a bearer credential: whoever holds this PC's Iroh id and the token can drive the Editor. At rest it is sealed
	// with Windows DPAPI, so a copied or shared project folder carries nothing a different Windows user or PC can open.
	bool SealPairing(const FString& Plain, TArray<uint8>& OutSealed)
	{
#if PLATFORM_WINDOWS
		const FTCHARToUTF8 Utf8(*Plain);
		DATA_BLOB In;
		In.cbData = static_cast<DWORD>(Utf8.Length());
		In.pbData = reinterpret_cast<BYTE*>(const_cast<ANSICHAR*>(Utf8.Get()));
		DATA_BLOB Out = {};
		if (!::CryptProtectData(&In, L"Yarin phone pairing", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
		{
			return false;
		}
		OutSealed.Append(Out.pbData, static_cast<int32>(Out.cbData));
		::LocalFree(Out.pbData);
		return true;
#else
		(void)Plain;
		(void)OutSealed;
		return false;
#endif
	}

	bool UnsealPairing(const TArray<uint8>& Sealed, FString& OutPlain)
	{
#if PLATFORM_WINDOWS
		DATA_BLOB In;
		In.cbData = static_cast<DWORD>(Sealed.Num());
		In.pbData = const_cast<BYTE*>(Sealed.GetData());
		DATA_BLOB Out = {};
		if (!::CryptUnprotectData(&In, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &Out))
		{
			return false;
		}
		const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Out.pbData), static_cast<int32>(Out.cbData));
		OutPlain = FString(Text.Length(), Text.Get());
		::SecureZeroMemory(Out.pbData, Out.cbData);
		::LocalFree(Out.pbData);
		return true;
#else
		(void)Sealed;
		(void)OutPlain;
		return false;
#endif
	}

	bool LoadIrohPairing(FString& OutSecret, FString& OutToken)
	{
		TArray<uint8> Sealed;
		if (!FFileHelper::LoadFileToArray(Sealed, *PairingFilePath(), FILEREAD_Silent) || Sealed.IsEmpty()) { return false; }
		FString Plain;
		if (!UnsealPairing(Sealed, Plain)) { return false; }
		TArray<FString> Lines;
		Plain.ParseIntoArrayLines(Lines);
		if (Lines.Num() != 2 || !IsLowerHex(Lines[0], 64) || !IsLowerHex(Lines[1], 32)) { return false; }
		OutSecret = Lines[0];
		OutToken = Lines[1];
		return true;
	}

	bool SaveIrohPairing(const FString& Secret, const FString& Token)
	{
		TArray<uint8> Sealed;
		if (!SealPairing(Secret + TEXT("\n") + Token, Sealed)) { return false; }
		const FString Path = PairingFilePath();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveArrayToFile(Sealed, *Path);
	}

	// Replaces the stored pairing so the old one can no longer be loaded. It overwrites rather than deletes: a delete can fail (a
	// scanner or sync client holding the file), and a surviving file would be reloaded as the very pairing being revoked.
	bool RevokeIrohPairing()
	{
		const FString Secret = RandomHex(32);
		const FString Token = RandomHex(16);
		if (!Secret.IsEmpty() && !Token.IsEmpty() && SaveIrohPairing(Secret, Token)) { return true; }
		IFileManager::Get().Delete(*PairingFilePath(), false, true, true);
		return !FPaths::FileExists(PairingFilePath());
	}

	// Sets AccessToken (and IrohSecretHex) for ActiveTransport. A Cloudflare link changes every time, so its token is fresh every
	// time. Iroh keeps one identity and one token per project: the QR code is scanned once and stays valid until Forget Paired Phone.
	void IssueAccessCredentials()
	{
		IrohSecretHex.Empty();
		if (ActiveTransport != EACFAssistantPhoneTransport::Iroh)
		{
			AccessToken = NewAccessToken();
			return;
		}
		FString Secret;
		FString Token;
		if (LoadIrohPairing(Secret, Token))
		{
			IrohSecretHex = Secret;
			AccessToken = Token;
			return;
		}
		Secret = RandomHex(32);
		Token = RandomHex(16);
		if (Secret.IsEmpty() || Token.IsEmpty())
		{
			// No system random source: do not invent a weaker identity. Fall back to a link that lasts one session.
			AccessToken = NewAccessToken();
			UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone pairing: no system random source; this Iroh link works for this session only."));
			return;
		}
		IrohSecretHex = Secret;
		AccessToken = Token;
		if (SaveIrohPairing(Secret, Token))
		{
			UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin phone pairing: created a new Iroh pairing for this project."));
		}
		else
		{
			UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone pairing: could not save the Iroh pairing; this link works for this session only."));
		}
	}

	bool IsLoopbackPeer(const FString& Ip)
	{
		return Ip.StartsWith(TEXT("127.")) || Ip == TEXT("::1") || Ip.Contains(TEXT("127.0.0.1"));
	}

	FString PeerIPv4(FSocket& Socket)
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets) { return FString(); }
		TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
		Socket.GetPeerAddress(*Address);
		return Address->ToString(false);
	}

	FString HeaderValue(const TMap<FString, FString>& Headers, const TCHAR* Name)
	{
		if (const FString* Value = Headers.Find(FString(Name).ToLower())) { return *Value; }
		return FString();
	}

	FString QueryValue(const FString& Query, const TCHAR* Name)
	{
		TArray<FString> Pairs;
		Query.ParseIntoArray(Pairs, TEXT("&"), true);
		for (const FString& Pair : Pairs)
		{
			FString Key;
			FString Value;
				if (Pair.Split(TEXT("="), &Key, &Value) && Key.Equals(Name, ESearchCase::IgnoreCase))
			{
				return Value;
			}
		}
		return FString();
	}

	void QueueBytes(FSocket& Socket, const uint8* Bytes, int32 Count)
	{
		FPhoneConnection* Connection = Connections.FindByPredicate([&Socket](const FPhoneConnection& Row) { return Row.Socket == &Socket; });
		if (!Connection || Connection->bResponseOverflow) { return; }
		if (Count > MaxResponseBytes - Connection->Response.Num())
		{
			Connection->bResponseOverflow = true;
			Connection->Response.Reset();
			const FTCHARToUTF8 Error(TEXT("HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n"));
			Connection->Response.Append(reinterpret_cast<const uint8*>(Error.Get()), Error.Length());
			return;
		}
		Connection->Response.Append(Bytes, Count);
	}

	void SendBytes(FSocket& Socket, const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		QueueBytes(Socket, reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	void SendResponse(FSocket& Socket, int32 Status, const TCHAR* StatusText, const TCHAR* ContentType, const FString& Body)
	{
		const FTCHARToUTF8 Utf8(*Body);
		SendBytes(Socket, FString::Printf(
			TEXT("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n"),
			Status, StatusText, ContentType, Utf8.Length()));
		QueueBytes(Socket, reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	void SendRaw(FSocket& Socket, int32 Status, const TCHAR* StatusText, const TCHAR* ContentType, const TArray<uint8>& Body, bool bAllowCache = false)
	{
		SendBytes(Socket, FString::Printf(
			TEXT("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\nCache-Control: %s\r\n\r\n"),
			Status, StatusText, ContentType, Body.Num(), bAllowCache ? TEXT("private, max-age=86400") : TEXT("no-store")));
		QueueBytes(Socket, Body.GetData(), Body.Num());
	}

	FString PhoneAppLink()
	{
		const FString Page = GetACFAssistantPhoneUrl();
		if (Page.IsEmpty()) { return FString(); }
		return TEXT("yarin://open?u=") + FGenericPlatformHttp::UrlEncode(Page);
	}

	void SendJson(FSocket& Socket, int32 Status, const TCHAR* StatusText, const TSharedRef<FJsonObject>& Object)
	{
		FString Body;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
		FJsonSerializer::Serialize(Object, Writer);
		SendResponse(Socket, Status, StatusText, TEXT("application/json; charset=utf-8"), Body);
	}

	void SendError(FSocket& Socket, int32 Status, const TCHAR* StatusText, const FString& Message)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("ok"), false);
		Object->SetStringField(TEXT("error"), Message);
		SendJson(Socket, Status, StatusText, Object);
	}

	FString PhonePageHtml()
	{
		static const FString MarkdownLibraries = []()
		{
			const TSharedPtr<IPlugin> Plugin = ACFAssistantPlugin::Owner();
			if (!Plugin) { return FString(); }
			FString Scripts;
			Scripts.Reserve(80 * 1024);
			for (const TCHAR* Name : { TEXT("marked.min.js"), TEXT("purify.min.js"), TEXT("markdown.js") })
			{
				FString Library;
				if (!FFileHelper::LoadFileToString(Library, *(Plugin->GetBaseDir() / TEXT("Resources/Phone") / Name)))
				{
					return FString();
				}
				Scripts += Library;
				Scripts += TEXT("\n");
			}
			return Scripts;
		}();
		if (MarkdownLibraries.IsEmpty()) { return FString(); }
		return FString()
			+ TEXT("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">")
			+ TEXT("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">")
			+ TEXT("<meta name=\"apple-mobile-web-app-capable\" content=\"yes\"><title>Yarin</title><style>")
			+ TEXT(":root{color-scheme:dark}body{margin:0;font:16px/1.45 system-ui;background:#111218;color:#f2f2f5;")
			+ TEXT("display:flex;flex-direction:column;height:100dvh}header{display:flex;align-items:center;gap:8px;padding:12px 12px 8px;background:#111218;")
			+ TEXT("border-bottom:1px solid #2a2b33}#menu{border:0;background:transparent;color:#f2f2f5;font-size:22px;padding:8px 10px;flex:0 0 auto}")
			+ TEXT("#mark{height:52px;width:auto;max-width:72%;object-fit:contain;object-position:left center}")
			+ TEXT("#title{padding:2px 16px 0;font-size:13px;font-weight:600;color:#9aa0aa;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}")
			+ TEXT("#scrim{position:fixed;inset:0;background:rgba(0,0,0,.5);display:none;z-index:2}")
			+ TEXT("#scrim.open{display:block}#drawer{position:fixed;top:0;left:0;bottom:0;width:min(320px,86vw);background:#1a1b22;")
			+ TEXT("z-index:3;transform:translateX(-105%);transition:transform .18s ease;display:flex;flex-direction:column}")
			+ TEXT("#drawer.open{transform:none}#drawer header{border:0;justify-content:flex-start}")
			+ TEXT("#drawer img{height:auto;width:calc(100% - 28px);margin:4px 14px 8px;object-fit:contain}")
			+ TEXT("#list{overflow:auto;padding:8px 0 24px}.navbtn{display:block;width:100%;text-align:left;border:0;background:transparent;")
			+ TEXT("color:#f2f2f5;padding:14px 18px;font:16px system-ui}.navbtn.on{background:#2a2b33}.navsub{font-size:12px;color:#9aa0aa;margin-top:3px}")
			+ TEXT(".navlabel{padding:16px 18px 4px;color:#9aa0aa;font-size:12px;letter-spacing:.04em;text-transform:uppercase}")
			+ TEXT("#status{padding:8px 16px;font-size:13px;color:#9aa0aa}#log{flex:1;overflow:auto;padding:8px 16px}")
			+ TEXT(".msg{color:#f2f2f5;margin:0 0 14px;padding:12px 14px;border-radius:8px;white-space:pre-wrap;word-break:break-word}")
			+ TEXT(".who{color:#b6b09a;font-size:13px;font-weight:600;margin:0 0 6px}")
			+ TEXT("img.shot{display:block;max-width:100%;height:auto;border-radius:10px;margin:0 0 12px}")
			+ TEXT("#controls{display:flex;gap:8px;padding:8px 16px 0}#controls select{flex:1;min-width:0;background:#2a2b33;color:#f2f2f5;border:0;border-radius:12px;padding:10px}")
			+ TEXT("#pending{display:flex;gap:8px;overflow:auto;padding:8px 16px 0}#pending img{height:72px;border-radius:8px}")
			+ TEXT(".user{background:#2a3852}.assistant{background:#22232a}.msg.system{color:#b6b09a;font-size:14px;font-weight:600;padding:0 4px}")
			+ TEXT("#status:before{content:'';display:inline-block;width:10px;height:10px;border-radius:50%;background:#6e6e72;margin-right:8px;vertical-align:-1px}")
			+ TEXT("#status.ok:before{background:#4caf7a}#status.warn:before{background:#d9b44a}#status.busy:before{background:#4caf7a;animation:pulse 1.2s ease-in-out infinite}")
			+ TEXT(".tick i{width:8px;height:8px;border-radius:50%;background:#6e6e72;flex:none}@keyframes pulse{0%,100%{opacity:.35}50%{opacity:1}}")
			+ TEXT(".tick{display:flex;justify-content:flex-end;align-items:center;gap:6px;margin-top:6px;font-size:11px;color:#9aa0aa}")
			+ TEXT(".sending .tick i{animation:pulse 1s ease-in-out infinite}.sent .tick i{background:#4caf7a}")
			+ TEXT(".failed{cursor:pointer}.failed .tick{color:#c06a47}.failed .tick i{background:#c06a47}")
			+ TEXT("details.work{margin:0 0 6px;color:#9aa0aa;font-size:12px;line-height:1.25}")
			+ TEXT("details.work summary{cursor:pointer;padding:2px 0;list-style:none}")
			+ TEXT("details.work summary::-webkit-details-marker{display:none}")
			+ TEXT("details.work summary:before{content:'\\25B8  '}")
			+ TEXT("details.work[open] summary:before{content:'\\25BE  '}")
			+ TEXT(".workbody{max-height:7.5em;overflow:auto;white-space:pre-wrap;padding:0 0 2px 14px;margin:0}")
			+ TEXT(".tool{color:#9aa0aa;font-size:13px}#actions{display:flex;flex-wrap:wrap;gap:8px;padding:0 16px 8px}")
			+ TEXT("form{display:flex;gap:8px;padding:12px 16px calc(12px + env(safe-area-inset-bottom));background:#1a1b22}")
			+ TEXT("textarea{flex:1;min-height:48px;max-height:120px;resize:none;border:0;border-radius:10px;padding:10px;")
			+ TEXT("background:#2a2b33;color:#f2f2f5}#actions button,form button{border:1px solid #a48f52;border-radius:8px;padding:0 14px;background:#c2ad6c;")
			+ TEXT("color:#08080a;font-weight:600}#actions button.ghost{border-color:transparent;background:#2a2b33;color:#f2f2f5}")
			+ TEXT("</style></head><body>")
			+ TEXT("<header><button id=\"menu\" type=\"button\">&#9776;</button><img id=\"mark\" src=\"/logo.png\" alt=\"Yarin\"></header><div id=\"title\"></div>")
			+ TEXT("<div id=\"scrim\"></div><aside id=\"drawer\"><header><img src=\"/logo.png\" alt=\"\"></header><div id=\"list\"></div></aside>")
			+ TEXT("<div id=\"log\"></div><div id=\"status\">Connecting...</div><div id=\"actions\"></div>")
			+ TEXT("<div id=\"controls\"><select id=\"mode\"></select><select id=\"model\"></select></div>")
			+ TEXT("<div id=\"pending\"></div><form id=\"f\"><textarea id=\"t\" placeholder=\"Message...\"></textarea>")
			+ TEXT("<button id=\"send\">Send</button></form><script>")
			+ MarkdownLibraries
			+ TEXT("const key=new URLSearchParams(location.search).get('k')||'';")
			+ TEXT("(function(){const q=new URLSearchParams(location.search);if(q.get('app')==='1'||q.get('web')==='1')return;")
			+ TEXT("const stay=new URL(location.href);stay.searchParams.set('web','1');")
			+ TEXT("location.href='intent://open?u='+encodeURIComponent(location.href)+'#Intent;scheme=yarin;package=com.darktower.yarin;S.browser_fallback_url='+encodeURIComponent(stay.toString())+';end';})();")
			+ TEXT("const api=async(path,opt={})=>{const c=new AbortController();const tm=setTimeout(()=>c.abort(),opt.timeout||8000);let r;")
			+ TEXT("try{r=await fetch(path+(path.includes('?')?'&':'?')+'k='+encodeURIComponent(key),{...opt,signal:c.signal});}finally{clearTimeout(tm);}")
			+ TEXT("if(!r.ok){let message='HTTP '+r.status;try{message=(await r.json()).error||message;}catch(e){}throw new Error(message);}return r;};")
			+ TEXT("const offline=e=>e instanceof TypeError||e.name==='AbortError';")
			+ TEXT("function report(e){document.getElementById('status').textContent=(offline(e)||!e.message)?'Cannot reach the Editor. Keep Unreal running.':e.message;}")
			+ TEXT("const esc=s=>String(s).replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));")
			+ TEXT("function post(path,body){return api(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{})})")
			+ TEXT(".then(r=>r.json()).then(render).catch(e=>{report(e);tick();});}")
			+ TEXT("function drawer(open){document.getElementById('drawer').classList.toggle('open',open);document.getElementById('scrim').classList.toggle('open',open);}")
			+ TEXT("document.getElementById('menu').onclick=()=>drawer(true);document.getElementById('scrim').onclick=()=>drawer(false);")
			+ TEXT("const modes=['Ask','Plan','Ask for approval','Approve for me'];const mode=document.getElementById('mode');")
			+ TEXT("modes.forEach(m=>{const o=document.createElement('option');o.value=m;o.textContent=m;mode.appendChild(o);});")
			+ TEXT("let shownId=0;mode.onchange=()=>{mode.blur();post('/api/workmode',{mode:mode.value,localId:shownId});};const model=document.getElementById('model');")
			+ TEXT("model.onchange=()=>{model.blur();post('/api/model',{id:model.value,localId:shownId});};")
			+ TEXT("function item(label,on,go,sub){const b=document.createElement('button');b.className='navbtn'+(on?' on':'');b.textContent=label;")
			+ TEXT("if(sub){const d=document.createElement('div');d.className='navsub';d.textContent=sub;b.appendChild(d);}b.onclick=()=>{drawer(false);go();};return b;}")
			+ TEXT("const openWork=new Set();")
			+ TEXT("let last=null,busy=false,fails=0,inflight=false,outbox=[],outSeq=0;const isUser=r=>r==='you'||r==='user';")
			+ TEXT("function conn(){const st=document.getElementById('status');")
			+ TEXT("st.className=!key||fails>=3?'':fails>0||!last||!last.ready?'warn':busy?'busy':'ok';")
			+ TEXT("if(fails>0)st.textContent=fails>=3?'Offline. Cannot reach the Editor. Keep Unreal running.':'Reconnecting...';}")
			// Messages typed on the phone show at once and stay until the Editor's own copy appears in the conversation.
			+ TEXT("function reconcile(msgs){const used=new Set();const old=it=>it.status==='sent'&&Date.now()-it.sentAt>120000;")
			+ TEXT("outbox=outbox.filter(it=>{if(it.conv!==shownId)return !old(it);if(it.base>msgs.length)it.base=msgs.length;")
			+ TEXT("for(let i=it.base;i<msgs.length;i++){if(!used.has(i)&&isUser((msgs[i].role||'').toLowerCase())&&(msgs[i].text||'').trim()===it.text){used.add(i);return false;}}")
			+ TEXT("return !old(it);});}")
			+ TEXT("function outHtml(it){const label=it.status==='sending'?'Sending...':it.status==='sent'?'Sent':(it.error||'Not sent')+(it.net?' \\u00b7 will retry':' \\u00b7 tap to retry');")
			+ TEXT("return '<div class=\"msg user '+it.status+'\" data-out=\"'+it.id+'\"><div class=\"who\">You</div><div>'+esc(it.text)+'</div><div class=\"tick\"><i></i><span>'+esc(label)+'</span></div></div>';}")
			+ TEXT("function queue(text){const it={id:++outSeq,text,status:'sending',base:((last&&last.messages)||[]).length,conv:shownId,net:false,error:'',sentAt:0};")
			+ TEXT("outbox.push(it);renderLog(true);deliver(it);}")
			+ TEXT("async function deliver(it){if(it.busy)return;it.busy=true;it.status='sending';it.error='';renderLog(false);")
			+ TEXT("try{const r=await api('/api/send',{method:'POST',timeout:15000,headers:{'Content-Type':'application/json'},body:JSON.stringify({text:it.text})});")
			+ TEXT("const s=await r.json();it.status='sent';it.net=false;it.sentAt=Date.now();const c=s.activeId||0;if(c!==it.conv){it.conv=c;it.base=0;}render(s);}")
			+ TEXT("catch(e){it.status='failed';it.net=offline(e);it.error=it.net?'No connection':(e.message||'Not sent');if(it.net){fails++;conn();}}")
			+ TEXT("finally{it.busy=false;renderLog(false);}}")
			+ TEXT("document.getElementById('log').addEventListener('click',e=>{const f=e.target.closest('.failed[data-out]');")
			+ TEXT("if(f){const it=outbox.find(x=>x.id===+f.dataset.out);if(it)deliver(it);}});")
			+ TEXT("function render(s){last=s;busy=!!s.busy;fails=0;shownId=s.activeId||0;const active=(s.conversations||[]).find(c=>c.active);document.getElementById('title').textContent=active?(active.title||'Chat'):'';")
			+ TEXT("const list=document.getElementById('list');const sig=JSON.stringify({c:s.conversations,r:s.recent});if(list.dataset.sig!==sig){list.dataset.sig=sig;list.innerHTML='';")
			+ TEXT("list.appendChild(item('New chat',false,()=>post('/api/new')));")
			+ TEXT("(s.conversations||[]).forEach(c=>list.appendChild(item((c.busy?'… ':'')+(c.title||'Chat'),!!c.active,()=>post('/api/open',{localId:c.localId}),[c.workMode,c.modelId].filter(Boolean).join(' \\u00b7 '))));")
			+ TEXT("if((s.recent||[]).length){const lab=document.createElement('div');lab.className='navlabel';lab.textContent='Recent';list.appendChild(lab);")
			+ TEXT("(s.recent||[]).forEach(c=>list.appendChild(item(c.title||'Recent',false,()=>post('/api/resume',{id:c.id}))));}}")
			+ TEXT("document.getElementById('status').textContent=(s.ready?'Connected':'Not connected')")
			+ TEXT("+(s.busy?' — working':'')+' — '+(s.status||'');reconcile(s.messages||[]);conn();renderLog(false);renderControls(s);")
			+ TEXT("if(s.ready)outbox.filter(it=>it.status==='failed'&&it.net).forEach(deliver);}")
			+ TEXT("function renderLog(bottom){const log=document.getElementById('log');const stick=bottom||log.scrollHeight-log.scrollTop-log.clientHeight<96;")
			+ TEXT("const msgs=(last&&last.messages)||[];const out=outbox.filter(it=>it.conv===shownId);")
			+ TEXT("const logSig=JSON.stringify([msgs,out.map(it=>[it.id,it.status,it.error,it.net])]);")
			+ TEXT("if(log.dataset.sig!==logSig){log.dataset.sig=logSig;phoneMarkdown.resize(msgs.length);let html='',i=0;")
			+ TEXT("const work=r=>r==='tool'||r==='tooloutput'||r==='reasoning';")
			+ TEXT("while(i<msgs.length){const role=(msgs[i].role||'assistant').toLowerCase();if(work(role)){const start=i;const lines=[];")
			+ TEXT("while(i<msgs.length&&work((msgs[i].role||'').toLowerCase())){const r=(msgs[i].role||'').toLowerCase();")
			+ TEXT("let t=(msgs[i].text||'').replace(/\\n{3,}/g,'\\n');if(r==='tool')t='> '+t;else if(r==='reasoning'&&!t.trim())t='Thinking';")
			+ TEXT("lines.push(t);i++;}const n=lines.length;")
			+ TEXT("html+='<details class=\"work\"'+(openWork.has(start)?' open':'')+' data-i=\"'+start+'\"><summary>Work - '+n+' '+(n===1?'step':'steps')+'</summary><div class=\"workbody\">'+esc(lines.join('\\n'))+'</div></details>';")
			+ TEXT("}else if(role==='image'||msgs[i].image){html+='<img class=\"shot\" alt=\"\" src=\"/api/image?i='+i+'&k='+encodeURIComponent(key)+'\">';i++;}else{")
			+ TEXT("const kind=role==='system'?'system':role==='you'?'user':'assistant';const text=msgs[i].text||'';")
			+ TEXT("if(text.length&&kind==='system')html+='<div class=\"msg system\">'+esc(text)+'</div>';")
			+ TEXT("else if(text.length)html+='<div class=\"msg '+kind+(kind==='user'?' sent':'')+'\"><div class=\"who\">'+(kind==='user'?'You':'Yarin')+'</div>'")
			+ TEXT("+(kind==='user'?'<div>'+esc(text)+'</div><div class=\"tick\"><i></i></div>':'<div class=\"markdown\">'+phoneMarkdown.render(text,i)+'</div>')+'</div>';i++;}}")
			+ TEXT("html+=out.map(outHtml).join('');")
			+ TEXT("log.innerHTML=html;log.querySelectorAll('details.work').forEach(d=>{d.ontoggle=()=>{const k=+d.dataset.i;if(d.open)openWork.add(k);else openWork.delete(k);};});}")
			+ TEXT("if(stick)log.scrollTop=log.scrollHeight;}")
			+ TEXT("function renderControls(s){const act=document.getElementById('actions');act.innerHTML='';")
			+ TEXT("if(s.busy){const b=document.createElement('button');b.className='ghost';b.textContent='Stop';")
			+ TEXT("b.onclick=()=>post('/api/stop');act.appendChild(b);}if(s.waitingApproval){")
			+ TEXT("const verb={'command':'run a command','file-change':'edit files','permissions':'use extra permissions','claude-tool':'use a tool'}[s.approvalKind]||'apply a change';")
			+ TEXT("const h=document.createElement('div');h.className='tool';h.style.width='100%';h.textContent='Yarin wants to '+verb+': '+(s.approvalTitle||'')+((s.approvalCount||0)>1?'  (+'+(s.approvalCount-1)+' more)':'');act.appendChild(h);")
			+ TEXT("const ok=document.createElement('button');ok.textContent='Approve';")
			+ TEXT("ok.onclick=()=>post('/api/approve');const all=document.createElement('button');all.className='ghost';all.textContent='Approve all';")
			+ TEXT("all.onclick=()=>post('/api/approveAll');const no=document.createElement('button');")
			+ TEXT("no.className='ghost';no.textContent='Deny';no.onclick=()=>post('/api/deny');")
			+ TEXT("act.appendChild(ok);act.appendChild(all);act.appendChild(no);if(s.approvalPreview){const p=document.createElement('details');p.className='work';")
			+ TEXT("const sm=document.createElement('summary');sm.textContent='Details';p.appendChild(sm);const body=document.createElement('div');body.className='workbody';")
			+ TEXT("try{body.textContent=JSON.stringify(JSON.parse(s.approvalPreview),null,1);}catch(e){body.textContent=s.approvalPreview;}p.appendChild(body);act.appendChild(p);}}")
			+ TEXT("(s.suggestions||[]).forEach(q=>{const h=document.createElement('div');h.className='tool';h.textContent=q.question||'';")
			+ TEXT("act.appendChild(h);(q.options||[]).forEach(o=>{const b=document.createElement('button');b.textContent=o.label;")
			+ TEXT("b.title=o.description||'';b.onclick=()=>post('/api/suggest',{questionId:q.id,label:o.label});act.appendChild(b);});});")
			+ TEXT("if(document.activeElement!==mode)mode.value=s.workMode||'Plan';mode.disabled=!!s.busy;")
			+ TEXT("const msig=JSON.stringify(s.models||[]);if(model.dataset.sig!==msig){model.dataset.sig=msig;model.innerHTML='';")
			+ TEXT("(s.models||[]).forEach(m=>{const o=document.createElement('option');o.value=m.id;o.textContent=m.label||m.id;model.appendChild(o);});}")
			+ TEXT("if(document.activeElement!==model&&s.modelId)model.value=s.modelId;model.disabled=!!s.busy||!(s.models||[]).length;")
			+ TEXT("const pend=document.getElementById('pending');pend.innerHTML='';(s.pending||[]).forEach(p=>{const img=document.createElement('img');")
			+ TEXT("img.src='/api/pending?i='+p.index+'&k='+encodeURIComponent(key);pend.appendChild(img);});")
			+ TEXT("document.getElementById('send').textContent=s.busy?'Steer':'Send';}")
			+ TEXT("async function tick(){if(inflight)return;inflight=true;try{render(await(await api('/api/state',{timeout:5000})).json());}")
			+ TEXT("catch(e){if(offline(e)){fails++;conn();if(fails>=3)report(e);}else report(e);}finally{inflight=false;}}")
			+ TEXT("document.getElementById('f').onsubmit=e=>{e.preventDefault();const t=document.getElementById('t');")
			+ TEXT("const text=t.value.trim();if(!text)return;t.value='';queue(text);};")
			+ TEXT("document.addEventListener('visibilitychange',()=>{if(!document.hidden)tick();});conn();")
			+ TEXT("if(!key){document.getElementById('status').textContent='Open the phone link copied by Unreal.';}")
			+ TEXT("else{tick();setInterval(tick,1000);}</script></body></html>");
	}

	bool HasValidToken(const FString& Query, const TMap<FString, FString>& Headers)
	{
		const FString FromQuery = QueryValue(Query, TEXT("k"));
		const FString FromHeader = HeaderValue(Headers, TEXT("X-Yarin-Key"));
		return !AccessToken.IsEmpty() && (FromQuery == AccessToken || FromHeader == AccessToken);
	}

	TSharedRef<FJsonObject> BuildState()
	{
		const TSharedPtr<FAscentACFAssistant> Client = GetSharedACFAssistant(true);
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("ok"), true);
		Root->SetBoolField(TEXT("ready"), Client->IsReady());
		Root->SetBoolField(TEXT("busy"), Client->IsBusy());
		Root->SetBoolField(TEXT("waitingApproval"), Client->IsWaitingForApproval());
		Root->SetStringField(TEXT("status"), Client->GetStatus());
		Root->SetStringField(TEXT("workMode"), FACFAssistantPermissions::Label(Client->GetWorkMode()));
		Root->SetStringField(TEXT("modelId"), Client->GetModelId());
		Root->SetNumberField(TEXT("activeId"), Client->GetActiveSessionId());
		TArray<TSharedPtr<FJsonValue>> Models;
		for (const TSharedPtr<FACFAssistantModel>& Model : Client->Models)
		{
			if (!Model.IsValid() || Model->Id.IsEmpty()) { continue; }
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("id"), Model->Id);
			Row->SetStringField(TEXT("label"), Model->Label.IsEmpty() ? Model->Id : Model->Label);
			Models.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("models"), Models);
		TArray<TSharedPtr<FJsonValue>> Pending;
		int32 PendingIndex = 0;
		for (const FString& ImagePath : Client->GetPendingImages())
		{
			if (ImagePath.IsEmpty()) { ++PendingIndex; continue; }
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("index"), PendingIndex++);
			Pending.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("pending"), Pending);
		FString Preview;
		FString ApprovalTitle;
		FString ApprovalKind;
		if (const TSharedPtr<FJsonObject> Approval = Client->GetApprovalPreview())
		{
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Preview);
			FJsonSerializer::Serialize(Approval.ToSharedRef(), Writer);
			Approval->TryGetStringField(TEXT("tool"), ApprovalTitle);
			Approval->TryGetStringField(TEXT("kind"), ApprovalKind);
		}
		Root->SetStringField(TEXT("approvalPreview"), Preview);
		Root->SetStringField(TEXT("approvalTitle"), ApprovalTitle);
		Root->SetStringField(TEXT("approvalKind"), ApprovalKind);
		Root->SetNumberField(TEXT("approvalCount"), Client->GetPendingApprovalCount());
		TArray<TSharedPtr<FJsonValue>> Conversations;
		TSet<FString> OpenThreadIds;
		for (const TSharedPtr<FACFAssistantSession>& Session : Client->GetSessions())
		{
			if (!Session.IsValid()) { continue; }
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("localId"), Session->LocalId);
			Row->SetStringField(TEXT("title"), Session->GetTitle());
			Row->SetBoolField(TEXT("active"), Session->LocalId == Client->GetActiveSessionId());
			Row->SetBoolField(TEXT("busy"), Session->bBusy);
			Row->SetStringField(TEXT("modelId"), Client->SessionModelId(Session));
			Row->SetStringField(TEXT("workMode"), FACFAssistantPermissions::Label(Session->Permissions.WorkMode));
			Conversations.Add(MakeShared<FJsonValueObject>(Row));
			if (!Session->ThreadId.IsEmpty()) { OpenThreadIds.Add(Session->ThreadId); }
		}
		Root->SetArrayField(TEXT("conversations"), Conversations);
		TArray<TSharedPtr<FJsonValue>> Recent;
		for (const TSharedPtr<FACFAssistantHistory>& Entry : Client->History)
		{
			if (!Entry.IsValid() || Entry->Id.IsEmpty() || OpenThreadIds.Contains(Entry->Id)) { continue; }
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("id"), Entry->Id);
			Row->SetStringField(TEXT("title"), Entry->Title.IsEmpty() ? TEXT("Recent") : Entry->Title);
			Recent.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("recent"), Recent);
		TArray<TSharedPtr<FJsonValue>> Messages;
		for (const FACFAssistantMessage& Message : Client->GetMessages())
		{
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			const bool bMesh = ACFAssistantMesh::IsMeshRole(Message.Role);
			Row->SetStringField(TEXT("role"), bMesh ? FString(TEXT("System")) : Message.Role);
			if (bMesh)
			{
				// The phone cannot draw a 3D view, so it gets a one line note in the style of a system message.
				Row->SetStringField(TEXT("text"), ACFAssistantMesh::DescribeForPhone(Message.Role, Message.Text));
			}
			else if (Message.Role == TEXT("Image"))
			{
				Row->SetBoolField(TEXT("image"), true);
			}
			else
			{
				Row->SetStringField(TEXT("text"), Message.Text);
			}
			Messages.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("messages"), Messages);
		TArray<TSharedPtr<FJsonValue>> Suggestions;
		for (const FACFAssistantQuestion& Question : Client->GetSuggestedQuestions())
		{
			if (Client->IsSuggestedAnswered(Question.Id)) { continue; }
			const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("id"), Question.Id);
			Row->SetStringField(TEXT("question"), Question.Question);
			TArray<TSharedPtr<FJsonValue>> Options;
			for (const FACFAssistantChoice& Choice : Question.Options)
			{
				const TSharedRef<FJsonObject> Option = MakeShared<FJsonObject>();
				Option->SetStringField(TEXT("label"), Choice.Label);
				Option->SetStringField(TEXT("description"), Choice.Description);
				Options.Add(MakeShared<FJsonValueObject>(Option));
			}
			Row->SetArrayField(TEXT("options"), Options);
			Suggestions.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("suggestions"), Suggestions);
		return Root;
	}

	TSharedPtr<FJsonObject> ReadJsonObject(const FString& Body)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Object);
		return Object;
	}

	FString ReadJsonText(const FString& Body)
	{
		const TSharedPtr<FJsonObject> Object = ReadJsonObject(Body);
		if (!Object) { return FString(); }
		FString Text;
		Object->TryGetStringField(TEXT("text"), Text);
		Text.TrimStartAndEndInline();
		return Text;
	}

	void HandleRequest(FSocket& Socket, const FString& Method, const FString& Path, const FString& Query,
		const TMap<FString, FString>& Headers, const FString& Body)
	{
		if (Method == TEXT("GET") && Path == TEXT("/logo.png"))
		{
			TArray<uint8> Png;
			FString LogoPath;
			if (const TSharedPtr<IPlugin> Plugin = ACFAssistantPlugin::Owner())
			{
				LogoPath = Plugin->GetBaseDir() / TEXT("Resources/YarinLong.png");
			}
			if (LogoPath.IsEmpty() || !FFileHelper::LoadFileToArray(Png, *LogoPath))
			{
				SendError(Socket, 404, TEXT("Not Found"), TEXT("Logo is missing."));
				return;
			}
			SendRaw(Socket, 200, TEXT("OK"), TEXT("image/png"), Png);
			return;
		}
		if (!HasValidToken(Query, Headers))
		{
			SendError(Socket, 401, TEXT("Unauthorized"), TEXT("Open Yarin from the phone app."));
			return;
		}
		const TSharedPtr<FAscentACFAssistant> Client = GetSharedACFAssistant(true);
		if (Method == TEXT("GET") && (Path == TEXT("/") || Path == TEXT("/index.html")))
		{
			const FString Page = PhonePageHtml();
			if (Page.IsEmpty())
			{
				SendError(Socket, 500, TEXT("Internal Server Error"), TEXT("Phone Markdown resources are missing. Reinstall the plugin."));
				return;
			}
			SendResponse(Socket, 200, TEXT("OK"), TEXT("text/html; charset=utf-8"), Page);
			return;
		}
		if (Method == TEXT("GET") && Path == TEXT("/api/state"))
		{
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("GET") && (Path == TEXT("/api/image") || Path == TEXT("/api/pending")))
		{
			const int32 Index = FCString::Atoi(*QueryValue(Query, TEXT("i")));
			FString FilePath;
			if (Path == TEXT("/api/pending"))
			{
				const TArray<FString>& PendingImages = Client->GetPendingImages();
				if (PendingImages.IsValidIndex(Index)) { FilePath = PendingImages[Index]; }
			}
			else
			{
				const TArray<FACFAssistantMessage>& Chat = Client->GetMessages();
				if (Chat.IsValidIndex(Index) && Chat[Index].Role == TEXT("Image")) { FilePath = Chat[Index].Text; }
			}
			TArray<uint8> Bytes;
			if (FilePath.IsEmpty() || !FPaths::FileExists(FilePath) || !FFileHelper::LoadFileToArray(Bytes, *FilePath))
			{
				SendError(Socket, 404, TEXT("Not Found"), TEXT("Image is missing."));
				return;
			}
			const TCHAR* Type = TEXT("application/octet-stream");
			if (FilePath.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase)) { Type = TEXT("image/png"); }
			else if (FilePath.EndsWith(TEXT(".jpg"), ESearchCase::IgnoreCase) || FilePath.EndsWith(TEXT(".jpeg"), ESearchCase::IgnoreCase)) { Type = TEXT("image/jpeg"); }
			else if (FilePath.EndsWith(TEXT(".webp"), ESearchCase::IgnoreCase)) { Type = TEXT("image/webp"); }
			else if (FilePath.EndsWith(TEXT(".gif"), ESearchCase::IgnoreCase)) { Type = TEXT("image/gif"); }
			else if (FilePath.EndsWith(TEXT(".bmp"), ESearchCase::IgnoreCase)) { Type = TEXT("image/bmp"); }
			SendRaw(Socket, 200, TEXT("OK"), Type, Bytes, true);
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/new"))
		{
			Client->NewConversation();
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/open"))
		{
			const TSharedPtr<FJsonObject> BodyJson = ReadJsonObject(Body);
			double LocalId = 0;
			if (!BodyJson.IsValid() || !BodyJson->TryGetNumberField(TEXT("localId"), LocalId) || LocalId < 1)
			{
				SendError(Socket, 400, TEXT("Bad Request"), TEXT("Choose an open conversation."));
				return;
			}
			Client->ActivateSession(static_cast<int32>(LocalId));
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/resume"))
		{
			const TSharedPtr<FJsonObject> BodyJson = ReadJsonObject(Body);
			FString Id;
			if (!BodyJson.IsValid() || !BodyJson->TryGetStringField(TEXT("id"), Id) || Id.TrimStartAndEnd().IsEmpty())
			{
				SendError(Socket, 400, TEXT("Bad Request"), TEXT("Choose a recent conversation."));
				return;
			}
			if (!Client->IsReady())
			{
				SendError(Socket, 409, TEXT("Conflict"), TEXT("Sign in to Yarin in the Unreal Editor first."));
				return;
			}
			Client->Resume(Id);
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/send"))
		{
			const FString Text = ReadJsonText(Body);
			if (Text.IsEmpty())
			{
				SendError(Socket, 400, TEXT("Bad Request"), TEXT("Type a message first."));
				return;
			}
			if (!Client->IsReady())
			{
				SendError(Socket, 409, TEXT("Conflict"), TEXT("Sign in to Yarin in the Unreal Editor first."));
				return;
			}
			Client->Send(Text);
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/stop"))
		{
			Client->StopTurn();
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/approve"))
		{
			Client->ResolveApproval(true);
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/deny"))
		{
			Client->ResolveApproval(false);
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/approveAll"))
		{
			Client->ApproveConversation();
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/suggest"))
		{
			TSharedPtr<FJsonObject> BodyJson;
			FString QuestionId;
			FString Label;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), BodyJson) || !BodyJson
				|| !BodyJson->TryGetStringField(TEXT("label"), Label) || Label.TrimStartAndEnd().IsEmpty())
			{
				SendError(Socket, 400, TEXT("Bad Request"), TEXT("Choose one of the suggested answers."));
				return;
			}
			BodyJson->TryGetStringField(TEXT("questionId"), QuestionId);
			Client->AnswerSuggestedReply(QuestionId, Label);
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/workmode"))
		{
			const TSharedPtr<FJsonObject> BodyJson = ReadJsonObject(Body);
			FString Label;
			if (!BodyJson.IsValid() || !BodyJson->TryGetStringField(TEXT("mode"), Label))
			{
				SendError(Socket, 400, TEXT("Bad Request"), TEXT("Choose a work mode."));
				return;
			}
			EACFAssistantWorkMode Mode = EACFAssistantWorkMode::Plan;
			if (Label.Equals(TEXT("Ask"), ESearchCase::IgnoreCase)) { Mode = EACFAssistantWorkMode::Ask; }
			else if (Label.Equals(TEXT("Ask for approval"), ESearchCase::IgnoreCase)) { Mode = EACFAssistantWorkMode::AskForApproval; }
			else if (Label.Equals(TEXT("Approve for me"), ESearchCase::IgnoreCase)) { Mode = EACFAssistantWorkMode::ApproveForMe; }
			else if (!Label.Equals(TEXT("Plan"), ESearchCase::IgnoreCase))
			{
				SendError(Socket, 400, TEXT("Bad Request"), TEXT("Choose a work mode."));
				return;
			}
			double LocalId = 0;
			BodyJson->TryGetNumberField(TEXT("localId"), LocalId);
			Client->SetWorkMode(Mode, static_cast<int32>(LocalId));
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		if (Method == TEXT("POST") && Path == TEXT("/api/model"))
		{
			const TSharedPtr<FJsonObject> BodyJson = ReadJsonObject(Body);
			FString Id;
			if (!BodyJson.IsValid() || !BodyJson->TryGetStringField(TEXT("id"), Id) || Id.TrimStartAndEnd().IsEmpty())
			{
				SendError(Socket, 400, TEXT("Bad Request"), TEXT("Choose a model."));
				return;
			}
			double LocalId = 0;
			BodyJson->TryGetNumberField(TEXT("localId"), LocalId);
			Client->SelectModel(Id, static_cast<int32>(LocalId));
			SendJson(Socket, 200, TEXT("OK"), BuildState());
			return;
		}
		SendError(Socket, 404, TEXT("Not Found"), TEXT("Unknown phone page path."));
	}

	bool TryCompleteHttp(FPhoneConnection& Connection)
	{
		int32 HeaderEnd = INDEX_NONE;
		int32 HeaderSkip = 0;
		for (int32 Index = 0; Index + 1 < Connection.Buffer.Num(); ++Index)
		{
			if (Connection.Buffer[Index] == '\n' && Connection.Buffer[Index + 1] == '\n')
			{
				HeaderEnd = Index;
				HeaderSkip = 2;
				break;
			}
			if (Index + 3 < Connection.Buffer.Num() && Connection.Buffer[Index] == '\r' && Connection.Buffer[Index + 1] == '\n'
				&& Connection.Buffer[Index + 2] == '\r' && Connection.Buffer[Index + 3] == '\n')
			{
				HeaderEnd = Index;
				HeaderSkip = 4;
				break;
			}
		}
		if (HeaderEnd == INDEX_NONE) { return false; }
		TArray<uint8> Header;
		Header.Append(Connection.Buffer.GetData(), HeaderEnd);
		Header.Add(0);
		const FString HeaderBlock(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Header.GetData())));
		TArray<FString> Lines;
		HeaderBlock.ParseIntoArrayLines(Lines, false);
		TArray<FString> First;
		if (Lines.Num() > 0) { Lines[0].ParseIntoArrayWS(First); }
		if (First.Num() != 3)
		{
			SendError(*Connection.Socket, 400, TEXT("Bad Request"), TEXT("Invalid HTTP request line."));
			return true;
		}
		const FString Method = First[0].ToUpper();
		FString Path = First[1];
		FString Query;
		Path.Split(TEXT("?"), &Path, &Query);
		TMap<FString, FString> Headers;
		int32 ContentLength = 0;
		bool bHasContentLength = false;
		for (int32 Index = 1; Index < Lines.Num(); ++Index)
		{
			FString Name;
			FString Value;
			if (!Lines[Index].Split(TEXT(":"), &Name, &Value))
			{
				SendError(*Connection.Socket, 400, TEXT("Bad Request"), TEXT("Invalid HTTP header."));
				return true;
			}
			Name.TrimStartAndEndInline();
			Name.ToLowerInline();
			Value.TrimStartAndEndInline();
			if (Name == TEXT("transfer-encoding"))
			{
				SendError(*Connection.Socket, 400, TEXT("Bad Request"), TEXT("Transfer-Encoding is not supported."));
				return true;
			}
			if (Name == TEXT("content-length"))
			{
				if (bHasContentLength || Value.IsEmpty())
				{
					SendError(*Connection.Socket, 400, TEXT("Bad Request"), TEXT("Invalid or repeated Content-Length."));
					return true;
				}
				bHasContentLength = true;
				for (const TCHAR Character : Value)
				{
					if (Character < '0' || Character > '9')
					{
						SendError(*Connection.Socket, 400, TEXT("Bad Request"), TEXT("Invalid Content-Length."));
						return true;
					}
					const int32 Digit = Character - '0';
					if (ContentLength > (MaxRequestBytes - Digit) / 10)
					{
						SendError(*Connection.Socket, 413, TEXT("Payload Too Large"), TEXT("Request is too large."));
						return true;
					}
					ContentLength = ContentLength * 10 + Digit;
				}
			}
			Headers.Add(Name, Value);
		}
		const int32 HeaderBytes = HeaderEnd + HeaderSkip;
		if (ContentLength > MaxRequestBytes - HeaderBytes)
		{
			SendError(*Connection.Socket, 413, TEXT("Payload Too Large"), TEXT("Request is too large."));
			return true;
		}
		if (Connection.Buffer.Num() < HeaderBytes + ContentLength) { return false; }
		FString Body;
		if (ContentLength > 0)
		{
			TArray<uint8> BodyBytes;
			BodyBytes.Append(Connection.Buffer.GetData() + HeaderBytes, ContentLength);
			BodyBytes.Add(0);
			Body = UTF8_TO_TCHAR(reinterpret_cast<const char*>(BodyBytes.GetData()));
		}
		HandleRequest(*Connection.Socket, Method, Path, Query, Headers, Body);
		return true;
	}

	void CloseConnection(int32 Index)
	{
		if (!Connections.IsValidIndex(Index)) { return; }
		DestroySocket(Connections[Index].Socket);
		Connections.RemoveAt(Index);
	}

	void AcceptNewConnections()
	{
		if (!ListenSocket) { return; }
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets) { return; }
		bool bHasPending = false;
		while (Connections.Num() < MaxConnections && ListenSocket->HasPendingConnection(bHasPending) && bHasPending)
		{
			TSharedRef<FInternetAddr> Unused = Sockets->CreateInternetAddr();
			FSocket* Client = ListenSocket->Accept(*Unused, TEXT("YarinPhoneClient"));
			if (!Client) { break; }
			Client->SetNonBlocking(true);
			const FString Ip = PeerIPv4(*Client);
			if (!IsLoopbackPeer(Ip))
			{
				UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone page refused a non-local connection from %s"), *Ip);
				DestroySocket(Client);
				continue;
			}
			FPhoneConnection Connection;
			Connection.Socket = Client;
			Connection.AcceptedAt = FPlatformTime::Seconds();
			Connections.Add(Connection);
		}
	}

	void PumpConnections()
	{
		const double Now = FPlatformTime::Seconds();
		for (int32 Index = Connections.Num() - 1; Index >= 0; --Index)
		{
			FPhoneConnection& Connection = Connections[Index];
			if (!Connection.Socket || Now - Connection.AcceptedAt > ConnectionTimeoutSeconds)
			{
				CloseConnection(Index);
				continue;
			}
			if (Connection.bRequestHandled)
			{
				const int32 Remaining = Connection.Response.Num() - Connection.SendOffset;
				if (Remaining <= 0) { CloseConnection(Index); continue; }
				int32 Sent = 0;
				if (Connection.Socket->Send(Connection.Response.GetData() + Connection.SendOffset, FMath::Min(Remaining, MaxSendBytesPerTick), Sent))
				{
					Connection.SendOffset += Sent;
					if (Sent > 0) { Connection.AcceptedAt = Now; }
					if (Connection.SendOffset == Connection.Response.Num()) { CloseConnection(Index); }
				}
				else if (ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode() != SE_EWOULDBLOCK)
				{
					CloseConnection(Index);
				}
				continue;
			}
			uint32 Pending = 0;
			while (Connection.Socket->HasPendingData(Pending) && Pending > 0)
			{
				const int32 Offset = Connection.Buffer.Num();
				const int32 ToRead = FMath::Min(static_cast<int32>(Pending), MaxRequestBytes - Offset);
				if (ToRead <= 0)
				{
					SendError(*Connection.Socket, 413, TEXT("Payload Too Large"), TEXT("Request is too large."));
					Connection.bRequestHandled = true;
					Connection.Buffer.Reset();
					break;
				}
				Connection.Buffer.SetNumUninitialized(Offset + ToRead);
				int32 Read = 0;
				if (!Connection.Socket->Recv(Connection.Buffer.GetData() + Offset, ToRead, Read) || Read <= 0)
				{
					Connection.Buffer.SetNum(Offset);
					break;
				}
				Connection.Buffer.SetNum(Offset + Read);
			}
			if (Connection.bRequestHandled) { continue; }
			if (TryCompleteHttp(Connection))
			{
				Connection.bRequestHandled = true;
				Connection.Buffer.Reset();
			}
		}
	}

	FString CloudflaredLogPath()
	{
		return FPaths::Combine(FPlatformProcess::UserSettingsDir(), FString::Printf(TEXT("YarinAI/phone-tunnel/cloudflared-%u.log"), FPlatformProcess::GetCurrentProcessId()));
	}

	FString ExtractTunnelOrigin(const FString& Log)
	{
		const FString Marker = TEXT(".trycloudflare.com");
		int32 SearchFrom = 0;
		while (SearchFrom < Log.Len())
		{
			const int32 Host = Log.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
			if (Host == INDEX_NONE) { return FString(); }
			int32 Start = Host;
			while (Start > 0)
			{
				const TCHAR Character = Log[Start - 1];
				if (FChar::IsAlnum(Character) || Character == TEXT('.') || Character == TEXT('-'))
				{
					--Start;
					continue;
				}
				break;
			}
			if (Start >= 8 && Log.Mid(Start - 8, 8).Equals(TEXT("https://"), ESearchCase::IgnoreCase))
			{
				return Log.Mid(Start - 8, Host + Marker.Len() - (Start - 8));
			}
			SearchFrom = Host + Marker.Len();
		}
		return FString();
	}

	// dumbpipe -v prints the full ticket, then "or:" and the short ticket (endpoint id plus relay, no IP addresses). The short one
	// keeps this PC's addresses out of the QR code and out of screenshots. Both lines must be complete before it is trusted.
	FString ExtractIrohTicket(const FString& Log)
	{
		const FString Marker = TEXT("connect-tcp endpoint");
		const int32 First = Log.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart);
		const int32 Last = Log.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		// One match or none: the short ticket has not been printed yet.
		if (First == Last) { return FString(); }
		const int32 Start = Last + FCString::Strlen(TEXT("connect-tcp "));
		int32 End = Start;
		while (End < Log.Len() && FChar::IsAlnum(Log[End])) { ++End; }
		if (End >= Log.Len() || (Log[End] != TEXT('\n') && Log[End] != TEXT('\r'))) { return FString(); }
		return Log.Mid(Start, End - Start);
	}

	void Notify(const FString& Text);
	void PollTunnelExtraction();

	bool Tick(float)
	{
		if (!TunnelProcess.IsValid() && !bTunnelDownloadInFlight && !ExtractProcess.IsValid())
		{
			StopACFAssistantPhoneAccess();
			return false;
		}
		PollTunnelExtraction();
		if (TunnelProcess.IsValid() && !FPlatformProcess::IsProcRunning(TunnelProcess))
		{
			UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone tunnel exited; phone access has been revoked."));
			StopACFAssistantPhoneAccess();
			Notify(TEXT("Yarin phone tunnel disconnected. Open tunnel again to create a new phone link."));
			return false;
		}
		AcceptNewConnections();
		PumpConnections();
		if (TunnelReadPipe)
		{
			const FString Fresh = FPlatformProcess::ReadPipe(TunnelReadPipe);
			if (ActiveTransport == EACFAssistantPhoneTransport::Iroh)
			{
				// The first line dumbpipe prints is its endpoint secret key. Keep its output only until the ticket is known,
				// never log it, and keep draining the pipe afterwards so the child can never block on a full pipe.
				if (IrohTicket.IsEmpty())
				{
					TunnelLog += Fresh;
					TunnelLog.RightInline(64 * 1024);
					IrohTicket = ExtractIrohTicket(TunnelLog);
					if (!IrohTicket.IsEmpty())
					{
						TunnelLog.Empty();
						UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin phone link is ready (Iroh)."));
					}
				}
			}
			else
			{
				TunnelLog += Fresh;
				TunnelLog.RightInline(64 * 1024);
				if (PublicHttpsOrigin.IsEmpty())
				{
					FString FileLog;
					FFileHelper::LoadFileToString(FileLog, *CloudflaredLogPath());
					const FString Origin = ExtractTunnelOrigin(TunnelLog + FileLog);
					if (!Origin.IsEmpty())
					{
						PublicHttpsOrigin = Origin;
						UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin phone link is ready."));
					}
				}
			}
		}
		return true;
	}

	void Notify(const FString& Text)
	{
		FNotificationInfo Info(FText::FromString(Text));
		Info.ExpireDuration = 12.0f;
		Info.bFireAndForget = true;
		FSlateNotificationManager::Get().AddNotification(Info);
	}


	void CloseTunnelJob();

	void StopInternetTunnel()
	{
		if (TunnelDownload.IsValid())
		{
			TunnelDownload->OnProcessRequestComplete().Unbind();
			TunnelDownload->CancelRequest();
			TunnelDownload.Reset();
		}
		bTunnelDownloadInFlight = false;
		if (ExtractProcess.IsValid())
		{
			FPlatformProcess::TerminateProc(ExtractProcess, true);
			// TerminateProc returns before the process is gone. Wait, or tar may still hold the files the delete below needs.
			FPlatformProcess::WaitForProc(ExtractProcess);
			FPlatformProcess::CloseProc(ExtractProcess);
			ExtractProcess.Reset();
		}
		if (!ExtractTempDir.IsEmpty())
		{
			IFileManager::Get().DeleteDirectory(*ExtractTempDir, false, true);
			ExtractTempDir.Empty();
		}
		if (TunnelProcess.IsValid())
		{
			FPlatformProcess::TerminateProc(TunnelProcess, true);
			FPlatformProcess::CloseProc(TunnelProcess);
			TunnelProcess.Reset();
		}
		CloseTunnelJob();
		if (TunnelReadPipe || TunnelWritePipe)
		{
			FPlatformProcess::ClosePipe(TunnelReadPipe, TunnelWritePipe);
			TunnelReadPipe = nullptr;
			TunnelWritePipe = nullptr;
		}
		TunnelLog.Empty();
		PublicHttpsOrigin.Empty();
		IrohTicket.Empty();
	}

	// Iroh runs n0's reference tool dumbpipe: `listen-tcp` forwards every Iroh stream to a local TCP port, the same job
	// cloudflared does for a public hostname. The release is pinned and its download is verified against the SHA-256 published
	// with the release, because the Android client speaks the protocol of exactly this version.
	constexpr const TCHAR* DumbpipeVersion = TEXT("0.39.0");
	constexpr const TCHAR* DumbpipeZipSha256 = TEXT("a58f4e1f58281b6f5407baa6502c58cc824c41d8e0c418bbb6eea51bfd674856");

	FString TunnelFolder()
	{
		return FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("YarinAI/phone-tunnel"));
	}

	FString TunnelBinaryPath(EACFAssistantPhoneTransport Transport)
	{
#if PLATFORM_WINDOWS
		constexpr const TCHAR* Extension = TEXT(".exe");
#else
		constexpr const TCHAR* Extension = TEXT("");
#endif
		if (Transport == EACFAssistantPhoneTransport::Iroh)
		{
			return FPaths::Combine(TunnelFolder(), FString::Printf(TEXT("dumbpipe-%s/dumbpipe%s"), DumbpipeVersion, Extension));
		}
		return FPaths::Combine(TunnelFolder(), FString::Printf(TEXT("cloudflared%s"), Extension));
	}

	FString TunnelLabel(EACFAssistantPhoneTransport Transport)
	{
		return Transport == EACFAssistantPhoneTransport::Iroh ? FString(TEXT("Iroh")) : FString(TEXT("Cloudflare"));
	}

	FString TunnelDownloadUrl(EACFAssistantPhoneTransport Transport)
	{
#if PLATFORM_WINDOWS
		if (Transport == EACFAssistantPhoneTransport::Iroh)
		{
			return FString::Printf(TEXT("https://github.com/n0-computer/dumbpipe/releases/download/v%s/dumbpipe-v%s-windows-x86_64.zip"), DumbpipeVersion, DumbpipeVersion);
		}
		return TEXT("https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-windows-amd64.exe");
#else
		(void)Transport;
		return FString();
#endif
	}

	FString ResolveTunnelBinary(EACFAssistantPhoneTransport Transport)
	{
		const FString Installed = TunnelBinaryPath(Transport);
		return FPaths::FileExists(Installed) ? Installed : FString();
	}

	FString NormalizedPath(const FString& Path)
	{
		FString Result = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Result);
		return Result;
	}

	// Only reclaim a tunnel when its parent is demonstrably gone. Other Editors share this executable.
	int32 KillOrphanedTunnelProcesses()
	{
		int32 Killed = 0;
#if PLATFORM_WINDOWS
		struct FHelper
		{
			const TCHAR* Prefix;
			FString Path;
		};
		const FHelper Helpers[] = {
			{ TEXT("cloudflared"), NormalizedPath(TunnelBinaryPath(EACFAssistantPhoneTransport::Cloudflare)) },
			{ TEXT("dumbpipe"), NormalizedPath(TunnelBinaryPath(EACFAssistantPhoneTransport::Iroh)) },
		};
		const uint32 OwnPid = FPlatformProcess::GetCurrentProcessId();
		TArray<uint32> Pids;
		{
			FPlatformProcess::FProcEnumerator Enumerator;
			while (Enumerator.MoveNext())
			{
				const FPlatformProcess::FProcEnumInfo Info = Enumerator.GetCurrent();
				if (Info.GetPID() == OwnPid) { continue; }
				const FHelper* Match = nullptr;
				for (const FHelper& Helper : Helpers)
				{
					if (Info.GetName().StartsWith(Helper.Prefix, ESearchCase::IgnoreCase)) { Match = &Helper; break; }
				}
				if (!Match) { continue; }
				if (!NormalizedPath(Info.GetFullPath()).Equals(Match->Path, ESearchCase::IgnoreCase)) { continue; }
				if (Info.GetParentPID() == 0) { continue; }
				HANDLE Parent = ::OpenProcess(SYNCHRONIZE, false, Info.GetParentPID());
				if (Parent)
				{
					const DWORD State = ::WaitForSingleObject(Parent, 0);
					::CloseHandle(Parent);
					if (State != WAIT_OBJECT_0) { continue; }
				}
				else if (::GetLastError() != ERROR_INVALID_PARAMETER)
				{
					continue;
				}
				Pids.Add(Info.GetPID());
			}
		}
		for (const uint32 Pid : Pids)
		{
			FProcHandle Handle = FPlatformProcess::OpenProcess(Pid);
			if (!Handle.IsValid()) { continue; }
			FPlatformProcess::TerminateProc(Handle, true);
			FPlatformProcess::CloseProc(Handle);
			++Killed;
		}
		if (Killed > 0)
		{
			UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone tunnel: closed %d orphaned tunnel process(es) left by a previous Editor session."), Killed);
		}
#endif
		return Killed;
	}

	void CloseTunnelJob()
	{
#if PLATFORM_WINDOWS
		if (TunnelJob)
		{
			// Closing the last handle of a KILL_ON_JOB_CLOSE job terminates everything still inside it.
			::CloseHandle(static_cast<HANDLE>(TunnelJob));
			TunnelJob = nullptr;
		}
#endif
	}

	// Puts the tunnel process inside a job that dies with the Editor. Returns false when the OS refuses, in which case the
	// caller must not keep the tunnel running: a tunnel that can outlive the Editor is not acceptable.
	bool BindTunnelToEditorLifetime(FProcHandle& Process)
	{
#if PLATFORM_WINDOWS
		if (!TunnelJob)
		{
			HANDLE Job = ::CreateJobObjectW(nullptr, nullptr);
			if (!Job) { return false; }
			JOBOBJECT_EXTENDED_LIMIT_INFORMATION Limits = {};
			Limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
			if (!::SetInformationJobObject(Job, JobObjectExtendedLimitInformation, &Limits, sizeof(Limits)))
			{
				::CloseHandle(Job);
				return false;
			}
			TunnelJob = Job;
		}
		// Nested jobs are supported since Windows 8, so this works even when the Editor itself runs inside a job (IDE, build agent).
		return ::AssignProcessToJobObject(static_cast<HANDLE>(TunnelJob), Process.Get()) != 0;
#else
		(void)Process;
		return true;
#endif
	}

	// FPlatformMisc::GetSHA256Signature has no Windows implementation: it asserts "No SHA256 Platform implementation" and takes the
	// whole Editor down. Windows' own CNG provides SHA-256 on every supported version, so use that.
	bool Sha256Matches(const TArray<uint8>& Data, const TCHAR* ExpectedHex)
	{
#if PLATFORM_WINDOWS
		BCRYPT_ALG_HANDLE Algorithm = nullptr;
		BCRYPT_HASH_HANDLE Hash = nullptr;
		uint8 Digest[32] = {};
		const bool bHashed = ::BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0
			&& ::BCryptCreateHash(Algorithm, &Hash, nullptr, 0, nullptr, 0, 0) >= 0
			&& ::BCryptHashData(Hash, const_cast<PUCHAR>(Data.GetData()), static_cast<ULONG>(Data.Num()), 0) >= 0
			&& ::BCryptFinishHash(Hash, Digest, sizeof(Digest), 0) >= 0;
		if (Hash) { ::BCryptDestroyHash(Hash); }
		if (Algorithm) { ::BCryptCloseAlgorithmProvider(Algorithm, 0); }
		return bHashed && BytesToHex(Digest, sizeof(Digest)).Equals(ExpectedHex, ESearchCase::IgnoreCase);
#else
		// Iroh is only offered on Windows, so this is never reached elsewhere. Fail closed rather than trust an unverified download.
		(void)Data;
		(void)ExpectedHex;
		return false;
#endif
	}

	// A crashed or killed Editor leaves its scratch folder behind and no later session ever reuses that process id. Remove the
	// ones whose owner is gone; an Editor that is extracting at this very moment keeps its own.
	void SweepStaleDumbpipeScratch()
	{
		const FString Folder = FPaths::GetPath(TunnelBinaryPath(EACFAssistantPhoneTransport::Iroh));
		TArray<FString> Found;
		IFileManager::Get().FindFiles(Found, *FPaths::Combine(Folder, TEXT("dumbpipe.exe.part-*")), false, true);
		for (const FString& Name : Found)
		{
			const int32 Marker = Name.Find(TEXT(".part-"));
			const int32 Pid = Marker == INDEX_NONE ? 0 : FCString::Atoi(*Name.RightChop(Marker + 6));
			if (Pid > 0 && FPlatformProcess::IsApplicationRunning(static_cast<uint32>(Pid))) { continue; }
			IFileManager::Get().DeleteDirectory(*FPaths::Combine(Folder, Name), false, true);
		}
	}

	// Unpacks the verified dumbpipe zip with the tar.exe that ships in every supported Windows. It runs as a child process that
	// the ticker polls, so the Editor thread never waits on it, and it extracts into a scratch folder so a half-written
	// executable can never sit at the final path where the next launch would trust it.
	bool BeginDumbpipeExtraction(const TArray<uint8>& Zip)
	{
#if PLATFORM_WINDOWS
		SweepStaleDumbpipeScratch();
		const FString Scratch = TunnelBinaryPath(EACFAssistantPhoneTransport::Iroh) + FString::Printf(TEXT(".part-%u"), FPlatformProcess::GetCurrentProcessId());
		IFileManager::Get().DeleteDirectory(*Scratch, false, true);
		IFileManager::Get().MakeDirectory(*Scratch, true);
		const FString ZipPath = FPaths::Combine(Scratch, TEXT("dumbpipe.zip"));
		const FString Tar = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot")), TEXT("System32/tar.exe"));
		if (!FPaths::FileExists(Tar) || !FFileHelper::SaveArrayToFile(Zip, *ZipPath))
		{
			IFileManager::Get().DeleteDirectory(*Scratch, false, true);
			return false;
		}
		const FString Args = FString::Printf(TEXT("-xf \"%s\" -C \"%s\""),
			*FPaths::ConvertRelativePathToFull(ZipPath).Replace(TEXT("/"), TEXT("\\")),
			*FPaths::ConvertRelativePathToFull(Scratch).Replace(TEXT("/"), TEXT("\\")));
		ExtractProcess = FPlatformProcess::CreateProc(*Tar, *Args, false, true, true, nullptr, 0, nullptr, nullptr);
		if (!ExtractProcess.IsValid())
		{
			IFileManager::Get().DeleteDirectory(*Scratch, false, true);
			return false;
		}
		ExtractTempDir = Scratch;
		return true;
#else
		(void)Zip;
		return false;
#endif
	}

	void StartInternetTunnel()
	{
		if (BoundPort <= 0 || TunnelProcess.IsValid()) { return; }
		const EACFAssistantPhoneTransport Transport = ActiveTransport;
		const FString Label = TunnelLabel(Transport);
		const FString Binary = ResolveTunnelBinary(Transport);
		if (Binary.IsEmpty()) { return; }
		// Never let a stale tunnel from an earlier session coexist with the new one.
		KillOrphanedTunnelProcesses();
		if (TunnelReadPipe || TunnelWritePipe)
		{
			FPlatformProcess::ClosePipe(TunnelReadPipe, TunnelWritePipe);
			TunnelReadPipe = nullptr;
			TunnelWritePipe = nullptr;
		}
		if (!FPlatformProcess::CreatePipe(TunnelReadPipe, TunnelWritePipe))
		{
			Notify(FString::Printf(TEXT("Yarin phone page could not start the %s tunnel."), *Label));
			return;
		}
		FString Args;
		if (Transport == EACFAssistantPhoneTransport::Iroh)
		{
			// -v makes dumbpipe print the short ticket as well as the full one.
			Args = FString::Printf(TEXT("listen-tcp --host 127.0.0.1:%d -v"), BoundPort);
		}
		else
		{
			IFileManager::Get().Delete(*CloudflaredLogPath(), false, true);
			Args = FString::Printf(TEXT("tunnel --no-autoupdate --logfile \"%s\" --url http://127.0.0.1:%d"), *CloudflaredLogPath(), BoundPort);
		}
		TunnelLog.Empty();
		PublicHttpsOrigin.Empty();
		IrohTicket.Empty();
		// dumbpipe reads its identity from IROH_SECRET. CreateProc takes no environment, so the variable is set process-wide for
		// this one call and removed straight after. A process another Editor thread spawns inside that window (a shader worker, say)
		// would inherit it; those are same-user engine children that do not log their environment.
		const bool bPinIrohIdentity = Transport == EACFAssistantPhoneTransport::Iroh && !IrohSecretHex.IsEmpty();
		if (bPinIrohIdentity) { FPlatformMisc::SetEnvironmentVar(TEXT("IROH_SECRET"), *IrohSecretHex); }
		TunnelProcess = FPlatformProcess::CreateProc(*Binary, *Args, false, true, true, nullptr, 0, nullptr, TunnelWritePipe);
		if (bPinIrohIdentity) { FPlatformMisc::SetEnvironmentVar(TEXT("IROH_SECRET"), TEXT("")); }
		if (!TunnelProcess.IsValid())
		{
			FPlatformProcess::ClosePipe(TunnelReadPipe, TunnelWritePipe);
			TunnelReadPipe = nullptr;
			TunnelWritePipe = nullptr;
			Notify(FString::Printf(TEXT("Yarin phone page could not launch the %s tunnel."), *Label));
			UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone tunnel failed to start %s"), *Binary);
			return;
		}
		if (!BindTunnelToEditorLifetime(TunnelProcess))
		{
			// The tunnel must never outlive the Editor. Without the job guarantee, do not publish at all.
			FPlatformProcess::TerminateProc(TunnelProcess, true);
			FPlatformProcess::CloseProc(TunnelProcess);
			TunnelProcess.Reset();
			CloseTunnelJob();
			FPlatformProcess::ClosePipe(TunnelReadPipe, TunnelWritePipe);
			TunnelReadPipe = nullptr;
			TunnelWritePipe = nullptr;
			Notify(TEXT("Yarin phone page stopped: the tunnel could not be tied to the Editor lifetime."));
			UE_LOG(LogAscentACFAssistant, Error, TEXT("Yarin phone tunnel could not be assigned to the Editor job object; tunnel terminated."));
			return;
		}
		Notify(Transport == EACFAssistantPhoneTransport::Iroh
			? TEXT("Yarin is publishing an encrypted Iroh phone link. Keep the Editor running.")
			: TEXT("Yarin is publishing a public HTTPS phone link. Keep the Editor running."));
		UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin phone tunnel started (%s)"), *Binary);
	}

	void PollTunnelExtraction()
	{
		if (!ExtractProcess.IsValid() || FPlatformProcess::IsProcRunning(ExtractProcess)) { return; }
		int32 ReturnCode = -1;
		FPlatformProcess::GetProcReturnCode(ExtractProcess, &ReturnCode);
		FPlatformProcess::CloseProc(ExtractProcess);
		ExtractProcess.Reset();
		const FString Final = TunnelBinaryPath(EACFAssistantPhoneTransport::Iroh);
		const FString Extracted = FPaths::Combine(ExtractTempDir, TEXT("dumbpipe.exe"));
		const bool bInstalled = ReturnCode == 0 && FPaths::FileExists(Extracted)
			&& IFileManager::Get().Move(*Final, *Extracted, true, true);
		IFileManager::Get().DeleteDirectory(*ExtractTempDir, false, true);
		ExtractTempDir.Empty();
		if (!bInstalled)
		{
			Notify(TEXT("Yarin could not unpack the Iroh tunnel helper. Open tunnel to try again."));
			UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone tunnel: unpacking dumbpipe failed (tar exit code %d)."), ReturnCode);
			return;
		}
		StartInternetTunnel();
	}

	void EnsureInternetTunnel()
	{
		if (TunnelProcess.IsValid() || bTunnelDownloadInFlight || ExtractProcess.IsValid()) { return; }
		const EACFAssistantPhoneTransport Transport = ActiveTransport;
		if (!ResolveTunnelBinary(Transport).IsEmpty())
		{
			StartInternetTunnel();
			return;
		}
		const FString Url = TunnelDownloadUrl(Transport);
		if (Url.IsEmpty())
		{
			Notify(TEXT("Yarin phone access from the internet is not packaged for this platform."));
			return;
		}
		const FString Label = TunnelLabel(Transport);
		const FString InstallPath = TunnelBinaryPath(Transport);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(InstallPath), true);
		Notify(FString::Printf(TEXT("Yarin is downloading the %s tunnel helper (first time only)..."), *Label));
		bTunnelDownloadInFlight = true;
		TunnelDownload = FHttpModule::Get().CreateRequest();
		TunnelDownload->SetVerb(TEXT("GET"));
		TunnelDownload->SetURL(Url);
		TunnelDownload->SetHeader(TEXT("User-Agent"), TEXT("YarinPhoneAccess"));
		TunnelDownload->OnProcessRequestComplete().BindLambda([InstallPath, Transport, Label](FHttpRequestPtr Completed, FHttpResponsePtr Response, bool bSucceeded)
		{
			if (Completed != TunnelDownload) { return; }
			bTunnelDownloadInFlight = false;
			TunnelDownload.Reset();
			if (!UACFAssistantSettings::Get().bAllowPhoneAccessFromTheInternet || !ListenSocket)
			{
				return;
			}
			const bool bOk = bSucceeded && Response.IsValid() && Response->GetResponseCode() >= 200 && Response->GetResponseCode() < 300
				&& Response->GetContent().Num() > 1024 * 1024;
			if (!bOk)
			{
				Notify(FString::Printf(TEXT("Yarin could not download the %s tunnel helper. Check internet access and try again."), *Label));
				UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone tunnel download failed from GitHub."));
				return;
			}
			if (Transport == EACFAssistantPhoneTransport::Iroh)
			{
				if (!Sha256Matches(Response->GetContent(), DumbpipeZipSha256))
				{
					Notify(TEXT("Yarin refused the Iroh tunnel helper: its checksum did not match the pinned release."));
					UE_LOG(LogAscentACFAssistant, Error, TEXT("Yarin phone tunnel: dumbpipe download failed SHA-256 verification; discarded."));
					return;
				}
				if (!BeginDumbpipeExtraction(Response->GetContent()))
				{
					Notify(TEXT("Yarin could not unpack the Iroh tunnel helper on this system."));
					UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone tunnel: could not start unpacking dumbpipe."));
				}
				return;
			}
			if (!FFileHelper::SaveArrayToFile(Response->GetContent(), *InstallPath))
			{
				Notify(FString::Printf(TEXT("Yarin could not save the %s tunnel helper."), *Label));
				return;
			}
#if PLATFORM_MAC || PLATFORM_LINUX
			::chmod(TCHAR_TO_UTF8(*InstallPath), 0755);
#endif
			StartInternetTunnel();
		});
		if (!TunnelDownload->ProcessRequest())
		{
			bTunnelDownloadInFlight = false;
			TunnelDownload.Reset();
			Notify(FString::Printf(TEXT("Yarin could not start the %s tunnel helper download."), *Label));
		}
	}

	class SYarinPhoneQr : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SYarinPhoneQr) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&)
		{
			Qr.SetNumZeroed(qrcodegen_BUFFER_LEN_MAX);
			SetCanTick(true);
		}

		virtual FVector2D ComputeDesiredSize(float) const override
		{
			return FVector2D(280.0f, 280.0f);
		}

		virtual void Tick(const FGeometry&, const double, const float) override
		{
			const FString Url = GetACFAssistantPhoneUrl();
			if (Url == CachedUrl) { return; }
			CachedUrl = Url;
			bHasCode = false;
			if (!Url.IsEmpty())
			{
				uint8 Temp[qrcodegen_BUFFER_LEN_MAX];
				bHasCode = qrcodegen_encodeText(TCHAR_TO_UTF8(*Url), Temp, Qr.GetData(), qrcodegen_Ecc_MEDIUM, 1, 20, qrcodegen_Mask_AUTO, true);
			}
			Invalidate(EInvalidateWidgetReason::Paint);
		}

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
		{
			const FSlateBrush* White = FAppStyle::GetBrush(TEXT("WhiteBrush"));
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), White, ESlateDrawEffect::None, FLinearColor::White);
			if (!bHasCode) { return LayerId; }
			const int32 Count = qrcodegen_getSize(Qr.GetData());
			if (Count <= 0) { return LayerId; }
			constexpr int32 Quiet = 4;
			const float Side = FMath::Min(AllottedGeometry.GetLocalSize().X, AllottedGeometry.GetLocalSize().Y);
			const float Module = Side / static_cast<float>(Count + Quiet * 2);
			const float Origin = Module * Quiet;
			const int32 DarkLayer = LayerId + 1;
			for (int32 Y = 0; Y < Count; ++Y)
			{
				for (int32 X = 0; X < Count; ++X)
				{
					if (!qrcodegen_getModule(Qr.GetData(), X, Y)) { continue; }
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						DarkLayer,
						AllottedGeometry.ToPaintGeometry(
							FVector2D(Module, Module),
							FSlateLayoutTransform(FVector2D(Origin + Module * X, Origin + Module * Y))),
						White,
						ESlateDrawEffect::None,
						FLinearColor::Black);
				}
			}
			return DarkLayer;
		}

	private:
		FString CachedUrl;
		bool bHasCode = false;
		TArray<uint8> Qr;
	};

	// Stops the server and tunnel without forgetting that the user asked for them, so a port change can restart in place.
	void StopPhoneServer()
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
		Ticker.Reset();
		for (int32 Index = Connections.Num() - 1; Index >= 0; --Index) { CloseConnection(Index); }
		DestroySocket(ListenSocket);
		StopInternetTunnel();
		BoundPort = 0;
		AccessToken.Empty();
		IrohSecretHex.Empty();
	}
}

void StopACFAssistantPhoneAccess()
{
	bTunnelRequested = false;
	StopPhoneServer();
}

void OpenACFAssistantPhoneTunnel()
{
	bTunnelRequested = true;
	SyncACFAssistantPhoneAccess();
}

void CloseACFAssistantPhoneTunnel()
{
	if (!bTunnelRequested && !ListenSocket) { return; }
	StopACFAssistantPhoneAccess();
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin phone tunnel closed."));
	Notify(TEXT("Yarin phone link is off."));
}

bool IsACFAssistantPhoneTunnelOpen()
{
	return bTunnelRequested;
}

bool IsACFAssistantPhoneAccessListening()
{
	return ListenSocket != nullptr;
}

int32 CleanupACFAssistantPhoneTunnels()
{
	return KillOrphanedTunnelProcesses();
}

FString GetACFAssistantPhoneUrl()
{
	if (!ListenSocket || AccessToken.IsEmpty()) { return FString(); }
	if (ActiveTransport == EACFAssistantPhoneTransport::Iroh)
	{
		if (IrohTicket.IsEmpty()) { return FString(); }
		return TEXT("yarin://iroh?ticket=") + FGenericPlatformHttp::UrlEncode(IrohTicket) + TEXT("&k=") + AccessToken;
	}
	if (PublicHttpsOrigin.IsEmpty()) { return FString(); }
	return PublicHttpsOrigin + TEXT("/?k=") + AccessToken;
}

void CopyACFAssistantPhoneUrl()
{
	const FString Url = GetACFAssistantPhoneUrl();
	if (Url.IsEmpty())
	{
		UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone page is not public yet. Enable Allow Phone Access From The Internet in Engine > Ascent Yarin."));
		Notify(TEXT("Yarin phone page is off or still publishing. Enable it in Project Settings > Engine > Ascent Yarin."));
		return;
	}
	FPlatformApplicationMisc::ClipboardCopy(*Url);
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin phone link copied to clipboard."));
	Notify(TEXT("Yarin phone link copied. Keep it private: it grants access to the Editor."));
}

void ForgetACFAssistantPhonePairing()
{
	if (!RevokeIrohPairing())
	{
		UE_LOG(LogAscentACFAssistant, Error, TEXT("Yarin phone pairing could not be replaced: %s is locked or not writable. The old link still works."), *PairingFilePath());
		Notify(TEXT("Yarin could not forget the paired phone: its pairing file is locked. The old link still works. Close the Editor and delete Saved/Yarin/PhonePairing.dat."));
		return;
	}
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin phone pairing forgotten."));
	if (ListenSocket && ActiveTransport == EACFAssistantPhoneTransport::Iroh)
	{
		// Same tunnel, new identity and token: the old QR code and the phone that scanned it stop working at once.
		StopInternetTunnel();
		IssueAccessCredentials();
		EnsureInternetTunnel();
		Notify(TEXT("Yarin forgot the paired phone. Scan the new QR code to pair again."));
		return;
	}
	Notify(TEXT("Yarin forgot the paired phone. The next Open tunnel shows a new QR code."));
}

void RegisterACFAssistantPhoneCommands()
{
	if (ShowUrlCommand) { return; }
	ShowUrlCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Yarin.ShowPhoneUrl"),
		TEXT("Copies the Yarin phone page URL. Open the tunnel in the Yarin phone window first."),
		FConsoleCommandDelegate::CreateStatic(&CopyACFAssistantPhoneUrl));
	ForgetPhoneCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("Yarin.ForgetPhone"),
		TEXT("Forgets the paired phone: the Iroh link and token are replaced, so the old QR code and any phone that scanned it stop working."),
		FConsoleCommandDelegate::CreateStatic(&ForgetACFAssistantPhonePairing));
}

void UnregisterACFAssistantPhoneCommands()
{
	for (IConsoleObject** Command : { &ShowUrlCommand, &ForgetPhoneCommand })
	{
		if (*Command) { IConsoleManager::Get().UnregisterConsoleObject(*Command, false); }
		*Command = nullptr;
	}
}

void OpenACFAssistantPhoneTunnelAtStartup()
{
	const UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
	if (bTunnelRequested || !Settings.bAllowPhoneAccessFromTheInternet || !Settings.bOpenPhoneTunnelAtStartup
		|| Settings.PhoneTransport != EACFAssistantPhoneTransport::Iroh)
	{
		return;
	}
	UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin phone tunnel: reopening the paired Iroh link (Open Phone Tunnel At Editor Startup is on)."));
	OpenACFAssistantPhoneTunnel();
}

void SyncACFAssistantPhoneAccess()
{
	const UACFAssistantSettings& Settings = UACFAssistantSettings::Get();
	if (!Settings.bAllowPhoneAccessFromTheInternet) { bTunnelRequested = false; }
	if (!bTunnelRequested)
	{
		if (ListenSocket || TunnelProcess.IsValid() || bTunnelDownloadInFlight || ExtractProcess.IsValid())
		{
			StopPhoneServer();
			UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin phone page stopped."));
		}
		return;
	}

	const int32 Port = FMath::Clamp(Settings.PhoneAccessPort, 1024, 65535);
	if (ListenSocket && BoundPort == Port)
	{
		if (Settings.PhoneTransport != ActiveTransport)
		{
			// A different transport is a different link. Stop the old helper and issue the new transport's credentials: Cloudflare gets a fresh token, Iroh loads this project's pairing.
			StopInternetTunnel();
			ActiveTransport = Settings.PhoneTransport;
			IssueAccessCredentials();
		}
		EnsureInternetTunnel();
		return;
	}

	StopPhoneServer();
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets)
	{
		Notify(TEXT("Yarin phone page could not start (no sockets)."));
		bTunnelRequested = false;
		return;
	}

	ListenSocket = Sockets->CreateSocket(NAME_Stream, TEXT("YarinPhoneListen"), false);
	if (!ListenSocket)
	{
		Notify(TEXT("Yarin phone page could not create a socket."));
		bTunnelRequested = false;
		return;
	}

	ListenSocket->SetReuseAddr(false);
	ListenSocket->SetNonBlocking(true);
	bool bIsValid = false;
	TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
	Address->SetIp(TEXT("127.0.0.1"), bIsValid);
	Address->SetPort(Port);
	if (!bIsValid || !ListenSocket->Bind(*Address) || !ListenSocket->Listen(MaxConnections))
	{
		DestroySocket(ListenSocket);
		Notify(FString::Printf(TEXT("Yarin phone page could not bind TCP port %d."), Port));
		UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin phone page failed to bind 127.0.0.1:%d"), Port);
		bTunnelRequested = false;
		return;
	}

	BoundPort = Port;
	ActiveTransport = Settings.PhoneTransport;
	IssueAccessCredentials();
	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.05f);
	if (const TSharedPtr<FAscentACFAssistant> Client = GetSharedACFAssistant(true))
	{
		Client->AutoConnect();
	}
	EnsureInternetTunnel();
}

void ShowACFAssistantPhoneWindow()
{
	if (!UACFAssistantSettings::Get().bAllowPhoneAccessFromTheInternet)
	{
		const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(TEXT(
			"Phone access is off.\n\n"
			"Turn it on in Project Settings > Engine > Ascent Yarin.\n"
			"While the tunnel is open, Yarin publishes a public link. Anyone with that link can send prompts and approve Editor changes.\n\n"
			"Open Project Settings?")));
		if (Answer == EAppReturnType::Yes)
		{
			if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings")))
			{
				SettingsModule->ShowViewer(TEXT("Project"), TEXT("Engine"), TEXT("Ascent Yarin"));
			}
		}
		return;
	}
	if (PhoneWindow.IsValid())
	{
		PhoneWindow->BringToFront();
		return;
	}
	const auto WhenOpen = [] { return bTunnelRequested ? EVisibility::Visible : EVisibility::Collapsed; };
	const auto WhenClosed = [] { return bTunnelRequested ? EVisibility::Collapsed : EVisibility::Visible; };
	PhoneWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("Yarin")))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::Autosized)
		[
			SNew(SBorder)
			.Padding(24.0f)
			.BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor::White)
			[
				SNew(SBox).WidthOverride(312.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 0.0f, 0.0f, 16.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([]
						{
							if (!bTunnelRequested) { return FText::FromString(TEXT("Phone access")); }
							return FText::FromString(ActiveTransport == EACFAssistantPhoneTransport::Iroh ? TEXT("Scan with the Yarin app") : TEXT("Scan with the phone camera"));
						})
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
						.ColorAndOpacity(FLinearColor::Black)
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
					[
						SNew(SBox).WidthOverride(280.0f).HeightOverride(280.0f).Visibility_Lambda(WhenOpen)
						[
							SNew(SYarinPhoneQr)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 16.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Justification(ETextJustify::Center)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11))
						.ColorAndOpacity(FLinearColor(0.2f, 0.2f, 0.2f))
						.Text_Lambda([]
						{
							if (!bTunnelRequested)
							{
								return FText::FromString(TEXT("Phone Access is in BETA. Do not use it on projects containing high-risk or sensitive data."));
							}
							if (GetACFAssistantPhoneUrl().IsEmpty())
							{
								return FText::FromString(TEXT("Publishing the link..."));
							}
							return FText::FromString(ActiveTransport == EACFAssistantPhoneTransport::Iroh
								? TEXT("Encrypted end to end. Scan it once with the Yarin Android app: the phone then reconnects by itself whenever this project's tunnel is open, even after an Editor restart. A plain browser cannot open an Iroh link. You can close this window: the link stays on until Close tunnel.")
								: TEXT("Opens the Yarin app. If it is not installed, the camera opens the browser. You can close this window: the link stays on until Close tunnel."));
						})
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 16.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Visibility_Lambda(WhenClosed)
						.ContentPadding(FMargin(16.0f, 6.0f))
						.Text(FText::FromString(TEXT("Open tunnel")))
						.OnClicked_Lambda([] { OpenACFAssistantPhoneTunnel(); return FReply::Handled(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 16.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Visibility_Lambda(WhenOpen)
						.ContentPadding(FMargin(16.0f, 6.0f))
						.Text(FText::FromString(TEXT("Close tunnel")))
						.OnClicked_Lambda([] { CloseACFAssistantPhoneTunnel(); return FReply::Handled(); })
					]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Visibility_Lambda([] { return bTunnelRequested && ActiveTransport == EACFAssistantPhoneTransport::Iroh ? EVisibility::Visible : EVisibility::Collapsed; })
						.ContentPadding(FMargin(16.0f, 6.0f))
						.ToolTipText(FText::FromString(TEXT("Replaces this project's Iroh link and token. The phone that scanned the old QR code stops working until it scans the new one.")))
						.Text(FText::FromString(TEXT("Forget paired phone")))
						.OnClicked_Lambda([] { ForgetACFAssistantPhonePairing(); return FReply::Handled(); })
					]
				]
			]
		];
	PhoneWindow->SetOnWindowClosed(FOnWindowClosed::CreateLambda([](const TSharedRef<SWindow>&)
	{
		PhoneWindow.Reset();
	}));
	FSlateApplication::Get().AddWindow(PhoneWindow.ToSharedRef());
}
