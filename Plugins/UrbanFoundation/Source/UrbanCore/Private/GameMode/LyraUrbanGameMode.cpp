#include "GameMode/LyraUrbanGameMode.h"

#include "GameMode/UrbanBotComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraUrbanGameMode)

ALyraUrbanGameMode::ALyraUrbanGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UrbanBotComponent = CreateDefaultSubobject<UUrbanBotComponent>(TEXT("UrbanBotComponent"));
}
