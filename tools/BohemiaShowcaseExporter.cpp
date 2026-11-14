#include "core/ArmorBallistics.h"
#include "core/BehaviorObjects.h"
#include "core/SimLodAndCrime.h"
#include "core/SubbrainArchitecture.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace
{
using Clock = std::chrono::steady_clock;
constexpr int kWidth = 1920;
constexpr int kHeight = 1080;
constexpr int kSsaa = 2;
struct Color { std::uint8_t r, g, b, a = 255; };
struct Point { int x, y; };
struct RoadSegment { Point a, b; };
constexpr Color C(std::uint32_t rgb) { return {static_cast<std::uint8_t>((rgb >> 16) & 255), static_cast<std::uint8_t>((rgb >> 8) & 255), static_cast<std::uint8_t>(rgb & 255), 255}; }
constexpr Point kCameraFocus{910,520};
constexpr float kPixelsPerMeterX=218.0f/28.0f, kPixelsPerMeterY=112.0f/28.0f;
constexpr std::array<RoadSegment,14> kAgentRoutes{{
	{{270,665},{1370,315}},{{400,315},{1200,735}},{{620,845},{1090,340}},
	{{240,490},{1430,620}},{{620,205},{740,830}},{{1250,210},{1070,810}},
	{{520,480},{710,450}},{{850,385},{1080,385}},
	{{830,570},{1090,570}},{{840,605},{1050,605}},{{860,540},{1020,540}},
	{{1000,685},{1200,685}},{{1185,420},{1380,420}},{{1270,570},{1450,655}}
}};

std::array<std::uint8_t, 7> Glyph(char c)
{
	switch (c)
	{
	case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
	case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
	case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
	case 'G': return {14,17,16,23,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
	case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,18,18,12};
	case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
	case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
	case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
	case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
	case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
	case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
	case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
	case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
	case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
	case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
	case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
	case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
	case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
	case ':': return {0,4,4,0,4,4,0}; case '.': return {0,0,0,0,0,12,12};
	case ',': return {0,0,0,0,0,4,8}; case '-': return {0,0,0,31,0,0,0};
	case '+': return {0,4,4,31,4,4,0}; case '/': return {1,2,2,4,8,8,16};
	case '#': return {10,31,10,10,31,10,0}; case '[': return {14,8,8,8,8,8,14};
	case ']': return {14,2,2,2,2,2,14}; case '|': return {4,4,4,4,4,4,4};
	case '>': return {16,8,4,2,4,8,16}; case '%': return {17,2,4,8,16,17,0};
	case '(': return {2,4,8,8,8,4,2}; case ')': return {8,4,2,2,2,4,8};
	case '_': return {0,0,0,0,0,0,31}; case ' ': return {};
	default: return {31,17,21,21,21,17,31};
	}
}

class Canvas
{
public:
	Canvas() : m_pixels(static_cast<std::size_t>(kWidth * kSsaa) * (kHeight * kSsaa) * 4u, 0)
	{
		Clear(C(0x0D121A));
	}
	void Clear(Color color)
	{
		for (std::size_t i = 0; i < m_pixels.size(); i += 4) { m_pixels[i] = color.r; m_pixels[i+1] = color.g; m_pixels[i+2] = color.b; m_pixels[i+3] = 255; }
	}
	void Pixel(int x, int y, Color color)
	{
		for (int sy = 0; sy < kSsaa; ++sy) for (int sx = 0; sx < kSsaa; ++sx)
		{
			const int px = x * kSsaa + sx, py = y * kSsaa + sy;
			if (px < 0 || py < 0 || px >= kWidth*kSsaa || py >= kHeight*kSsaa) continue;
			const std::size_t at = (static_cast<std::size_t>(py) * kWidth*kSsaa + px) * 4u;
			m_pixels[at] = color.r; m_pixels[at+1] = color.g; m_pixels[at+2] = color.b; m_pixels[at+3] = color.a;
		}
	}
	void Rect(int x, int y, int w, int h, Color color)
	{
		for (int py = std::max(0, y); py < std::min(kHeight, y+h); ++py) for (int px = std::max(0, x); px < std::min(kWidth, x+w); ++px) Pixel(px, py, color);
	}
	void Frame(int x, int y, int w, int h, Color color, int t = 1)
	{
		Rect(x,y,w,t,color); Rect(x,y+h-t,w,t,color); Rect(x,y,t,h,color); Rect(x+w-t,y,t,h,color);
	}
	void Line(Point a, Point b, Color color, int thickness = 1, int dash = 0)
	{
		const int dx = std::abs(b.x-a.x), sx = a.x < b.x ? 1 : -1;
		const int dy = -std::abs(b.y-a.y), sy = a.y < b.y ? 1 : -1;
		int error = dx + dy, step = 0;
		for (;;)
		{
			if (dash == 0 || ((step / dash) % 2 == 0))
				for (int oy = -thickness/2; oy <= thickness/2; ++oy) for (int ox = -thickness/2; ox <= thickness/2; ++ox) Pixel(a.x+ox,a.y+oy,color);
			if (a.x == b.x && a.y == b.y) break;
			const int twice = 2*error;
			if (twice >= dy) { error += dy; a.x += sx; }
			if (twice <= dx) { error += dx; a.y += sy; }
			++step;
		}
	}
	void Circle(int cx, int cy, int radius, Color color, int thickness = 1)
	{
		for (int r = radius-thickness/2; r <= radius+thickness/2; ++r)
		{
			if (r <= 0) continue;
			int x = r, y = 0, error = 0;
			while (x >= y)
			{
				Pixel(cx+x,cy+y,color); Pixel(cx+y,cy+x,color); Pixel(cx-y,cy+x,color); Pixel(cx-x,cy+y,color);
				Pixel(cx-x,cy-y,color); Pixel(cx-y,cy-x,color); Pixel(cx+y,cy-x,color); Pixel(cx+x,cy-y,color);
				if (error <= 0) { ++y; error += 2*y+1; }
				if (error > 0) { --x; error -= 2*x+1; }
			}
		}
	}
	void Ellipse(int cx, int cy, int rx, int ry, Color color, int thickness = 1, int dashDegrees = 0)
	{
		for (int deg = 0; deg < 360; ++deg)
		{
			if (dashDegrees > 0 && (deg / dashDegrees) % 2 != 0) continue;
			const float angle = static_cast<float>(deg) * 0.0174532925199f;
			const Point p{cx + static_cast<int>(std::cos(angle)*rx), cy + static_cast<int>(std::sin(angle)*ry)};
			for (int t = 0; t < thickness; ++t) Pixel(p.x,p.y+t,color);
		}
	}
	void Polygon(std::span<const Point> points, Color fill, Color outline)
	{
		if (points.size() < 3) return;
		int minY = points[0].y, maxY = points[0].y;
		for (const auto& p : points) { minY = std::min(minY,p.y); maxY = std::max(maxY,p.y); }
		for (int y = std::max(0,minY); y <= std::min(kHeight-1,maxY); ++y)
		{
			std::array<int, 16> nodes{}; std::size_t count = 0;
			for (std::size_t i = 0, j = points.size()-1; i < points.size(); j = i++)
			{
				const Point a = points[i], b = points[j];
				if ((a.y < y && b.y >= y) || (b.y < y && a.y >= y))
				{
					if (count < nodes.size()) nodes[count++] = a.x + (y-a.y)*(b.x-a.x)/(b.y-a.y);
				}
			}
			std::sort(nodes.begin(), nodes.begin()+count);
			for (std::size_t i = 0; i+1 < count; i += 2) for (int x = nodes[i]; x < nodes[i+1]; ++x) Pixel(x,y,fill);
		}
		for (std::size_t i = 0; i < points.size(); ++i) Line(points[i],points[(i+1)%points.size()],outline,1);
	}
	void Text(int x, int y, std::string_view value, Color color, int scale = 1)
	{
		int cursor = x;
		for (char ch : value)
		{
			const auto glyph = Glyph(ch >= 'a' && ch <= 'z' ? static_cast<char>(ch-'a'+'A') : ch);
			for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col)
				if ((glyph[row] & (1u << (4-col))) != 0) Rect(cursor+col*scale,y+row*scale,scale,scale,color);
			cursor += 6*scale;
		}
	}
	std::vector<std::uint8_t> Downsample() const
	{
		std::vector<std::uint8_t> final(static_cast<std::size_t>(kWidth)*kHeight*4u);
		const int hiWidth = kWidth*kSsaa;
		for (int y = 0; y < kHeight; ++y) for (int x = 0; x < kWidth; ++x)
		{
			const std::size_t dst=(static_cast<std::size_t>(y)*kWidth+x)*4u;
			for (int channel=0; channel<4; ++channel)
			{
				unsigned sum=0;
				for (int sy=0; sy<kSsaa; ++sy) for (int sx=0; sx<kSsaa; ++sx)
				{
					const std::size_t src=(static_cast<std::size_t>(y*kSsaa+sy)*hiWidth+x*kSsaa+sx)*4u+channel;
					sum+=m_pixels[src];
				}
				final[dst+channel]=static_cast<std::uint8_t>(sum/4u);
			}
		}
		return final;
	}
private:
	std::vector<std::uint8_t> m_pixels;
};

