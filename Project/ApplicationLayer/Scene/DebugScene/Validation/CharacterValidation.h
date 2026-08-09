#pragma once

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	class ActorWorld;
	class CharacterActor;
	class HumanoidVisualComponent;
}

namespace K4E = ::Ken4lowEngine;

/// DebugScene上でCharacter共通機能の責務境界とライフサイクルを検証する。
class CharacterValidation
{
public:
	/// 初期化済みActorWorldへ検証用CharacterActorを生成する。
	void Initialize(K4E::ActorWorld& actorWorld);

	/// Debug UIから予約された操作をActorWorld更新前に処理する。
	void ProcessRequests();

	/// Character共通機能の状態と検証操作をImGuiへ表示する。
	void DrawImGui();

	/// Scene終了時にActorWorldとListenerの非所有参照を解除する。
	void Finalize();

private:
	/// 人型表示に関する各確認項目の直近結果を保持する。
	struct HumanoidValidationResults
	{
		bool composition = false;
		bool models = false;
		bool actorTransform = false;
		bool partHierarchy = false;
		bool partVisibility = false;
		bool shadow = false;
		bool colliderFollow = false;
		bool definitionJson = false;
		bool actorJson = false;
		bool editorAndPlay = false;

		/// すべての人型表示確認項目に成功しているか返す。
		bool AllSucceeded() const;
	};

	/// 検証対象CharacterActorが所有する人型表示Componentを返す。
	K4E::HumanoidVisualComponent* FindHumanoidVisual() const;

	/// 名前で検証対象CharacterActorを検索する。
	K4E::CharacterActor* FindTarget() const;

	/// 検証用のActor構成、人型表示、死亡Listenerを重複なく適用する。
	void ConfigureTarget(K4E::CharacterActor& actor);

	/// HP、移動、当たり判定、人型表示、死亡通知、JSON復元をまとめて検証する。
	void RunValidation();

	/// 指定した人型部位の現在表示状態を反転する。
	void TogglePartVisibility(const char* partId);

private:
	K4E::ActorWorld* actorWorld_ = nullptr;
	K4E::CharacterActor* listenerActor_ = nullptr;
	std::uint64_t listenerId_ = 0;
	std::string targetActorName_ = "ValidationCharacter";
	std::string jsonPath_ = "../Generated/Intermediate/CharacterValidation.json";
	std::string validationDefinitionPath_ = "../Generated/Intermediate/HumanoidDefinitionValidation.json";
	std::string lastMessage_ = "未検証";
	HumanoidValidationResults humanoidResults_{};
	float lastAppliedDamage_ = 0.0f;
	float lastDeathDamage_ = 0.0f;
	int deathEventCount_ = 0;
	bool lastSucceeded_ = false;
	bool requestSpawn_ = false;
	bool requestDamage_ = false;
	bool requestLethalDamage_ = false;
	bool requestRestore_ = false;
	bool requestMove_ = false;
	bool requestStop_ = false;
	bool requestSave_ = false;
	bool requestReload_ = false;
	bool requestReloadDefinition_ = false;
	bool requestToggleHead_ = false;
	bool requestToggleLeftArm_ = false;
	bool requestToggleRightArm_ = false;
	bool requestShowAllParts_ = false;
	bool requestValidation_ = true;
};
