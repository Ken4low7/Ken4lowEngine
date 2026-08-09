#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// PostEffectの実行順序と有効判定だけを管理する薄いチェーン定義です。<br/>
	/// RenderTarget、BackBuffer、ResourceBarrier、CommandListには触れず、将来Bloom/ToneMapping/SSRなどを
	/// 順序付きで追加しやすくするために実行順管理だけを分離します。
	/// </summary>
	class PostEffectChain
	{
	public:
		/// <summary>
		/// 登録済みEffect順序を初期化します。<br/>
		/// PostEffectManager::Finalizeや再初期化時に、古い順序情報を残さないために呼び出します。
		/// </summary>
		void Clear();

		/// <summary>
		/// Effect名と実行順序を登録します。<br/>
		/// 既存のeffectTableに書かれているorder値をそのまま受け取り、従来の適用順を維持します。
		/// </summary>
		void RegisterEffect(const std::string& name, int order);

		/// <summary>
		/// 登録済みEffect名をorder昇順で返します。<br/>
		/// 有効判定はPostEffectRuntimeState、ApplyはPostEffectExecutorが担当します。
		/// </summary>
		std::vector<std::string> GetOrderedEffectNames() const;

	private:
		std::vector<std::pair<std::string, int>> effectOrder_; // Effect名と適用順だけを保持し、描画リソースは所有しない。
	};
}