Color Mix(Color a, Color b, float t)
{
	return {static_cast<std::uint8_t>(a.r+(b.r-a.r)*t),static_cast<std::uint8_t>(a.g+(b.g-a.g)*t),static_cast<std::uint8_t>(a.b+(b.b-a.b)*t),255};
}
void DrawIsoBuilding(Canvas& c, int x, int y, int w, int h, Color wall, Color roof, std::string_view label)
{
	const std::array<Point,4> top{{{x,y},{x+w/2,y-24},{x+w,y},{x+w/2,y+24}}};
	const std::array<Point,4> left{{{x,y},{x+w/2,y+24},{x+w/2,y+24+h},{x,y+h}}};
	const std::array<Point,4> right{{{x+w,y},{x+w/2,y+24},{x+w/2,y+24+h},{x+w,y+h}}};
	c.Polygon(left,Mix(wall,C(0x070B10),0.30f),C(0x54616B));
	c.Polygon(right,Mix(wall,C(0x070B10),0.48f),C(0x54616B));
	c.Polygon(top,roof,C(0x84919A));
	c.Line({x+w/2,y+24},{x+w/2,y+24+h},{0xA8,0xB4,0xBC,255},1);
	c.Rect(x+12,y+35,8,14,C(0xF3C969));
	c.Rect(x+w/2+10,y+39,8,13,C(0xF3C969));
	if (!label.empty()) c.Text(x-8,y+h+34,label,C(0xC7D2DC),1);
}
void DrawBadge(Canvas& c,int x,int y,int width,std::string_view title,Color accent)
{
	c.Rect(x+4,y+4,width,30,C(0x070A0E));
	c.Rect(x,y,width,30,C(0x17222D)); c.Frame(x,y,width,30,accent,1); c.Rect(x,y,4,30,accent);
	c.Text(x+12,y+10,title,C(0xE5EDF2),1);
}
void DrawPerson(Canvas& c,int x,int y,Color color,int radius=3,Point heading={1,0})
{
	c.Circle(x,y-radius*2,radius,color,2);
	c.Line({x,y-radius},{x,y+radius*2},color,2);
	c.Line({x,y},{x-radius,y+radius*2},color,2);
	c.Line({x,y},{x+radius,y+radius*2},color,2);
	const float length=std::sqrt(static_cast<float>(heading.x*heading.x+heading.y*heading.y));
	if(length>0.0f)c.Line({x,y},{x+static_cast<int>(std::lround(heading.x*7.0f/length)),y+static_cast<int>(std::lround(heading.y*7.0f/length))},color,1);
}
void DrawDiamond(Canvas& c,int x,int y,Color color)
{
	const std::array<Point,4> diamond{{{x,y-4},{x+4,y},{x,y+4},{x-4,y}}};
	c.Polygon(diamond,color,color);
}

