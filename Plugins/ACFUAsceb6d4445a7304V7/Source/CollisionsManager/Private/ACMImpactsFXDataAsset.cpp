// Copyright (C) Developed by Pask, Published by Dark Tower Interactive SRL 2026. All Rights Reserved.

#include "ACMImpactsFXDataAsset.h"
#include "ACMTypes.h"



const FMaterialImpactFX* UACMImpactsFXDataAsset::FindMaterialFX(const FImpactsArray& impacts, const UPhysicalMaterial* materialImpacted)
{
    const FMaterialImpactFX* matfx = impacts.ImpactsFX.FindByKey(materialImpacted);

    // THE ENTRY WITH NO MATERIAL SET IS THE DEFAULT FOR EVERY UNLISTED SURFACE
    if (!matfx && materialImpacted) {
        matfx = impacts.ImpactsFX.FindByKey<const UPhysicalMaterial*>(nullptr);
    }
    return matfx;
}

bool UACMImpactsFXDataAsset::TryGetImpactFX(const TSubclassOf<class UDamageType>& damageImpacting, class UPhysicalMaterial* materialImpacted, FBaseFX& outFXtoPlay) const
{
    if (!damageImpacting) {
        return false;
    }

    const FImpactsArray* impacts = ImpactFXsByDamageType.Find(damageImpacting);

    if (!impacts) {
        // IF WE DON'T FIND THAT DAMAGE TYPE, WE TRY HIS PARENT CLASSES
        for (const auto& impfx : ImpactFXsByDamageType) {
            if (impfx.Key && damageImpacting->IsChildOf(impfx.Key)) {
                impacts = &impfx.Value;
                break;
            }
        }
    }

    if (!impacts) {
        return false;
    }

    const FMaterialImpactFX* matfx = FindMaterialFX(*impacts, materialImpacted);
    if (!matfx) {
        return false;
    }

    outFXtoPlay = FBaseFX(matfx->ActionSound, matfx->NiagaraParticle, matfx->ActionParticle);
    return true;
}
