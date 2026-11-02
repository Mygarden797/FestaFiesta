// FFZAAuthoredZoneCustomization.h
// 역할: FF Authored Zone 디테일 패널 정리.
//  - "Zone Definition" 분류를 맨 위로 올린다.
//  - 정의 에셋이 지정된 Zone은 Zone ID / 표시 이름 / 허용 시설 규칙을 회색(수정 불가)으로 잠근다.
//    → 값은 항상 정의 에셋에서 오므로 손으로 고쳐 어긋나는 일이 없다.
#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class FFFZAAuthoredZoneCustomization final : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> MakeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
