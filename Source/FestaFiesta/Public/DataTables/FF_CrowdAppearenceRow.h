// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Engine/DataTable.h"
#include "CoreMinimal.h"
#include "CrowdSystem/Components/FF_CrowdAnimInstance.h"
#include "FF_CrowdAppearenceRow.generated.h"

USTRUCT(BlueprintType)
struct FFF_CrowdAppearanceVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	TSoftClassPtr<UFF_CrowdAnimInstance> AnimClass;
};

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crowd")
	TArray<FFF_CrowdAppearanceVariant> AnimVariants;

	//Mesh + MeshVariants 중 Seed로 고정된 하나를 고른다. 후보가 없으면 빈 포인터
	TSoftObjectPtr<USkeletalMesh> PickMesh(int32 Seed) const;

	void PickAppearance(int32 Seed, TSoftObjectPtr<USkeletalMesh>& OutMesh, TSoftClassPtr<UFF_CrowdAnimInstance>& OutAnimClass) const;

	//Seed로 성별을 정한다. 호출부가 행 이름의 _female/_male 접미사를 붙여 행을 찾는 데 쓴다
	static bool IsFemaleSeed(int32 Seed);
	
	
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
