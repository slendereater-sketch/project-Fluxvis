#include "video_decoder.h"
#include <android/log.h>
#include <unistd.h>

#define TAG "VideoDecoder"

VideoDecoder::VideoDecoder() {}

VideoDecoder::~VideoDecoder() {
    Reset();
}

void VideoDecoder::Reset() {
    if (mCodec) {
        AMediaCodec_stop(mCodec);
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
    }
    if (mExtractor) {
        AMediaExtractor_delete(mExtractor);
        mExtractor = nullptr;
    }
    mSawInputEOS = false;
    mSawOutputEOS = false;
    mStarted = false;
    mFinished = false;
}

bool VideoDecoder::LoadVideo(const std::string& path) {
    Reset();
    mExtractor = AMediaExtractor_new();
    media_status_t err = AMediaExtractor_setDataSource(mExtractor, path.c_str());
    if (err != AMEDIA_OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Error setting data source: %d", err);
        return false;
    }

    int numTracks = AMediaExtractor_getTrackCount(mExtractor);
    for (int i = 0; i < numTracks; i++) {
        AMediaFormat* format = AMediaExtractor_getTrackFormat(mExtractor, i);
        const char* mime;
        if (!AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
            continue;
        }

        if (strncmp(mime, "video/", 6) == 0) {
            AMediaExtractor_selectTrack(mExtractor, i);
            mCodec = AMediaCodec_createDecoderByType(mime);
            AMediaCodec_configure(mCodec, format, mWindow, nullptr, 0);
            AMediaFormat_delete(format);
            break;
        }
        AMediaFormat_delete(format);
    }

    if (!mCodec) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "No video track found in %s", path.c_str());
        return false;
    }

    return true;
}

void VideoDecoder::SetOutputWindow(ANativeWindow* window) {
    mWindow = window;
}

bool VideoDecoder::Start() {
    if (!mCodec) return false;
    media_status_t err = AMediaCodec_start(mCodec);
    if (err != AMEDIA_OK) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Error starting codec: %d", err);
        return false;
    }
    mStarted = true;
    return true;
}

void VideoDecoder::Stop() {
    if (mCodec && mStarted) {
        AMediaCodec_stop(mCodec);
        mStarted = false;
    }
}

void VideoDecoder::Update() {
    if (!mStarted || mSawOutputEOS) return;

    // Feed input
    if (!mSawInputEOS) {
        ssize_t bufidx = AMediaCodec_dequeueInputBuffer(mCodec, 2000);
        if (bufidx >= 0) {
            size_t bufsize;
            uint8_t* buf = AMediaCodec_getInputBuffer(mCodec, bufidx, &bufsize);
            ssize_t sampleSize = AMediaExtractor_readSampleData(mExtractor, buf, bufsize);
            if (sampleSize < 0) {
                sampleSize = 0;
                mSawInputEOS = true;
                __android_log_print(ANDROID_LOG_INFO, TAG, "Input EOS");
            }
            int64_t presentationTimeUs = AMediaExtractor_getSampleTime(mExtractor);
            AMediaCodec_queueInputBuffer(mCodec, bufidx, 0, sampleSize, presentationTimeUs,
                                         mSawInputEOS ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0);
            AMediaExtractor_advance(mExtractor);
        }
    }

    // Get output
    AMediaCodecBufferInfo info;
    ssize_t status = AMediaCodec_dequeueOutputBuffer(mCodec, &info, 0);
    if (status >= 0) {
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            __android_log_print(ANDROID_LOG_INFO, TAG, "Output EOS");
            mSawOutputEOS = true;
            mFinished = true;
        }
        // Render to surface (mWindow)
        AMediaCodec_releaseOutputBuffer(mCodec, status, info.size > 0);
    } else if (status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        auto format = AMediaCodec_getOutputFormat(mCodec);
        __android_log_print(ANDROID_LOG_INFO, TAG, "format changed to: %s", AMediaFormat_toString(format));
        AMediaFormat_delete(format);
    }
}
