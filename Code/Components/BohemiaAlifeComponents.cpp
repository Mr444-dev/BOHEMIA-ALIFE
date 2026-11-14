#include "../StdAfx.h"
#include "BohemiaAlifeComponents.h"
#include "../../src/core/TownRouteGraph.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CryCore/StaticInstanceList.h>
#include <CryEntitySystem/IEntitySystem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <tuple>

namespace
{
constexpr std::size_t kNpcCount = 1000;
constexpr std::size_t kHydratedCapacity = bohemia::kBodyPoolCapacity;
constexpr float kPi = 3.14159265358979323846f;

using SRoutePoint = bohemia::town::Point;
using SNpcRoute = bohemia::town::Route;
struct SSceneProp { const char* name; const char* geometry; Vec3 position; Vec3 scale; };

constexpr const auto& kNpcRoutes = bohemia::town::kRoutes;

std::uint32_t MixHash(std::uint32_t value) noexcept
{
	value ^= value >> 16u; value *= 0x7FEB352Du; value ^= value >> 15u; value *= 0x846CA68Bu; value ^= value >> 16u;
	return value;
}
float RouteLength(const SNpcRoute& route) noexcept
{
	return bohemia::town::Length(route);
}
SRoutePoint RoutePosition(const SNpcRoute& route, float distance) noexcept
{
	return bohemia::town::PositionAtDistance(route, distance);
}

float GroundHeight(float x, float y) noexcept
{
	return gEnv != nullptr && gEnv->p3DEngine != nullptr ? gEnv->p3DEngine->GetTerrainElevation(x, y) : 0.0f;
}

IEntity* SpawnMeshEntity(const char* name, const char* geometry, const Vec3& position, const Vec3& scale)
{
	if (gEnv == nullptr || gEnv->pEntitySystem == nullptr) return nullptr;
	SEntitySpawnParams params;
	params.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
	params.sName = name;
	params.vPosition = position;
	IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(params);
	if (pEntity == nullptr) return nullptr;
	if (pEntity->LoadGeometry(0, geometry) < 0 && pEntity->LoadGeometry(0, "objects/sphere.cgf") < 0)
	{
		gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
		return nullptr;
	}
	pEntity->SetScale(scale);
	return pEntity;
}

template<class T> void Describe(T& desc, const char* label)
{
	desc.SetLabel(label);
	desc.SetDescription("BOHEMIA-ALIFE simulation adapter backed by the standalone C++20 core.");
	desc.SetComponentFlags(EEntityComponentFlags::Singleton);
}
void RegisterBohemiaComponents(Schematyc::IEnvRegistrar& registrar)
{
	Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
	scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CBohemiaAlifeWorldComponent));
	scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CNpcHydratedBodyComponent));
	scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CSmartObjectStationComponent));
	scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CLayeredArmorCombatComponent));
}
CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterBohemiaComponents);
}

void CBohemiaAlifeWorldComponent::ReflectType(Schematyc::CTypeDesc<CBohemiaAlifeWorldComponent>& desc)
{
	desc.SetGUID("{1595B8C1-E7A7-4A6B-AFEA-5BA703901001}"_cry_guid); Describe(desc, "BOHEMIA ALIFE World");
}

