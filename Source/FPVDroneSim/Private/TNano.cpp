#include "TNano.h"

constexpr float LiftScale = 7.89f;
constexpr float YawTorqueStrength = 25.0f;
constexpr float PitchTorqueStrength = 25.0f;
constexpr float RollTorqueStrength = 25.0f;

constexpr float MaxYawRateRad = FMath::DegreesToRadians(4.5f);   // Z axis
constexpr float MaxPitchRateRad = FMath::DegreesToRadians(5.0f); // X axis
constexpr float MaxRollRateRad = FMath::DegreesToRadians(5.0f);  // Y axis

UTNano::UTNano()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTNano::BeginPlay()
{
	Super::BeginPlay();
	DroneBody->SetSimulatePhysics(true);

	APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	// Add Mapping Context
	if (PC && Subsystem)
	{
		Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
		Subsystem->AddMappingContext(IMC_TNano, 0);
	}


	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTNano::SetupInput);
}

void UTNano::SetupInput()
{
	APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
	auto* InputComp = Cast<UEnhancedInputComponent>(PC->InputComponent);

	if (IA_Throttle) InputComp->BindAction(IA_Throttle, ETriggerEvent::Triggered, this, &UTNano::OnThrottle);
	if (IA_Pitch)    InputComp->BindAction(IA_Pitch, ETriggerEvent::Triggered, this, &UTNano::OnPitch);
	if (IA_Roll)     InputComp->BindAction(IA_Roll, ETriggerEvent::Triggered, this, &UTNano::OnRoll);
	if (IA_Yaw)      InputComp->BindAction(IA_Yaw, ETriggerEvent::Triggered, this, &UTNano::OnYaw);
}

void UTNano::OnThrottle(const FInputActionInstance& Instance)
{
    float throttle = 1 - Instance.GetValue().GetMagnitude();
    if (throttle > 1)
        Throttle = FMath::GetMappedRangeValueClamped(FVector2D(0, 2), FVector2D(0, 1), throttle);
    else
	    Throttle = FMath::Clamp(1.0f - Instance.GetValue().GetMagnitude(), 0.0f, 1.0f);
    UE_LOG(LogTemp, Warning, TEXT("Throttle: %.2f"), throttle);
    UE_LOG(LogTemp, Warning, TEXT("ThrottleClamped: %.2f"), Throttle);
}

void UTNano::OnPitch(const FInputActionInstance& Instance)
{
	Pitch = FMath::Clamp(Instance.GetValue().GetMagnitude() * 2.0f - 1.0f, -1.0f, 1.0f);
}

void UTNano::OnRoll(const FInputActionInstance& Instance)
{
	Roll = FMath::Clamp(Instance.GetValue().GetMagnitude() * 2.0f - 1.0f, -1.0f, 1.0f);
}

void UTNano::OnYaw(const FInputActionInstance& Instance)
{
	Yaw = FMath::Clamp(Instance.GetValue().GetMagnitude() * 2.0f - 1.0f, -1.0f, 1.0f);
}

void UTNano::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    GetWorld()->DeltaTimeSeconds;
    if(physicsReady)
	    ApplyForces();
}

bool UTNano::ShouldAutoStabilize() const
{
    if (!DroneBody) return false;

    float Altitude = DroneBody->GetComponentLocation().Z;
    bool IsAboveThreshold = Altitude > AutoStabilizeAltitude;

    bool NoPitchInput = FMath::IsNearlyZero(Pitch, 0.05f);
    bool NoRollInput = FMath::IsNearlyZero(Roll, 0.05f);

    return IsAboveThreshold && NoPitchInput && NoRollInput;
}

void UTNano::ApplyForces()
{
    if (!DroneBody || !DroneBody->IsSimulatingPhysics()) return;
    else physicsReady = false;

    stabilize = ShouldAutoStabilize();
    GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTNano::ApplyYaw);
}

void UTNano::ApplyYaw()
{
    if (!DroneBody || !DroneBody->IsSimulatingPhysics()) return;

    const FVector Up = DroneBody->GetUpVector();

    // Apply lift
    float ClampedThrottle = FMath::Clamp(Throttle, 0.0f, 1.0f);
    float TotalLift = ClampedThrottle * MaxThrustPerMotor * 4.0f * LiftScale;
    DroneBody->AddForce(Up * TotalLift);

    // Apply yaw torque
    float EffectiveYaw = FMath::Clamp(Yaw, -1.0f, 1.0f);
    FVector YawTorque = Up * EffectiveYaw * YawTorqueStrength;
    DroneBody->AddTorqueInRadians(YawTorque);

    // Clamp yaw rate (Z)
    FVector AngularVelocity = DroneBody->GetPhysicsAngularVelocityInRadians();
    AngularVelocity.Z = FMath::Clamp(AngularVelocity.Z, -MaxYawRateRad, MaxYawRateRad);
    DroneBody->SetPhysicsAngularVelocityInRadians(AngularVelocity, false);

    //UE_LOG(LogTemp, Warning, TEXT("Yaw: %.2f | AngularVel.Z: %.2f"), Yaw, AngularVelocity.Z);

    GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTNano::ApplyPitch);
}


