// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// UI 전용 로그 카테고리.
// LogTemp를 쓰지 않는다 — 가드에서 조용히 return하고 끝내면 원인을 찾을 수 없기 때문에
// UI에서 무언가를 건너뛸 때는 반드시 이 카테고리로 이유를 남긴다.
DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaUI, Log, All);
