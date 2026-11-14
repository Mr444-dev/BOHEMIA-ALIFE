#pragma once

#include "BehaviorObjects.h"
#include <array>
#include <cstdint>

namespace bohemia
{
struct Blackboard
{
	std::array<std::int32_t, static_cast<std::size_t>(BlackboardKey::Count)> values{};
	std::int32_t& operator[](BlackboardKey key) noexcept { return values[static_cast<std::size_t>(key)]; }
	std::int32_t operator[](BlackboardKey key) const noexcept { return values[static_cast<std::size_t>(key)]; }
};

struct SubbrainState
{
	Blackboard blackboard{};
	LeaseToken smartObjectLease{};
	SubtreeInjection injection{};
	std::uint32_t canonicalItemState = 0;
	std::uint32_t currentItemState = 0;
	std::uint8_t layer = 0;
	bool running = false;
	bool itemTemporarilyChanged = false;
};

class PreemptionCleanupTree
{
public:
	PreemptionCleanupTree(BehaviorObjectRegistry& registry, SubbrainState& interrupted) noexcept : m_registry(registry), m_interrupted(interrupted) {}
	PreemptionCleanupTree(const PreemptionCleanupTree&) = delete;
	PreemptionCleanupTree& operator=(const PreemptionCleanupTree&) = delete;
	~PreemptionCleanupTree();
private:
	BehaviorObjectRegistry& m_registry;
	SubbrainState& m_interrupted;
};

struct NeedScores { std::array<float, static_cast<std::size_t>(Intention::Count)> value{}; };
struct IntentionSelection { Intention intention = Intention::Work; float score = 0.0f; };
class BdiIntentionSelector
{
public:
	[[nodiscard]] IntentionSelection Select(const NeedScores& needs, std::uint32_t tick) noexcept;
	[[nodiscard]] static constexpr float CommitmentBonus() noexcept { return 0.18f; }
	[[nodiscard]] static constexpr std::uint32_t MinimumLockTicks() noexcept { return 20; }
private:
	IntentionSelection m_current{};
	std::uint32_t m_lockedUntilTick = 0;
	bool m_hasCurrent = false;
};

struct SwitchingConditions { bool socialDisturbance = false; bool investigateCrime = false; bool combatThreat = false; };
struct SubbrainDecision { SubbrainLayer layer = SubbrainLayer::Ambient; };
class SwitchingBtEvaluator
{
public:
	[[nodiscard]] SubbrainDecision Evaluate(const SwitchingConditions& conditions) const noexcept;
};

class SubbrainController
{
public:
	explicit SubbrainController(BehaviorObjectRegistry& registry) noexcept : m_registry(registry) {}
	void Start(SubbrainLayer layer) noexcept;
	void BindActiveActivity(const LeaseToken& lease, const SubtreeInjection& injection, std::uint32_t canonicalItemState = 0) noexcept;
	void SetTemporaryItemState(std::uint32_t currentItemState) noexcept;
	bool Preempt(SubbrainLayer layer, const LeaseToken& lease, const SubtreeInjection& injection, std::uint32_t temporaryItemState = 0) noexcept;
	[[nodiscard]] SubbrainLayer ActiveLayer() const noexcept { return m_active; }
	[[nodiscard]] const SubbrainState& State(SubbrainLayer layer) const noexcept { return m_states[static_cast<std::size_t>(layer)]; }
	[[nodiscard]] std::uint32_t CleanupCount() const noexcept { return m_cleanupCount; }
private:
	BehaviorObjectRegistry& m_registry;
	std::array<SubbrainState, 4> m_states{};
	SubbrainLayer m_active = SubbrainLayer::Ambient;
	std::uint32_t m_cleanupCount = 0;
};
}