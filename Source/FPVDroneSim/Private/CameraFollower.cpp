#include "CameraFollower.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"

UCameraFollower::UCameraFollower()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCameraFollower::BeginPlay()
{

	Super::BeginPlay();

	if (USceneComponent* Parent = GetAttachParent())
	{
		TargetCamera = Cast<UCameraComponent>(Parent);
		if (TargetCamera)
		{
			// Store the initial world rotation of the camera
			InitialWorldRotation = TargetCamera->GetComponentRotation();

			InitialYawOffset = InitialWorldRotation.Yaw - GetOwner()->GetActorRotation().Yaw;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("CameraFollower: Parent is not a UCameraComponent."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CameraFollower: No attach parent found."));
	}
}

void UCameraFollower::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!TargetCamera) return;

	// Get the pawn's current yaw
	float PawnYaw = GetOwner()->GetActorRotation().Yaw;

	// Decompose initial rotation
	float InitialPitch = InitialWorldRotation.Pitch;
	float InitialRoll = InitialWorldRotation.Roll;
	float InitialYaw = InitialWorldRotation.Yaw;

	// In TickComponent:
	float NewYaw = GetOwner()->GetActorRotation().Yaw + InitialYawOffset;

	FRotator NewRotation = FRotator(InitialPitch, NewYaw, InitialRoll);

	TargetCamera->SetWorldRotation(NewRotation);

	// Calculate new yaw = pawn yaw + initial camera yaw offset
	float NewYaw = PawnYaw + (InitialYaw - PawnYaw);

	// Actually, better just keep initial yaw offset relative to pawn:
	NewYaw = PawnYaw + (InitialYaw - PawnYaw);

	// But this simplifies to just initial yaw (which is wrong),
	// so better approach is:

	NewYaw = PawnYaw + InitialYawOffset;

	// We'll store InitialYawOffset at BeginPlay:

	// So let's fix this by adding a variable:

	// --- Solution below ---
}
