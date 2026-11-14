#pragma once

#include "Types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace bohemia
{
struct SubtreeInjection
{
	SmartObjectHandle source{};
	std::uint16_t subtreeId = kNoSubtree;
	std::uint8_t frameSlot = 0;
	std::uint8_t active = 0;
	std::array<std::int32_t, 4> payload{};
};

struct SmartObject
{
	SmartObjectHandle handle{};
	std::uint16_t subtreeId = kNoSubtree;
	[[nodiscard]] SubtreeInjection Inject(std::uint8_t slot = 0) const noexcept { return {handle, subtreeId, slot, 1, {}}; }
};

struct SmartObjectState
{
	BehaviorObjectKind kind = BehaviorObjectKind::SmartObject;
	std::uint16_t subtreeId = kNoSubtree;
	std::uint8_t capacity = 1;
	std::uint8_t occupantCount = 0;
	std::array<NpcId, 8> occupants{};
	std::uint32_t generation = 1;
	bool enabled = false;
};

class BehaviorObjectRegistry
{
public:
	[[nodiscard]] SmartObjectHandle Register(BehaviorObjectKind kind, std::uint16_t subtreeId, std::uint8_t capacity = 1) noexcept;
	[[nodiscard]] LeaseToken TryAcquire(SmartObjectHandle handle, NpcId npc) noexcept;
	void Release(const LeaseToken& token) noexcept;
	[[nodiscard]] bool IsLeasedBy(const LeaseToken& token) const noexcept;
	[[nodiscard]] bool Resolve(SmartObjectHandle handle, SmartObjectState& out) const noexcept;
	[[nodiscard]] std::size_t Size() const noexcept { return m_size; }
private:
	std::array<SmartObjectState, kMaxSmartObjects> m_objects{};
	std::size_t m_size = 0;
};

class SmartObjectLease
{
public:
	SmartObjectLease(BehaviorObjectRegistry& registry, LeaseToken token) noexcept : m_registry(registry), m_token(token) {}
	SmartObjectLease(const SmartObjectLease&) = delete;
	SmartObjectLease& operator=(const SmartObjectLease&) = delete;
	SmartObjectLease(SmartObjectLease&& other) noexcept;
	SmartObjectLease& operator=(SmartObjectLease&& other) noexcept;
	~SmartObjectLease();
	void Reset() noexcept;
	[[nodiscard]] bool IsValid() const noexcept { return m_token.IsValid(); }
private:
	BehaviorObjectRegistry& m_registry;
	LeaseToken m_token{};
};

enum class DoorState : std::uint8_t { Open, Locked, Unlocked };
struct NavigationTraversal { bool allowed = false; SubtreeInjection injection{}; };
class NavigationSmartObject
{
public:
	NavigationSmartObject(SmartObjectHandle handle, DoorState state, std::uint16_t unlockSubtree, std::uint16_t lockpickSubtree) noexcept;
	[[nodiscard]] NavigationTraversal Traverse(bool hasKey, bool allowLockpick) const noexcept;
	void SetState(DoorState state) noexcept { m_state = state; }
private:
	SmartObjectHandle m_handle{};
	DoorState m_state = DoorState::Open;
	std::uint16_t m_unlockSubtree = kNoSubtree;
	std::uint16_t m_lockpickSubtree = kNoSubtree;
};

struct RoleIntent { NpcId npc = kUnknownActor; Intention intention = Intention::HaveFun; };
struct RoleAssignment { NpcId npc = kUnknownActor; ActivityRole role = ActivityRole::None; };
struct SmartArea
{
	std::uint16_t areaId = 0;
	ActivityRole primaryRole = ActivityRole::None;
	ActivityRole soloFallback = ActivityRole::None;
	std::uint8_t minimumCardinality = 1;
};
struct ForgeSmartArea { static constexpr SmartArea Configuration{1, ActivityRole::WorkAtAnvil, ActivityRole::Grindstone, 1}; };
struct TavernSmartArea { static constexpr SmartArea Configuration{2, ActivityRole::DicePlayer, ActivityRole::DrinkAleAtBar, 2}; };
struct MarketSmartArea { static constexpr SmartArea Configuration{3, ActivityRole::MarketBrowse, ActivityRole::None, 1}; };
struct GuardhouseSmartArea { static constexpr SmartArea Configuration{4, ActivityRole::GuardPost, ActivityRole::None, 1}; };

class AreaBrain
{
public:
	[[nodiscard]] std::size_t Solve(const SmartArea& area, std::span<const RoleIntent> participants, std::span<RoleAssignment> output, bool fallbackAvailable = true) const noexcept;
};
struct QuestSmartObject
{
	SmartObjectHandle handle{};
	std::uint16_t temporarySubtree = kNoSubtree;
	std::uint32_t expiresAtTick = 0;
	[[nodiscard]] SubtreeInjection Inject(std::uint32_t tick) const noexcept;
};

enum class SituationRole : std::uint8_t { Accuser, ArrestingGuard, Suspect, BystanderWitness };
struct SituationAssignment { NpcId npc = kUnknownActor; SituationRole role = SituationRole::BystanderWitness; };
class Situation
{
public:
	[[nodiscard]] std::span<const SituationAssignment> Assignments() const noexcept { return {m_assignments.data(), m_count}; }
protected:
	std::array<SituationAssignment, kMaxAreaParticipants> m_assignments{};
	std::uint8_t m_count = 0;
};
class CrimeArrestSituation final : public Situation
{
public:
	void Configure(NpcId accuser, NpcId guard, NpcId suspect, std::span<const NpcId> witnesses) noexcept;
};
}