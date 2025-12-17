// kernel.cl
// Simple example showing vector add and a small image kernel
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

__kernel void vec_add(
    __global const float *a,
    __global const float *b,
    __global float *out,
    const unsigned int n)
{
    size_t gid = get_global_id(0);
    if (gid < n) out[gid] = a[gid] + b[gid];
}

__kernel void invert_image(
    read_only image2d_t src,
    write_only image2d_t dst)
{
    const sampler_t smp = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
    int2 coord = (int2)(get_global_id(0), get_global_id(1));
    float4 pix = read_imagef(src, smp, coord);
    // simple invert
    pix.x = 1.0f - pix.x;
    pix.y = 1.0f - pix.y;
    pix.z = 1.0f - pix.z;
    write_imagef(dst, coord, pix);
}
