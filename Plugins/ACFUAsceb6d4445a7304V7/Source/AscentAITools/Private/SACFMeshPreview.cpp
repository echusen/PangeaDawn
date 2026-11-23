// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "SACFMeshPreview.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureCube.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "SEditorViewport.h"
#include "Slate/SceneViewport.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ACFMeshPreview"

namespace
{
	/** Degrees the mesh (turntable) turns every second while it is on screen. */
	constexpr float TurntableDegreesPerSecond = 18.0f;

	/** Live previews stop turning on their own after this many seconds until the user interacts again. */
	constexpr float TurntableBudgetSeconds = 20.0f;

	/** Turntable redraw cadence. The active timer runs at this period, so an animating preview is never redrawn faster than this. */
	constexpr float RedrawIntervalSeconds = 1.0f / 30.0f;

	/** After the user takes over, redraws keep following the camera for this long after the last input stops. */
	constexpr float InteractionGraceSeconds = 0.6f;

	/** Fraction of the mesh bounding sphere radius kept as the near plane so it never clips the mesh. */
	constexpr float NearClipRadiusFraction = 0.05f;

	constexpr float NearClipMin = 0.05f;

	constexpr float NearClipMax = 10.0f;

	/** Orbit pitch clamps so dragging cannot flip the camera upside down or to a top down view. */
	constexpr float OrbitPitchMinDegrees = -85.0f;
	constexpr float OrbitPitchMaxDegrees = 85.0f;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// FACFMeshPreviewViewportClient
//
// FEditorViewportClient orbit client. The camera never leaves orbit mode and the client stays non
// realtime for its whole life: redraws are driven explicitly through Invalidate (bNeedsRedraw), so a
// turntable cannot race ahead of the 30 fps redraw cadence decided by the widget's active timer.
///////////////////////////////////////////////////////////////////////////////////////////////////

class FACFMeshPreviewViewportClient : public FEditorViewportClient
{
public:
	FACFMeshPreviewViewportClient(FPreviewScene* InPreviewScene, const TSharedRef<SEditorViewport>& InViewport)
		: FEditorViewportClient(nullptr, InPreviewScene, InViewport)
	{
		SetRealtime(false);
		EngineShowFlags.SetGrid(false);
		bDrawAxes = false;
		bSetListenerPosition = false;

		// Small preview preset: horizontal FOV, aspect ratio and near plane known before the first draw.
		ViewFOV = 60.0f;
	}

	/** Frames an orbit camera around the mesh bounds centre using the view FOV and aspect ratio. */
	void FrameMesh(const FBoxSphereBounds& Bounds, const FVector2D& InViewportSize)
	{
		const FVector Center = Bounds.Origin;
		const float Radius = FMath::Max(Bounds.SphereRadius, 1.0f);
		MinOrbitDistance = Radius * 1.2f;
		MaxOrbitDistance = Radius * 6.0f;

		// The narrower of the horizontal or vertical half FOV
		// decides the framing distance: for a wide aspect the
		// vertical half angle is the limiting one.
		const float Aspect = InViewportSize.Y > 0.0f ? InViewportSize.X / InViewportSize.Y : 1.0f;
		const float HalfHorizontal = FMath::DegreesToRadians(ViewFOV * 0.5f);
		const float HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / FMath::Max(Aspect, 0.25f));
		const float HalfLimiting = FMath::Min(HalfHorizontal, HalfVertical);

		const float Distance = FMath::Max(Radius * 1.05f / FMath::Tan(HalfLimiting), Radius * 1.5f);

		const FVector CameraDirection = FVector(0.62f, 0.63f, 0.45f).GetSafeNormal();
		const FVector CameraLocation = Center + Distance * CameraDirection;
		const FRotator LookRotation = FRotationMatrix::MakeFromX(Center - CameraLocation).Rotator();

		SetViewLocation(CameraLocation);
		SetViewRotation(LookRotation);
		SetLookAtLocation(Center);

		// Enter orbit mode with the pivot pinned on the bounds centre. Drag to orbit and mouse wheel zoom
		// are built into the orbit camera; both redraw paths run through the widget's Invalidate cadence.
		ToggleOrbitCamera(true);

		// The framing distance sits between the two extremes, and the clamps keep it there afterwards.
		OverrideNearClipPlane(FMath::Clamp(Radius * NearClipRadiusFraction, NearClipMin, NearClipMax));
		EnforceOrbitClamps();
	}

	/**
	 * Wheel dolly for one notch: moves the camera by ten percent of the current orbit distance along the
	 * look at axis and clamps the result to [MinOrbitDistance, MaxOrbitDistance]. The pivot stays fixed.
	 */
	void DollyOrbitDistance(float InScale)
	{
		const FVector Center = GetLookAtLocation();
		const FVector Ray = GetViewLocation() - Center;
		const double Distance = FMath::Clamp<double>(Ray.Length() * InScale, MinOrbitDistance, MaxOrbitDistance);
		SetViewLocation(Center + Ray.GetSafeNormal() * Distance);
		Invalidate();
	}

