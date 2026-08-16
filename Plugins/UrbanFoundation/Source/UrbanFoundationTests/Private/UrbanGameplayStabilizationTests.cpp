#include "AI/UrbanSimpleBotComponent.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "UI/WeaponSelection/LyraWeaponSelectionScreen.h"
#include "Weapons/UrbanSmokeCloud.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUrbanBotWeaponProfilesTest,
	"UrbanSpear.GameplayStabilization.Bot.WeaponProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanBotWeaponProfilesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UUrbanSimpleBotComponent* Bot = NewObject<UUrbanSimpleBotComponent>();
	TestNotNull(TEXT("bot component can be constructed"), Bot);
	if (!Bot)
	{
		return false;
	}

	Bot->SetWeaponType(EBotWeaponType::Pistol);
	const FUrbanBotWeaponProfile Pistol = Bot->GetWeaponProfile();
	TestEqual(TEXT("pistol damage"), Pistol.Damage, 20.0f);
	TestEqual(TEXT("pistol interval"), Pistol.FireInterval, 0.55f);
	TestEqual(TEXT("pistol range"), Pistol.Range, 3000.0f);
	TestEqual(TEXT("pistol pellet count"), Pistol.Pellets, 1);
	TestEqual(TEXT("pistol magazine"), Pistol.MagazineSize, 12);

	Bot->SetWeaponType(EBotWeaponType::Rifle);
	const FUrbanBotWeaponProfile Rifle = Bot->GetWeaponProfile();
	TestEqual(TEXT("rifle damage"), Rifle.Damage, 10.0f);
	TestEqual(TEXT("rifle interval"), Rifle.FireInterval, 0.30f);
	TestEqual(TEXT("rifle range"), Rifle.Range, 3500.0f);
	TestEqual(TEXT("rifle pellet count"), Rifle.Pellets, 1);
	TestEqual(TEXT("rifle magazine"), Rifle.MagazineSize, 30);

	Bot->SetWeaponType(EBotWeaponType::Shotgun);
	const FUrbanBotWeaponProfile Shotgun = Bot->GetWeaponProfile();
	TestEqual(TEXT("shotgun damage per pellet"), Shotgun.Damage, 8.0f);
	TestEqual(TEXT("shotgun interval"), Shotgun.FireInterval, 1.0f);
	TestEqual(TEXT("shotgun range"), Shotgun.Range, 1500.0f);
	TestEqual(TEXT("shotgun pellet count"), Shotgun.Pellets, 3);
	TestEqual(TEXT("shotgun magazine"), Shotgun.MagazineSize, 6);

	TestTrue(TEXT("all weapon intervals are positive"),
		Pistol.FireInterval > 0.0f && Rifle.FireInterval > 0.0f && Shotgun.FireInterval > 0.0f);
	TestTrue(TEXT("all weapon ranges are positive"),
		Pistol.Range > 0.0f && Rifle.Range > 0.0f && Shotgun.Range > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUrbanSmokeLifecycleTest,
	"UrbanSpear.GameplayStabilization.Smoke.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanSmokeLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const AUrbanSmokeCloud* Cloud = GetDefault<AUrbanSmokeCloud>();
	TestNotNull(TEXT("smoke cloud has a class default object"), Cloud);
	if (!Cloud)
	{
		return false;
	}

	TestTrue(TEXT("cloud radius is positive"), Cloud->GetCloudRadius() > 0.0f);
	TestTrue(TEXT("cloud lifetime is bounded to fifteen seconds"),
		Cloud->GetLifetime() > 0.0f && Cloud->GetLifetime() <= 15.0f);
	TestTrue(TEXT("grow time fits within lifetime"), Cloud->GetGrowTime() <= Cloud->GetLifetime());
	TestTrue(TEXT("fade time fits within lifetime"), Cloud->GetFadeTime() <= Cloud->GetLifetime());

	TestEqual(TEXT("cloud starts transparent"),
		AUrbanSmokeCloud::ResolveLifecycleAlpha(0.0f, 1.5f, 8.0f, 2.0f), 0.0f);
	TestEqual(TEXT("cloud reaches half density during growth"),
		AUrbanSmokeCloud::ResolveLifecycleAlpha(0.75f, 1.5f, 8.0f, 2.0f), 0.5f);
	TestEqual(TEXT("cloud holds at full density"),
		AUrbanSmokeCloud::ResolveLifecycleAlpha(4.0f, 1.5f, 8.0f, 2.0f), 1.0f);
	TestEqual(TEXT("cloud reaches half density during fade"),
		AUrbanSmokeCloud::ResolveLifecycleAlpha(7.0f, 1.5f, 8.0f, 2.0f), 0.5f);
	TestEqual(TEXT("cloud ends transparent"),
		AUrbanSmokeCloud::ResolveLifecycleAlpha(8.0f, 1.5f, 8.0f, 2.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUrbanWeaponSelectionContractTest,
	"UrbanSpear.GameplayStabilization.UI.WeaponSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanWeaponSelectionContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const ULyraWeaponSelectionScreen* Screen = GetDefault<ULyraWeaponSelectionScreen>();
	TestNotNull(TEXT("weapon selection has a class default object"), Screen);
	if (!Screen)
	{
		return false;
	}

	TestTrue(TEXT("pistol definition is configured"), !Screen->GetPistolItemDefinition().IsNull());
	TestTrue(TEXT("rifle definition is configured"), !Screen->GetRifleItemDefinition().IsNull());
	TestTrue(TEXT("shotgun definition is configured"), !Screen->GetShotgunItemDefinition().IsNull());
	TestNotEqual(TEXT("pistol and rifle definitions differ"),
		Screen->GetPistolItemDefinition().ToSoftObjectPath(), Screen->GetRifleItemDefinition().ToSoftObjectPath());
	TestNotEqual(TEXT("rifle and shotgun definitions differ"),
		Screen->GetRifleItemDefinition().ToSoftObjectPath(), Screen->GetShotgunItemDefinition().ToSoftObjectPath());

	TestEqual(TEXT("1 selects pistol"), ULyraWeaponSelectionScreen::ResolveSelectionIndex(EKeys::One), 0);
	TestEqual(TEXT("numpad 2 selects rifle"), ULyraWeaponSelectionScreen::ResolveSelectionIndex(EKeys::NumPadTwo), 1);
	TestEqual(TEXT("3 selects shotgun"), ULyraWeaponSelectionScreen::ResolveSelectionIndex(EKeys::Three), 2);
	TestEqual(TEXT("enter accepts pistol default"), ULyraWeaponSelectionScreen::ResolveSelectionIndex(EKeys::Enter), 0);
	TestEqual(TEXT("unsupported key is rejected"), ULyraWeaponSelectionScreen::ResolveSelectionIndex(EKeys::Escape), INDEX_NONE);
	return true;
}

#endif
