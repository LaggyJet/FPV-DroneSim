#include "TNano.h"

// Constants for drone lift and torque strengths
constexpr float LiftScale = 7.99f;
constexpr float YawTorqueStrength = 75.0f;
constexpr float PitchTorqueStrength = 125.0f;
constexpr float RollTorqueStrength = 125.0f;

// Max angular velocity (in radians/sec) to prevent over-rotation
constexpr float MaxYawRateRad = FMath::DegreesToRadians(105.0f);   // Around Z axis
constexpr float MaxPitchRateRad = FMath::DegreesToRadians(60.5f);  // Around X axis
constexpr float MaxRollRateRad = FMath::DegreesToRadians(60.5f);   // Around Y axis

constexpr float AngularVelocityGain = 15.0f;

// Constructor
UTNano::UTNano()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when game starts
void UTNano::BeginPlay()
{
	Super::BeginPlay();

	// Enable physics simulation on drone mesh
	DroneBody->SetSimulatePhysics(true);

	// Setup enhanced input mapping
	APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
	if (PC)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(IMC_TNano, 0);
		}
	}

	// Delay input setup to next tick
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTNano::SetupInput);
}

// Bind input actions to methods
void UTNano::SetupInput()
{
	APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
	if (!PC) return;

	if (UEnhancedInputComponent* InputComp = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		if (IA_Throttle) InputComp->BindAction(IA_Throttle, ETriggerEvent::Triggered, this, &UTNano::OnThrottle);
		if (IA_Pitch)    InputComp->BindAction(IA_Pitch, ETriggerEvent::Triggered, this, &UTNano::OnPitch);
		if (IA_Roll)     InputComp->BindAction(IA_Roll, ETriggerEvent::Triggered, this, &UTNano::OnRoll);
		if (IA_Yaw)      InputComp->BindAction(IA_Yaw, ETriggerEvent::Triggered, this, &UTNano::OnYaw);
	}
}

// Handle throttle input
void UTNano::OnThrottle(const FInputActionInstance& Instance)
{
	const float Raw = Instance.GetValue().GetMagnitude();
	const float throttle = 1.0f - Raw;

	// Map value beyond 1, otherwise just clamp
	if (throttle > 1.0f)
		Throttle = FMath::GetMappedRangeValueClamped(FVector2D(0, 2), ThrottleRange, throttle);
	else
		Throttle = FMath::Clamp(throttle, ThrottleRange[0], ThrottleRange[1]);

	UE_LOG(LogTemp, Warning, TEXT("Throttle: %.2f | Clamped: %.2f"), throttle, Throttle);
}

// Handle pitch input (-0.65 to 0.65)
void UTNano::OnPitch(const FInputActionInstance& Instance)
{
	Pitch = FMath::Clamp(Instance.GetValue().GetMagnitude() * 2.0f - 1.0f, -0.65f, 0.65f);
}

// Handle roll input (-0.65 to 0.65)
void UTNano::OnRoll(const FInputActionInstance& Instance)
{
	Roll = FMath::Clamp(Instance.GetValue().GetMagnitude() * 2.0f - 1.0f, -0.65f, 0.65f);
}

// Handle yaw input (-0.8 to 0.8)
void UTNano::OnYaw(const FInputActionInstance& Instance)
{
	Yaw = FMath::Clamp(Instance.GetValue().GetMagnitude() * 2.0f - 1.0f, -0.8f, 0.8f);
}

// Main tick function
void UTNano::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Apply lift (always)
	if (DroneBody && DroneBody->IsSimulatingPhysics())
	{
		const FVector Up = DroneBody->GetUpVector();
		const float TotalLift = Throttle * MaxThrustPerMotor * 4.0f * LiftScale;
		DroneBody->AddForce(Up * TotalLift);
	}

	// Call parent tick AFTER applying lift
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Begin torque cycle if ready
	if (physicsReady)
	{
		ApplyForces();
	}
}

// Determine if stabilization should be engaged
bool UTNano::ShouldAutoStabilize() const
{
	if (!DroneBody) return false;

	const float Altitude = DroneBody->GetComponentLocation().Z;
	const bool IsAboveThreshold = Altitude > AutoStabilizeAltitude;

	const bool NoPitchInput = FMath::IsNearlyZero(Pitch, 0.05f);
	const bool NoRollInput = FMath::IsNearlyZero(Roll, 0.05f);

	return IsAboveThreshold && NoPitchInput && NoRollInput;
}