	/** Orbit distance range and vertical clamps for this preview, all derived from the bounds sphere. */
	void EnforceOrbitClamps()
	{
		if (MinOrbitDistance <= 0.0f || MaxOrbitDistance <= 0.0f)
		{
			return;
		}

		// Drag must never flip the camera upside down.
		const FRotator Rotation = GetViewRotation();
		FRotator ClampedRotation = Rotation;
		ClampedRotation.Pitch = FMath::Clamp(Rotation.Pitch, OrbitPitchMinDegrees, OrbitPitchMaxDegrees);
		if (ClampedRotation != Rotation)
		{
			SetViewRotation(ClampedRotation);
		}

		const FVector Center = GetLookAtLocation();
		const FVector Ray = GetViewLocation() - Center;
		const double Distance = Ray.Length();
		const double ClampedDistance = FMath::Clamp<double>(Distance, MinOrbitDistance, MaxOrbitDistance);
		if (ClampedDistance != Distance)
		{
			SetViewLocation(Center + Ray.GetSafeNormal() * ClampedDistance);
		}
	}

	/** Sets a per viewport camera speed that suits the size of the preview, since editor defaults assume level sized scenes. */
	void SetCameraSpeedForRadius(float Radius)
	{
		// Orbit wheel zooming (OnDollyPerspectiveCamera) moves at cameraSpeed * 32 per notch, so derive the
		// speed from the mesh size rather than using the editor default.
		const float Speed = FMath::Clamp(Radius / 40.0f, 0.01f, 1.0f);
		SetCameraSpeedSettings(FEditorViewportCameraSpeedSettings(Speed));
	}

	/** Bound by the viewport widget: user contact routes into the turntable/state and the widget invalidate. */
	TUniqueFunction<void()> OnUserContact;

	virtual bool InputKey(const FInputKeyEventArgs& InEventArgs) override
	{
		const FKey& Key = InEventArgs.Key;

		if (Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown)
		{
			// One dolly per notch (the scene viewport raises pressed and released for every scroll), at a
			// gentle ten percent of the current distance instead of the editor's fixed camera speed step.
			if (InEventArgs.Event == IE_Pressed)
			{
				DollyOrbitDistance(Key == EKeys::MouseScrollUp ? 0.90f : 1.10f);
				NotifyUserInput(Key);
			}
			return true;
		}

		const bool bHandled = FEditorViewportClient::InputKey(InEventArgs);
		NotifyUserInput(Key);
		return bHandled;
	}

	virtual bool InputAxis(const FInputKeyEventArgs& InArgs) override
	{
		// The wheel axis event is swallowed: the dolly runs through the scroll key above.
		if (InArgs.Key == EKeys::MouseWheelAxis)
		{
			return true;
		}

		const bool bHandled = FEditorViewportClient::InputAxis(InArgs);
		NotifyUserInput(InArgs.Key);
		return bHandled;
	}

	/** Keeps drag orbit camera locked in pitch steps and drags inside the orbit distance clamps. */
	virtual bool ShouldLockPitch() const override
	{
		return true;
	}

	/** Capture only while a button is held: after release the widget level handles click to engage state. */
	virtual EMouseCaptureMode GetMouseCaptureMode() const override
	{
		return EMouseCaptureMode::CaptureDuringMouseDown;
	}

	virtual void Tick(float DeltaSeconds) override
	{
		FEditorViewportClient::Tick(DeltaSeconds);
		EnforceOrbitClamps();
	}

	/** Reports user contact through the viewport client: movement, buttons, wheel, modifiers or gamepad. */
	void NotifyUserInput(FKey Key)
	{
		if (Key == EKeys::MouseX || Key == EKeys::MouseY || Key == EKeys::MouseScrollUp || Key == EKeys::MouseScrollDown ||
			Key.IsMouseButton() || Key.IsGamepadKey() || Key.IsModifierKey())
		{
			if (OnUserContact)
			{
				OnUserContact();
			}
		}
	}

