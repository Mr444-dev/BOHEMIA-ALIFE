#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace bohemia
{
inline constexpr std::size_t kMaxNpcs = 5000;
inline constexpr std::size_t kMaxSmartObjects = 64;
inline constexpr std::size_t kMaxAreaParticipants = 16;
inline constexpr std::size_t kEpisodicMemoryCapacity = 16;
inline constexpr std::uint32_t kUnknownActor = 0;
inline constexpr std::uint16_t kNoSubtree = 0;

using NpcId = std::uint32_t;
using ItemId = std::uint32_t;

enum class SimLod : std::uint8_t { Hydrated3D = 0, Kinematic = 1, GraphSoul = 2 };
enum class SubbrainLayer : std::uint8_t { Ambient = 0, SocialWeather = 1, Investigation = 2, CombatArrest = 3 };
enum class Intention : std::uint8_t { Work, Eat, HaveFun, Sleep, Count };
enum class BlackboardKey : std::uint8_t { NeedWork, NeedFood, NeedFun, NeedSleep, CurrentArea, CurrentTarget, AlarmLevel, Count };
enum class BehaviorObjectKind : std::uint8_t { SmartObject, NavigationSmartObject, SmartArea, QuestSmartObject, Situation };
enum class ActivityRole : std::uint8_t { None, WorkAtAnvil, Grindstone, DicePlayer, DrinkAleAtBar, ListenToBard, MarketBrowse, GuardPost, Accuser, ArrestingGuard, Suspect, Bystander };

struct Vec3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct SmartObjectHandle
{
	std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t generation = 0;
	[[nodiscard]] constexpr bool IsValid() const noexcept { return index != std::numeric_limits<std::uint32_t>::max(); }
	friend constexpr bool operator==(SmartObjectHandle, SmartObjectHandle) noexcept = default;
};

struct BodyHandle
{
	std::uint16_t index = std::numeric_limits<std::uint16_t>::max();
	std::uint16_t generation = 0;
	[[nodiscard]] constexpr bool IsValid() const noexcept { return index != std::numeric_limits<std::uint16_t>::max(); }
};

struct LeaseToken
{
	SmartObjectHandle object{};
	NpcId owner = kUnknownActor;
	std::uint16_t subtreeId = kNoSubtree;
	[[nodiscard]] constexpr bool IsValid() const noexcept { return object.IsValid() && owner != kUnknownActor; }
};

struct SoulState
{
	std::array<ItemId, 8> canonicalItems{};
	std::uint8_t itemCount = 0;
	[[nodiscard]] bool Owns(ItemId item) const noexcept
	{
		for (std::uint8_t i = 0; i < itemCount; ++i) if (canonicalItems[i] == item) return true;
		return false;
	}
};
template<class T>
struct alignas(64) SoaColumn
{
	std::array<T, kMaxNpcs> values{};
};

constexpr std::uint32_t WangHash(std::uint32_t value) noexcept
{
	value = (value ^ 61u) ^ (value >> 16u);
	value *= 9u;
	value ^= value >> 4u;
	value *= 0x27d4eb2du;
	value ^= value >> 15u;
	return value;
}
}