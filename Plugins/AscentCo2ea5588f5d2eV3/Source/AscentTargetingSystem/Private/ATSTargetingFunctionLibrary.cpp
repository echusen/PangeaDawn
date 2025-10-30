#include "ATSTargetingFunctionLibrary.h"
#include "ATSBaseTargetComponent.h"  
#include "ATSTargetPointComponent.h"  
#include "GameFramework/Controller.h" 
#include "GameFramework/Pawn.h" 

AActor* UATSTargetingFunctionLibrary::GetTargetedActor(const APawn* pawn)  
{  
   if (pawn) {  
       const TObjectPtr<AController> controller = pawn->GetController();  
       if (controller) {  
           const TObjectPtr<UATSBaseTargetComponent> target = controller->FindComponentByClass<UATSBaseTargetComponent>();  
           if (target) {  
               return target->GetCurrentTarget();  
           }  
       }  
   }  
   return nullptr;  
}  

UATSTargetPointComponent* UATSTargetingFunctionLibrary::GetCurrentTargetPoint(const APawn* pawn)  
{  
   if (pawn) {  
       const TObjectPtr<AController> controller = pawn->GetController();  
       if (controller) {  
           const TObjectPtr<UATSBaseTargetComponent> target = controller->FindComponentByClass<UATSBaseTargetComponent>();  
           if (target) {  
               return target->GetCurrentTargetPoint();  
           }  
       }  
   }  
   return nullptr;  
}
