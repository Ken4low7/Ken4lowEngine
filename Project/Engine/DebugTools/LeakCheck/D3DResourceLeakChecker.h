#pragma once

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///			　Direct3Dリソースのリークをチェックするクラス
/// -------------------------------------------------------------
class D3DResourceLeakChecker
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// デストラクタ
	/// </summary>
	~D3DResourceLeakChecker();

	/// <summary>
	/// 現時点のD3D/DXGIライブオブジェクトを報告します。
	/// </summary>
	static void ReportLiveObjects();

private:
	static bool hasReported_;
};


} // namespace Ken4lowEngine
