#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SpinObserverComponent.generated.h"

UENUM(BlueprintType)
enum class ESpinAxis : uint8
{
	X UMETA(DisplayName = "X Axis"),
	Y UMETA(DisplayName = "Y Axis"),
	Z UMETA(DisplayName = "Z Axis")
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPVDRONESIM_API USpinObserverComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USpinObserverComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Observed float value (0 to 1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin")
	float SpinAmount = 0.0f;

	/** If true, spin is reversed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin")
	bool bReverseDirection = false;

	/** Axis to spin around */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin")
	ESpinAxis SpinAxis = ESpinAxis::Z;

	/** Max spin speed in degrees per second at SpinAmount = 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spin")
	float MaxSpinSpeed = 180.0f;
};
