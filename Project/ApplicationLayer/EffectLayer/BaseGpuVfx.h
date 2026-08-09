#pragma once
#include <string>

#include "GpuParticleManager.h" // GpuParticleEmitter も含まれる

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///			　GPUパーティクル演出の共通基底クラス
/// -------------------------------------------------------------
class BaseGpuVfx
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~BaseGpuVfx() = default;

	/// <summary>
	/// K4E::GpuParticleManager を初期化します。
	/// </summary>
	/// <param name="manager">初期化対象の K4E::GpuParticleManager へのポインタ。</param>
	/// <param name="prefix">オプションの接頭辞。リソース名や識別子に使われることがあり、デフォルトは空文字列</param>
	void Initialize(K4E::GpuParticleManager* manager, const std::string& prefix = "");

	/// <summary>
	/// オブジェクトの状態を初期状態にリセットする仮想メソッド。
	/// </summary>
	virtual void Reset() {}

protected: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 派生先でエミッターを登録する純粋仮想メソッド。
	/// </summary>
	virtual void RegisterEmitters() = 0;

protected: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定したキーから名前を生成して返します。
	/// </summary>
	/// <param name="key">名前を生成するためのキーとなる文字列。</param>
	/// <returns>生成された名前を表すstd::string。</returns>
	std::string MakeName(const std::string& key);

	/// <summary>
	/// 指定したキーに対応するエミッターを取得し、存在しない場合は新しく作成して返します。
	/// </summary>
	/// <param name="key">エミッターを識別するキー。</param>
	/// <param name="info">新しくエミッターを作成する際の設定情報（K4E::GpuParticleEmitter::K4E::EmitterInfo）。</param>
	/// <returns>指定したキーに対応する K4E::GpuParticleEmitter へのポインタ。既存のエミッターがあればそのポインタを、存在しなければ新たに作成したエミッターのポインタを返します。</returns>
	K4E::GpuParticleEmitter* GetOrCreateEmitter(const std::string& key, const K4E::GpuParticleEmitter::EmitterInfo& info);

	/// <summary>
	/// 指定されたキーに対応するGPUパーティクルエミッタを検索して返します。
	/// </summary>
	/// <param name="key">検索に使用する識別キー（const std::string&）。</param>
	/// <returns>対応するGpuParticleEmitterへのポインタ。該当するエミッタが存在しない場合はnullptrを返します。</returns>
	K4E::GpuParticleEmitter* FindEmitter(const std::string& key);

	/// <summary>
	/// エミッタの位置を設定し、指定数のパーティクルを放出する。
	/// </summary>
	/// <param name="emitter">パーティクルエミッタを指すポインタ。位置の設定と放出の対象となる。</param>
	/// <param name="position">設定する位置（const K4E::Vector3&）。</param>
	/// <param name="count">放出するパーティクルの数（uint32_t）。</param>
	void SetPositionAndEmit(K4E::GpuParticleEmitter* emitter, const K4E::Vector3& position, uint32_t count);

protected: /// ---------- メンバ変数 ---------- ///

	K4E::GpuParticleManager* manager_ = nullptr; // GPUパーティクルマネージャへのポインタ
	std::string prefix_;              // 名前の接頭辞
};