void CBohemiaAlifeWorldComponent::Initialize()
{
	m_world.InitializeDemo(kNpcCount);
	m_fixedAccumulator = 0.0f;
	m_tick = 0;
	m_landmarks.fill(INVALID_ENTITYID);
	m_ownsLandmarks.fill(false);
	m_hydratedBodies.fill(INVALID_ENTITYID);

	for (std::size_t i = 0; i < kNpcCount; ++i)
	{
		const std::uint32_t seed = MixHash(static_cast<std::uint32_t>(i + 1u));
		m_routeIds[i] = static_cast<std::uint8_t>(seed % kNpcRoutes.size());
		const SNpcRoute& route = kNpcRoutes[m_routeIds[i]];
		const float length = RouteLength(route);
		m_routeProgress[i] = length * static_cast<float>(MixHash(seed ^ 0xA511E9B3u) % 10000u) / 10000.0f;
		m_routeSpeeds[i] = 0.55f + static_cast<float>(MixHash(seed ^ 0x9E3779B9u) % 100u) / 100.0f;
		const SRoutePoint p = RoutePosition(route, m_routeProgress[i]);
		m_positions[i] = {p.x, p.y, GroundHeight(p.x, p.y)};
	}

	const std::array<SSceneProp, 18> scene{{
		{"BOHEMIA Forge", "engineassets/objects/primitive_box.cgf", Vec3(62.0f,43.0f,3.0f), Vec3(12.0f,8.0f,3.0f)},
		{"BOHEMIA Forge Roof", "engineassets/objects/primitive_box.cgf", Vec3(62.0f,43.0f,6.4f), Vec3(13.0f,9.0f,1.2f)},
		{"BOHEMIA Forge Anvil", "engineassets/objects/primitive_cylinder.cgf", Vec3(66.0f,49.0f,1.3f), Vec3(1.8f,1.1f,0.9f)},
		{"BOHEMIA Forge Workbench", "engineassets/objects/primitive_box.cgf", Vec3(63.0f,47.0f,0.8f), Vec3(2.2f,1.4f,0.7f)},
		{"BOHEMIA Tavern", "engineassets/objects/primitive_box.cgf", Vec3(109.0f,32.0f,3.0f), Vec3(13.0f,9.0f,3.0f)},
		{"BOHEMIA Tavern Roof", "engineassets/objects/primitive_box.cgf", Vec3(109.0f,32.0f,6.4f), Vec3(14.0f,10.0f,1.2f)},
		{"BOHEMIA Tavern Bar", "engineassets/objects/primitive_box.cgf", Vec3(109.0f,36.0f,1.0f), Vec3(7.0f,0.7f,0.5f)},
		{"BOHEMIA Guardhouse", "engineassets/objects/primitive_box.cgf", Vec3(160.0f,43.0f,3.0f), Vec3(11.5f,8.5f,3.0f)},
		{"BOHEMIA Guardhouse Roof", "engineassets/objects/primitive_box.cgf", Vec3(160.0f,43.0f,6.4f), Vec3(12.5f,9.5f,1.2f)},
		{"BOHEMIA Market Stall 1", "engineassets/objects/primitive_cylinder.cgf", Vec3(101.0f,75.0f,1.0f), Vec3(2.5f,1.7f,1.0f)},
		{"BOHEMIA Market Stall 2", "engineassets/objects/primitive_cylinder.cgf", Vec3(110.0f,71.0f,1.0f), Vec3(2.5f,1.7f,1.0f)},
		{"BOHEMIA Market Stall 3", "engineassets/objects/primitive_cylinder.cgf", Vec3(120.0f,78.0f,1.0f), Vec3(2.5f,1.7f,1.0f)},
		{"BOHEMIA Market Square", "engineassets/objects/primitive_box.cgf", Vec3(110.0f,76.0f,0.08f), Vec3(19.0f,11.5f,0.08f)},
		{"BOHEMIA Guard Post", "engineassets/objects/primitive_cylinder.cgf", Vec3(157.0f,51.0f,1.8f), Vec3(1.2f,1.2f,1.8f)},
		{"BOHEMIA Dice Table 1", "engineassets/objects/primitive_cylinder.cgf", Vec3(103.0f,31.0f,0.9f), Vec3(1.3f,1.3f,0.5f)},
		{"BOHEMIA Dice Table 2", "engineassets/objects/primitive_cylinder.cgf", Vec3(111.0f,30.0f,0.9f), Vec3(1.3f,1.3f,0.5f)},
		{"BOHEMIA Dice Table 3", "engineassets/objects/primitive_cylinder.cgf", Vec3(117.0f,34.0f,0.9f), Vec3(1.3f,1.3f,0.5f)},
		{"BOHEMIA Gate", "engineassets/objects/primitive_box.cgf", Vec3(181.0f,83.0f,3.0f), Vec3(1.0f,8.0f,3.0f)}
	}};
	for (std::size_t i = 0; i < scene.size(); ++i)
	{
		const SSceneProp& prop = scene[i];
		Vec3 position(prop.position.x, prop.position.y, GroundHeight(prop.position.x, prop.position.y) + prop.position.z);
		IEntity* pEntity = gEnv != nullptr && gEnv->pEntitySystem != nullptr ? gEnv->pEntitySystem->FindEntityByName(prop.name) : nullptr;
		if (pEntity == nullptr)
		{
			pEntity = SpawnMeshEntity(prop.name, prop.geometry, position, prop.scale);
			m_ownsLandmarks[i] = pEntity != nullptr;
		}
		if (pEntity != nullptr) m_landmarks[i] = pEntity->GetId();
	}
}

