#pragma once
#include "Core/Containers/Array.h"
#include "Core/Containers/Map.h"
#include "Core/HAL/PlatformTypes.h"

#include <memory>

class FManualMemoryCategoryHandle;

namespace Engine::Core::Misc
{
    class ENGINE_API FNameSubsystem
    {
        friend struct FName;

      private:
        FNameSubsystem();
        ~FNameSubsystem();
        FNameSubsystem(const FNameSubsystem& Other) = delete; 
        FNameSubsystem& operator=(const FNameSubsystem& Other) = delete; 

      public:
        static FNameSubsystem& Get();
        static void Init();
        static void Shutdown();

      private:
        [[nodiscard]] FString GetString(const int32 InIdx) const;
        [[nodiscard]]  int32 FindOrAdd(const FString& InStr);

      private:
        static FNameSubsystem* Instance;
        TMap<FString, int32>   StringToIndexMap;
        TArray<FString>        IndexToStringTable;
        std::unique_ptr<FManualMemoryCategoryHandle> MemoryTrackHandle;
    };
} // namespace Engine::Core::Misc
