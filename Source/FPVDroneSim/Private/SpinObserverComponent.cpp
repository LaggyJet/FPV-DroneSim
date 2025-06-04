#include "SpinObserverComponent.h"

USpinObserverComponent::USpinObserverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USpinObserverComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USpinObserverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetAttachParent()) return;

	// Clamp spin amount
	float ClampedAmount = FMath::Clamp(SpinAmount, 0.0f, 1.0f);
	float SpinSpeed = MaxSpinSpeed * ClampedAmount * (bReverseDirection ? -1.0f : 1.0f);

	FVector AxisVector;
	switch (SpinAxis)
	{
	case ESpinAxis::X:
		AxisVector = FVector::ForwardVector;
		break;
	case ESpinAxis::Y:
		AxisVector = FVector::RightVector;
		break;
	case ESpinAxis::Z:
	default:
		AxisVector = FVector::UpVector;
		break;
	}

	FRotator DeltaRotation = FQuat(AxisVector, FMath::DegreesToRadians(SpinSpeed * DeltaTime)).Rotator();
	GetAttachParent()->AddLocalRotation(DeltaRotation);
}
