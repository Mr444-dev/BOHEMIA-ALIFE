#pragma once

#include <array>
#include <cstdint>

namespace bohemia
{
enum class ArmorLayer : std::uint8_t { Plate, Maille, Gambeson };
struct ArmorState
{
	std::array<float, 3> condition{1.0f, 1.0f, 1.0f};
	float stamina = 100.0f;
	float health = 100.0f;
};
struct StrikeInput
{
	float energyJoules = 115.0f;
	std::array<float, 3> direction{0.0f, 0.0f, 1.0f};
	std::array<float, 3> plateNormal{0.0f, 0.0f, 1.0f};
	float steelFrictionCoefficient = 0.60f;
	float platePerforationJoules = 185.0f;
	float gambesonThicknessMeters = 0.05f;
	float gambesonAttenuationPerMeter = 19.0f;
};
struct ArmorImpact
{
	float incidenceRadians = 0.0f;
	float postGlanceEnergyJoules = 0.0f;
	float glancingDeflectedJoules = 0.0f;
	float plateAbsorbedJoules = 0.0f;
	float mailleAbsorbedJoules = 0.0f;
	float gambesonAbsorbedJoules = 0.0f;
	float staminaAbsorbedJoules = 0.0f;
	float tissueEnergyJoules = 0.0f;
	float staminaSpent = 0.0f;
	float healthDamage = 0.0f;
	bool glanced = false;
	bool platePerforated = false;
};
class ArmorBallistics
{
public:
	[[nodiscard]] static ArmorImpact ResolveStrike(ArmorState& armor, const StrikeInput& strike) noexcept;
	[[nodiscard]] static float EffectivePerforationResistance(float baseJoules, float condition) noexcept;
};
}