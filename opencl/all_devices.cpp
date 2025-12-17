#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>

#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_TARGET_OPENCL_VERSION 120
#include <CL/opencl.hpp>

// Structure to hold device information and resources
struct DeviceContext {
    cl::Device device;
    cl::Context context;
    cl::CommandQueue queue;
    cl::Kernel kernel;
    std::string name;
    cl_device_type type;
    size_t work_size;
};

// OpenCL kernel source
const std::string kernel_source = R"(
__kernel void vector_add(__global const float* a,
                        __global const float* b,
                        __global float* c) {
    const int gid = get_global_id(0);
    c[gid] = a[gid] + b[gid];
}
)";

int main() {
    try {
        // Discover platforms
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        if (platforms.empty()) {
            std::cerr << "No OpenCL platforms found!" << std::endl;
            return 1;
        }

        std::vector<DeviceContext> device_contexts;

        // Collect all CPU and GPU devices
        for (auto &platform : platforms) {
            std::vector<cl::Device> platform_devices;
            
            // Get GPUs
            try {
                platform.getDevices(CL_DEVICE_TYPE_GPU, &platform_devices);
                for (auto &device : platform_devices) {
                    DeviceContext dc;
                    dc.device = device;
                    dc.type = CL_DEVICE_TYPE_GPU;
                    dc.name = device.getInfo<CL_DEVICE_NAME>();
                    device_contexts.push_back(dc);
                }
            } catch (...) {} // Ignore if no GPUs found
            
            // Get CPUs
            try {
                platform.getDevices(CL_DEVICE_TYPE_CPU, &platform_devices);
                for (auto &device : platform_devices) {
                    DeviceContext dc;
                    dc.device = device;
                    dc.type = CL_DEVICE_TYPE_CPU;
                    dc.name = device.getInfo<CL_DEVICE_NAME>();
                    device_contexts.push_back(dc);
                }
            } catch (...) {} // Ignore if no CPUs found
        }

        if (device_contexts.empty()) {
            std::cerr << "No compatible devices found!" << std::endl;
            return 1;
        }

        // Initialize device contexts
        cl::Program::Sources sources;
        sources.push_back({kernel_source.c_str(), kernel_source.length()});
        
        for (auto &dc : device_contexts) {
            try {
                // Create context
                dc.context = cl::Context(dc.device);
                
                // Build program
                cl::Program program(dc.context, sources);
                program.build("-cl-std=CL1.2");
                
                // Create command queue and kernel
                dc.queue = cl::CommandQueue(dc.context, dc.device);
                dc.kernel = cl::Kernel(program, "vector_add");
                
                std::cout << "Initialized device: " << dc.name 
                          << " (" << ((dc.type == CL_DEVICE_TYPE_GPU) ? "GPU" : "CPU") << ")"
                          << std::endl;
            } catch (const cl::Error &e) {
                std::cerr << "Error initializing device " << dc.name << ": "
                          << e.what() << " (" << e.err() << ")" << std::endl;
                
                // Get build log if available
                if (e.err() == CL_BUILD_PROGRAM_FAILURE) {
                    cl::Program program = dc.kernel.getInfo<CL_KERNEL_PROGRAM>();
                    std::string build_log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dc.device);
                    std::cerr << "Build log:\n" << build_log << std::endl;
                }
            }
        }

        // Problem parameters
        const size_t N = 1024 * 1024;  // 1M elements
        const size_t data_size = N * sizeof(float);
        std::vector<float> A(N, 1.0f);  // Input vector A
        std::vector<float> B(N, 2.0f);  // Input vector B
        std::vector<float> C(N, 0.0f);  // Output vector C

        // Work distribution
        size_t total_work = N;
        for (auto &dc : device_contexts) {
            dc.work_size = total_work / device_contexts.size();
            total_work -= dc.work_size;
        }
        // Distribute remainder
        device_contexts.front().work_size += total_work;

        // Process data on each device
        size_t offset = 0;
        for (auto &dc : device_contexts) {
            if (dc.work_size == 0) continue;
            
            try {
                // Create device buffers
                cl::Buffer buffer_A(dc.context, CL_MEM_READ_ONLY, dc.work_size * sizeof(float));
                cl::Buffer buffer_B(dc.context, CL_MEM_READ_ONLY, dc.work_size * sizeof(float));
                cl::Buffer buffer_C(dc.context, CL_MEM_WRITE_ONLY, dc.work_size * sizeof(float));

                // Write data to device
                dc.queue.enqueueWriteBuffer(buffer_A, CL_TRUE, 0, 
                                          dc.work_size * sizeof(float), 
                                          &A[offset]);
                dc.queue.enqueueWriteBuffer(buffer_B, CL_TRUE, 0, 
                                          dc.work_size * sizeof(float), 
                                          &B[offset]);

                // Set kernel arguments
                dc.kernel.setArg(0, buffer_A);
                dc.kernel.setArg(1, buffer_B);
                dc.kernel.setArg(2, buffer_C);

                // Execute kernel
                dc.queue.enqueueNDRangeKernel(dc.kernel, cl::NullRange, 
                                            cl::NDRange(dc.work_size), 
                                            cl::NullRange);

                // Read results
                dc.queue.enqueueReadBuffer(buffer_C, CL_TRUE, 0, 
                                         dc.work_size * sizeof(float), 
                                         &C[offset]);

                std::cout << "Processed " << dc.work_size << " elements on "
                          << dc.name << std::endl;
                
            } catch (const cl::Error &e) {
                std::cerr << "Error processing on " << dc.name << ": "
                          << e.what() << " (" << e.err() << ")" << std::endl;
            }
            
            offset += dc.work_size;
        }

        // Verify results
        bool correct = true;
        for (size_t i = 0; i < N; ++i) {
            if (C[i] != 3.0f) {
                correct = false;
                break;
            }
        }

        if (correct) {
            std::cout << "Vector addition succeeded!" << std::endl;
        } else {
            std::cerr << "Vector addition failed!" << std::endl;
        }

    } catch (const cl::Error &e) {
        std::cerr << "OpenCL error: " << e.what() 
                  << " (" << e.err() << ")" << std::endl;
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}