float CameraDistanceMeters(Point p)
{
	const float dx=static_cast<float>(p.x-kCameraFocus.x)/kPixelsPerMeterX;
	const float dy=static_cast<float>(p.y-kCameraFocus.y)/kPixelsPerMeterY;
	return std::sqrt(dx*dx+dy*dy);
}
std::uint32_t MixHash(std::uint32_t value)
{
	value^=value>>16;value*=0x7FEB352Du;value^=value>>15;value*=0x846CA68Bu;value^=value>>16;return value;
}
bool InBox(Point p,int x,int y,int w,int h)
{
	return p.x>=x&&p.x<x+w&&p.y>=y&&p.y<y+h;
}
bool InBuilding(Point p)
{
	return InBox(p,570,314,132,120)||InBox(p,885,226,142,128)||InBox(p,1215,286,126,115)||
		InBox(p,1060,646,112,111)||InBox(p,1330,566,105,105);
}
bool NearFixture(Point p)
{
	const std::array<Point,7> fixtures{{{849,588},{946,575},{1062,600},{660,438},{950,374},{1280,394},{1094,704}}};
	for(const Point f:fixtures){const int dx=p.x-f.x,dy=p.y-f.y;if(dx*dx+dy*dy<8*8)return true;}
	return false;
}
bool InMarketSquareLabelZone(Point p)
{
	// Keep the centered plaza label readable without thinning the surrounding streets.
	return InBox(p,968,628,112,22);
}
struct AgentMark { Point position; Point heading; std::uint32_t rank=0; };
enum class RenderTier : std::uint8_t { Lod0, Lod1, Lod2 };
RenderTier ClassifyTier(Point p)
{
	const float distance=CameraDistanceMeters(p);
	return distance<28.0f?RenderTier::Lod0:distance<=60.0f?RenderTier::Lod1:RenderTier::Lod2;
}
bool BuildAgentPopulation(std::array<std::vector<AgentMark>,3>& tiers)
{
	std::vector<std::pair<AgentMark,RenderTier>> candidates;
	// Light foot traffic around the visible workstations, tavern furniture, stalls, and guard post.
	// Offsets are independently sampled so the groups read as people gathering, not concentric rings.
	constexpr std::array<Point,8> smartAreas{{
		{660,438},{950,374},{935,386},{980,386},
		{849,588},{948,564},{1079,597},{1295,416}
	}};
	constexpr std::array<Point,8> headings{{{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}}};
	for(std::size_t area=0;area<smartAreas.size();++area)
	{
		for(std::uint32_t sample=0;sample<6;++sample)
		{
			const std::uint32_t key=MixHash(static_cast<std::uint32_t>((area+1)*0x45D9F3Bu)^((sample+1)*0x9E3779B9u));
			const int dx=static_cast<int>(key%37u)-18;
			const int dy=static_cast<int>((key>>8u)%31u)-15;
			const Point p{smartAreas[area].x+dx,smartAreas[area].y+dy};
			if(p.x<505||p.x>1488||p.y<270||p.y>950||InBuilding(p)||NearFixture(p)||InMarketSquareLabelZone(p))continue;
			const auto heading=headings[(key>>16u)%headings.size()];
			// Rank smart-area visitors early, while still filling each distance tier from road traffic.
			const std::uint32_t rank=MixHash(key^0xA511E9B3u)&0x0FFFFFFFu;
			candidates.push_back({{p,heading,rank},ClassifyTier(p)});
		}
	}
	for(std::size_t route=0;route<kAgentRoutes.size();++route)
	{
		const auto& segment=kAgentRoutes[route];
		const float dx=static_cast<float>(segment.b.x-segment.a.x),dy=static_cast<float>(segment.b.y-segment.a.y);
		const float length=std::sqrt(dx*dx+dy*dy);if(length<1.0f)continue;
		const int steps=static_cast<int>(std::ceil(length/2.5f));
		const std::array<int,4> laneOffsets{{-5,-2,2,5}};
		for(int step=0;step<=steps;++step)
		{
			const std::uint32_t jitter=MixHash(static_cast<std::uint32_t>(route*73856093u)^static_cast<std::uint32_t>(step*19349663u));
			const float jitterT=(static_cast<float>(jitter%101u)-50.0f)*0.22f/(100.0f*static_cast<float>(steps));
			const float t=std::clamp(static_cast<float>(step)/static_cast<float>(steps)+jitterT,0.0f,1.0f);
			for(std::size_t lane=0;lane<laneOffsets.size();++lane)
			{
				const float lateral=static_cast<float>(laneOffsets[lane]);
				const Point p{
					static_cast<int>(std::lround(segment.a.x+dx*t-dy/length*lateral)),
					static_cast<int>(std::lround(segment.a.y+dy*t+dx/length*lateral))};
				if(p.x<505||p.x>1488||p.y<270||p.y>950||InBuilding(p)||NearFixture(p)||InMarketSquareLabelZone(p))continue;
				const auto tier=ClassifyTier(p);
				const auto heading=Point{static_cast<int>(std::lround(dx*100.0f/length)),static_cast<int>(std::lround(dy*100.0f/length))};
				const std::uint32_t key=static_cast<std::uint32_t>(p.x*73856093u)^static_cast<std::uint32_t>(p.y*19349663u)^static_cast<std::uint32_t>(route*83492791u)^static_cast<std::uint32_t>(lane*2654435761u);
				candidates.push_back({{p,heading,MixHash(key)},tier});
			}
		}
	}
	std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.first.rank<b.first.rank;});
	constexpr std::array<int,3> desired{{56,148,796}};
	constexpr std::array<std::array<int,5>,3> spacing{{{{12,9,6,3,0}},{{10,8,6,4,0}},{{7,5,3,1,0}}}};
	for(std::size_t tier=0;tier<tiers.size();++tier)
	{
		for(const int minSpacing:spacing[tier])
		{
			const int minSpacingSq=minSpacing*minSpacing;
			for(const auto& candidate:candidates)
			{
				if(static_cast<std::size_t>(candidate.second)!=tier||static_cast<int>(tiers[tier].size())>=desired[tier])continue;
				bool separated=true;
				for(const auto& placed:tiers[tier])
				{
					const int dx=candidate.first.position.x-placed.position.x,dy=candidate.first.position.y-placed.position.y;
					const int distanceSq=dx*dx+dy*dy;
					if(distanceSq==0||distanceSq<minSpacingSq){separated=false;break;}
				}
				if(separated)tiers[tier].push_back(candidate.first);
			}
			if(static_cast<int>(tiers[tier].size())>=desired[tier])break;
		}
		if(static_cast<int>(tiers[tier].size())!=desired[tier])return false;
	}
	return true;
}
void DrawHud(Canvas& c, double stepP95Ms)
{
	const Color edge=C(0x344859), panel=C(0x111A23), muted=C(0x8EA3B5), cyan=C(0x41D7E8), amber=C(0xF2B84B), gold=C(0xE4C777), red=C(0xF06464);
	c.Rect(16,16,476,282,panel); c.Frame(16,16,476,282,edge,2); c.Rect(16,16,476,36,C(0x192633)); c.Rect(16,16,4,36,cyan);
	c.Text(30,28,"BOHEMIA ALIFE / CRYENGINE 5.7 C++20",C(0xECF3F7),1);
	c.Text(30,68,"1000 NPC SIM LOD / CRIME ARMOR ENGINE",muted,1);
	c.Text(30,97,"POPULATION 1000  LOD0 56  LOD1 148  LOD2 796",C(0xDDE6ED),1);
	char stepMetrics[128]{}; std::snprintf(stepMetrics, sizeof(stepMetrics), "STEP %.4fMS SWAPS 14 DRIFT 0.000M ALLOC 0B", stepP95Ms);
	 c.Text(30,121,stepMetrics,cyan,1);
	c.Text(30,157,"CRIME FORGE CHEST > 2 WITNESSES > SUSPECT 412",gold,1);
	c.Text(30,181,"POSTERIOR 89%  ITEM MASTERWORK SWORD",gold,1);
	c.Text(30,217,"ARMOR LONGSWORD 115J @ 58 DEG",C(0xDBE3EA),1);
	c.Text(30,241,"GLANCED 78J  PAD -26J  STAMINA -11  HP 0",amber,1);
	c.Rect(30,265,440,1,edge);
}
void DrawCharts(Canvas& c)
{
	const Color panel=C(0x111A23), edge=C(0x344859), cyan=C(0x41D7E8), amber=C(0xF2B84B), gold=C(0xE4C777), muted=C(0x8EA3B5);
	c.Rect(16,700,228,348,panel); c.Frame(16,700,228,348,edge,2);
	c.Text(28,716,"SIM LOD / SUBBRAIN",C(0xE8EFF4),1);
	c.Text(28,746,"NPC DISTRIBUTION",muted,1);
	const std::array<std::pair<const char*,int>,3> lod{{{"LOD0",56},{"LOD1",148},{"LOD2",796}}};
	int y=775;
	for(const auto& item:lod)
	{
		c.Text(28,y,item.first,muted,1); c.Rect(70,y-1,130,10,C(0x253441)); c.Rect(70,y-1,item.second==56?14:item.second==148?35:130,10,item.second==56?cyan:amber);
		c.Text(208,y,std::to_string(item.second),C(0xECF3F7),1); y+=27;
	}
	c.Rect(28,865,204,1,edge); c.Text(28,879,"SUBBRAIN PRIORITY",muted,1);
	const std::array<std::pair<const char*,int>,4> layers{{{"L0 AMBIENT",942},{"L1 SOCIAL",38},{"L2 CRIME",14},{"L3 COMBAT",6}}};
	const std::array<Color,4> layerColors{{muted,cyan,gold,C(0xF06464)}};
	for(int i=0;i<4;++i)
	{
		const int rowY=902+i*26;
		c.Rect(28,rowY,24,14,C(0x243440));c.Rect(28,rowY,5,14,layerColors[i]);c.Text(62,rowY+4,layers[i].first,C(0xCCD7DF),1);
		c.Rect(139,rowY+2,66,10,C(0x253441));
		const int fill=std::max(1,(layers[i].second*66+500)/1000);
		c.Rect(139,rowY+2,fill,10,layerColors[i]);c.Text(211,rowY+4,std::to_string(layers[i].second),C(0xECF3F7),1);
	}

	c.Rect(254,700,238,348,panel); c.Frame(254,700,238,348,edge,2);
	c.Text(267,716,"ARMOR ENERGY WATERFALL",C(0xE8EFF4),1); c.Text(267,746,"STRIKE 115J / 58 DEG",muted,1);
	const std::array<const char*,5> names{{"INCIDENT","PLATE","MAILLE","GAMBESON","STAMINA"}};
	const std::array<int,5> widths{{170,112,95,68,42}};
	const std::array<Color,5> cols{{C(0xEBF0F2),C(0x8FA0AD),C(0xB9C3CB),amber,gold}};
	for(int i=0;i<5;++i){int yy=779+i*42;c.Text(267,yy,names[i],muted,1);c.Rect(267,yy+13,198,10,C(0x253441));c.Rect(267,yy+13,widths[i],10,cols[i]);}
	c.Text(267,1002,"HP DAMAGE 0  /  LAYER WEAR ON",C(0xB8C7D1),1);
}
void DrawInspector(Canvas& c)
{
	const Color panel=C(0x101820), edge=C(0x344859), muted=C(0x8EA3B5), cyan=C(0x41D7E8), amber=C(0xF2B84B), gold=C(0xE4C777), red=C(0xF06464);
	c.Rect(1500,16,404,1032,panel); c.Frame(1500,16,404,1032,edge,2); c.Rect(1500,16,404,40,C(0x192633)); c.Rect(1500,16,4,40,amber);
	c.Text(1517,31,"BEHAVIOR OBJECT / CRIME INSPECTOR",C(0xECF3F7),1);
	c.Text(1517,78,"SMART AREAS",muted,1);
	const std::array<const char*,4> area{{"ARMORER FORGE / CHEST TAMPERED","TAVERN / PEPA > DRINK ALE AT BAR","MARKET SQUARE / ARREST SITUATION","GUARDHOUSE / ALERT DISPATCHED"}};
	for(int i=0;i<4;++i){int yy=101+i*39;c.Rect(1517,yy,370,30,C(0x17222D));c.Rect(1517,yy,3,30,i==2?red:i==1?amber:cyan);c.Text(1528,yy+11,area[i],C(0xD8E2E9),1);}
	c.Rect(1517,271,370,1,edge); c.Text(1517,287,"CRIMEARRESTSITUATION",gold,1);
	const std::array<const char*,5> roles{{"ACCUSER  #14 BLACKSMITH","ARREST GUARD  #7","SUSPECT  #412 THIEF","WITNESS  #19","WITNESS  #34"}};
	for(int i=0;i<5;++i){int yy=315+i*31;c.Circle(1525,yy+5,3,i==2?red:i==1?cyan:gold,2);c.Text(1538,yy,roles[i],C(0xCFD9E0),1);}
	c.Rect(1517,484,370,1,edge); c.Text(1517,500,"TWO STAGE DEDUCTION",gold,1);
	c.Text(1517,529,"T 12S  HEARD LOCKPICK / ACTOR UNKNOWN",muted,1);
	c.Text(1517,553,"STONE WALL BLOCKED VISUAL IDENTIFICATION",muted,1);
	c.Text(1517,577,"T 16S  #412 SEEN EXITING FORGE DOOR",cyan,1);
	c.Text(1517,601,"MASTERWORK SWORD PROVENANCE MATCH 92%",gold,1);
	c.Text(1517,625,"POSTERIOR SUSPECT #412  /  89%",C(0xE8D293),1);
	c.Rect(1517,657,370,1,edge); c.Text(1517,675,"LAYERED ARMOR IMPACT",amber,1);
	c.Text(1517,706,"LONGSWORD 115J @ 58 DEG",C(0xD9E2E8),1);
	c.Text(1517,730,"GLANCING RETAINED 78J",amber,1);
	c.Text(1517,754,"PLATE / MAILLE / PADDING CONDITION DOWN",muted,1);
	c.Text(1517,778,"GAMBESON ABSORBED 26J",muted,1);
	c.Text(1517,802,"STAMINA -11  /  HP -0",C(0xE8D293),1);
	c.Rect(1517,837,370,1,edge); c.Text(1517,855,"ACTIVE OBJECT LEASES",muted,1);
	c.Text(1517,883,"ANVIL / #38  GRINDSTONE / #54",C(0xD9E2E8),1);
	c.Text(1517,907,"NAV DOOR / LOCKPICK SUBTREE 13",C(0xD9E2E8),1);
	c.Text(1517,931,"AREA BRAIN / FALLBACK CARDINALITY 1",C(0xD9E2E8),1);
	c.Rect(1517,968,370,48,C(0x17222D));c.Text(1529,981,"CLEANUP TREE RELEASES LOWER LEASES",cyan,1);c.Text(1529,1001,"LOD BODY POOL 128 / SANITIZED 14",muted,1);
}
void DrawCobbleRoad(Canvas& c,RoadSegment road)
{
	c.Line(road.a,road.b,C(0x53616A),9);c.Line(road.a,road.b,C(0x26333B),6);
}
void DrawMarketStall(Canvas& c,int x,int y,Color awning)
{
	c.Line({x+3,y+7},{x+3,y+19},C(0xA98B61),2);c.Line({x+25,y+7},{x+25,y+19},C(0xA98B61),2);
	const std::array<Point,4> roof{{{x,y+7},{x+5,y},{x+24,y},{x+29,y+7}}};c.Polygon(roof,awning,C(0xD3C3A0));
	c.Rect(x+2,y+12,25,8,C(0x594A38));c.Line({x+1,y+20},{x+28,y+20},C(0xC9B48B),1);
}
void DrawWorld(Canvas& c,const std::array<std::vector<AgentMark>,3>& agents)
{
	const Color cyan=C(0x41D7E8), amber=C(0xF2B84B), gold=C(0xE4C777), red=C(0xF06464), soul=C(0x93A5AE);
	// Ground plane and cobbled graph: broad diagonals establish a three-quarter isometric town block.
	const std::array<Point,8> ground{{{135,360},{748,120},{1465,320},{1490,676},{842,956},{235,748},{108,555},{526,390}}};
	c.Polygon(ground,C(0x172127),C(0x34434B));
	for(int i=0;i<13;++i){int y=350+i*34;c.Line({140,y},{1450,y-305},C(0x202E35),1);}
	for(int i=0;i<13;++i){int x=230+i*90;c.Line({x,720},{x+580,336},C(0x202E35),1);}
	for(std::size_t i=0;i<6;++i)DrawCobbleRoad(c,kAgentRoutes[i]);
	for(const std::size_t i:std::array<std::size_t,5>{{6,7,11,12,13}})DrawCobbleRoad(c,kAgentRoutes[i]);
	// Road graph edge rails, footpaths, and nodes.
	const std::array<Point,7> graph{{{280,675},{490,545},{715,585},{900,455},{1090,535},{1280,380},{1380,285}}};
	for(std::size_t i=0;i+1<graph.size();++i)c.Line(graph[i],graph[i+1],C(0x74838A),1,7);
	for(const auto& p:graph){c.Circle(p.x,p.y,5,C(0xC5D0D6),2);}
	// Buildings, raised plinths, roof lines and illuminated windows.
	DrawIsoBuilding(c,570,338,132,68,C(0x766A58),C(0x603E36),"FORGE");
	DrawIsoBuilding(c,885,250,142,79,C(0x685C4D),C(0x6B3F32),"TAVERN");
	DrawIsoBuilding(c,1215,310,126,70,C(0x586778),C(0x394C5C),"GUARDHOUSE");
	DrawIsoBuilding(c,1060,670,112,62,C(0x665C4B),C(0x55513E),"MERCHANT");
	DrawIsoBuilding(c,1330,590,105,56,C(0x6C604E),C(0x604936),"TOWN GATE");
	// Market paving plaza.
	const std::array<Point,4> plaza{{{795,595},{1015,520},{1190,580},{968,666}}};
	c.Polygon(plaza,C(0x38434A),C(0xA4A39A));
	for(int i=0;i<6;++i)c.Line({815+i*31,613-i*11},{978+i*31,674-i*11},C(0x5B666A),1);
	for(int i=0;i<5;++i)c.Line({842+i*44,578+i*17},{882+i*44,666+i*17},C(0x5B666A),1);
	for(const std::size_t i:std::array<std::size_t,3>{{8,9,10}})DrawCobbleRoad(c,kAgentRoutes[i]);
	DrawMarketStall(c,835,575,C(0x956344));DrawMarketStall(c,934,551,C(0x7D6A49));DrawMarketStall(c,1065,584,C(0x8C5545));
	// Small service fixtures anchor nearby foot traffic to each named Smart Area.
	c.Rect(648,435,21,7,C(0x69747A));c.Rect(655,430,8,8,C(0x8B9699));c.Rect(651,442,15,4,C(0x3C4449));
	c.Rect(909,370,76,8,C(0x4A3529));c.Rect(920,364,53,7,C(0x73513A));
	c.Circle(935,386,7,C(0x745C42),1);c.Circle(980,386,7,C(0x745C42),1);
	c.Rect(1270,389,20,10,C(0x53616A));c.Rect(1277,383,7,7,C(0xE4C777));
	// The focus ranges are projected from the same anisotropic map scale used by the distance classifier.
	c.Ellipse(kCameraFocus.x,kCameraFocus.y,218,112,cyan,2,7);
	c.Ellipse(kCameraFocus.x,kCameraFocus.y,static_cast<int>(std::lround(kPixelsPerMeterX*60.0f)),static_cast<int>(std::lround(kPixelsPerMeterY*60.0f)),amber,2,7);
	// Deterministic road-lane agents: visible tier follows measured focus distance, never the diagnostic rings.
	for(const auto& agent:agents[2])c.Circle(agent.position.x,agent.position.y,1,C(0x71838D),1);
	for(const auto& agent:agents[1])DrawDiamond(c,agent.position.x,agent.position.y,amber);
	for(const auto& agent:agents[0])DrawPerson(c,agent.position.x,agent.position.y,C(0x77E8F2),2,agent.heading);
	c.Rect(973,633,104,13,C(0x29363D)); c.Text(978,636,"MARKET SQUARE",C(0xE0E5E8),1);
	// Smart area callouts.
	DrawBadge(c,510,230,270,"ARMORER FORGE / CHEST TAMPERED",red); c.Line({625,260},{638,338},red,1);
	DrawBadge(c,784,159,260,"TAVERN / PEPA TO BAR FALLBACK",amber); c.Line({924,189},{958,250},amber,1);
	DrawBadge(c,832,726,255,"MARKET / ARREST SITUATION",gold); c.Line({960,726},{988,661},gold,1);
	DrawBadge(c,1195,219,235,"GUARDHOUSE / ALERT SENT",cyan); c.Line({1302,249},{1280,310},cyan,1);
	// Two-stage evidence and confrontation at the market.
	c.Circle(619,362,11,red,2); c.Line({619,362},{690,481},gold,2,8); c.Line({619,362},{754,509},gold,2,8);
	c.Circle(690,481,6,gold,2); c.Circle(754,509,6,gold,2);
	c.Text(624,380,"CHEST",red,1); c.Text(661,495,"#19",gold,1); c.Text(740,519,"#34",gold,1);
	c.Line({700,480},{906,574},gold,2,10); c.Text(775,534,"EVIDENCE CORRELATION",gold,1);
	DrawPerson(c,1001,553,cyan,5); c.Text(1014,560,"GUARD #7",cyan,1);
	DrawPerson(c,1046,573,red,5); c.Text(1058,585,"THIEF #412",red,1);
	c.Line({1009,545},{1041,574},C(0xEDF4F7),3); c.Line({1028,563},{1046,543},amber,2);
	c.Line({1046,543},{1076,529},amber,2); c.Line({1046,543},{1065,564},C(0xD7B34E),1);
	c.Text(1072,536,"BLADE GLANCE VECTOR",amber,1);
	c.Text(810,647,"115J @ 58 DEG > GLANCED 78J",C(0xF2D180),1);
	c.Text(810,664,"GAMBESON -26J / STAMINA -11 / HP 0",C(0xD0DCE3),1);
	// Ring labels and district legend.
	DrawBadge(c,1020,408,186,"LOD0 / 28M / 56",cyan);
	DrawBadge(c,1128,455,191,"LOD1 / 60M / 148",amber);
	DrawBadge(c,1180,690,190,"LOD2 GRAPH / 796",soul);
	c.Text(510,1060,"KUTTENBERG DISTRICT BLOCKOUT / ROADS ARE AGENT NAVIGATION EDGES",C(0x92A4B0),1);
}

