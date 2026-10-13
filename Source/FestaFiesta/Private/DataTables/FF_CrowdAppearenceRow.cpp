// Fill out your copyright notice in the Description page of Project Settings.

/**
 * 성별과 PickMesh 규칙
 *  1. 성별은 행 이름으로 나눈다. IsFemaleSeed 가 Seed 한 비트로 정하고, 호출부가 {ArchetypeID}_female / _male 행을 찾는다.
 *  2. PickMesh / PickAppearance 는 한 행 안에서 Mesh + Variants 를 후보 풀로 삼아 Seed 로 하나를 고른다. Variants 는 메시와 AnimClass 를 한 쌍으로 들고, AnimClass 가 비면 행의 AnimClass 를 쓴다.
 *  3. 같은 Seed 는 항상 같은 결과를 낸다.
 */

#include "DataTables/FF_CrowdAppearenceRow.h"

namespace
{
	constexpr uint32 GenderSalt = 0x85EBCA6Bu;
	constexpr uint32 PickSalt = 0x9E3779B1u;

	uint32 MixSeed(int32 Seed, uint32 Salt)
	{
		uint32 Hash = static_cast<uint32>(Seed) * Salt;
		Hash ^= Hash >> 16;
		Hash *= 0x7FEB352Du;
		Hash ^= Hash >> 15;
		return Hash;
	}
}

bool FFF_CrowdAppearenceRow::IsFemaleSeed(int32 Seed)
{
	return (MixSeed(Seed, GenderSalt) & 1u) != 0u;
}

void FFF_CrowdAppearenceRow::PickAppearance(int32 Seed, TSoftObjectPtr<USkeletalMesh>& OutMesh, TSoftClassPtr<UFF_CrowdAnimInstance>& OutAnimClass) const
{
	struct FCandidate
	{
		const TSoftObjectPtr<USkeletalMesh>* MeshPtr;
		const TSoftClassPtr<UFF_CrowdAnimInstance>* AnimClassPtr;
	};

	TArray<FCandidate, TInlineAllocator<32>> Pool;
	if (!Mesh.IsNull())
	{
		Pool.Add({ &Mesh, &AnimClass });
	}
	for (const FFF_CrowdAppearanceVariant& Entry : Variants)
	{
		if (!Entry.Mesh.IsNull())
		{
			Pool.Add({ &Entry.Mesh, Entry.AnimClass.IsNull() ? &AnimClass : &Entry.AnimClass });
		}
	}

	if (Pool.IsEmpty())
	{
		OutMesh = TSoftObjectPtr<USkeletalMesh>();
		OutAnimClass = AnimClass;
		return;
	}

	const FCandidate& Picked = Pool[MixSeed(Seed, PickSalt) % static_cast<uint32>(Pool.Num())];
	OutMesh = *Picked.MeshPtr;
	OutAnimClass = *Picked.AnimClassPtr;
}

TSoftObjectPtr<USkeletalMesh> FFF_CrowdAppearenceRow::PickMesh(int32 Seed) const
{
	TSoftObjectPtr<USkeletalMesh> PickedMesh;
	TSoftClassPtr<UFF_CrowdAnimInstance> PickedAnimClass;
	PickAppearance(Seed, PickedMesh, PickedAnimClass);
	return PickedMesh;
}
