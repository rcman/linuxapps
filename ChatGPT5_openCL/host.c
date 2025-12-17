// host.c
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CL/cl.h>

#define CHECK_ERR(err, msg) \
    if (err != CL_SUCCESS) { fprintf(stderr, "%s: %d\n", msg, (int)err); goto cleanup; }

static char *read_file_alloc(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
    buf[sz] = '\0';
    fclose(f);
    if (out_size) *out_size = (size_t)sz;
    return buf;
}

int main(int argc, char **argv) {
    cl_int err;
    cl_uint num_platforms = 0;
    err = clGetPlatformIDs(0, NULL, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0) {
        fprintf(stderr, "No OpenCL platforms found. (err=%d)\n", err);
        return 1;
    }

    cl_platform_id *platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * num_platforms);
    clGetPlatformIDs(num_platforms, platforms, NULL);

    // Try to pick an Intel platform/device
    cl_platform_id chosen_platform = NULL;
    cl_device_id chosen_device = NULL;

    for (cl_uint p = 0; p < num_platforms; ++p) {
        size_t vlen = 0;
        clGetPlatformInfo(platforms[p], CL_PLATFORM_NAME, 0, NULL, &vlen);
        char *pname = (char*)malloc(vlen+1);
        clGetPlatformInfo(platforms[p], CL_PLATFORM_NAME, vlen, pname, NULL);
        pname[vlen] = '\0';
        printf("Platform %u: %s\n", p, pname);

        // query GPU devices on this platform
        cl_uint num_devices = 0;
        if (clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices) != CL_SUCCESS || num_devices == 0) {
            free(pname);
            continue;
        }
        cl_device_id *devices = (cl_device_id*)malloc(sizeof(cl_device_id) * num_devices);
        clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_GPU, num_devices, devices, NULL);

        for (cl_uint d = 0; d < num_devices; ++d) {
            size_t dlen = 0;
            clGetDeviceInfo(devices[d], CL_DEVICE_VENDOR, 0, NULL, &dlen);
            char *dvendor = (char*)malloc(dlen+1);
            clGetDeviceInfo(devices[d], CL_DEVICE_VENDOR, dlen, dvendor, NULL);
            dvendor[dlen] = '\0';

            clGetDeviceInfo(devices[d], CL_DEVICE_NAME, 0, NULL, &dlen);
            char *dname = (char*)malloc(dlen+1);
            clGetDeviceInfo(devices[d], CL_DEVICE_NAME, dlen, dname, NULL);
            dname[dlen] = '\0';

            printf("  GPU Device %u: %s (vendor: %s)\n", d, dname, dvendor);

            // Prefer vendor string containing "Intel"
            if (strstr(dvendor, "Intel") != NULL && chosen_device == NULL) {
                chosen_platform = platforms[p];
                chosen_device = devices[d];
            } else if (chosen_device == NULL) {
                // if not intel yet, pick the first GPU as fallback
                chosen_platform = platforms[p];
                chosen_device = devices[d];
            }

            free(dvendor);
            free(dname);
        }

        free(devices);
        free(pname);
    }

    if (!chosen_device) {
        fprintf(stderr, "No GPU device found on any platform.\n");
        free(platforms);
        return 1;
    }

    // Print chosen device details
    {
        size_t dlen = 0;
        clGetDeviceInfo(chosen_device, CL_DEVICE_VENDOR, 0, NULL, &dlen);
        char *vendor = (char*)malloc(dlen+1);
        clGetDeviceInfo(chosen_device, CL_DEVICE_VENDOR, dlen, vendor, NULL);
        vendor[dlen] = '\0';
        clGetDeviceInfo(chosen_device, CL_DEVICE_NAME, 0, NULL, &dlen);
        char *name = (char*)malloc(dlen+1);
        clGetDeviceInfo(chosen_device, CL_DEVICE_NAME, dlen, name, NULL);
        name[dlen] = '\0';
        printf("Chosen device: %s (vendor: %s)\n", name, vendor);
        free(vendor);
        free(name);
    }

    // Create context
    cl_context_properties ctx_props[] = {
        CL_CONTEXT_PLATFORM, (cl_context_properties)chosen_platform,
        0
    };
    cl_context context = clCreateContext(ctx_props, 1, &chosen_device, NULL, NULL, &err);
    CHECK_ERR(err, "clCreateContext failed");

    // Create command queue (use properties if available)
    cl_command_queue queue = NULL;
#if defined(CL_VERSION_2_0)
    cl_command_queue_properties queue_props[] = {0};
    queue = clCreateCommandQueueWithProperties(context, chosen_device, queue_props, &err);
#else
    queue = clCreateCommandQueue(context, chosen_device, 0, &err);