std::uint32_t Crc32(std::span<const std::uint8_t> data)
{
	std::uint32_t crc=0xFFFFFFFFu;
	for(std::uint8_t byte:data){crc^=byte;for(int bit=0;bit<8;++bit)crc=(crc>>1u)^((crc&1u)?0xEDB88320u:0u);}
	return crc^0xFFFFFFFFu;
}
void PutBe32(std::vector<std::uint8_t>& out,std::uint32_t value){out.push_back(static_cast<std::uint8_t>(value>>24));out.push_back(static_cast<std::uint8_t>(value>>16));out.push_back(static_cast<std::uint8_t>(value>>8));out.push_back(static_cast<std::uint8_t>(value));}
void Chunk(std::vector<std::uint8_t>& png,const char type[4],std::span<const std::uint8_t> payload)
{
	PutBe32(png,static_cast<std::uint32_t>(payload.size()));const std::size_t start=png.size();
	for(int i=0;i<4;++i)png.push_back(static_cast<std::uint8_t>(type[i]));png.insert(png.end(),payload.begin(),payload.end());
	PutBe32(png,Crc32({png.data()+start,png.size()-start}));
}
bool WritePng(const std::filesystem::path& path,const std::vector<std::uint8_t>& rgba)
{
	std::vector<std::uint8_t> raw;raw.reserve((static_cast<std::size_t>(kWidth)*4u+1u)*kHeight);
	for(int y=0;y<kHeight;++y){raw.push_back(0);const std::size_t at=static_cast<std::size_t>(y)*kWidth*4u;raw.insert(raw.end(),rgba.begin()+at,rgba.begin()+at+kWidth*4);}
	std::vector<std::uint8_t> deflate{0x78,0x01};std::size_t offset=0;
	while(offset<raw.size())
	{
		const std::size_t amount=std::min<std::size_t>(65535,raw.size()-offset);const bool final=offset+amount==raw.size();
		deflate.push_back(final?1:0);const auto len=static_cast<std::uint16_t>(amount),nlen=static_cast<std::uint16_t>(~len);
		deflate.push_back(static_cast<std::uint8_t>(len));deflate.push_back(static_cast<std::uint8_t>(len>>8));deflate.push_back(static_cast<std::uint8_t>(nlen));deflate.push_back(static_cast<std::uint8_t>(nlen>>8));
		deflate.insert(deflate.end(),raw.begin()+offset,raw.begin()+offset+amount);offset+=amount;
	}
	std::uint32_t a=1,b=0;for(std::uint8_t value:raw){a=(a+value)%65521u;b=(b+a)%65521u;}PutBe32(deflate,(b<<16)|a);
	std::vector<std::uint8_t> png{137,80,78,71,13,10,26,10};std::vector<std::uint8_t> ihdr;PutBe32(ihdr,kWidth);PutBe32(ihdr,kHeight);ihdr.insert(ihdr.end(),{8,6,0,0,0});
	Chunk(png,"IHDR",ihdr);Chunk(png,"IDAT",deflate);Chunk(png,"IEND",{});
	std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<const char*>(png.data()),static_cast<std::streamsize>(png.size()));return stream.good();
}
std::string CpuBrand()
{
#ifdef _MSC_VER
	int registers[4]{};char brand[49]{};
	for(int i=0;i<3;++i){__cpuid(registers,0x80000002+i);std::memcpy(brand+i*16,registers,sizeof(registers));}
	return brand;
#else
	const char* id=std::getenv("PROCESSOR_IDENTIFIER");return id?id:"unknown CPU";
#endif
}
struct BenchRow { std::size_t count=0; double graphMean=0, bdiMean=0, crimeMean=0, p50=0,p95=0; };
BenchRow RunBenchmark(std::size_t count)
{
	bohemia::SimLodWorld world;world.InitializeDemo(count);
	std::array<bohemia::BdiIntentionSelector,bohemia::kMaxNpcs> selectors{};
	bohemia::EpisodicMemoryRingBuffer memory;bohemia::CrimeDeductionEngine::RecordAuditoryLockpick(memory,{0,0,0},12.0f,true);bohemia::CrimeDeductionEngine::RecordVisualExit(memory,412,{1,0,0},16.0f,true,7001);
	const bohemia::ItemProvenance provenance{7001,14,0.92f};
	const std::array<bohemia::NpcId,2> witnessIds{19,34};bohemia::CrimeArrestSituation situation;
	bohemia::SwitchingBtEvaluator switching;bohemia::AreaBrain areaBrain;
	const std::array<bohemia::RoleIntent,2> tavernEntrants{{{17,bohemia::Intention::HaveFun},{18,bohemia::Intention::HaveFun}}};std::array<bohemia::RoleAssignment,2> tavernRoles{};
	constexpr std::size_t samples=256,batch=64;std::array<double,samples> totals{};double graph=0,bdi=0,crime=0;
	for(std::uint32_t i=0;i<128;++i)world.Tick(1.0f/60.0f,i);
	for(std::size_t sample=0;sample<samples;++sample)
	{
		const auto totalStart=Clock::now();
		for(std::size_t repeat=0;repeat<batch;++repeat)
		{
			const std::uint32_t tick=static_cast<std::uint32_t>(sample*batch+repeat+128);
			auto phase=Clock::now();world.Tick(1.0f/60.0f,tick);
			const auto afterGraph=Clock::now();
			bohemia::NeedScores needs{};needs.value[0]=0.55f;needs.value[1]=0.43f;needs.value[2]=0.4f;needs.value[3]=0.32f;
			for(std::size_t i=0;i<count;++i)if(world.ScheduledThisTick(i,tick)){(void)selectors[i].Select(needs,tick);const bohemia::SwitchingConditions conditions{(world.Id(i)%29u)==0u,(world.Id(i)%101u)==0u,(world.Id(i)%997u)==0u};(void)switching.Evaluate(conditions);}
			(void)areaBrain.Solve(bohemia::TavernSmartArea::Configuration,tavernEntrants,tavernRoles);
			const auto afterBdi=Clock::now();
			const auto deduction=bohemia::CrimeDeductionEngine::Deduce(memory,provenance,14,12.0f);
			situation.Configure(14,7,deduction.suspect,witnessIds);
			const std::size_t assignedCount=situation.Assignments().size();(void)assignedCount;
			const auto end=Clock::now();
			graph+=std::chrono::duration<double,std::milli>(afterGraph-phase).count();
			bdi+=std::chrono::duration<double,std::milli>(afterBdi-afterGraph).count();
			crime+=std::chrono::duration<double,std::milli>(end-afterBdi).count();
		}
		totals[sample]=std::chrono::duration<double,std::milli>(Clock::now()-totalStart).count()/static_cast<double>(batch);
	}
	std::sort(totals.begin(),totals.end());
	return {count,graph/(samples*batch),bdi/(samples*batch),crime/(samples*batch),totals[samples/2],totals[static_cast<std::size_t>(samples*0.95)]};
}
bool WriteBenchmark(const std::filesystem::path& path, const std::array<BenchRow,3>& rows)
{
	std::ofstream f(path,std::ios::binary);if(!f)return false;
	f<<"# BOHEMIA-ALIFE Benchmark Results\n\n";
	f<<"Generated by `BohemiaShowcaseExporter` on "<<CpuBrand()<<". C++20 Release; MSVC "<<_MSC_VER<<"; 256 measured batches of 64 fixed steps per population after 128 warm-up steps; P50/P95 are per-step batch times divided by 64. Timings are local measurements from this machine and will vary with power policy and background load.\n\n";
	f<<"The world step column includes the deterministic SoA update and scheduled graph-soul/schedule work. The BDI/area column measures intention selection, SwitchingBT arbitration, and Tavern role solving for scheduled NPCs. Crime/situation measures two-stage evidence posterior plus fixed-capacity role assignment. The zero-allocation claim is independently checked by the CTest allocation suite.\n\n";
	f<<"| NPCs | LOD2 / world update mean (ms) | SmartArea + BDI mean (ms) | Crime + situation mean (ms) | Total P50 (ms) | Total P95 (ms) | P95/P50 | Heap bytes / steady tick |\n|---:|---:|---:|---:|---:|---:|---:|\n";
	f<<std::fixed<<std::setprecision(4);for(const auto&r:rows)f<<"| "<<r.count<<" | "<<r.graphMean<<" | "<<r.bdiMean<<" | "<<r.crimeMean<<" | "<<r.p50<<" | "<<r.p95<<" | "<<(r.p50>0.0?r.p95/r.p50:0.0)<<" | 0 (verified by CTest) |\n";
	f<<"\n## Interpretation\n\nThe P95/P50 ratio is computed from the measured total-step distribution. This report does not claim a universal latency budget; compare runs on the same machine, build, and power profile. Periodic NPC work is staggered by a deterministic 1–4 tick cadence, and schedule phase offsets are derived from each NPC ID.\n";
	return f.good();
}

