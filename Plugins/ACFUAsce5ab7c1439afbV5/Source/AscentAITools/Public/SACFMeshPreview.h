// Copyright (C) Developed by Insodimension, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Live 3D preview of an imported mesh, small enough to sit inside a chat bubble. The mesh must be a UStaticMesh
 * or a USkeletalMesh (reference pose). Drag to orbit, mouse wheel to zoom. The mesh turns slowly on its own until
 * the user takes over.
 *
 * Cost model: only one preview in the whole process renders in real time, and only while it is on screen. Every
 * other preview keeps its last rendered frame and redraws on demand (resize, hover, user input), so a long chat
 * with many generated meshes does not load the GPU.
 */
class ASCENTAITOOLS_API SACFMeshPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SACFMeshPreview)
		: _ViewportSize(FVector2D(360.0f, 270.0f))
	{}
		/** Size of the 3D view in Slate units. */
		SLATE_ARGUMENT(FVector2D, ViewportSize)
	SLATE_END_ARGS()

public:
	/** Opaque implementation class; defined in SACFMeshPreview.cpp only. */
	class FImpl;

	/** InMesh is a UStaticMesh or USkeletalMesh. A null or unsupported object shows a short notice instead of a view. */
	void Construct(const FArguments& InArgs, UObject* InMesh);
	virtual ~SACFMeshPreview() override;

private:
	TSharedPtr<FImpl> Impl;
};

namespace ACFMeshPreview
{
	/** Loads the mesh at an object path such as /Game/Yarin/Chest/SM_Chest.SM_Chest. Null when it is missing or is not a static or skeletal mesh. */
	ASCENTAITOOLS_API UObject* LoadMesh(const FString& ObjectPath);

	/** Opens the asset's own editor (the static or skeletal mesh editor). */
	ASCENTAITOOLS_API void OpenInEditor(UObject* Mesh);

	/** Selects the asset in the Content Browser, opening the browser if needed. */
	ASCENTAITOOLS_API void ShowInContentBrowser(UObject* Mesh);
}
