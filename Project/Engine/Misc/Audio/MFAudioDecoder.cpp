#include "MFAudioDecoder.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <propvarutil.h>
#include <comdef.h>
#include <vector>
#include <string>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "propsys.lib")

namespace Ken4lowEngine
{
    namespace
    {
        /// <summary>
        /// UTF-8 の std::string を Media Foundation 用の UTF-16 文字列へ変換する。
        /// </summary>
        std::wstring ToWide(const std::string& s)
        {
            if (s.empty()) return {};

            const int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
            if (size <= 0)
            {
                // UTF-8 変換に失敗した場合の最低限のフォールバック
                return std::wstring(s.begin(), s.end());
            }

            std::wstring out(static_cast<size_t>(size - 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), size);
            return out;
        }

        /// <summary>
        /// SourceReader から現在の音声フォーマット情報を取得し、
        /// XAudio2 の SourceVoice 作成に使える WAVEFORMATEX を構築する。
        /// </summary>
        bool ReadCurrentAudioFormat(IMFSourceReader* reader, WAVEFORMATEX& outFormat)
        {
            Microsoft::WRL::ComPtr<IMFMediaType> currentType;
            HRESULT hr = reader->GetCurrentMediaType(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
                &currentType);

            if (FAILED(hr) || !currentType)
            {
                return false;
            }

            UINT32 channels = 0;
            UINT32 sampleRate = 0;
            UINT32 bitsPerSample = 0;
            UINT32 blockAlign = 0;
            UINT32 avgBytesPerSec = 0;

            // 音声の基本情報を取得する
            if (FAILED(currentType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels))) return false;
            if (FAILED(currentType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate))) return false;
            if (FAILED(currentType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample))) return false;

            // 一部環境では取得できない場合があるため、計算で補完する
            if (FAILED(currentType->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, &blockAlign)))
            {
                blockAlign = channels * (bitsPerSample / 8);
            }

            if (FAILED(currentType->GetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &avgBytesPerSec)))
            {
                avgBytesPerSec = sampleRate * blockAlign;
            }

            // XAudio2 に渡すための WAVEFORMATEX を構築する
            outFormat = {};
            outFormat.wFormatTag = WAVE_FORMAT_PCM;
            outFormat.nChannels = static_cast<WORD>(channels);
            outFormat.nSamplesPerSec = sampleRate;
            outFormat.wBitsPerSample = static_cast<WORD>(bitsPerSample);
            outFormat.nBlockAlign = static_cast<WORD>(blockAlign);
            outFormat.nAvgBytesPerSec = avgBytesPerSec;
            outFormat.cbSize = 0;

            return true;
        }
    }

    bool MFAudioDecoder::DecodeFile(const std::string& filePath, DecodedAudioData& outData)
    {
        // 前回のデータが残らないよう、出力先を初期化する
        outData = {};

        // Media Foundation API に渡すため UTF-16 文字列へ変換する
        const std::wstring widePath = ToWide(filePath);
        if (widePath.empty())
        {
            OutputDebugStringA("MFAudioDecoder: empty path\n");
            return false;
        }

        // ファイルから音声ストリームを読み取る SourceReader を作成する
        Microsoft::WRL::ComPtr<IMFSourceReader> reader;
        HRESULT hr = MFCreateSourceReaderFromURL(widePath.c_str(), nullptr, &reader);
        if (FAILED(hr) || !reader)
        {
            _com_error err(hr);
            OutputDebugStringW((std::wstring(L"MFCreateSourceReaderFromURL failed: ") + err.ErrorMessage() + L"\n").c_str());
            return false;
        }

        // 出力形式として PCM 16bit を要求する
        Microsoft::WRL::ComPtr<IMFMediaType> outputType;
        hr = MFCreateMediaType(&outputType);
        if (FAILED(hr) || !outputType) return false;

        hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (FAILED(hr)) return false;

        hr = outputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        if (FAILED(hr)) return false;

        hr = outputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        if (FAILED(hr)) return false;

        // SourceReader の出力を PCM 16bit に切り替える
        hr = reader->SetCurrentMediaType(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
            nullptr,
            outputType.Get());
        if (FAILED(hr))
        {
            _com_error err(hr);
            OutputDebugStringW((std::wstring(L"SetCurrentMediaType failed: ") + err.ErrorMessage() + L"\n").c_str());
            return false;
        }

        // デコード後の波形フォーマットを取得する
        if (!ReadCurrentAudioFormat(reader.Get(), outData.waveFormat))
        {
            OutputDebugStringA("MFAudioDecoder: failed to read current audio format\n");
            return false;
        }

        outData.pcmData.clear();

        // ファイル終端に達するまでサンプルを読み出し、PCM バッファへ連結していく
        while (true)
        {
            DWORD streamIndex = 0;
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            Microsoft::WRL::ComPtr<IMFSample> sample;

            hr = reader->ReadSample(
                static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
                0,
                &streamIndex,
                &flags,
                &timestamp,
                &sample);

            if (FAILED(hr))
            {
                _com_error err(hr);
                OutputDebugStringW((std::wstring(L"ReadSample failed: ") + err.ErrorMessage() + L"\n").c_str());
                return false;
            }

            // 終端に到達したら読み込み終了
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                break;
            }

            // サンプルが空のときは次を読む
            if (!sample)
            {
                continue;
            }

            // 扱いやすい連続バッファへ変換する
            Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
            hr = sample->ConvertToContiguousBuffer(&mediaBuffer);
            if (FAILED(hr) || !mediaBuffer)
            {
                return false;
            }

            BYTE* audioData = nullptr;
            DWORD maxLen = 0;
            DWORD currentLen = 0;

            // バッファをロックして PCM データを取り出す
            hr = mediaBuffer->Lock(&audioData, &maxLen, &currentLen);
            if (FAILED(hr))
            {
                return false;
            }

            // 読み出した PCM データを末尾へ追加する
            outData.pcmData.insert(outData.pcmData.end(), audioData, audioData + currentLen);

            // ロックは必ず解除する
            mediaBuffer->Unlock();
        }

        // 1 バイトも読めなかった場合は失敗扱い
        return !outData.pcmData.empty();
    }

} // namespace Ken4lowEngine