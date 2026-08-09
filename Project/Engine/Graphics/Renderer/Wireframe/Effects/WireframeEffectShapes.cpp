// Wireframe の魔法陣・ハートなど演出向け形状をまとめる。
#include "Wireframe.h"
#include <cmath>
#include <numbers>

namespace Ken4lowEngine
{

	void Wireframe::DrawPentagram(const Vector3& center, float radius, const Vector4& color)
	{
		const float PI = std::numbers::pi_v<float>;
		Vector3 points[5];
		// 五芒星の5頂点を計算
		for (int i = 0; i < 5; i++) {
			float angle = PI / 2.0f + (2.0f * PI / 5.0f) * i * 2; // 星の頂点の角度
			points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
		}
		// 線を結ぶ（星の交差する部分）
		for (int i = 0; i < 5; i++) {
			DrawLine(points[i], points[(i + 2) % 5], color);
		}
	}

	void Wireframe::DrawHexagram(const Vector3& center, float radius, const Vector4& color)
	{
		const float PI = std::numbers::pi_v<float>;
		Vector3 points[6];
		// 六芒星の6頂点を計算
		for (int i = 0; i < 6; i++) {
			float angle = (PI / 6.0f) + (2.0f * PI / 6.0f) * i;
			points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
		}
		// 正三角形1
		DrawLine(points[0], points[2], color);
		DrawLine(points[2], points[4], color);
		DrawLine(points[4], points[0], color);
		// 正三角形2（逆向き）
		DrawLine(points[1], points[3], color);
		DrawLine(points[3], points[5], color);
		DrawLine(points[5], points[1], color);
	}

	void Wireframe::DrawMagicCircle(const Vector3& center, float radius, const Vector4& color)
	{
		// 外円
		DrawCircle(center, radius, 60, color);
		// 五芒星
		DrawPentagram(center, radius * 0.8f, color);
		// 六芒星
		DrawHexagram(center, radius * 0.6f, color);
		// 中心を囲む小円
		DrawConcentricCircles(center, radius * 0.5f, 3, radius * 0.15f, color);
		// 12本の放射状の線
		const float PI = std::numbers::pi_v<float>;
		for (int i = 0; i < 12; i++) {
			float angle = (2.0f * PI / 12) * i;
			Vector3 start = center + Vector3(cos(angle) * radius * 0.3f, 0.0f, sin(angle) * radius * 0.3f);
			Vector3 end = center + Vector3(cos(angle) * radius, 0.0f, sin(angle) * radius);
			DrawLine(start, end, color);
		}
		// 円周上の多角形
		DrawPolygon(center, radius * 0.9f, 8, color);
	}

	void Wireframe::DrawAdvancedMagicCircle(const Vector3& center, float radius, const Vector4& color)
	{
		// 五芒星
		DrawPentagram(center, radius * 0.8f, color);
		// 外円
		DrawCircle(center, radius, 50, color);
		// 同心円
		DrawConcentricCircles(center, radius * 0.5f, 3, radius * 0.15f, color);
		// 六芒星
		DrawHexagram(center, radius * 0.6f, color);
	}

	void Wireframe::DrawRotatingPentagram(const Vector3& center, float radius, const Vector4& color, float time)
	{
		const float PI = std::numbers::pi_v<float>;
		Vector3 points[5];
		// 回転角度（時間に応じて回転）
		float rotation = time * PI * 0.2f;
		for (int i = 0; i < 5; i++) {
			float angle = PI / 2.0f + (2.0f * PI / 5.0f) * i * 2 + rotation;
			points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
		}
		// 線を描画
		for (int i = 0; i < 5; i++) {
			DrawLine(points[i], points[(i + 2) % 5], color);
		}
	}

	void Wireframe::DrawExpandingMagicCircle(const Vector3& center, float baseRadius, const Vector4& color, float time)
	{
		float scale = 1.0f + 0.5f * sin(time * 2.0f); // 半径が時間に応じて振動
		// 拡大縮小する魔法陣
		DrawPentagram(center, baseRadius * scale, color);
		DrawCircle(center, baseRadius * scale * 1.1f, 50, color);
	}

