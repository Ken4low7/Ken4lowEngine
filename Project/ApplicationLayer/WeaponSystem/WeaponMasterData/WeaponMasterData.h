#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <optional>

/// ---------- レアリティ定義 ---------- ///
enum class EWeaponRarity : uint8_t
{
	Common,      // コモン
	Rare,        // レア
	Epic,        // エピック
	Legendary,   // レジェンダリー
	Mythical,	 // ミシカル
};

/// ---------- 弾薬タイプ定義 ---------- ///
enum class EAmmoType : uint8_t
{
	Default,    // デフォルト
	Energy,     // エネルギー
	Explosive,  // 爆発物
	None, 		// なし（近接武器等）
};

/// ---------- カテゴリー定義 ---------- ///
enum class EWeaponCategory : uint8_t
{
	Primary,   // プライマリ
	Backup,    // バックアップ
	Melee,     // 近接
	Special,   // 特殊
	Sniper,    // スナイパー
	Heavy      // ヘビー
};

enum class EDeathKnockbackType : uint8_t
{
	Default,
	Light,
	Sniper,
	Heavy,
	Explosion,
};

/// ---------- 射撃モード定義 ---------- ///
enum class EFireMode : uint8_t
{
	SemiAuto,   // セミオート
	Burst,      // バースト
	FullAuto,   // フルオート
	Charge,     // チャージ
};

/// ---------- レティクル定義 ---------- ///
enum class EReticleType : uint8_t
{
	None,	// なし
	Dot,	// 点
	Cross,
	Circle,
	Scope,
};

/// ---------- 特殊能力定義 ---------- ///
enum class EWeaponAttribute : uint16_t
{
	None,		// なし
	Poison,		// 毒
	Burning,	// 発火
	AreaDamage,	// 爆風ダメージ
	Bouncing,	// 反射弾
	LifeSteal,	// 吸血
	WallBreak,	// 壁貫通
	FixedDelay,	// 固定ディレイ
};

/// ---------- アニメーション定義 ---------- ///
enum class EWeaponAnimation : uint8_t
{
	Equip,          // 装備
	Idle,           // 待機
	Fire,           // 発射
	Reload,         // リロード
	MeleeAttack,    // 近接攻撃
};

/// ---------- サウンド定義 ---------- ///
enum class EWeaponSound : uint8_t
{
	Fire,           // 発射音
	Reload,         // リロード音
	Impact,         // ヒット音
	Empty,          // 空撃ち音
	Equip,          // 装備音
};

/// ---------- 基本構造体 ---------- ///
struct FWeaponCore
{
	int32_t weaponID = 0;									// 武器ID
	std::string weaponName = "Weapon";						// 武器名
	EWeaponCategory category = EWeaponCategory::Primary;	// 武器カテゴリー
	EWeaponRarity rarity = EWeaponRarity::Common;           // レアリティ
};

/// ---------- 射撃性能データ ---------- ///
struct FWeaponStats
{
	float damage = 10.0f;                       // ダメージ
	float minDamage = 10.0f;                    // 最小ダメージ（距離減衰などがある場合）
	float damageFalloffStart = 0.0f;			// 距離減衰開始距離
	float damageFalloffEnd = 0.0f;				// 距離減衰終了距離

	float fireRate = 600.0f;                    // 発射速度（RPM）
	float mobility = 1.0f;                      // 機動性
	int32_t capacity = 30;                      // マガジン弾薬容量
	EAmmoType ammoType = EAmmoType::Default;	// 弾薬タイプ

	float reloadTime = 1.6f;                    // リロード時間（秒）
	float tacticalReloadTime = 1.6f;			// タクティカルリロード時間（秒）
	float emptyReloadTime = 2.0f;				// 空マガジンからのリロード時間（秒）
	bool bReloadByShell = false;				// 1発ずつリロード（SG等）
	bool bCanInterruptReload = true;			// リロード中断可能かどうか

	int32_t ammoPerShot = 1;                    // 1発あたりの消費弾薬数
	int32_t maxReserveAmmo = 90;				// 最大予備弾薬数
	bool bHasChamber = true;					// 薬室を持つかどうか
	int32_t chamberSize = 1;					// 薬室の弾数

	// クリティカル関連
	float criticalChance = 0.05f;               // クリティカルヒット確率
	float headshotMultiplier = 1.5f;            // ヘッドショットダメージ倍率
	float bodyMultiplier = 1.0f;				// ボディショットダメージ倍率
	float armMultiplier = 0.9f;					// 腕ショットダメージ倍率
	float legMultiplier = 0.8f;					// 脚ショットダメージ倍率

