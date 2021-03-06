/**********************************************************************
Copyright �2015 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

�   Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
�   Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************/

#ifndef URNG_H_
#define URNG_H_

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS

#define SAMPLE_VERSION "AMD-APP-SDK-v3.0.130.3"

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * Header Files
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "CLUtil.hpp"
#include "SDKBitMap.hpp"
// GLEW and GLUT includes
#include <GL/glew.h>
#include <CL/cl_gl.h>
#include <CL/cl.hpp>
#include "sphere.hpp"
#ifdef _WIN32
#pragma comment(lib,"opengl32.lib")
#pragma comment(lib,"glu32.lib")
#pragma warning( disable : 4996)
#endif

// Define DISPLAY_DEVICE_ACTIVE as it is not defined in MinGW
#ifdef _WIN32
#ifndef DISPLAY_DEVICE_ACTIVE
#define DISPLAY_DEVICE_ACTIVE    0x00000001
#endif
#endif

#ifndef _WIN32
#include <GL/glut.h>
#endif

#define screenWidth  512
#define screenHeight 512
#define AA_LEVEL 1


#define SPHERECOUNT 17
#define SUM -1
#define INTERSEC -2
#define SUB -3


#define GROUP_SIZE 64

cl_int3 pos;       

typedef CL_API_ENTRY cl_int (CL_API_CALL *clGetGLContextInfoKHR_fn)(
    const cl_context_properties *properties,
    cl_gl_context_info param_name,
    size_t param_value_size,
    void *param_value,
    size_t *param_value_size_ret);

/**
 * Rename references to this dynamically linked function to avoid
 * collision with static link version
 */
#define clGetGLContextInfoKHR clGetGLContextInfoKHR_proc
static clGetGLContextInfoKHR_fn clGetGLContextInfoKHR;

using namespace appsdk;

/**
* RayCastCSG
* Class implements OpenCL URNG GL Interop sample
*/

class RayCastCSG
{
    public:

        static RayCastCSG *RayCast;
		sphere* spheres;
		cl_int* treeInONP;
        cl_double setupTime;                /**< time taken to setup OpenCL resources and building kernel */
        cl_double kernelTime;               /**< time taken to run kernel and read result back */
        cl_uchar4* inputImageData;          /**< Input bitmap data to device */
        cl_uchar4* outputImageData;         /**< Output from device */
        std::vector<cl::Platform> platforms;           /**< CL device list */
        cl::Context   context;            /**< CL context */
        std::vector<cl::Device> devices;           /**< CL device list */
        std::vector<cl::Device> device;     /**< CL device to be used */
        cl::Buffer inputImageBuffer;            /**< CL memory buffer for input Image*/
        cl::Buffer outputImageBuffer;           /**< CL memory buffer for Output Image*/
        cl_uchar4*
        verificationOutput;       /**< Output array for reference implementation */
        cl::CommandQueue commandQueue;   /**< CL command queue */
        cl::Program   program;            /**< CL program  */
        cl::Kernel kernel;                   /**< CL kernel */
        SDKBitMap inputBitmap;           /**< Bitmap class object */
        uchar4* pixelData;               /**< Pointer to image data */
        cl_uint pixelSize;                  /**< Size of a pixel in BMP format> */
        cl_uint width;                      /**< Width of image */
        cl_uint height;                     /**< Height of image */
		cl_uint windowWidth;
		cl_uint windowHeight;
        cl_bool byteRWSupport;
        size_t blockSizeX;                  /**< Work-group size in x-direction */
        size_t blockSizeY;                  /**< Work-group size in y-direction */
        int iterations;                     /**< Number of iterations for kernel execution */
        clock_t t1, t2;
        int frameCount;
        int frameRefCount;
        double totalElapsedTime;
        GLuint pbo;                         //pixel-buffer object to hold-image data
        GLuint tex;                         //Texture to display
        cl::Device interopDeviceId;
        //KernelWorkGroupInfo kernelInfo;
        //SDKDeviceInfo deviceInfo;

        size_t       maxWorkGroupSize;      /**< Device Specific Information */
        cl_uint         maxDimensions;
        size_t *     maxWorkItemSizes;
        cl_ulong     totalLocalMemory;

        SDKTimer    *sampleTimer;      /**< SDKTimer object */

    public:

        CLCommandArgs   *sampleArgs;   /**< CLCommand argument class */

        /**
        * Read bitmap image and allocate host memory
        * @param inputImageName name of the input file
        * @return SDK_SUCCESS on success and SDK_FAILURE on failure
        */
        int createOutputImage();