	void Wireframe::DrawFadingMagicCircle(const Vector3& center, float radius, float time)
	{
		float alpha = (sin(time) + 1.0f) * 0.5f; // 0.0 ～ 1.0 の範囲でフェード
		Vector4 color = { 1.0f, 0.5f, 0.0f, alpha };
		DrawPentagram(center, radius, color);
		DrawCircle(center, radius * 1.1f, 50, color);
	}

	void Wireframe::DrawAnimatedMagicCircle(const Vector3& center, float radius, float time)
	{
		float rotation = time * 1.5f;    // 回転速度
		float scale = 1.0f + 0.2f * sin(time * 3.0f); // 拡縮
		float alpha = (sin(time) + 1.0f) * 0.5f; // フェード
		Vector4 color = { 1.0f, 0.5f, 0.0f, alpha };
		// 魔法陣の円
		DrawCircle(center, radius * scale, 50, color);
		// 五芒星の回転
		const float PI = std::numbers::pi_v<float>;
		Vector3 points[5];
		for (int i = 0; i < 5; i++) {
			float angle = PI / 2.0f + (2.0f * PI / 5.0f) * i * 2 + rotation;
			points[i] = center + Vector3(radius * scale * cos(angle), 0.0f, radius * scale * sin(angle));
		}
		for (int i = 0; i < 5; i++) {
			DrawLine(points[i], points[(i + 2) % 5], color);
		}
	}

	void Wireframe::DrawProgressiveMagicCircle(const Vector3& center, float radius, const Vector4& baseColor, float time)
	{
		const float PI = std::numbers::pi_v<float>;
		// フェードイン効果
		float alpha = (sin(time) + 1.0f) * 0.5f;
		Vector4 color = { baseColor.x, baseColor.y, baseColor.z, alpha };
		// 回転速度
		float rotation = time * 1.5f;
		// 線が描かれる進行度（0.0 ～ 1.0）
		float progress = fmod(time, 5.0f) / 5.0f;
		// **外円をなぞって描く**
		if (progress > 0.0f) {
			float circleProgress = progress * 2.0f; // 0.0 ~ 2.0
			int segmentCount = 60;
			int visibleSegments = static_cast<int>(segmentCount * fmin(circleProgress, 1.0f)); // なぞる部分を計算
			DrawCircle(center, radius, visibleSegments, color);
		}
		// **放射線を1本ずつ描く**
		if (progress > 0.2f) {
			float lineProgress = (progress - 0.2f) * 5.0f; // 0.0 ~ 1.0 の範囲
			int visibleLines = static_cast<int>(12 * fmin(lineProgress, 1.0f));
			for (int i = 0; i < visibleLines; i++) {
				float angle = (2.0f * PI / 12) * i + rotation;
				Vector3 start = center + Vector3(cos(angle) * radius * 0.3f, 0.0f, sin(angle) * radius * 0.3f);
				Vector3 end = center + Vector3(cos(angle) * radius, 0.0f, sin(angle) * radius);
				DrawLine(start, end, color);
			}
		}
		// **五芒星を線をなぞりながら描く**
		if (progress > 0.4f) {
			float starProgress = (progress - 0.4f) * 5.0f;
			DrawPentagramProgressive(center, radius * 0.8f, color, starProgress);
		}
		// **六芒星を線をなぞりながら描く**
		if (progress > 0.6f) {
			float hexProgress = (progress - 0.6f) * 5.0f;
			DrawHexagramProgressive(center, radius * 0.6f, color, hexProgress);
		}
		// **同心円を線をなぞりながら描く**
		if (progress > 0.8f) {
			float concentricProgress = (progress - 0.8f) * 5.0f;
			int visibleCircles = static_cast<int>(3 * fmin(concentricProgress, 1.0f));
			DrawConcentricCircles(center, radius * 0.5f, visibleCircles, radius * 0.15f, color);
		}
	}

