// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "FF_SaveArchive.generated.h"

/* 구조체를 바이트로 바꾸고 되돌리는 도우미.

   각 파트가 직렬화 코드를 직접 쓰면 저마다 다른 방식이 되고, 그중 하나가
   잘못돼도 저장 전체가 깨진다. 한 곳에 모아두면 고칠 데도 한 곳이다.
*/
UCLASS()
class FESTAFIESTA_API UFF_SaveArchive : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/* USTRUCT 하나를 바이트로 쓴다.

	   UPROPERTY 가 붙은 필드만 담긴다. 순수 C++ 멤버는 빠지므로,
	   저장하고 싶은 값에는 반드시 UPROPERTY 를 붙여야 한다.
	*/
	template <typename StructType>
	static void WriteStruct(TArray<uint8>& OutBytes, const StructType& Value)
	{
		OutBytes.Reset();

		FBufferArchive Buffer;
		FObjectAndNameAsStringProxyArchive Archive(Buffer, /*bInLoadIfFindFails*/ true);

		// SaveGame 플래그가 붙은 것만 고르지 않고 전부 담는다.
		// 구조체는 저장용으로 따로 만드는 것이라 걸러낼 이유가 없다.
		Archive.ArIsSaveGame = false;

		StructType Mutable = Value;
		StructType::StaticStruct()->SerializeItem(Archive, &Mutable, nullptr);

		OutBytes = MoveTemp(Buffer);
	}

	/* 바이트에서 구조체를 되돌린다.

	   빈 배열이면 false 를 돌려준다. 저장 당시에 그 시스템이 없었거나
	   옛 저장이라서 그렇고, 오류가 아니다.
	*/
	template <typename StructType>
	static bool ReadStruct(const TArray<uint8>& InBytes, StructType& OutValue)
	{
		if (InBytes.Num() == 0)
		{
			return false;
		}

		FMemoryReader Reader(InBytes, /*bIsPersistent*/ true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, /*bInLoadIfFindFails*/ true);
		Archive.ArIsSaveGame = false;

		StructType::StaticStruct()->SerializeItem(Archive, &OutValue, nullptr);

		/* 읽다가 끝에 닿았으면 파일이 잘렸다.

		   막지는 않는다. 앞부분은 정상일 수 있고, 여기서 버리면 되살릴 수 있는
		   것까지 버리게 된다. 판단은 호출부에 맡긴다.
		*/
		return !Archive.IsError();
	}
};
