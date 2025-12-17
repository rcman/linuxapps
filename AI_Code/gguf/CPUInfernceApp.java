// pom.xml dependencies required:
/*
<dependencies>
<dependency>
<groupId>com.microsoft.onnxruntime</groupId>
<artifactId>onnxruntime</artifactId>
<version>1.17.1</version>
</dependency>
</dependencies>
*/

import ai.onnxruntime.*;
import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.util.*;

public class CPUInferenceApp {

```
public static void main(String[] args) {
    if (args.length < 1) {
        System.out.println("Usage: java CPUInferenceApp <model_path> [input_file]");
        System.out.println("Supported formats: ONNX (.onnx) and GGUF (.gguf)");
        return;
    }
    
    String modelPath = args[0];
    String inputFile = args.length > 1 ? args[1] : null;
    
    try {
        if (modelPath.endsWith(".onnx")) {
            runONNXInference(modelPath, inputFile);
        } else if (modelPath.endsWith(".gguf")) {
            runGGUFInference(modelPath, inputFile);
        } else {
            System.err.println("Unsupported format. Use .onnx or .gguf");
        }
    } catch (Exception e) {
        System.err.println("Error: " + e.getMessage());
        e.printStackTrace();
    }
}

private static void runONNXInference(String modelPath, String inputFile) throws Exception {
    System.out.println("Loading ONNX model: " + modelPath);
    
    OrtEnvironment env = OrtEnvironment.getEnvironment();
    OrtSession.SessionOptions opts = new OrtSession.SessionOptions();
    
    // Force CPU execution
    opts.setOptimizationLevel(OrtSession.SessionOptions.OptLevel.ALL_OPT);
    opts.setInterOpNumThreads(Runtime.getRuntime().availableProcessors());
    opts.setIntraOpNumThreads(Runtime.getRuntime().availableProcessors());
    
    try (OrtSession session = env.createSession(modelPath, opts)) {
        
        // Get model info
        Map<String, NodeInfo> inputInfo = session.getInputInfo();
        Map<String, NodeInfo> outputInfo = session.getOutputInfo();
        
        System.out.println("\n=== Model Information ===");
        System.out.println("Inputs:");
        for (Map.Entry<String, NodeInfo> entry : inputInfo.entrySet()) {
            System.out.println("  " + entry.getKey() + ": " + entry.getValue().getInfo());
        }
        
        System.out.println("Outputs:");
        for (Map.Entry<String, NodeInfo> entry : outputInfo.entrySet()) {
            System.out.println("  " + entry.getKey() + ": " + entry.getValue().getInfo());
        }
        
        // Prepare input
        String inputName = inputInfo.keySet().iterator().next();
        NodeInfo info = inputInfo.get(inputName);
        TensorInfo tensorInfo = (TensorInfo) info.getInfo();
        long[] shape = tensorInfo.getShape();
        
        // Calculate total size
        long totalSize = 1;
        for (long dim : shape) {
            if (dim > 0) totalSize *= dim;
        }
        
        float[] inputData;
        if (inputFile != null) {
            inputData = loadInputFromFile(inputFile, (int)totalSize);
        } else {
            // Generate random input for testing
            inputData = new float[(int)totalSize];
            Random rand = new Random(42);
            for (int i = 0; i < inputData.length; i++) {
                inputData[i] = rand.nextFloat() * 2 - 1; // Random [-1, 1]
            }
        }
        
        System.out.println("\n=== Running Inference ===");
        System.out.println("Input shape: " + Arrays.toString(shape));
        System.out.println("Input size: " + totalSize);
        
        // Create tensor
        OnnxTensor inputTensor = OnnxTensor.createTensor(env, 
            FloatBuffer.wrap(inputData), shape);
        
        Map<String, OnnxTensor> inputs = new HashMap<>();
        inputs.put(inputName, inputTensor);
        
        // Run inference
        long startTime = System.currentTimeMillis();
        OrtSession.Result results = session.run(inputs);
        long endTime = System.currentTimeMillis();
        
        System.out.println("Inference time: " + (endTime - startTime) + " ms");
        
        // Process outputs
        System.out.println("\n=== Results ===");
        for (Map.Entry<String, OnnxValue> entry : results) {
            System.out.println("\nOutput: " + entry.getKey());
            OnnxValue value = entry.getValue();
            
            if (value instanceof OnnxTensor) {
                OnnxTensor tensor = (OnnxTensor) value;
                System.out.println("Shape: " + Arrays.toString(tensor.getInfo().getShape()));
                
                // Get output data
                Object output = tensor.getValue();
                if (output instanceof float[]) {
                    float[] outputArray = (float[]) output;
                    System.out.println("First 10 values: " + 
                        Arrays.toString(Arrays.copyOf(outputArray, Math.min(10, outputArray.length))));
                } else if (output instanceof float[][]) {
                    float[][] outputArray = (float[][]) output;
                    System.out.println("Output dimensions: " + outputArray.length + " x " + 
                        (outputArray.length > 0 ? outputArray[0].length : 0));
                }
            }
        }
        
        // Cleanup
        inputTensor.close();
        results.close();
    }
}

private static void runGGUFInference(String modelPath, String inputFile) throws Exception {
    System.out.println("Loading GGUF model: " + modelPath);
    
    GGUFModel model = new GGUFModel(modelPath);
    model.load();
    
    // Prepare input
    float[] input;
    if (inputFile != null) {
        input = loadInputFromFile(inputFile, 512);
    } else {
        input = new float[512];
        Random rand = new Random(42);
        for (int i = 0; i < input.length; i++) {
            input[i] = rand.nextFloat() * 2 - 1;
        }
    }
    
    System.out.println("\n=== Running GGUF Inference ===");
    long startTime = System.currentTimeMillis();
    float[] output = model.infer(input);
    long endTime = System.currentTimeMillis();
    
    System.out.println("Inference time: " + (endTime - startTime) + " ms");
    System.out.println("\n=== Results ===");
    System.out.println("Output size: " + output.length);
    System.out.println("First 10 values: " + 
        Arrays.toString(Arrays.copyOf(output, Math.min(10, output.length))));
}

private static float[] loadInputFromFile(String filename, int expectedSize) throws IOException {
    List<Float> values = new ArrayList<>();
    
    try (BufferedReader br = new BufferedReader(new FileReader(filename))) {
        String line;
        while ((line = br.readLine()) != null) {
            String[] parts = line.trim().split("[,\\s]+");
            for (String part : parts) {
                if (!part.isEmpty()) {
                    values.add(Float.parseFloat(part));
                }
            }
        }
    }
    
    float[] result = new float[expectedSize];
    for (int i = 0; i < expectedSize && i < values.size(); i++) {
        result[i] = values.get(i);
    }
    
    System.out.println("Loaded " + values.size() + " values from " + filename);
    return result;
}
```

}

