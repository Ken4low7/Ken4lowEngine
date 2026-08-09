#pragma once
#include <string>
#include <DirectXTex.h>

/// -------------------------------------------------------------
///					　	テクスチャコンバータ
/// -------------------------------------------------------------
class TextureConverter
{
public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	TextureConverter() = default;
	// デストラクタ
	~TextureConverter() = default;

	/// <summary>
	/// テクスチャをWIC形式からDDS形式に変換する
	/// </summary>
	/// <param name="filePath">ファイルパス</param>
	/// <param name="numOptions">オプションの数</param>
	/// <param name="options">オプションの配列</param>
	void ConvertTextureWICToDDS(const std::string& filePath, int numOptions = 0, char* options[] = nullptr);

public: /// ---------- 静的メンバ関数 ---------- ///

	/// <summary>
	/// 使用方法を出力する
	/// </summary>
	static void OoutputUsage();

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// テクスチャファイル読み込み(WIC形式)
	/// </summary>
	/// <param name="filePath"></param>
	void LoadWICTextureFromFile(const std::string& filePath);

	/// <summary>
	/// マルチバイト文字列をワイド文字列に変換
	/// </summary>
	// アルベドはsRGB、非カラーは-linearでLinear/UNORMとして扱う。
	bool outputSRGB_ = true;

	/// <param name="multiByteString">マルチバイト文字列</param>
	/// <returns></returns>
	static std::wstring ConcertMultiByteStringToWideString(const std::string& multiByteString);

	/// <summary>
	/// フォルダパスとファイル名を分離する
	/// </summary>
	/// <param name="filePath">ファイルパス</param>
	void SeparateFilePath(const std::wstring& filePath);

	/// <summary>
	/// DDSテクスチャとしてファイル書き出し
	/// </summary>
	/// <param name="numOptions">オプションの数</param>
	/// <param name="options">オプションの配列</param>
	void SaveDDSTextureToFile(int numOptions, char* options[]);

private: /// ---------- メンバ変数 ---------- ///

	// 画像の情報
	DirectX::TexMetadata   metadate_;

	// 画像データ本体
	DirectX::ScratchImage scrachImage_;

private: /// ---------- ファイルパス分解用変数 ---------- ///

	// ディレクトリ
	std::wstring directoryPath_;

	// ファイル名
	std::wstring fileName_;

	// 拡張子
	std::wstring extension_;
};