void CBohemiaAlifeWorldComponent::OnShutDown()
{
	if (gEnv == nullptr || gEnv->pEntitySystem == nullptr) return;
	for (EntityId id : m_hydratedBodies) if (id != INVALID_ENTITYID) gEnv->pEntitySystem->RemoveEntity(id);
	for (std::size_t i = 0; i < m_landmarks.size(); ++i)
		if (m_ownsLandmarks[i] && m_landmarks[i] != INVALID_ENTITYID) gEnv->pEntitySystem->RemoveEntity(m_landmarks[i]);
	m_hydratedBodies.fill(INVALID_ENTITYID);
	m_landmarks.fill(INVALID_ENTITYID);
}

Cry::Entity::EventFlags CBohemiaAlifeWorldComponent::GetEventMask() const { return Cry::Entity::EEvent::Update; }

void CBohemiaAlifeWorldComponent::ProcessEvent(const SEntityEvent& event)
{
	if (event.event != Cry::Entity::EEvent::Update || gEnv == nullptr || gEnv->pRenderer == nullptr) return;
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	const float frameDelta = std::clamp(event.fParam[0], 0.0f, 0.1f);
	m_fixedAccumulator += frameDelta;
	constexpr float fixedStep = 1.0f / 30.0f;
	while (m_fixedAccumulator >= fixedStep)
	{
		m_world.Tick(fixedStep * 144.0f, m_tick++);
		for (std::size_t i = 0; i < kNpcCount; ++i)
		{
			const SNpcRoute& route = kNpcRoutes[m_routeIds[i]];
			const float length = RouteLength(route);
			m_routeProgress[i] = std::fmod(m_routeProgress[i] + m_routeSpeeds[i] * fixedStep, length);
			const SRoutePoint p = RoutePosition(route, m_routeProgress[i]);
			m_positions[i] = {p.x, p.y, GroundHeight(p.x, p.y)};
		}
		m_fixedAccumulator -= fixedStep;
	}

	const CCamera& camera = gEnv->pSystem->GetViewCamera();
	const Vec3 view = camera.GetPosition();
	m_focus = Vec3(view.x, view.y, GroundHeight(view.x, view.y) + 0.25f);
	IRenderAuxGeom* aux = gEnv->pRenderer->GetIRenderAuxGeom();
	if (aux == nullptr) return;
	aux->SetRenderFlags(SAuxGeomRenderFlags());

	std::size_t hydratedCount = 0;
	for (std::size_t i = 0; i < kNpcCount; ++i)
	{
		const bohemia::Vec3& simPosition = m_positions[i];
		const Vec3 p(simPosition.x, simPosition.y, simPosition.z);
		const float dx = p.x - m_focus.x, dy = p.y - m_focus.y, dz = p.z - m_focus.z;
		m_world.SetNpcDistance(i, std::sqrt(dx * dx + dy * dy + dz * dz));
		if (m_world.Lod(i) != bohemia::SimLod::Hydrated3D || hydratedCount >= m_hydratedBodies.size()) continue;

		EntityId& entityId = m_hydratedBodies[hydratedCount++];
		IEntity* pBody = entityId == INVALID_ENTITYID ? nullptr : gEnv->pEntitySystem->GetEntity(entityId);
		if (pBody == nullptr)
		{
			char name[64]{};
			std::snprintf(name, sizeof(name), "BOHEMIA LOD0 NPC %03u", static_cast<unsigned>(m_world.Id(i)));
			pBody = SpawnMeshEntity(name, "engineassets/objects/primitive_cylinder.cgf", Vec3(p.x,p.y,p.z+0.75f), Vec3(0.28f,0.28f,0.72f));
			if (pBody != nullptr) entityId = pBody->GetId();
		}
		if (pBody != nullptr) pBody->SetPos(Vec3(p.x, p.y, p.z + 0.75f));
		const Vec3 body(p.x, p.y, p.z + 0.75f);
		aux->DrawSphere(Vec3(p.x,p.y,p.z+1.3f), 0.22f, ColorB(65,215,232,230));
		aux->DrawLine(Vec3(p.x,p.y,p.z+0.2f), ColorB(65,215,232,220), Vec3(p.x,p.y,p.z+1.1f), ColorB(65,215,232,220), 0.12f);
		aux->DrawLine(body, ColorB(65,215,232,220), Vec3(p.x-0.24f,p.y,p.z), ColorB(65,215,232,220), 0.10f);
		aux->DrawLine(body, ColorB(65,215,232,220), Vec3(p.x+0.24f,p.y,p.z), ColorB(65,215,232,220), 0.10f);
		if (m_world.Id(i) == 19u || m_world.Id(i) == 34u || m_world.Id(i) == 412u)
		{
			char label[40]{}; std::snprintf(label, sizeof(label), "NPC #%u", static_cast<unsigned>(m_world.Id(i)));
			IRenderAuxText::DrawLabelEx(Vec3(p.x,p.y,p.z+2.0f), 1.4f, IRenderAuxText::AColor(ColorB(220,230,235,255)), true, true, label);
		}
	}
	for (std::size_t i = hydratedCount; i < m_hydratedBodies.size(); ++i)
	{
		if (m_hydratedBodies[i] != INVALID_ENTITYID) gEnv->pEntitySystem->RemoveEntity(m_hydratedBodies[i]);
		m_hydratedBodies[i] = INVALID_ENTITYID;
	}

	DrawTown(aux);
	DrawHydrationRings(aux);
	DrawCrimeLinks(aux);
	DrawBladeVector(aux);
	char stats[128]{};
	const bohemia::LodCounts counts = m_world.CountLods();
	std::snprintf(stats, sizeof(stats), "BOHEMIA-ALIFE  |  LOD0 %u  LOD1 %u  LOD2 %u  |  1000 NPC", counts.lod0, counts.lod1, counts.lod2);
	IRenderAuxText::DrawLabelEx(m_focus + Vec3(0.0f,0.0f,3.0f), 1.7f, IRenderAuxText::AColor(ColorB(230,240,243,255)), true, true, stats);
}

