#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Containers/Map.h"
#include "Containers/Array.h"
#include "Misc/Paths.h"
#include "HAL/ThreadSafeBool.h"
#include "Async/AsyncWork.h"
#include "UObject/NoExportTypes.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "BakedColorMapManager.generated.h"

UCLASS()
class FPVDRONESIM_API UBakedColorMapManager : public UObject {
    GENERATED_BODY()

    public:
        static UBakedColorMapManager* Get(UWorld *world);
        static void ShutdownForWorld(UWorld *world);
        void BakeAndSaveAllMapsIfMissing(const FString &mapName, TFunction<void()> onComplete = nullptr);
        const TArray<FColor> *GetColorMap(UStaticMesh *mesh, UMaterialInterface *material) const;
        void ClearCache();
        static constexpr int32 bakeResolution = 256;
        FORCEINLINE int32 GetBakeResolution() const { return bakeResolution; }
        virtual ~UBakedColorMapManager() override;

    protected:
        struct FMeshMaterialKey {
            UStaticMesh *mesh = nullptr;
            UMaterialInterface *material = nullptr;
            bool operator==(const FMeshMaterialKey &other) const { return mesh == other.mesh && material == other.material; }
        };
        friend uint32 GetTypeHash(const FMeshMaterialKey &key) { return HashCombine(GetTypeHash(key.mesh), GetTypeHash(key.material)); }
        TMap<FMeshMaterialKey, TArray<FColor>> colorMapCache;
        FString GenerateFileName(const UStaticMesh *mesh, const UMaterialInterface *material) const;
        bool SaveColorMapToDisk(const FString &filePath, const TArray<FColor> &colorMap);
        bool LoadColorMapFromDisk(const FString &filePath, int32 expectedSize, TArray<FColor> &outColorMap);
        void GatherMeshMaterialCombos(UWorld *world, TSet<FMeshMaterialKey> &outCombos);
        bool BakeMaterialOnPlane(UWorld *world, UMaterialInterface *material, int32 resolution, TArray<FColor> &outColorMap);
        FString CleanMapName(const FString &mapName) const;
        bool SetupRenderingActorsIfNeeded(UWorld *world);
        void CleanupRenderingActors();
        AStaticMeshActor *sharedPlaneActor = nullptr;
        ASceneCapture2D *sharedCaptureActor = nullptr;
        UTextureRenderTarget2D *sharedRenderTarget = nullptr;
        ADirectionalLight *directionalLightActor = nullptr;
        ADirectionalLight *spawnedDirectionalLight = nullptr;

    private:
        static TMap<UWorld*, UBakedColorMapManager*> &GetInstanceMap();
};