#include "ScanSurrounding.h"
#include "CheckpointManager.h"
#include "Containers/Set.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "BakedColorMapManager.h"

UScanSurrounding::UScanSurrounding() { PrimaryComponentTick.bCanEverTick = true; }

void UScanSurrounding::BeginPlay() {
    Super::BeginPlay();
    visitedAngles.Reset();
    lastYaw = GetOwner() ? GetOwner()->GetActorRotation().Yaw : 0.f;
    bScanComplete = bIsScanning = true;
    UBakedColorMapManager::Get(GetWorld())->BakeAndSaveAllMapsIfMissing((GetWorld() ? GetWorld()->GetMapName() : TEXT("UnknownMap")), [this]() { bIsScanning = false; });
    APlayerController *pC = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
    if (auto *inputComp = pC ? Cast<UEnhancedInputComponent>(pC->InputComponent) : nullptr; IA_StartScanning && inputComp)
        inputComp->BindAction(IA_StartScanning, ETriggerEvent::Triggered, this, &UScanSurrounding::StartScan);
    for (TActorIterator<ACheckpointManager> it(GetWorld()); it; ++it) {
        if (ACheckpointManager* manager = *it) {
            manager->finishedMap = false;
            break;
        }
    }
}

void UScanSurrounding::StartScan() {
    if (bIsScanRunning)
        return;
    bIsScanRunning = bIsScanning = true;
    bScanComplete = false;
    visitedAngles.Reset();
    AActor *owner = GetOwner();
    if (owner)
        lastYaw = owner->GetActorRotation().Yaw;
    OnScanStart.Broadcast();
    FVector InitialLocation = owner ? owner->GetActorLocation() : FVector::ZeroVector;
    Async(EAsyncExecution::Thread, [this, InitialLocation]() { ON_SCOPE_EXIT{ bIsScanRunning = false; }; HandleScan(InitialLocation); });
}

void UScanSurrounding::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction *thisTickFunction) {
    if (IsBeingDestroyed() || !GetOwner()) 
        return;
    Super::TickComponent(deltaTime, tickType, thisTickFunction);
    const int32 maxDestroysPerFrame = 500;
    int32 destroyCount = 0;
    AActor *actorToDestroy;
    while (destroyCount++ < maxDestroysPerFrame && destroyQueue.Dequeue(actorToDestroy))
        if (actorToDestroy && !actorToDestroy->IsPendingKillPending())
            actorToDestroy->Destroy();
    if (!bIsScanning || bScanComplete) 
        return;
    HandleSpinning();
    FVector hitLoc;
    while (hitQueue.Dequeue(hitLoc)) {
        if (!spawnedHitLocations.Contains(hitLoc)) {
            if (AActor *spawned = GetWorld()->SpawnActor<AActor>(ScanPointActorClass, hitLoc, FRotator::ZeroRotator)) {
                spawnedActors.Add(spawned);
                spawnedHitLocations.Add(hitLoc);
            }
        }
    }
}

void UScanSurrounding::HandleSpinning() {
    AActor *owner = GetOwner();
    if (!owner) 
        return;
    const float curYaw = FMath::Wrap(float(owner->GetActorRotation().Yaw), 0.f, 360.f);
    const float prevYaw = FMath::Wrap(lastYaw, 0.f, 360.f);
    const float delta = FMath::FindDeltaAngleDegrees(prevYaw, curYaw);
    const int32 stepCount = FMath::CeilToInt(FMath::Abs(delta) / segmentSizeDegrees);
    const float direction = (delta >= 0.0f) ? 1.0f : -1.0f;
    for (int32 step = 0; step <= stepCount; step++) {
        float interpYaw = FMath::Wrap(prevYaw + direction * step * segmentSizeDegrees, 0.f, 360.f);
        int32 segment = FMath::FloorToInt(interpYaw / segmentSizeDegrees);
        visitedAngles.Add(segment);
    }
    lastYaw = curYaw;
    if (visitedAngles.Num() >= requiredSegmentCount) {
        bScanComplete = true;
        bIsScanning = bIsScanRunning = false;
        for (TActorIterator<ACheckpointManager> it(GetWorld()); it; ++it) {
            if (ACheckpointManager *manager = *it) {
                manager->ScanState = EScanProgress::Completed;
                break;
            }
        }
    }
}