void CBohemiaAlifeWorldComponent::DrawTown(IRenderAuxGeom* aux)
{
	for (const SNpcRoute& route : kNpcRoutes)
	{
		for (std::size_t i = 0; i + 1 < route.points.size(); ++i)
		{
			const SRoutePoint a = route.points[i], b = route.points[i + 1];
			const float zA = GroundHeight(a.x, a.y) + 0.12f, zB = GroundHeight(b.x, b.y) + 0.12f;
			aux->DrawLine(Vec3(a.x,a.y,zA), ColorB(90,101,103,210), Vec3(b.x,b.y,zB), ColorB(90,101,103,210), 0.4f);
		}
	}
	const std::array<std::tuple<const char*,Vec3,ColorB>,4> labels{{
		{"FORGE",Vec3(62,43,11),ColorB(242,184,75,255)},
		{"TAVERN",Vec3(109,32,11),ColorB(223,155,122,255)},
		{"GUARDHOUSE",Vec3(160,43,11),ColorB(148,190,211,255)},
		{"MARKET SQUARE",Vec3(110,62,3),ColorB(231,224,194,255)}
	}};
	for (const auto& [text, position, color] : labels) IRenderAuxText::DrawLabelEx(position, 1.6f, IRenderAuxText::AColor(color), true, true, text);
	(void)aux;
}