	/** Whether the user is currently pushing the camera (button held) or it is still moving after input. */
	virtual bool IsInteracting() const
	{
		const bool bButtonHeld =
			Viewport != nullptr &&
			(Viewport->KeyState(EKeys::LeftMouseButton) || Viewport->KeyState(EKeys::MiddleMouseButton) || Viewport->KeyState(EKeys::RightMouseButton));
		return bButtonHeld || IsMovingCamera();
	}

private:
	/** Orbit distance clamps derived from the mesh bounds sphere, set by FrameMesh. */
	double MinOrbitDistance = 0.0;
	double MaxOrbitDistance = 0.0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// SACFMeshPreview::FImpl
//
// Holds the scene, the mesh components and the turntable budget. One static weak pointer keeps at
// most one preview live: becoming the live preview (construction or hover) freezes the previous
// one, which keeps its last frame because redraws only happen through explicit Invalidate calls.
///////////////////////////////////////////////////////////////////////////////////////////////////

class SACFMeshPreview::FImpl : public TSharedFromThis<FImpl>
{
public:
	~FImpl()
	{
		// The live slot dies with its preview so no other preview is ever quietly driven by a dead one.
		if (IsLive())
		{
			LivePreview.Reset();
		}
		ClearScene();
	}

	/** Builds the whole preview: scene, components, bounds. False switches the widget to the notice. */
	bool Initialize(UObject* InMesh)
	{
		bSupported = false;

		StaticMesh.Reset(Cast<UStaticMesh>(InMesh));
		SkeletalMesh.Reset(Cast<USkeletalMesh>(InMesh));
		if (StaticMesh.IsValid() == false && SkeletalMesh.IsValid() == false)
		{
			return false;
		}

		FPreviewScene::ConstructionValues CVS;
		CVS.SetCreateDefaultLighting(false);
		CVS.SetCreatePhysicsScene(false);
		CVS.SetForceMipsResident(true);
		CVS.AllowAudioPlayback(false);
		CVS.ShouldSimulatePhysics(false);

		// A plain FPreviewScene with our own static lighting rig: no Asset Viewer coupling and no global
		// user state, so no other editor panel can ever reach in here and change the floor or the lights.
		Scene = MakeUnique<FPreviewScene>(CVS);
		if (Scene->IsInitialized() == false)
		{
			Scene.Reset();
			return false;
		}

		if (!BuildLightingRig())
		{
			Scene.Reset();
			return false;
		}

		// Local strong references keep the unregistered components garbage collectable until registered
		// into the preview scene (AddComponent takes a scene reference through its Components array).
		TStrongObjectPtr<USceneComponent> PivotRef(NewObject<USceneComponent>(GetTransientPackage(), TEXT("ACFMeshPreviewPivot")));
		PivotRef->SetMobility(EComponentMobility::Movable);
		Scene->AddComponent(PivotRef.Get(), FTransform::Identity);

		FBoxSphereBounds Bounds(ForceInit);
		if (StaticMesh.IsValid())
		{
			TStrongObjectPtr<UStaticMeshComponent> GeometryRef(NewObject<UStaticMeshComponent>(GetTransientPackage(), TEXT("ACFMeshPreviewStaticMesh")));
			GeometryRef->SetMobility(EComponentMobility::Movable);
			GeometryRef->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (GeometryRef->SetStaticMesh(StaticMesh.Get()) == false)
			{
				ClearScene();
				return false;
			}

			Bounds = StaticMesh.Get()->GetBounds();
			GeometryRef->SetupAttachment(PivotRef.Get());
			GeometryRef->SetRelativeLocation(-Bounds.Origin);
			Scene->AddComponent(GeometryRef.Get(), GeometryRef.Get()->GetRelativeTransform());

			Pivot = PivotRef.Get();
			StaticGeometry = GeometryRef.Get();
		}
		else
		{
			TStrongObjectPtr<USkeletalMeshComponent> GeometryRef(NewObject<USkeletalMeshComponent>(GetTransientPackage(), TEXT("ACFMeshPreviewSkeletalMesh")));
			GeometryRef->SetMobility(EComponentMobility::Movable);
			GeometryRef->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GeometryRef->SetSkeletalMesh(SkeletalMesh.Get(), true);

			Bounds = SkeletalMesh.Get()->GetBounds();
			GeometryRef->SetupAttachment(PivotRef.Get());
			GeometryRef->SetRelativeLocation(-Bounds.Origin);
			Scene->AddComponent(GeometryRef.Get(), GeometryRef.Get()->GetRelativeTransform());

			Pivot = PivotRef.Get();
			SkeletalGeometry = GeometryRef.Get();
		}

		// The mesh hangs from the pivot so its bounds centre sits on the orbit look at point.
		MeshBounds = Bounds;
		bSupported = true;
		TurntableSecondsRemaining = TurntableBudgetSeconds;
		return true;
	}

	/** Wiring from the Slate construction pass: the client is created by the editor viewport widget. */
	void BindViewportClient(TSharedPtr<FACFMeshPreviewViewportClient> InClient, const FVector2D& InViewportSize)
	{
		Client = MoveTemp(InClient);
		if (Client.IsValid())
		{
			Client->FrameMesh(MeshBounds, InViewportSize);
			Client->SetCameraSpeedForRadius(MeshBounds.SphereRadius);
		}
	}

