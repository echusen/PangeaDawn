// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACFAssistantToolGate.h"
#include "ACFAssistantClientInternal.h"
#include "ACFAssistantMesh.h"
#include "ACFAssistantToolPolicy.h"
#include "AscentACFAssistant.h"
#include "AscentEpicMCP.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "HttpPath.h"
#include "HttpRouteHandle.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr uint32 PortAttempts = 10;
	constexpr int32 MaxBodyBytes = 16 * 1024 * 1024;
	// Editor tools and approvals can take a long time. Codex and Claude apply their own MCP timeouts, configured by the client.
	constexpr float UpstreamTimeoutSeconds = 3600.0f;
	const TCHAR* GatePath = TEXT("/mcp");

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle PostRoute;
	FHttpRouteHandle GetRoute;
	FHttpRouteHandle DeleteRoute;
	uint32 GatePort = 0;
	TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> InFlight;

	// Everything the gate needs from a request after the handler returned. FHttpServerRequest itself is not kept alive.
	struct FRelayRequest
	{
		FString Verb;
		TMap<FString, FString> Headers;
		TArray<uint8> Body;
	};

	FString HeaderValue(const TMap<FString, TArray<FString>>& Headers, const TCHAR* Name)
	{
		// FString keys hash and compare case-insensitively, and the HTTP server lower-cases incoming names anyway.
		if (const TArray<FString>* Values = Headers.Find(Name))
		{
			return Values->IsEmpty() ? FString() : (*Values)[0];
		}
		return FString();
	}

	FRelayRequest Capture(const FHttpServerRequest& Request)
	{
		FRelayRequest Relay;
		Relay.Verb = Request.Verb == EHttpServerRequestVerbs::VERB_DELETE ? TEXT("DELETE") : TEXT("POST");
		for (const TCHAR* Name : {TEXT("Content-Type"), TEXT("Accept"), TEXT("Mcp-Session-Id"), TEXT("MCP-Protocol-Version"), TEXT("Authorization")})
		{
			const FString Value = HeaderValue(Request.Headers, Name);
			if (!Value.IsEmpty()) { Relay.Headers.Add(Name, Value); }
		}
		Relay.Body = Request.Body;
		return Relay;
	}

	uint32 UpstreamPort()
	{
		const FString Upstream = AscentEpicMCP::Url();
		int32 Colon = INDEX_NONE;
		if (!Upstream.FindLastChar(TEXT(':'), Colon)) { return 0; }
		return static_cast<uint32>(FCString::Atoi(*Upstream.Mid(Colon + 1)));
	}

	FString Serialize(const TSharedRef<FJsonObject>& Object)
	{
		FString Text;
		FJsonSerializer::Serialize(Object, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
		return Text;
	}

	TUniquePtr<FHttpServerResponse> JsonResponse(const FString& Body, EHttpServerResponseCodes Code)
	{
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(Body, TEXT("application/json"));
		Response->Code = Code;
		return Response;
	}

	TUniquePtr<FHttpServerResponse> JsonRpcError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message, EHttpServerResponseCodes HttpCode)
	{
		const TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetNumberField(TEXT("code"), Code);
		Error->SetStringField(TEXT("message"), Message);
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
		Root->SetObjectField(TEXT("error"), Error);
		return JsonResponse(Serialize(Root), HttpCode);
	}

	// A JSON-RPC success whose MCP result is a text block. bIsError marks a tool failure the model should read and react to.
	TUniquePtr<FHttpServerResponse> ToolTextResult(const TSharedPtr<FJsonValue>& Id, const FString& Text, bool bIsError)
	{
		const TSharedRef<FJsonObject> Content = MakeShared<FJsonObject>();
		Content->SetStringField(TEXT("type"), TEXT("text"));
		Content->SetStringField(TEXT("text"), Text);
		const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("content"), {MakeShared<FJsonValueObject>(Content)});
		Result->SetBoolField(TEXT("isError"), bIsError);
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
		Root->SetField(TEXT("id"), Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
		Root->SetObjectField(TEXT("result"), Result);
		return JsonResponse(Serialize(Root), EHttpServerResponseCodes::Ok);
	}

	// Relays one message to the Editor tool server and streams its answer back. Optional PostProcess edits a JSON result before it leaves.
	// Observe sees the outcome of every relay exactly once, as the HTTP code (0 when nothing answered) and the body text.
	void Forward(const FRelayRequest& Relay, const FHttpResultCallback& OnComplete, TFunction<void(TSharedPtr<FJsonObject>&)> PostProcess = nullptr, TFunction<void(int32, const FString&)> Observe = nullptr)
	{
		const FString Upstream = AscentEpicMCP::Url();
		if (Upstream.IsEmpty() || UpstreamPort() == 0)
		{
			if (Observe) { Observe(0, FString()); }
			OnComplete(JsonRpcError(nullptr, -32000, TEXT("The Editor tool server is not running."), EHttpServerResponseCodes::BadGateway));
			return;
		}
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Upstream);
		Request->SetVerb(Relay.Verb);
		for (const auto& Header : Relay.Headers) { Request->SetHeader(Header.Key, Header.Value); }
		if (!Relay.Headers.Contains(TEXT("Content-Type")) && Relay.Verb == TEXT("POST")) { Request->SetHeader(TEXT("Content-Type"), TEXT("application/json")); }
		if (!Relay.Headers.Contains(TEXT("Accept"))) { Request->SetHeader(TEXT("Accept"), TEXT("application/json, text/event-stream")); }
		if (Relay.Body.Num() > 0) { Request->SetContent(Relay.Body); }
		Request->SetTimeout(UpstreamTimeoutSeconds);
		Request->SetActivityTimeout(UpstreamTimeoutSeconds);
		InFlight.Add(Request);
		Request->OnProcessRequestComplete().BindLambda([OnComplete, PostProcess, Observe](FHttpRequestPtr Sent, FHttpResponsePtr Response, bool bSucceeded)
		{
			InFlight.Remove(Sent);
			if (!bSucceeded || !Response.IsValid())
			{
				if (Observe) { Observe(0, FString()); }
				OnComplete(JsonRpcError(nullptr, -32000, TEXT("The Editor tool server did not answer."), EHttpServerResponseCodes::BadGateway));
				return;
			}
			FString ContentType = Response->GetContentType();
			if (ContentType.IsEmpty()) { ContentType = TEXT("application/json"); }
			TArray<uint8> Body = Response->GetContent();
			if (Observe)
			{
				const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Body.GetData()), Body.Num());
				Observe(Response->GetResponseCode(), FString(Text.Length(), Text.Get()));
			}
			if (PostProcess && ContentType.Contains(TEXT("json"), ESearchCase::IgnoreCase) && Body.Num() > 0)
			{
				TSharedPtr<FJsonObject> Parsed;
				const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Body.GetData()), Body.Num());
				if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(Converted.Length(), Converted.Get())), Parsed) && Parsed.IsValid())
				{
					PostProcess(Parsed);
					const FString Rewritten = Serialize(Parsed.ToSharedRef());
					const FTCHARToUTF8 Utf8(*Rewritten);
					Body.Reset();
					Body.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
				}
			}
			TUniquePtr<FHttpServerResponse> Reply = FHttpServerResponse::Create(MoveTemp(Body), ContentType);
			Reply->Code = static_cast<EHttpServerResponseCodes>(Response->GetResponseCode());
			const FString SessionId = Response->GetHeader(TEXT("Mcp-Session-Id"));
			if (!SessionId.IsEmpty()) { Reply->Headers.Add(TEXT("Mcp-Session-Id"), {SessionId}); }
			OnComplete(MoveTemp(Reply));
		});
		if (!Request->ProcessRequest())
		{
			InFlight.Remove(Request);
			if (Observe) { Observe(0, FString()); }
			OnComplete(JsonRpcError(nullptr, -32000, TEXT("The Editor tool server could not be reached."), EHttpServerResponseCodes::BadGateway));
		}
	}

	TSharedRef<FJsonObject> ClaudePermissionToolDefinition()
	{
		const TSharedRef<FJsonObject> ToolName = MakeShared<FJsonObject>();
		ToolName->SetStringField(TEXT("type"), TEXT("string"));
		const TSharedRef<FJsonObject> Input = MakeShared<FJsonObject>();
		Input->SetStringField(TEXT("type"), TEXT("object"));
		const TSharedRef<FJsonObject> ToolUseId = MakeShared<FJsonObject>();
		ToolUseId->SetStringField(TEXT("type"), TEXT("string"));
		const TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		Properties->SetObjectField(TEXT("tool_name"), ToolName);
		Properties->SetObjectField(TEXT("input"), Input);
		Properties->SetObjectField(TEXT("tool_use_id"), ToolUseId);
		const TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
		Schema->SetStringField(TEXT("type"), TEXT("object"));
		Schema->SetObjectField(TEXT("properties"), Properties);
		Schema->SetArrayField(TEXT("required"), {MakeShared<FJsonValueString>(TEXT("tool_name")), MakeShared<FJsonValueString>(TEXT("input"))});
		const TSharedRef<FJsonObject> Tool = MakeShared<FJsonObject>();
		Tool->SetStringField(TEXT("name"), ACFAssistantToolGate::ClaudePermissionToolName);
		Tool->SetStringField(TEXT("description"), TEXT("Yarin's permission prompt for the host. Never call it yourself; the host calls it when a tool needs the user's approval."));
		Tool->SetObjectField(TEXT("inputSchema"), Schema);
		return Tool;
	}

	// Claude Code asks here before Bash, Edit, Write and the other prompting tools while the turn runs in default permission mode.
	// The answer is the JSON Claude expects as the tool's text: allow with the original input, or deny with a message.
	void HandleClaudePermission(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Arguments, const FString& ConversationToken, const FHttpResultCallback& OnComplete)
	{
		FString ToolName;
		TSharedPtr<FJsonObject> Input = MakeShared<FJsonObject>();
		if (Arguments.IsValid())
		{
			Arguments->TryGetStringField(TEXT("tool_name"), ToolName);
			const TSharedPtr<FJsonObject>* InputObject = nullptr;
			if (Arguments->TryGetObjectField(TEXT("input"), InputObject) && InputObject && InputObject->IsValid()) { Input = *InputObject; }
		}
		auto Answer = [Id, Input, OnComplete](bool bAllow, const FString& Message)
		{
			const TSharedRef<FJsonObject> Decision = MakeShared<FJsonObject>();
			Decision->SetStringField(TEXT("behavior"), bAllow ? TEXT("allow") : TEXT("deny"));
			if (bAllow) { Decision->SetObjectField(TEXT("updatedInput"), Input); }
			else { Decision->SetStringField(TEXT("message"), Message); }
			OnComplete(ToolTextResult(Id, Serialize(Decision), false));
		};
		const TSharedPtr<FAscentACFAssistant> Client = GetSharedACFAssistant(true);
		const FACFAssistantToolVerdict Verdict = Client->JudgeMutation(ConversationToken);
		if (Verdict.Verdict == EACFAssistantToolVerdict::Allow) { Answer(true, FString()); return; }
		if (Verdict.Verdict == EACFAssistantToolVerdict::Block || !Verdict.Session.IsValid()) { Answer(false, Verdict.Reason); return; }
		const TSharedRef<FJsonObject> Preview = MakeShared<FJsonObject>();
		Preview->SetStringField(TEXT("tool"), ToolName);
		Preview->SetStringField(TEXT("kind"), TEXT("claude-tool"));
		Preview->SetObjectField(TEXT("arguments"), Input);
		Client->RequestApproval(Verdict.Session, TEXT("claude-tool"), ToolName.IsEmpty() ? TEXT("tool") : ToolName, Preview, [Answer](bool bApproved)
		{
			Answer(bApproved, TEXT("The user denied this action in the Unreal Editor. Do not retry it; explain what you wanted to do and ask how to proceed."));
		});
	}

	// A mesh tool runs for minutes and has no image file to show, so its conversation gets a progress card for the length of the call and
	// a 3D viewer when the call ends. This sits in the gate because both providers' calls pass through it, with the conversation attached.
	// Every other tool is relayed unchanged.
	void ForwardTool(const FRelayRequest& Relay, const FHttpResultCallback& OnComplete, const TSharedPtr<FAscentACFAssistant>& Client,
		const TSharedPtr<FACFAssistantSession>& Session, const ACFAssistantToolPolicy::FToolCall& Call)
	{
		if (!ACFAssistantMesh::IsMeshGenerationTool(Call.Tool) || !Client.IsValid() || !Session.IsValid())
		{
			Forward(Relay, OnComplete);
			return;
		}
		const int32 Card = Client->BeginMeshCard(Session);
		Forward(Relay, OnComplete, nullptr, [Client, Session, Card](int32, const FString& Body)
		{
			const ACFAssistantMesh::FToolResult Result = ACFAssistantMesh::ParseToolResponse(Body);
			Client->FinishMeshCard(Session, Card, Result.bOk, Result.bOk ? Result.AssetPath : Result.Error);
		});
	}

	bool HandleToolCall(const TSharedPtr<FJsonObject>& Message, const FRelayRequest& Relay, const FString& ConversationToken, const FHttpResultCallback& OnComplete)
	{
		const TSharedPtr<FJsonValue> Id = Message->TryGetField(TEXT("id"));
		const TSharedPtr<FJsonObject>* ParamsPtr = nullptr;
		const TSharedPtr<FJsonObject> Params = Message->TryGetObjectField(TEXT("params"), ParamsPtr) && ParamsPtr ? *ParamsPtr : nullptr;
		FString Name;
		if (Params.IsValid()) { Params->TryGetStringField(TEXT("name"), Name); }
		const TSharedPtr<FJsonObject>* ArgumentsPtr = nullptr;
		const TSharedPtr<FJsonObject> Arguments = Params.IsValid() && Params->TryGetObjectField(TEXT("arguments"), ArgumentsPtr) && ArgumentsPtr ? *ArgumentsPtr : nullptr;
		if (Name.Equals(ACFAssistantToolGate::ClaudePermissionToolName, ESearchCase::IgnoreCase))
		{
			HandleClaudePermission(Id, Arguments, ConversationToken, OnComplete);
			return true;
		}
		const TSharedPtr<FAscentACFAssistant> Client = GetSharedACFAssistant(true);
		const FACFAssistantToolVerdict Verdict = Client->JudgeEditorTool(Name, Arguments, ConversationToken);
		const ACFAssistantToolPolicy::FToolCall Call = ACFAssistantToolPolicy::Unwrap(Name, Arguments);
		switch (Verdict.Verdict)
		{
		case EACFAssistantToolVerdict::Allow:
			ForwardTool(Relay, OnComplete, Client, Verdict.Session, Call);
			return true;
		case EACFAssistantToolVerdict::Block:
			UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin gate blocked %s: %s"), *Call.Label(), *Verdict.Reason);
			OnComplete(ToolTextResult(Id, Verdict.Reason, true));
			return true;
		case EACFAssistantToolVerdict::Ask:
		default:
			break;
		}
		if (!Verdict.Session.IsValid())
		{
			OnComplete(ToolTextResult(Id, TEXT("No conversation is available to approve this change."), true));
			return true;
		}
		const TSharedRef<FJsonObject> Preview = MakeShared<FJsonObject>();
		Preview->SetStringField(TEXT("tool"), Call.Label());
		Preview->SetStringField(TEXT("kind"), TEXT("editor-tool"));
		if (Call.Arguments.IsValid()) { Preview->SetObjectField(TEXT("arguments"), Call.Arguments); }
		Client->RequestApproval(Verdict.Session, TEXT("editor-tool"), Call.Label(), Preview, [Relay, Id, OnComplete, Client, Session = Verdict.Session, Call](bool bApproved)
		{
			if (bApproved)
			{
				ForwardTool(Relay, OnComplete, Client, Session, Call);
				return;
			}
			OnComplete(ToolTextResult(Id,
				TEXT("The user denied this change in the Unreal Editor. Do not retry it; describe what you intended and ask how to proceed."), true));
		}, Verdict.bAlwaysAsk);
		return true;
	}

	bool HandlePost(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
	{
		if (Request.Body.Num() > MaxBodyBytes)
		{
			OnComplete(JsonRpcError(nullptr, -32600, TEXT("Request is too large."), EHttpServerResponseCodes::BadRequest));
			return true;
		}
		const FRelayRequest Relay = Capture(Request);
		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
		const FString Text(Converted.Length(), Converted.Get());
		TSharedPtr<FJsonValue> Parsed;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Parsed) || !Parsed.IsValid())
		{
			OnComplete(JsonRpcError(nullptr, -32700, TEXT("Malformed JSON-RPC message."), EHttpServerResponseCodes::BadRequest));
			return true;
		}
		if (Parsed->Type != EJson::Object)
		{
			// A batch could smuggle a tools/call past the gate, so batches are refused outright. Neither CLI sends them.
			OnComplete(JsonRpcError(nullptr, -32600, TEXT("Batch requests are not accepted by Yarin's gate."), EHttpServerResponseCodes::BadRequest));
			return true;
		}
		const TSharedPtr<FJsonObject> Message = Parsed->AsObject();
		FString Method;
		Message->TryGetStringField(TEXT("method"), Method);
		const FString* TokenPtr = Request.QueryParams.Find(ACFAssistantToolGate::ConversationQueryKey);
		const FString ConversationToken = TokenPtr ? *TokenPtr : FString();
		if (Method == TEXT("tools/call"))
		{
			return HandleToolCall(Message, Relay, ConversationToken, OnComplete);
		}
		if (Method == TEXT("tools/list"))
		{
			const TSharedPtr<FAscentACFAssistant> Client = GetSharedACFAssistant(true);
			TFunction<void(TSharedPtr<FJsonObject>&)> PostProcess;
			if (Client.IsValid() && Client->IsAnyBusy(EACFAssistantProvider::ClaudeCode))
			{
				// Claude Code resolves --permission-prompt-tool against the server's tool list, so the gate advertises its own tool there.
				PostProcess = [](TSharedPtr<FJsonObject>& Root)
				{
					const TSharedPtr<FJsonObject>* ResultPtr = nullptr;
					if (!Root->TryGetObjectField(TEXT("result"), ResultPtr) || !ResultPtr || !ResultPtr->IsValid()) { return; }
					const TArray<TSharedPtr<FJsonValue>>* Tools = nullptr;
					TArray<TSharedPtr<FJsonValue>> Extended;
					if ((*ResultPtr)->TryGetArrayField(TEXT("tools"), Tools) && Tools) { Extended = *Tools; }
					Extended.Add(MakeShared<FJsonValueObject>(ClaudePermissionToolDefinition()));
					(*ResultPtr)->SetArrayField(TEXT("tools"), Extended);
				};
			}
			Forward(Relay, OnComplete, PostProcess);
			return true;
		}
		Forward(Relay, OnComplete);
		return true;
	}

	bool HandleGet(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
	{
		// No server-initiated stream through the gate: notifications/tools/list_changed is not needed by the CLIs, and 405 is the
		// spec's way of saying so.
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(FString(), TEXT("text/plain"));
		Response->Code = EHttpServerResponseCodes::BadMethod;
		Response->Headers.Add(TEXT("Allow"), {TEXT("POST, DELETE")});
		OnComplete(MoveTemp(Response));
		return true;
	}

	bool HandleDelete(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
	{
		Forward(Capture(Request), OnComplete);
		return true;
	}
}

namespace ACFAssistantToolGate
{
	bool IsRunning()
	{
		return Router.IsValid() && GatePort != 0;
	}

	FString Url()
	{
		return FString::Printf(TEXT("http://127.0.0.1:%u%s"), GatePort, GatePath);
	}

	bool Ensure()
	{
		check(IsInGameThread());
		const uint32 Upstream = UpstreamPort();
		if (Upstream == 0)
		{
			UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin gate cannot start before the Editor tool server."));
			return false;
		}
		// The Editor tool server probes McpServerPort..+9; the gate takes the next block so a busy port never lands on the wrong service.
		const uint32 FirstPort = Upstream - (Upstream % PortAttempts) + PortAttempts * 2;
		if (IsRunning() && GatePort >= FirstPort && GatePort < FirstPort + PortAttempts) { return true; }
		Shutdown();
		for (uint32 Port = FirstPort; Port < FirstPort + PortAttempts && Port <= 65535; ++Port)
		{
			// Loopback only, exactly like the Editor tool server it fronts.
			TArray<FString> Overrides;
			GConfig->GetArray(TEXT("HTTPServer.Listeners"), TEXT("ListenerOverrides"), Overrides, GEngineIni);
			const FString Override = FString::Printf(TEXT("(Port=%u,BindAddress=127.0.0.1,ReuseAddressAndPort=false)"), Port);
			if (!Overrides.Contains(Override))
			{
				Overrides.Insert(Override, 0);
				GConfig->SetArray(TEXT("HTTPServer.Listeners"), TEXT("ListenerOverrides"), Overrides, GEngineIni);
			}
			TSharedPtr<IHttpRouter> Candidate = FHttpServerModule::Get().GetHttpRouter(Port, true);
			if (!Candidate.IsValid()) { continue; }
			const FHttpPath Path(GatePath);
			PostRoute = Candidate->BindRoute(Path, EHttpServerRequestVerbs::VERB_POST, FHttpRequestHandler::CreateStatic(&HandlePost));
			GetRoute = Candidate->BindRoute(Path, EHttpServerRequestVerbs::VERB_GET, FHttpRequestHandler::CreateStatic(&HandleGet));
			DeleteRoute = Candidate->BindRoute(Path, EHttpServerRequestVerbs::VERB_DELETE, FHttpRequestHandler::CreateStatic(&HandleDelete));
			if (!PostRoute.IsValid())
			{
				if (GetRoute.IsValid()) { Candidate->UnbindRoute(GetRoute); }
				if (DeleteRoute.IsValid()) { Candidate->UnbindRoute(DeleteRoute); }
				continue;
			}
			Router = Candidate;
			GatePort = Port;
			FHttpServerModule::Get().StartAllListeners();
			UE_LOG(LogAscentACFAssistant, Log, TEXT("Yarin gate listening at %s, relaying to %s"), *Url(), *AscentEpicMCP::Url());
			return true;
		}
		UE_LOG(LogAscentACFAssistant, Warning, TEXT("Yarin gate could not bind any port from %u to %u."), FirstPort, FirstPort + PortAttempts - 1);
		return false;
	}

	void Shutdown()
	{
		for (const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request : InFlight)
		{
			if (Request.IsValid()) { Request->CancelRequest(); }
		}
		InFlight.Reset();
		if (Router.IsValid())
		{
			if (PostRoute.IsValid()) { Router->UnbindRoute(PostRoute); }
			if (GetRoute.IsValid()) { Router->UnbindRoute(GetRoute); }
			if (DeleteRoute.IsValid()) { Router->UnbindRoute(DeleteRoute); }
		}
		PostRoute = FHttpRouteHandle();
		GetRoute = FHttpRouteHandle();
		DeleteRoute = FHttpRouteHandle();
		Router.Reset();
		GatePort = 0;
	}
}
