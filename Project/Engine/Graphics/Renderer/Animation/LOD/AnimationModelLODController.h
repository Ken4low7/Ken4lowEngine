#pragma once
#include <vector>
#include <cstdint>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// 距離に応じたLOD選択 + 距離カリング + 更新間引き を担当
	///
	/// - カリング判定は毎フレ更新（距離に対して即応）
	/// - LOD切替は lodSwitchUpdateEvery_ で間引き可能（チラつき/負荷対策）
	/// - 重い更新（スケルトン更新など）は heavyUpdateEveryByLOD_ でLOD別に間引き可能
	/// - ヒステリシス（gap）で境界付近のパカパカ切替を防止
	/// -------------------------------------------------------------
	class AnimationModelLODController
	{
	public:
		// thresholds: LOD境界距離（サイズは lodCount-1 を想定、超過分は無視）
		void SetThresholds(const std::vector<float>& thresholds);
		const std::vector<float>& GetThresholds() const { return thresholds_; }

		// ヒステリシス幅（例：2.0f）
		void SetHysteresisGap(float g) { hysteresisGap_ = (g < 0.0f) ? 0.0f : g; }
		float GetHysteresisGap() const { return hysteresisGap_; }

		// カリング距離（これを超えたら描画スキップ）
		void SetCullDistance(float d) { cullDistance_ = d; }
		float GetCullDistance() const { return cullDistance_; }

		// 追加で伸ばせる（デバッグ調整用）
		void SetFarCullExtra(float extra) { farCullExtra_ = extra; }
		float GetFarCullExtra() const { return farCullExtra_; }

		// LOD切替判定の更新間隔（フレーム）
		void SetLodSwitchUpdateEvery(uint32_t frames) { lodSwitchUpdateEvery_ = (frames == 0) ? 1u : frames; }
		uint32_t GetLodSwitchUpdateEvery() const { return lodSwitchUpdateEvery_; }

		// 旧名互換
		void SetUpdateEvery(int frames) { SetLodSwitchUpdateEvery((frames <= 0) ? 1u : (uint32_t)frames); }
		int  GetUpdateEvery() const { return (int)lodSwitchUpdateEvery_; }

		// LOD別の重い更新間隔（例：{1,1,2,4}）
		void SetHeavyUpdateEveryByLOD(const std::vector<uint32_t>& v) { heavyUpdateEveryByLOD_ = v; }
		const std::vector<uint32_t>& GetHeavyUpdateEveryByLOD() const { return heavyUpdateEveryByLOD_; }

		// 強制LOD（デバッグ）
		void SetForceLOD(bool enable, int index);
		bool IsForceLOD() const { return forceLOD_; }
		int  GetForcedLODIndex() const { return forcedLODIndex_; }

		// 現在状態
		int  GetLODIndex() const { return lodIndex_; }
		int& GetLODIndexRef() { return lodIndex_; } // デバッグ用途
		bool IsCulled() const { return culledByDistance_; }

		// 毎フレーム呼ぶ：距離（float）とLOD数を渡す
		// 戻り値：LOD/カリング状態が変わったら true
		bool Update(float distance, int lodCount);

		// 毎フレーム呼ぶ：距離^2 とLOD数を渡す（sqrt不要）
		// 戻り値：LOD/カリング状態が変わったら true
		bool UpdateByDistanceSq(float distanceSq, int lodCount);

		// 今フレーム「重い更新」を実行してよいか（LOD別の間引き）
		bool ShouldDoHeavyUpdate() const;

		// 設定も含めて全リセット
		void Reset();

		// 設定は保持して、ランタイム状態だけリセット
		void ResetRuntimeState();

	private:
		// 設定値
		std::vector<float> thresholds_; // LOD境界距離（線形）
		float hysteresisGap_ = 0.0f;

		float cullDistance_ = 200.0f;   // ベースカリング距離
		float farCullExtra_ = 0.0f;

		std::vector<uint32_t> heavyUpdateEveryByLOD_; // LOD別「重い更新」間隔
		uint32_t lodSwitchUpdateEvery_ = 1;

		bool forceLOD_ = false;
		int  forcedLODIndex_ = 0;

		// ランタイム状態
		bool culledByDistance_ = false;
		int  lodIndex_ = 0;
		uint64_t frame_ = 0;
	};
}
