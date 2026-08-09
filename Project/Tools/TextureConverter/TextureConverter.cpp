#include "TextureConverter.h"
#include <iostream>
#include <windows.h>

using namespace std;
// DirectX名前空間を使用
using namespace DirectX;

/// -------------------------------------------------------------
///			テクスチャをWIC形式からDDS形式に変換する処理
/// -------------------------------------------------------------
void TextureConverter::ConvertTextureWICToDDS(const std::string& filePath, int numOptions, char* options[])
{
	outputSRGB_ = true;
	for (int i = 0; i < numOptions; ++i)
	{
		if (std::string(options[i]) == "-linear")
		{
			outputSRGB_ = false;
		}
	}

	// WIC形式のテクスチャを用途に応じた色空間で読み込む
	LoadWICTextureFromFile(filePath);

	// DDS形式に変換して保存する処理をここに追加する
	SaveDDSTextureToFile(numOptions, options);
}

/// -------------------------------------------------------------
///					使用方法を出力する処理
/// -------------------------------------------------------------
void TextureConverter::OoutputUsage()
{
	cout << "画像ファイルをWIC形式からDDS形式に変換します。" << endl;
	cout << endl; // 空行
	cout << "TextureConverter [ドライブ:][パス][ファイル名]" << endl;
	cout << endl; // 空行
	cout << " [ドライブ:][パス][ファイル名]: 変換するWIC形式の画像ファイルのパスを指定します。" << endl;
	cout << endl; // 空行
	cout << " [-ml level]: ミップレベルを指定します。0を指定すると1x1までのフルミップマップチェーンを生成します。" << endl;
	cout << " [-linear]: 法線/Roughness等の非カラーテクスチャをUNORM DDSとして保存します。" << endl;
	cout << " [-nomip]: PixelArt向けにミップマップを生成せずに保存します。" << endl;
}

/// -------------------------------------------------------------
///				テクスチャファイル読み込み(WIC形式)
/// -------------------------------------------------------------
void TextureConverter::LoadWICTextureFromFile(const std::string& filePath)
{
	// ファイルパスをワイド文字列に変換
	std::wstring wideFilePath = ConcertMultiByteStringToWideString(filePath);

	// アルベドPNGはsRGBとして読み込み、非カラーTextureは-linear指定で通常UNORMとして読む。
	const WIC_FLAGS wicFlags = outputSRGB_ ? WIC_FLAGS_FORCE_SRGB : WIC_FLAGS_NONE;
	HRESULT hr = LoadFromWICFile(wideFilePath.c_str(), wicFlags, &metadate_, scrachImage_);
	assert(SUCCEEDED(hr) && "WIC形式のテクスチャの読み込みに失敗しました。");

#ifdef _DEBUG
	// 入力時点のsRGB判定を出してPNG→DDS変換の色空間を確認しやすくする。
	std::wcout << L"[TextureConverter] Input PNG/WIC: " << wideFilePath
		<< L" format=" << static_cast<int>(metadate_.format)
		<< L" size=" << metadate_.width << L"x" << metadate_.height
		<< L" mipLevels=" << metadate_.mipLevels
		<< L" srgb=" << (DirectX::IsSRGB(metadate_.format) ? L"true" : L"false")
		<< L" alphaMode=" << static_cast<int>(metadate_.GetAlphaMode()) << std::endl;
#endif

	// フォルダパスとファイル名を分離する
	SeparateFilePath(wideFilePath);
}

/// -------------------------------------------------------------
///			　マルチバイト文字列をワイド文字列に変換
/// -------------------------------------------------------------
std::wstring TextureConverter::ConcertMultiByteStringToWideString(const std::string& multiByteString)
{
	// ワイド文字列に変換した際の文字数を計算
	int filePathBufferSize = MultiByteToWideChar(CP_ACP, 0, multiByteString.c_str(), -1, nullptr, 0);

	// ワイド文字列用のバッファを確保
	std::wstring wideString;
	wideString.resize(filePathBufferSize);

	// マルチバイト文字列をワイド文字列に変換
	MultiByteToWideChar(CP_ACP, 0, multiByteString.c_str(), -1, wideString.data(), filePathBufferSize);

	// ワイド文字列を返す
	return wideString;
}

