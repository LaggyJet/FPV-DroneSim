#include "FPVDroneFunctionLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"

EWeatherEnum UFPVDroneFunctionLibrary::StringToWeatherEnum(const FString& Input) {
    const UEnum *Enum = StaticEnum<EWeatherEnum>();
    if (!Enum) return EWeatherEnum::Default;
    for (int32 i = 0; i < Enum->NumEnums(); ++i) {
        FString Name = Enum->GetNameStringByIndex(i);
        if (Name.Equals(Input.Replace(TEXT(" "), TEXT("")), ESearchCase::IgnoreCase))
            return static_cast<EWeatherEnum>(Enum->GetValueByIndex(i));
    }
    return EWeatherEnum::Default;
}

TArray<FString> UFPVDroneFunctionLibrary::GetPlayableMaps() {
    TArray<FString> OutMapNames;
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    FARFilter Filter;
    Filter.PackagePaths.Add("/Game/Maps/Playables");
    Filter.ClassNames.Add("/Script/Engine.World");
    Filter.bRecursivePaths = true;
    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssets(Filter, Assets);
    for (const FAssetData& Asset : Assets)
        OutMapNames.Add(Asset.PackageName.ToString());
    return OutMapNames;
}

