#pragma once

#include <dwrite.h>

/// <summary>
/// DirectWriteへ任意のフォントファイルパスを渡すための独自フォントローダー
/// CreateStreamFromKey() で受け取ったファイルパスから FontFileStream を生成する。
/// </summary>
class FontFileLoader : public IDWriteFontFileLoader
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// コンストラクタ
	/// </summary>
	FontFileLoader() = default;

	// IUnknown
	/// <summary>
	/// COMオブジェクトとして要求されたインターフェースを返す。
	/// </summary>
	IFACEMETHOD(QueryInterface)(REFIID iid, void** ppvObject) override;

	/// <summary>
	/// 参照カウントを増やす。
	/// </summary>
	IFACEMETHOD_(ULONG, AddRef)() override;

	/// <summary>
	/// 参照カウントを減らし、0になったら自身を破棄
	/// </summary>
	IFACEMETHOD_(ULONG, Release)() override;

	// IDWriteFontFileLoader
	/// <summary>
	/// DirectWriteから渡されたキーをフォントファイルパスとして解釈し、読み取り用ストリームを作成
	/// </summary>
	IFACEMETHOD(CreateStreamFromKey)(void const* fontFileReferenceKey, UINT32 fontFileReferenceKeySize, IDWriteFontFileStream** fontFileStream) override;

private: /// ---------- デストラクタ ---------- ///

	~FontFileLoader() = default;

private: /// ---------- メンバ変数 ---------- ///

	// <summary>COM形式の手動参照カウント
	ULONG refCount_ = 1;
};
