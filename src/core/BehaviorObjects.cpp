#include "BehaviorObjects.h"
#include <algorithm>
#include <utility>

namespace bohemia
{
SmartObjectHandle BehaviorObjectRegistry::Register(BehaviorObjectKind kind, std::uint16_t subtreeId, std::uint8_t capacity) noexcept
{
	if (m_size >= m_objects.size() || capacity == 0 || capacity > m_objects[0].occupants.size()) return {};
	const std::uint32_t index = static_cast<std::uint32_t>(m_size++);
	SmartObjectState& object = m_objects[index];
	object = {};
	object.kind = kind;
	object.subtreeId = subtreeId;
	object.capacity = capacity;
	object.generation = 1;
	object.enabled = true;
	return {index, object.generation};
}

LeaseToken BehaviorObjectRegistry::TryAcquire(SmartObjectHandle handle, NpcId npc) noexcept
{
	if (npc == kUnknownActor || !handle.IsValid() || handle.index >= m_size) return {};
	SmartObjectState& object = m_objects[handle.index];
	if (!object.enabled || object.generation != handle.generation || object.occupantCount >= object.capacity) return {};
	for (std::uint8_t i = 0; i < object.occupantCount; ++i) if (object.occupants[i] == npc) return {};
	object.occupants[object.occupantCount++] = npc;
	return {handle, npc, object.subtreeId};
}

void BehaviorObjectRegistry::Release(const LeaseToken& token) noexcept
{
	if (!token.IsValid() || token.object.index >= m_size) return;
	SmartObjectState& object = m_objects[token.object.index];
	if (object.generation != token.object.generation) return;
	for (std::uint8_t i = 0; i < object.occupantCount; ++i)
	{
		if (object.occupants[i] != token.owner) continue;
		object.occupants[i] = object.occupants[--object.occupantCount];
		object.occupants[object.occupantCount] = kUnknownActor;
		return;
	}
}

bool BehaviorObjectRegistry::IsLeasedBy(const LeaseToken& token) const noexcept
{
	if (!token.IsValid() || token.object.index >= m_size) return false;
	const SmartObjectState& object = m_objects[token.object.index];
	if (!object.enabled || object.generation != token.object.generation) return false;
	for (std::uint8_t i = 0; i < object.occupantCount; ++i) if (object.occupants[i] == token.owner) return true;
	return false;
}
bool BehaviorObjectRegistry::Resolve(SmartObjectHandle handle, SmartObjectState& out) const noexcept
{
	if (!handle.IsValid() || handle.index >= m_size) return false;
	const SmartObjectState& object = m_objects[handle.index];
	if (object.generation != handle.generation || !object.enabled) return false;
	out = object;
	return true;
}

SmartObjectLease::SmartObjectLease(SmartObjectLease&& other) noexcept : m_registry(other.m_registry), m_token(other.m_token)
{
	other.m_token = {};
}
SmartObjectLease& SmartObjectLease::operator=(SmartObjectLease&& other) noexcept
{
	if (this == &other) return *this;
	Reset();
	if (&m_registry != &other.m_registry) { other.Reset(); return *this; }
	m_token = other.m_token;
	other.m_token = {};
	return *this;
}
SmartObjectLease::~SmartObjectLease() { Reset(); }
void SmartObjectLease::Reset() noexcept
{
	if (m_token.IsValid()) m_registry.Release(m_token);
	m_token = {};
}

NavigationSmartObject::NavigationSmartObject(SmartObjectHandle handle, DoorState state, std::uint16_t unlockSubtree, std::uint16_t lockpickSubtree) noexcept
	: m_handle(handle), m_state(state), m_unlockSubtree(unlockSubtree), m_lockpickSubtree(lockpickSubtree) {}
NavigationTraversal NavigationSmartObject::Traverse(bool hasKey, bool allowLockpick) const noexcept
{
	NavigationTraversal result{};
	if (m_state == DoorState::Open || m_state == DoorState::Unlocked)
	{
		result.allowed = true;
		return result;
	}
	if (hasKey)
	{
		result.allowed = true;
		result.injection = {m_handle, m_unlockSubtree, 0, 1, {}};
	}
	else if (allowLockpick)
	{
		result.allowed = true;
		result.injection = {m_handle, m_lockpickSubtree, 0, 1, {}};
	}
	return result;
}

std::size_t AreaBrain::Solve(const SmartArea& area, std::span<const RoleIntent> participants, std::span<RoleAssignment> output, bool fallbackAvailable) const noexcept
{
	const std::size_t count = std::min({participants.size(), output.size(), kMaxAreaParticipants});
	if (count == 0) return 0;
	const bool minimumMet = count >= area.minimumCardinality;
	const ActivityRole selectedRole = minimumMet ? area.primaryRole : (fallbackAvailable ? area.soloFallback : ActivityRole::None);
	for (std::size_t i = 0; i < count; ++i) output[i] = {participants[i].npc, selectedRole};
	return count;
}

SubtreeInjection QuestSmartObject::Inject(std::uint32_t tick) const noexcept
{
	if (tick >= expiresAtTick || temporarySubtree == kNoSubtree) return {};
	return {handle, temporarySubtree, 0, 1, {static_cast<std::int32_t>(expiresAtTick), 0, 0, 0}};
}

void CrimeArrestSituation::Configure(NpcId accuser, NpcId guard, NpcId suspect, std::span<const NpcId> witnesses) noexcept
{
	m_count = 0;
	if (accuser != kUnknownActor && m_count < m_assignments.size()) m_assignments[m_count++] = {accuser, SituationRole::Accuser};
	if (guard != kUnknownActor && m_count < m_assignments.size()) m_assignments[m_count++] = {guard, SituationRole::ArrestingGuard};
	if (suspect != kUnknownActor && m_count < m_assignments.size()) m_assignments[m_count++] = {suspect, SituationRole::Suspect};
	for (NpcId witness : witnesses)
	{
		if (witness == kUnknownActor || m_count >= m_assignments.size()) continue;
		m_assignments[m_count++] = {witness, SituationRole::BystanderWitness};
	}
}
}