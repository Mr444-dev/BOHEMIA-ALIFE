#include "SubbrainArchitecture.h"
#include <algorithm>

namespace bohemia
{
PreemptionCleanupTree::~PreemptionCleanupTree()
{
	if (m_interrupted.smartObjectLease.IsValid()) m_registry.Release(m_interrupted.smartObjectLease);
	m_interrupted.smartObjectLease = {};
	m_interrupted.injection = {};
	if (m_interrupted.itemTemporarilyChanged) m_interrupted.currentItemState = m_interrupted.canonicalItemState;
	m_interrupted.itemTemporarilyChanged = false;
	m_interrupted.running = false;
}

IntentionSelection BdiIntentionSelector::Select(const NeedScores& needs, std::uint32_t tick) noexcept
{
	IntentionSelection best{Intention::Work, needs.value[0]};
	for (std::size_t i = 1; i < needs.value.size(); ++i)
	{
		if (needs.value[i] > best.score) best = {static_cast<Intention>(i), needs.value[i]};
	}
	if (!m_hasCurrent)
	{
		m_current = best;
		m_lockedUntilTick = tick + MinimumLockTicks();
		m_hasCurrent = true;
		return m_current;
	}
	const float currentBase = needs.value[static_cast<std::size_t>(m_current.intention)];
	const float currentScore = currentBase + CommitmentBonus();
	if (tick < m_lockedUntilTick || best.score <= currentScore)
	{
		m_current.score = currentBase;
		return m_current;
	}
	m_current = best;
	m_lockedUntilTick = tick + MinimumLockTicks();
	return m_current;
}

SubbrainDecision SwitchingBtEvaluator::Evaluate(const SwitchingConditions& conditions) const noexcept
{
	if (conditions.combatThreat) return {SubbrainLayer::CombatArrest};
	if (conditions.investigateCrime) return {SubbrainLayer::Investigation};
	if (conditions.socialDisturbance) return {SubbrainLayer::SocialWeather};
	return {SubbrainLayer::Ambient};
}

void SubbrainController::Start(SubbrainLayer layer) noexcept
{
	m_active = layer;
	SubbrainState& state = m_states[static_cast<std::size_t>(layer)];
	state.layer = static_cast<std::uint8_t>(layer);
	state.running = true;
}
void SubbrainController::BindActiveActivity(const LeaseToken& lease, const SubtreeInjection& injection, std::uint32_t canonicalItemState) noexcept
{
	SubbrainState& state = m_states[static_cast<std::size_t>(m_active)];
	state.smartObjectLease = lease;
	state.injection = injection;
	state.canonicalItemState = canonicalItemState;
	state.currentItemState = canonicalItemState;
	state.itemTemporarilyChanged = false;
}

void SubbrainController::SetTemporaryItemState(std::uint32_t currentItemState) noexcept
{
	SubbrainState& state = m_states[static_cast<std::size_t>(m_active)];
	state.currentItemState = currentItemState;
	state.itemTemporarilyChanged = currentItemState != state.canonicalItemState;
}
bool SubbrainController::Preempt(SubbrainLayer layer, const LeaseToken& lease, const SubtreeInjection& injection, std::uint32_t temporaryItemState) noexcept
{
	if (static_cast<std::uint8_t>(layer) <= static_cast<std::uint8_t>(m_active)) return false;
	SubbrainState& interrupted = m_states[static_cast<std::size_t>(m_active)];
	{
		PreemptionCleanupTree cleanup(m_registry, interrupted);
		++m_cleanupCount;
	}
	m_active = layer;
	SubbrainState& next = m_states[static_cast<std::size_t>(layer)];
	next.layer = static_cast<std::uint8_t>(layer);
	next.smartObjectLease = lease;
	next.injection = injection;
	next.canonicalItemState = 0;
	next.currentItemState = temporaryItemState;
	next.itemTemporarilyChanged = temporaryItemState != 0;
	next.running = true;
	return true;
}
}