/// -------------------------------------------------------------
///		　		フォルダパスとファイル名を分離する
/// -------------------------------------------------------------
void TextureConverter::SeparateFilePath(const std::wstring& filePath)
{
	size_t position1 = 0;
	std::wstring exceptExit;

	// 区切り文字 '.' が出てくる一番最後の部分を検索
	position1 = filePath.rfind(L'.');

	// 検索がヒットしたら
	if (position1 != std::wstring::npos)
	{
		// 区切り文字の後ろをファイル拡張子として保存
		extension_ = filePath.substr(position1 + 1, filePath.size() - position1 - 1);

		// 区切り文字の前までを抜き出す
		exceptExit = filePath.substr(0, position1);
	}
	else
	{
		extension_ = L"";
		exceptExit = filePath;
	}

	// 区切り文字 '\\' が出てくる一番最後の部分を検索
	position1 = exceptExit.rfind(L'\\');

	// 検索がヒットしたら
	if (position1 != std::wstring::npos)
	{
		// 区切り文字の前までをフォルダパスとして保存
		directoryPath_ = exceptExit.substr(0, position1);

		// 区切り文字の後ろをファイル名として保存
		fileName_ = exceptExit.substr(position1 + 1, exceptExit.size() - position1 - 1);

		return;
	}

	// 区切り文字 '/' が出てくる一番最後の部分を検索
	position1 = exceptExit.rfind(L'/');

	// 検索がヒットしたら
	if (position1 != std::wstring::npos)
	{
		// 区切り文字の前までをフォルダパスとして保存
		directoryPath_ = exceptExit.substr(0, position1);

		// 区切り文字の後ろをファイル名として保存
		fileName_ = exceptExit.substr(position1 + 1, exceptExit.size() - position1 - 1);
		return;
	}

	// 区切り文字が見つからなかった場合、フォルダパスは空、ファイル名はそのまま保存
	directoryPath_ = L"";
	fileName_ = exceptExit;
}

/// -------------------------------------------------------------
///			 	 DDSテクスチャとしてファイル書き出し
/// -------------------------------------------------------------
void TextureConverter::SaveDDSTextureToFile(int numOptions, char* options[])
{
	HRESULT hr = S_FALSE;

	size_t mipLevel = 0;
	bool disableMipMap = false;

	// ミップ/色空間オプションを解析し、非カラーTextureは-linearでUNORM出力できるようにする。
	for (int i = 0; i < numOptions; i++)
	{
		const std::string option = options[i];
		if (option == "-ml" && i + 1 < numOptions)
		{
			// ミップレベル数を取得
			mipLevel = stoi(options[i + 1]);
			++i;
		}
		else if (option == "-linear")
		{
			outputSRGB_ = false;
		}
		else if (option == "-nomip")
		{
			disableMipMap = true;
		}
	}

	// 出力ファイル名を先に設定し、デバッグログでも同じパスを参照する。
	std::wstring filePath = directoryPath_ + L"\\" + fileName_ + L".dds";

	ScratchImage mipChain;
	if (!disableMipMap)
	{
		// PixelArt指定が無い通常テクスチャは従来どおりミップを生成する。
		hr = GenerateMipMaps(scrachImage_.GetImages(), scrachImage_.GetImageCount(), scrachImage_.GetMetadata(), TEX_FILTER_DEFAULT, mipLevel, mipChain);
	}
	else
	{
		hr = E_FAIL;
	}

	// ミップマップ生成に成功した場合
	if (!disableMipMap && SUCCEEDED(hr))
	{
#ifdef _DEBUG
	std::wcout << L"[TextureConverter] Output DDS: " << filePath
		<< L" format=" << static_cast<int>(metadate_.format)
		<< L" size=" << metadate_.width << L"x" << metadate_.height
		<< L" mipLevels=" << metadate_.mipLevels
		<< L" srgb=" << (DirectX::IsSRGB(metadate_.format) ? L"true" : L"false")
		<< L" alphaMode=" << static_cast<int>(metadate_.GetAlphaMode()) << std::endl;
#endif

	// ϊチ^f[^O DDS ƂĕۑAPNGDDS ǐՂB
		scrachImage_ = std::move(mipChain);
		metadate_ = scrachImage_.GetMetadata();
	}

	// 圧縮形式 : アルベドはBC7_SRGB、非カラーはBC7_UNORMに変換する。
	ScratchImage convertedImage;
	const DXGI_FORMAT outputFormat = outputSRGB_ ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
	TEX_COMPRESS_FLAGS compressFlags = static_cast<TEX_COMPRESS_FLAGS>(TEX_COMPRESS_BC7_QUICK | TEX_COMPRESS_PARALLEL);
	if (outputSRGB_)
	{
		compressFlags = static_cast<TEX_COMPRESS_FLAGS>(compressFlags | TEX_COMPRESS_SRGB_OUT);
	}
	hr = Compress(scrachImage_.GetImages(), scrachImage_.GetImageCount(), metadate_,
		outputFormat, compressFlags, 1.0f, convertedImage);

	// 圧縮変換に成功した場合
	if (SUCCEEDED(hr))
	{
		// イメージとメタデータを、圧縮後のものに置き換える
		scrachImage_ = std::move(convertedImage);
		metadate_ = scrachImage_.GetMetadata();
	}

	// 読み込んだテクスチャを用途に応じた形式で保存する
	metadate_.format = outputSRGB_ ? MakeSRGB(metadate_.format) : MakeLinear(metadate_.format);

	// DDS形式でファイルに保存する
	hr = SaveToDDSFile(scrachImage_.GetImages(), scrachImage_.GetImageCount(), metadate_, DDS_FLAGS_NONE, filePath.c_str());
	assert(SUCCEEDED(hr) && "DDS形式でのテクスチャの保存に失敗しました。");
}
