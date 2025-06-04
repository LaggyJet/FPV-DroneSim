#include "ScanSurrounding.h"
#include "CheckpointManager.h"
#include "Containers/Set.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"

UScanSurrounding::UScanSurrounding() { PrimaryComponentTick.bCanEverTick = true; }

void UScanSurrounding::BeginPlay() {
	Super::BeginPlay();
	VisitedAngles.Reset();
	lastYaw = GetOwner()->GetActorRotation().Yaw;
	bScanComplete = false;
	bIsScanning = true;

    APlayerController* PC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
    auto* InputComp = Cast<UEnhancedInputComponent>(PC->InputComponent);
    if (IA_StartScanning) InputComp->BindAction(IA_StartScanning, ETriggerEvent::Triggered, this, &UScanSurrounding::StartScan);
}

void UScanSurrounding::StartScan() {
	if (bIsScanRunning) return;
	bIsScanRunning = bIsScanning = true;
	bScanComplete = false;
	VisitedAngles.Reset();
	AActor* Owner = GetOwner();
	if (Owner)
		lastYaw = Owner->GetActorRotation().Yaw;
	OnScanStart.Broadcast();
	FVector InitialLocation = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	Async(EAsyncExecution::Thread, [this, InitialLocation]() { ON_SCOPE_EXIT{ bIsScanRunning = false; }; HandleScan(InitialLocation); });
}

void UScanSurrounding::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	if (IsBeingDestroyed() || !GetOwner()) return;
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bIsScanning || bScanComplete) return;
	HandleSpinning();
}

void UScanSurrounding::HandleSpinning() {
	AActor* owner = GetOwner();
	if (!owner) return;
	const float curYaw = FMath::Wrap(float(owner->GetActorRotation().Yaw), 0.f, 360.f);
	const float prevYaw = FMath::Wrap(lastYaw, 0.f, 360.f);
	const float delta = FMath::FindDeltaAngleDegrees(prevYaw, curYaw);
	const int32 stepCount = FMath::CeilToInt(FMath::Abs(delta) / SegmentSizeDegrees);
	const float direction = (delta >= 0.0f) ? 1.0f : -1.0f;
	for (int32 step = 0; step <= stepCount; ++step) {
		float interpYaw = FMath::Wrap(prevYaw + direction * step * SegmentSizeDegrees, 0.f, 360.f);
		int32 segment = FMath::FloorToInt(interpYaw / SegmentSizeDegrees);
		VisitedAngles.Add(segment);
	}
	lastYaw = curYaw;
	if (VisitedAngles.Num() >= RequiredSegmentCount) {
		bScanComplete = true;
		bIsScanning = bIsScanRunning = false;
		for (TActorIterator<ACheckpointManager> It(GetWorld()); It; ++It) {
			if (ACheckpointManager* Manager = *It) {
				Manager->ScanState = EScanProgress::Completed;
				break;
			}
		}
	}
}

void UScanSurrounding::HandleScan(FVector StartPoint) {
    TArray<FVector> Points;
    Points.Add(StartPoint);
    while(bIsScanning) {
        AActor* Owner = GetOwner();
        if (!Owner || IsBeingDestroyed()) break;
        const FVector StartLocation = Owner->GetActorLocation();
        const FVector Forward = Owner->GetActorRightVector();
        const float RayLength = 800.f;
        const float VerticalFOV = 60.f; 
        const float PitchStep = 1.0f;   
        UWorld* World = GetWorld();
        if (!World) break;
        static float CurrentPitch = -VerticalFOV * 0.5f;
        if (CurrentPitch > VerticalFOV * 0.5f)
            CurrentPitch = -VerticalFOV * 0.5f;
        FRotator RotationOffset(CurrentPitch, 0.f, 0.f); 
        FVector ScanDir = RotationOffset.RotateVector(Forward);
        FVector End = StartLocation + ScanDir * RayLength;
        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(Owner);
        if (World->LineTraceSingleByChannel(Hit, StartLocation, End, ECC_Visibility, Params))
            Points.Add(Hit.ImpactPoint);
        #if WITH_EDITOR
                if (GEngine) {
                    AsyncTask(ENamedThreads::GameThread, [World, StartLocation, End, Hit]() {
                        DrawDebugLine(World, StartLocation, End, FColor::Blue, false, 0.05f);
                        if (Hit.bBlockingHit)
                            DrawDebugPoint(World, Hit.ImpactPoint, 5.0f, FColor::Red, false, 0.05f);
                    });
                }
        #endif
        CurrentPitch += PitchStep;
    }
    FString CsvPath = FPaths::ProjectContentDir() / TEXT("Scripts/scan_points.csv");
    FString CsvContent = TEXT("X,Y,Z\n");
    for (const FVector& Point : Points)
        CsvContent += FString::Printf(TEXT("%.3f,%.3f,%.3f\n"), Point.X, Point.Y, Point.Z);
    FFileHelper::SaveStringToFile(CsvContent, *CsvPath);
    FString PythonExe = FPaths::ProjectContentDir() / TEXT("ThirdParty/Python/python.exe");
    FString ScriptPath = FPaths::ProjectContentDir() / TEXT("Scripts/CreateScan.py");
    FString Params = FString::Printf(TEXT("\"%s\" %d"), *ScriptPath, ScanCounter);
    void* PipeRead = nullptr;
    void* PipeWrite = nullptr;
    FPlatformProcess::CreatePipe(PipeRead, PipeWrite);
    FProcHandle Proc = FPlatformProcess::CreateProc(*PythonExe, *Params, true, false, false, nullptr, 0, nullptr, PipeWrite);
    FPlatformProcess::ClosePipe(nullptr, PipeWrite);
    FString Output;
    while (FPlatformProcess::IsProcRunning(Proc)) {
        Output += FPlatformProcess::ReadPipe(PipeRead);
        FPlatformProcess::Sleep(0.01f);
    }
    Output += FPlatformProcess::ReadPipe(PipeRead);
    FPlatformProcess::ClosePipe(PipeRead, nullptr);
    UE_LOG(LogTemp, Warning, TEXT("Python Output: %s"), *Output);
    ScanCounter++;
}