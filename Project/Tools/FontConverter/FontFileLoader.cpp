#include "FontFileLoader.h"
#include "FontFileStream.h"

#include <Windows.h>

#include <string>

HRESULT FontFileLoader::QueryInterface(REFIID iid, void** ppvObject)
{
	// COMの規約として、出力先ポインタが無効な場合はエラーにする。
	if (ppvObject == nullptr)
	{
		return E_INVALIDARG;
	}

	*ppvObject = nullptr;

	// このクラスが対応しているインターフェースだけを返す。
	if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteFontFileLoader))
	{
		*ppvObject = static_cast<IDWriteFontFileLoader*>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG FontFileLoader::AddRef()
{
	// COMオブジェクトなので、スレッドセーフに参照カウントを増やす。
	return InterlockedIncrement(reinterpret_cast<LONG*>(&refCount_));
}

ULONG FontFileLoader::Release()
{
	// 参照がなくなったタイミングで自分自身を破棄する。
	const ULONG count = InterlockedDecrement(reinterpret_cast<LONG*>(&refCount_));
	if (count == 0)
	{
		delete this;
	}
	return count;
}

HRESULT FontFileLoader::CreateStreamFromKey(void const* fontFileReferenceKey, UINT32 fontFileReferenceKeySize, IDWriteFontFileStream** fontFileStream)
{
	// DirectWriteから渡されるキーと出力先が有効か確認する。
	if (fontFileReferenceKey == nullptr || fontFileStream == nullptr)
	{
		return E_INVALIDARG;
	}

	*fontFileStream = nullptr;

	// キーは null 終端込みの wchar_t 文字列として渡しているため、サイズの整合性を確認する。
	if (fontFileReferenceKeySize < sizeof(wchar_t) || (fontFileReferenceKeySize % sizeof(wchar_t)) != 0)
	{
		return E_INVALIDARG;
	}

	const wchar_t* keyChars = static_cast<const wchar_t*>(fontFileReferenceKey);
	const size_t charCount = (fontFileReferenceKeySize / sizeof(wchar_t)) - 1;

	// CreateCustomFontFileReferenceで渡したキーを、フォントファイルパスへ復元する。
	std::wstring filePath(keyChars, keyChars + charCount);

	// 復元したパスからフォントファイルの読み取りストリームを作成する。
	FontFileStream* stream = new FontFileStream(filePath);
	if (!stream->IsLoaded())
	{
		stream->Release();
		return E_FAIL;
	}

	*fontFileStream = stream;
	return S_OK;
}