#include "CheckpointManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"

ACheckpointManager::ACheckpointManager() { PrimaryActorTick.bCanEverTick = true; }

void ACheckpointManager::Tick(float deltaTime) {
	Super::Tick(deltaTime);
    if (finishedMap) {
        finishedMap = false;
        Async(EAsyncExecution::Thread, []() {
            const FString scanDir = FPaths::ProjectSavedDir() / TEXT("ScannedData");
            const FString combinedPath = scanDir / TEXT("CombinedMapPoints.txt");
            TArray<FString> fileNames;
            IFileManager::Get().FindFiles(fileNames, *scanDir, TEXT("txt"));
            FString combinedContent;
            for (const FString& fileName : fileNames) {
                if (fileName.StartsWith(TEXT("scan_points_"))) {
                    FString fileContent;
                    if (FFileHelper::LoadFileToString(fileContent, *(scanDir / fileName)))
                        combinedContent += fileContent;
                }
            }
            FFileHelper::SaveStringToFile(combinedContent, *combinedPath);
        });
    }
}