	/** Becomes the live preview. The previous live preview, if any, is frozen and keeps its last frame. */
	void AcquireLive()
	{
		const TSharedPtr<FImpl> Previous = LivePreview.Pin();
		if (Previous.IsValid() && Previous.Get() != this)
		{
			Previous->Freeze();
		}

		LivePreview = SharedThis(this);
		TurntableSecondsRemaining = TurntableBudgetSeconds;
		bUserTookOver = false;
	}

	bool IsLive() const
	{
		return LivePreview.Pin().Get() == this;
	}

	bool IsSupported() const
	{
		return bSupported;
	}

	FPreviewScene* GetScene() const
	{
		return Scene.Get();
	}

	/** Whether the preview wants Slate to keep its repaint timer running right now. */
	bool WantsFrame() const
	{
		if (bSupported == false)
		{
			return false;
		}
		if (bUserTookOver)
		{
			// Camera driven: keep the redraw cadence while buttons are held or the camera still moves.
			return Client.IsValid() && (Client->IsInteracting() || InteractionGrace > 0.0f);
		}
		return TurntableSecondsRemaining > 0.0f;
	}

	/**
	 * Advances the preview one pump (called from the 30 fps active timer). Returns true while the timer
	 * must keep running; false stops the timer, leaving the last rendered frame on screen.
	 */
	bool Pump(float DeltaTime, bool bOnScreen)
	{
		if (IsLive() == false || bOnScreen == false)
		{
			return false;
		}

		if (InteractionGrace > 0.0f)
		{
			InteractionGrace = FMath::Max(InteractionGrace - DeltaTime, 0.0f);
		}

		// User took over: the mesh stops turning, but the camera keeps following drag and zoom input.
		if (bUserTookOver)
		{
			const bool bMoving = Client.IsValid() && Client->IsInteracting();
			if (bMoving == false && InteractionGrace <= 0.0f)
			{
				return false;
			}

			if (Client.IsValid())
			{
				Client->Invalidate();
			}
			return true;
		}

		// Turntable runs on a 20 second budget, then it freezes until the user interacts again.
		TurntableSecondsRemaining -= DeltaTime;
		if (TurntableSecondsRemaining <= 0.0f)
		{
			return false;
		}

		YawDegrees = FMath::Fmod(YawDegrees + TurntableDegreesPerSecond * DeltaTime, 360.0f);
		if (Pivot != nullptr)
		{
			Pivot->SetWorldRotation(FRotator(0.0f, YawDegrees, 0.0f));
		}

		if (Client.IsValid())
		{
			Client->Invalidate();
		}
		return true;
	}

	/** First user contact ends the automatic rotation and opens a short redraw grace for camera follow. */
	void OnUserInput()
	{
		bUserTookOver = true;
		InteractionGrace = InteractionGraceSeconds;
	}

