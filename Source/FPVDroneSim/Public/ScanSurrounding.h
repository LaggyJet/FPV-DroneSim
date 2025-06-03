#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ScanSurrounding.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnScanStart);

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

		virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	protected:
		virtual void BeginPlay() override;

	private:
		bool bIsScanning = false;
		bool bScanComplete = false;
		float lastYaw = 0.f;
		TSet<int32> VisitedAngles;
		const float SegmentSizeDegrees = 5.0f;
		const int32 RequiredSegmentCount = 360 / SegmentSizeDegrees;
};
