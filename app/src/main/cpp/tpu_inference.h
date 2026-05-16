#pragma once

#include "audio_capture.h"
#include <vector>
#include <memory>

// Forward declarations for TFLite
namespace tflite {
    class Interpreter;
    class FlatBufferModel;
}

class TPUInference {
public:
    TPUInference();
    ~TPUInference();

    bool Initialize(const char* modelPath);
    void ProcessNeuralWeights(const AudioFeatures& audio, const float* user_controls, float* out_100_weights);

private:
    std::unique_ptr<tflite::FlatBufferModel> mModel;
    std::unique_ptr<tflite::Interpreter> mInterpreter;

    // Tensor indices
    int mInputTensorIndex = -1;
    int mOutputTensorIndex = -1;
};
