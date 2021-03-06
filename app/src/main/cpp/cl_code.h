//
// Created by dhruva on 12-05-2021.
//

#ifndef ANDROID_SHADER_DEMO_JNI_CL_CODE_H
#define ANDROID_SHADER_DEMO_JNI_CL_CODE_H
#include <fstream>
#include "cl_wrapper.h"
#include "CL/cl.hpp"
#define LOG_TAG    "cl_code.hpp"

#define DPRINTF(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)

#define IPRINTF(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

#define EPRINTF(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)


#if 1
int testOpencl()
{
   //DPRINTF("testOpencl start ");
// OpenCL kernel. Each work item takes care of one element of c
//"#pragma OPENCL EXTENSION cl_khr_fp64 : enable \n"
const char *kernelSource = "__kernel void vecAdd(  __global float *a,\n" \
"                       __global float *b,                       \n" \
"                       __global float *c,                       \n" \
"                       const unsigned int n)                    \n" \
"{                                                               \n" \
"    int id = get_global_id(0);                                  \n" \
"    if (id < n)                                                 \n" \
"        c[id] = a[id] * b[id];                                  \n" \
"}                                                               \n" \
                                                                "\n";

   const char *kernelSource_ = "__kernel                                   \n" \
            "void saxpy_kernel(float alpha,     \n" \
            "                  __global float *A,       \n" \
            "                  __global float *B,       \n" \
            "                  __global float *C)       \n" \
            "{                                          \n" \
            "    //Get the index of the work-item       \n" \
            "    int index = get_global_id(0);          \n" \
            "    C[index] = alpha* A[index] + B[index]; \n" \
            "}";

   // Length of vectors
//            unsigned int n = 1000;
   unsigned int n = 10;
   // Host input vectors
   float *h_a;
   float *h_b;
   // Host output vector
   float *h_c;

   // Device input buffers
   cl::Buffer d_a;
   cl::Buffer d_b;
   // Device output buffer
   cl::Buffer d_c;

   // Size, in bytes, of each vector
   size_t bytes = n * sizeof(float);

   // Allocate memory for each vector on host
   h_a = new float[n];
   h_b = new float[n];
   h_c = new float[n];

   // Initialize vectors on host
   for (int i = 0; i < n; i++) {
      h_a[i] = i;
      h_b[i] = i;
      DPRINTF("h_a[i]:[%lf] h_b[i]:[%lf]",h_a[i],h_b[i]);
   }

   cl_int err = CL_SUCCESS;
   //try {

      // Query platforms

      std::vector <cl::Platform> platforms;
      cl::Platform::get(&platforms);
      DPRINTF("after platform get");
      DPRINTF("platforms.size():[%ld]",platforms.size());
      if (platforms.size() == 0) {
         DPRINTF("Platform size 0\n");
         return -1;
      }

      // Get list of devices on default platform and create context
      cl_context_properties properties[] =
              {CL_CONTEXT_PLATFORM, (cl_context_properties)(platforms[0])(), 0};
      cl::Context context(CL_DEVICE_TYPE_GPU, properties);
      std::vector <cl::Device> devices = context.getInfo<CL_CONTEXT_DEVICES>();
      DPRINTF("devices.size():[%ld]",devices.size());
      DPRINTF("device name:[%s]",devices[0].getInfo<CL_DEVICE_NAME>().c_str());
      DPRINTF("device type:[%ld]",devices[0].getInfo<CL_DEVICE_TYPE>());
      DPRINTF("device vendor:[%s]",devices[0].getInfo<CL_DEVICE_VENDOR>().c_str());
      DPRINTF("device version:[%s]",devices[0].getInfo<CL_DEVICE_VERSION>().c_str());
      DPRINTF("device opencl versionr:[%s]",devices[0].getInfo<CL_DEVICE_OPENCL_C_VERSION>().c_str());
      DPRINTF("device max clock freq:[%d]",devices[0].getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>());
      DPRINTF("device compiler available:[%d]",devices[0].getInfo<CL_DEVICE_COMPILER_AVAILABLE>());
      DPRINTF("device Global mem size:[%ld]",devices[0].getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>());
      DPRINTF("device local mem size:[%ld]",devices[0].getInfo<CL_DEVICE_LOCAL_MEM_SIZE>());
      DPRINTF("device max work group size:[%ld]",devices[0].getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>());
      DPRINTF("device max work group size:[%ld]",devices[0].getInfo<CL_DEVICE_EXECUTION_CAPABILITIES>());


      // Create command queue for first device
      cl::CommandQueue queue(context, devices[0], 0, &err);

      // Create device memory buffers
      d_a = cl::Buffer(context, CL_MEM_READ_ONLY, bytes);
      d_b = cl::Buffer(context, CL_MEM_READ_ONLY, bytes);
      d_c = cl::Buffer(context, CL_MEM_WRITE_ONLY, bytes);

      // Bind memory buffers
      queue.enqueueWriteBuffer(d_a, CL_TRUE, 0, bytes, h_a);
      queue.enqueueWriteBuffer(d_b, CL_TRUE, 0, bytes, h_b);

      //Build kernel from source string
      cl::Program::Sources source(1,
                                  std::make_pair(kernelSource, strlen(kernelSource)));
      cl::Program program_ = cl::Program(context, source);
      program_.build(devices);

      // Create kernel object
      cl::Kernel kernel(program_, "vecAdd", &err);

      // Bind kernel arguments to kernel
      kernel.setArg(0, d_a);
      kernel.setArg(1, d_b);
      kernel.setArg(2, d_c);
      kernel.setArg(3, n);

      // Number of work items in each local work group
      cl::NDRange localSize(64);
      // Number of total work items - localSize must be devisor
      cl::NDRange globalSize((int) (ceil(n / (float) 64) * 64));

      // Enqueue kernel
      cl::Event event;
      queue.enqueueNDRangeKernel(
              kernel,
              cl::NullRange,
              globalSize,
              localSize,
              NULL,
              &event);

      // Block until kernel completion
      event.wait();


      queue.finish();
      // Read back d_c
      queue.enqueueReadBuffer(d_c, CL_TRUE, 0, bytes, h_c);

   //}
   //catch ( ) {
      //DPRINTF("catch error:[%s]",err.what());
      //std::cerr<< "ERROR: " << err.what() << "(" << err.err() << ")" << std::endl;
   //}

   // Sum up vector c and print result divided by n, this should equal 1 within error

   DPRINTF("for loop before");
   for (int i = 0; i < n; i++)
   {
      DPRINTF("final result: %lf", h_c[i]);
   }
   DPRINTF("for loop after");
   // Release host memory
   delete (h_a);
   delete (h_b);
   delete (h_c);
   DPRINTF("testOpencl end ");
   return 0;
}

