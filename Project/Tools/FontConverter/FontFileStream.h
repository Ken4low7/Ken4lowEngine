#pragma once

#include <dwrite.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>

/// <summary>
/// DirectWriteがフォントファイルを読み込むためのストリームクラス
/// ファイル全体をメモリへ読み込み、ReadFileFragment() でDirectWriteに必要範囲のポインタを返す。
/// </summary>
class FontFileStream : public IDWriteFontFileStream
{
private: /// ---------- デストラクタ ---------- ///

	~FontFileStream() = default;

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定されたフォントファイルをバイナリとして読み込み、メモリ上に保持する。
	/// </summary>
	explicit FontFileStream(const std::wstring& filePath);

	/// <summary>
	/// ファイル読み込みに成功しているかを返す。
	/// </summary>
	bool IsLoaded() const { return !fileData_.empty(); }

	/// <summary>
	/// COMオブジェクトとして要求されたインターフェースを返す。
	/// </summary>
	IFACEMETHOD(QueryInterface)(REFIID iid, void** ppvObject) override;

	/// <summary>
	/// 参照カウントを増やす。
	/// </summary>
	IFACEMETHOD_(ULONG, AddRef)() override;

	/// <summary>
	/// 参照カウントを減らし、0になったら自身を破棄する。
	/// </summary>
	IFACEMETHOD_(ULONG, Release)() override;

	/// <summary>
	/// DirectWriteから要求されたファイル範囲の先頭ポインタを返す。
	/// </summary>
	IFACEMETHOD(ReadFileFragment)(void const** fragmentStart, UINT64 fileOffset, UINT64 fragmentSize, void** fragmentContext) override;

	/// <summary>
	/// ReadFileFragment() で返した範囲の解放通知です。
	/// fileData_の所有権はこのクラスが持つため、個別解放は行いません。
	/// </summary>
	IFACEMETHOD_(void, ReleaseFileFragment)(void* fragmentContext) override;

	/// <summary>
	/// 読み込んだフォントファイル全体のサイズを返す。
	/// </summary>
	IFACEMETHOD(GetFileSize)(UINT64* fileSize) override;

	/// <summary>
	/// 最終更新時刻を返す。現在は未実装として扱う
	/// </summary>
	IFACEMETHOD(GetLastWriteTime)(UINT64* lastWriteTime) override;

private: /// ---------- メンバ変数 ---------- ///

	// COM形式の手動参照カウント
	ULONG refCount_ = 1;

	// 読み込んだフォントファイルの全バイト列
	std::vector<std::uint8_t> fileData_;
};
