#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Camera/CameraComponent.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "TNano.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPVDRONESIM_API UTNano : public UActorComponent
{
	GENERATED_BODY()

public:
	UTNano();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	UStaticMeshComponent* DroneBody;

	UPROPERTY(EditAnywhere)
	UInputMappingContext* IMC_TNano;
	UPROPERTY(EditAnywhere)
	UInputAction* IA_Throttle;
	UPROPERTY(EditAnywhere)
	UInputAction* IA_Pitch;
	UPROPERTY(EditAnywhere)
	UInputAction* IA_Roll;
	UPROPERTY(EditAnywhere)
	UInputAction* IA_Yaw;

	UPROPERTY(EditAnywhere, Category = "Stabilization")
	float AutoStabilizeAltitude = 300.0f; // in centimeters

	UPROPERTY(EditAnywhere, Category = "Stabilization")
	float StabilizeTorqueStrength = 20.0f;
	// Smoothed stabilization torques
	FVector SmoothedPitchTorque = FVector::ZeroVector;
	FVector SmoothedRollTorque = FVector::ZeroVector;

	// Smoothing speed (higher = faster response)
	UPROPERTY(EditAnywhere, Category = "Stabilization")
	float StabilizeSmoothingSpeed = 5.0f;
	bool stabilize;

	const int SpinDirection[4] = { +1, -1, +1, -1 }; // CW or CCW for motors
	const float SmoothedThrottle = 0.0f; // Current throttle being applied (smoothed)


	float Throttle = 0.0f;
	float Pitch = 0.0f;
	float Roll = 0.0f;
	float Yaw = 0.0f;

	float MaxThrustPerMotor = 3.75f; // Newtons

	UFUNCTION(BlueprintCallable)
	float GetThrottle() { return Throttle; };

protected:
	virtual void BeginPlay() override;
	virtual void SetupInput();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool ShouldAutoStabilize() const;

	bool physicsReady = true;

	void OnThrottle(const FInputActionInstance& Instance);
	void OnPitch(const FInputActionInstance& Instance);
	void OnRoll(const FInputActionInstance& Instance);
	void OnYaw(const FInputActionInstance& Instance);
	void ApplyYaw();
	void ApplyRoll();
	void ApplyPitch();

	void ApplyForces();
};