	void Freeze()
	{
		TurntableSecondsRemaining = 0.0f;
	}

private:
	/**
	 * Own studio lighting for the plain preview scene, matching the stock Asset Viewer default: one white key light with
	 * shadows, a sky light driven by the neutral studio panorama, the inverse normal sky sphere as a blurred backdrop, and
	 * auto exposure clamped to the same -1..1 range so a dim scene never brightens into white. A single directional light
	 * is deliberate: two of them tie for forward shading priority and the renderer prints a warning into the viewport.
	 * No floor, no Asset Viewer profile coupling; these components are ours alone.
	 */
	bool BuildLightingRig()
	{
		TStrongObjectPtr<UDirectionalLightComponent> KeyLightRef(NewObject<UDirectionalLightComponent>(GetTransientPackage(), TEXT("ACFMeshPreviewKeyLight")));
		KeyLightRef->SetMobility(EComponentMobility::Movable);
		KeyLightRef->Intensity = 1.0f;
		KeyLightRef->LightColor = FColor::White;
		KeyLightRef->bTransmission = true;
		KeyLightRef->ForwardShadingPriority = 1;
		Scene->AddComponent(KeyLightRef.Get(), FTransform(FRotator(-40.0f, -67.5f, 0.0f)));

		// The same panorama the Asset Viewer default profile uses: neutral, soft, and it reads well on every material.
		TStrongObjectPtr<UTextureCube> AmbientCubemapRef(LoadObject<UTextureCube>(
			nullptr, TEXT("/Engine/EditorMaterials/AssetViewer/EpicQuadPanorama_CC+EV1.EpicQuadPanorama_CC+EV1")));
		if (AmbientCubemapRef.IsValid() == false)
		{
			return false;
		}

		TStrongObjectPtr<USkyLightComponent> SkyLightRef(NewObject<USkyLightComponent>(GetTransientPackage(), TEXT("ACFMeshPreviewSkyLight")));
		SkyLightRef->SetMobility(EComponentMobility::Movable);
		// Light from the cubemap itself. The default source captures the whole scene, which costs GPU time and, until every mesh
		// and texture has streamed in, makes the renderer print "Sky Light waiting on Meshes, Textures" into the viewport.
		SkyLightRef->SourceType = SLS_SpecifiedCubemap;
		SkyLightRef->SetCubemap(AmbientCubemapRef.Get());
		SkyLightRef->SetIntensity(1.0f);
		Scene->AddComponent(SkyLightRef.Get(), FTransform::Identity);

		// Same exposure clamp as the Asset Viewer default profile ("stops the scene from becoming bright due to exposure").
		TStrongObjectPtr<UPostProcessComponent> ExposureRef(NewObject<UPostProcessComponent>(GetTransientPackage(), TEXT("ACFMeshPreviewExposure")));
		ExposureRef->bUnbound = true;
		ExposureRef->Settings.bOverride_AutoExposureMinBrightness = true;
		ExposureRef->Settings.AutoExposureMinBrightness = -1.0f;
		ExposureRef->Settings.bOverride_AutoExposureMaxBrightness = true;
		ExposureRef->Settings.AutoExposureMaxBrightness = 1.0f;
		Scene->AddComponent(ExposureRef.Get(), FTransform::Identity);

		// The blurred environment sphere: engine sky sphere mesh with the engine sky box material and the
		// same texture parameter names the asset viewer environment uses. No floor component exists here.
		TStrongObjectPtr<UStaticMesh> SkySphereMeshRef(LoadObject<UStaticMesh>(
			nullptr, TEXT("/Engine/EditorMeshes/AssetViewer/Sphere_inversenormals.Sphere_inversenormals")));
		TStrongObjectPtr<UMaterial> SkyMaterialRef(LoadObject<UMaterial>(
			nullptr, TEXT("/Engine/EditorMaterials/AssetViewer/M_SkyBox.M_SkyBox")));
		if (SkySphereMeshRef.IsValid() == false || SkyMaterialRef.IsValid() == false)
		{
			return false;
		}

		TStrongObjectPtr<UStaticMeshComponent> SkySphereRef(NewObject<UStaticMeshComponent>(GetTransientPackage(), TEXT("ACFMeshPreviewSkySphere")));
		SkySphereRef->SetMobility(EComponentMobility::Movable);
		SkySphereRef->SetStaticMesh(SkySphereMeshRef.Get());
		SkySphereRef->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SkySphereRef->CastShadow = false;
		SkySphereRef->bCastDynamicShadow = false;

		TStrongObjectPtr<UMaterialInstanceConstant> SkyMaterialInstanceRef(NewObject<UMaterialInstanceConstant>(GetTransientPackage()));
		SkyMaterialInstanceRef->Parent = SkyMaterialRef.Get();
		SkyMaterialInstanceRef->SetTextureParameterValueEditorOnly(FName("SkyBox"), AmbientCubemapRef.Get());
		SkyMaterialInstanceRef->SetScalarParameterValueEditorOnly(FName("CubemapRotation"), 0.0f);
		SkyMaterialInstanceRef->SetScalarParameterValueEditorOnly(FName("Intensity"), 1.0f);
		SkyMaterialInstanceRef->PostLoad();
		SkySphereRef->SetMaterial(0, SkyMaterialInstanceRef.Get());

		// Large scale so the mesh, camera and lights stay well inside the sphere.
		Scene->AddComponent(SkySphereRef.Get(), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(2000.0f)));

		// Flush the sky capture once so the sky light is live on the first drawn frame.
		Scene->UpdateCaptureContents();
		return true;
	}

	void ClearScene()
	{
		Scene.Reset();
		Pivot = nullptr;
		StaticGeometry = nullptr;
		SkeletalGeometry = nullptr;
		bSupported = false;
	}

	// The one live preview in the process.
	static TWeakPtr<FImpl> LivePreview;

	// Mesh strong while the preview widget lives, none afterwards.
	TStrongObjectPtr<UStaticMesh> StaticMesh;
	TStrongObjectPtr<USkeletalMesh> SkeletalMesh;

	TUniquePtr<FPreviewScene> Scene;
	USceneComponent* Pivot = nullptr;
	UStaticMeshComponent* StaticGeometry = nullptr;
	USkeletalMeshComponent* SkeletalGeometry = nullptr;

	FBoxSphereBounds MeshBounds = FBoxSphereBounds(ForceInit);

	// Shared reference is cycle free: the widget only holds weak pointers back here.
	TSharedPtr<FACFMeshPreviewViewportClient> Client;

	bool bSupported = false;
	bool bUserTookOver = false;
	float TurntableSecondsRemaining = 0.0f;
	float InteractionGrace = 0.0f;
	float YawDegrees = 0.0f;
};

TWeakPtr<SACFMeshPreview::FImpl> SACFMeshPreview::FImpl::LivePreview;

