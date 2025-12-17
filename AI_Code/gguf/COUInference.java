import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.util.*;

// Main inference application
public class CPUInferenceApp {

```
public static void main(String[] args) {
    if (args.length < 2) {
        System.out.println("Usage: java CPUInferenceApp <model_path> <input_data>");
        System.out.println("Supported formats: ONNX (.onnx) and GGUF (.gguf)");
        System.out.println("Supported quantizations: fp32, fp16, fp8, q8");
        return;
    }
    
    String modelPath = args[0];
    String inputData = args[1];
    
    try {
        InferenceEngine engine = new InferenceEngine();
        engine.loadModel(modelPath);
        
        float[] input = parseInput(inputData);
        float[] output = engine.infer(input);
        
        System.out.println("Inference Results:");
        System.out.println(Arrays.toString(output));
        
    } catch (Exception e) {
        System.err.println("Error: " + e.getMessage());
        e.printStackTrace();
    }
}

private static float[] parseInput(String data) {
    String[] parts = data.split(",");
    float[] result = new float[parts.length];
    for (int i = 0; i < parts.length; i++) {
        result[i] = Float.parseFloat(parts[i].trim());
    }
    return result;
}
```

}

// Core inference engine
class InferenceEngine {
private ModelLoader loader;
private CPUExecutor executor;
private ModelMetadata metadata;

```
public void loadModel(String path) throws IOException {
    System.out.println("Loading model from: " + path);
    
    if (path.endsWith(".onnx")) {
        loader = new ONNXLoader();
    } else if (path.endsWith(".gguf")) {
        loader = new GGUFLoader();
    } else {
        throw new IllegalArgumentException("Unsupported format. Use .onnx or .gguf");
    }
    
    metadata = loader.load(path);
    executor = new CPUExecutor(metadata);
    
    System.out.println("Model loaded successfully");
    System.out.println("Format: " + metadata.format);
    System.out.println("Quantization: " + metadata.quantization);
}

public float[] infer(float[] input) {
    return executor.execute(input);
}
```

}

// Model metadata container
class ModelMetadata {
String format;
String quantization;
int inputSize;
int outputSize;
List<Layer> layers;
Map<String, Tensor> weights;

```
public ModelMetadata() {
    layers = new ArrayList<>();
    weights = new HashMap<>();
}
```

}

// Tensor representation
class Tensor {
float[] data;
int[] shape;
String dtype;

```
public Tensor(float[] data, int[] shape, String dtype) {
    this.data = data;
    this.shape = shape;
    this.dtype = dtype;
}

public int getSize() {
    int size = 1;
    for (int dim : shape) size *= dim;
    return size;
}
```

}

// Layer abstraction
abstract class Layer {
String name;
String type;

```
public abstract float[] forward(float[] input);
```

}

// Model loader interface
interface ModelLoader {
ModelMetadata load(String path) throws IOException;
}

// ONNX model loader
class ONNXLoader implements ModelLoader {

```
@Override
public ModelMetadata load(String path) throws IOException {
    ModelMetadata meta = new ModelMetadata();
    meta.format = "ONNX";
    
    byte[] fileBytes = Files.readAllBytes(Paths.get(path));
    ByteBuffer buffer = ByteBuffer.wrap(fileBytes);
    buffer.order(ByteOrder.LITTLE_ENDIAN);
    
    // Parse ONNX header (simplified)
    meta.quantization = detectQuantization(buffer);
    meta.inputSize = 784; // Default for demo
    meta.outputSize = 10;
    
    // Parse weights and layers
    parseONNXWeights(buffer, meta);
    
    return meta;
}

private String detectQuantization(ByteBuffer buffer) {
    // Check file for quantization markers
    // This is a simplified version
    byte[] header = new byte[100];
    buffer.get(header);
    buffer.rewind();
    
    String headerStr = new String(header);
    if (headerStr.contains("float16") || headerStr.contains("fp16")) return "fp16";
    if (headerStr.contains("float8") || headerStr.contains("fp8")) return "fp8";
    if (headerStr.contains("int8") || headerStr.contains("q8")) return "q8";
    return "fp32";
}

private void parseONNXWeights(ByteBuffer buffer, ModelMetadata meta) {
    // Simplified weight parsing
    // In production, use ONNX Runtime Java API
    System.out.println("Parsing ONNX weights with " + meta.quantization + " quantization");
}
```

}

