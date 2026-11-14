#include "ArmorBallistics.h"
#include <algorithm>
#include <cmath>

namespace bohemia
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;
float Clamp01(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }
float Length(const std::array<float, 3>& v) noexcept { return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]); }
}
float ArmorBallistics::EffectivePerforationResistance(float baseJoules, float condition) noexcept
{
	return baseJoules * (0.4f + 0.6f * Clamp01(condition));
}

ArmorImpact ArmorBallistics::ResolveStrike(ArmorState& armor, const StrikeInput& strike) noexcept
{
	ArmorImpact result{};
	const float directionLength = Length(strike.direction);
	const float normalLength = Length(strike.plateNormal);
	const float dot = directionLength > 0.0f && normalLength > 0.0f
		? (strike.direction[0]*strike.plateNormal[0] + strike.direction[1]*strike.plateNormal[1] + strike.direction[2]*strike.plateNormal[2]) / (directionLength * normalLength)
		: 1.0f;
	const float cosine = std::clamp(std::abs(dot), 0.0f, 1.0f);
	result.incidenceRadians = std::acos(cosine);
	const float angleTangent = std::tan(result.incidenceRadians);
	const float incident = std::max(0.0f, strike.energyJoules);
	float energy = incident;
	if (angleTangent > std::max(0.0f, strike.steelFrictionCoefficient))
	{
		result.glanced = true;
		// Gameplay-calibrated glancing retention, not a historical material law.
		const float sinAngle = std::sin(result.incidenceRadians);
		const float retention = std::clamp(1.0f - 0.45f * sinAngle * sinAngle, 0.45f, 1.0f);
		energy = incident * retention;
	}
	result.postGlanceEnergyJoules = energy;
	result.glancingDeflectedJoules = incident - energy;

	const float plateResistance = EffectivePerforationResistance(strike.platePerforationJoules, armor.condition[0]);
	float residual = energy;
	if (residual >= plateResistance)
	{
		result.platePerforated = true;
		result.plateAbsorbedJoules = plateResistance;
		residual -= plateResistance;
	}
	else
	{
		// A stopped cut still transmits a fraction as blunt shock through the plate.
		result.plateAbsorbedJoules = residual * 0.40f;
		residual -= result.plateAbsorbedJoules;
	}

	const float mailleCapacity = 120.0f * (0.4f + 0.6f * Clamp01(armor.condition[1]));
	if (result.platePerforated && residual > 0.0f)
	{
		result.mailleAbsorbedJoules = std::min(residual, mailleCapacity);
		residual -= result.mailleAbsorbedJoules;
	}
	else if (residual > 0.0f)
	{
		result.mailleAbsorbedJoules = residual * 0.10f;
		residual -= result.mailleAbsorbedJoules;
	}

	const float padRetention = std::exp(-std::max(0.0f, strike.gambesonAttenuationPerMeter) * std::max(0.0f, strike.gambesonThicknessMeters) * Clamp01(armor.condition[2]));
	const float bodyEnergy = residual * padRetention;
	result.gambesonAbsorbedJoules = residual - bodyEnergy;
	result.staminaAbsorbedJoules = std::min(bodyEnergy, std::max(0.0f, armor.stamina) * 1.5f);
	result.staminaSpent = result.staminaAbsorbedJoules / 1.5f;
	armor.stamina = std::max(0.0f, armor.stamina - result.staminaSpent);
	const float unshieldedEnergy = std::max(0.0f, bodyEnergy - result.staminaAbsorbedJoules);
	result.tissueEnergyJoules = unshieldedEnergy;
	result.healthDamage = result.platePerforated ? unshieldedEnergy * 0.35f : unshieldedEnergy * 0.08f;
	armor.health = std::max(0.0f, armor.health - result.healthDamage);

	const float plateWear = incident / 4000.0f;
	const float mailleWear = energy / 6000.0f;
	const float padWear = residual / 2400.0f;
	armor.condition[0] = Clamp01(armor.condition[0] - plateWear);
	armor.condition[1] = Clamp01(armor.condition[1] - mailleWear);
	armor.condition[2] = Clamp01(armor.condition[2] - padWear);
	return result;
}
}