#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

#include "core/ArmorBallistics.h"
#include "core/BehaviorObjects.h"
#include "core/SimLodAndCrime.h"
#include "core/TownRouteGraph.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr int kNpcCount = 1000;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDaySeconds = 86400.0f;
constexpr int kSideWidth = 278;

using Vec2 = bohemia::town::Point;
struct IPoint { int x = 0, y = 0; };
struct ScreenRect { int left = 0, top = 0, right = 0, bottom = 0; };
using Route = bohemia::town::Route;
constexpr const auto& kRoutes = bohemia::town::kRoutes;
struct Npc
{
	float routeDistance = 0.0f;
	float speed = 1.0f;
	Vec2 position{};
	std::uint8_t route = 0;
};

std::uint32_t Hash(std::uint32_t value) noexcept
{
	value ^= value >> 16; value *= 0x7FEB352Du; value ^= value >> 15; value *= 0x846CA68Bu; value ^= value >> 16;
	return value;
}
std::uint32_t Pack(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept
{
	return (static_cast<std::uint32_t>(r) << 16u) | (static_cast<std::uint32_t>(g) << 8u) | b;
}
COLORREF TextColor(std::uint32_t rgb) noexcept
{
	return RGB((rgb >> 16u) & 255u, (rgb >> 8u) & 255u, rgb & 255u);
}
float RouteLength(const Route& route) noexcept
{
	return bohemia::town::Length(route);
}
Vec2 RoutePosition(const Route& route, float distance) noexcept
{
	return bohemia::town::PositionAtDistance(route, distance);
}

class LiveSimulator
{
public:
	bool Create(HINSTANCE instance)
	{
		m_instance = instance;
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = L"BohemiaAlifeLiveSimWindow";
		if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
		m_world.InitializeDemo(kNpcCount);
		for (std::size_t i = 0; i < m_npcs.size(); ++i)
		{
			const std::uint32_t seed = Hash(static_cast<std::uint32_t>(i + 1u));
			Npc& npc = m_npcs[i];
			npc.route = static_cast<std::uint8_t>(seed % kRoutes.size());
			const Route& route = kRoutes[npc.route];
			const float length = RouteLength(route);
			npc.routeDistance = length * static_cast<float>(Hash(seed ^ 0xA511E9B3u) % 10000u) / 10000.0f;
			npc.speed = 0.75f + static_cast<float>(Hash(seed ^ 0x9E3779B9u) % 1000u) / 500.0f;
			npc.position = RoutePosition(route, npc.routeDistance);
		}
		m_pepaPosition = m_npcs[18].position;
		m_pepaTarget = {109.0f, 32.0f};
		m_window = CreateWindowExW(0, wc.lpszClassName, L"BOHEMIA-ALIFE | Live 60 FPS Simulator", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 1500, 960, nullptr, nullptr, instance, this);
		if (m_window == nullptr) return false;
		ShowWindow(m_window, SW_SHOW);
		UpdateWindow(m_window);
		return true;
	}

	int Run()
	{
		timeBeginPeriod(1);
		using Clock = std::chrono::steady_clock;
		auto previous = Clock::now();
		auto nextFrame = previous;
		MSG message{};
		while (m_running)
		{
			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
			{
				if (message.message == WM_QUIT) { m_running = false; break; }
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
			if (!m_running) break;
			const auto now = Clock::now();
			const float dt = std::clamp(std::chrono::duration<float>(now - previous).count(), 0.0f, 0.05f);
			previous = now;
			Update(dt);
			Render();
			if (m_window != nullptr && IsWindowVisible(m_window) && m_pixels != nullptr)
			{
				HDC dc = GetDC(m_window);
				if (dc != nullptr)
				{
					StretchDIBits(dc, 0, 0, m_width, m_height, 0, 0, m_width, m_height, m_pixels, &m_bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
					ReleaseDC(m_window, dc);
				}
			}
			nextFrame += std::chrono::microseconds(16667);
			const auto afterRender = Clock::now();
			if (nextFrame > afterRender)
				std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::milliseconds>(nextFrame - afterRender));
			else if (afterRender - nextFrame > std::chrono::milliseconds(100))
				nextFrame = afterRender;
		}
		timeEndPeriod(1);
		ReleaseBackbuffer();
		return static_cast<int>(message.wParam);
	}

private:
	static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		LiveSimulator* app = reinterpret_cast<LiveSimulator*>(GetWindowLongPtrW(window, GWLP_USERDATA));
		if (message == WM_NCCREATE)
		{
			const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
			app = static_cast<LiveSimulator*>(create->lpCreateParams);
			SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
			app->m_window = window;
		}
		if (app == nullptr) return DefWindowProcW(window, message, wParam, lParam);
		switch (message)
		{
		case WM_SIZE: app->Resize(LOWORD(lParam), HIWORD(lParam)); return 0;
		case WM_ERASEBKGND: return 1;
		case WM_PAINT:
		{
			PAINTSTRUCT ps{}; HDC dc = BeginPaint(window, &ps);
			if (app->m_pixels != nullptr) StretchDIBits(dc, 0, 0, app->m_width, app->m_height, 0, 0, app->m_width, app->m_height, app->m_pixels, &app->m_bitmapInfo, DIB_RGB_COLORS, SRCCOPY);
			EndPaint(window, &ps); return 0;
		}
		case WM_KEYDOWN:
			if ((lParam & (1LL << 30)) == 0) app->KeyDown(static_cast<int>(wParam));
			return 0;
		case WM_LBUTTONDOWN: app->MouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
		case WM_LBUTTONUP: app->MouseUp(); return 0;
		case WM_MOUSEMOVE: app->MouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
		case WM_CAPTURECHANGED: app->m_dragging = false; return 0;
		case WM_CLOSE: DestroyWindow(window); return 0;
		case WM_DESTROY: app->m_running = false; PostQuitMessage(0); return 0;
		default: return DefWindowProcW(window, message, wParam, lParam);
		}
	}

	void Resize(int width, int height)
	{
		if (width < 1 || height < 1) return;
		m_width = width; m_height = height;
		ReleaseBackbuffer();
		HDC screen = GetDC(m_window);
		m_memoryDc = CreateCompatibleDC(screen);
		m_bitmapInfo = {};
		m_bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		m_bitmapInfo.bmiHeader.biWidth = width;
		m_bitmapInfo.bmiHeader.biHeight = -height;
		m_bitmapInfo.bmiHeader.biPlanes = 1;
		m_bitmapInfo.bmiHeader.biBitCount = 32;
		m_bitmapInfo.bmiHeader.biCompression = BI_RGB;
		m_bitmap = CreateDIBSection(screen, &m_bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_pixels), nullptr, 0);
		ReleaseDC(m_window, screen);
		if (m_bitmap != nullptr && m_memoryDc != nullptr)
		{
			m_oldBitmap = static_cast<HBITMAP>(SelectObject(m_memoryDc, m_bitmap));
			SetBkMode(m_memoryDc, TRANSPARENT);
			m_font = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
			if (m_font != nullptr) m_oldFont = static_cast<HFONT>(SelectObject(m_memoryDc, m_font));
		}
	}

	void ReleaseBackbuffer()
	{
		if (m_memoryDc != nullptr && m_oldFont != nullptr) SelectObject(m_memoryDc, m_oldFont);
		if (m_font != nullptr) DeleteObject(m_font);
		if (m_memoryDc != nullptr && m_oldBitmap != nullptr) SelectObject(m_memoryDc, m_oldBitmap);
		if (m_bitmap != nullptr) DeleteObject(m_bitmap);
		if (m_memoryDc != nullptr) DeleteDC(m_memoryDc);
		m_memoryDc = nullptr; m_bitmap = nullptr; m_font = nullptr; m_oldBitmap = nullptr; m_oldFont = nullptr; m_pixels = nullptr;
	}

	void KeyDown(int key)
	{
		switch (key)
		{
		case VK_ESCAPE: SendMessageW(m_window, WM_CLOSE, 0, 0); break;
		case VK_SPACE: m_paused = !m_paused; break;
		case '1': TriggerCrime(); break;
		case '2': ToggleFranta(); break;
		case '3': StrikeArmor(); break;
		case VK_OEM_PLUS: case VK_ADD: SetSpeed(1); break;
		case VK_OEM_MINUS: case VK_SUBTRACT: SetSpeed(-1); break;
		default: break;
		}
	}

	void MouseDown(int x, int y)
	{
		SetCapture(m_window);
		for (int index = 0; index < 3; ++index)
		{
			const ScreenRect button = ButtonRect(index);
			if (x >= button.left && x <= button.right && y >= button.top && y <= button.bottom)
			{
				if (index == 0) TriggerCrime(); else if (index == 1) ToggleFranta(); else StrikeArmor();
				return;
			}
		}
		m_dragging = true; m_lastMouse = {x, y};
	}

	void MouseUp()
	{
		m_dragging = false;
		if (GetCapture() == m_window) ReleaseCapture();
	}

	void MouseMove(int x, int y)
	{
		if (!m_dragging) return;
		const float scale = MapScale();
		if (scale > 0.0f)
		{
			m_focus.x -= static_cast<float>(x - m_lastMouse.x) / scale;
			m_focus.y -= static_cast<float>(y - m_lastMouse.y) / scale;
			ClampFocus();
		}
		m_lastMouse = {x, y};
	}

	void SetSpeed(int direction)
	{
		constexpr std::array<int,3> speeds{{1,2,5}};
		int index = 0;
		for (int i = 0; i < static_cast<int>(speeds.size()); ++i) if (speeds[static_cast<std::size_t>(i)] == m_speedMultiplier) index = i;
		index = std::clamp(index + direction, 0, static_cast<int>(speeds.size()) - 1);
		m_speedMultiplier = speeds[static_cast<std::size_t>(index)];
	}

	void TriggerCrime()
	{
		m_crimeActive = true; m_crimeElapsed = 0.0f; m_heardRecorded = false; m_exitRecorded = false;
		m_deduction = {}; m_memory = bohemia::EpisodicMemoryRingBuffer{};
	}

	void ToggleFranta()
	{
		m_frantaPresent = !m_frantaPresent;
		m_pepaTarget = m_frantaPresent ? Vec2{109.0f,32.0f} : Vec2{116.0f,36.0f};
		std::array<bohemia::RoleIntent,2> intents{{{19,bohemia::Intention::HaveFun},{24,bohemia::Intention::HaveFun}}};
		std::array<bohemia::RoleAssignment,2> assignments{};
		const std::span<const bohemia::RoleIntent> present(intents.data(), m_frantaPresent ? 2u : 1u);
		m_tavernAssignmentCount = static_cast<int>(bohemia::AreaBrain{}.Solve(bohemia::TavernSmartArea::Configuration, present, assignments));
		m_pepaRole = m_frantaPresent ? "DICE TABLE (N=2 >= 2)" : "BAR FALLBACK (N=1 < 2)";
	}

	void StrikeArmor()
	{
		const int weapon = static_cast<int>(m_random() % 3u);
		const float angle = 15.0f + static_cast<float>(m_random() % 5500u) / 100.0f;
		bohemia::StrikeInput strike{};
		strike.energyJoules = weapon == 0 ? 115.0f : weapon == 1 ? 92.0f : 145.0f;
		strike.platePerforationJoules = weapon == 1 ? 152.0f : weapon == 2 ? 125.0f : 185.0f;
		const float radians = angle * kPi / 180.0f;
		strike.direction = {std::sin(radians), 0.0f, std::cos(radians)};
		strike.plateNormal = {0.0f, 0.0f, 1.0f};
		m_armorImpact = bohemia::ArmorBallistics::ResolveStrike(m_armor, strike);
		m_weaponName = weapon == 0 ? "LONGSWORD" : weapon == 1 ? "ESTOC" : "MACE";
		m_strikeAngle = angle; m_hasArmorImpact = true;
	}

	void ClampFocus()
	{
		m_focus.x = std::clamp(m_focus.x, 0.0f, 200.0f);
		m_focus.y = std::clamp(m_focus.y, 0.0f, 130.0f);
	}

	float UiScale() const noexcept
	{
		return std::clamp(static_cast<float>(m_height - 112) / 884.0f, 0.62f, 1.0f);
	}
	int UiY(int designY) const noexcept
	{
		return 72 + static_cast<int>(std::lround(static_cast<float>(designY - 72) * UiScale()));
	}
	int UiSize(int designSize) const noexcept
	{
		return std::max(1, static_cast<int>(std::lround(static_cast<float>(designSize) * UiScale())));
	}
	ScreenRect ButtonRect(int index) const noexcept
	{
		const int sideX = m_width - kSideWidth;
		const int top = UiY(110 + index * 44);
		return {sideX + 12, top, m_width - 12, top + UiSize(34)};
	}

	void Update(float dt)
	{
		if (m_width <= 0 || m_height <= 0) return;
		m_frameAccumulator += dt; ++m_frameCount;
		if (m_frameAccumulator >= 0.5f)
		{
			m_fps = static_cast<float>(m_frameCount) / m_frameAccumulator;
			m_frameAccumulator = 0.0f; m_frameCount = 0;
		}
		const float panSpeed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 ? 70.0f : 28.0f;
		if ((GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState(VK_UP) & 0x8000)) m_focus.y -= panSpeed * dt;
		if ((GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000)) m_focus.y += panSpeed * dt;
		if ((GetAsyncKeyState('A') & 0x8000) || (GetAsyncKeyState(VK_LEFT) & 0x8000)) m_focus.x -= panSpeed * dt;
		if ((GetAsyncKeyState('D') & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000)) m_focus.x += panSpeed * dt;
		ClampFocus();
		if (m_paused) return;
		const float simulationDelta = dt * 144.0f * static_cast<float>(m_speedMultiplier);
		m_gameSeconds = std::fmod(m_gameSeconds + simulationDelta, kDaySeconds);
		m_world.Tick(simulationDelta, m_tick++);
		for (Npc& npc : m_npcs)
		{
			const Route& route = kRoutes[npc.route];
			const float length = RouteLength(route);
			npc.routeDistance = std::fmod(npc.routeDistance + npc.speed * dt, length);
			npc.position = RoutePosition(route, npc.routeDistance);
		}
		UpdatePepa(dt);
		UpdateCrime(dt);
		for (std::size_t i = 0; i < m_npcs.size(); ++i)
		{
			if (m_crimeActive) OverrideCrimeCharacter(i);
			if (i == 18u && !m_crimeActive) m_npcs[i].position = m_pepaPosition;
			const bohemia::SimLod before = m_world.Lod(i);
			m_world.SetNpcDistance(i, Distance(m_npcs[i].position, m_focus));
			if (before != m_world.Lod(i)) ++m_lodSwaps;
		}
		m_lodCounts = m_world.CountLods();
	}

	void UpdatePepa(float dt)
	{
		const float distance = Distance(m_pepaPosition, m_pepaTarget);
		if (distance > 0.001f)
		{
			const float amount = std::min(distance, 2.3f * dt);
			m_pepaPosition.x += (m_pepaTarget.x - m_pepaPosition.x) * amount / distance;
			m_pepaPosition.y += (m_pepaTarget.y - m_pepaPosition.y) * amount / distance;
		}
	}

	void UpdateCrime(float dt)
	{
		if (!m_crimeActive) return;
		m_crimeElapsed += dt;
		if (m_crimeElapsed >= 2.0f && !m_heardRecorded)
		{
			bohemia::CrimeDeductionEngine::RecordAuditoryLockpick(m_memory, {62,47,0}, m_crimeElapsed, true);
			m_heardRecorded = true;
		}
		if (m_crimeElapsed >= 5.0f && !m_exitRecorded)
		{
			bohemia::CrimeDeductionEngine::RecordVisualExit(m_memory, 412, {70,49,0}, m_crimeElapsed, true, 7001);
			m_deduction = bohemia::CrimeDeductionEngine::Deduce(m_memory, {7001,14,0.9f}, 14, 2.0f);
			m_exitRecorded = true;
		}
		if (m_crimeElapsed >= 14.0f) m_crimeActive = false;
	}

	void OverrideCrimeCharacter(std::size_t index)
	{
		const float t = m_crimeElapsed;
		if (index == 411u)
		{
			if (t < 2.0f) m_npcs[index].position = {57.0f + 2.0f * t, 48.0f - 0.4f * t};
			else if (t < 5.0f) m_npcs[index].position = {62.0f + (t - 2.0f) * 1.2f, 47.0f};
			else if (t < 9.0f) m_npcs[index].position = {66.0f + (t - 5.0f) * 8.0f, 48.0f + (t - 5.0f) * 5.5f};
			else m_npcs[index].position = {98.0f + (t - 9.0f) * 3.2f, 70.0f + (t - 9.0f) * 2.0f};
		}
		else if (index == 6u && t >= 8.0f)
			m_npcs[index].position = {153.0f - std::min(t - 8.0f, 5.0f) * 7.0f, 47.0f + std::min(t - 8.0f, 5.0f) * 4.0f};
		else if (index == 13u)
			m_npcs[index].position = {60.0f, 49.0f};
		else if (index == 18u)
			m_npcs[index].position = {79.0f, 54.0f};
		else if (index == 33u)
			m_npcs[index].position = {84.0f, 50.0f};
	}

	ScreenRect MapViewport() const noexcept { return {12, 72, std::max(13, m_width - kSideWidth - 8), std::max(73, m_height - 12)}; }
	float MapScale() const noexcept
	{
		const ScreenRect map = MapViewport();
		const float width = static_cast<float>(map.right - map.left);
		const float height = static_cast<float>(map.bottom - map.top);
		const float overviewScale = std::min(width / 218.0f, height / 144.0f);
		const float horizontalExtent = std::max({m_focus.x, 200.0f - m_focus.x, 1.0f});
		const float verticalExtent = std::max({m_focus.y, 130.0f - m_focus.y, 1.0f});
		const float fitScale = std::min(width * 0.5f / horizontalExtent, height * 0.5f / verticalExtent);
		return std::min(overviewScale, fitScale);
	}
	IPoint ToScreen(Vec2 p) const noexcept
	{
		const ScreenRect map = MapViewport(); const float scale = MapScale();
		return {static_cast<int>(std::lround((map.left + map.right) * 0.5f + (p.x - m_focus.x) * scale)),
			static_cast<int>(std::lround((map.top + map.bottom) * 0.5f + (p.y - m_focus.y) * scale))};
	}
	void WorldLine(Vec2 a, Vec2 b, std::uint32_t color, int width = 1, int dash = 0)
	{
		Line(ToScreen(a), ToScreen(b), color, width, dash);
	}
	void WorldRect(Vec2 center, float width, float height, std::uint32_t fill, std::uint32_t edge)
	{
		const IPoint a = ToScreen({center.x - width * 0.5f, center.y - height * 0.5f});
		const IPoint b = ToScreen({center.x + width * 0.5f, center.y + height * 0.5f});
		Rect(a.x, a.y, b.x - a.x, b.y - a.y, fill); Frame(a.x, a.y, b.x - a.x, b.y - a.y, edge, 2);
	}

	void Pixel(int x, int y, std::uint32_t color)
	{
		if (m_pixels == nullptr || x < 0 || y < 0 || x >= m_width || y >= m_height) return;
		m_pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x)] = color;
	}
	void Clear(std::uint32_t color)
	{
		if (m_pixels != nullptr) std::fill_n(m_pixels, static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height), color);
	}
	void Rect(int x, int y, int width, int height, std::uint32_t color)
	{
		const int x0 = std::clamp(x, 0, m_width), y0 = std::clamp(y, 0, m_height);
		const int x1 = std::clamp(x + width, 0, m_width), y1 = std::clamp(y + height, 0, m_height);
		for (int py = y0; py < y1; ++py)
			std::fill(m_pixels + static_cast<std::size_t>(py) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x0),
				m_pixels + static_cast<std::size_t>(py) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x1), color);
	}
	void Frame(int x, int y, int width, int height, std::uint32_t color, int thickness = 1)
	{
		Rect(x, y, width, thickness, color); Rect(x, y + height - thickness, width, thickness, color);
		Rect(x, y, thickness, height, color); Rect(x + width - thickness, y, thickness, height, color);
	}
	void Line(IPoint a, IPoint b, std::uint32_t color, int thickness = 1, int dash = 0)
	{
		const int dx = std::abs(b.x - a.x), sx = a.x < b.x ? 1 : -1;
		const int dy = -std::abs(b.y - a.y), sy = a.y < b.y ? 1 : -1;
		int error = dx + dy, step = 0;
		for (;;)
		{
			if (dash == 0 || ((step / dash) % 2 == 0))
				for (int oy = -thickness / 2; oy <= thickness / 2; ++oy) for (int ox = -thickness / 2; ox <= thickness / 2; ++ox) Pixel(a.x + ox, a.y + oy, color);
			if (a.x == b.x && a.y == b.y) break;
			const int twice = 2 * error;
			if (twice >= dy) { error += dy; a.x += sx; }
			if (twice <= dx) { error += dx; a.y += sy; }
			++step;
		}
	}
	void Circle(int cx, int cy, int radius, std::uint32_t color, int thickness = 1, bool dashed = false)
	{
		if (radius < 1) return;
		IPoint previous{}; bool havePrevious = false;
		for (int degree = 0; degree <= 360; degree += 3)
		{
			if (dashed && ((degree / 9) % 2) != 0) { havePrevious = false; continue; }
			const float angle = static_cast<float>(degree) * kPi / 180.0f;
			const IPoint current{cx + static_cast<int>(std::lround(std::cos(angle) * radius)), cy + static_cast<int>(std::lround(std::sin(angle) * radius))};
			if (havePrevious) Line(previous, current, color, thickness);
			previous = current; havePrevious = true;
		}
	}
	void FilledCircle(int cx, int cy, int radius, std::uint32_t color)
	{
		for (int y = -radius; y <= radius; ++y)
			for (int x = -radius; x <= radius; ++x)
				if (x * x + y * y <= radius * radius) Pixel(cx + x, cy + y, color);
	}
	void Text(int x, int y, const std::string& text, std::uint32_t color, int width = 250, int height = 22)
	{
		if (m_memoryDc == nullptr) return;
		SetTextColor(m_memoryDc, TextColor(color));
		RECT rect{x, y, x + width, y + height};
		DrawTextA(m_memoryDc, text.c_str(), static_cast<int>(text.size()), &rect, DT_SINGLELINE | DT_VCENTER | DT_NOCLIP);
	}
	void CenterText(int x, int y, int width, int height, const std::string& text, std::uint32_t color)
	{
		if (m_memoryDc == nullptr) return;
		SetTextColor(m_memoryDc, TextColor(color));
		RECT rect{x, y, x + width, y + height};
		DrawTextA(m_memoryDc, text.c_str(), static_cast<int>(text.size()), &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOCLIP);
	}
	void Pawn(IPoint p, std::uint32_t color, int size)
	{
		FilledCircle(p.x, p.y - size * 2, size, color);
		Line({p.x, p.y - size}, {p.x, p.y + size * 2}, color, std::max(1, size));
		Line({p.x, p.y}, {p.x - size, p.y + size * 2}, color, std::max(1, size));
		Line({p.x, p.y}, {p.x + size, p.y + size * 2}, color, std::max(1, size));
		Line({p.x, p.y}, {p.x + size * 3, p.y - size * 2}, color, 1);
	}
	void Diamond(IPoint p, std::uint32_t color, int size)
	{
		Line({p.x, p.y - size}, {p.x + size, p.y}, color, 2); Line({p.x + size, p.y}, {p.x, p.y + size}, color, 2);
		Line({p.x, p.y + size}, {p.x - size, p.y}, color, 2); Line({p.x - size, p.y}, {p.x, p.y - size}, color, 2);
	}

	void DrawRoadNetwork()
	{
		const ScreenRect map = MapViewport();
		Rect(map.left, map.top, map.right - map.left, map.bottom - map.top, Pack(26, 35, 39));
		for (int x = 0; x <= 200; x += 10) WorldLine({static_cast<float>(x),0},{static_cast<float>(x),130},Pack(33,44,47),1);
		for (int y = 0; y <= 130; y += 10) WorldLine({0,static_cast<float>(y)},{200,static_cast<float>(y)},Pack(33,44,47),1);
		for (const Route& route : kRoutes)
		{
			for (std::size_t i = 0; i + 1 < route.points.size(); ++i)
			{
				WorldLine(route.points[i], route.points[i + 1], Pack(69,78,79), 11);
				WorldLine(route.points[i], route.points[i + 1], Pack(48,57,59), 7);
				WorldLine(route.points[i], route.points[i + 1], Pack(108,112,103), 1, 5);
			}
		}
		WorldRect({110,76},38,23,Pack(56,63,59),Pack(138,137,119));
		const IPoint marketLabel = ToScreen({110,54});
		CenterText(marketLabel.x - 90, marketLabel.y - 11, 180, 22, "MARKET SQUARE", Pack(231,224,194));
		DrawBuilding({62,43},25,17,"FORGE",Pack(91,70,53),Pack(163,123,76));
		DrawBuilding({109,32},27,18,"TAVERN",Pack(78,58,48),Pack(146,80,60));
		DrawBuilding({160,43},24,18,"GUARDHOUSE",Pack(63,72,78),Pack(92,112,125));
		DrawBuilding({130,90},18,14,"MERCHANT",Pack(91,79,59),Pack(126,102,66));
		DrawBuilding({181,83},16,13,"GATE",Pack(76,66,50),Pack(129,105,67));
		DrawStall({101,75},Pack(171,108,69)); DrawStall({110,71},Pack(127,123,75)); DrawStall({120,78},Pack(147,82,70));
		WorldLine({102,36},{116,36},Pack(185,145,103),4); // tavern bar
		const std::array<Vec2,3> tables{{{103,31},{111,30},{117,34}}};
		for (Vec2 table : tables) { const IPoint p=ToScreen(table); Circle(p.x,p.y,std::max(3,static_cast<int>(MapScale()*1.1f)),Pack(196,168,124),2); }
		WorldRect({65,48},5,3,Pack(121,130,132),Pack(208,190,150)); // forge workstation
		WorldRect({157,51},5,3,Pack(91,105,111),Pack(194,199,191)); // guard post
	}

	void DrawBuilding(Vec2 center, float width, float height, const char* label, std::uint32_t wall, std::uint32_t roof)
	{
		const IPoint a=ToScreen({center.x-width*0.5f,center.y-height*0.5f});
		const IPoint b=ToScreen({center.x+width*0.5f,center.y+height*0.5f});
		Rect(a.x,a.y,b.x-a.x,b.y-a.y,wall); Frame(a.x,a.y,b.x-a.x,b.y-a.y,Pack(168,157,130),2);
		Rect(a.x+3,a.y+3,std::max(1,b.x-a.x-6),std::max(2,(b.y-a.y)/4),roof);
		Rect(a.x+(b.x-a.x)/2-4,b.y-12,8,12,Pack(44,38,31));
		Text(a.x,a.y-21,label,Pack(226,225,210),180,19);
	}

	void DrawStall(Vec2 center, std::uint32_t awning)
	{
		const IPoint p=ToScreen(center); const int width=std::max(15,static_cast<int>(MapScale()*5.0f));
		const int height=std::max(9,static_cast<int>(MapScale()*2.7f));
		Rect(p.x-width/2,p.y-height/2,width,height,Pack(71,57,42));
		Rect(p.x-width/2,p.y-height/2,width, std::max(3,height/2), awning);
		Frame(p.x-width/2,p.y-height/2,width,height,Pack(224,194,144),1);
	}

	bool InsideBuilding(Vec2 p) const noexcept
	{
		return (p.x>50&&p.x<74&&p.y>34&&p.y<51)||(p.x>96&&p.x<122&&p.y>23&&p.y<40)||
			(p.x>148&&p.x<172&&p.y>34&&p.y<52)||(p.x>121&&p.x<139&&p.y>83&&p.y<97)||(p.x>173&&p.x<189&&p.y>76&&p.y<90);
	}

	void DrawDiagnostics()
	{
		const IPoint center=ToScreen(m_focus); const float scale=MapScale();
		Circle(center.x,center.y,static_cast<int>(std::lround(28.0f*scale)),Pack(65,215,232),2,true);
		Circle(center.x,center.y,static_cast<int>(std::lround(60.0f*scale)),Pack(242,184,75),2,true);
		Line({center.x-9,center.y},{center.x+9,center.y},Pack(240,243,240),1);
		Line({center.x,center.y-9},{center.x,center.y+9},Pack(240,243,240),1);
		if (m_crimeActive && m_crimeElapsed >= 2.0f && m_crimeElapsed < 7.0f)
		{
			const IPoint forge=ToScreen({62,47});
			const int radius=static_cast<int>(std::lround(std::min(36.0f,(m_crimeElapsed-2.0f)*9.0f)*scale));
			Circle(forge.x,forge.y,radius,Pack(236,190,93),2,true);
			WorldLine({62,47},{79,54},Pack(230,198,112),1,7);
			WorldLine({62,47},{84,50},Pack(230,198,112),1,7);
			if (m_exitRecorded) WorldLine({70,49},{79,54},Pack(240,225,166),2,5);
			if (m_crimeElapsed >= 8.0f) WorldLine({153,47},m_npcs[411].position,Pack(243,120,87),2);
		}
		for (std::size_t i=0;i<m_npcs.size();++i)
		{
			const bohemia::SimLod tier=m_world.Lod(i);
			const IPoint p=ToScreen(m_npcs[i].position);
			if (p.x<10||p.y<10||p.x>m_width-kSideWidth||p.y>m_height) continue;
			if (InsideBuilding(m_npcs[i].position)) continue;
			if (tier==bohemia::SimLod::Hydrated3D) Pawn(p,Pack(113,231,240),std::clamp(static_cast<int>(MapScale()*0.75f),2,5));
			else if (tier==bohemia::SimLod::Kinematic) Diamond(p,Pack(242,184,75),std::clamp(static_cast<int>(MapScale()*0.65f),2,4));
			else Pixel(p.x,p.y,Pack(119,138,145));
		}
		DrawNamedNpc(14,"BLACKSMITH #14",Pack(230,204,119));
		DrawNamedNpc(7,"GUARD #7",Pack(107,220,232));
		DrawNamedNpc(412,"THIEF #412",Pack(240,111,104));
		if (m_crimeActive) { DrawNamedNpc(19,"WITNESS #19",Pack(236,211,131)); DrawNamedNpc(34,"WITNESS #34",Pack(236,211,131)); }
		if (m_frantaPresent) DrawNamedNpc(24,"FRANTA #24",Pack(201,171,235));
	}

	void DrawNamedNpc(std::uint32_t id, const char* label, std::uint32_t color)
	{
		const IPoint p=ToScreen(m_npcs[id-1u].position);
		FilledCircle(p.x,p.y,5,color); Circle(p.x,p.y,7,Pack(232,239,237),1);
		Text(p.x+8,p.y-12,label,color,150,18);
	}

	void DrawButton(int index, int x, int y, int width, int height, const char* label, std::uint32_t accent)
	{
		Rect(x,y,width,height,Pack(35,47,55)); Frame(x,y,width,height,accent,1); Rect(x,y,4,height,accent);
		Text(x+12,y,label,Pack(235,240,241),width-16,height);
		(void)index;
	}

	void DrawUi()
	{
		const std::uint32_t panel=Pack(15,23,30), edge=Pack(62,82,96), muted=Pack(146,165,176), white=Pack(232,240,244);
		const int sideX=m_width-kSideWidth, scaleH=UiSize(22);
		Rect(0,0,m_width,62,Pack(16,25,34)); Rect(0,60,m_width,2,Pack(65,215,232));
		Text(18,8,"BOHEMIA-ALIFE  /  LIVE 60 FPS SIMULATOR",white,520,24);
		char top[256]{}; std::snprintf(top,sizeof(top),"1000 NPC  |  %.0f FPS  |  SIM %dx  |  %s  |  SPACE PAUSE  +/- SPEED",m_fps,m_speedMultiplier,m_paused?"PAUSED":"RUNNING");
		Text(18,34,top,Pack(170,197,208),850,21);
		Rect(sideX,72,kSideWidth,m_height-84,panel); Frame(sideX,72,kSideWidth,m_height-84,edge,2);
		Text(sideX+14,UiY(78),"LIVE SYSTEM CONTROLS",white,kSideWidth-28,scaleH);
		DrawButton(0,sideX+12,UiY(110),kSideWidth-24,UiSize(34),"[1] TRIGGER FORGE THEFT",Pack(240,111,104));
		DrawButton(1,sideX+12,UiY(154),kSideWidth-24,UiSize(34),m_frantaPresent?"[2] FRANTA AT TAVERN: ON":"[2] FRANTA AT TAVERN: OFF",Pack(201,171,235));
		DrawButton(2,sideX+12,UiY(198),kSideWidth-24,UiSize(34),"[3] STRIKE ARMOR TEST",Pack(242,184,75));
		Rect(sideX+12,UiY(238),kSideWidth-24,1,edge);
		char time[96]{}; const int hour=static_cast<int>(m_gameSeconds/3600.0f), minute=static_cast<int>(m_gameSeconds/60.0f)%60;
		std::snprintf(time,sizeof(time),"TIME %02d:%02d  /  10M DAY",hour,minute);
		Text(sideX+14,UiY(244),time,muted,kSideWidth-28,scaleH);
		char cam[96]{}; std::snprintf(cam,sizeof(cam),"FOCUS %.0f,%.0fM / WASD",m_focus.x,m_focus.y);
		Text(sideX+14,UiY(266),cam,muted,kSideWidth-28,scaleH);
		Text(sideX+14,UiY(291),"LOD HYDRATION / NPC COUNTS",white,kSideWidth-28,scaleH);
		DrawCountBar(sideX+14,UiY(318),"LOD0 CYAN",m_lodCounts.lod0,kNpcCount,Pack(65,215,232));
		DrawCountBar(sideX+14,UiY(344),"LOD1 AMBER",m_lodCounts.lod1,kNpcCount,Pack(242,184,75));
		DrawCountBar(sideX+14,UiY(370),"LOD2 SOULS",m_lodCounts.lod2,kNpcCount,Pack(119,138,145));
		char swaps[96]{}; std::snprintf(swaps,sizeof(swaps),"SWAPS %llu / H28+ H32-",static_cast<unsigned long long>(m_lodSwaps));
		Text(sideX+14,UiY(397),swaps,Pack(176,201,211),kSideWidth-28,scaleH);
		Rect(sideX+12,UiY(422),kSideWidth-24,1,edge); Text(sideX+14,UiY(429),"SIM LOD / SUBBRAIN",white,kSideWidth-28,scaleH);
		const int social=std::clamp(26+(m_frantaPresent?12:0),0,80);
		const int crime=m_crimeActive?14:1, combat=m_crimeActive&&m_crimeElapsed>=8.0f?6:0;
		const std::array<std::pair<const char*,int>,4> layers{{{"L0 AMBIENT",kNpcCount-social-crime-combat},{"L1 SOCIAL",social},{"L2 CRIME",crime},{"L3 COMBAT",combat}}};
		const std::array<std::uint32_t,4> colors{{muted,Pack(65,215,232),Pack(230,204,119),Pack(240,111,104)}};
		for (int i=0;i<4;++i) DrawSmallBar(sideX+14,UiY(454+i*21),layers[static_cast<std::size_t>(i)].first,layers[static_cast<std::size_t>(i)].second,colors[static_cast<std::size_t>(i)]);
		Rect(sideX+12,UiY(545),kSideWidth-24,1,edge); Text(sideX+14,UiY(552),"CRIME DEDUCTION",Pack(230,204,119),kSideWidth-28,scaleH);
		Text(sideX+14,UiY(577),CrimeStatus(),muted,kSideWidth-28,UiSize(38));
		char deduction[96]{}; std::snprintf(deduction,sizeof(deduction),"SUSPECT #%u / %.0f%%",m_deduction.suspect,m_deduction.posterior*100.0f);
		Text(sideX+14,UiY(619),deduction,white,kSideWidth-28,scaleH);
		Text(sideX+14,UiY(644),m_exitRecorded?"AUDIO UNKNOWN > VISIBLE EXIT":"LOCKPICK / AUDIO UNKNOWN",Pack(230,204,119),kSideWidth-28,UiSize(28));
		Rect(sideX+12,UiY(674),kSideWidth-24,1,edge); Text(sideX+14,UiY(681),"TAVERN SMART AREA",Pack(201,171,235),kSideWidth-28,scaleH);
		Text(sideX+14,UiY(706),m_frantaPresent?"PEPA: DICE TABLE / N=2":"PEPA: BAR FALLBACK / N=1",white,kSideWidth-28,scaleH);
		Text(sideX+14,UiY(728),m_pepaRole,muted,kSideWidth-28,scaleH);
		Rect(sideX+12,UiY(756),kSideWidth-24,1,edge); Text(sideX+14,UiY(763),"ARMOR ENERGY WATERFALL",Pack(242,184,75),kSideWidth-28,scaleH);
		DrawArmorPanel(sideX+14,UiY(788),kSideWidth-28,UiSize(164),UiScale());
		const ScreenRect map=MapViewport(); Frame(map.left,map.top,map.right-map.left,map.bottom-map.top,edge,1);
		Text(map.left+12,map.bottom-UiSize(32),"WASD / ARROWS / LMB DRAG: CAMERA  |  [1] CRIME  [2] TAVERN  [3] ARMOR  |  ESC CLOSE",Pack(202,216,223),map.right-map.left-22,UiSize(23));
	}

	void DrawCountBar(int x,int y,const char* label,int count,int total,std::uint32_t color)
	{
		const int scale=UiSize(20); char text[96]{}; std::snprintf(text,sizeof(text),"%s  %d",label,count); Text(x,y,text,Pack(208,221,227),kSideWidth-52,scale);
		const int barX=x, barY=y+UiSize(18), barW=kSideWidth-52;
		Rect(barX,barY,barW,UiSize(5),Pack(42,57,67));
		Rect(barX,barY,std::max(1,barW*count/total),UiSize(5),color);
	}
	void DrawSmallBar(int x,int y,const char* label,int count,std::uint32_t color)
	{
		const int scale=UiSize(19); char text[80]{}; std::snprintf(text,sizeof(text),"%s",label); Text(x,y,text,Pack(197,211,219),113,scale);
		const int valueX=x+116,barW=89;
		Rect(valueX,y+UiSize(4),barW,UiSize(8),Pack(42,57,67)); Rect(valueX,y+UiSize(4),std::max(count?2:0,barW*count/kNpcCount),UiSize(8),color);
		char number[24]{}; std::snprintf(number,sizeof(number),"%d",count); Text(valueX+barW+7,y,number,Pack(235,240,244),48,scale);
	}
	std::string CrimeStatus() const
	{
		if (!m_crimeActive) return "IDLE / PRESS 1 TO TRIGGER";
		if (m_crimeElapsed < 2.0f) return "#412 APPROACHES FORGE";
		if (m_crimeElapsed < 5.0f) return "LOCKPICK / AUDIO UNKNOWN";
		if (m_crimeElapsed < 8.0f) return "VISIBLE EXIT / EVIDENCE MATCH";
		if (m_crimeElapsed < 14.0f) return "GUARD #7 CHASES #412";
		return "ARREST COMPLETE";
	}
	void DrawArmorPanel(int x,int y,int width,int height,float scale)
	{
		if (height < 60) return;
		char strike[96]{};
		if (!m_hasArmorImpact) std::snprintf(strike,sizeof(strike),"PRESS 3: STRIKE");
		else std::snprintf(strike,sizeof(strike),"%s %.0fJ @ %.0fD",m_weaponName.c_str(),m_lastStrikeEnergy,m_strikeAngle);
		const auto size=[&](int n){return std::max(1,static_cast<int>(std::lround(static_cast<float>(n)*scale)));};
		Text(x,y,strike,Pack(229,235,238),width,size(18));
		const int labelsY=y+size(20),barX=x+82,barW=width-88;
		const std::array<const char*,4> names{{"INCIDENT","PLATE","MAILLE","GAMBESON"}};
		std::array<float,4> values{{m_hasArmorImpact?m_lastStrikeEnergy:115.0f,m_hasArmorImpact?m_armorImpact.plateAbsorbedJoules:0.0f,
			m_hasArmorImpact?m_armorImpact.mailleAbsorbedJoules:0.0f,m_hasArmorImpact?m_armorImpact.gambesonAbsorbedJoules:0.0f}};
		for (int i=0;i<4;++i)
		{
			const int row=labelsY+i*size(18); Text(x,row,names[static_cast<std::size_t>(i)],Pack(156,175,185),68,size(17));
			Rect(barX,row+size(5),barW,size(7),Pack(42,57,67)); const int fill=static_cast<int>(std::clamp(values[static_cast<std::size_t>(i)]/160.0f,0.0f,1.0f)*static_cast<float>(barW));
			Rect(barX,row+size(5),fill,size(7),i==0?Pack(221,229,232):i==1?Pack(143,160,173):i==2?Pack(189,199,208):Pack(242,184,75));
		}
		const int vectorY=labelsY+size(78);
		Text(x,vectorY,"BLADE GLANCE VECTOR",Pack(242,184,75),width,size(18));
		const IPoint origin{x+30,vectorY+size(34)}; Circle(origin.x,origin.y,size(10),Pack(143,160,173),2);
		const float angle=m_strikeAngle*kPi/180.0f;
		const IPoint end{origin.x+static_cast<int>(std::lround(std::cos(angle)*size(46))),origin.y-static_cast<int>(std::lround(std::sin(angle)*size(34)))};
		Line(origin,end,Pack(240,111,104),3); Line({end.x-7,end.y+2},{end.x,end.y},Pack(240,111,104),2); Line({end.x,end.y},{end.x-2,end.y+7},Pack(240,111,104),2);
		char wear[96]{}; std::snprintf(wear,sizeof(wear),"PLT%.0f MAIL%.0f HP%.0f",m_armor.condition[0]*100.0f,m_armor.condition[1]*100.0f,m_armor.health);
		Text(x,vectorY+size(62),wear,Pack(186,202,211),width,size(18));
	}

	void Render()
	{
		if (m_pixels == nullptr) return;
		Clear(Pack(10,15,20)); DrawRoadNetwork(); DrawDiagnostics(); DrawUi();
	}

	HINSTANCE m_instance = nullptr;
	HWND m_window = nullptr;
	HDC m_memoryDc = nullptr;
	HBITMAP m_bitmap = nullptr, m_oldBitmap = nullptr;
	HFONT m_font = nullptr, m_oldFont = nullptr;
	BITMAPINFO m_bitmapInfo{};
	std::uint32_t* m_pixels = nullptr;
	int m_width = 1500, m_height = 960;
	bool m_running = true, m_paused = false, m_dragging = false, m_frantaPresent = true;
	IPoint m_lastMouse{};
	Vec2 m_focus{36.5f,60.0f}, m_pepaPosition{}, m_pepaTarget{};
	std::array<Npc,kNpcCount> m_npcs{};
	bohemia::SimLodWorld m_world{};
	bohemia::LodCounts m_lodCounts{56,148,796};
	bohemia::EpisodicMemoryRingBuffer m_memory{};
	bohemia::CrimeDeduction m_deduction{};
	bohemia::ArmorState m_armor{};
	bohemia::ArmorImpact m_armorImpact{};
	std::mt19937 m_random{0xB0E11Au};
	std::string m_weaponName{"LONGSWORD"}, m_pepaRole{"DICE TABLE (N=2 >= 2)"};
	float m_gameSeconds=8.0f*3600.0f,m_crimeElapsed=0.0f,m_fps=60.0f,m_frameAccumulator=0.0f,m_strikeAngle=58.0f,m_lastStrikeEnergy=115.0f;
	std::uint64_t m_lodSwaps=0;
	std::uint32_t m_tick=0;
	int m_speedMultiplier=1,m_frameCount=0,m_tavernAssignmentCount=2;
	bool m_crimeActive=false,m_heardRecorded=false,m_exitRecorded=false,m_hasArmorImpact=false;
};
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
	LiveSimulator app;
	if (!app.Create(instance)) return 1;
	return app.Run();
}