void UScanSurrounding::HandleScan(FVector startPoint) {
    TArray<FLidarPoint> points;
    points.Add(FLidarPoint(startPoint));
    static float currentPitch = -135.f * 0.5f;
    UBakedColorMapManager* colorMapMgr = UBakedColorMapManager::Get(GetWorld());
    if (!colorMapMgr)
        return;

    const int32 mapSize = colorMapMgr->GetBakeResolution();

    while (bIsScanning) {
        AActor* owner = GetOwner();
        if (!owner || IsBeingDestroyed())
            break;

        if (currentPitch > 135.f * 0.5f)
            currentPitch = -135.f * 0.5f;

        FVector start = owner->GetActorLocation();
        FVector forward = owner->GetActorRightVector();
        FVector axis = FVector::CrossProduct(forward, FVector::UpVector).GetSafeNormal();
        FVector dir = FQuat(axis, FMath::DegreesToRadians(currentPitch)).RotateVector(forward);
        FVector end = start + dir * 800.f;

        FHitResult hit;
        FCollisionQueryParams params(TEXT("LidarTrace"), true, owner);
        params.bReturnPhysicalMaterial = params.bReturnFaceIndex = params.bTraceComplex = true;

        if (!GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_Visibility, params)) {
            currentPitch += 1.f;
            continue;
        }

        FVector2D uv;
        if (!UGameplayStatics::FindCollisionUV(hit, 0, uv)) {
            currentPitch += 1.f;
            continue;
        }

        UStaticMeshComponent *meshComp = Cast<UStaticMeshComponent>(hit.GetComponent());
        if (!meshComp) {
            currentPitch += 1.f;
            continue;
        }

        UStaticMesh *mesh = meshComp->GetStaticMesh();
        if (!mesh) {
            currentPitch += 1.f;
            continue;
        }

        int32 materialIndex = INDEX_NONE;
        FStaticMeshRenderData *renderData = mesh->GetRenderData();
        if (renderData && renderData->LODResources.Num() > 0) {
            const FStaticMeshLODResources &lod = renderData->LODResources[0];
            for (const FStaticMeshSection &section : lod.Sections) {
                int32 firstTri = section.FirstIndex / 3;
                if (hit.FaceIndex >= firstTri && hit.FaceIndex < firstTri + (int32)section.NumTriangles) {
                    materialIndex = section.MaterialIndex;
                    break;
                }
            }
        }

        if (materialIndex == INDEX_NONE)
            materialIndex = 0;

        UMaterialInterface *mat = meshComp->GetMaterial(materialIndex);
        if (!mat) {
            currentPitch += 1.f;
            continue;
        }

        const TArray<FColor> *colorMap = colorMapMgr->GetColorMap(mesh, mat);
        if (!colorMap) {
            currentPitch += 1.f;
            continue;
        }

        float U = FMath::Fmod(uv.X, 1.0f); 
        if (U < 0) 
            U += 1.0f;
        float V = FMath::Fmod(uv.Y, 1.0f); 
        if (V < 0) 
            V += 1.0f;

        int32 x = FMath::Clamp(FMath::FloorToInt(U * mapSize), 0, mapSize - 1);
        int32 y = FMath::Clamp(FMath::FloorToInt(V * mapSize), 0, mapSize - 1);
        int32 colorIndex = y * mapSize + x;

        if (!colorMap->IsValidIndex(colorIndex)) {
            currentPitch += 1.f;
            continue;
        }

        points.Add(FLidarPoint(hit.ImpactPoint, (*colorMap)[colorIndex]));
        hitQueue.Enqueue(hit.ImpactPoint);
        currentPitch += 1.f;
    }

    AsyncTask(ENamedThreads::GameThread, [this]() {
        for (AActor *actor : spawnedActors) {
            if (actor && !actor->IsPendingKillPending()) {
                destroyQueue.Enqueue(actor);
                actor->SetActorHiddenInGame(true);
                actor->UpdateComponentVisibility();
            }
        }
        spawnedActors.Empty();
        spawnedHitLocations.Reset();
    });

    Async(EAsyncExecution::Thread, [points = MoveTemp(points), scanCounter = scanCounter]() {
        FString txtContent;
        for (int i = 0; i < points.Num(); i++) {
            const FLidarPoint &point = points[i];
            txtContent += FString::Printf(TEXT("%.3f %.3f %.3f %d %d %d\n"), point.location.X, point.location.Y, point.location.Z, point.color.R, point.color.G, point.color.B);
            if (i % 250 == 0)
                FPlatformProcess::Sleep(0.0005f);
        }

        FString txtPath = FPaths::ProjectSavedDir() / TEXT("ScannedData/scan_points_") + FString::FromInt(scanCounter) + TEXT(".txt");
        FString txtDir = FPaths::GetPath(txtPath);
        if (!IFileManager::Get().DirectoryExists(*txtDir))
            IFileManager::Get().MakeDirectory(*txtDir, true);

        FFileHelper::SaveStringToFile(txtContent, *txtPath);
    });

    scanCounter++;
}