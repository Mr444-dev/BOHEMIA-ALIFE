#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace bohemia::town
{
struct Point
{
	float x = 0.0f;
	float y = 0.0f;
};

inline constexpr std::size_t kPointsPerRoute = 8;
struct Route
{
	std::array<Point, kPointsPerRoute> points{};
};

// Shared by the standalone live view and the CRYENGINE adapter so their town
// layouts, NPC paths, and diagnostic rendering use the same authored routes.
inline constexpr std::array<Route, 8> kRoutes{{
	{{{{57,48},{67,52},{82,64},{99,75},{113,76},{127,62},{116,40},{87,40}}}},
	{{{{151,46},{161,41},{170,49},{175,69},{179,88},{153,87},{126,75},{136,58}}}},
	{{{{104,34},{117,34},{130,43},{148,45},{158,52},{139,64},{117,75},{96,61}}}},
	{{{{96,75},{101,70},{108,74},{116,71},{124,78},{119,88},{103,91},{91,83}}}},
	{{{{53,45},{61,39},{70,43},{75,51},{69,58},{58,57},{51,52},{54,48}}}},
	{{{{98,30},{105,25},{114,28},{121,35},{117,41},{106,42},{99,37},{98,33}}}},
	{{{{151,41},{159,37},{167,42},{169,50},{164,56},{155,55},{149,49},{150,44}}}},
	{{{{18,98},{42,110},{78,104},{102,93},{133,103},{171,99},{187,76},{164,61}}}}
}};

[[nodiscard]] inline float Distance(Point a, Point b) noexcept
{
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	return std::sqrt(dx * dx + dy * dy);
}

[[nodiscard]] inline float Length(const Route& route) noexcept
{
	float length = 0.0f;
	for (std::size_t i = 0; i + 1 < route.points.size(); ++i)
		length += Distance(route.points[i], route.points[i + 1]);
	return length;
}

[[nodiscard]] inline Point PositionAtDistance(const Route& route, float distance) noexcept
{
	for (std::size_t i = 0; i + 1 < route.points.size(); ++i)
	{
		const Point a = route.points[i], b = route.points[i + 1];
		const float segment = Distance(a, b);
		if (distance <= segment || i + 2 == route.points.size())
		{
			const float t = segment > 0.001f ? std::clamp(distance / segment, 0.0f, 1.0f) : 0.0f;
			return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
		}
		distance -= segment;
	}
	return route.points.front();
}
}
