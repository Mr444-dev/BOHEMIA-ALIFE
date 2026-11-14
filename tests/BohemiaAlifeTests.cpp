#include "core/ArmorBallistics.h"
#include "core/BehaviorObjects.h"
#include "core/SimLodAndCrime.h"
#include "core/SubbrainArchitecture.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string_view>

namespace
{
std::atomic<bool> g_countAllocations{false};
std::atomic<std::uint64_t> g_allocationCount{0};
using TestFn = bool(*)();
struct TestCase { std::string_view name; TestFn run; };
bool Check(bool condition, std::string_view message)
{
	if (!condition) std::cerr << "  assertion failed: " << message << '\n';
	return condition;
}

bool SubbrainPreemption()
{
	bohemia::BehaviorObjectRegistry objects;
	const auto station = objects.Register(bohemia::BehaviorObjectKind::SmartObject, 101);
	const auto lease = objects.TryAcquire(station, 42);
	if (!Check(lease.IsValid() && objects.IsLeasedBy(lease), "ambient subbrain acquired the station")) return false;
	bohemia::SubbrainController brain(objects);
	brain.Start(bohemia::SubbrainLayer::Ambient);
	brain.BindActiveActivity(lease, {station, 101, 0, 1, {}}, 77);
	brain.SetTemporaryItemState(123);
	if (!Check(brain.Preempt(bohemia::SubbrainLayer::CombatArrest, {}, {}), "combat subbrain preempts ambient")) return false;
	const auto& oldState = brain.State(bohemia::SubbrainLayer::Ambient);
	return Check(!objects.IsLeasedBy(lease), "preemption releases the station lease") &&
		Check(!oldState.injection.active && !oldState.smartObjectLease.IsValid(), "cleanup unbinds injection and clears handle") &&
		Check(oldState.canonicalItemState == 77 && oldState.currentItemState == 77 && !oldState.itemTemporarilyChanged, "cleanup restores canonical item state") &&
		Check(brain.CleanupCount() == 1, "cleanup tree ran exactly once");
}

bool TavernFallback()
{
	const bohemia::SmartArea tavern = bohemia::TavernSmartArea::Configuration;
	const std::array<bohemia::RoleIntent, 1> one{{{17, bohemia::Intention::HaveFun}}};
	std::array<bohemia::RoleAssignment, 2> assigned{};
	bohemia::AreaBrain solver;
	const auto count = solver.Solve(tavern, one, assigned);
	if (!Check(count == 1 && assigned[0].npc == 17, "one entrant receives an assignment")) return false;
	if (!Check(assigned[0].role == bohemia::ActivityRole::DrinkAleAtBar, "Pepa falls back to a valid solo tavern activity")) return false;
	const std::array<bohemia::RoleIntent, 2> pair{{{17, bohemia::Intention::HaveFun}, {18, bohemia::Intention::HaveFun}}};
	const auto pairCount = solver.Solve(tavern, pair, assigned); (void)pairCount;
	return Check(assigned[0].role == bohemia::ActivityRole::DicePlayer && assigned[1].role == bohemia::ActivityRole::DicePlayer, "dice role is enabled only at cardinality two");
}

bool NavigationInjection()
{
	bohemia::BehaviorObjectRegistry objects;
	const auto door = objects.Register(bohemia::BehaviorObjectKind::NavigationSmartObject, 0);
	const bohemia::NavigationSmartObject nav(door, bohemia::DoorState::Locked, 12, 13);
	const auto traversal = nav.Traverse(false, true);
	return Check(traversal.allowed, "lockpicking permits path traversal") &&
		Check(traversal.injection.active != 0 && traversal.injection.subtreeId == 13, "locked edge injects the lockpick subtree") &&
		Check(traversal.injection.source == door, "injection retains a generation-checked object handle");
}

bool SituationCoordination()
{
	bohemia::CrimeArrestSituation situation;
	const std::array<bohemia::NpcId, 2> witnesses{19, 34};
	situation.Configure(14, 7, 412, witnesses);
	const auto roles = situation.Assignments();
	return Check(roles.size() == 5, "accuser, guard, suspect, and both witnesses are assigned") &&
		Check(roles[0].npc == 14 && roles[0].role == bohemia::SituationRole::Accuser, "blacksmith is accuser") &&
		Check(roles[1].npc == 7 && roles[1].role == bohemia::SituationRole::ArrestingGuard, "guard receives arrest role") &&
		Check(roles[2].npc == 412 && roles[2].role == bohemia::SituationRole::Suspect, "thief receives suspect role");
}

bool LodHydrationAndSanitization()
{
	bohemia::SimLodWorld world;
	world.InitializeDemo(1000);
	const auto counts = world.CountLods();
	if (!Check(counts.lod0 == 56 && counts.lod1 == 148 && counts.lod2 == 796, "demo population has requested LOD distribution")) return false;
	world.GiveSoulItem(0, 9001);
	bohemia::BodySlot* body = world.BodyForNpc(0);
	if (!Check(body != nullptr, "near NPC owns a hydrated body")) return false;
	body->attachedToolProps = {100, 101, 0, 0};
	body->animationState = 5;
	body->combatFlags = 0xF;
	const bohemia::Vec3 original = world.Position(0);
	world.SetNpcDistance(0, 33.0f);
	if (!Check(world.Lod(0) == bohemia::SimLod::Kinematic, "dehydrates only beyond 32m")) return false;
	if (!Check(world.Bodies().SanitizedReleaseCount() == 1, "body slot passes through strict sanitation")) return false;
	world.SetNpcDistance(0, 27.0f);
	body = world.BodyForNpc(0);
	return Check(world.Lod(0) == bohemia::SimLod::Hydrated3D && body != nullptr, "rehydrates below 28m") &&
		Check(std::abs(world.Position(0).x - original.x) < 0.0001f && std::abs(world.Position(0).y - original.y) < 0.0001f, "hydration preserves exact world position") &&
		Check(world.PositionDriftCount() == 0, "no measured hydration position drift") &&
		Check(world.SoulOwns(0, 9001), "canonical stolen-item ownership remains in SoulState") &&
		Check(body->attachedToolProps[0] == 0 && body->animationState == 0 && body->combatFlags == 0, "recycled visual body is clean");
}

bool CommitmentHysteresis()
{
	bohemia::BdiIntentionSelector selector;
	bohemia::NeedScores scores{};
	scores.value[static_cast<std::size_t>(bohemia::Intention::Work)] = 0.50f;
	scores.value[static_cast<std::size_t>(bohemia::Intention::HaveFun)] = 0.50f;
	const auto first = selector.Select(scores, 0);
	scores.value[static_cast<std::size_t>(bohemia::Intention::HaveFun)] = 0.55f;
	const auto second = selector.Select(scores, 25);
	if (!Check(first.intention == bohemia::Intention::Work && second.intention == bohemia::Intention::Work, "commitment bonus prevents equal-need ping-pong")) return false;
	scores.value[static_cast<std::size_t>(bohemia::Intention::HaveFun)] = 0.75f;
	const auto third = selector.Select(scores, 26);
	return Check(third.intention == bohemia::Intention::HaveFun, "a sufficiently stronger need can replace the intention");
}

bool TwoStageCrimeDeduction()
{
	bohemia::EpisodicMemoryRingBuffer blocked;
	const bohemia::Vec3 forge{10, 20, 0};
	bohemia::CrimeDeductionEngine::RecordAuditoryLockpick(blocked, forge, 12.0f, true);
	bohemia::CrimeDeductionEngine::RecordVisualExit(blocked, 412, forge, 16.0f, false, 7001);
	if (!Check(blocked.Count() == 1 && blocked.Events()[0].subject == bohemia::kUnknownActor, "stone wall blocks visual identification during the lockpick")) return false;
	const bohemia::ItemProvenance provenance{7001, 14, 0.92f};
	const auto noVisual = bohemia::CrimeDeductionEngine::Deduce(blocked, provenance, 14, 12.0f);
	if (!Check(noVisual.suspect == bohemia::kUnknownActor, "audio alone does not identify a culprit")) return false;
	bohemia::EpisodicMemoryRingBuffer correlated;
	bohemia::CrimeDeductionEngine::RecordAuditoryLockpick(correlated, forge, 12.0f, true);
	bohemia::CrimeDeductionEngine::RecordVisualExit(correlated, 412, forge, 16.0f, true, 7001);
	const auto result = bohemia::CrimeDeductionEngine::Deduce(correlated, provenance, 14, 12.0f);
	return Check(result.suspect == 412 && result.correlatedAudioAndExit, "visible exit is correlated with the prior unknown audio event") &&
		Check(result.provenanceMatched && result.posterior >= 0.88f && result.posterior <= 0.90f, "stolen-item provenance yields the 89% demo posterior");
}

bool ScheduleJitterAndTimeSlicing()
{
	bohemia::SimLodWorld world;
	world.InitializeDemo(1000);
	for (std::size_t i = 0; i < world.Size(); ++i)
	{
		const int expected = static_cast<int>(bohemia::WangHash(world.Id(i)) % 900u) - 450;
		if (!Check(world.ScheduleJitter(i) == expected && expected >= -450 && expected <= 449, "deterministic schedule phase stays within +/- 7.5 minutes")) return false;
	}
	std::array<std::uint32_t, 32> perTick{};
	for (std::uint32_t tick = 0; tick < perTick.size(); ++tick)
	{
		for (std::size_t i = 0; i < world.Size(); ++i) if (world.ScheduledThisTick(i, tick)) ++perTick[tick];
	}
	auto sorted = perTick;
	std::sort(sorted.begin(), sorted.end());
	const float p50 = static_cast<float>(sorted[sorted.size() / 2]);
	const float p95 = static_cast<float>(sorted[static_cast<std::size_t>(sorted.size() * 0.95f)]);
	return Check(p50 > 0.0f && p95 / p50 < 1.5f, "time-sliced work keeps sampled P95/P50 below 1.5");
}

bool ArmorGlancingAndDurability()
{
	bohemia::ArmorState armor{};
	bohemia::StrikeInput strike{};
	constexpr float angle = 58.0f * 3.14159265358979323846f / 180.0f;
	strike.direction = {std::sin(angle), 0.0f, std::cos(angle)};
	const auto impact = bohemia::ArmorBallistics::ResolveStrike(armor, strike);
	const float accounted = impact.glancingDeflectedJoules + impact.plateAbsorbedJoules + impact.mailleAbsorbedJoules + impact.gambesonAbsorbedJoules + impact.staminaAbsorbedJoules + impact.tissueEnergyJoules;
	return Check(impact.glanced && impact.platePerforated == false, "oblique blade glances while the plate stops a 115J cut") &&
		Check(std::abs(impact.postGlanceEnergyJoules - 78.0f) < 1.0f, "demo strike retains about 78J after glancing") &&
		Check(std::abs(impact.gambesonAbsorbedJoules - 26.0f) < 2.0f && std::abs(impact.staminaSpent - 11.0f) < 1.0f, "pad absorbs about 26J and stamina shield spends about 11") &&
		Check(impact.healthDamage == 0.0f && armor.stamina < 100.0f, "stamina depletes before tissue damage") &&
		Check(armor.condition[0] < 1.0f && armor.condition[1] < 1.0f && armor.condition[2] < 1.0f, "all three protective layers degrade") &&
		Check(std::abs(accounted - strike.energyJoules) < 0.01f, "energy ledger conserves incident strike energy");
}

bool ZeroHeapAllocationTick()
{
	bohemia::SimLodWorld world;
	world.InitializeDemo(1000);
	std::array<bohemia::BdiIntentionSelector, 1000> selectors{};
	bohemia::SwitchingBtEvaluator switching;
	bohemia::AreaBrain areaBrain;
	const std::array<bohemia::RoleIntent, 2> tavernEntrants{{{17, bohemia::Intention::HaveFun}, {18, bohemia::Intention::HaveFun}}};
	std::array<bohemia::RoleAssignment, 2> tavernRoles{};
	bohemia::EpisodicMemoryRingBuffer memory;
	bohemia::CrimeDeductionEngine::RecordAuditoryLockpick(memory, {0,0,0}, 12.0f, true);
	bohemia::CrimeDeductionEngine::RecordVisualExit(memory, 412, {1,0,0}, 16.0f, true, 7001);
	const bohemia::ItemProvenance provenance{7001,14,0.92f};
	const std::array<bohemia::NpcId,2> witnesses{19,34};
	bohemia::CrimeArrestSituation situation;
	bohemia::NeedScores needs{}; needs.value[0]=0.55f; needs.value[1]=0.43f; needs.value[2]=0.40f; needs.value[3]=0.32f;
	auto runTick = [&](std::uint32_t tick) noexcept
	{
		world.Tick(1.0f/60.0f, tick);
		for(std::size_t i=0;i<world.Size();++i)
		{
			if(!world.ScheduledThisTick(i,tick)) continue;
			(void)selectors[i].Select(needs,tick);
			const bohemia::SwitchingConditions conditions{(world.Id(i)%29u)==0u,(world.Id(i)%101u)==0u,(world.Id(i)%997u)==0u};
			(void)switching.Evaluate(conditions);
		}
		(void)areaBrain.Solve(bohemia::TavernSmartArea::Configuration,tavernEntrants,tavernRoles);
		const auto evidence=bohemia::CrimeDeductionEngine::Deduce(memory,provenance,14,12.0f);
		situation.Configure(14,7,evidence.suspect,witnesses);
	};
	for (std::uint32_t tick = 0; tick < 8; ++tick) runTick(tick);
	g_allocationCount.store(0, std::memory_order_relaxed);
	g_countAllocations.store(true, std::memory_order_relaxed);
	for (std::uint32_t tick = 8; tick < 108; ++tick) runTick(tick);
	g_countAllocations.store(false, std::memory_order_relaxed);
	return Check(g_allocationCount.load(std::memory_order_relaxed) == 0, "100 full steady-state 1,000-NPC ticks allocate zero bytes through operator new");
}

const std::array<TestCase, 10> kTests{{
	{"SubbrainPreemption_ExecutesCleanupTreeAndReleasesSmartObjectLease", &SubbrainPreemption},
	{"SmartAreaConstraintSatisfaction_ReassignsPepaWhenFrantaIsAbsentFromDiceTable", &TavernFallback},
	{"NavigationSmartObject_InjectsLockpickSubtreeOnLockedDoorEdge", &NavigationInjection},
	{"MultiAgentSituation_CoordinatesAccuserGuardAndBystanderRoles", &SituationCoordination},
	{"SimLodHydrationHysteresis_PreservesExactPositionAndSanitizesRecycledBodySlot", &LodHydrationAndSanitization},
	{"CommitmentHysteresis_PreventsUtilityPingPongOscillationBetweenEqualNeeds", &CommitmentHysteresis},
	{"TwoStageCrimeDeduction_WallBlocksVisualSight_CorrelatesAudioClickWithDoorExit", &TwoStageCrimeDeduction},
	{"SchedulePhaseJitterAndTimeSlicing_EliminatesThunderingHerdFrameSpikes", &ScheduleJitterAndTimeSlicing},
	{"WilliamsFourLayerArmor_ObliqueBladeGlances_AndBluntShockDegradesPaddingAndStamina", &ArmorGlancingAndDurability},
	{"ZeroHeapAllocation_SteadyState1000NpcTick_AllocatesZeroBytes", &ZeroHeapAllocationTick}
}};
}

void* operator new(std::size_t size)
{
	if (g_countAllocations.load(std::memory_order_relaxed)) g_allocationCount.fetch_add(1, std::memory_order_relaxed);
	if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
	throw std::bad_alloc{};
}
void* operator new[](std::size_t size)
{
	if (g_countAllocations.load(std::memory_order_relaxed)) g_allocationCount.fetch_add(1, std::memory_order_relaxed);
	if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
	throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main(int argc, char** argv)
{
	const std::string_view requested = argc > 1 ? argv[1] : std::string_view{};
	std::size_t passed = 0;
	for (const auto& test : kTests)
	{
		if (!requested.empty() && requested != test.name) continue;
		std::cout << "[ RUN      ] " << test.name << '\n';
		if (!test.run())
		{
			std::cout << "[  FAILED  ] " << test.name << '\n';
			return 1;
		}
		++passed;
		std::cout << "[       OK ] " << test.name << '\n';
	}
	if (passed == 0)
	{
		std::cerr << "Unknown test: " << requested << '\n';
		return 2;
	}
	std::cout << "Passed " << passed << " deterministic suite(s).\n";
	return 0;
}