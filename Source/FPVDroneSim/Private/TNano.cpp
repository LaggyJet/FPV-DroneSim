#include "TNano.h"

// Constructor
UTNano::UTNano()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Initialize other values as needed
	for (int i = 0; i < 4; ++i)
	{
		MotorThrottle[i] = 0.0f;
		MotorThrust[i] = 0.0f;
	}

	MotorPositions[0] = GetMotorOffsetFromCenter(ArmLength, -45.0f);   // Front right
	MotorPositions[1] = GetMotorOffsetFromCenter(ArmLength, 45.0f);  // Back right
	MotorPositions[2] = GetMotorOffsetFromCenter(ArmLength, 135.0f);  // Back left
	MotorPositions[3] = GetMotorOffsetFromCenter(ArmLength, -135.0f);  // Front left

	if (KV > 0.0f && Voltage > 0.0f)
	{
		MaxRPMs = KV * Voltage;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("KV or Voltage not set. MaxRPMs will be zero!"));
	}
}

// Called when the game starts
void UTNano::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("NanoController BeginPlay called"));

	SetComponentTickEnabled(true);

	if (!DroneBody)
	{
		DroneBody = Cast<UStaticMeshComponent>(GetOwner()->GetComponentByClass(UStaticMeshComponent::StaticClass()));
		if (!DroneBody)
		{
			UE_LOG(LogTemp, Error, TEXT("DroneBody is null! Physics won't simulate."));
		}
	}

	APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());

	// Add Mapping Context
	if (PC)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (IMC_DroneControllerContext)
			{
				UE_LOG(LogTemp, Warning, TEXT("Adding Mapping Context..."));
				//UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
				Subsystem->AddMappingContext(IMC_DroneControllerContext, 0);

			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("IMC_DroneControllerContext is null!"));
			}
		}
	}

	// Enable input for the actor this component is attached to
	if (PC && GetOwner())
	{
		GetOwner()->EnableInput(PC);
		UE_LOG(LogTemp, Warning, TEXT("Input enabled for: %s"), *GetOwner()->GetName());

		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}

	// Bind inputs
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTNano::SetupInput);

}

void UTNano::SetupInput()
{
	APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("No PlayerController found"));
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PC->InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("InputComponent is NOT EnhancedInputComponent!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("EnhancedInputComponent found, binding actions"));

	if (IA_MoveLGimble)
	{
		EnhancedInput->BindAction(IA_MoveLGimble, ETriggerEvent::Triggered, this, &UTNano::OnMoveLGimble);
	}
	if (IA_MoveRGimble)
	{
		EnhancedInput->BindAction(IA_MoveRGimble, ETriggerEvent::Triggered, this, &UTNano::OnMoveRGimble);
	}
}

void UTNano::OnMoveLGimble(const FInputActionInstance& Instance)
{
	FVector2D Input = Instance.GetValue().Get<FVector2D>();
	LeftGimbleLateral = Input.X;
	LeftGimbleLongitudal = Input.Y;

	UE_LOG(LogTemp, Warning, TEXT("Left Gimble: X=%f, Y=%f"), Input.X, Input.Y);
}

void UTNano::OnMoveRGimble(const FInputActionInstance& Instance)
{
	FVector2D Input = Instance.GetValue().Get<FVector2D>();
	RightGimbleLateral = Input.X;
	RightGimbleLongitudal = Input.Y;

	UE_LOG(LogTemp, Warning, TEXT("Right Gimble: X=%f, Y=%f"), Input.X, Input.Y);
}

// Called every frame
void UTNano::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	//UE_LOG(LogTemp, Warning, TEXT("NanoController TickComponent called"));
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SimulatePhysics();

	APlayerController* PC = Cast<APlayerController>(GetOwner()->GetInstigatorController());
	if (PC)
	{
		float LX = 0.0f;
		float LY = 0.0f;
		//PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_LeftStick, LX, LY);
		float RX = 0.0f;
		float RY = 0.0f;
		//PC->GetInputAnalogStickState(EControllerAnalogStick::CAS_RightStick, RX, RY);

		UE_LOG(LogTemp, Warning, TEXT("L: (%f, %f), R: (%f, %f)"), LX, LY, RX, RY);
	}
}

