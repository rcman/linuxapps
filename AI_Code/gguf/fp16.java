import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.util.*;

public class CPUInferenceApp {

```
public static void main(String[] args) {
    if (args.length < 1) {
        System.out.println("Usage: java CPUInferenceApp <model.gguf> [input_file]");
        System.out.println("FP16 quantization only");
        return;
    }
    
    String modelPath = args[0];
    String inputFile = args.length > 1 ? args[1] : null;
    
    if (!modelPath.endsWith(".gguf")) {
        System.err.println("Error: Only GGUF files are supported");
        return;
    }
    
    try {
        GGUFModel model = new GGUFModel(modelPath);
        model.load();
        
        if (!model.isFP16()) {
            System.err.println("Error: Model is not FP16 quantized");
            System.err.println("This application only supports FP16 GGUF models");
            return;
        }
        
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
            System.out.println("Using random test input (512 values)");
        }
        
        System.out.println("\n=== Running FP16 Inference ===");
        long startTime = System.currentTimeMillis();
        float[] output = model.infer(input);
        long endTime = System.currentTimeMillis();
        
        System.out.println("Inference time: " + (endTime - startTime) + " ms");
        System.out.println("\n=== Results ===");
        System.out.println("Output size: " + output.length);
        System.out.println("First 10 values: " + 
            Arrays.toString(Arrays.copyOf(output, Math.min(10, output.length))));
        
        model.close();
        
    } catch (Exception e) {
        System.err.println("Error: " + e.getMessage());
        e.printStackTrace();
    }
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
    
    if (values.size() < expectedSize) {
        System.out.println("Warning: Input file has " + values.size() + 
                         " values, expected " + expectedSize + ". Padding with zeros.");
    }
    
    System.out.println("Loaded " + values.size() + " values from " + filename);
    return result;
}
```

}