///////////////////////////////////////////////////////////////////////////////////////////////////
// SACFMeshPreviewViewportWidget
//
// SEditorViewport child. Tick runs the hover and visibility (paint geometry against the window rect)
// checks. An active timer at the turntable cadence drives redraws; it removes itself once the preview
// is frozen, off screen or done, so an idle chat has no timer, no tick and no GPU work.
///////////////////////////////////////////////////////////////////////////////////////////////////

class SACFMeshPreviewViewportWidget final : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SACFMeshPreviewViewportWidget)
		: _ViewportSize(FVector2D(360.0f, 270.0f))
		{
		}
		SLATE_ARGUMENT(FVector2D, ViewportSize)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<SACFMeshPreview::FImpl>& InImpl)
	{
		Impl = InImpl;
		ViewportSize = InArgs._ViewportSize;

		SEditorViewport::Construct(SEditorViewport::FArguments().ViewportSize(InArgs._ViewportSize));

		// Wheel zoom engages on the first click inside; tell the user so scrolling stays obvious.
		SetToolTipText(LOCTEXT("MeshPreviewClickToRotate", "Click to rotate and zoom"));

		// Click to engage: while unengaged the inner engine SViewport is hit test invisible, so the event
		// path ends on this outer widget. Wheel presses then return Unhandled and bubble into the chat
		// scroll box; a mouse button press here switches the inner viewport back to Visible so it becomes
		// the hit leaf and receives the drag orbit and wheel zoom through the normal engine path again.
		bEngaged = false;
		if (ViewportWidget.IsValid())
		{
			const TWeakPtr<SACFMeshPreviewViewportWidget> WeakWidget = SharedThis(this);
			ViewportWidget->SetVisibility(TAttribute<EVisibility>::CreateLambda([WeakWidget]()
			{
				const TSharedPtr<SACFMeshPreviewViewportWidget> PinnedWidget = WeakWidget.Pin();
				return PinnedWidget.IsValid() && PinnedWidget->bEngaged ? EVisibility::Visible : EVisibility::HitTestInvisible;
			}));
		}

		// First claim: this preview is live from construction until another one hovers or is built.
		if (TSharedPtr<SACFMeshPreview::FImpl> PinnedImpl = Impl.Pin())
		{
			PinnedImpl->AcquireLive();
			WakeAnimation();
		}
	}

	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
	{
		const TSharedPtr<SACFMeshPreview::FImpl> PinnedImpl = Impl.Pin();

		Client = MakeShared<FACFMeshPreviewViewportClient>(
			PinnedImpl.IsValid() ? PinnedImpl->GetScene() : nullptr,
			SharedThis(this));

		if (PinnedImpl.IsValid())
		{
			PinnedImpl->BindViewportClient(Client, ViewportSize);
		}

		// User contact: hand over to the impl state, redraw the scene and the widget surface.
		const TWeakPtr<SACFMeshPreview::FImpl> ImplWeak = Impl;
		const TWeakPtr<FACFMeshPreviewViewportClient> ClientWeak = Client;
		Client->OnUserContact = TUniqueFunction<void()>([ImplWeak, ClientWeak, Widget = TWeakPtr<SEditorViewport>(SharedThis(this))]()
		{
			if (const TSharedPtr<SACFMeshPreview::FImpl> PinnedImpl = ImplWeak.Pin())
			{
				PinnedImpl->OnUserInput();
			}
			if (const TSharedPtr<FACFMeshPreviewViewportClient> PinnedClient = ClientWeak.Pin())
			{
				PinnedClient->Invalidate();
			}
			if (const TSharedPtr<SEditorViewport> PinnedWidget = Widget.Pin())
			{
				// Public SEditorViewport invalidation: one extra Slate tick so the surface repaints.
				PinnedWidget->Invalidate();
			}
		});

		return Client.ToSharedRef();
	}

	/** Starts (or keeps) the active timer that drives the turntable pump at the redraw cadence. */
	void WakeAnimation()
	{
		if (AnimationTimerHandle.IsValid())
		{
			return;
		}
		LastPumpTime = 0.0;
		AnimationTimerHandle = RegisterActiveTimer(
			RedrawIntervalSeconds,
			FWidgetActiveTimerDelegate::CreateSP(this, &SACFMeshPreviewViewportWidget::OnAnimationActiveTimer));
	}

	/** Disengages wheel interaction until the next press inside (engine active only while engaged). */
	void ReleaseEngagement()
	{
		if (SceneViewport.IsValid() && SceneViewport->HasMouseCapture())
		{
			// A drag is in progress: Slate captures the mouse, and engagement survives until it releases.
			return;
		}
		bEngaged = false;
	}

