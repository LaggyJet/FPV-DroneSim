#include "BakedColorMapManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "HAL/PlatformProcess.h"
#include "Engine/Engine.h"

UBakedColorMapManager::~UBakedColorMapManager() {
    CleanupRenderingActors();

    if (sharedRenderTarget && sharedRenderTarget->IsValidLowLevel() && sharedRenderTarget->IsRooted()) {
        sharedRenderTarget->RemoveFromRoot();
        sharedRenderTarget = nullptr;
    }
}


TMap<UWorld*, UBakedColorMapManager*> &UBakedColorMapManager::GetInstanceMap() {
    static TMap<UWorld*, UBakedColorMapManager*> instances;
    return instances;
}

UBakedColorMapManager *UBakedColorMapManager::Get(UWorld *world) {
    if (!world) 
        return nullptr;

    TMap<UWorld*, UBakedColorMapManager*>& instances = GetInstanceMap();
    if (UBakedColorMapManager **found = instances.Find(world))
        return *found;

    UBakedColorMapManager *newInstance = NewObject<UBakedColorMapManager>(world->GetGameInstance());
    newInstance->AddToRoot();
    instances.Add(world, newInstance);

    static bool bCleanupBound = false;
    if (!bCleanupBound) {
        FWorldDelegates::OnWorldCleanup.AddStatic([](UWorld *world, bool, bool) { UBakedColorMapManager::ShutdownForWorld(world); });
        bCleanupBound = true;
    }

    return newInstance;
}

void UBakedColorMapManager::ShutdownForWorld(UWorld *world) {
    TMap<UWorld*, UBakedColorMapManager*> &instances = GetInstanceMap();
    if (UBakedColorMapManager* instance = instances.FindRef(world)) {
        instance->CleanupRenderingActors();
        if (instance->sharedRenderTarget && instance->sharedRenderTarget->IsRooted())
            instance->sharedRenderTarget->RemoveFromRoot();
        if (instance->IsRooted())
            instance->RemoveFromRoot();
        instances.Remove(world);
    }
}

const TArray<FColor> *UBakedColorMapManager::GetColorMap(UStaticMesh *mesh, UMaterialInterface *material) const { 
    return (!mesh || !material) ? nullptr : colorMapCache.Find(FMeshMaterialKey{ mesh, material }); 
}

void UBakedColorMapManager::ClearCache() {
    colorMapCache.Empty();
    CleanupRenderingActors();
}

FString UBakedColorMapManager::CleanMapName(const FString &mapName) const {
    const FString prefix = TEXT("UEDPIE_0_");
    return mapName.StartsWith(prefix) ? mapName.RightChop(prefix.Len()) : mapName;
}

FString UBakedColorMapManager::GenerateFileName(const UStaticMesh *mesh, const UMaterialInterface *material) const {
    return FString::Printf(TEXT("%s_%s.bin"), *(mesh ? mesh->GetName() : TEXT("NoneMesh")), *(material ? material->GetName() : TEXT("NoneMat")));
}

bool UBakedColorMapManager::SaveColorMapToDisk(const FString &filePath, const TArray<FColor> &colorMap) {
    if (colorMap.Num() == 0) 
        return false;
    TArrayView<const uint8> bytes(reinterpret_cast<const uint8*>(colorMap.GetData()), colorMap.Num() * sizeof(FColor));
    return FFileHelper::SaveArrayToFile(bytes, *filePath);
}

bool UBakedColorMapManager::LoadColorMapFromDisk(const FString &filePath, int32 expectedSize, TArray<FColor> &outColorMap) {
    TArray<uint8> rawData;
    if (!FFileHelper::LoadFileToArray(rawData, *filePath)) 
        return false;
    if (rawData.Num() != expectedSize * sizeof(FColor)) 
        return false;
    outColorMap.SetNumUninitialized(expectedSize);
    FMemory::Memcpy(outColorMap.GetData(), rawData.GetData(), rawData.Num());
    return true;
}

void UBakedColorMapManager::GatherMeshMaterialCombos(UWorld *world, TSet<FMeshMaterialKey> &outCombos) {
    for (TActorIterator<AActor> it(world); it; ++it) {
        TArray<UStaticMeshComponent*> meshComps;
        it->GetComponents<UStaticMeshComponent>(meshComps);
        for (UStaticMeshComponent *meshComp : meshComps)
            if (UStaticMesh *mesh = meshComp->GetStaticMesh())
                for (int i = 0; i < meshComp->GetNumMaterials(); i++)
                    if (UMaterialInterface *mat = meshComp->GetMaterial(i))
                        outCombos.Add(FMeshMaterialKey{ mesh, mat });
    }
}

