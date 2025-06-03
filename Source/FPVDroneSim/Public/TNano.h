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

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), Blueprintable)
class FPVDRONESIM_API UTNano : public UActorComponent
{
	GENERATED_BODY()

public:
	UTNano();

	// Components
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "DRONE")
	UStaticMeshComponent* DroneBody;

	// Input mappings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRONE|Input")
	UInputMappingContext* IMC_DroneControllerContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRONE|Input")
	UInputAction* IA_MoveLGimble;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DRONE|Input")
	UInputAction* IA_MoveRGimble;

	// Physics constants
	const float AirDensity = 1.255f;
	const float TorqueFactor = 0.0001f;

	// Drone mechanics
	float Drag = 0.0f;
	float MaxLiftForce;
	float DragCoefficient;
	float Weight = 60.0f; //in grams

	// Center of gravity
	float COG_Longitudal_Offset = 0.0f;
	float COG_Lateral_Offset = 0.0f;
	float COG_Vertical_Offset = 0.0f;

	// Propeller specs
	float PropPitch = 1.0f; //in inches
	float PropDiameter = 40.0f; //in millimeters
	float PropAngleR = 0.0f;
	float AoA = 0.0f;
	float ThrustCoefficient = 0.0f;
	float AltitudeFactor = 0.0f;
	int KV = 12000; //rpms per volt
	float Voltage = 8.4f;
	float MaxRPMs = 0.0f;
	float CurrRPS = 0.0f;
	int Blades = 2;
	float ArmLength = 5.5f; //in centimeters
	float ArmAngle = 0.0f;

	// Flight dynamics
	FVector Velocity = FVector::ZeroVector;
	double ForwardAirSpeed = 0.0;
	float AdvanceRatio = 0.0f;
	float Altitude = 0.0f;
	float MotorThrottle[4] = {};
	float MotorThrust[4] = {};
	FVector MotorPositions[4];
	int SpinDirection[4] = { +1, -1, +1, -1 };

	// Control input
	float LeftGimbleLateral = 0.0f;
	float LeftGimbleLongitudal = 0.0f;
	float RightGimbleLateral = 0.0f;
	float RightGimbleLongitudal = 0.0f;

	// Mix factors
	const float PitchScale = 1.0f;
	const float RollScale = 1.0f;
	const float YawScale = 1.0f;

	// Utility
	FVector GetMotorOffsetFromCenter(float InArmLength, float InArmAngleDegrees) const
	{
		float ThetaRad = FMath::DegreesToRadians(InArmAngleDegrees);
		return FVector(InArmLength * FMath::Cos(ThetaRad), InArmLength * FMath::Sin(ThetaRad), 0.0f);
	}

protected:
	virtual void BeginPlay() override;

	// Input setup and handlers
	void SetupInput();
	void OnMoveLGimble(const FInputActionInstance& Instance);
	void OnMoveRGimble(const FInputActionInstance& Instance);

	// Simulation flow
	virtual void SimulatePhysics();
	virtual void CalculateThrottle();
	virtual void CalculateThrustForces();
	virtual void DistributeForces();

	// Optional overrides, kept empty for extensibility
	virtual void CalculateThrustFromCurve() {}
	virtual void CalculateDragForce() {}
	virtual void CheckSpecialConditions() {}
	virtual void CalculateGravityForce() const {}
	virtual void ApplyPhysics() const {}

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void ActivateButton(int ButtonID) {}
};
