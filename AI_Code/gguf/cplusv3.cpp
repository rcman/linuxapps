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
#include <chrono>
#include <stdexcept>
#include <thread>

// GGUF constants
constexpr uint32_t GGUF_MAGIC = 0x46554747;
constexpr int GGML_TYPE_F16 = 1;

class GGUFException : public std::runtime_error {
public:
    explicit GGUFException(const std::string& msg) : std::runtime_error(msg) {}
};

// FP16 to FP32 conversion
inline float fp16_to_fp32(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exponent = (h >> 10) & 0x1F;
    uint32_t mantissa = h & 0x3FF;
    
    if (exponent == 0) {
        if (mantissa == 0) return sign ? -0.0f : 0.0f;
        float value = (mantissa / 1024.0f) * 0.00006103515625f;
        return sign ? -value : value;
    } else if (exponent == 31) {
        if (mantissa == 0) return sign ? -INFINITY : INFINITY;
        return NAN;
    }
    
    uint32_t fp32_exp = (exponent - 15 + 127) << 23;
    uint32_t fp32_mantissa = mantissa << 13;
    uint32_t fp32_sign = sign << 31;
    uint32_t fp32_bits = fp32_sign | fp32_exp | fp32_mantissa;
    
    float result;
    std::memcpy(&result, &fp32_bits, sizeof(float));
    return result;
}

struct Tensor {
    std::string name;
    std::vector<uint64_t> shape;
    std::vector<float> data;
    
    uint64_t size() const {
        uint64_t s = 1;
        for (auto dim : shape) s *= dim;
        return s;
    }
    
    void print_info() const {
        std::cout << name << " [";
        for (size_t i = 0; i < shape.size(); i++) {
            if (i > 0) std::cout << " x ";
            std::cout << shape[i];
        }
        std::cout << "] = " << size() << " elements" << std::endl;
    }
};

class GGUFModel {
private:
    std::string path;
    std::map<std::string, std::string> metadata;
    std::map<std::string, Tensor> tensors;
    uint64_t n_vocab;
    uint64_t n_embd;
    uint64_t n_layer;
    uint64_t n_head;
    uint64_t n_ctx;
    
    template<typename T>
    T read_value(std::ifstream& file) {
        T value;
        file.read(reinterpret_cast<char*>(&value), sizeof(T));
        return value;
    }
    
    std::string read_string(std::ifstream& file) {
        uint64_t len = read_value<uint64_t>(file);
        if (len > 10000000) throw GGUFException("String too long");
        std::vector<char> buf(len);
        file.read(buf.data(), len);
        return std::string(buf.begin(), buf.end());
    }
    
    void skip_metadata_value(std::ifstream& file, int type) {
        switch (type) {
            case 0: case 1: file.seekg(1, std::ios::cur); break;
            case 2: case 3: file.seekg(2, std::ios::cur); break;
            case 4: case 5: case 6: file.seekg(4, std::ios::cur); break;
            case 7: file.seekg(1, std::ios::cur); break;
            case 8: read_string(file); break;
            case 9: {
                int atype = read_value<int32_t>(file);
                uint64_t alen = read_value<uint64_t>(file);
                for (uint64_t i = 0; i < alen; i++) skip_metadata_value(file, atype);
                break;
            }
            case 10: case 11: case 12: file.seekg(8, std::ios::cur); break;
        }
    }
    
    void read_metadata_value(std::ifstream& file, const std::string& key) {
        int type = read_value<int32_t>(file);
        
        if (type == 8) {
            metadata[key] = read_string(file);
        } else if (type == 4 || type == 5) {
            uint32_t val = read_value<uint32_t>(file);
            metadata[key] = std::to_string(val);
        } else if (type == 10 || type == 11) {
            uint64_t val = read_value<uint64_t>(file);
            metadata[key] = std::to_string(val);
        } else {
            skip_metadata_value(file, type);
        }
    }
    
    std::vector<float> read_fp16_tensor(std::ifstream& file, uint64_t count) {
        std::vector<uint16_t> fp16_data(count);
        file.read(reinterpret_cast<char*>(fp16_data.data()), count * 2);
        
        std::vector<float> fp32_data(count);
        for (uint64_t i = 0; i < count; i++) {
            fp32_data[i] = fp16_to_fp32(fp16_data[i]);
        }
        return fp32_data;
    }
    
