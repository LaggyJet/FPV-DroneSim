#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "ScanSurrounding.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScanStart);

USTRUCT(BlueprintType)
struct FLidarPoint {
    GENERATED_BODY()

    public:
        FVector location;
        FColor color;

        FLidarPoint() = default;
        FLidarPoint(const FVector &inLocation, const FColor &inColor = FColor::Blue) : location(inLocation), color(inColor) {}
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPVDRONESIM_API UScanSurrounding : public USceneComponent {
    GENERATED_BODY()

    public:
        UScanSurrounding();

        UFUNCTION(BlueprintCallable)
        void StartScan();

        UFUNCTION(BlueprintCallable)
        bool IsScanComplete() const { return bScanComplete; }

        UPROPERTY(BlueprintAssignable)
        FOnScanStart OnScanStart;

        UPROPERTY(EditAnywhere)
        UInputAction *IA_StartScanning = nullptr;

        UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Scan Surroundings")
        TSubclassOf<AActor> ScanPointActorClass;

        virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction *thisTickFunction) override;

    protected:
        virtual void BeginPlay() override;

    private:
        FThreadSafeBool bIsScanning = false;
        FThreadSafeBool bIsScanRunning = false;
        bool bScanComplete = false;
        float lastYaw = 0.f;
        TSet<int32> visitedAngles;
        const float segmentSizeDegrees = 10.0f;
        const int32 requiredSegmentCount = 360 / segmentSizeDegrees;
        int32 scanCounter = 0;
        TArray<AActor*> spawnedActors;
        TSet<FVector> spawnedHitLocations;
        TQueue<FVector, EQueueMode::Mpsc> hitQueue;
        TQueue<AActor*, EQueueMode::Mpsc> destroyQueue;

        void HandleSpinning();
        void HandleScan(FVector startPoint);
};