bool UBakedColorMapManager::SetupRenderingActorsIfNeeded(UWorld *world) {
    if (!sharedRenderTarget) {
        sharedRenderTarget = NewObject<UTextureRenderTarget2D>();
        sharedRenderTarget->AddToRoot();
        sharedRenderTarget->InitAutoFormat(bakeResolution, bakeResolution);
        sharedRenderTarget->RenderTargetFormat = RTF_RGBA8;
        sharedRenderTarget->ClearColor = FLinearColor::Black;
        sharedRenderTarget->TargetGamma = 1.0f;
        sharedRenderTarget->UpdateResourceImmediate(true);
    }

    if (!sharedPlaneActor) {
        static UStaticMesh *planeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
        if (!planeMesh)
            return false;

        sharedPlaneActor = world->SpawnActor<AStaticMeshActor>();
        if (!sharedPlaneActor)
            return false;

        UStaticMeshComponent *meshComp = sharedPlaneActor->GetStaticMeshComponent();
        meshComp->SetMobility(EComponentMobility::Movable);
        meshComp->SetStaticMesh(planeMesh);
        meshComp->SetVisibility(true);
        meshComp->SetHiddenInGame(false);
        meshComp->MarkRenderStateDirty();

        sharedPlaneActor->SetActorScale3D(FVector(5.f));
        sharedPlaneActor->SetActorLocation(FVector::ZeroVector);
        sharedPlaneActor->RegisterAllComponents();
    }

    if (!sharedCaptureActor) {
        sharedCaptureActor = world->SpawnActor<ASceneCapture2D>();
        sharedCaptureActor->SetActorLocation(FVector(0, 0, 20.f));
        sharedCaptureActor->SetActorRotation(FRotator(-90.f, 0.f, 0.f));
    }

    if (!spawnedDirectionalLight) {
        spawnedDirectionalLight = world->SpawnActor<ADirectionalLight>(FVector(0, 0, 300.f), FRotator(-45.f, 0.f, 0.f));
        if (spawnedDirectionalLight) {
            auto *dirLightComp = Cast<UDirectionalLightComponent>(spawnedDirectionalLight->GetLightComponent());
            if (dirLightComp) {
                dirLightComp->SetIntensity(20000.f);
                dirLightComp->SetCastShadows(true);
            }
        }
    }

    auto *captureComp = sharedCaptureActor->GetCaptureComponent2D();
    captureComp->TextureTarget = sharedRenderTarget;
    captureComp->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
    captureComp->ProjectionType = ECameraProjectionMode::Orthographic;
    captureComp->OrthoWidth = 400.f;
    captureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
    captureComp->bCaptureEveryFrame = false;
    captureComp->bCaptureOnMovement = false;

    return true;
}

bool UBakedColorMapManager::BakeMaterialOnPlane(UWorld *world, UMaterialInterface *material, int32 resolution, TArray<FColor> &outColorMap) {
    if (!SetupRenderingActorsIfNeeded(world))
        return false;

    auto *meshComp = sharedPlaneActor->GetStaticMeshComponent();
    if (!meshComp || !material) 
        return false;

    meshComp->SetMaterial(0, material);
    meshComp->RecreateRenderState_Concurrent();
    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.05f);

    auto *captureComp = sharedCaptureActor->GetCaptureComponent2D();
    captureComp->CaptureScene();

    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.1f);

    outColorMap.Reset();
    bool bSuccess = sharedRenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(outColorMap);
    return bSuccess && outColorMap.Num() == resolution * resolution;
}

void UBakedColorMapManager::CleanupRenderingActors() {
    if (sharedPlaneActor) {
        sharedPlaneActor->Destroy();
        sharedPlaneActor = nullptr;
    }
    if (sharedCaptureActor) {
        sharedCaptureActor->Destroy();
        sharedCaptureActor = nullptr;
    }
    if (spawnedDirectionalLight) {
        spawnedDirectionalLight->Destroy();
        spawnedDirectionalLight = nullptr;
    }
    if (sharedRenderTarget && sharedRenderTarget->IsRooted()) {
        sharedRenderTarget->RemoveFromRoot();
        sharedRenderTarget = nullptr;
    }
}

void UBakedColorMapManager::BakeAndSaveAllMapsIfMissing(const FString &mapName, TFunction<void()> onComplete) {
    UWorld *world = GetWorld();
    if (!world) 
        return;

    FString basePath = FPaths::ProjectSavedDir() / TEXT("BakedMatData") / CleanMapName(mapName);
    IFileManager::Get().MakeDirectory(*basePath, true);
    const int32 resolution = GetBakeResolution();

    AsyncTask(ENamedThreads::GameThread, [this, world, basePath, resolution, onComplete]() {
        TSet<FMeshMaterialKey> combos;
        GatherMeshMaterialCombos(world, combos);

        for (const auto &key : combos) {
            FString fullPath = basePath / GenerateFileName(key.mesh, key.material);
            TArray<FColor> colorMap;
            if (!FPaths::FileExists(fullPath) || !LoadColorMapFromDisk(fullPath, resolution * resolution, colorMap))
                if (BakeMaterialOnPlane(world, key.material, resolution, colorMap))
                    SaveColorMapToDisk(fullPath, colorMap);
            if (colorMap.Num() > 0)
                colorMapCache.Add(key, MoveTemp(colorMap));
        }

        CleanupRenderingActors();
        if (onComplete)
            AsyncTask(ENamedThreads::GameThread, onComplete);
    });
}