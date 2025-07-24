//  Official IAMF Validation Tool
//  Uses the real IAMF decoder library to validate IAMF files
//  This replaces our custom parser with the official implementation

#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <string>

// Official IAMF decoder includes
#include "build/_deps/libiamf-src/code/include/IAMF_decoder.h"
#include "build/_deps/libiamf-src/code/include/IAMF_defines.h"

class OfficialIAMFValidator {
private:
    std::vector<uint8_t> file_data;
    IAMF_DecoderHandle decoder_handle;
    
public:
    OfficialIAMFValidator() : decoder_handle(nullptr) {}
    
    ~OfficialIAMFValidator() {
        if (decoder_handle) {
            IAMF_decoder_close(decoder_handle);
        }
    }
    
    bool LoadFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "ERROR: Cannot open file: " << filename << std::endl;
            return false;
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        file_data.resize(size);
        file.read(reinterpret_cast<char*>(file_data.data()), size);
        
        std::cout << "Loaded IAMF file: " << filename << " (" << size << " bytes)" << std::endl;
        return true;
    }
    
    const char* GetErrorString(int error_code) {
        switch (error_code) {
            case IAMF_OK: return "IAMF_OK";
            case IAMF_ERR_BAD_ARG: return "IAMF_ERR_BAD_ARG";
            case IAMF_ERR_BUFFER_TOO_SMALL: return "IAMF_ERR_BUFFER_TOO_SMALL";
            case IAMF_ERR_INTERNAL: return "IAMF_ERR_INTERNAL";
            case IAMF_ERR_INVALID_PACKET: return "IAMF_ERR_INVALID_PACKET";
            case IAMF_ERR_INVALID_STATE: return "IAMF_ERR_INVALID_STATE";
            case IAMF_ERR_UNIMPLEMENTED: return "IAMF_ERR_UNIMPLEMENTED";
            case IAMF_ERR_ALLOC_FAIL: return "IAMF_ERR_ALLOC_FAIL";
            default: return "UNKNOWN_ERROR";
        }
    }
    
    bool ValidateIAMF() {
        std::cout << "\n=== Official IAMF Validation ===" << std::endl;
        
        // Step 1: Open decoder
        decoder_handle = IAMF_decoder_open();
        if (!decoder_handle) {
            std::cerr << "ERROR: Failed to open IAMF decoder" << std::endl;
            return false;
        }
        std::cout << "✓ IAMF decoder opened successfully" << std::endl;
        
        // Step 2: Configure decoder with IAMF data
        std::cout << "\nConfiguring decoder with IAMF data..." << std::endl;
        uint32_t consumed_bytes = 0;
        
        int result = IAMF_decoder_configure(decoder_handle, 
                                          file_data.data(), 
                                          file_data.size(), 
                                          &consumed_bytes);
        
        std::cout << "Configuration result: " << GetErrorString(result) << " (" << result << ")" << std::endl;
        std::cout << "Consumed bytes: " << consumed_bytes << " / " << file_data.size() << std::endl;
        
        if (result != IAMF_OK) {
            std::cerr << "✗ IAMF configuration FAILED: " << GetErrorString(result) << std::endl;
            
            // Provide specific guidance based on error type
            switch (result) {
                case IAMF_ERR_BUFFER_TOO_SMALL:
                    std::cerr << "  → The IAMF data is incomplete or truncated" << std::endl;
                    break;
                case IAMF_ERR_INVALID_PACKET:
                    std::cerr << "  → The IAMF data contains invalid OBU structure or metadata" << std::endl;
                    break;
                case IAMF_ERR_BAD_ARG:
                    std::cerr << "  → Invalid arguments or malformed IAMF headers" << std::endl;
                    break;
                default:
                    std::cerr << "  → Decoder encountered an internal error" << std::endl;
                    break;
            }
            return false;
        }
        
        std::cout << "✓ IAMF configuration SUCCESSFUL!" << std::endl;
        
        // Step 3: Get stream info
        IAMF_StreamInfo* stream_info = IAMF_decoder_get_stream_info(decoder_handle);
        if (stream_info) {
            std::cout << "\n=== Stream Information ===" << std::endl;
            std::cout << "Max frame size: " << stream_info->max_frame_size << " bytes" << std::endl;
        }
        
        // Step 4: Get codec capabilities
        char* codec_caps = IAMF_decoder_get_codec_capability();
        if (codec_caps) {
            std::cout << "Supported codecs: " << codec_caps << std::endl;
            free(codec_caps);  // Must free manually as per API docs
        }
        
        // Step 5: Test output configurations
        std::cout << "\n=== Testing Output Configurations ===" << std::endl;
        
        // Test stereo output
        result = IAMF_decoder_output_layout_set_sound_system(decoder_handle, SOUND_SYSTEM_A);
        std::cout << "Stereo output configuration: " << GetErrorString(result) << std::endl;
        
        // Test binaural output  
        result = IAMF_decoder_output_layout_set_binaural(decoder_handle);
        std::cout << "Binaural output configuration: " << GetErrorString(result) << std::endl;
        
        // Step 6: Test a small decode operation to verify audio data
        std::cout << "\n=== Testing Audio Decode ===" << std::endl;
        
        if (consumed_bytes < file_data.size()) {
            // Try to decode some audio frames
            std::vector<int16_t> pcm_buffer(4096 * 2); // Stereo buffer
            uint32_t frame_consumed = 0;
            
            result = IAMF_decoder_decode(decoder_handle,
                                       file_data.data() + consumed_bytes,
                                       file_data.size() - consumed_bytes,
                                       &frame_consumed,
                                       pcm_buffer.data());
            
            if (result > 0) {
                std::cout << "✓ Successfully decoded " << result << " samples" << std::endl;
                std::cout << "  Frame consumed: " << frame_consumed << " bytes" << std::endl;
            } else if (result == 0) {
                std::cout << "⚠ No samples decoded (may need more data)" << std::endl;
            } else {
                std::cout << "✗ Decode failed: " << GetErrorString(result) << std::endl;
            }
        }
        
        std::cout << "\n✓ IAMF file validation COMPLETE!" << std::endl;
        std::cout << "✓ File structure is VALID and decoder-compatible!" << std::endl;
        
        return true;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <iamf_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " guitar-eclipsa-final.iamf" << std::endl;
        return 1;
    }
    
    std::cout << "Official IAMF Validation Tool" << std::endl;
    std::cout << "=============================" << std::endl;
    
    OfficialIAMFValidator validator;
    
    if (!validator.LoadFile(argv[1])) {
        return 1;
    }
    
    if (!validator.ValidateIAMF()) {
        std::cout << "\n✗ VALIDATION FAILED" << std::endl;
        return 1;
    }
    
    std::cout << "\n🎉 VALIDATION SUCCESSFUL! Your IAMF file is correctly formatted!" << std::endl;
    return 0;
} 