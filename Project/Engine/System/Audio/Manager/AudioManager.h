#pragma once
#include "AudioCategory.h"
#include "MFAudioDecoder.h"

#include <xaudio2.h>
#include <wrl/client.h>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <string>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///						音声再生全体を管理するクラス
	/// -------------------------------------------------------------
	/// ・Media Foundation を使った音声デコード
	/// ・XAudio2 を使った BGM / SE / Voice の再生
	/// ・再利用のためのデコード済みクリップのキャッシュ
	/// ・再生終了したワンショット Voice の回収をまとめて担当する。
	class AudioManager
	{
	public:
		using AudioHandle = uint64_t;
		static constexpr AudioHandle InvalidAudioHandle = 0;

	private: /// ---------- 内部構造体 ---------- ///

		/// <summary>
		/// デコード済み音声データを保持するキャッシュ単位。
		/// 同じ音声ファイルを何度も再デコードしないために使う。
		/// </summary>
		struct CachedClip
		{
			DecodedAudioData data;
		};

		/// <summary>
		/// 再生中のワンショット Voice を管理するための情報。
		/// clip を保持しておくことで、再生中に PCM バッファが解放されないようにする。
		/// </summary>
		struct ActiveVoice
		{
			IXAudio2SourceVoice* voice = nullptr;
			std::shared_ptr<CachedClip> clip;
			AudioHandle handle = InvalidAudioHandle;
			AudioCategory category = AudioCategory::SE;
			float baseVolume = 1.0f;
			float pan = 0.0f;
			float pitch = 1.0f;
			bool loop = false;
		};

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// シングルトンインスタンスを取得する。
		/// エンジン全体で音声管理を一元化するため、インスタンスは 1 つだけにする。
		/// </summary>
		static AudioManager* GetInstance();

		/// <summary>
		/// 音声システムを初期化する。
		/// Media Foundation と XAudio2 を起動し、再生の土台を作る。
		/// 起動時に一度呼べば十分だが、未初期化時は再生関数から遅延初期化も行う。
		/// </summary>
		bool Initialize();

		/// <summary>
		/// 音声システムを終了する。
		/// 再生中の Voice、キャッシュ、XAudio2、Media Foundation を順に解放する。
		/// </summary>
		void Finalize();

		/// <summary>
		/// 毎フレーム呼ぶ更新処理。
		/// ワンショット再生が終わった Voice を回収して、不要なリソースを解放する。
		/// </summary>
		void Update();

		/// <summary>
		/// BGM を再生する。
		/// すでに別の BGM が鳴っている場合は停止してから差し替える。
		/// </summary>
		/// <param name="filePath">再生する音声ファイルパス。ファイル名のみの場合は Resources/Sounds 配下として扱う。</param>
		/// <param name="volume">個別音量。カテゴリ音量と掛け合わせた値が最終音量になる。</param>
		/// <param name="pitch">再生ピッチ。1.0f で等速。</param>
		/// <param name="loop">true の場合はループ再生する。</param>
		void PlayBGM(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

		/// <summary>
		/// 効果音をワンショット再生する。
		/// </summary>
		void PlaySE(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

		/// <summary>
		/// 効果音を再生し、後から停止や音量変更に使えるハンドルを返す。
		/// </summary>
		AudioHandle PlaySEWithHandle(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

		/// <summary>
		/// ボイスをワンショット再生する。
		/// </summary>
		void PlayVoice(const std::string& filePath, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

		/// <summary>
		/// 指定したハンドルの音声を停止する。
		/// </summary>
		void Stop(AudioHandle handle);

		/// <summary>
		/// 指定したハンドルの音声が再生中か確認する。
		/// </summary>
		bool IsPlaying(AudioHandle handle) const;

		/// <summary>
		/// 指定したハンドルの音量を更新する。
		/// </summary>
		void SetVoiceVolume(AudioHandle handle, float volume);

		/// <summary>
		/// 指定したハンドルの左右パンを更新する。
		/// </summary>
		void SetVoicePan(AudioHandle handle, float pan);

		/// <summary>
		/// 指定したハンドルの再生ピッチを更新する。
		/// </summary>
		void SetVoicePitch(AudioHandle handle, float pitch);

		/// <summary>
		/// 現在再生中の BGM を停止する。
		/// </summary>
		void StopBGM();

		/// <summary>
		/// 現在再生中の BGM を一時停止する。
		/// </summary>
		void PauseBGM();

		/// <summary>
		/// 一時停止中の BGM を再開する。
		/// </summary>
		void ResumeBGM();

		/// <summary>
		/// カテゴリごとの音量を設定する。
		/// BGM / SE / Voice ごとにマスターボリュームのような役割を持つ。
		/// </summary>
		void SetCategoryVolume(AudioCategory category, float volume);

		/// <summary>
		/// カテゴリごとの音量を取得する。
		/// </summary>
		float GetCategoryVolume(AudioCategory category) const;

	private: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// 未初期化なら初期化を試みる。
		/// </summary>
		bool EnsureInitialized();

		/// <summary>
		/// 音声ファイルパスを正規化する。
		/// フォルダ指定がない場合は Resources/Sounds 配下として扱う。
		/// </summary>
		std::string NormalizePath(const std::string& filePath) const;

		/// <summary>
		/// 呼び出し側の音量とカテゴリ音量を掛け合わせ、実際の再生音量を作る。
		/// </summary>
		float BuildActualVolume(AudioCategory category, float volume) const;

		/// <summary>
		/// SourceVoiceへ左右パンを反映する。
		/// </summary>
		void ApplyVoicePan(ActiveVoice& active);

		/// <summary>
		/// 音声ファイルを読み込み、デコード済みクリップを取得する。
		/// すでにキャッシュ済みなら再利用する。
		/// </summary>
		std::shared_ptr<CachedClip> LoadClip(const std::string& filePath);

		/// <summary>
		/// クリップのフォーマット情報から SourceVoice を作成する。
		/// </summary>
		IXAudio2SourceVoice* CreateVoiceForClip(const CachedClip& clip) const;

		/// <summary>
		/// SE / Voice などのワンショット再生を行う共通処理。
		/// </summary>
		AudioHandle PlayOneShot(AudioCategory category, const std::string& filePath, float volume, float pitch, bool loop);

		/// <summary>
		/// SourceVoice を安全に停止・破棄する。
		/// </summary>
		void DestroyVoice(IXAudio2SourceVoice*& voice);

		/// <summary>
		/// 再生終了したワンショット Voice を回収する。
		/// </summary>
		void CleanupFinishedVoices();

	private: /// ---------- メンバ変数 ---------- ///

		/// 初期化が完了しているかどうか
		bool initialized_ = false;

		/// カテゴリごとの音量
		float categoryVolumes_[static_cast<int>(AudioCategory::Count)] = { 1.0f, 1.0f, 1.0f };

		/// XAudio2 本体
		Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;

		/// 最終出力先となるマスターボイス
		IXAudio2MasteringVoice* masteringVoice_ = nullptr;

		/// デコード済みクリップのキャッシュ
		std::unordered_map<std::string, std::weak_ptr<CachedClip>> cache_;

		/// 再生中のワンショット Voice 一覧
		std::vector<ActiveVoice> activeVoices_;

		/// AudioComponentなどから個別制御するための再生ハンドル
		AudioHandle nextAudioHandle_ = 1;

		/// BGM 専用 Voice
		IXAudio2SourceVoice* bgmVoice_ = nullptr;

		/// BGM で使用中のクリップ
		std::shared_ptr<CachedClip> bgmClip_;

		/// BGM 再生開始時に指定された個別音量
		/// カテゴリ音量変更時に、個別音量を失わず再計算するため保持しておく
		float bgmBaseVolume_ = 1.0f;

		/// BGM が一時停止中かどうか
		bool bgmPaused_ = false;

		/// 音声操作全体を守るミューテックス
		mutable std::mutex mutex_;

	private: /// ---------- シングルトンインスタンス管理 ---------- ///

		AudioManager() = default;
		~AudioManager() = default;
		AudioManager(const AudioManager&) = delete;
		AudioManager& operator=(const AudioManager&) = delete;
	};

} // namespace Ken4lowEngine
