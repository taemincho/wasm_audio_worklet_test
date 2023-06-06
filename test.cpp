#include "test.hpp"
#include <emscripten.h>
#include <unistd.h>
#include <emscripten/bind.h>
#include <filesystem>
#include <unistd.h>

#define USE_BUFFER 0
#define AELog(fmt, ...) emscripten_log(EM_LOG_CONSOLE, "AudioEngine: " fmt, ##__VA_ARGS__)

inline constexpr int MIN_SUPPORTED_SAMPLE_RATE = 8000;
inline constexpr int MAX_SUPPORTED_SAMPLE_RATE = 384000;

typedef uint32_t FOURCC;
#define MAKEFOURCC(c0, c1, c2, c3) ((c0) | ((c1) << 8) | ((c2) << 16) | ((c3) << 24))

static constexpr FOURCC FOURCC_RIFF = MAKEFOURCC('R','I','F','F'); // "RIFF"
static constexpr FOURCC FOURCC_WAVE = MAKEFOURCC('W','A','V','E'); // "WAVE"
static constexpr FOURCC FOURCC_fmt_ = MAKEFOURCC('f','m','t',' '); // "fmt "
static constexpr FOURCC FOURCC_data = MAKEFOURCC('d','a','t','a'); // "data"

inline constexpr int16_t WAVE_FORMAT_PCM         = int16_t( 0x0001 ); // PCM
inline constexpr int16_t WAVE_FORMAT_IEEE_FLOAT  = int16_t( 0x0003 ); // IEEE float
inline constexpr int16_t WAVE_FORMAT_ALAW        = int16_t( 0x0006 ); // 8-bit ITU-T G.711 A-law
inline constexpr int16_t WAVE_FORMAT_MULAW       = int16_t( 0x0007 ); // 8-bit ITU-T G.711 µ-law
inline constexpr int16_t WAVE_FORMAT_EXTENSIBLE  = int16_t( 0xFFFE ); // Determined by SubFormat

inline constexpr int MIN_SAMPLE_RATE = 8000;
inline constexpr int MAX_SAMPLE_RATE = 192000;
inline constexpr int BITS_PER_BYTE = 8;
inline constexpr int BITS_PER_SAMPLE = 16; // 16-bits per audio sample

bool exists(const std::string& path)
{
    // http://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c-cross-platform
    return (access(path.c_str(), 0) != -1);
}

void PCMfloat32_to_PCMint16(const float *from, short *to, size_t n) {
    float scale = -SHRT_MIN;
    while (n--) {
        float _f32 = *from++;
        _f32 *= scale;
        _f32 = _f32 < SHRT_MAX ? _f32 : SHRT_MAX;
        _f32 = _f32 > SHRT_MIN ? _f32 : SHRT_MIN;
        *to++ = (short) std::round(_f32);
    }
}

void writeWavHeader(std::ostream& fp, short audioFormat, int sampleRate, int numSamples, short numChannels, short bitsPerSample)
{
	//	The canonical WAVE format starts with the RIFF header:
	//
	//	0         4   ChunkID          Contains the letters "RIFF" in ASCII form
	//size_t numWritten;
    fp.write((char*)&FOURCC_RIFF, sizeof(FOURCC));

	//	4         4   ChunkSize        36 + SubChunk2Size, or more precisely:
	//								   4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
	//								   This is the size of the rest of the chunk
	//								   following this number.  This is the size of the
	//								   entire file in bytes minus 8 bytes for the
	//								   two fields not included in this count:
	//								   ChunkID and ChunkSize.
    int subChunk2Size = (int)((int64_t)numSamples*numChannels*bitsPerSample/8);
	int chunkSize = 36 + subChunk2Size;

	fp.write((char*)&chunkSize, sizeof(chunkSize));

	//	8         4   Format           Contains the letters "WAVE"
    fp.write((char*)&FOURCC_WAVE, sizeof(FOURCC));

	//	The "WAVE" format consists of two subchunks: "fmt " and "data":
	//	The "fmt " subchunk describes the sound data's format:
	//
	//	12        4   Subchunk1ID      Contains the letters "fmt "
    fp.write((char*)&FOURCC_fmt_, sizeof(FOURCC));

	//	16        4   Subchunk1Size    16 for PCM.  This is the size of the
	//								   rest of the Subchunk which follows this number.
	int subChunk1Size = 16;
	fp.write((char*)&subChunk1Size, sizeof(subChunk1Size));

	//	20        2   AudioFormat      PCM = 1 (i.e. Linear quantization)
	//								   Values other than 1 indicate some
	//								   form of compression.
	fp.write((char*)&audioFormat, sizeof(audioFormat));

	//	22        2   NumChannels      Mono = 1, Stereo = 2, etc.
	fp.write((char*)&numChannels, sizeof(numChannels));

	//	24        4   SampleRate       8000, 44100, etc.
	fp.write((char*)&sampleRate, sizeof(sampleRate));

	//	28        4   ByteRate         == SampleRate * NumChannels * BitsPerSample/8
	int byteRate = sampleRate * numChannels * bitsPerSample/BITS_PER_BYTE;
	fp.write((char*)&byteRate, sizeof(byteRate));

	//	32        2   BlockAlign       == NumChannels * BitsPerSample/8
	//								   The number of bytes for one sample including
	//								   all channels. I wonder what happens when
	//								   this number isn't an integer?
	auto blockAlign = static_cast<short>(numChannels * bitsPerSample/BITS_PER_BYTE);
	fp.write((char*)&blockAlign, sizeof(blockAlign));

	//	34        2   BitsPerSample    8 bits = 8, 16 bits = 16, etc.
	//			  2   ExtraParamSize   if PCM, then doesn't exist
	//			  X   ExtraParams      space for extra parameters
	fp.write((char*)&bitsPerSample, sizeof(bitsPerSample));

	//	The "data" subchunk contains the size of the data and the actual sound:
	//
	//	36        4   Subchunk2ID      Contains the letters "data"
    fp.write((char*)&FOURCC_data, sizeof(FOURCC));

	//	40        4   Subchunk2Size    == NumSamples * NumChannels * BitsPerSample/8
	//								   This is the number of bytes in the data.
	//								   You can also think of this as the size
	//								   of the read of the subchunk following this
	//								   number.
	fp.write((char*)&subChunk2Size, sizeof(subChunk2Size));

	// NB: in the eventuality this function is modified to add custom subchunks before the data subchunk,
	// or if the position of the data subchunk size value is modified in any way to be different than 40,
	// Android's AudioEncoder will break. Just let the android developers know :)

	//return subChunk2Size;

	/*
	 //	44        *   Data             The actual sound data.
	 numWritten = fwrite( data, subChunk2Size, 1, fp );

	 fclose(fp);
	 */
}