void CBohemiaAlifeWorldComponent::DrawDashedRing(IRenderAuxGeom* aux, float radius, const ColorB& color)
{
	const float floor = GroundHeight(m_focus.x, m_focus.y) + 0.18f;
	for (int degree = 0; degree < 360; degree += 10)
	{
		if ((degree / 10) % 2 != 0) continue;
		const float a = static_cast<float>(degree) * kPi / 180.0f;
		const float b = static_cast<float>(degree + 8) * kPi / 180.0f;
		aux->DrawLine(Vec3(m_focus.x+std::cos(a)*radius,m_focus.y+std::sin(a)*radius,floor), color,
			Vec3(m_focus.x+std::cos(b)*radius,m_focus.y+std::sin(b)*radius,floor), color, 0.18f);
	}
}
void CBohemiaAlifeWorldComponent::DrawHydrationRings(IRenderAuxGeom* aux)
{
	DrawDashedRing(aux, 28.0f, ColorB(65,215,232,220));
	DrawDashedRing(aux, 60.0f, ColorB(242,184,75,205));
	IRenderAuxText::DrawLabelEx(Vec3(m_focus.x+2.0f,m_focus.y,GroundHeight(m_focus.x,m_focus.y)+0.4f),1.3f,IRenderAuxText::AColor(ColorB(65,215,232,255)),true,false,"LOD0 HYDRATION 28m");
	IRenderAuxText::DrawLabelEx(Vec3(m_focus.x+4.0f,m_focus.y,GroundHeight(m_focus.x,m_focus.y)+0.4f),1.3f,IRenderAuxText::AColor(ColorB(242,184,75,255)),true,false,"LOD1 KINEMATIC 60m");
}
void CBohemiaAlifeWorldComponent::DrawCrimeLinks(IRenderAuxGeom* aux)
{
	const Vec3 forgeDoor(70.0f,47.0f,GroundHeight(70.0f,47.0f)+1.5f);
	const Vec3 witness19(m_positions[18].x,m_positions[18].y,m_positions[18].z+1.6f);
	const Vec3 witness34(m_positions[33].x,m_positions[33].y,m_positions[33].z+1.6f);
	aux->DrawLine(forgeDoor,ColorB(240,190,93,220),witness19,ColorB(240,190,93,110),0.08f);
	aux->DrawLine(forgeDoor,ColorB(230,204,119,220),witness34,ColorB(230,204,119,110),0.08f);
	IRenderAuxText::DrawLabelEx(forgeDoor+Vec3(0,0,1.2f),1.2f,IRenderAuxText::AColor(ColorB(240,190,93,255)),true,true,"LOCKPICK SOUND / ACTOR UNKNOWN");
}
void CBohemiaAlifeWorldComponent::DrawBladeVector(IRenderAuxGeom* aux)
{
	const Vec3 origin(65.0f,48.0f,GroundHeight(65.0f,48.0f)+2.0f);
	const Vec3 tip=origin+Vec3(3.3f,1.7f,0.8f);
	aux->DrawLine(origin,ColorB(240,111,104,255),tip,ColorB(242,184,75,255),0.16f);
	aux->DrawLine(tip,ColorB(242,184,75,255),tip+Vec3(-0.7f,0.1f,0),ColorB(242,184,75,255),0.12f);
	aux->DrawLine(tip,ColorB(242,184,75,255),tip+Vec3(-0.1f,-0.7f,0),ColorB(242,184,75,255),0.12f);
	IRenderAuxText::DrawLabelEx(tip+Vec3(0,0,0.7f),1.2f,IRenderAuxText::AColor(ColorB(242,184,75,255)),true,true,"BLADE GLANCING VECTOR");
}

void CNpcHydratedBodyComponent::ReflectType(Schematyc::CTypeDesc<CNpcHydratedBodyComponent>& desc)
{
	desc.SetGUID("{1595B8C1-E7A7-4A6B-AFEA-5BA703901002}"_cry_guid); Describe(desc, "BOHEMIA Hydrated NPC Body");
}
void CSmartObjectStationComponent::ReflectType(Schematyc::CTypeDesc<CSmartObjectStationComponent>& desc)
{
	desc.SetGUID("{1595B8C1-E7A7-4A6B-AFEA-5BA703901003}"_cry_guid); Describe(desc, "BOHEMIA Smart Object Station");
}
void CLayeredArmorCombatComponent::ReflectType(Schematyc::CTypeDesc<CLayeredArmorCombatComponent>& desc)
{
	desc.SetGUID("{1595B8C1-E7A7-4A6B-AFEA-5BA703901004}"_cry_guid); Describe(desc, "BOHEMIA Layered Armor Combat");
}