class GGUFModel {
private String path;
private int version;
private Map<String, GGUFMetadata> metadata;
private List<GGUFTensor> tensors;
private long dataOffset;
private RandomAccessFile file;

```
private static final int GGUF_MAGIC = 0x46554747; // "GGUF"
private static final int GGML_TYPE_F16 = 1;

public GGUFModel(String path) {
    this.path = path;
    this.metadata = new HashMap<>();
    this.tensors = new ArrayList<>();
}

public void load() throws IOException {
    file = new RandomAccessFile(path, "r");
    
    // Read and verify magic number
    int magic = readInt();
    if (magic != GGUF_MAGIC) {
        throw new IOException("Invalid GGUF file. Magic number: 0x" + 
                            Integer.toHexString(magic) + ", expected: 0x" + 
                            Integer.toHexString(GGUF_MAGIC));
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
    
    // Read tensor information
    System.out.println("\n=== Tensors ===");
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
        
        System.out.println("\nTensor #" + (i + 1) + ": " + tensor.name);
        System.out.println("  Shape: " + Arrays.toString(tensor.shape));
        System.out.println("  Type: " + getTypeName(tensor.type));
        System.out.println("  Type Code: " + tensor.type);
        System.out.println("  Offset: " + tensor.offset);
    }
    
    // Calculate data section offset with alignment
    dataOffset = file.getFilePointer();
    long alignment = 32;
    dataOffset = ((dataOffset + alignment - 1) / alignment) * alignment;
    
    System.out.println("\nData section offset: " + dataOffset);
}

public boolean isFP16() {
    // Check if all tensors are FP16
    for (GGUFTensor tensor : tensors) {
        if (tensor.type != GGML_TYPE_F16) {
            return false;
        }
    }
    return !tensors.isEmpty();
}

public float[] infer(float[] input) throws IOException {
    if (tensors.isEmpty()) {
        throw new IOException("No tensors loaded");
    }
    
    // Find the first suitable tensor for inference
    GGUFTensor mainTensor = null;
    for (GGUFTensor t : tensors) {
        if (t.shape.length >= 2) {
            mainTensor = t;
            break;
        }
    }
    
    if (mainTensor == null) {
        mainTensor = tensors.get(0);
    }
    
    System.out.println("Using tensor for inference: " + mainTensor.name);
    System.out.println("Tensor shape: " + Arrays.toString(mainTensor.shape));
    
    // Read tensor weights (FP16 format)
    float[] weights = readTensorDataFP16(mainTensor);
    
    // Perform matrix multiplication
    // Assuming shape is [rows, cols] or [vocab_size, embedding_dim]
    long rows = mainTensor.shape.length > 0 ? mainTensor.shape[0] : 1;
    long cols = mainTensor.shape.length > 1 ? mainTensor.shape[1] : weights.length;
    
    int outputSize = Math.min((int)rows, 512);
    int inputSize = Math.min((int)cols, input.length);
    
    float[] output = new float[outputSize];
    
    System.out.println("Computing output: " + outputSize + " values");
    System.out.println("Using input size: " + inputSize);
    
    // Matrix-vector multiplication: output = weights * input
    for (int i = 0; i < outputSize; i++) {
        float sum = 0.0f;
        for (int j = 0; j < inputSize; j++) {
            int weightIdx = (int)(i * cols + j);
            if (weightIdx < weights.length) {
                sum += weights[weightIdx] * input[j];
            }
        }
        // Apply activation (tanh for bounded output)
        output[i] = (float) Math.tanh(sum);
    }
    
    return output;
}

private float[] readTensorDataFP16(GGUFTensor tensor) throws IOException {
    if (tensor.type != GGML_TYPE_F16) {
        throw new IOException("Tensor " + tensor.name + " is not FP16 (type: " + 
                            tensor.type + ")");
    }
    
    // Calculate total elements
    long totalElements = 1;
    for (long dim : tensor.shape) {
        totalElements *= dim;
    }
    
    // Limit size to prevent memory issues (max 10M elements)
    totalElements = Math.min(totalElements, 10_000_000);
    
    System.out.println("Reading " + totalElements + " FP16 values from offset " + 
                     (dataOffset + tensor.offset));
    
    // Seek to tensor data
    file.seek(dataOffset + tensor.offset);
    
    // Read FP16 values and convert to FP32
    float[] result = new float[(int)totalElements];
    for (int i = 0; i < totalElements; i++) {
        short fp16Bits = file.readShort();
        result[i] = fp16ToFloat(fp16Bits);
    }
    
    System.out.println("Successfully read and converted " + totalElements + 
                     " FP16 values to FP32");
    
    return result;
}

private float fp16ToFloat(short h) {
    // Extract sign, exponent, and mantissa from FP16
    int bits = h & 0xFFFF;
    int sign = (bits >> 15) & 0x1;
    int exponent = (bits >> 10) & 0x1F;
    int mantissa = bits & 0x3FF;
    
    // Handle special cases
    if (exponent == 0) {
        if (mantissa == 0) {
            // Zero
            return sign != 0 ? -0.0f : 0.0f;
        } else {
            // Subnormal numbers
            float value = mantissa / 1024.0f * (float)Math.pow(2, -14);
            return sign != 0 ? -value : value;
        }
    } else if (exponent == 31) {
        // Infinity or NaN
        if (mantissa == 0) {
            return sign != 0 ? Float.NEGATIVE_INFINITY : Float.POSITIVE_INFINITY;
        } else {
            return Float.NaN;
        }
    }
    
    // Normal numbers
    // FP16: sign * 2^(exp-15) * (1 + mantissa/1024)
    float value = (1.0f + mantissa / 1024.0f) * (float)Math.pow(2, exponent - 15);
    return sign != 0 ? -value : value;
}

private String getTypeName(int type) {
    switch (type) {
        case 0: return "F32";
        case 1: return "F16";
        case 2: return "Q4_0";
        case 3: return "Q4_1";
        case 6: return "Q5_0";
        case 7: return "Q5_1";
        case 8: return "Q8_0";
        case 9: return "Q8_1";
        default: return "Unknown(" + type + ")";
    }
}

// Binary reading utilities
private int readInt() throws IOException {
    return Integer.reverseBytes(file.readInt());
}

private long readLong() throws IOException {
    return Long.reverseBytes(file.readLong());
}

private String readString() throws IOException {
    long len = readLong();
    if (len < 0 || len > 1_000_000) {
        throw new IOException("Invalid string length: " + len);
    }
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
    
    if (arrayLen < 0 || arrayLen > 100_000) {
        throw new IOException("Invalid array length: " + arrayLen);
    }
    
    List<Object> array = new ArrayList<>();
    for (int i = 0; i < arrayLen; i++) {
        switch (arrayType) {
            case 8: // string array
                array.add(readString());
                break;
            case 5: // int32 array
                array.add(readInt());
                break;
            case 6: // float32 array
                array.add(Float.intBitsToFloat(readInt()));
                break;
            default:
                // For other types, read as generic metadata
                GGUFMetadata item = new GGUFMetadata();
                item.type = arrayType;
                item.value = readMetadataValue().value;
                array.add(item.value);
                break;
        }
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
    if (value instanceof List) {
        List<?> list = (List<?>) value;
        if (list.size() > 10) {
            return "[" + list.subList(0, 10) + "... (" + list.size() + " items)]";
        }
    }
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