    void extract_hyperparameters() {
        auto get_param = [this](const std::string& key, uint64_t default_val) -> uint64_t {
            auto it = metadata.find(key);
            if (it != metadata.end()) {
                try {
                    return std::stoull(it->second);
                } catch(...) {}
            }
            return default_val;
        };
        
        n_vocab = get_param("llama.vocab_size", 32000);
        n_embd = get_param("llama.embedding_length", 4096);
        n_layer = get_param("llama.block_count", 32);
        n_head = get_param("llama.attention.head_count", 32);
        n_ctx = get_param("llama.context_length", 2048);
        
        std::cout << "\nModel Architecture:" << std::endl;
        std::cout << "  Vocabulary size: " << n_vocab << std::endl;
        std::cout << "  Embedding dim: " << n_embd << std::endl;
        std::cout << "  Layers: " << n_layer << std::endl;
        std::cout << "  Attention heads: " << n_head << std::endl;
        std::cout << "  Context length: " << n_ctx << std::endl;
    }
    
public:
    explicit GGUFModel(const std::string& model_path) : path(model_path) {}
    
    void load() {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw GGUFException("Cannot open file: " + path);
        
        uint32_t magic = read_value<uint32_t>(file);
        if (magic != GGUF_MAGIC) {
            throw GGUFException("Invalid GGUF magic: 0x" + std::to_string(magic));
        }
        
        uint32_t version = read_value<uint32_t>(file);
        uint64_t tensor_count = read_value<uint64_t>(file);
        uint64_t metadata_count = read_value<uint64_t>(file);
        
        std::cout << "GGUF version: " << version << std::endl;
        std::cout << "Tensors: " << tensor_count << std::endl;
        std::cout << "Metadata entries: " << metadata_count << std::endl;
        
        // Read metadata
        for (uint64_t i = 0; i < metadata_count; i++) {
            std::string key = read_string(file);
            read_metadata_value(file, key);
        }
        
        extract_hyperparameters();
        
        // Read tensor metadata
        std::vector<std::tuple<std::string, std::vector<uint64_t>, int, uint64_t>> tensor_info;
        
        for (uint64_t i = 0; i < tensor_count; i++) {
            std::string name = read_string(file);
            uint32_t ndim = read_value<uint32_t>(file);
            
            std::vector<uint64_t> shape(ndim);
            for (uint32_t j = 0; j < ndim; j++) {
                shape[j] = read_value<uint64_t>(file);
            }
            
            int type = read_value<int32_t>(file);
            uint64_t offset = read_value<uint64_t>(file);
            
            if (type != GGML_TYPE_F16) {
                throw GGUFException("Tensor " + name + " is not FP16");
            }
            
            tensor_info.push_back({name, shape, type, offset});
        }
        
        uint64_t data_offset = file.tellg();
        data_offset = ((data_offset + 31) / 32) * 32;
        
        std::cout << "\nLoading tensors..." << std::endl;
        
        for (const auto& [name, shape, type, offset] : tensor_info) {
            Tensor tensor;
            tensor.name = name;
            tensor.shape = shape;
            
            uint64_t elem_count = 1;
            for (auto dim : shape) elem_count *= dim;
            
            file.seekg(data_offset + offset);
            tensor.data = read_fp16_tensor(file, elem_count);
            
            tensors[name] = std::move(tensor);
            
            std::cout << "  ✓ " << name << " ";
            for (size_t i = 0; i < shape.size(); i++) {
                std::cout << (i ? "x" : "[") << shape[i];
            }
            std::cout << "]" << std::endl;
        }
        
        file.close();
        std::cout << "\nAll " << tensors.size() << " tensors loaded" << std::endl;
    }
    
    // Matrix multiplication: C = A * B
    std::vector<float> matmul(const std::vector<float>& A, const Tensor& B,
                              size_t m, size_t k, size_t n) {
        std::vector<float> C(m * n, 0.0f);
        
        for (size_t i = 0; i < m; i++) {
            for (size_t j = 0; j < n; j++) {
                float sum = 0.0f;
                for (size_t p = 0; p < k; p++) {
                    sum += A[i * k + p] * B.data[p * n + j];
                }
                C[i * n + j] = sum;
            }
        }
        return C;
    }
    