void UTNano::ApplyPitch()
{
    if (!DroneBody || !DroneBody->IsSimulatingPhysics()) return;

    const FVector Up = DroneBody->GetUpVector();
    const FVector Forward = DroneBody->GetForwardVector();

    // Apply lift
    float ClampedThrottle = FMath::Clamp(Throttle, 0.0f, 1.0f);
    float TotalLift = ClampedThrottle * MaxThrustPerMotor * 4.0f * LiftScale;
    DroneBody->AddForce(Up * TotalLift);

    // Apply pitch torque
    float EffectivePitch = FMath::Clamp(Pitch, -1.0f, 1.0f);
    FVector PitchTorque;

    if (stabilize)
    {
        FVector LocalUp = DroneBody->GetUpVector();
        FVector WorldUp = FVector::UpVector;

        FVector TorqueDirection = FVector::CrossProduct(LocalUp, WorldUp);
        FVector PitchAxis = DroneBody->GetRightVector();

        float PitchCorrectionStrength = FVector::DotProduct(TorqueDirection, PitchAxis);
        FVector TargetTorque = PitchAxis * PitchCorrectionStrength * StabilizeTorqueStrength;

        // Smooth torque application
        SmoothedPitchTorque = FMath::VInterpTo(SmoothedPitchTorque, TargetTorque, GetWorld()->DeltaTimeSeconds, StabilizeSmoothingSpeed);
        PitchTorque = SmoothedPitchTorque;
    }
    else
    {
        PitchTorque = Forward * EffectivePitch * PitchTorqueStrength;

        SmoothedPitchTorque = FVector::ZeroVector; // Reset smoothing when user input resumes
    }

    DroneBody->AddTorqueInRadians(PitchTorque);


    // Clamp pitch rate (X)
    FVector AngularVelocity = DroneBody->GetPhysicsAngularVelocityInRadians();
    AngularVelocity.X = FMath::Clamp(AngularVelocity.X, -MaxPitchRateRad, MaxPitchRateRad);
    DroneBody->SetPhysicsAngularVelocityInRadians(AngularVelocity, false);

    //UE_LOG(LogTemp, Warning, TEXT("Pitch: %.2f | AngularVel.X: %.2f"), Pitch, AngularVelocity.X);

    GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTNano::ApplyRoll);
}

void UTNano::ApplyRoll()
{
    if (!DroneBody || !DroneBody->IsSimulatingPhysics()) return;

    const FVector Up = DroneBody->GetUpVector();
    const FVector Right = DroneBody->GetRightVector();

    // Apply lift
    float ClampedThrottle = FMath::Clamp(Throttle, 0.0f, 1.0f);
    float TotalLift = ClampedThrottle * MaxThrustPerMotor * 4.0f * LiftScale;
    DroneBody->AddForce(Up * TotalLift);

    // Apply roll torque
    float EffectiveRoll = FMath::Clamp(Roll, -1.0f, 1.0f);
    FVector RollTorque;

    if (stabilize)
    {
        FVector LocalUp = DroneBody->GetUpVector();
        FVector WorldUp = FVector::UpVector;

        FVector TorqueDirection = FVector::CrossProduct(LocalUp, WorldUp);
        FVector RollAxis = DroneBody->GetForwardVector();

        float RollCorrectionStrength = FVector::DotProduct(TorqueDirection, RollAxis);
        FVector TargetTorque = RollAxis * RollCorrectionStrength * StabilizeTorqueStrength;

        // Smooth torque application
        SmoothedRollTorque = FMath::VInterpTo(SmoothedRollTorque, TargetTorque, GetWorld()->DeltaTimeSeconds, StabilizeSmoothingSpeed);
        RollTorque = SmoothedRollTorque;
    }
    else
    {
        RollTorque = -Right * EffectiveRoll * RollTorqueStrength;

        SmoothedRollTorque = FVector::ZeroVector; // Reset smoothing when user input resumes
    }

    DroneBody->AddTorqueInRadians(RollTorque);


    // Clamp roll rate (Y)
    FVector AngularVelocity = DroneBody->GetPhysicsAngularVelocityInRadians();
    AngularVelocity.Y = FMath::Clamp(AngularVelocity.Y, -MaxRollRateRad, MaxRollRateRad);
    DroneBody->SetPhysicsAngularVelocityInRadians(AngularVelocity, false);

    //UE_LOG(LogTemp, Warning, TEXT("Roll: %.2f | AngularVel.Y: %.2f"), Roll, AngularVelocity.Y);

    // Final step in cycle
    physicsReady = true;
}
