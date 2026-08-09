#pragma once

namespace Ken4lowEngine
{
	class LightManager;

	/// <summary>
	/// LightManagerの保存対象ライト設定をParameterManagerへ接続する補助クラスです。<br/>
	/// LightManager本体はライト管理・GPU転送・Bind・Shadow行列生成に集中し、<br/>
	/// このクラスがParameterManagerへの登録、保存済みJSONの読み込み、値の反映、同期、解除を担当します。
	/// </summary>
	class LightParameterController
	{
	public:
		/// <summary>
		/// LightManagerと接続し、ParameterManagerへ登録する準備を行います。
		/// </summary>
		void Initialize(LightManager* lightManager);

		/// <summary>
		/// ParameterManagerとの接続を解除します。
		/// </summary>
		void Finalize();

		/// <summary>
		/// LightManagerの保存対象値をParameterManagerへ登録します。
		/// </summary>
		void RegisterParameters();

		/// <summary>
		/// ParameterManager上の値をLightManagerへ反映します。
		/// </summary>
		void ApplyParameters();

		/// <summary>
		/// LightManagerの現在値をParameterManager側へ同期します。
		/// </summary>
		void SyncFromCurrentState();

	private:
		LightManager* lightManager_ = nullptr;
		bool registered_ = false;
	};
} // namespace Ken4lowEngine
