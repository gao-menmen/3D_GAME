#include "AI/UrbanSimpleBotComponent.h"
#include "Character/LyraTacticalOperatorAppearanceComponent.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Player/LyraTacticalEconomyComponent.h"
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
	TestEqual(TEXT("selection state uses a stable controller tag"),
		ULyraWeaponSelectionScreen::GetSelectionShownTag(), FName(TEXT("Urban.WeaponSelectionShown")));
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
	TestEqual(TEXT("4 selects body armor"), ULyraWeaponSelectionScreen::ResolveSelectionIndex(EKeys::Four), 3);
	TestEqual(TEXT("numpad 5 selects helmet"), ULyraWeaponSelectionScreen::ResolveSelectionIndex(EKeys::NumPadFive), 4);
	TestEqual(TEXT("enter accepts pistol default"), ULyraWeaponSelectionScreen::ResolveSelectionIndex(EKeys::Enter), 0);
	TestEqual(TEXT("unsupported key is rejected"), ULyraWeaponSelectionScreen::ResolveSelectionIndex(EKeys::Escape), INDEX_NONE);
	TestEqual(TEXT("sidearm is the free fallback"), ULyraWeaponSelectionScreen::ResolveWeaponPrice(0), 0);
	TestEqual(TEXT("rifle has a premium tactical price"), ULyraWeaponSelectionScreen::ResolveWeaponPrice(1), 2700);
	TestEqual(TEXT("shotgun has a close-range price"), ULyraWeaponSelectionScreen::ResolveWeaponPrice(2), 1800);
	TestEqual(TEXT("body armor has an equipment price"), ULyraWeaponSelectionScreen::ResolveWeaponPrice(3), 650);
	TestEqual(TEXT("helmet has an equipment price"), ULyraWeaponSelectionScreen::ResolveWeaponPrice(4), 350);
	TestEqual(TEXT("invalid selection has no price"), ULyraWeaponSelectionScreen::ResolveWeaponPrice(99), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUrbanRuntimeBotArchetypeIntegrationTest,
	"UrbanSpear.GameplayStabilization.Bot.ArchetypeIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanRuntimeBotArchetypeIntegrationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UUrbanSimpleBotComponent* Bot = NewObject<UUrbanSimpleBotComponent>();
	TestNotNull(TEXT("bot component can be constructed"), Bot);
	if (!Bot)
	{
		return false;
	}

	Bot->SetEnemyArchetype(EUrbanEnemyArchetype::Rifleman);
	TestEqual(TEXT("rifleman role is retained"), Bot->GetEnemyArchetype(), EUrbanEnemyArchetype::Rifleman);
	TestEqual(TEXT("rifleman receives rifle magazine"), Bot->GetWeaponProfile().MagazineSize, 30);
	TestTrue(TEXT("rifleman strongly prefers cover"), Bot->GetArchetypeTuning().CoverPreference >= 0.9f);

	Bot->SetEnemyArchetype(EUrbanEnemyArchetype::Assault);
	TestEqual(TEXT("assault receives shotgun magazine"), Bot->GetWeaponProfile().MagazineSize, 6);
	TestTrue(TEXT("assault is grenade capable"), Bot->GetArchetypeTuning().bCanUseGrenades);
	TestTrue(TEXT("assault strongly prefers flanking"), Bot->GetArchetypeTuning().FlankPreference >= 0.8f);

	Bot->SetEnemyArchetype(EUrbanEnemyArchetype::Marksman);
	TestEqual(TEXT("marksman receives rifle magazine"), Bot->GetWeaponProfile().MagazineSize, 30);
	TestTrue(TEXT("marksman has long preferred range"), Bot->GetArchetypeTuning().PreferredEngagementRangeMeters >= 70.0f);

	Bot->SetEnemyArchetype(EUrbanEnemyArchetype::Drone);
	TestTrue(TEXT("drone role remains aerial"), Bot->GetArchetypeTuning().bIsAerial);
	TestEqual(TEXT("new runtime bot begins in patrol state"), Bot->GetBehaviorState(), EUrbanAIBehaviorState::PatrolOrGuard);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUrbanTacticalEconomyTest,
	"UrbanSpear.GameplayStabilization.Economy.PurchaseAndRoundAwards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanTacticalEconomyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	ULyraTacticalEconomyComponent* Economy = NewObject<ULyraTacticalEconomyComponent>();
	TestNotNull(TEXT("economy component can be constructed"), Economy);
	if (!Economy)
	{
		return false;
	}

	TestEqual(TEXT("player starts with tactical funds"), Economy->GetFunds(), 4000);
	TestTrue(TEXT("rifle purchase succeeds"), Economy->TryPurchase(2700));
	TestEqual(TEXT("purchase deducts exact price"), Economy->GetFunds(), 1300);
	TestFalse(TEXT("unaffordable purchase is rejected"), Economy->TryPurchase(1800));
	TestEqual(TEXT("failed purchase preserves funds"), Economy->GetFunds(), 1300);
	TestFalse(TEXT("negative price is rejected"), Economy->TryPurchase(-1));
	TestFalse(TEXT("helmet requires body armor"), Economy->TryPurchaseHelmet());
	TestTrue(TEXT("body armor purchase succeeds"), Economy->TryPurchaseArmor());
	TestEqual(TEXT("armor starts at full durability"), Economy->GetArmor(), 100.0f, 0.001f);
	TestFalse(TEXT("duplicate armor purchase is rejected"), Economy->TryPurchaseArmor());
	TestTrue(TEXT("helmet purchase succeeds after armor"), Economy->TryPurchaseHelmet());
	TestTrue(TEXT("helmet ownership is retained"), Economy->HasHelmet());
	TestFalse(TEXT("duplicate helmet purchase is rejected"), Economy->TryPurchaseHelmet());
	Economy->ApplyArmorDamage(35.0f);
	TestEqual(TEXT("combat damage consumes armor durability"), Economy->GetArmor(), 65.0f, 0.001f);
	Economy->ApplyArmorDamage(-10.0f);
	TestEqual(TEXT("negative armor damage is ignored"), Economy->GetArmor(), 65.0f, 0.001f);
	Economy->ApplyArmorDamage(500.0f);
	TestEqual(TEXT("armor durability cannot become negative"), Economy->GetArmor(), 0.0f, 0.001f);

	Economy->AddKillReward();
	TestEqual(TEXT("kill grants default reward"), Economy->GetFunds(), 600);
	Economy->AddRoundAward(false, 2);
	TestEqual(TEXT("loss streak increases recovery award"), Economy->GetFunds(), 3000);
	TestEqual(TEXT("round win award is stable"), ULyraTacticalEconomyComponent::ResolveRoundAward(true, 4), 3250);
	TestEqual(TEXT("loss award is capped"), ULyraTacticalEconomyComponent::ResolveRoundAward(false, 99), 3400);
	TestEqual(TEXT("wallet maximum is capped"), ULyraTacticalEconomyComponent::ClampFunds(50000), 16000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUrbanTacticalOperatorAppearanceTest,
	"UrbanSpear.GameplayStabilization.Character.EquipmentAppearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUrbanTacticalOperatorAppearanceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const uint8 None = ULyraTacticalOperatorAppearanceComponent::MakeAppearanceFlags(0.0f, false);
	const uint8 Armor = ULyraTacticalOperatorAppearanceComponent::MakeAppearanceFlags(100.0f, false);
	const uint8 HelmetOnly = ULyraTacticalOperatorAppearanceComponent::MakeAppearanceFlags(0.0f, true);
	const uint8 FullKit = ULyraTacticalOperatorAppearanceComponent::MakeAppearanceFlags(65.0f, true);

	TestEqual(TEXT("unarmored operator has no equipment flags"), None, uint8{0});
	TestEqual(TEXT("positive armor durability enables vest appearance"), Armor, uint8{1});
	TestEqual(TEXT("helmet state is represented independently"), HelmetOnly, uint8{2});
	TestEqual(TEXT("full equipment kit packs into two public bits"), FullKit, uint8{3});
	TestEqual(TEXT("negative armor does not produce a vest"),
		ULyraTacticalOperatorAppearanceComponent::MakeAppearanceFlags(-1.0f, false), uint8{0});
	return true;
}
#endif