    // RMS normalization
    std::vector<float> rms_norm(const std::vector<float>& x, const Tensor& weight, float eps = 1e-5f) {
        size_t size = x.size();
        
        float sum_sq = 0.0f;
        for (float val : x) sum_sq += val * val;
        float rms = std::sqrt(sum_sq / size + eps);
        
        std::vector<float> output(size);
        for (size_t i = 0; i < size; i++) {
            output[i] = (x[i] / rms) * weight.data[i];
        }
        return output;
    }
    
    // SiLU activation
    float silu(float x) {
        return x / (1.0f + std::exp(-x));
    }
    
    // Softmax
    std::vector<float> softmax(const std::vector<float>& x) {
        float max_val = *std::max_element(x.begin(), x.end());
        std::vector<float> exp_vals(x.size());
        float sum = 0.0f;
        
        for (size_t i = 0; i < x.size(); i++) {
            exp_vals[i] = std::exp(x[i] - max_val);
            sum += exp_vals[i];
        }
        
        for (size_t i = 0; i < x.size(); i++) {
            exp_vals[i] /= sum;
        }
        return exp_vals;
    }
    
    // RoPE (Rotary Position Embedding)
    void apply_rope(std::vector<float>& q, std::vector<float>& k, int pos) {
        size_t head_dim = n_embd / n_head;
        
        for (size_t i = 0; i < q.size(); i += 2) {
            float freq = 1.0f / std::pow(10000.0f, float(i) / head_dim);
            float angle = pos * freq;
            float cos_val = std::cos(angle);
            float sin_val = std::sin(angle);
            
            float q0 = q[i];
            float q1 = q[i + 1];
            q[i] = q0 * cos_val - q1 * sin_val;
            q[i + 1] = q0 * sin_val + q1 * cos_val;
            
            if (i < k.size()) {
                float k0 = k[i];
                float k1 = k[i + 1];
                k[i] = k0 * cos_val - k1 * sin_val;
                k[i + 1] = k0 * sin_val + k1 * cos_val;
            }
        }
    }
    
    // Single transformer layer forward pass
    std::vector<float> transformer_layer(const std::vector<float>& x, int layer) {
        std::string prefix = "blk." + std::to_string(layer) + ".";
        
        // Attention normalization
        auto attn_norm = rms_norm(x, tensors[prefix + "attn_norm.weight"]);
        
        // Q, K, V projections
        auto q = matmul(attn_norm, tensors[prefix + "attn_q.weight"], 1, n_embd, n_embd);
        auto k = matmul(attn_norm, tensors[prefix + "attn_k.weight"], 1, n_embd, n_embd / n_head);
        auto v = matmul(attn_norm, tensors[prefix + "attn_v.weight"], 1, n_embd, n_embd / n_head);
        
        // Apply RoPE
        apply_rope(q, k, 0);
        
        // Compute attention scores
        size_t head_dim = n_embd / n_head;
        std::vector<float> scores(k.size() / head_dim);
        for (size_t i = 0; i < scores.size(); i++) {
            float score = 0.0f;
            for (size_t j = 0; j < head_dim; j++) {
                score += q[j] * k[i * head_dim + j];
            }
            scores[i] = score / std::sqrt(float(head_dim));
        }
        
        // Apply softmax
        auto attn_weights = softmax(scores);
        
        // Apply attention to values
        std::vector<float> attn_out(n_embd, 0.0f);
        for (size_t i = 0; i < attn_weights.size(); i++) {
            for (size_t j = 0; j < v.size(); j++) {
                attn_out[j] += attn_weights[i] * v[j];
            }
        }
        
        // Output projection
        auto attn_proj = matmul(attn_out, tensors[prefix + "attn_output.weight"], 1, n_embd, n_embd);
        
        // Residual connection
        std::vector<float> h(n_embd);
        for (size_t i = 0; i < n_embd; i++) {
            h[i] = x[i] + attn_proj[i];
        }
        
        // FFN normalization
        auto ffn_norm = rms_norm(h, tensors[prefix + "ffn_norm.weight"]);
        
        // FFN: gate, up, down projections
        size_t ffn_dim = tensors[prefix + "ffn_gate.weight"].shape[1];
        auto gate = matmul(ffn_norm, tensors[prefix + "ffn_gate.weight"], 1, n_embd, ffn_dim);
        auto up = matmul(ffn_norm, tensors[prefix + "ffn_up.weight"], 1, n_embd, ffn_dim);
        
        // SiLU activation
        std::vector<float> ffn_hidden(ffn_dim);
        for (size_t i = 0; i < ffn_dim; i++) {
            ffn_hidden[i] = silu(gate[i]) * up[i];
        }
        
        // Down projection
        auto ffn_out = matmul(ffn_hidden, tensors[prefix + "ffn_down.weight"], 1, ffn_dim, n_embd);
        
        // Final residual
        for (size_t i = 0; i < n_embd; i++) {
            h[i] += ffn_out[i];
        }
        
        return h;
    }
    
