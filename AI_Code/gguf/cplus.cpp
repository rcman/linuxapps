#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <map>
#include <cmath>
#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>

// GGUF constants
constexpr uint32_t GGUF_MAGIC = 0x46554747; // “GGUF”
constexpr int GGML_TYPE_F16 = 1;

// Utility functions for byte order conversion (little-endian)
inline uint32_t read_uint32_le(std::ifstream& file) {
uint32_t value;
file.read(reinterpret_cast<char*>(&value), sizeof(value));
return value;
}

inline uint64_t read_uint64_le(std::ifstream& file) {
uint64_t value;
file.read(reinterpret_cast<char*>(&value), sizeof(value));
return value;
}

inline int32_t read_int32_le(std::ifstream& file) {
int32_t value;
file.read(reinterpret_cast<char*>(&value), sizeof(value));
return value;
}

inline int64_t read_int64_le(std::ifstream& file) {
int64_t value;
file.read(reinterpret_cast<char*>(&value), sizeof(value));
return value;
}

inline uint16_t read_uint16_le(std::ifstream& file) {
uint16_t value;
file.read(reinterpret_cast<char*>(&value), sizeof(value));
return value;
}

inline float read_float32_le(std::ifstream& file) {
float value;
file.read(reinterpret_cast<char*>(&value), sizeof(value));
return value;
}

inline double read_float64_le(std::ifstream& file) {
double value;
file.read(reinterpret_cast<char*>(&value), sizeof(value));
return value;
}

inline std::string read_string(std::ifstream& file) {
uint64_t len = read_uint64_le(file);
if (len > 1000000) {
throw std::runtime_error(“Invalid string length: “ + std::to_string(len));
}
std::vector<char> buffer(len);
file.read(buffer.data(), len);
return std::string(buffer.begin(), buffer.end());
}

// FP16 to FP32 conversion
inline float fp16_to_fp32(uint16_t h) {
uint32_t sign = (h >> 15) & 0x1;
uint32_t exponent = (h >> 10) & 0x1F;
uint32_t mantissa = h & 0x3FF;

```
// Handle special cases
if (exponent == 0) {
    if (mantissa == 0) {
        // Zero
        return sign ? -0.0f : 0.0f;
    } else {
        // Subnormal
        float value = (mantissa / 1024.0f) * std::pow(2.0f, -14.0f);
        return sign ? -value : value;
    }
} else if (exponent == 31) {
    // Infinity or NaN
    if (mantissa == 0) {
        return sign ? -INFINITY : INFINITY;
    } else {
        return NAN;
    }
}

// Normal numbers
float value = (1.0f + mantissa / 1024.0f) * std::pow(2.0f, static_cast<float>(exponent) - 15.0f);
return sign ? -value : value;
```

}

// Metadata value structure
struct MetadataValue {
int type;
std::string str_value;
int64_t int_value;
double float_value;
bool bool_value;
std::vector<std::string> array_value;

```
std::string to_string() const {
    switch (type) {
        case 8: return str_value;
        case 5: return std::to_string(int_value);
        case 6: return std::to_string(float_value);
        case 7: return bool_value ? "true" : "false";
        case 9: {
            if (array_value.size() > 10) {
                return "[" + std::to_string(array_value.size()) + " items]";
            }
            std::string result = "[";
            for (size_t i = 0; i < array_value.size(); i++) {
                if (i > 0) result += ", ";
                result += array_value[i];
            }
            result += "]";
            return result;
        }
        default: return "unknown";
    }
}
```

};

// Tensor structure
struct GGUFTensor {
std::string name;
std::vector<uint64_t> shape;
int type;
uint64_t offset;

```
uint64_t element_count() const {
    uint64_t count = 1;
    for (auto dim : shape) {
        count *= dim;
    }
    return count;
}
```

};

