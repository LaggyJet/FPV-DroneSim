#include "ScanSurrounding.h"
#include "CheckpointManager.h"
#include "Containers/Set.h"
#include "EngineUtils.h"

UScanSurrounding::UScanSurrounding() { PrimaryComponentTick.bCanEverTick = true; }

void UScanSurrounding::BeginPlay() {
	Super::BeginPlay();
	VisitedAngles.Reset();
	lastYaw = GetOwner()->GetActorRotation().Yaw;
	bScanComplete = false;
	bIsScanning = true;
}

void UScanSurrounding::StartScan() {
	bIsScanning = true;
	bScanComplete = false;
	VisitedAngles.Reset();
	if (AActor* Owner = GetOwner())
		lastYaw = Owner->GetActorRotation().Yaw;
	OnScanStart.Broadcast();
}

void UScanSurrounding::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	if (IsBeingDestroyed() || !GetOwner()) return;

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsScanning || bScanComplete)
		return;

	AActor* owner = GetOwner();
	if (!owner)
		return;

	float curYaw = owner->GetActorRotation().Yaw;
	lastYaw = curYaw;
	float normalizedYaw = FMath::Fmod(curYaw + 360.0f, 360.0f);
	int32 segment = FMath::FloorToInt(normalizedYaw / 10.0f);
	VisitedAngles.Add(segment);
	if (VisitedAngles.Num() >= 36) {
		bScanComplete = true;
		bIsScanning = false;
		for (TActorIterator<ACheckpointManager> It(GetWorld()); It; ++It) {
			if (ACheckpointManager* Manager = *It) {
				Manager->ScanState = EScanProgress::Completed;
				break;
			}
		}
	}
}
