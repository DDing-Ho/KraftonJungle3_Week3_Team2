#include "Core/CoreMinimal.h"
#include "Engine/EngineStatics.h"
#include "Engine/MemoryProfiler.h"
#include "Object.h"

TArray<UObject*> GUObjectArray;

namespace
{
    struct FAllocatedObjectInfo
    {
        const char* TypeName = "UObject";
    };

    TMap<const void*, FAllocatedObjectInfo>& GetAllocatedObjectInfos()
    {
        static TMap<const void*, FAllocatedObjectInfo> AllocatedObjectInfos;
        return AllocatedObjectInfos;
    }

    const char* ResolveAllocatedObjectTypeName(const UObject* Object)
    {
        if (Object == nullptr)
        {
            return "UObject";
        }

        auto& Infos = GetAllocatedObjectInfos();
        const auto It = Infos.find(Object);
        if (It != Infos.end() && It->second.TypeName != nullptr)
        {
            return It->second.TypeName;
        }

        return "UObject";
    }

    FString ResolveObjectName(const UObject* Object)
    {
        if (Object == nullptr || !Object->Name.IsValid())
        {
            return "<unnamed>";
        }

        return Object->Name.ToFString();
    }
} // namespace

UObject::UObject()
{
	UUID = UEngineStatics::GenUUID();
	InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(this);

    const FString ObjectName = ResolveObjectName(this);
    UE_LOG(UObject, ELogVerbosity::Log, "Created %s (UUID=%u, Name=%s, Address=%p)",
           ResolveAllocatedObjectTypeName(this), UUID, ObjectName.c_str(), this);
}

UObject::~UObject()
{
    const FString ObjectName = ResolveObjectName(this);
    UE_LOG(UObject, ELogVerbosity::Log, "Destroyed %s (UUID=%u, Name=%s, Address=%p)",
           ResolveAllocatedObjectTypeName(this), UUID, ObjectName.c_str(), this);

	if (InternalIndex < GUObjectArray.size() && GUObjectArray[InternalIndex] == this)
	{
		GUObjectArray[InternalIndex] = nullptr;
	}
}

void* UObject::operator new(size_t Size)
{
    return AllocateObject(Size, "UObject");
}

void UObject::operator delete(void* Pointer, size_t Size)
{
    FreeObject(Pointer, Size);
}

void* UObject::AllocateObject(size_t Size, const char* InTypeName)
{
	UEngineStatics::TotalAllocatedBytes += static_cast<uint32>(Size);
	UEngineStatics::TotalAllocationCount++;
    FMemoryProfiler::Get().RegisterUObjectAllocation(InTypeName, Size);

	void* Pointer = ::operator new(Size);
    GetAllocatedObjectInfos()[Pointer] = {.TypeName = InTypeName != nullptr ? InTypeName : "UObject"};
	return Pointer;
}

void UObject::FreeObject(void* Pointer, size_t Size)
{
    if (Pointer == nullptr)
    {
        return;
    }

	UEngineStatics::TotalAllocatedBytes -= static_cast<uint32>(Size);
	UEngineStatics::TotalAllocationCount--;

    const char* TypeName = "UObject";
    auto&       Infos = GetAllocatedObjectInfos();
    const auto  It = Infos.find(Pointer);
    if (It != Infos.end() && It->second.TypeName != nullptr)
    {
        TypeName = It->second.TypeName;
        Infos.erase(It);
    }

    FMemoryProfiler::Get().RegisterUObjectFree(TypeName, Size);

	::operator delete(Pointer, Size);
}

REGISTER_CLASS(, UObject)