// GGUF Model class
class GGUFModel {
private:
std::string path;
int version;
std::map<std::string, MetadataValue> metadata;
std::vector<GGUFTensor> tensors;
uint64_t data_offset;

```
MetadataValue read_metadata_value(std::ifstream& file) {
    MetadataValue meta;
    meta.type = read_int32_le(file);
    
    switch (meta.type) {
        case 0: // uint8
            meta.int_value = file.get();
            break;
        case 1: // int8
            meta.int_value = static_cast<int8_t>(file.get());
            break;
        case 2: // uint16
            meta.int_value = read_uint16_le(file);
            break;
        case 3: // int16
            meta.int_value = static_cast<int16_t>(read_uint16_le(file));
            break;
        case 4: // uint32
            meta.int_value = read_uint32_le(file);
            break;
        case 5: // int32
            meta.int_value = read_int32_le(file);
            break;
        case 6: // float32
            meta.float_value = read_float32_le(file);
            break;
        case 7: // bool
            meta.bool_value = file.get() != 0;
            break;
        case 8: // string
            meta.str_value = read_string(file);
            break;
        case 9: { // array
            int array_type = read_int32_le(file);
            uint64_t array_len = read_uint64_le(file);
            
            if (array_len > 100000) {
                throw std::runtime_error("Array too large");
            }
            
            for (uint64_t i = 0; i < array_len; i++) {
                if (array_type == 8) {
                    meta.array_value.push_back(read_string(file));
                } else if (array_type == 5) {
                    meta.array_value.push_back(std::to_string(read_int32_le(file)));
                } else {
                    // Skip other types
                    MetadataValue item = read_metadata_value(file);
                    meta.array_value.push_back(item.to_string());
                }
            }
            break;
        }
        case 10: // uint64
            meta.int_value = read_uint64_le(file);
            break;
        case 11: // int64
            meta.int_value = read_int64_le(file);
            break;
        case 12: // float64
            meta.float_value = read_float64_le(file);
            break;
        default:
            throw std::runtime_error("Unknown metadata type: " + std::to_string(meta.type));
    }
    
    return meta;
}

std::string get_type_name(int type) const {
    switch (type) {
        case 0: return "F32";
        case 1: return "F16";
        case 2: return "Q4_0";
        case 3: return "Q4_1";
        case 6: return "Q5_0";
        case 7: return "Q5_1";
        case 8: return "Q8_0";
        case 9: return "Q8_1";
        default: return "Unknown(" + std::to_string(type) + ")";
    }
}
```

public:
GGUFModel(const std::string& model_path) : path(model_path), version(0), data_offset(0) {}

```
void load() {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    
    // Read and verify magic number
    uint32_t magic = read_uint32_le(file);
    if (magic != GGUF_MAGIC) {
        throw std::runtime_error("Invalid GGUF file. Magic: 0x" + 
                               std::to_string(magic) + ", expected: 0x" + 
                               std::to_string(GGUF_MAGIC));
    }
    
    // Read version
    version = read_int32_le(file);
    std::cout << "GGUF version: " << version << std::endl;
    
    // Read counts
    uint64_t tensor_count = read_uint64_le(file);
    uint64_t metadata_count = read_uint64_le(file);
    
    std::cout << "Tensor count: " << tensor_count << std::endl;
    std::cout << "Metadata count: " << metadata_count << std::endl;
    
    // Read metadata
    std::cout << "\n=== Model Metadata ===" << std::endl;
    for (uint64_t i = 0; i < metadata_count; i++) {
        std::string key = read_string(file);
        MetadataValue value = read_metadata_value(file);
        metadata[key] = value;
        std::cout << key << " = " << value.to_string() << std::endl;
    }
    
    // Read tensor information
    std::cout << "\n=== Tensors ===" << std::endl;
    for (uint64_t i = 0; i < tensor_count; i++) {
        GGUFTensor tensor;
        tensor.name = read_string(file);
        
        uint32_t ndim = read_uint32_le(file);
        for (uint32_t j = 0; j < ndim; j++) {
            tensor.shape.push_back(read_uint64_le(file));
        }
        
        tensor.type = read_int32_le(file);
        tensor.offset = read_uint64_le(file);
        
        tensors.push_back(tensor);
        
        std::cout << "\nTensor #" << (i + 1) << ": " << tensor.name << std::endl;
        std::cout << "  Shape: [";
        for (size_t j = 0; j < tensor.shape.size(); j++) {
            if (j > 0) std::cout << ", ";
            std::cout << tensor.shape[j];
        }
        std::cout << "]" << std::endl;
        std::cout << "  Type: " << get_type_name(tensor.type) << std::endl;
        std::cout << "  Type Code: " << tensor.type << std::endl;
        std::cout << "  Offset: " << tensor.offset << std::endl;
    }
    
    // Calculate data offset with alignment
    data_offset = file.tellg();
    uint64_t alignment = 32;
    data_offset = ((data_offset + alignment - 1) / alignment) * alignment;
    
    std::cout << "\nData section offset: " << data_offset << std::endl;
    
    file.close();
}

bool is_fp16() const {
    if (tensors.empty()) return false;
    
    for (const auto& tensor : tensors) {
        if (tensor.type != GGML_TYPE_F16) {
            return false;
        }
    }
    return true;
}

std::vector<float> read_tensor_fp16(const GGUFTensor& tensor) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    
    uint64_t total_elements = tensor.element_count();
    // Limit to prevent memory issues
    total_elements = std::min(total_elements, static_cast<uint64_t>(10000000));
    
    std::cout << "Reading " << total_elements << " FP16 values from offset " 
              << (data_offset + tensor.offset) << std::endl;
    
    file.seekg(data_offset + tensor.offset);
    
    std::vector<float> result(total_elements);
    for (uint64_t i = 0; i < total_elements; i++) {
        uint16_t fp16_bits = read_uint16_le(file);
        result[i] = fp16_to_fp32(fp16_bits);
    }
    
    std::cout << "Successfully read and converted " << total_elements 
              << " FP16 values to FP32" << std::endl;
    
    file.close();
    return result;
}

std::vector<float> infer(const std::vector<float>& input) {
    if (tensors.empty()) {
        throw std::runtime_error("No tensors loaded");
    }
    
    // Find suitable tensor
    const GGUFTensor* main_tensor = nullptr;
    for (const auto& t : tensors) {
        if (t.shape.size() >= 2) {
            main_tensor = &t;
            break;
        }
    }
    
    if (!main_tensor) {
        main_tensor = &tensors[0];
    }
    
    std::cout << "Using tensor for inference: " << main_tensor->name << std::endl;
    std::cout << "Tensor shape: [";
    for (size_t i = 0; i < main_tensor->shape.size(); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << main_tensor->shape[i];
    }
    std::cout << "]" << std::endl;
    
    // Read weights
    std::vector<float> weights = read_tensor_fp16(*main_tensor);
    
    // Matrix dimensions
    uint64_t rows = main_tensor->shape.size() > 0 ? main_tensor->shape[0] : 1;
    uint64_t cols = main_tensor->shape.size() > 1 ? main_tensor->shape[1] : weights.size();
    
    size_t output_size = std::min(static_cast<size_t>(rows), static_cast<size_t>(512));
    size_t input_size = std::min(static_cast<size_t>(cols), input.size());
    
    std::cout << "Computing output: " << output_size << " values" << std::endl;
    std::cout << "Using input size: " << input_size << std::endl;
    
    // Matrix-vector multiplication
    std::vector<float> output(output_size);
    
    for (size_t i = 0; i < output_size; i++) {
        float sum = 0.0f;
        for (size_t j = 0; j < input_size; j++) {
            size_t weight_idx = i * cols + j;
            if (weight_idx < weights.size()) {
                sum += weights[weight_idx] * input[j];
            }
        }
        // Apply activation
        output[i] = std::tanh(sum);
    }
    
    return output;
}

const std::vector<GGUFTensor>& get_tensors() const { return tensors; }
```

};

