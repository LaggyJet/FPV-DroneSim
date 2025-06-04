#include "CameraSelector.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/InputComponent.h"

UCameraSelector::UCameraSelector()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCameraSelector::BeginPlay()
{
	Super::BeginPlay();

	// Ensure both cameras are assigned
	if (!CameraOne || !CameraTwo) return;

	CameraOne->SetActive(true);
	CameraTwo->SetActive(false);

	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UCameraSelector::SetupEnhancedInput);
}

void UCameraSelector::SetupEnhancedInput()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn) return;

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC) return;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		if (InputMappingContext)
		{
			Subsystem->AddMappingContext(InputMappingContext, 0);
		}
	}

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(OwnerPawn->InputComponent))
	{
		if (SwitchCameraAction)
		{
			EIC->BindAction(SwitchCameraAction, ETriggerEvent::Started, this, &UCameraSelector::OnSwitchCamera);
		}
	}
}

void UCameraSelector::OnSwitchCamera(const FInputActionValue& Value)
{
	if (!CameraOne || !CameraTwo) return;

	CameraOne->SetActive(bUsingFirstCamera ? false : true);
	CameraTwo->SetActive(bUsingFirstCamera ? true : false);

	bUsingFirstCamera = !bUsingFirstCamera;
}
