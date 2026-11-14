#pragma once

#include "Types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace bohemia
{
inline constexpr std::size_t kBodyPoolCapacity = 128;

struct BodySlot
{
	NpcId owner = kUnknownActor;
	std::array<ItemId, 4> attachedToolProps{};
	std::uint32_t animationState = 0;
	std::uint32_t combatFlags = 0;
	bool inUse = false;
};

class FixedBodyPool
{
public:
	FixedBodyPool() noexcept;
	[[nodiscard]] BodyHandle Acquire(NpcId owner) noexcept;
	void Release(BodyHandle handle) noexcept;
	[[nodiscard]] const BodySlot* Resolve(BodyHandle handle) const noexcept;
	[[nodiscard]] BodySlot* Resolve(BodyHandle handle) noexcept;
	[[nodiscard]] std::size_t Available() const noexcept { return m_freeCount; }
	[[nodiscard]] std::uint32_t SanitizedReleaseCount() const noexcept { return m_sanitizedReleaseCount; }
private:
	std::array<BodySlot, kBodyPoolCapacity> m_slots{};
	std::array<std::uint16_t, kBodyPoolCapacity> m_free{};
	std::array<std::uint16_t, kBodyPoolCapacity> m_generation{};
	std::size_t m_freeCount = 0;
	std::uint32_t m_sanitizedReleaseCount = 0;
};

struct LodCounts { std::uint32_t lod0 = 0, lod1 = 0, lod2 = 0; };
struct TickStats { std::uint32_t visited = 0, updated = 0; };

class SimLodWorld
{
public:
	void InitializeDemo(std::size_t count) noexcept;
	void SetNpcDistance(std::size_t index, float distanceMeters) noexcept;
	void Tick(float deltaSeconds, std::uint32_t tickIndex) noexcept;
	[[nodiscard]] LodCounts CountLods() const noexcept;
	[[nodiscard]] Vec3 Position(std::size_t index) const noexcept;
	[[nodiscard]] SimLod Lod(std::size_t index) const noexcept;
	[[nodiscard]] float ScheduleClock(std::size_t index) const noexcept;
	[[nodiscard]] std::int16_t ScheduleJitter(std::size_t index) const noexcept;
	[[nodiscard]] std::uint32_t Id(std::size_t index) const noexcept;
	void GiveSoulItem(std::size_t index, ItemId item) noexcept;
	[[nodiscard]] bool SoulOwns(std::size_t index, ItemId item) const noexcept;
	[[nodiscard]] const SoulState& Soul(std::size_t index) const noexcept;
	[[nodiscard]] const FixedBodyPool& Bodies() const noexcept { return m_bodies; }
	[[nodiscard]] FixedBodyPool& Bodies() noexcept { return m_bodies; }
	[[nodiscard]] BodySlot* BodyForNpc(std::size_t index) noexcept;
	[[nodiscard]] bool ScheduledThisTick(std::size_t index, std::uint32_t tickIndex) const noexcept;
	[[nodiscard]] std::size_t Size() const noexcept { return m_count; }
	[[nodiscard]] std::uint32_t ScheduleTransitions() const noexcept { return m_scheduleTransitions; }
	[[nodiscard]] std::uint64_t PositionDriftCount() const noexcept { return m_positionDriftCount; }
private:
	void UpdateNpcLod(std::size_t index) noexcept;
	std::size_t m_count = 0;
	SoaColumn<NpcId> m_ids{};
	SoaColumn<float> m_x{}, m_y{}, m_z{}, m_distance{}, m_arcProgress{}, m_scheduleClock{}, m_speed{};
	SoaColumn<std::int16_t> m_scheduleJitter{};
	SoaColumn<SimLod> m_lod{};
	SoaColumn<BodyHandle> m_bodyHandles{};
	std::array<SoulState, kMaxNpcs> m_souls{};
	SoaColumn<std::uint8_t> m_updatePeriod{};
	FixedBodyPool m_bodies{};
	std::uint32_t m_scheduleTransitions = 0;
	std::uint64_t m_positionDriftCount = 0;
};

enum class MemoryEventType : std::uint8_t { HeardLockpickNoise, SawActorNearForge, SawActorExitForge, SawStolenItem, ChestTampered };
struct EpisodicMemory
{
	float timeSeconds = 0.0f;
	NpcId subject = kUnknownActor;
	Vec3 position{};
	MemoryEventType type = MemoryEventType::HeardLockpickNoise;
	float clarity = 0.0f;
	ItemId item = 0;
	bool visualLineOfSight = false;
};

class EpisodicMemoryRingBuffer
{
public:
	void Record(const EpisodicMemory& event) noexcept;
	[[nodiscard]] std::span<const EpisodicMemory> Events() const noexcept;
	[[nodiscard]] std::size_t Count() const noexcept { return m_count; }
private:
	std::array<EpisodicMemory, kEpisodicMemoryCapacity> m_events{};
	std::uint8_t m_next = 0;
	std::uint8_t m_count = 0;
};

struct ItemProvenance { ItemId item = 0; NpcId owner = kUnknownActor; float conspicuousness = 0.0f; };
struct CrimeDeduction { NpcId suspect = kUnknownActor; float posterior = 0.0f; bool correlatedAudioAndExit = false; bool provenanceMatched = false; };
class CrimeDeductionEngine
{
public:
	static void RecordAuditoryLockpick(EpisodicMemoryRingBuffer& memory, Vec3 forge, float timeSeconds, bool wallOccludes) noexcept;
	static void RecordVisualExit(EpisodicMemoryRingBuffer& memory, NpcId visibleActor, Vec3 door, float timeSeconds, bool lineOfSight, ItemId visibleItem) noexcept;
	[[nodiscard]] static CrimeDeduction Deduce(const EpisodicMemoryRingBuffer& memory, const ItemProvenance& provenance, NpcId expectedOwner, float chestTamperTime) noexcept;
};
}