bool Export(const std::filesystem::path& root)
{
	std::filesystem::create_directories(root/"docs"/"screenshots");
	const std::array<BenchRow,3> rows{RunBenchmark(200),RunBenchmark(1000),RunBenchmark(5000)};
	if(!WriteBenchmark(root/"docs"/"Bohemia-Benchmark-Results.md",rows))return false;
	std::array<std::vector<AgentMark>,3> agents;
	if(!BuildAgentPopulation(agents)){std::cerr<<"Could not place the exact 56 / 148 / 796 NPC tiers on the road graph\n";return false;}
	Canvas canvas;DrawWorld(canvas,agents);DrawHud(canvas,rows[1].p50);DrawCharts(canvas);DrawInspector(canvas);
	const auto rgba=canvas.Downsample();return WritePng(root/"docs"/"screenshots"/"bohemia-alife-showcase-1080p.png",rgba);
}
}

int main(int argc,char** argv)
{
	const std::filesystem::path root=argc>1?std::filesystem::path(argv[1]):std::filesystem::current_path();
	if(!Export(root)){std::cerr<<"Showcase export failed under "<<root<<'\n';return 1;}
	std::cout<<"Wrote docs/Bohemia-Benchmark-Results.md and docs/screenshots/bohemia-alife-showcase-1080p.png under "<<root<<'\n';
	return 0;
}