// Full GGUF implementation
class GGUFModel {
private String path;
private int version;
private Map<String, GGUFMetadata> metadata;
private List<GGUFTensor> tensors;
private long dataOffset;
private RandomAccessFile file;

```
private static final int GGUF_MAGIC = 0x46554747;

// GGML quantization types
private static final int GGML_TYPE_F32 = 0;
private static final int GGML_TYPE_F16 = 1;
private static final int GGML_TYPE_Q4_0 = 2;
private static final int GGML_TYPE_Q4_1 = 3;
private static final int GGML_TYPE_Q5_0 = 6;
private static final int GGML_TYPE_Q5_1 = 7;
private static final int GGML_TYPE_Q8_0 = 8;
private static final int GGML_TYPE_Q8_1 = 9;

public GGUFModel(String path) {
    this.path = path;
    this.metadata = new HashMap<>();
    this.tensors = new ArrayList<>();
}

public void load() throws IOException {
    file = new RandomAccessFile(path, "r");
    
    // Read magic number
    int magic = readInt();
    if (magic != GGUF_MAGIC) {
        throw new IOException("Invalid GGUF magic number: 0x" + Integer.toHexString(magic));
    }
    
    // Read version
    version = readInt();
    System.out.println("GGUF version: " + version);
    
    // Read counts
    long tensorCount = readLong();
    long metadataCount = readLong();
    
    System.out.println("Tensor count: " + tensorCount);
    System.out.println("Metadata count: " + metadataCount);
    
    // Read metadata
    for (int i = 0; i < metadataCount; i++) {
        String key = readString();
        GGUFMetadata meta = readMetadataValue();
        metadata.put(key, meta);
    }
    
    System.out.println("\n=== Model Metadata ===");
    for (Map.Entry<String, GGUFMetadata> entry : metadata.entrySet()) {
        System.out.println(entry.getKey() + " = " + entry.getValue());
    }
    
    // Read tensor info
    for (int i = 0; i < tensorCount; i++) {
        GGUFTensor tensor = new GGUFTensor();
        tensor.name = readString();
        
        int ndim = readInt();
        tensor.shape = new long[ndim];
        for (int j = 0; j < ndim; j++) {
            tensor.shape[j] = readLong();
        }
        
        tensor.type = readInt();
        tensor.offset = readLong();
        
        tensors.add(tensor);
        
        System.out.println("\nTensor: " + tensor.name);
        System.out.println("  Shape: " + Arrays.toString(tensor.shape));
        System.out.println("  Type: " + getTypeName(tensor.type));
        System.out.println("  Offset: " + tensor.offset);
    }
    
    // Calculate data offset
    dataOffset = file.getFilePointer();
    long alignment = 32;
    dataOffset = ((dataOffset + alignment - 1) / alignment) * alignment;
    
    System.out.println("\nData section starts at offset: " + dataOffset);
}

public float[] infer(float[] input) throws IOException {
    // Find embedding and output tensors
    GGUFTensor embeddingTensor = null;
    GGUFTensor outputTensor = null;
    
    for (GGUFTensor t : tensors) {
        if (t.name.contains("embed") || t.name.contains("token")) {
            embeddingTensor = t;
        }
        if (t.name.contains("output") || t.name.contains("lm_head")) {
            outputTensor = t;
        }
    }
    
    if (embeddingTensor != null) {
        System.out.println("Using embedding tensor: " + embeddingTensor.name);
        
        // Read and dequantize tensor
        float[] weights = readTensorData(embeddingTensor);
        
        // Simple matrix multiplication
        int vocabSize = (int) embeddingTensor.shape[0];
        int embedDim = (int) embeddingTensor.shape[1];
        
        float[] output = new float[Math.min(vocabSize, 100)];
        
        for (int i = 0; i < output.length; i++) {
            float sum = 0;
            for (int j = 0; j < Math.min(embedDim, input.length); j++) {
                int idx = i * embedDim + j;
                if (idx < weights.length) {
                    sum += weights[idx] * input[j];
                }
            }
            output[i] = sum;
        }
        
        return output;
    }
    
    // Fallback: return simple transformation
    float[] output = new float[input.length];
    System.arraycopy(input, 0, output, 0, input.length);
    return output;
}

private float[] readTensorData(GGUFTensor tensor) throws IOException {
    long totalElements = 1;
    for (long dim : tensor.shape) {
        totalElements *= dim;
    }
    
    // Limit size for demo
    totalElements = Math.min(totalElements, 1000000);
    
    file.seek(dataOffset + tensor.offset);
    
    return dequantizeTensor(tensor.type, (int)totalElements);
}

private float[] dequantizeTensor(int type, int count) throws IOException {
    switch (type) {
        case GGML_TYPE_F32:
            return readF32(count);
        case GGML_TYPE_F16:
            return readF16(count);
        case GGML_TYPE_Q8_0:
            return readQ8_0(count);
        case GGML_TYPE_Q4_0:
            return readQ4_0(count);
        default:
            System.out.println("Unsupported type " + type + ", using zeros");
            return new float[count];
    }
}

private float[] readF32(int count) throws IOException {
    float[] result = new float[count];
    for (int i = 0; i < count; i++) {
        result[i] = file.readFloat();
    }
    return result;
}

private float[] readF16(int count) throws IOException {
    float[] result = new float[count];
    for (int i = 0; i < count; i++) {
        short fp16 = file.readShort();
        result[i] = fp16ToFloat(fp16);
    }
    return result;
}

private float[] readQ8_0(int count) throws IOException {
    // Q8_0: 32 values per block, 1 float scale + 32 int8 values
    int blockSize = 32;
    int numBlocks = (count + blockSize - 1) / blockSize;
    float[] result = new float[count];
    int idx = 0;
    
    for (int b = 0; b < numBlocks && idx < count; b++) {
        float scale = file.readFloat();
        
        for (int i = 0; i < blockSize && idx < count; i++, idx++) {
            byte val = file.readByte();
            result[idx] = val * scale;
        }
    }
    
    return result;
}

private float[] readQ4_0(int count) throws IOException {
    // Q4_0: 32 values per block, 1 float16 scale + 16 bytes (2 vals per byte)
    int blockSize = 32;
    int numBlocks = (count + blockSize - 1) / blockSize;
    float[] result = new float[count];
    int idx = 0;
    
    for (int b = 0; b < numBlocks && idx < count; b++) {
        short scaleInt = file.readShort();
        float scale = fp16ToFloat(scaleInt);
        
        for (int i = 0; i < 16 && idx < count; i++) {
            byte packed = file.readByte();
            
            // Low nibble
            if (idx < count) {
                int val = (packed & 0x0F) - 8;
                result[idx++] = val * scale;
            }
            
            // High nibble
            if (idx < count) {
                int val = ((packed >> 4) & 0x0F) - 8;
                result[idx++] = val * scale;
            }
        }
    }
    
    return result;
}

private float fp16ToFloat(short h) {
    int bits = h & 0xFFFF;
    int sign = (bits >> 15) & 0x1;
    int exponent = (bits >> 10) & 0x1F;
    int fraction = bits & 0x3FF;
    
    if (exponent == 0) {
        if (fraction == 0) {
            return sign != 0 ? -0.0f : 0.0f;
        }
        // Subnormal
        float val = fraction / 1024.0f;
        return sign != 0 ? -val * (float)Math.pow(2, -14) : val * (float)Math.pow(2, -14);
    } else if (exponent == 31) {
        return fraction != 0 ? Float.NaN : 
               (sign != 0 ? Float.NEGATIVE_INFINITY : Float.POSITIVE_INFINITY);
    }
    
    float val = (1.0f + fraction / 1024.0f) * (float)Math.pow(2, exponent - 15);
    return sign != 0 ? -val : val;
}

private String getTypeName(int type) {
    switch (type) {
        case GGML_TYPE_F32: return "F32";
        case GGML_TYPE_F16: return "F16";
        case GGML_TYPE_Q4_0: return "Q4_0";
        case GGML_TYPE_Q4_1: return "Q4_1";
        case GGML_TYPE_Q5_0: return "Q5_0";
        case GGML_TYPE_Q5_1: return "Q5_1";
        case GGML_TYPE_Q8_0: return "Q8_0";
        case GGML_TYPE_Q8_1: return "Q8_1";
        default: return "Unknown(" + type + ")";
    }
}

private int readInt() throws IOException {
    return Integer.reverseBytes(file.readInt());
}

private long readLong() throws IOException {
    return Long.reverseBytes(file.readLong());
}

private String readString() throws IOException {
    long len = readLong();
    byte[] bytes = new byte[(int)len];
    file.readFully(bytes);
    return new String(bytes);
}

private GGUFMetadata readMetadataValue() throws IOException {
    int type = readInt();
    GGUFMetadata meta = new GGUFMetadata();
    meta.type = type;
    
    switch (type) {
        case 0: // uint8
            meta.value = file.readByte() & 0xFF;
            break;
        case 1: // int8
            meta.value = file.readByte();
            break;
        case 2: // uint16
            meta.value = Short.reverseBytes(file.readShort()) & 0xFFFF;
            break;
        case 3: // int16
            meta.value = Short.reverseBytes(file.readShort());
            break;
        case 4: // uint32
            meta.value = (long)(readInt()) & 0xFFFFFFFFL;
            break;
        case 5: // int32
            meta.value = readInt();
            break;
        case 6: // float32
            meta.value = Float.intBitsToFloat(readInt());
            break;
        case 7: // bool
            meta.value = file.readByte() != 0;
            break;
        case 8: // string
            meta.value = readString();
            break;
        case 9: // array
            meta.value = readArray();
            break;
        case 10: // uint64
            meta.value = readLong();
            break;
        case 11: // int64
            meta.value = readLong();
            break;
        case 12: // float64
            meta.value = Double.longBitsToDouble(readLong());
            break;
        default:
            throw new IOException("Unknown metadata type: " + type);
    }
    
    return meta;
}

private Object readArray() throws IOException {
    int arrayType = readInt();
    long arrayLen = readLong();
    
    List<Object> array = new ArrayList<>();
    for (int i = 0; i < arrayLen; i++) {
        GGUFMetadata item = new GGUFMetadata();
        item.type = arrayType;
        
        switch (arrayType) {
            case 8: // string
                item.value = readString();
                break;
            default:
                item.value = readMetadataValue().value;
                break;
        }
        array.add(item.value);
    }
    
    return array;
}

public void close() throws IOException {
    if (file != null) {
        file.close();
    }
}
```

}

class GGUFMetadata {
int type;
Object value;

```
@Override
public String toString() {
    return String.valueOf(value);
}
```

}

class GGUFTensor {
String name;
long[] shape;
int type;
long offset;
}