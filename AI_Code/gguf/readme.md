Key Features:
	•	Multi-format support: Handles both .onnx and .gguf files
	•	Quantization handling: Supports fp8, fp16, q8, and fp32 with proper dequantization
	•	CPU-only execution: No GPU dependencies
	•	Modular architecture: Separate loaders for each format, pluggable quantization handlers
To compile and run:

Architecture:
	1.	InferenceEngine: Main coordinator that selects the appropriate loader
	2.	ONNXLoader/GGUFLoader: Parse model files and detect quantization
	3.	CPUExecutor: Runs inference computations on CPU
	4.	QuantizationHandler: Converts quantized values to fp32 for computation
Note: This is a framework implementation. For production use, you’d want to:
	•	Integrate ONNX Runtime Java API for actual ONNX execution
	•	Add proper tensor operations library (like ND4J)
	•	Implement full GGML quantization schemes
	•	Add layer-by-layer execution logic
The code handles file parsing, quantization detection, and provides the structure for CPU-based inference. Would you like me to expand any specific part, such as adding more detailed tensor operations or specific layer implementations?​​​​​​​​​​​​​​​​