private:
	/**
	 * Hit leaf while unengaged (the inner SViewport is HitTestInvisible): the unhandled wheel bubbles out
	 * of this widget and the parent SScrollBox scrolls the thread.
	 */
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Unhandled();
	}

	/** The engaging click: any mouse button press inside switches the inner engine viewport back on. */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bEngaged == false && MouseEvent.GetEffectingButton().IsMouseButton())
		{
			bEngaged = true;

			// A press counts as user contact for the turntable, even before the engine takes over.
			if (const TSharedPtr<SACFMeshPreview::FImpl> PinnedImpl = Impl.Pin())
			{
				PinnedImpl->OnUserInput();
			}
			if (const TSharedPtr<FACFMeshPreviewViewportClient> PinnedClient = Client)
			{
				PinnedClient->Invalidate();
			}
			return FReply::Handled();
		}
		return SEditorViewport::OnMouseButtonDown(MyGeometry, MouseEvent);
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		ReleaseEngagement();
		SEditorViewport::OnMouseLeave(MouseEvent);
	}

	/** Active timer: pumps at the turntable cadence, stops itself once nothing animates anymore. */
	EActiveTimerReturnType OnAnimationActiveTimer(double InCurrentTime, float InDeltaTime)
	{
		const TSharedPtr<SACFMeshPreview::FImpl> PinnedImpl = Impl.Pin();
		if (PinnedImpl.IsValid() == false)
		{
			return EActiveTimerReturnType::Stop;
		}

		// Slate hands every active timer the delta of the whole frame, not the time since the timer last ran. The editor draws at
		// 60 fps or more while this timer runs at 30, so the frame delta would burn the 20 second turntable budget in about 40 and
		// spin the mesh at half speed. The timer clock gives the real elapsed time.
		const float Elapsed = LastPumpTime > 0.0 ? FMath::Clamp(static_cast<float>(InCurrentTime - LastPumpTime), 0.0f, 0.25f) : RedrawIntervalSeconds;
		LastPumpTime = InCurrentTime;

		const bool bOnScreen = IsOnScreenAllotted();
		const bool bContinue = PinnedImpl->Pump(Elapsed, bOnScreen);
		if (bContinue == false)
		{
			LastPumpTime = 0.0;
		}
		return bContinue ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SEditorViewport::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		// Visibility checks run at most ~10 times per second on a normal repaint pass.
		if (InCurrentTime < NextVisibilityCheckTime)
		{
			return;
		}
		NextVisibilityCheckTime = InCurrentTime + 0.1;

		const TSharedPtr<SACFMeshPreview::FImpl> PinnedImpl = Impl.Pin();
		if (PinnedImpl.IsValid() == false)
		{
			return;
		}

		const bool bOnScreen = IsOnScreenAllotted();

		// Hover is a user gesture: it claims the live slot and revives the turntable budget.
		if (bOnScreen && IsHovered() && PinnedImpl->IsLive() == false)
		{
			PinnedImpl->AcquireLive();
			WakeAnimation();
		}
		else if (bOnScreen && PinnedImpl->IsLive() && PinnedImpl->WantsFrame())
		{
			// The pump expects frames and the timer is not running (e.g. re-armed after user input).
			WakeAnimation();
		}
	}

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		SEditorViewport::OnMouseEnter(MyGeometry, MouseEvent);

		if (const TSharedPtr<SACFMeshPreview::FImpl> PinnedImpl = Impl.Pin(); PinnedImpl.IsValid() && IsOnScreenAllotted())
		{
			PinnedImpl->AcquireLive();
			WakeAnimation();
		}
	}

	/**
	 * On screen test: a hidden widget, or geometry scrolled fully outside the containing window, is off
	 * screen. Missing layout geometry or no window means nothing paints anyway, so the widget is assumed
	 * on screen until a window can be resolved.
	 */
	bool IsOnScreenAllotted() const
	{
		if (GetVisibility().IsVisible() == false)
		{
			return false;
		}
		if (SceneViewport.IsValid() == false)
		{
			return true;
		}

		const TSharedPtr<SWindow> OwnerWindow = SceneViewport->FindWindow();
		if (OwnerWindow.IsValid() == false)
		{
			return true;
		}

		const FGeometry& WidgetGeometry = GetTickSpaceGeometry();
		if (WidgetGeometry.GetLocalSize().IsNearlyZero())
		{
			return true;
		}

		bool bOverlapping = false;
		const FSlateRect RenderRect = WidgetGeometry.GetRenderBoundingRect();
		const FSlateRect WindowRect = OwnerWindow.Get()->GetRectInScreen();
		RenderRect.IntersectionWith(WindowRect, bOverlapping);
		return bOverlapping;
	}