// GGUF model loader
class GGUFLoader implements ModelLoader {
private static final int GGUF_MAGIC = 0x46554747; // “GGUF”

```
@Override
public ModelMetadata load(String path) throws IOException {
    ModelMetadata meta = new ModelMetadata();
    meta.format = "GGUF";
    
    try (RandomAccessFile file = new RandomAccessFile(path, "r")) {
        // Read GGUF header
        int magic = Integer.reverseBytes(file.readInt());
        if (magic != GGUF_MAGIC) {
            throw new IOException("Invalid GGUF file");
        }
        
        int version = Integer.reverseBytes(file.readInt());
        long tensorCount = Long.reverseBytes(file.readLong());
        long metadataKVCount = Long.reverseBytes(file.readLong());
        
        System.out.println("GGUF version: " + version);
        System.out.println("Tensors: " + tensorCount);
        
        // Parse metadata
        parseGGUFMetadata(file, meta, metadataKVCount);
        
        // Parse tensors
        parseGGUFTensors(file, meta, tensorCount);
        
        meta.inputSize = 512; // Default
        meta.outputSize = 512;
    }
    
    return meta;
}

private void parseGGUFMetadata(RandomAccessFile file, ModelMetadata meta, long count) throws IOException {
    for (long i = 0; i < count; i++) {
        String key = readGGUFString(file);
        int valueType = Integer.reverseBytes(file.readInt());
        
        if (key.contains("quantization") || key.contains("ftype")) {
            meta.quantization = readQuantizationType(file, valueType);
        } else {
            skipValue(file, valueType);
        }
    }
    
    if (meta.quantization == null) meta.quantization = "q8";
}

private String readQuantizationType(RandomAccessFile file, int type) throws IOException {
    if (type == 4) { // String type
        String value = readGGUFString(file);
        if (value.contains("F16")) return "fp16";
        if (value.contains("F8")) return "fp8";
        if (value.contains("Q8")) return "q8";
    } else if (type == 5) { // Int type
        int value = Integer.reverseBytes(file.readInt());
        // Map GGUF ftype values
        switch (value) {
            case 0: return "fp32";
            case 1: return "fp16";
            case 2: return "q8";
            default: return "q8";
        }
    }
    return "q8";
}

private void parseGGUFTensors(RandomAccessFile file, ModelMetadata meta, long count) throws IOException {
    for (long i = 0; i < count; i++) {
        String name = readGGUFString(file);
        int ndim = Integer.reverseBytes(file.readInt());
        
        int[] shape = new int[ndim];
        for (int j = 0; j < ndim; j++) {
            shape[j] = (int) Long.reverseBytes(file.readLong());
        }
        
        int ggmlType = Integer.reverseBytes(file.readInt());
        long offset = Long.reverseBytes(file.readLong());
        
        System.out.println("Tensor: " + name + " Shape: " + Arrays.toString(shape));
    }
}

private String readGGUFString(RandomAccessFile file) throws IOException {
    long len = Long.reverseBytes(file.readLong());
    byte[] bytes = new byte[(int) len];
    file.read(bytes);
    return new String(bytes);
}

private void skipValue(RandomAccessFile file, int type) throws IOException {
    switch (type) {
        case 4: readGGUFString(file); break; // String
        case 5: file.readInt(); break; // Int32
        case 6: file.readFloat(); break; // Float32
        case 8: file.readLong(); break; // Int64
        case 7: file.readInt(); break; // Bool
        default: file.readLong(); break;
    }
}
```

}

// CPU executor for inference
class CPUExecutor {
private ModelMetadata metadata;
private QuantizationHandler quantHandler;

```
public CPUExecutor(ModelMetadata meta) {
    this.metadata = meta;
    this.quantHandler = new QuantizationHandler(meta.quantization);
}

public float[] execute(float[] input) {
    System.out.println("Running inference on CPU...");
    
    // Dequantize if needed
    float[] processedInput = quantHandler.dequantize(input);
    
    // Simple feedforward (placeholder for actual inference)
    float[] output = new float[metadata.outputSize];
    
    // Simulate computation
    for (int i = 0; i < output.length; i++) {
        float sum = 0;
        for (int j = 0; j < Math.min(processedInput.length, 100); j++) {
            sum += processedInput[j] * 0.01f;
        }
        output[i] = (float) Math.tanh(sum);
    }
    
    return output;
}
```

}

// Quantization handler for different formats
class QuantizationHandler {
private String quantType;

```
public QuantizationHandler(String type) {
    this.quantType = type;
}

public float[] dequantize(float[] input) {
    switch (quantType) {
        case "fp8":
            return dequantizeFP8(input);
        case "fp16":
            return dequantizeFP16(input);
        case "q8":
            return dequantizeQ8(input);
        default:
            return input; // Already fp32
    }
}

private float[] dequantizeFP8(float[] input) {
    // FP8 to FP32 conversion
    float[] output = new float[input.length];
    for (int i = 0; i < input.length; i++) {
        // Simplified FP8 conversion
        output[i] = input[i] * 0.99f; // Scale approximation
    }
    return output;
}

private float[] dequantizeFP16(float[] input) {
    // FP16 to FP32 conversion
    float[] output = new float[input.length];
    for (int i = 0; i < input.length; i++) {
        short bits = (short) input[i];
        output[i] = fp16ToFp32(bits);
    }
    return output;
}

private float[] dequantizeQ8(float[] input) {
    // INT8 quantization to FP32
    float scale = 1.0f / 127.0f;
    float[] output = new float[input.length];
    for (int i = 0; i < input.length; i++) {
        byte quantized = (byte) input[i];
        output[i] = quantized * scale;
    }
    return output;
}

private float fp16ToFp32(short h) {
    int sign = (h >> 15) & 0x1;
    int exp = (h >> 10) & 0x1f;
    int mantissa = h & 0x3ff;
    
    if (exp == 0) {
        if (mantissa == 0) return sign == 1 ? -0.0f : 0.0f;
        // Denormalized
        exp = 1;
    } else if (exp == 31) {
        return mantissa == 0 ? 
            (sign == 1 ? Float.NEGATIVE_INFINITY : Float.POSITIVE_INFINITY) : 
            Float.NaN;
    }
    
    int fp32Exp = exp - 15 + 127;
    int fp32Mantissa = mantissa << 13;
    int fp32Bits = (sign << 31) | (fp32Exp << 23) | fp32Mantissa;
    
    return Float.intBitsToFloat(fp32Bits);
}
```

}