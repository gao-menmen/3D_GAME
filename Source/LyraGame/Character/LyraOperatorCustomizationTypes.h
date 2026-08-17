// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraOperatorCustomizationTypes.generated.h"

/** Licensed Lyra body meshes available to players. */
UENUM(BlueprintType)
enum class ELyraOperatorBodyType : uint8
{
	Manny = 0 UMETA(DisplayName="Manny"),
	Quinn = 1 UMETA(DisplayName="Quinn")
};

/** Material-driven tactical uniform styles; team colors remain untouched. */
UENUM(BlueprintType)
enum class ELyraOperatorUniformPreset : uint8
{
	Urban = 0 UMETA(DisplayName="Urban"),
	Stealth = 1 UMETA(DisplayName="Stealth"),
	Assault = 2 UMETA(DisplayName="Assault")
};
