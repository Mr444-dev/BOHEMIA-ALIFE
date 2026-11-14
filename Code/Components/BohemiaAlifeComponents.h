#pragma once

#include <CryRenderer/IRenderAuxGeom.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <array>
#include <cstdint>
#include "../../src/core/ArmorBallistics.h"
#include "../../src/core/BehaviorObjects.h"
#include "../../src/core/SimLodAndCrime.h"

class CBohemiaAlifeWorldComponent final : public IEntityComponent
{
public:
	static void ReflectType(Schematyc::CTypeDesc<CBohemiaAlifeWorldComponent>& desc);
	void Initialize() override;
	void OnShutDown() override;
	Cry::Entity::EventFlags GetEventMask() const override;
	void ProcessEvent(const SEntityEvent& event) override;
	[[nodiscard]] const bohemia::SimLodWorld& GetSimulation() const noexcept { return m_world; }
private:
	void DrawTown(IRenderAuxGeom* aux);
	void DrawDashedRing(IRenderAuxGeom* aux, float radius, const ColorB& color);
	void DrawHydrationRings(IRenderAuxGeom* aux);
	void DrawCrimeLinks(IRenderAuxGeom* aux);
	void DrawBladeVector(IRenderAuxGeom* aux);
	static constexpr std::size_t kNpcCount = 1000;
	bohemia::SimLodWorld m_world;
	std::array<bohemia::Vec3, kNpcCount> m_positions{};
	std::array<float, kNpcCount> m_routeProgress{};
	std::array<float, kNpcCount> m_routeSpeeds{};
	std::array<std::uint8_t, kNpcCount> m_routeIds{};
	std::array<EntityId, 18> m_landmarks{};
	std::array<bool, 18> m_ownsLandmarks{};
	std::array<EntityId, bohemia::kBodyPoolCapacity> m_hydratedBodies{};
	Vec3 m_focus = Vec3(101.0f, 65.0f, 0.25f);
	float m_fixedAccumulator = 0.0f;
	std::uint32_t m_tick = 0;
};

class CNpcHydratedBodyComponent final : public IEntityComponent
{
public:
	static void ReflectType(Schematyc::CTypeDesc<CNpcHydratedBodyComponent>& desc);
	void SetBodyHandle(bohemia::BodyHandle handle) noexcept { m_body = handle; }
	[[nodiscard]] bohemia::BodyHandle GetBodyHandle() const noexcept { return m_body; }
private:
	bohemia::BodyHandle m_body{};
};

class CSmartObjectStationComponent final : public IEntityComponent
{
public:
	static void ReflectType(Schematyc::CTypeDesc<CSmartObjectStationComponent>& desc);
	[[nodiscard]] bohemia::SmartObjectHandle GetStationHandle() const noexcept { return m_handle; }
private:
	bohemia::SmartObjectHandle m_handle{};
};

class CLayeredArmorCombatComponent final : public IEntityComponent
{
public:
	static void ReflectType(Schematyc::CTypeDesc<CLayeredArmorCombatComponent>& desc);
	[[nodiscard]] bohemia::ArmorImpact ApplyStrike(const bohemia::StrikeInput& strike) noexcept { return bohemia::ArmorBallistics::ResolveStrike(m_armor, strike); }
	[[nodiscard]] const bohemia::ArmorState& GetArmorState() const noexcept { return m_armor; }
private:
	bohemia::ArmorState m_armor{};
};