#endif
    CHECK_ERR(err, "clCreateCommandQueue failed");

    // Read kernel source
    size_t klen = 0;
    char *ksrc = read_file_alloc("kernel.cl", &klen);
    if (!ksrc) { fprintf(stderr, "Failed to load kernel.cl\n"); err = -1; goto cleanup; }

    const char *sources[1] = { ksrc };
    cl_program program = clCreateProgramWithSource(context, 1, sources, &klen, &err);
    CHECK_ERR(err, "clCreateProgramWithSource failed");

    // build program (request all warnings)
    err = clBuildProgram(program, 1, &chosen_device, "-cl-std=CL1.2 -Werror", NULL, NULL);
    if (err != CL_SUCCESS) {
        // Get build log
        size_t logsz = 0;
        clGetProgramBuildInfo(program, chosen_device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logsz);
        char *log = (char*)malloc(logsz + 1);
        clGetProgramBuildInfo(program, chosen_device, CL_PROGRAM_BUILD_LOG, logsz, log, NULL);
        log[logsz] = '\0';
        fprintf(stderr, "clBuildProgram failed (err=%d). Build log:\n%s\n", err, log);
        free(log);
        goto cleanup;
    }

    // --- Vector add demo ---
    const unsigned int N = 1 << 20;
    size_t bytes = sizeof(float) * N;
    float *A = (float*)malloc(bytes);
    float *B = (float*)malloc(bytes);
    float *C = (float*)malloc(bytes);
    for (unsigned int i = 0; i < N; ++i) { A[i] = (float)i; B[i] = (float)(N - i); }

    cl_mem bufA = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, A, &err);
    CHECK_ERR(err, "clCreateBuffer A failed");
    cl_mem bufB = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, bytes, B, &err);
    CHECK_ERR(err, "clCreateBuffer B failed");
    cl_mem bufC = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bytes, NULL, &err);
    CHECK_ERR(err, "clCreateBuffer C failed");

    cl_kernel k_vec_add = clCreateKernel(program, "vec_add", &err);
    CHECK_ERR(err, "clCreateKernel vec_add failed");
    err = clSetKernelArg(k_vec_add, 0, sizeof(cl_mem), &bufA);
    err |= clSetKernelArg(k_vec_add, 1, sizeof(cl_mem), &bufB);
    err |= clSetKernelArg(k_vec_add, 2, sizeof(cl_mem), &bufC);
    err |= clSetKernelArg(k_vec_add, 3, sizeof(unsigned int), &N);
    CHECK_ERR(err, "clSetKernelArg failed");

    size_t global = N;
    size_t local = 256;
    if (global % local != 0) global = ((global / local) + 1) * local;

    err = clEnqueueNDRangeKernel(queue, k_vec_add, 1, NULL, &global, &local, 0, NULL, NULL);
    CHECK_ERR(err, "clEnqueueNDRangeKernel failed");

    // read back
    err = clEnqueueReadBuffer(queue, bufC, CL_TRUE, 0, bytes, C, 0, NULL, NULL);
    CHECK_ERR(err, "clEnqueueReadBuffer failed");

    // verify
    int ok = 1;
    for (unsigned int i = 0; i < N; ++i) {
        float want = A[i] + B[i];
        if (C[i] != want) { ok = 0; fprintf(stderr, "Mismatch at %u: %f != %f\n", i, C[i], want); break; }
    }
    if (ok) printf("vec_add passed for N=%u\n", N);

    // --- Image invert demo (small) ---
    // create a small RGBA float image 256x256 filled with a pattern
    cl_image_format fmt;
    fmt.image_channel_order = CL_RGBA;
    fmt.image_channel_data_type = CL_FLOAT;

    const int W = 256, H = 256;
    float *img_in = (float*)malloc(sizeof(float)*4*W*H);
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        int idx = 4 * (y * W + x);
        img_in[idx + 0] = (float)x / (float)W;
        img_in[idx + 1] = (float)y / (float)H;
        img_in[idx + 2] = 0.25f;
        img_in[idx + 3] = 1.0f;
    }

    cl_image_desc desc;
    memset(&desc, 0, sizeof(desc));
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = W;
    desc.image_height = H;

    cl_mem img_src = clCreateImage(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &fmt, &desc, img_in, &err);
    CHECK_ERR(err, "clCreateImage src failed");
    cl_mem img_dst = clCreateImage(context, CL_MEM_WRITE_ONLY, &fmt, &desc, NULL, &err);
    CHECK_ERR(err, "clCreateImage dst failed");

    cl_kernel k_invert = clCreateKernel(program, "invert_image", &err);
    CHECK_ERR(err, "clCreateKernel invert_image failed");
    err = clSetKernelArg(k_invert, 0, sizeof(cl_mem), &img_src);
    err |= clSetKernelArg(k_invert, 1, sizeof(cl_mem), &img_dst);
    CHECK_ERR(err, "clSetKernelArg image failed");

    size_t global2[2] = { (size_t)W, (size_t)H };
    err = clEnqueueNDRangeKernel(queue, k_invert, 2, NULL, global2, NULL, 0, NULL, NULL);
    CHECK_ERR(err, "clEnqueueNDRangeKernel invert failed");

    // read back image region
    size_t origin[3] = {0,0,0};
    size_t region[3] = {W,H,1};
    float *img_out = (float*)malloc(sizeof(float)*4*W*H);
    err = clEnqueueReadImage(queue, img_dst, CL_TRUE, origin, region, 0, 0, img_out, 0, NULL, NULL);
    CHECK_ERR(err, "clEnqueueReadImage failed");

    printf("Image invert ran OK (sample out pixel [0,0]: %.3f %.3f %.3f %.3f)\n",
        img_out[0], img_out[1], img_out[2], img_out[3]);

    // success
    err = CL_SUCCESS;

cleanup:
    if (ksrc) free(ksrc);
    if (program) clReleaseProgram(program);
    if (k_vec_add) clReleaseKernel(k_vec_add);
    if (k_invert) clReleaseKernel(k_invert);
    if (bufA) clReleaseMemObject(bufA);
    if (bufB) clReleaseMemObject(bufB);
    if (bufC) clReleaseMemObject(bufC);
    if (img_src) clReleaseMemObject(img_src);
    if (img_dst) clReleaseMemObject(img_dst);
    if (queue) clReleaseCommandQueue(queue);
    if (context) clReleaseContext(context);
    if (platforms) free(platforms);
    if (A) free(A);
    if (B) free(B);
    if (C) free(C);
    if (img_in) free(img_in);
    if (img_out) free(img_out);

    return (err == CL_SUCCESS) ? 0 : 1;
}
