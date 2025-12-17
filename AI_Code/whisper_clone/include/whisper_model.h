#ifndef WHISPER_MODEL_H
#define WHISPER_MODEL_H

#include <string>
#include <vector>
#include <memory>

class WhisperModel {
public:
    struct TranscriptionResult {
        std::string text;
        float confidence;
        std::vector<float> timestamps;
        bool success;
    };

    enum class ModelSize {
        TINY,
        BASE,
        SMALL,
        MEDIUM,
        LARGE
    };

    WhisperModel();
    ~WhisperModel();

    // Initialize model
    bool initialize(ModelSize size = ModelSize::BASE);

    // Load model from file
    bool loadModel(const std::string& model_path);

    // Transcribe audio samples
    TranscriptionResult transcribe(const std::vector<float>& audio_samples,
                                   int sample_rate = 16000);

    // Transcribe audio file directly (uses whisper.cpp)
    TranscriptionResult transcribeFile(const std::string& audio_file,
                                       const std::string& language = "");

    // Transcribe with language hint
    TranscriptionResult transcribeWithLanguage(const std::vector<float>& audio_samples,
                                               const std::string& language,
                                               int sample_rate = 16000);

    // Check if model is loaded
    bool isLoaded() const { return model_loaded_; }

    // Get model info
    std::string getModelInfo() const;

private:
    bool model_loaded_;
    ModelSize current_model_size_;
    std::string model_path_;

    // Internal processing methods
    std::vector<float> preprocessAudio(const std::vector<float>& samples, int sample_rate);
    std::vector<float> computeMelSpectrogram(const std::vector<float>& samples);
    std::string decodeTokens(const std::vector<int>& tokens);

    // Whisper.cpp wrapper methods
    bool findWhisperExecutable();
    TranscriptionResult callWhisperCpp(const std::string& audio_file, const std::string& language);
    std::string parseWhisperOutput(const std::string& output_file);
    std::string whisper_executable_path_;
};

#endif // WHISPER_MODEL_H