private:
	TWeakPtr<SACFMeshPreview::FImpl> Impl;
	TSharedPtr<FACFMeshPreviewViewportClient> Client;
	TWeakPtr<FActiveTimerHandle> AnimationTimerHandle;
	FVector2D ViewportSize = FVector2D(360.0f, 270.0f);
	double NextVisibilityCheckTime = 0.0;
	/** Clock of the previous active timer call. Zero while no timer is running, so the first call uses the nominal period. */
	double LastPumpTime = 0.0;

	/** Click to engage state: any mouse press inside turns the engine viewport back on for interaction. */
	bool bEngaged = false;
};

namespace
{
	/** Short muted notice shown instead of a viewport when the mesh is missing or unsupported. */
	TSharedRef<SWidget> MakeNoticeWidget(const FVector2D& InViewportSize)
	{
		return SNew(SBox)
			.WidthOverride(InViewportSize.X)
			.HeightOverride(InViewportSize.Y)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(FMargin(12.0f))
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)))
				.Text(LOCTEXT("MeshPreviewUnavailable", "Preview unavailable"))
			];
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// SACFMeshPreview
///////////////////////////////////////////////////////////////////////////////////////////////////

void SACFMeshPreview::Construct(const FArguments& InArgs, UObject* InMesh)
{
	const bool bUsableMesh = InMesh != nullptr && (InMesh->IsA<UStaticMesh>() || InMesh->IsA<USkeletalMesh>());

	if (bUsableMesh == false)
	{
		ChildSlot
		[
			MakeNoticeWidget(InArgs._ViewportSize)
		];
		return;
	}

	Impl = MakeShared<FImpl>();
	if (Impl->Initialize(InMesh) == false)
	{
		// Initialization failed: fall back to the notice and drop the half built state.
		Impl.Reset();
	}

	if (Impl.IsValid() == false)
	{
		ChildSlot
		[
			MakeNoticeWidget(InArgs._ViewportSize)
		];
		return;
	}

	ChildSlot
	[
		SNew(SACFMeshPreviewViewportWidget, Impl)
		.ViewportSize(InArgs._ViewportSize)
	];
}

SACFMeshPreview::~SACFMeshPreview()
{
	// Members reverse on their own: Impl dies before the base, releasing the scene, then the widget tree
	// releases the editor viewport which releases the client. No explicit cleanup needed here.
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// ACFMeshPreview helpers
///////////////////////////////////////////////////////////////////////////////////////////////////


namespace ACFMeshPreview
{
	UObject* LoadMesh(const FString& ObjectPath)
	{
		if (ObjectPath.IsEmpty())
		{
			return nullptr;
		}

		const FSoftObjectPath SoftPath(ObjectPath);

		// The asset registry lets us skip a synchronous load for assets that are known on disk not to be
		// meshes. Matching is exact on the class path, so the IsA test below stays the final authority.
		if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::LoadModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
		{
			if (IAssetRegistry* AssetRegistry = AssetRegistryModule->TryGet())
			{
				const FAssetData AssetData = AssetRegistry->GetAssetByObjectPath(SoftPath);
				if (AssetData.IsValid())
				{
					const FTopLevelAssetPath StaticMeshPath = UStaticMesh::StaticClass()->GetClassPathName();
					const FTopLevelAssetPath SkeletalMeshPath = USkeletalMesh::StaticClass()->GetClassPathName();
					if (AssetData.AssetClassPath != StaticMeshPath && AssetData.AssetClassPath != SkeletalMeshPath)
					{
						// Known on disk as an unsupported class: skip the load entirely.
						return nullptr;
					}
				}
			}
		}

		UObject* Loaded = SoftPath.TryLoad();
		if (Loaded == nullptr)
		{
			return nullptr;
		}

		// Only static or skeletal meshes may leave this function.
		if (Loaded->IsA<UStaticMesh>() || Loaded->IsA<USkeletalMesh>())
		{
			return Loaded;
		}
		return nullptr;
	}

	void OpenInEditor(UObject* Mesh)
	{
		if (Mesh == nullptr || (Mesh->IsA<UStaticMesh>() == false && Mesh->IsA<USkeletalMesh>() == false))
		{
			return;
		}

		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor != nullptr ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
		if (AssetEditorSubsystem != nullptr)
		{
			AssetEditorSubsystem->OpenEditorForAsset(Mesh);
		}
	}

	void ShowInContentBrowser(UObject* Mesh)
	{
		if (Mesh == nullptr || (Mesh->IsA<UStaticMesh>() == false && Mesh->IsA<USkeletalMesh>() == false))
		{
			return;
		}

		if (FContentBrowserModule* ContentBrowserModule = FModuleManager::LoadModulePtr<FContentBrowserModule>(TEXT("ContentBrowser")))
		{
			TArray<UObject*> Assets = { Mesh };
			ContentBrowserModule->Get().SyncBrowserToAssets(Assets);
		}
	}
}

#undef LOCTEXT_NAMESPACE
