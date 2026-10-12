// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Engine/DataTable.h"
#include "CoreMinimal.h"
#include "CrowdSystem/Components/FF_CrowdAnimInstance.h"
#include "FF_CrowdAppearenceRow.generated.h"

/**
 * 
 */
USTRUCT(Blueprintable)
struct FFF_CrowdAppearenceRow : public FTableRowBase
{
	
public:
	GENERATED_BODY()
	
	//아키타입 ID
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	FName ArchtypeID;
	
	//아동인지의 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	bool bIsChild = false;
	
	//설정할 외형
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	TSoftObjectPtr<USkeletalMesh> Mesh;
	
	//설정한 외형에 쓸 AnimBP 클래스. 비워 두면 BP_Crowd에 지정된 기본 AnimClass로 되돌아간다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	TSoftClassPtr<UFF_CrowdAnimInstance> AnimClass;
	
	
	//같은 아키타입 내 외형 변주
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	TArray<TSoftObjectPtr<USkeletalMesh>> MeshVariants;

	//성별 구분 외형 풀. 하나라도 채워 두면 Seed로 성별을 정한 뒤 그 풀에서 고른다. 한쪽만 채우면 그쪽으로만 나온다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	TArray<TSoftObjectPtr<USkeletalMesh>> FemaleMeshVariants;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	TArray<TSoftObjectPtr<USkeletalMesh>> MaleMeshVariants;

	//성별 풀이 모두 비어 있으면 Mesh + MeshVariants 중에서, 아니면 성별 풀에서 Seed로 고정된 하나를 고른다. 후보가 없으면 빈 포인터
	TSoftObjectPtr<USkeletalMesh> PickMesh(int32 Seed) const;
	
	
	//아동 슬롯용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	TSoftObjectPtr<USkeletalMesh> ChildMesh;

	//정면 축 보정. 메시 정면이 +X가 아닌 애셋을 위한 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	float MeshYawOffset = 0.f;


	//체격 수치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	float MeshUniformScale = 1.f;
};
