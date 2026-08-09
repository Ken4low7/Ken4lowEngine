#include "FontFileStream.h"

#include <Windows.h>
#include <fstream>

FontFileStream::FontFileStream(const std::wstring& filePath)
{
	// ファイルサイズを先に取得するため、末尾位置で開く。
	std::ifstream ifs(filePath, std::ios::binary | std::ios::ate);
	if (!ifs)
	{
		return;
	}

	// tellg()でファイル全体のバイト数を取得する。
	const std::streamsize size = ifs.tellg();
	if (size <= 0)
	{
		return;
	}

	ifs.seekg(0, std::ios::beg);

	// DirectWriteからの部分読み出しに対応するため、フォントファイル全体をメモリに保持する。
	fileData_.resize(static_cast<size_t>(size));
	ifs.read(reinterpret_cast<char*>(fileData_.data()), size);
}

HRESULT FontFileStream::QueryInterface(REFIID iid, void** ppvObject)
{
	// COMの規約として、出力先ポインタが無効な場合はエラーにする。
	if (ppvObject == nullptr)
	{
		return E_INVALIDARG;
	}

	*ppvObject = nullptr;

	// このクラスが対応しているインターフェースだけを返す。
	if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontFileStream))
	{
		*ppvObject = static_cast<IDWriteFontFileStream*>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG FontFileStream::AddRef()
{
	// COMオブジェクトなので、スレッドセーフに参照カウントを増やす。
	return InterlockedIncrement(reinterpret_cast<LONG*>(&refCount_));
}

ULONG FontFileStream::Release()
{
	// 参照がなくなったタイミングで自分自身を破棄する。
	const ULONG count = InterlockedDecrement(reinterpret_cast<LONG*>(&refCount_));
	if (count == 0)
	{
		delete this;
	}
	return count;
}

HRESULT FontFileStream::ReadFileFragment(
	void const** fragmentStart,
	UINT64 fileOffset,
	UINT64 fragmentSize,
	void** fragmentContext
)
{
	// DirectWriteへ返す出力先が有効か確認する。
	if (fragmentStart == nullptr || fragmentContext == nullptr)
	{
		return E_INVALIDARG;
	}

	*fragmentStart = nullptr;
	*fragmentContext = nullptr;

	// 要求範囲が読み込んだファイルサイズを超える場合は失敗にする。
	if (fileOffset + fragmentSize > fileData_.size())
	{
		return E_FAIL;
	}

	// fileData_内の要求位置を直接返す。メモリ所有権はこのクラスが保持する。
	*fragmentStart = fileData_.data() + static_cast<size_t>(fileOffset);
	return S_OK;
}

void FontFileStream::ReleaseFileFragment(void* fragmentContext)
{
	// fileData_全体を保持しているため、フラグメント単位の解放処理は不要。
	(void)fragmentContext;
}

HRESULT FontFileStream::GetFileSize(UINT64* fileSize)
{
	if (fileSize == nullptr)
	{
		return E_INVALIDARG;
	}

	// DirectWriteへ、読み込んだフォントファイル全体のサイズを返す。
	*fileSize = static_cast<UINT64>(fileData_.size());
	return S_OK;
}

HRESULT FontFileStream::GetLastWriteTime(UINT64* lastWriteTime)
{
	if (lastWriteTime == nullptr)
	{
		return E_INVALIDARG;
	}

	// 最終更新時刻は変換処理に不要なため、未実装として返す。
	*lastWriteTime = 0;
	return E_NOTIMPL;
}