	void Wireframe::DrawPentagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress)
	{
		const float PI = std::numbers::pi_v<float>;
		Vector3 points[5];
		for (int i = 0; i < 5; i++) {
			float angle = PI / 2.0f + (2.0f * PI / 5.0f) * i * 2; // 星の頂点の角度
			points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
		}
		int visibleLines = static_cast<int>(5 * fmin(progress, 1.0f)); // 5本の線を順番に描く
		for (int i = 0; i < visibleLines; i++) {
			DrawLine(points[i], points[(i + 2) % 5], color);
		}
	}

	void Wireframe::DrawHexagramProgressive(const Vector3& center, float radius, const Vector4& color, float progress)
	{
		const float PI = std::numbers::pi_v<float>;
		Vector3 points[6];
		for (int i = 0; i < 6; i++) {
			float angle = (PI / 6.0f) + (2.0f * PI / 6.0f) * i;
			points[i] = center + Vector3(radius * cos(angle), 0.0f, radius * sin(angle));
		}
		int visibleLines = static_cast<int>(6 * fmin(progress, 1.0f)); // 6本の線を順番に描く
		for (int i = 0; i < visibleLines; i++) {
			DrawLine(points[i], points[(i + 1) % 6], color);
		}
	}

	void Wireframe::DrawAnimatedHeart(const Vector3& center, float size, float time)
	{
		const float PI = std::numbers::pi_v<float>;
		const uint32_t segments = 100;
		float progress = fmin(time / 2.0f, 1.0f);  // 0.0 ～ 1.0 で線を増やす
		float alpha = fmin(time / 2.0f, 1.0f);  // フェードイン
		Vector4 color = { 1.0f, 0.0f, 0.2f, alpha };  // 赤色 (フェードイン)
		// スケールのゆらぎ（ポップなアニメーション）
		float scale = size * (1.0f + 0.05f * sin(time * 3.0f));
		std::vector<Vector3> points;
		// ハートのパラメトリック曲線
		for (uint32_t i = 0; i < segments * progress; i++) {
			float t = PI * 2.0f * i / segments;
			float x = 16.0f * pow(sin(t), 3.0f);
			float y = 13.0f * cos(t) - 5.0f * cos(2 * t) - 2.0f * cos(3 * t) - cos(4 * t);
			points.push_back(center + Vector3(x * scale * 0.1f, y * scale * 0.1f, 0.0f));
		}
		// ハートの輪郭を線でつなぐ
		for (size_t i = 1; i < points.size(); i++) {
			DrawLine(points[i - 1], points[i], color);
		}
	}

	void Wireframe::DrawGlowingHeart(const Vector3& center, float size, float time)
	{
		DrawAnimatedHeart(center, size, time);
		float glowAlpha = (sin(time * 2.0f) + 1.0f) * 0.5f; // 明滅
		Vector4 glowColor = { 1.0f, 0.4f, 0.6f, glowAlpha };
		DrawCircle(center, size * 0.6f, 50, glowColor);
		DrawCircle(center, size * 0.7f, 50, glowColor);
	}

	void Wireframe::DrawPoppingHeart(const Vector3& center, float size, float time)
	{
		float popScale = size * (1.0f + 0.1f * sin(time * 5.0f)); // 拡縮
		DrawAnimatedHeart(center, popScale, time);
	}

	void Wireframe::DrawFloatingHeart(const Vector3& basePosition, float size, float time)
	{
		Vector3 floatingCenter = basePosition + Vector3(0.0f, sin(time * 2.0f) * 0.5f, 0.0f);
		DrawAnimatedHeart(floatingCenter, size, time);
	}

	void Wireframe::DrawMagicPentagram(const Vector3& center, float radius, const Vector4& color)
	{
		// 五芒星を描画
		DrawPentagram(center, radius, color);
		// 外円を描画
		DrawCircle(center, radius * 1.1f, 50, color);
	}

	void Wireframe::DrawConcentricCircles(const Vector3& center, float radius, uint32_t count, float spacing, const Vector4& color)
	{
		for (uint32_t i = 0; i < count; i++) {
			DrawCircle(center, radius + (spacing * i), 50, color);
		}
	}

	void Wireframe::DrawRotatingMagicCircle(const Vector3& center, float radius, const Vector4& color)
	{
		DrawMagicCircle(center, radius, color);
		DrawPentagram(center, radius * 0.8f, color);
	}

} // namespace Ken4lowEngine