// Load input from file
std::vector<float> load_input_from_file(const std::string& filename, size_t expected_size) {
std::ifstream file(filename);
if (!file.is_open()) {
throw std::runtime_error(“Failed to open input file: “ + filename);
}

```
std::vector<float> values;
std::string line;

while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        // Remove whitespace
        token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
        if (!token.empty()) {
            values.push_back(std::stof(token));
        }
    }
}

std::vector<float> result(expected_size, 0.0f);
for (size_t i = 0; i < expected_size && i < values.size(); i++) {
    result[i] = values[i];
}

if (values.size() < expected_size) {
    std::cout << "Warning: Input file has " << values.size() 
              << " values, expected " << expected_size << ". Padding with zeros." << std::endl;
}

std::cout << "Loaded " << values.size() << " values from " << filename << std::endl;

file.close();
return result;
```

}

int main(int argc, char* argv[]) {
if (argc < 2) {
std::cout << “GGUF FP16 CPU Inference Engine” << std::endl;
std::cout << “Usage: “ << argv[0] << “ <model.gguf> [input_file]” << std::endl;
std::cout << “FP16 quantization only - Pure C++, no dependencies” << std::endl;
return 1;
}

```
std::string model_path = argv[1];
std::string input_file = argc > 2 ? argv[2] : "";

if (model_path.substr(model_path.find_last_of(".") + 1) != "gguf") {
    std::cerr << "Error: Only GGUF files are supported" << std::endl;
    return 1;
}

try {
    GGUFModel model(model_path);
    model.load();
    
    if (!model.is_fp16()) {
        std::cerr << "Error: Model is not FP16 quantized" << std::endl;
        std::cerr << "This application only supports FP16 GGUF models" << std::endl;
        return 1;
    }
    
    // Prepare input
    std::vector<float> input;
    if (!input_file.empty()) {
        input = load_input_from_file(input_file, 512);
    } else {
        input.resize(512);
        for (size_t i = 0; i < input.size(); i++) {
            input[i] = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
        }
        std::cout << "Using random test input (512 values)" << std::endl;
    }
    
    std::cout << "\n=== Running FP16 Inference ===" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<float> output = model.infer(input);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Inference time: " << duration.count() << " ms" << std::endl;
    
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Output size: " << output.size() << std::endl;
    std::cout << "First 10 values: [";
    for (size_t i = 0; i < std::min(static_cast<size_t>(10), output.size()); i++) {
        if (i > 0) std::cout << ", ";
        std::cout << std::fixed << std::setprecision(6) << output[i];
    }
    std::cout << "]" << std::endl;
    
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
}

return 0;
```

}