bool AudioEncoder::open(const std::string &filePath, int sampleRate, int numChannels) {
    stream = std::ofstream(filePath, std::ios::out | std::ios::binary);
    if (sr != sampleRate || numCh != numChannels) {
        sr = sampleRate; 
        numCh = numChannels;
    }
	// std::filesystem::resize_file(filePath, sr * numCh * sizeof(float) * 60 * 5);
 
    audioFilePath = filePath;
    writeWavHeader(stream, WAVE_FORMAT_PCM, sampleRate, 0, static_cast<short>(numChannels), BITS_PER_SAMPLE);
	numWritten = 0;
    return true;
}

bool AudioEncoder::write(const float* samples, const int &numSamples)
{
    if (shortBuf.size() < size_t(numSamples)) {
        shortBuf.resize(numSamples);
    }

    PCMfloat32_to_PCMint16(samples, shortBuf.data(), numSamples);
    stream.write((char *) shortBuf.data(), std::streamsize(numSamples * sizeof(short)));
    numWritten += numSamples / numCh;
    
    return true;
}

bool AudioEncoder::close()
{    
    bool ok = false;
    
    if (stream.is_open()) {
        stream.seekp(0, std::ios::beg);
        // this will update both the chunk size and the data subchunk size:
        writeWavHeader(stream, WAVE_FORMAT_PCM, sr, numWritten, static_cast<short>(numCh), BITS_PER_SAMPLE);
        stream.flush();
        stream.close();
		// std::filesystem::resize_file(audioFilePath, sizeof(float) * numWritten * numCh);
        ok = exists(audioFilePath);
    }

    return ok;
}

int AudioEncoder::getNumWritten() {
	return numWritten;
}


void Recorder::startRecording(const std::string &filePath) {
    isRecording = true;
#if USE_BUFFER	
	buffer.clear();
	// buffer.reserve(sampleRate*numCh*60*5);
#else
	encoder.open(filePath, sampleRate, numCh);
#endif
	path = filePath;
	pos = 0;
}

void Recorder::stopRecording() {
    isRecording = false;
	AELog("Num written frames %d", encoder.getNumWritten());
	pos = 0;
#if USE_BUFFER
	if (buffer.size()) {
		encoder.open(path, sampleRate, numCh);	
		encoder.write(buffer.data(), (int) buffer.size());
		encoder.close();
	}
#else	
    encoder.close();
#endif
}

void Recorder::monitoring(bool enabled) {
    isMornitoring = enabled;
	AELog("Monitoring %s", enabled ? "On":"Off");
}

void Recorder::processInternal(float* input, float* output, int numFrames) {
    if (isRecording == true) {
#if USE_BUFFER
		for (int i = 0; i < numFrames * numCh; i++) {
			buffer.push_back(input[i]);
		}
#else
		encoder.write(input, numFrames * numCh);
#endif
		pos += float(numFrames) / float(sampleRate);
    } 

    if (isMornitoring == true) {
        for (int i = 0; i < numFrames * numCh; i++) {
            output[i] = input[i];
        }
    } else {
		for (int i = 0; i < numFrames * numCh; i++) {
            output[i] = 0;
        }
	}

	// float a = 0;
	// if (pos > 0 && (pos - int(pos) < float(numFrames) / float(sampleRate)) ) {
	// 	while (a < 10000) {
	// 		a+=0.004;
	// 	}
	// 	AELog("%f %f\n", pos, a);
	// }
}

float Recorder::getPos() {
	return pos;
}

using namespace emscripten;

EMSCRIPTEN_BINDINGS(AudioEngine) {
	class_<Recorder>("Recorder")
		.smart_ptr<std::shared_ptr<Recorder>>("AudioSink")
	    .class_function("create", &Recorder::create)
	    .constructor<int, int>()
	    .function("process", &Recorder::process, allow_raw_pointers())
        .function("monitoring", &Recorder::monitoring)
		.function("getPos", &Recorder::getPos) 
	    .function("startRecording", &Recorder::startRecording)
	    .function("stopRecording", &Recorder::stopRecording);
}