void test_cl_image()
{
   static const char* PROGRAM_IMAGE_2D_COPY_SOURCE[] = {
           "__constant sampler_t imageSampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;\n",
           /* Copy input 2D image to output 2D image */
           "__kernel void image2dCopy(__read_only image2d_t input,\n",
           " __write_only image2d_t output )\n",
           "{\n",
           "    int2 coord = (int2)(get_global_id(0), get_global_id(1));\n",
           "    uint4 temp = read_imageui(input, imageSampler, coord);\n",
           "    write_imageui(output, coord, temp);\n",
           "}"
   };

   cl_wrapper       wrapper;
   static const cl_uint PROGRAM_IMAGE_2D_COPY_SOURCE_LEN = sizeof(PROGRAM_IMAGE_2D_COPY_SOURCE) / sizeof(const char*);
   cl_context       context = wrapper.get_context();
   cl_command_queue command_queue = wrapper.get_command_queue();
   cl_program       program_image_2d_program = wrapper.make_program(PROGRAM_IMAGE_2D_COPY_SOURCE, PROGRAM_IMAGE_2D_COPY_SOURCE_LEN);
   cl_kernel        program_image_2d_kernel = wrapper.make_kernel("image2dCopy", program_image_2d_program);

   std::string src_filename = std::string("/storage/emulated/0/opencvTesting/out148Yonly.yuv");
   std::ifstream fin(src_filename, std::ios::binary);
   if (!fin) {
      DPRINTF("Couldn't open file %s", src_filename.c_str());
      std::exit(EXIT_FAILURE);
   }
   const auto        fin_begin = fin.tellg();
   fin.seekg(0, std::ios::end);
   const auto        fin_end = fin.tellg();
   const size_t      buf_size = static_cast<size_t>(fin_end - fin_begin);
   std::vector<char> buf(buf_size);
   fin.seekg(0, std::ios::beg);
   fin.read(buf.data(), buf_size);

   cl_int status = CL_SUCCESS;
   // Create and initialize image objects
   cl_image_desc imageDesc;
   memset(&imageDesc, '\0', sizeof(cl_image_desc));
   imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
   imageDesc.image_width = 1440;
   imageDesc.image_height = 1080;

   cl_image_format imageFormat;
   imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
   imageFormat.image_channel_order = CL_R;
   // Create 2D input image
   cl_mem inputImage2D;
   cl_mem outputImage2D;

   inputImage2D = clCreateImage(context,
                                CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
                                &imageFormat,
                                &imageDesc,
                                buf.data(),
                                &status);

   // Create 2D output image
   outputImage2D = clCreateImage(context,
                                 CL_MEM_WRITE_ONLY,
                                 &imageFormat,
                                 &imageDesc,
                                 0,
                                 &status);


   status = clSetKernelArg(
           program_image_2d_kernel,
           0,
           sizeof(cl_mem),
           &inputImage2D);

   status = clSetKernelArg(
           program_image_2d_kernel,
           1,
           sizeof(cl_mem),
           &outputImage2D);

   size_t globalThreads[] = { 1440, 1080 };

   struct timeval startNDKKernel, endNDKKernel;
   gettimeofday(&startNDKKernel, NULL);
   status = clEnqueueNDRangeKernel(
           command_queue,
           program_image_2d_kernel,
           2,
           NULL,
           globalThreads,
           0,
           0,
           NULL,
           0);
   clFinish(command_queue);
   gettimeofday(&endNDKKernel, NULL);
   DPRINTF("time taken :%ld",((endNDKKernel.tv_sec * 1000000 + endNDKKernel.tv_usec)- (startNDKKernel.tv_sec * 1000000 + startNDKKernel.tv_usec)));
   // Enqueue Read Image
   size_t origin[] = { 0, 0, 0 };
   size_t region[] = { 1440, 1080, 1 };

   unsigned char *outputImageData2D = (unsigned char*)malloc(1440* 1080);
   // Read output of 2D copy
   status = clEnqueueReadImage(command_queue,
                               outputImage2D,
                               1,
                               origin,
                               region,
                               0,
                               0,
                               outputImageData2D,
                               0, 0, 0);

   std::string filename("/storage/emulated/0/opencvTesting/output_copy2.yuv");
   std::ofstream fout(filename, std::ios::binary);
   if (!fout)
   {
      std::cerr << "Couldn't open file " << filename << "\n";
      std::exit(EXIT_FAILURE);
   }

   char* output_image_U8 = new char[buf_size];

   for (int pix_i = 0; pix_i < buf_size; pix_i++)
      output_image_U8[pix_i] = (unsigned char)outputImageData2D[pix_i];

   fout.write(output_image_U8, buf_size);
   delete[] output_image_U8;
   free(outputImageData2D);
   fout.close();
   clReleaseMemObject(outputImage2D);
   clReleaseMemObject(inputImage2D);
}
#endif
#endif //ANDROID_SHADER_DEMO_JNI_CL_CODE_H
