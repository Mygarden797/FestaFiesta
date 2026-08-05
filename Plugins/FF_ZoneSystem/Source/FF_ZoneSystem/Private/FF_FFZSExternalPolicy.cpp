/**
 * @file FF_FFZSExternalPolicy.cpp
 * @brief UHT가 외부 정책 인터페이스 타입을 링크할 수 있게 하는 구현 단위다.
 * 흐름: 실제 정책 콜백 구현은 각 외부 소유자의 UObject에 있다.
 * 불변식: 플러그인은 기본 허용 정책이나 숨은 전역 상태를 만들지 않는다.
 * 실패/성능: 런타임 실행 코드와 지속 비용이 의도적으로 없다.
 */

#include "FF_FFZSExternalPolicy.h"

// BlueprintNativeEvent의 실제 기본 thunk와 리플렉션 코드는 UHT가 생성한다.
// 이 파일에 기본 허용 구현을 두지 않아 정책 누락이 숨겨지지 않게 한다.