    std::vector<float> infer(const std::vector<int>& tokens) {
        std::cout << "\n=== Running Full Transformer Inference ===" << std::endl;
        std::cout << "Input tokens: " << tokens.size() << std::endl;
        
        // Token embedding
        std::vector<float> x(n_embd, 0.0f);
        int token = tokens.empty() ? 0 : tokens[0];
        
        if (tensors.find("token_embd.weight") != tensors.end()) {
            const auto& emb_tensor = tensors["token_embd.weight"];
            size_t token_idx = std::min(size_t(token), emb_tensor.shape[0] - 1);
            std::copy_n(emb_tensor.data.begin() + token_idx * n_embd, n_embd, x.begin());
        }
        
        std::cout << "Embedding lookup complete" << std::endl;
        
        // Process through all transformer layers
        for (uint64_t i = 0; i < n_layer; i++) {
            std::cout << "Layer " << i << "..." << std::flush;
            x = transformer_layer(x, i);
            std::cout << " ✓" << std::endl;
        }
        
        // Final normalization
        if (tensors.find("output_norm.weight") != tensors.end()) {
            x = rms_norm(x, tensors["output_norm.weight"]);
        }
        
        // Output projection
        std::vector<float> logits(n_vocab, 0.0f);
        if (tensors.find("output.weight") != tensors.end()) {
            logits = matmul(x, tensors["output.weight"], 1, n_embd, n_vocab);
        }
        
        std::cout << "Inference complete" << std::endl;
        return logits;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "GGUF FP16 Full Inference Engine\n";
        std::cout << "Usage: " << argv[0] << " <model.gguf> [token_ids]\n";
        std::cout << "Example: " << argv[0] << " model.gguf 1,2,3\n";
        return 1;
    }
    
    try {
        GGUFModel model(argv[1]);
        
        auto start = std::chrono::high_resolution_clock::now();
        model.load();
        auto end = std::chrono::high_resolution_clock::now();
        auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\nModel loaded in " << load_time.count() << " ms" << std::endl;
        
        // Parse input tokens
        std::vector<int> tokens = {1}; // Default BOS token
        if (argc > 2) {
            std::string token_str = argv[2];
            std::istringstream ss(token_str);
            std::string token;
            tokens.clear();
            while (std::getline(ss, token, ',')) {
                tokens.push_back(std::stoi(token));
            }
        }
        
        start = std::chrono::high_resolution_clock::now();
        auto logits = model.infer(tokens);
        end = std::chrono::high_resolution_clock::now();
        auto infer_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "\n=== Results ===" << std::endl;
        std::cout << "Inference time: " << infer_time.count() << " ms" << std::endl;
        std::cout << "Output logits: " << logits.size() << std::endl;
        
        // Show top 10 predicted tokens
        std::vector<std::pair<float, int>> top_logits;
        for (size_t i = 0; i < logits.size(); i++) {
            top_logits.push_back({logits[i], i});
        }
        std::partial_sort(top_logits.begin(), top_logits.begin() + 10, top_logits.end(),
                         [](auto& a, auto& b) { return a.first > b.first; });
        
        std::cout << "\nTop 10 predictions:" << std::endl;
        for (int i = 0; i < 10; i++) {
            std::cout << "  Token " << top_logits[i].second 
                     << ": " << top_logits[i].first << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}