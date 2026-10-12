// Fill out your copyright notice in the Description page of Project Settings.

/**
 * PickMesh 선택 규칙
 *  1. Seed 한 비트로 성별을 정한다. 뽑힌 성별의 풀(FemaleMeshVariants / MaleMeshVariants)이 비어 있으면 반대 성별 풀로 간다.
 *  2. 두 성별 풀이 모두 비어 있으면 성별 구분 없는 풀(Mesh + MeshVariants)에서 고른다.
 *  3. 풀 안에서의 인덱스는 성별 비트와 다른 해시로 고른다. 같은 Seed는 항상 같은 결과를 낸다.
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

	using FMeshPool = TArray<const TSoftObjectPtr<USkeletalMesh>*, TInlineAllocator<32>>;

	void AppendValid(FMeshPool& Pool, const TArray<TSoftObjectPtr<USkeletalMesh>>& Source)
	{
		for (const TSoftObjectPtr<USkeletalMesh>& Entry : Source)
		{
			if (!Entry.IsNull())
			{
				Pool.Add(&Entry);
			}
		}
	}
}

TSoftObjectPtr<USkeletalMesh> FFF_CrowdAppearenceRow::PickMesh(int32 Seed) const
{
	FMeshPool Female;
	FMeshPool Male;
	AppendValid(Female, FemaleMeshVariants);
	AppendValid(Male, MaleMeshVariants);

	FMeshPool Pool;
	if (!Female.IsEmpty() || !Male.IsEmpty())
	{
		const bool bWantFemale = (MixSeed(Seed, GenderSalt) & 1u) != 0u;
		const bool bUseFemale = bWantFemale ? !Female.IsEmpty() : Male.IsEmpty();
		Pool = bUseFemale ? Female : Male;
	}
	else
	{
		if (!Mesh.IsNull())
		{
			Pool.Add(&Mesh);
		}
		AppendValid(Pool, MeshVariants);
	}

	if (Pool.IsEmpty())
	{
		return TSoftObjectPtr<USkeletalMesh>();
	}

	return *Pool[MixSeed(Seed, PickSalt) % static_cast<uint32>(Pool.Num())];
}
