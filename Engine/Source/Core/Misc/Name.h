#pragma once

#include "Core/CoreMinimal.h"

class ENGINE_API FName
{
  public:
    FName() : Index(0) {}
    FName(const wchar_t *InString);
    FName(const FWString &InString);

    // 가장 중요한 연산자: 문자열 비교가 아닌 정수 비교로 끝납니다.
    bool operator==(const FName &Other) const { return Index == Other.Index; }
    bool operator!=(const FName &Other) const { return Index != Other.Index; }

    // 디버깅이나 UI 출력을 위해 다시 문자열로 바꾸는 함수
    FWString ToString() const;

    uint32 GetIndex() const { return Index; }

  private:
    uint32 Index; // 실제 데이터는 이 숫자 하나뿐!
};

struct FNameTable
{

    FNameTable() { AddName(L"None"); }

    uint32 AddName(const FWString &InString)
    {
        auto It = StringToIndex.find(InString);
        if (It != StringToIndex.end())
        {
            return It->second;
        }

        uint32 NewIndex = static_cast<uint32>(IndexToString.size());
        IndexToString.push_back(InString);
        StringToIndex[InString] = NewIndex;
        return NewIndex;
    }

  public:
    // 문자열 -> 인덱스 검색용 (생성 시 사용)
    TMap<FWString, uint32> StringToIndex;
    // 인덱스 -> 문자열 검색용 (ToString 시 사용)
    TArray<FWString> IndexToString;
};
