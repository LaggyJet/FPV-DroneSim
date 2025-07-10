// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPVDroneFunctionLibrary.h"
#include "CheckpointManager.generated.h"

UCLASS()
class FPVDRONESIM_API ACheckpointManager : public AActor {
	GENERATED_BODY()
	
	public:	
		ACheckpointManager();

		virtual void Tick(float deltaTime) override;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
		EScanProgress ScanState;

		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scan")
		bool finishedMap = false;
};