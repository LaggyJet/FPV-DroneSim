#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FPVDroneFunctionLibrary.generated.h"

UENUM(BlueprintType)
enum class EWeatherEnum : uint8 {
	Default,
	LightWind,
	HeavyWind,
	LightRain,
	HeavyRain,
	Night
};

UENUM(BlueprintType)
enum class EScanProgress : uint8 {
	NotStarted,
	Started,
	Completed
};

UCLASS()
class FPVDRONESIM_API UFPVDroneFunctionLibrary : public UBlueprintFunctionLibrary {
	GENERATED_BODY()
	
	public:
		UFUNCTION(BlueprintPure, Category = "Enum")
		static EWeatherEnum StringToWeatherEnum(const FString& Input);

		UFUNCTION(BlueprintPure, Category = "Menu")
		static TArray<FString> GetPlayableMaps();
};
