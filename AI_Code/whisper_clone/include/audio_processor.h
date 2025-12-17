#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

class AudioProcessor {
public:
    struct AudioData {
        std::vector<float> samples;
        int sample_rate;
        int channels;
        float duration;
    };

    AudioProcessor();
    ~AudioProcessor();

    // Load audio from file
    bool loadAudioFile(const std::string& filepath, AudioData& audio);

    // Convert stereo to mono
    static std::vector<float> stereoToMono(const std::vector<float>& stereo_data);

    // Resample audio to target sample rate
    static std::vector<float> resample(const std::vector<float>& input,
                                       int input_rate,
                                       int output_rate);

    // Normalize audio samples
    static void normalize(std::vector<float>& samples);

    // Read WAV file
    bool readWavFile(const std::string& filepath, AudioData& audio);

    // Convert PCM data to float
    static std::vector<float> pcmToFloat(const std::vector<int16_t>& pcm_data);

private:
    struct WavHeader {
        char riff[4];
        uint32_t file_size;
        char wave[4];
        char fmt[4];
        uint32_t fmt_size;
        uint16_t audio_format;
        uint16_t num_channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
        char data[4];
        uint32_t data_size;
    };

    bool parseWavHeader(std::ifstream& file, WavHeader& header);
};

#endif // AUDIO_PROCESSOR_H