// Main simulation function
void UTNano::SimulatePhysics()
{
	CalculateThrottle();
	CalculateThrustForces();
	DistributeForces();
	//CheckSpecialConditions();
}

void UTNano::CalculateThrottle()
{
	MotorThrottle[0] = FMath::Clamp((RightGimbleLongitudal + PitchScale * LeftGimbleLongitudal - RollScale * LeftGimbleLateral - YawScale * RightGimbleLateral), 0.0f, 1.0f);
	MotorThrottle[1] = FMath::Clamp((RightGimbleLongitudal + PitchScale * LeftGimbleLongitudal + RollScale * LeftGimbleLateral + YawScale * RightGimbleLateral), 0.0f, 1.0f);
	MotorThrottle[2] = FMath::Clamp((RightGimbleLongitudal - PitchScale * LeftGimbleLongitudal + RollScale * LeftGimbleLateral - YawScale * RightGimbleLateral), 0.0f, 1.0f);
	MotorThrottle[3] = FMath::Clamp((RightGimbleLongitudal - PitchScale * LeftGimbleLongitudal - RollScale * LeftGimbleLateral + YawScale * RightGimbleLateral), 0.0f, 1.0f);
}

void UTNano::CalculateThrustForces()
{
	Velocity = DroneBody->GetComponentVelocity();
	ForwardAirSpeed = -FVector::DotProduct(Velocity, -DroneBody->GetUpVector());

	PropAngleR = FMath::DegreesToRadians(FMath::Atan(PropPitch / (PI * (PropDiameter / 25.4f)))); //arc tangent of ( prop pitch in inches times ( pi divided by prop diameter converted from mm to inches))

	for (int x = 0; x < 4; x++)
	{
		// Convert to more useful units
		CurrRPS = (MotorThrottle[x] * MaxRPMs) / 60.0f;

		//FVector PropDirection = PropellerComponent->GetUpVector();
		//for if props are not perfectly vertical or have different angles
		//float ForwardAirspeed = -FVector::DotProduct(Velocity, PropDirection);
		//if that is the case forward air speed would need to be calculated for each prop

		// Estimate advance ratio:
		AdvanceRatio = ForwardAirSpeed / (CurrRPS * PropDiameter + 0.00003f); // Avoid division by zero

		// Estimate effective angle of attack from pitch and inflow
		AoA = FMath::Atan(FMath::Tan(PropAngleR) - AdvanceRatio);

		// Estimate dynamic thrust coefficient
		ThrustCoefficient = 0.1f * FMath::Sin(2.0f * AoA); // Empirical formula

		// Clamp to reasonable range
		ThrustCoefficient = FMath::Clamp(ThrustCoefficient, 0.02f, 0.12f);

		// Momentum theory:
		float Thrust = ThrustCoefficient * AirDensity * FMath::Square(CurrRPS) * FMath::Pow(PropDiameter, 4);

		// Altitude correction (optional): adjust for thinner air
		if (Altitude > 0.0f)
		{
			AltitudeFactor = FMath::Pow(1.0f - (0.0000225577f * Altitude), 5.25588f);
			Thrust *= AltitudeFactor;
		}

		MotorThrust[x] = Thrust;
	}
}

void UTNano::DistributeForces()
{
	if (!DroneBody) return;

	FVector TotalForce = FVector::ZeroVector;
	FVector TotalTorque = FVector::ZeroVector;

	// Direction in which thrust is applied (adjust if drone is a pusher)
	FVector ThrustDirection = DroneBody->GetUpVector(); // Or -GetUpVector() for pushers

	for (int i = 0; i < 4; i++)
	{
		FVector Force = ThrustDirection * MotorThrust[i];
		FVector r = MotorPositions[i] - DroneBody->GetComponentLocation(); // position relative to center of mass

		// Torque from thrust offset
		TotalTorque += FVector::CrossProduct(r, Force);

		// Torque from propeller spin (yaw)
		float SpinYawTorque = MotorThrust[i] * SpinDirection[i] * TorqueFactor;
		TotalTorque += ThrustDirection * SpinYawTorque;

		TotalForce += Force;
	}
	//UE_LOG(LogTemp, Warning, TEXT("TotalForce: %s"), *TotalForce.ToString());


	// Apply to physics body
	DroneBody->AddForce(TotalForce);
	DroneBody->AddTorqueInRadians(TotalTorque);
}
