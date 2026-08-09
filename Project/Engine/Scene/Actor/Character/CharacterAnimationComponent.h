#pragma once

#include "ActorComponent.h"
#include "ComponentProperty.h"

#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// 描画方式に依存せずCharacterの再生中アニメーションと再生時間を管理する。
	class CharacterAnimationComponent : public ActorComponent
	{
	public:
		/// JSON復元値を有効な再生設定へ補正する。
		void Initialize() override;

		/// Play中のアニメーション再生時間を進める。
		void Update(float deltaTime) override;

		/// 再生状態をDetails上で編集・確認する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "CharacterAnimationComponent"; }

		/// アニメーション再生設定をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONからアニメーション再生設定を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 指定アニメーションを先頭から再生する。
		void Play(std::string_view animationName, float duration, bool loop);

		/// AttackComponentから攻撃モーション名と尺を受け取り、通常再生より優先して開始する。
		void RequestAttack(std::string_view animationName, float duration);

		/// 指定攻撃モーションが終了した場合だけ要求を解除し、移動またはIdleへ戻す。
		void FinishAttack(std::string_view animationName);

		/// 現在位置を維持したまま再生を一時停止する。
		void Pause() { isPlaying_ = false; }

		/// 再生を停止して再生位置を先頭へ戻す。
		void Stop();

		/// 現在のアニメーションを先頭から再開する。
		void Restart();

		/// 再生速度を0以上の有限値へ補正して設定する。
		void SetPlaybackSpeed(float playbackSpeed);

		/// 再生位置をアニメーション時間内へ補正して設定する。
		void SetPlaybackTime(float playbackTime);

		/// 現在選択中のアニメーション名を返す。
		const std::string& GetAnimationName() const { return animationName_; }

		/// 現在の再生位置を秒で返す。
		float GetPlaybackTime() const { return playbackTime_; }

		/// 現在のアニメーション尺を秒で返す。
		float GetDuration() const { return duration_; }

		/// 0から1までの正規化再生位置を返す。
		float GetNormalizedTime() const;

		/// 現在の再生速度を返す。
		float GetPlaybackSpeed() const { return playbackSpeed_; }

		/// 現在再生中か返す。
		bool IsPlaying() const { return isPlaying_; }

		/// ループ再生が有効か返す。
		bool IsLooping() const { return loop_; }

	private:
		/// JSONとDetailsで共有する編集プロパティ一覧を生成する。
		std::vector<ComponentProperty> CreateProperties();

		/// 外部入力された再生設定を安全な範囲へ補正する。
		void SanitizePlaybackState();

		/// 攻撃要求が無い間はMovement速度からIdleとWalkを選択する。
		void UpdateLocomotionAnimation();

		/// 現在の名前と正規化時間をHumanoidVisualComponentの部位ポーズへ反映する。
		void ApplyHumanoidPose();

	private:
		std::string animationName_ = "Idle";
		float duration_ = 1.0f;
		float playbackTime_ = 0.0f;
		float playbackSpeed_ = 1.0f;
		bool loop_ = true;
		bool isPlaying_ = true;
		bool attackRequestActive_ = false;
	};
} // namespace Ken4lowEngine