        /**
        * Constructor
        * Initialize member variables
        */
        RayCastCSG()
            : inputImageData(NULL),
              outputImageData(NULL),
              verificationOutput(NULL),
              byteRWSupport(true)
        {
            sampleArgs = new CLCommandArgs();
            sampleTimer = new SDKTimer();
            sampleArgs->sampleVerStr = SAMPLE_VERSION;
            pixelSize = sizeof(uchar4);
            pixelData = NULL;
            blockSizeX = 8;
            blockSizeY = 8;
            iterations = 1;
            pos = pos;
            frameCount = 0;
            frameRefCount = 120;

			spheres = new sphere[SPHERECOUNT]{
				sphere(200, 200, 800, 50,		0.9,0.9,0.9),
				sphere(240, 240, 760, 26,		0.9,0.9,0.9),
				sphere(50, 0, 800, 65,			1, 0, 0),
				sphere(150, 0, 800, 65,			0, 1, 0),
				sphere(110, 30, 750, 60,		1, 1, 0),
				sphere(-50, -180, 800, 65,		1, 0, 0),
				sphere(50, -180, 800, 65,		0, 1, 0),
				sphere(230, -150, 750, 60,		1, 1, 0),
				sphere(-350, -170, 800, 40,		0, 0, 1),
				sphere(-250, -210, 810, 45,		0, 1, 1),
				sphere(-280, 0, 800, 40,		0, 0, 1),
				sphere(-280, -40, 800, 45,		0, 1, 1),
				sphere(-100, 190, 800, 65,		1, 0, 0),
				sphere(0, 190, 800, 65,		0, 1, 0),
				sphere(-40, 220, 750, 60,		1, 1, 0),
				sphere(-80, 200, 750, 40,		0, 0, 1),
				sphere(-80, 150, 750, 45,		0, 1, 1)
			};

			treeInONP = new int[SPHERECOUNT * 2 - 1]{
				0, 1, SUB,
				2, 3, SUM, 4, SUB, SUM, 
				5, 6, SUM, 
				7, SUM, SUM,
				8, 9, SUM, SUM,
				10, 11, INTERSEC, SUM,
				12, 13, SUM, 14, SUB, 15, 16, INTERSEC, SUB, SUM
			};
			
        }

        ~RayCastCSG()
        {
        }


        /**
         * Initialize the GL.
         * @return SDK_SUCCESS on success and SDK_FAILURE on failure
         */
        int initializeGL(int argc, char * argv[]);

        void keyboardFunc(unsigned char, int, int);


#ifdef _WIN32
        int enableGLAndGetGLContext(HWND hWnd,
                                    HDC &hDC,
                                    HGLRC &hRC,
                                    cl_platform_id platform,
                                    cl_context &context,
                                    cl_device_id &interopDevice);

        void disableGL(HWND hWnd, HDC hDC, HGLRC hRC);
#else
        void enableGLForLinux();
        void disableGLForLinux();
#endif
        /**
        * Initializing GL and get interoperable CL context
        * @param argc number of arguments
        * @param argv command line arguments
        * @
        * @return SDK_SUCCESS on success and SDK_FALIURE on failure.
        */
        int InitializeGLAndGetCLContext(cl_platform_id platform,
                                        cl_context &context,
                                        cl_device_id &interopDevice);
        /**
        * OpenCL related initializations.
        * Set up Context, Device list, Command Queue, Memory buffers
        * Build CL kernel program executable
        * @return SDK_SUCCESS on success and SDK_FAILURE on failure
        */
        int setupCL();

        /**
        * Set values for kernels' arguments, enqueue calls to the kernels
        * on to the command queue, wait till end of kernel execution.
        * Get kernel start and end time if timing is enabled
        * @return SDK_SUCCESS on success and SDK_FAILURE on failure
        */
        int runCLKernels();

        /**
        * Reference CPU implementation of Binomial Option
        * for performance comparison
        */
        void URNGCPUReference();

        /**
        * Override from SDKSample. Print sample stats.
        */
        void printStats();

        /**
        * Override from SDKSample. Initialize
        * command line parser, add custom options
        * @return SDK_SUCCESS on success and SDK_FAILURE on failure
        */
        int initialize();

        /**
         * Override from SDKSample, Generate binary image of given kernel
         * and exit application
         * @return SDK_SUCCESS on success and SDK_FAILURE on failure
         */
        int genBinaryImage();

        /**
        * Override from SDKSample, adjust width and height
        * of execution domain, perform all sample setup
        * @return SDK_SUCCESS on success and SDK_FAILURE on failure
        */
        int setup();

        /**
        * Override from SDKSample
        * @return SDK_SUCCESS on success and SDK_FAILURE on failure
        */
        int run();

        /**
        * Override from SDKSample
        * Cleanup memory allocations
        * @return SDK_SUCCESS on success and SDK_FAILURE on failure
        */
        int cleanup();
};

#endif // URNG_H_
