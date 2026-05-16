#include "tpu_inference.h"
#include <tensorflow/lite/interpreter_builder.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/delegates/nnapi/nnapi_delegate.h>
#include <android/log.h>

#define TAG "TPUInference"

TPUInference::TPUInference() {}

TPUInference::~TPUInference() {}

bool TPUInference::Initialize(const char* modelPath) {
    mModel = tflite::FlatBufferModel::BuildFromFile(modelPath);
    if (!mModel) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to load model: %s", modelPath);
        return false;
    }

    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder builder(*mModel, resolver);
    builder(&mInterpreter);

    if (!mInterpreter) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to create interpreter");
        return false;
    }

    // Configure NNAPI Delegate for TPU acceleration
    tflite::StatefulNnApiDelegate::Options options;
    options.accelerator_name = "google-tensor-tpu"; // Target specific TPU if known
    auto delegate = std::make_unique<tflite::StatefulNnApiDelegate>(options);
    
    if (mInterpreter->ModifyGraphWithDelegate(std::move(delegate)) != kTfLiteOk) {
        __android_log_print(ANDROID_LOG_WARN, TAG, "Failed to apply NNAPI delegate, falling back to CPU");
    }

    if (mInterpreter->AllocateTensors() != kTfLiteOk) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to allocate tensors");
        return false;
    }

    mInputTensorIndex = mInterpreter->inputs()[0];
    mOutputTensorIndex = mInterpreter->outputs()[0];

    return true;
}

void TPUInference::ProcessNeuralWeights(const AudioFeatures& audio, const float* user_controls, float* out_100_weights) {
    if (!mInterpreter) return;

    float* input = mInterpreter->typed_input_tensor<float>(0);
    if (!input) return;

    // Map audio features and user controls to input tensor
    input[0] = audio.bass;
    input[1] = audio.mid;
    input[2] = audio.treble;
    input[3] = audio.is_beat ? 1.0f : 0.0f;
    for (int i = 0; i < 64; ++i) {
        input[4 + i] = audio.frequency_bins[i];
    }
    // Assume 3 user controls for now: zoom, warp, dampening
    input[68] = user_controls[0];
    input[69] = user_controls[1];
    input[70] = user_controls[2];

    if (mInterpreter->Invoke() != kTfLiteOk) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to invoke interpreter");
        return;
    }

    float* output = mInterpreter->typed_output_tensor<float>(0);
    if (output) {
        std::copy(output, output + 100, out_100_weights);
    }
}