	// ショットガン等のペレット設定
	int32_t pelletCount = 1;					// ペレット数（1なら通常の弾）
	float pelletSpreadAngle = 0.0f;				// ペレットの拡散角度（度）
};

/// ---------- 操作・反動データ ---------- ///
struct FWeaponHandling
{
	float accuracy = 0.8f;                      // 精度
	float spreadIncrease = 0.01f;				// 射撃ごとの拡散増加量

	// 散布界（ブルーム）詳細
	float baseHipSpread = 1.0f;					// ヒップショット時の基本拡散
	float baseAdsSpread = 0.25f;				// ADS基本拡散（度）
	float moveSpreadMultiplier = 1.5f;			// 移動時の拡散倍率
	float jumpSpreadMultiplier = 2.0f;			// ジャンプ時の拡散倍率
	float crouchSpreadMultiplier = 0.8f;		// しゃがみ時の拡散倍率
	float spreadRecoveryRate = 8.0f;			// 拡散回復速度（秒あたり）
	float maxSpread = 6.0f;						// 最大拡散（度）

	// 反動（視点 / 武器）
	float verticalRecoil = 0.02f;               // 垂直反動
	float horizontalRecoil = 0.1f;              // 水平反動
	float cameraRecoilPitch = 0.6f;				// 視点反動（上下）
	float cameraRecoilYaw = 0.25f;				// 視点反動（左右）
	float weaponKickBack = 0.02f;				// 武器モデルの後退量
	float recoilRecovery = 8.0f;				// 反動回復速度
	float recoilResetDelay = 0.06f;				// 反動リセットまでの猶予時間（秒）

	// エイムダウンサイト（ADS）関連
	float adsZoomFov = 60.0f;					// ADS時の視野角
	float zoomLevel = 1.0f;						// スコープ倍率
	float adsTransitionSpeed = 10.0f;			// ADSへの移行速度
	float adsInTime = 0.12f;					// ADSに入るまでの時間（秒）
	float adsOutTime = 0.1f;					// ADSから出るまでの時間（秒）
	float adsMoveSpeedMultiplier = 0.85f;		// ADS中の移動速度倍率

	// 武器切替・スプリント遷移
	float equipTime = 0.35f;					// 装備時間（秒）
	float unequipTime = 0.25f;					// しまう時間（秒）
	float sprintToFireTime = 0.12f;				// スプリントから射撃までの時間
	float fireToSprintTime = 0.08f;				// 射撃後スプリント復帰まで
	float fixedDelayTime = 10.0f;				// 武器切替時の固定ディレイ時間（秒）
};

/// ---------- 弾道・判定詳細データ ---------- ///
struct FWeaponProjectileData
{
	// 射程・弾道
	float maxRange = 0.0f;						// 最大射程
	bool bIsProjectile = false;                 // 弾道武器かどうか
	float projectileSpeed = 0.0f;               // 弾道速度（弾道武器の場合）
	float gravityScale = 1.0f;					// 重力倍率
	float projectileLifeTime = 5.0f;			// 弾の寿命
	float projectileDrag = 0.0f;				// 弾の空気抵抗
	float projectileRadius = 0.0f;				// 弾体半径
	float traceRadius = 0.0f;					// ヒットスキャンの判定半径（0なら線）
	float spawnForwardOffset = 0.0f;			// 銃口から前方にずらす距離

	int32_t pierceCount = 0;					// 貫通可能な敵数
	int32_t ricochetCount = 0;					// 反射可能な回数
	float splashRadius = 0.0f;					// 爆風半径（爆発物の場合）
	bool bCanDamageSelf = false;				// 自己ダメージ可能かどうか
};

/// ---------- バースト設定データ ---------- ///
struct FBurstSettings
{
	int32_t count = 0;						// バースト内の射撃数
	float interval = 0.0f;                 // バースト内の射撃間隔（秒）
};

/// ---------- チャージ設定データ ---------- ///
struct FChargeSettings
{
	float maxChargeTime = 0.0f;	// 最大チャージ時間（秒）
};

/// ---------- 近接武器専用データ ---------- ///
struct FMeleeWeaponData
{
	// 攻撃範囲・判定
	float attackRange = 2.0f;			 // 攻撃範囲
	float attackArc = 90.0f;			 // 攻撃角度
	float verticalAngle = 20.0f;		 // 垂直攻撃角度

	// タイミング
	float startupDelay = 0.1f;			 // 攻撃ボタンを押してから判定が出るまでの時間（予備動作）
	float activeFrames = 0.15f;			 // 攻撃判定が持続する時間
	float recoveryDelay = 0.25f;		 // 振り終わった後の硬直時間

