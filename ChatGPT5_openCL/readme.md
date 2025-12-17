If everything is configured you should see platform/device listings, vec_add passed for N=..., and an image invert sample printout.

Windows notes

Use Visual Studio or MSYS2. Link against OpenCL.lib and ensure OpenCL.dll is available (comes with vendor ICD / driver). You’ll need Intel’s Windows OpenCL runtime (part of oneAPI/driver). The host code is portable; change the Makefile build step to use MSVC or MinGW equivalents and link OpenCL.lib (MSVC) or -lOpenCL (MinGW).

Common issues & troubleshooting

No platforms or no GPU devices found — most likely the Intel OpenCL runtime (driver) is not installed or the ICD is not registered. Install Intel oneAPI/compute-runtime and ensure the ICD JSON is present under /etc/OpenCL/vendors/ (Linux) or that OpenCL.dll is available on Windows. 
Intel
GitHub

Build errors — the kernel build log is printed if clBuildProgram fails; read it to spot unsupported language features or extension issues.

Prefer Level Zero / oneAPI runtime — on systems where Level Zero is available, runtime selection could prefer Level Zero; you can control selection with ONEAPI_DEVICE_SELECTOR or by installing different runtimes. 
Intel

Why this targets Intel GPUs

The host code explicitly attempts to pick devices with vendor string containing "Intel". Modern Intel GPU support is provided by the Intel Graphics Compute Runtime / oneAPI packages — make sure those are installed for the best compatibility. 
GitHub
Intel

Extra tips & next steps (if you want them)

If you want to use advanced Intel-specific OpenCL extensions or debug performance, I can extend the host to query CL_DEVICE_EXTENSIONS, show subgroup capabilities, or enable profiling timers (use clCreateCommandQueue with CL_QUEUE_PROFILING_ENABLE).

For maximum performance on newest Intel hardware you may also evaluate migrating to SYCL / oneAPI DPC++ in the future — Intel provides guides for migrating OpenCL designs. 
Intel
+1

Caveat & final note
I provided a complete, runnable example, plus installation guidance and troubleshooting pointers. However, whether it “works the first time” depends on your system’s driver/runtime installation — the most common failure is a missing OpenCL driver or ICD. If you paste the exact runtime output/error you get when running ./host I’ll diagnose precisely and provide the smallest change required.

Want me to:

produce a Windows-ready Visual Studio project file next?

add device profiling and timing with event-based counters?

or rewrite the host in C++ with RAII wrappers and nicer error handling?
