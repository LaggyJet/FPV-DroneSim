#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CameraFollower.generated.h"

class UCameraComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), Blueprintable)
class FPVDRONESIM_API UCameraFollower : public USceneComponent
{
	GENERATED_BODY()

public:
	UCameraFollower();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Add this to header:
	FRotator InitialWorldRotation;
	float InitialYawOffset;

	// The camera this component is controlling
	UCameraComponent* TargetCamera = nullptr;

	// Cached relative rotation to maintain
	FRotator InitialRelativeRotation;
};