	// コンボ設定
	int32_t maxComboCount = 3;			 // 最大コンボ数
	float comboWindow = 0.25f;			 // 次のコンボ入力を受け付ける猶予時間

	// 特殊アクション
	float dashImpulse = 3.0f;			 // 攻撃時の前進ダッシュ速度
	bool bCanCharge = false;			 // チャージ攻撃が可能かどうか
	float maxChargeTime = 0.0f;			 // 最大チャージ時間
	float chargeDamageMultiplier = 1.0f; // チャージ時のダメージ倍率

	// 防御挙動
	bool bCanBlock = false;				 // ブロック可能かどうか
	float blockDamageReduction = 0.5f;	 // ブロック時のダメージ軽減率

	// ヒット時の挙動
	float hitStopDuration = 0.2f;		 // 当たった瞬間にゲームを止める時間（手応えの演出）
	float knockbackForce = 30.0f;		 // 敵を弾き飛ばす力
};

/// ---------- レティクルデータ ---------- ///
struct FWeaponReticleData
{
	// ==========================================
	// 基本（腰だめ）
	// ==========================================
	std::string reticleTexturePath = "";            // 腰だめ用レティクル
	EReticleType reticleType = EReticleType::Cross; // レティクル形状
	float reticleBaseSize = 12.0f;                  // 通常サイズ
	float reticleMaxSize = 28.0f;                   // 最大拡張サイズ
	float reticleExpandPerShot = 2.0f;              // 射撃ごとの拡張量
	float reticleRecoverSpeed = 18.0f;              // 縮小速度

	// ==========================================
	// 移動・空中・着地状態に応じたレティクル拡張演出
	// ==========================================
	bool bEnableMoveReticleExpand = true;           // 移動に応じて広がる
	float moveExpandMultiplier = 1.15f;             // 歩き時の倍率
	float sprintExpandMultiplier = 1.35f;           // ダッシュ時の倍率
	float airExpandMultiplier = 1.60f;              // 空中時の倍率
	float landExpandImpulse = 2.0f;                 // 着地時の瞬間拡張量

	// ==========================================
	// ADS時の見た目切り替え
	// ==========================================
	bool bHideReticleInADS = false;                 // ADS時に全体非表示（スコープ用）
	bool bUseAdsReticleOverride = false;            // ADS時に別レティクル画像を使う
	std::string adsReticleTexturePath = "";         // ADS用レティクル画像

	bool bUseAdsCenterDot = false;                  // ADS時に中央ドットを表示
	std::string adsCenterDotTexturePath = "";       // ADS中央ドット画像

	float adsReticleBlendTime = 0.06f;              // ADS切替ブレンド時間（秒）

	// ==========================================
	// ヒット / 撃破演出
	// ==========================================
	bool bShowHitMarker = true;                     // ヒットマーカー表示
	std::string hitMarkerTexturePath = "";          // 通常ヒット画像

	bool bUseHeadshotMarker = false;                // ヘッドショット用を分ける
	std::string headshotHitMarkerTexturePath = "";  // ヘッドショット画像

	bool bUseKillConfirmMarker = false;             // 撃破時マーカー
	std::string killConfirmMarkerTexturePath = "";  // 撃破時画像

	float hitMarkerDuration = 0.06f;                // ヒット表示時間
	float killConfirmDuration = 0.12f;              // 撃破表示時間
};

/// ---------- VFXデータ ---------- ///
struct FWeaponVfx
{
	std::string muzzleFlashVfxPath = "";           // マズルフラッシュ
	std::string tracerVfxPath = "";                // トレーサー
	std::string impactVfxPath = "";                // ヒットVFX
	std::string shellEjectVfxPath = "";            // 排莢VFX

	// リロード / 特殊
	std::string reloadVfxPath = "";				// リロードVFX
	std::string chargeVfxPath = "";				// チャージVFX

	// 近接用
	std::string meleeSwingVfxPath = "";			// 近接攻撃のスイングエフェクト
	std::string meleeHitVfxPath = "";				// 近接攻撃のヒットエフェクト

	// 強さ調整
	float muzzleFlashScale = 1.0f;				// マズルフラッシュのスケール
	float tracerScale = 1.0f;					// トレーサーのスケール
	float impactScale = 1.0f;					// ヒットVFXのスケール
	float meleeSwingScale = 1.0f;				// 近接スイングVFXのスケール
	float meleeHitScale = 1.0f;					// 近接ヒットVFXのスケール
};

