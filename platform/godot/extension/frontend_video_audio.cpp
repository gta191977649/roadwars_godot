#include "platform/godot/extension/frontend_video_audio.hpp"
#include <exception>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Windows multimedia headers require the base declarations in this order.
// clang-format off
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
// clang-format on
#endif

namespace roadwars {
Result decode_frontend_video_audio(const FrontendVideoAudio& source, WavePcm& output,
                                   std::string& error) {
    error.clear();
    if (source.format_tag != 0x31 || source.channels != 1 || source.block_alignment != 65 ||
        source.bits_per_sample != 0 || !source.sample_rate || source.sample_rate > 192000 ||
        source.encoded.empty() || source.encoded.size() % 65 ||
        source.encoded.size() > 8 * 1024 * 1024) {
        error = "Invalid frontend GSM 6.10 audio format or block length";
        return Result::invalid_argument;
    }
#ifdef _WIN32
    try {
        // GSM 6.10 WAV packs two 160-sample speech frames into each 65-byte block.
        GSM610WAVEFORMAT compressed{};
        compressed.wfx.wFormatTag = WAVE_FORMAT_GSM610;
        compressed.wfx.nChannels = source.channels;
        compressed.wfx.nSamplesPerSec = source.sample_rate;
        compressed.wfx.nAvgBytesPerSec = source.sample_rate * 65 / 320;
        compressed.wfx.nBlockAlign = source.block_alignment;
        compressed.wfx.wBitsPerSample = source.bits_per_sample;
        compressed.wfx.cbSize = sizeof(compressed.wSamplesPerBlock);
        compressed.wSamplesPerBlock = 320;
        WAVEFORMATEX pcm{};
        pcm.wFormatTag = WAVE_FORMAT_PCM;
        pcm.nChannels = 1;
        pcm.nSamplesPerSec = source.sample_rate;
        pcm.nAvgBytesPerSec = source.sample_rate * 2;
        pcm.nBlockAlign = 2;
        pcm.wBitsPerSample = 16;
        struct Stream {
            HACMSTREAM handle{};
            ~Stream() {
                if (handle)
                    acmStreamClose(handle, 0);
            }
        } stream;
        auto status = acmStreamOpen(&stream.handle, nullptr, &compressed.wfx, &pcm, nullptr, 0, 0,
                                    ACM_STREAMOPENF_NONREALTIME);
        if (status != MMSYSERR_NOERROR) {
            error = "Frontend GSM 6.10 decoder unavailable (ACM " + std::to_string(status) + ")";
            return Result::unavailable;
        }
        WavePcm decoded;
        decoded.channels = 1;
        decoded.bits_per_sample = 16;
        decoded.sample_rate = source.sample_rate;
        decoded.data.resize(source.encoded.size() / 65 * 640);
        ACMSTREAMHEADER header{};
        header.cbStruct = sizeof(header);
        header.pbSrc = reinterpret_cast<LPBYTE>(const_cast<std::byte*>(source.encoded.data()));
        header.cbSrcLength = static_cast<DWORD>(source.encoded.size());
        header.pbDst = reinterpret_cast<LPBYTE>(decoded.data.data());
        header.cbDstLength = static_cast<DWORD>(decoded.data.size());
        status = acmStreamPrepareHeader(stream.handle, &header, 0);
        if (status != MMSYSERR_NOERROR) {
            error = "Frontend GSM stream preparation failed (ACM " + std::to_string(status) + ")";
            return Result::unavailable;
        }
        const auto converted = acmStreamConvert(stream.handle, &header,
                                                ACM_STREAMCONVERTF_START | ACM_STREAMCONVERTF_END |
                                                    ACM_STREAMCONVERTF_BLOCKALIGN);
        const auto unprepared = acmStreamUnprepareHeader(stream.handle, &header, 0);
        if (converted != MMSYSERR_NOERROR || unprepared != MMSYSERR_NOERROR ||
            header.cbSrcLengthUsed != header.cbSrcLength ||
            header.cbDstLengthUsed != header.cbDstLength) {
            error = "Frontend GSM stream conversion failed or returned incomplete PCM";
            return Result::unavailable;
        }
        output = std::move(decoded);
        return Result::ok;
    } catch (const std::exception& exception) {
        error = std::string("Frontend GSM conversion failed: ") + exception.what();
        return Result::unavailable;
    }
#else
    (void)output;
    error = "Frontend GSM 6.10 conversion requires a host codec adapter on this platform";
    return Result::unavailable;
#endif
}
} // namespace roadwars
