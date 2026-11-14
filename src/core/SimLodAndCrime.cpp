#include "SimLodAndCrime.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bohemia
{
namespace
{
constexpr float kDaySeconds = 86400.0f;
float WrapDay(float seconds) noexcept
{
	float wrapped = std::fmod(seconds, kDaySeconds);
	return wrapped < 0.0f ? wrapped + kDaySeconds : wrapped;
}
}

FixedBodyPool::FixedBodyPool() noexcept
{
	m_freeCount = kBodyPoolCapacity;
	for (std::size_t i = 0; i < kBodyPoolCapacity; ++i)
	{
		m_free[i] = static_cast<std::uint16_t>(kBodyPoolCapacity - 1u - i);
		m_generation[i] = 1;
	}
}

BodyHandle FixedBodyPool::Acquire(NpcId owner) noexcept
{
	if (m_freeCount == 0 || owner == kUnknownActor) return {};
	const std::uint16_t index = m_free[--m_freeCount];
	BodySlot& slot = m_slots[index];
	slot = {};
	slot.owner = owner;
	slot.inUse = true;
	return { index, m_generation[index] };
}

void FixedBodyPool::Release(BodyHandle handle) noexcept
{
	BodySlot* slot = Resolve(handle);
	if (slot == nullptr) return;
	// Canonical item ownership lives in SoulState. Never let pooled presentation state leak to another NPC.
	slot->attachedToolProps.fill(0);
	slot->animationState = 0;
	slot->combatFlags = 0;
	slot->owner = kUnknownActor;
	slot->inUse = false;
	const std::uint16_t index = handle.index;
	m_generation[index] = static_cast<std::uint16_t>(m_generation[index] + 1u);
	if (m_generation[index] == 0) m_generation[index] = 1;
	m_free[m_freeCount++] = index;
	++m_sanitizedReleaseCount;
}

const BodySlot* FixedBodyPool::Resolve(BodyHandle handle) const noexcept
{
	if (!handle.IsValid() || handle.index >= kBodyPoolCapacity || m_generation[handle.index] != handle.generation || !m_slots[handle.index].inUse) return nullptr;
	return &m_slots[handle.index];
}
BodySlot* FixedBodyPool::Resolve(BodyHandle handle) noexcept
{
	return const_cast<BodySlot*>(static_cast<const FixedBodyPool*>(this)->Resolve(handle));
}

void SimLodWorld::InitializeDemo(std::size_t count) noexcept
{
	m_count = std::min(count, kMaxNpcs);
	m_bodies = FixedBodyPool{};
	m_scheduleTransitions = 0;
	m_positionDriftCount = 0;
	for (std::size_t i = 0; i < m_count; ++i)
	{
		const NpcId id = static_cast<NpcId>(i + 1u);
		m_ids.values[i] = id;
		const std::uint32_t phase = WangHash(id) % 900u;
		m_scheduleJitter.values[i] = static_cast<std::int16_t>(static_cast<int>(phase) - 450);
		m_scheduleClock.values[i] = WrapDay(8.0f * 3600.0f + static_cast<float>(m_scheduleJitter.values[i]));
		m_arcProgress.values[i] = static_cast<float>(WangHash(id) % 10000u) / 10000.0f;
		m_speed.values[i] = 0.45f + static_cast<float>(WangHash(id ^ 0xA5A5u) % 120u) / 100.0f;
		m_updatePeriod.values[i] = static_cast<std::uint8_t>(1u + (WangHash(id ^ 0xBEEFu) % 4u));
		m_bodyHandles.values[i] = {};
		m_souls[i] = {};
		if (i < 56u) m_distance.values[i] = 3.0f + static_cast<float>(i % 25u);
		else if (i < 204u) m_distance.values[i] = 29.0f + static_cast<float>(i % 30u);
		else m_distance.values[i] = 61.0f + static_cast<float>(i % 90u);
		m_x.values[i] = m_distance.values[i];
		m_y.values[i] = (static_cast<float>(WangHash(id ^ 0x1234u) % 200u) - 100.0f) * 0.1f;
		m_z.values[i] = 0.0f;
		m_lod.values[i] = m_distance.values[i] < 28.0f ? SimLod::Hydrated3D : (m_distance.values[i] <= 60.0f ? SimLod::Kinematic : SimLod::GraphSoul);
		if (m_lod.values[i] == SimLod::Hydrated3D) m_bodyHandles.values[i] = m_bodies.Acquire(id);
	}
}

void SimLodWorld::SetNpcDistance(std::size_t index, float distanceMeters) noexcept
{
	if (index >= m_count) return;
	const Vec3 before = Position(index);
	m_distance.values[index] = std::max(0.0f, distanceMeters);
	UpdateNpcLod(index);
	const Vec3 after = Position(index);
	if (std::abs(before.x - after.x) > 0.0001f || std::abs(before.y - after.y) > 0.0001f || std::abs(before.z - after.z) > 0.0001f) ++m_positionDriftCount;
}

void SimLodWorld::UpdateNpcLod(std::size_t index) noexcept
{
	const float distance = m_distance.values[index];
	const SimLod old = m_lod.values[index];
	SimLod next = old;
	if (old == SimLod::Hydrated3D)
	{
		if (distance > 32.0f) next = distance > 60.0f ? SimLod::GraphSoul : SimLod::Kinematic;
	}
	else if (old == SimLod::Kinematic)
	{
		if (distance < 28.0f) next = SimLod::Hydrated3D;
		else if (distance > 60.0f) next = SimLod::GraphSoul;
	}
	else
	{
		if (distance < 28.0f) next = SimLod::Hydrated3D;
		else if (distance <= 60.0f) next = SimLod::Kinematic;
	}
	if (next == old) return;
	if (next == SimLod::Hydrated3D)
	{
		const BodyHandle body = m_bodies.Acquire(m_ids.values[index]);
		if (!body.IsValid()) return;
		m_bodyHandles.values[index] = body;
	}
	else if (old == SimLod::Hydrated3D)
	{
		m_bodies.Release(m_bodyHandles.values[index]);
		m_bodyHandles.values[index] = {};
	}
	m_lod.values[index] = next;
}

void SimLodWorld::Tick(float deltaSeconds, std::uint32_t tickIndex) noexcept
{
	const float safeDelta = std::max(0.0f, deltaSeconds);
	for (std::size_t i = 0; i < m_count; ++i)
	{
		const std::uint32_t id = m_ids.values[i];
		const std::uint8_t period = m_updatePeriod.values[i];
		if (((id + tickIndex) % period) != 0u) continue;
		if (m_lod.values[i] == SimLod::GraphSoul)
		{
			m_arcProgress.values[i] += m_speed.values[i] * safeDelta / 240.0f;
			m_arcProgress.values[i] -= std::floor(m_arcProgress.values[i]);
		}
		m_scheduleClock.values[i] = WrapDay(m_scheduleClock.values[i] + safeDelta);
		if (static_cast<std::uint32_t>(m_scheduleClock.values[i] / 3600.0f) != static_cast<std::uint32_t>((m_scheduleClock.values[i] - safeDelta) / 3600.0f)) ++m_scheduleTransitions;
	}
}

LodCounts SimLodWorld::CountLods() const noexcept
{
	LodCounts counts{};
	for (std::size_t i = 0; i < m_count; ++i)
	{
		switch (m_lod.values[i])
		{
		case SimLod::Hydrated3D: ++counts.lod0; break;
		case SimLod::Kinematic: ++counts.lod1; break;
		case SimLod::GraphSoul: ++counts.lod2; break;
		}
	}
	return counts;
}
Vec3 SimLodWorld::Position(std::size_t index) const noexcept { return index < m_count ? Vec3{m_x.values[index], m_y.values[index], m_z.values[index]} : Vec3{}; }
SimLod SimLodWorld::Lod(std::size_t index) const noexcept { return index < m_count ? m_lod.values[index] : SimLod::GraphSoul; }
float SimLodWorld::ScheduleClock(std::size_t index) const noexcept { return index < m_count ? m_scheduleClock.values[index] : 0.0f; }
std::int16_t SimLodWorld::ScheduleJitter(std::size_t index) const noexcept { return index < m_count ? m_scheduleJitter.values[index] : 0; }
std::uint32_t SimLodWorld::Id(std::size_t index) const noexcept { return index < m_count ? m_ids.values[index] : kUnknownActor; }void SimLodWorld::GiveSoulItem(std::size_t index, ItemId item) noexcept
{
	if (index >= m_count || item == 0) return;
	SoulState& soul = m_souls[index];
	if (soul.Owns(item) || soul.itemCount >= soul.canonicalItems.size()) return;
	soul.canonicalItems[soul.itemCount++] = item;
}
bool SimLodWorld::SoulOwns(std::size_t index, ItemId item) const noexcept { return index < m_count && m_souls[index].Owns(item); }
const SoulState& SimLodWorld::Soul(std::size_t index) const noexcept { static const SoulState empty{}; return index < m_count ? m_souls[index] : empty; }
BodySlot* SimLodWorld::BodyForNpc(std::size_t index) noexcept { return index < m_count && m_lod.values[index] == SimLod::Hydrated3D ? m_bodies.Resolve(m_bodyHandles.values[index]) : nullptr; }
bool SimLodWorld::ScheduledThisTick(std::size_t index, std::uint32_t tickIndex) const noexcept { return index < m_count && ((m_ids.values[index] + tickIndex) % m_updatePeriod.values[index]) == 0u; }

void EpisodicMemoryRingBuffer::Record(const EpisodicMemory& event) noexcept
{
	m_events[m_next] = event;
	m_next = static_cast<std::uint8_t>((m_next + 1u) % kEpisodicMemoryCapacity);
	if (m_count < kEpisodicMemoryCapacity) ++m_count;
}
std::span<const EpisodicMemory> EpisodicMemoryRingBuffer::Events() const noexcept
{
	return {m_events.data(), m_count};
}

void CrimeDeductionEngine::RecordAuditoryLockpick(EpisodicMemoryRingBuffer& memory, Vec3 forge, float timeSeconds, bool wallOccludes) noexcept
{
	memory.Record({timeSeconds, kUnknownActor, forge, MemoryEventType::HeardLockpickNoise, wallOccludes ? 0.55f : 0.85f, 0, false});
}
void CrimeDeductionEngine::RecordVisualExit(EpisodicMemoryRingBuffer& memory, NpcId visibleActor, Vec3 door, float timeSeconds, bool lineOfSight, ItemId visibleItem) noexcept
{
	if (!lineOfSight || visibleActor == kUnknownActor) return;
	memory.Record({timeSeconds, visibleActor, door, MemoryEventType::SawActorExitForge, 0.92f, visibleItem, true});
	if (visibleItem != 0) memory.Record({timeSeconds, visibleActor, door, MemoryEventType::SawStolenItem, 0.92f, visibleItem, true});
}
CrimeDeduction CrimeDeductionEngine::Deduce(const EpisodicMemoryRingBuffer& memory, const ItemProvenance& provenance, NpcId expectedOwner, float chestTamperTime) noexcept
{
	const auto events = memory.Events();
	const EpisodicMemory* auditory = nullptr;
	const EpisodicMemory* exit = nullptr;
	for (const EpisodicMemory& event : events)
	{
		if (event.type == MemoryEventType::HeardLockpickNoise && event.subject == kUnknownActor && event.timeSeconds <= chestTamperTime) auditory = &event;
		if (event.type == MemoryEventType::SawActorExitForge && event.visualLineOfSight && event.timeSeconds >= chestTamperTime) exit = &event;
	}
	if (auditory == nullptr || exit == nullptr) return {};
	const float deltaTime = exit->timeSeconds - auditory->timeSeconds;
	if (deltaTime < 0.0f || deltaTime > 20.0f) return {};
	const float dx = exit->position.x - auditory->position.x;
	const float dy = exit->position.y - auditory->position.y;
	const float dz = exit->position.z - auditory->position.z;
	const float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
	if (distance > 25.0f) return {};
	const bool itemMatches = provenance.item != 0 && exit->item == provenance.item;
	const bool ownerMatches = provenance.owner == expectedOwner;
	const float priorOdds = 0.05f / 0.95f;
	const float temporalLikelihood = 5.3f * std::exp(-deltaTime / 30.0f);
	const float spatialLikelihood = 5.0f * std::exp(-distance / 50.0f);
	const float provenanceLikelihood = (itemMatches && ownerMatches) ? (4.5f + 2.2f * std::clamp(provenance.conspicuousness, 0.0f, 1.0f)) : 1.0f;
	const float odds = priorOdds * temporalLikelihood * spatialLikelihood * provenanceLikelihood;
	return {exit->subject, odds / (1.0f + odds), true, itemMatches && ownerMatches};
}
}