/// ---------- ソケット名データ ---------- ///
struct FWeaponSockets
{
	// 共通
	std::string weaponAttachSocket = ""; // 手に持つ時の武器ルート（例: weapon_root）
	std::string rightHandSocket = "";    // 右手基準（必要なら）
	std::string leftHandIkSocket = "";   // 左手IK用
	std::string adsCameraSocket = "";    // ADSカメラ基準（任意）
	std::string magazineSocket = "";     // マガジン差し替え用（任意）

	// 射撃系
	std::string muzzleSocket = "";       // マズル
	std::string shellEjectSocket = "";   // 薬莢排出
	std::string tracerStartSocket = "";  // トレーサー開始位置（任意）
	std::string scopeSocket = "";        // スコープ表示位置（任意）

	// 近接系
	std::string meleeTraceStartSocket = ""; // 近接判定開始
	std::string meleeTraceEndSocket = "";   // 近接判定終了
	std::string meleeHitSocket = "";        // 近接ヒットVFX基準（任意）
};

/// ---------- 決済データ ---------- ///
struct FWeaponEconomyData
{
	std::string currencyType = "";	// 通貨タイプ（例："Gold", "Gems"）
	int32_t purchasePrice = 100;	// 購入価格
	int32_t minLevelToUnlock = 0;	// 解放に必要なプレイヤーレベル

	// アップグレードコスト（レベルごと）
	std::vector<int32_t> upgradeCosts = { 100, 200, 300 };

	// セール情報
	bool bIsLimitedTime = false;	// 限定販売フラグ
	float discountRate = 0.0f;		// 割引率（例：0.2は20%オフ）
};

/// ---------- アセットデータ ---------- ///
struct FWeaponAssets
{
	std::string modelPath = "";						// 3Dモデルのパス
	std::string iconPath = "";						// アイコン画像のパス
};

/// ---------- サウンドデータ ---------- ///
struct FWeaponSounds
{
	std::string fireSoundPath = "";					// 発射音のパス
	std::string reloadSoundPath = "";				// リロード音のパス
	std::string emptySoundPath = "";				// 空撃ち音のパス
	std::string equipSoundPath = "";				// 装備音のパス
	std::string impactSoundPath = "";				// ヒット音のパス
};

struct FWeaponDeathReaction
{
	EDeathKnockbackType type = EDeathKnockbackType::Default;
	float power = 8.0f;
	float upPower = 2.0f;
	float explosionRadius = 0.0f;
	float impulseScale = 1.0f;
};

/// <summary>
/// JSONから読み書きする武器1本分のマスターデータ。
/// エディタで編集した値を保持し、WeaponSystemで実行時用のWeaponParamsへ変換する。
/// </summary>
struct FWeaponMasterData
{
	/// ---------- 共通データ ---------- ///
	FWeaponCore coreData;

	/// ---------- 経済データ ---------- ///
	FWeaponEconomyData economyData;

	/// ---------- 射撃武器の場合のみ使用するデータ ---------- ///
	FWeaponStats stats;

	/// ---------- 操作・反動データ ---------- ///
	FWeaponHandling handling;

	/// ---------- レティクルデータ ---------- ///
	FWeaponReticleData reticleData;

	/// ---------- アセットデータ ---------- ///
	FWeaponAssets assetData;

	/// ---------- サウンドデータ ---------- ///
	FWeaponSounds soundData;

	/// ---------- VFXデータ ---------- ///
	FWeaponVfx vfxData;

	/// ---------- 敵死亡時の吹っ飛び設定 ---------- ///
	FWeaponDeathReaction deathReaction;

	/// ---------- ソケットデータ ---------- ///
	FWeaponSockets socketData;

	/// ---------- 近接武器の場合のみ使用するデータ ---------- ///
	std::optional<FMeleeWeaponData> meleeData;			 // 近接武器専用データ

	// 弾道・判定詳細データ（弾道武器の場合のみ）
	std::optional<FWeaponProjectileData> projectileData;

	// バースト設定（バースト武器の場合のみ）
	std::optional<FBurstSettings> burstSettings;

	// チャージ設定（チャージ武器の場合のみ）
	std::optional<FChargeSettings> chargeSettings;

	// 射撃モード
	EFireMode defaultFireMode = EFireMode::SemiAuto; // デフォルトの射撃モード
	std::vector<EFireMode> supportedFireModels;		// 対応する射撃モードのリスト

	bool bIsAutomatic = false;				  // フルオートかどうか
	bool bCanToggleFireMode = false;          // フル/セミ切替可能か（V等）

	// 特殊能力
	std::vector<EWeaponAttribute> attributes; // 武器の特殊能力リスト
};