// Start applying torque cycle
void UTNano::ApplyForces()
{
	if (!DroneBody || !DroneBody->IsSimulatingPhysics()) return;

	physicsReady = false;
	stabilize = ShouldAutoStabilize();

	// Step 1: Yaw
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTNano::ApplyYaw);
}

// Apply yaw torque
void UTNano::ApplyYaw()
{
	if (!DroneBody || !DroneBody->IsSimulatingPhysics()) return;

	const FVector Up = DroneBody->GetUpVector();
	AngularVelocity = DroneBody->GetPhysicsAngularVelocityInRadians();

	// Target yaw rate
	const float TargetYawRate = Yaw * MaxYawRateRad;
	const float CurrentYawRate = FVector::DotProduct(AngularVelocity, Up);
	const float ErrorYawRate = TargetYawRate - CurrentYawRate;

	// PD torque: Proportional control (derivative = 0)
	FVector YawTorque = Up * ErrorYawRate * AngularVelocityGain;

	DroneBody->AddTorqueInRadians(YawTorque);

	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTNano::ApplyPitch);
}

// Apply pitch torque
void UTNano::ApplyPitch()
{
	if (!DroneBody || !DroneBody->IsSimulatingPhysics()) return;

	const FVector Forward = DroneBody->GetForwardVector();
	const FVector Right = DroneBody->GetRightVector();
	const FVector Up = DroneBody->GetUpVector();

	AngularVelocity = DroneBody->GetPhysicsAngularVelocityInRadians();

	FVector TargetAxis = Forward; // Pitch is around local right axis
	float TargetRate = 0.0f;

	if (stabilize)
	{
		// Auto-stabilize: match up vector to world up
		const FVector WorldUp = FVector::UpVector;
		const FVector LocalUp = DroneBody->GetUpVector();
		const FVector TorqueDirection = FVector::CrossProduct(LocalUp, WorldUp);
		float CorrectionStrength = FVector::DotProduct(TorqueDirection, TargetAxis);
		TargetRate = CorrectionStrength * StabilizeTorqueStrength;
	}
	else
	{
		// User input: match desired pitch rate
		TargetRate = Pitch * MaxPitchRateRad;
	}

	// PD angular velocity matching
	float CurrentRate = FVector::DotProduct(AngularVelocity, TargetAxis);
	float RateError = TargetRate - CurrentRate;
	FVector PitchTorque = TargetAxis * RateError * AngularVelocityGain;

	DroneBody->AddTorqueInRadians(PitchTorque);

	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTNano::ApplyRoll);
}


// Apply roll torque
void UTNano::ApplyRoll()
{
	if (!DroneBody || !DroneBody->IsSimulatingPhysics()) return;

	const FVector Forward = DroneBody->GetForwardVector();
	const FVector Right = DroneBody->GetRightVector();
	const FVector Up = DroneBody->GetUpVector();

	AngularVelocity = DroneBody->GetPhysicsAngularVelocityInRadians();

	FVector TargetAxis = Right; // Roll is around local forward axis
	float TargetRate = 0.0f;

	if (stabilize)
	{
		// Auto-stabilize: match up vector to world up
		const FVector WorldUp = FVector::UpVector;
		const FVector LocalUp = DroneBody->GetUpVector();
		const FVector TorqueDirection = FVector::CrossProduct(LocalUp, WorldUp);
		float CorrectionStrength = FVector::DotProduct(TorqueDirection, TargetAxis);
		TargetRate = CorrectionStrength * StabilizeTorqueStrength;
	}
	else
	{
		// User input: match desired roll rate
		TargetRate = -Roll * MaxRollRateRad; // Negative since UE uses left-hand rule
	}

	// PD angular velocity matching
	float CurrentRate = FVector::DotProduct(AngularVelocity, TargetAxis);
	float RateError = TargetRate - CurrentRate;
	FVector RollTorque = TargetAxis * RateError * AngularVelocityGain;

	DroneBody->AddTorqueInRadians(RollTorque);

	// End of physics tick loop
	physicsReady = true;
}
