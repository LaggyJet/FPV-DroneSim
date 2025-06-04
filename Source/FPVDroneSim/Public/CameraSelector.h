#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"
#include "CameraSelector.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), Blueprintable)
class FPVDRONESIM_API UCameraSelector : public UActorComponent
{
	GENERATED_BODY()

public:
	UCameraSelector();

protected:
	virtual void BeginPlay() override;

	// Bind input via Enhanced Input
	void SetupEnhancedInput();

	// Called when input is triggered
	void OnSwitchCamera(const FInputActionValue& Value);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SELECTOR")
	UCameraComponent* CameraOne;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SELECTOR")
	UCameraComponent* CameraTwo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SELECTOR")
	UInputMappingContext* InputMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SELECTOR")
	UInputAction* SwitchCameraAction;

	bool bUsingFirstCamera = true;
};
