#include <fstream>
#include <vector>
#include <atomic>
#include <memory>
#include <string>
#include <cmath>

#define AELog(fmt, ...) emscripten_log(EM_LOG_CONSOLE, "AudioEngine: " fmt, ##__VA_ARGS__)

class AudioEncoder {
public:
    bool open(const std::string &filePath, int sampleRate, int numChannels);
    bool write(const float* samples, const int &numSamples);
    bool close();
    int getNumWritten();

private:
    std::ofstream stream;

    std::string audioFilePath;
    int sr = 0;
    int numCh = 0;
    int numWritten = 0;
    
	std::vector<short> shortBuf;
};

class Recorder {
public:

    static inline std::shared_ptr<Recorder> create(int sampleRate, int numCh)
    {
        return std::make_shared<Recorder>(sampleRate, numCh);
    }

    Recorder(int sampleRate, int numCh):sampleRate(sampleRate), numCh(numCh){
        sine.resize(sampleRate * numCh * 20);
        for (size_t i = 0; i < sine.size()/numCh; i++)
        {
            float val = sin(2*M_PI*220*i/float(sampleRate));
            for (size_t ch = 0; ch < numCh; ch++)
            {
                sine[i*numCh+ch] = val;
            }
            
        }
        
    };

    void startRecording(const std::string &filePath);
    void stopRecording();
    void monitoring(bool enabled);
    float getPos();

    void process(uintptr_t input_ptr, uintptr_t output_ptr, int numFrames)
    {
        float *input = reinterpret_cast<float *>(input_ptr);
        float *output = reinterpret_cast<float *>(output_ptr);
        processInternal(input, output, numFrames);
    }

private:
    void processInternal(float* input, float* output, int numFrames);
    std::atomic<bool> isRecording = ATOMIC_VAR_INIT(false);
    std::atomic<bool> isMornitoring = ATOMIC_VAR_INIT(false);
    // bool isRecording = false;
    // bool isMornitoring = false;
    AudioEncoder encoder;

    std::vector<float> buffer;
    std::string path;

    int sampleRate;
    int numCh;
    float pos = 0;
    int posi = 0;

    std::vector<float> sine;
};