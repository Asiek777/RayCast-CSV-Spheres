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


#include "RayCastCSG.hpp"
#include <cmath>

#ifndef _WIN32
#include <GL/glx.h>
#endif //!_WIN32

#ifdef _WIN32
static HWND               gHwnd;
HDC                       gHdc;
HGLRC                     gGlCtx;
BOOL quit = FALSE;
MSG msg;
#else
GLXContext gGlCtx;
#define GLX_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB           0x2092
typedef GLXContext (*GLXCREATECONTEXTATTRIBSARBPROC)(Display*, GLXFBConfig,
        GLXContext, Bool, const int*);
Window          win;
Display         *displayName;
XEvent          xev;
#endif

#ifdef _WIN32
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {

    case WM_CREATE:
        return 0;

    case WM_CLOSE:
        PostQuitMessage( 0 );
        return 0;

    case WM_DESTROY:
        return 0;

    case WM_KEYDOWN:
        switch ( wParam )
        {

        case VK_ESCAPE:
            PostQuitMessage(0);
            return 0;
        case 0x57: //'W'
            pos.y++;;
            break;
        case 0x53://'S'
            pos.y--;
            break;
		case 0x41:
			pos.x--;
			break;
		case 0x44:
			pos.x++;
			break;
		case 0x45:
			pos.z++;
			break;
		case 0x51:
			pos.z--;
			break;
		}
        return 0;

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );

    }
}
#endif

int
RayCastCSG::createOutputImage()
{
    // get width and height of input image
	windowHeight = 1000;
	windowWidth = 1800;
	height = windowHeight * AA_LEVEL;
	width = windowWidth * AA_LEVEL;

    // allocate memory for output image data
    outputImageData = (cl_uchar4*)malloc(width * height * sizeof(cl_uchar4));
    CHECK_ALLOCATION(outputImageData,
                     "Failed to allocate memory! (outputImageData)");

    // initialize the Image data to NULL
    memset(outputImageData, 0, width * height * pixelSize);

    return SDK_SUCCESS;

}

int
RayCastCSG::genBinaryImage()
{
    bifData binaryData;
    binaryData.kernelName = std::string("RayCastCSG_Kernels.cl");
    binaryData.flagsStr = std::string("");
    if(sampleArgs->isComplierFlagsSpecified())
    {
        binaryData.flagsFileName = std::string(sampleArgs->flags.c_str());
    }

    binaryData.binaryName = std::string(sampleArgs->dumpBinary.c_str());
    int status = generateBinaryImage(binaryData);
    return status;
}


int
RayCastCSG::setupCL()
{
    cl_int status = CL_SUCCESS;
    cl::Event writeEvt;
    cl_int eventStatus = CL_QUEUED;
    cl_device_type dType;

    if(sampleArgs->deviceType.compare("cpu") == 0)
    {
        dType = CL_DEVICE_TYPE_CPU;
    }
    else //deviceType = "gpu"
    {
        dType = CL_DEVICE_TYPE_GPU;
        if(sampleArgs->isThereGPU() == false)
        {
            std::cout << "GPU not found. Falling back to CPU device" << std::endl;
            dType = CL_DEVICE_TYPE_CPU;
        }
    }

    /*
     * Have a look at the available platforms and pick either
     * the AMD one if available or a reasonable default.
     */
    status = cl::Platform::get(&platforms);
    CHECK_OPENCL_ERROR(status, "Platform::get() failed.");

    std::vector<cl::Platform>::iterator i;
    if(platforms.size() > 0)
    {
        if(sampleArgs->isPlatformEnabled())
        {
            i = platforms.begin() + sampleArgs->platformId;
        }
        else
        {
            for(i = platforms.begin(); i != platforms.end(); ++i)
            {
                if(true)
                {
                    break;
                }
            }
        }
    }

    // Display available devices.
    // std::cout << "Platform :" << (*i).getInfo<CL_PLATFORM_VENDOR>().c_str() << "\n";

#ifdef _WIN32
    int success = enableGLAndGetGLContext(gHwnd, gHdc, gGlCtx, (*i)(), context(),
                                          interopDeviceId());
    if (success != SDK_SUCCESS)
    {
        return success;
    }
#else
    int retValue = InitializeGLAndGetCLContext((*i)(),
                   context(),
                   interopDeviceId());
    if (retValue != SDK_SUCCESS)
    {
        return retValue;
    }
#endif

    // getting device on which to run the sample
    devices = context.getInfo<CL_CONTEXT_DEVICES>();
    CHECK_OPENCL_ERROR(status, "Context::getInfo() failed.");

    int deviceCount = (int)devices.size();

    int j = 0;
    for (std::vector<cl::Device>::iterator i = devices.begin(); i != devices.end();
            ++i, ++j)
    {
        std::cout << "Device " << j << " : ";
        std::string deviceName = (*i).getInfo<CL_DEVICE_NAME>();
        std::cout << deviceName.c_str() << "\n";
    }

    std::cout << "\n";

    if (deviceCount == 0)
    {
        std::cout << "No device available\n";
        return SDK_FAILURE;
    }

    if(validateDeviceId(sampleArgs->deviceId, deviceCount) != SDK_SUCCESS)
    {
        std::cout << "validateDeviceId() failed";
        return SDK_FAILURE;
    }

    // Get Device specific Information
    status = devices[sampleArgs->deviceId].getInfo<size_t>(
                 CL_DEVICE_MAX_WORK_GROUP_SIZE,
                 &maxWorkGroupSize);
    CHECK_OPENCL_ERROR(status,
                       "Device::getInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE) failed.");

    status = devices[sampleArgs->deviceId].getInfo<cl_uint>(
                 CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,
                 &maxDimensions);
    CHECK_OPENCL_ERROR(status,
                       "Device::getInfo(CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS) failed.");

    maxWorkItemSizes = (size_t*)malloc(maxDimensions * sizeof(size_t));

    std::vector<size_t> workItems =
        devices[sampleArgs->deviceId].getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>();

    for(cl_uint i = 0; i < maxDimensions; ++i)
    {
        maxWorkItemSizes[i] = workItems[i];
    }

    status = devices[sampleArgs->deviceId].getInfo<cl_ulong>(
                 CL_DEVICE_LOCAL_MEM_SIZE,
                 &totalLocalMemory);
    CHECK_OPENCL_ERROR(status,
                       "Device::getInfo(CL_DEVICE_LOCAL_MEM_SIZES) failed.");

    if (dType == CL_DEVICE_TYPE_CPU)
    {
        interopDeviceId = devices[sampleArgs->deviceId];
    }

    // Create command queue

    commandQueue = cl::CommandQueue(context, interopDeviceId, 0, &status);
    CHECK_OPENCL_ERROR(status, "CommandQueue::CommandQueue() failed.");

    // Create and initialize memory objects

    // Set Persistent memory only for AMD platform
    cl_mem_flags inMemFlags = CL_MEM_READ_ONLY;
    // if(sampleArgs->isAmdPlatform())
    // {
    //     inMemFlags |= CL_MEM_USE_PERSISTENT_MEM_AMD;
    // }


    // Create texture object
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // Set parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create pixel-buffer object
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_ARRAY_BUFFER, pbo);

    // initialize buffer object
    unsigned int size = width * height * sizeof(cl_uchar4);

    // buffer data
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Create OpenCL buffer from GL PBO
    outputImageBuffer() = clCreateFromGLBuffer(context(),
                          CL_MEM_WRITE_ONLY,
                          pbo,
                          &status);
    CHECK_OPENCL_ERROR(status, "clCreateFromGLBuffer failed. (outputImageBuffer)");

    device.push_back(devices[sampleArgs->deviceId]);

    // create a CL program using the kernel source
    SDKFile kernelFile;
    std::string kernelPath = getPath();

    // create a CL program using the kernel source

    if(sampleArgs->isLoadBinaryEnabled())
    {
        kernelPath.append(sampleArgs->loadBinary.c_str());
        if(kernelFile.readBinaryFromFile(kernelPath.c_str()))
        {
            std::cout << "Failed to load kernel file : " << kernelPath << std::endl;
            return SDK_FAILURE;
        }
        cl::Program::Binaries programBinary(1,std::make_pair(
                                                (const void*)kernelFile.source().data(),
                                                kernelFile.source().size()));

        program = cl::Program(context, device, programBinary, NULL, &status);
        CHECK_OPENCL_ERROR(status, "Program::Program(Binary) failed.");

    }
    else
    {
        kernelPath.append("RayCastCSG_Kernels.cl");
        if(!kernelFile.open(kernelPath.c_str()))
        {
            std::cout << "Failed to load kernel file : " << kernelPath << std::endl;
            return SDK_FAILURE;
        }
        cl::Program::Sources programSource(
            1,
            std::make_pair(kernelFile.source().data(),
                           kernelFile.source().size()));

        program = cl::Program(context, programSource, &status);
        CHECK_OPENCL_ERROR(status, "Program::Program(Source) failed.");

    }
    std::string flagsStr = std::string("");

    if(sampleArgs->isComplierFlagsSpecified())
    {
        SDKFile flagsFile;
        std::string flagsPath = getPath();
        flagsPath.append(sampleArgs->flags.c_str());
        if(!flagsFile.open(flagsPath.c_str()))
        {
            std::cout << "Failed to load flags file: " << flagsPath << std::endl;
            return SDK_FAILURE;
        }
        flagsFile.replaceNewlineWithSpaces();
        const char * flags = flagsFile.source().c_str();
        flagsStr.append(flags);
    }

    if(flagsStr.size() != 0)
    {
        std::cout << "Build Options are : " << flagsStr.c_str() << std::endl;
    }

    status = program.build(device, flagsStr.c_str());

    if(status != CL_SUCCESS)
    {
        if(status == CL_BUILD_PROGRAM_FAILURE)
        {
            std::string str = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[sampleArgs->deviceId]);

            std::cout << " \n\t\t\tBUILD LOG\n";
            std::cout << " ************************************************\n";
            std::cout << str << std::endl;
            std::cout << " ************************************************\n";
        }
    }
    CHECK_OPENCL_ERROR(status, "Program::build() failed.");

    // get a kernel object handle for a kernel with the given name
    kernel = cl::Kernel(program, "noise_uniform", &status);
    CHECK_OPENCL_ERROR(status, "Kernel::Kernel failed.");

    status = commandQueue.flush();
    CHECK_OPENCL_ERROR(status, "cl::CommandQueue.flush failed.");

    return SDK_SUCCESS;
}

int RayCastCSG::InitializeGLAndGetCLContext(cl_platform_id platform,
        cl_context &context,
        cl_device_id &interopDevice)
{
#ifndef _WIN32
    cl_int status = SDK_SUCCESS;
    int numDevices ;
    displayName = XOpenDisplay(NULL);
    int screenNumber = ScreenCount(displayName);
    std::cout<<"Number of displays "<<screenNumber<<std::endl;
    XCloseDisplay(displayName);
    for (int i = 0; i < screenNumber; i++)
    {
        if (sampleArgs->isDeviceIdEnabled())
        {
            if (i < sampleArgs->deviceId)
            {
                continue;
            }
        }
        char disp[100];
        sprintf(disp, "DISPLAY=:0.%d", i);
        putenv(disp);
        displayName = XOpenDisplay(0);
        int nelements;
        GLXFBConfig *fbc = glXChooseFBConfig(displayName,
                                             DefaultScreen(displayName),
                                             0,
                                             &nelements);

	if(fbc == NULL)
	  {
	    std::cout << "ERROR:" ;
	    std::cout << "Unable to find any frame buffer configuration..";
	    std::cout << std::endl;
	    std::cout << "glxChooseFBConfig returned NULL pointer." << std::endl;
	    return SDK_FAILURE;
	  }

        static int attributeList[] = { GLX_RGBA,
                                       GLX_DOUBLEBUFFER,
                                       GLX_RED_SIZE,
                                       1,
                                       GLX_GREEN_SIZE,
                                       1,
                                       GLX_BLUE_SIZE,
                                       1,
                                       None
                                     };
        XVisualInfo *vi = glXChooseVisual(displayName,
                                          DefaultScreen(displayName),
                                          attributeList);
        XSetWindowAttributes swa;
        swa.colormap = XCreateColormap(displayName,
                                       RootWindow(displayName, vi->screen),
                                       vi->visual,
                                       AllocNone);
        swa.border_pixel = 0;
        swa.event_mask = StructureNotifyMask;

        win = XCreateWindow(displayName,
                            RootWindow(displayName, vi->screen),
                            10,
                            10,
                            width,
                            height,
                            0,
                            vi->depth,
                            InputOutput,
                            vi->visual,
                            CWBorderPixel|CWColormap|CWEventMask,
                            &swa);

        XMapWindow (displayName, win);

        std::cout << "glXCreateContextAttribsARB "
                  << (void*) glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB")
                  << std::endl;
        GLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB =
            (GLXCREATECONTEXTATTRIBSARBPROC)
            glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");

        int attribs[] =
        {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 0,
            0
        };

        GLXContext ctx = glXCreateContextAttribsARB(displayName,
                         *fbc,
                         0,
                         true,
                         attribs);

	if(!ctx)
	  {
	    std::cout << "ERROR:GL context creation failed." << std::endl;
	    return SDK_FAILURE;
	  }

        glXMakeCurrent (displayName,
                        win,
                        ctx);
        gGlCtx = glXGetCurrentContext();
        cl_context_properties cpsGL[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
                                          CL_GLX_DISPLAY_KHR, (intptr_t) glXGetCurrentDisplay(),
                                          CL_GL_CONTEXT_KHR, (intptr_t) gGlCtx, 0
                                        };
        if (!clGetGLContextInfoKHR)
        {
#ifdef CL_VERSION_1_2
            clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)
                                    clGetExtensionFunctionAddressForPlatform(platform,"clGetGLContextInfoKHR");
#else
            clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)
                                    clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
#endif
            if (!clGetGLContextInfoKHR)
            {
                std::cout << "Failed to query proc address for clGetGLContextInfoKHR";
            }
        }

        size_t deviceSize = 0;
        status = clGetGLContextInfoKHR(cpsGL,
                                       CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR,
                                       0,
                                       NULL,
                                       &deviceSize);
        CHECK_OPENCL_ERROR(status, "clGetGLContextInfoKHR failed!!");

        numDevices = (deviceSize / sizeof(cl_device_id));
        std::cout<<"Number of interoperable devices "<<numDevices<<std::endl;
        if(numDevices == 0)
        {
            glXDestroyContext(glXGetCurrentDisplay(), gGlCtx);
	    gGlCtx=NULL;
            continue;
        }
        else
        {
            //Interoperable device found
            std::cout<<"Interoperable device found "<<std::endl;
            break;
        }
    }

    if (numDevices == 0)
    {
        std::cout << "Interoperable device not found."
                  << std::endl;
        return SDK_EXPECTED_FAILURE;
    }

    cl_context_properties cpsGL[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
                                      CL_GLX_DISPLAY_KHR, (intptr_t) glXGetCurrentDisplay(),
                                      CL_GL_CONTEXT_KHR, (intptr_t) gGlCtx, 0
                                    };
    if (sampleArgs->deviceType.compare("gpu") == 0)
    {
        status = clGetGLContextInfoKHR( cpsGL,
                                        CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR,
                                        sizeof(cl_device_id),
                                        &interopDeviceId,
                                        NULL);
        CHECK_OPENCL_ERROR(status, "clGetGLContextInfoKHR failed!!");

        //std::cout<<"Interop Device ID is "<<interopDeviceId<<std::endl;

        // Create OpenCL context from device's id
        context = clCreateContext(cpsGL,
                                  1,
                                  &interopDevice,
                                  0,
                                  0,
                                  &status);
        CHECK_OPENCL_ERROR(status, "clCreateContext failed.");
    }
    else
    {
        context = clCreateContextFromType(cpsGL,
                                          CL_DEVICE_TYPE_CPU,
                                          NULL,
                                          NULL,
                                          &status);
        CHECK_OPENCL_ERROR(status, "clCreateContextFromType failed!!");
    }
    // OpenGL animation code goes here
    // GL init
    glewInit();
    if (! glewIsSupported("GL_VERSION_2_0 " "GL_ARB_pixel_buffer_object"))
    {
        std::cout << "Support for necessary OpenGL extensions missing."
                  << std::endl;
        return SDK_FAILURE;
    }

    //glEnable(GL_TEXTURE_2D);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glDisable(GL_DEPTH_TEST);

    glViewport(0, 0, windowWidth, windowHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(
        60.0,
        (GLfloat)width / (GLfloat)height,
        0.1,
        10.0);

#endif
    return SDK_SUCCESS;
}
int
RayCastCSG::runCLKernels()
{
    cl_int status;
    cl_int eventStatus = CL_QUEUED;

    // Acquire outputImageBuffer from GL
    cl_event acquireEvt;
    status = clEnqueueAcquireGLObjects(
                 (commandQueue)(),
                 1,
                 &((outputImageBuffer)()),
                 0,
                 NULL,
                 &acquireEvt);
    CHECK_OPENCL_ERROR(status, "clEnqueueAcquireGLObjects failed.");

	cl::Buffer sphereBuffer(spheres, spheres + SPHERECOUNT, true);
	cl::Buffer ONPtreeBuffer(treeInONP, treeInONP + (SPHERECOUNT * 2 - 1), true);

    status = commandQueue.flush();
    CHECK_OPENCL_ERROR(status, "cl::CommandQueue.flush() failed.");

    status = waitForEventAndRelease(&acquireEvt);
    CHECK_ERROR(status, SDK_SUCCESS, "WaitForEventAndRelease(acquireEvt) Failed");

    // outBuffer imager
    status = kernel.setArg( 0 ,outputImageBuffer);
    CHECK_OPENCL_ERROR(status, "cl::Kernel.setArg() failed. (outputImageBuffer)");

    // pos
    status = kernel.setArg( 1 ,pos);
    CHECK_OPENCL_ERROR(status, "cl::Kernel.setArg() failed. (pos)");

	status = kernel.setArg(2, sphereBuffer);
	CHECK_OPENCL_ERROR(status, "cl::Kernel.setArg() failed. (spheres)");

	status = kernel.setArg(3, ONPtreeBuffer);
	/*status = kernel.setArg(5, sizeof(cl_float), nullptr);*/

	//printf("%d\n", sizeof(sphere));
    // Enqueue a kernel run call.
    size_t globalThreads[2] = {width, height};
    size_t localThreads[2] = {blockSizeX, blockSizeY};

    cl::NDRange gThreads(globalThreads[0], globalThreads[1]);
    cl::NDRange lThreads(localThreads[0], localThreads[1]);

    cl::Event ndrEvt;
    status = commandQueue.enqueueNDRangeKernel(kernel,
             cl::NullRange,
             gThreads,
             lThreads,
             0,
             &ndrEvt);
    CHECK_OPENCL_ERROR(status, "CommandQueue::enqueueNDRangeKernel() failed.");

    status = commandQueue.flush();
    CHECK_OPENCL_ERROR(status, "cl::CommandQueue.flush failed.");

    eventStatus = CL_QUEUED;
    while(eventStatus != CL_COMPLETE)
    {
        status = ndrEvt.getInfo<cl_int>(
                     CL_EVENT_COMMAND_EXECUTION_STATUS,
                     &eventStatus);
        CHECK_OPENCL_ERROR(status,
                           "cl::Event.getInfo(CL_EVENT_COMMAND_EXECUTION_STATUS) failed.");
    }

    if(sampleArgs->verify)
    {
        // Read back the value
        cl_int status = commandQueue.enqueueReadBuffer(
                            outputImageBuffer,
                            CL_TRUE,
                            0,
                            width * height * sizeof(cl_uchar4),
                            outputImageData,
                            NULL,
                            NULL);
        CHECK_OPENCL_ERROR(status, "cl::CommandQueue.enqueueReadBuffer failed.");
    }

    // Now OpenGL gets control of outputImageBuffer
    cl_event releaseEvt;
    status = clEnqueueReleaseGLObjects(
                 (commandQueue)(),
                 1,
                 &((outputImageBuffer)()),
                 0,
                 NULL,
                 &releaseEvt);
    CHECK_OPENCL_ERROR(status, "clEnqueueReleaseGLObjects failed.");

    status = commandQueue.flush();
    CHECK_OPENCL_ERROR(status, "cl::CommandQueue.flush() failed.");

    status = waitForEventAndRelease(&releaseEvt);
    CHECK_ERROR(status, SDK_SUCCESS, "WaitForEventAndRelease(releaseEvt) Failed");

    return SDK_SUCCESS;
}

int
RayCastCSG::initialize()
{
    // Call base class Initialize to get default configuration
    if(sampleArgs->initialize() != SDK_SUCCESS)
    {
        return SDK_FAILURE;
    }

    Option* iteration_option = new Option;
    CHECK_ALLOCATION(iteration_option, "Memory Allocation error.\n");

    iteration_option->_sVersion = "i";
    iteration_option->_lVersion = "iterations";
    iteration_option->_description = "Number of iterations to execute kernel";
    iteration_option->_type = CA_ARG_INT;
    iteration_option->_value = &iterations;

    sampleArgs->AddOption(iteration_option);

    delete iteration_option;

    Option* pos_option = new Option;
    CHECK_ALLOCATION(pos_option, "Memory Allocation error.\n");

    pos_option->_sVersion = "f";
    pos_option->_lVersion = "pos";
    pos_option->_description = "Noise pos";
    pos_option->_type = CA_ARG_INT;
    pos_option->_value = &pos;

    sampleArgs->AddOption(pos_option);

    delete pos_option;

    return SDK_SUCCESS;
}

int
RayCastCSG::setup()
{
    int status = 0;
    // Allocate host memory and read input image
    status = createOutputImage();
    CHECK_ERROR(status, SDK_SUCCESS, "Read Input Image Failed");

    // create and initialize timers
    int timer = sampleTimer->createTimer();
    sampleTimer->resetTimer(timer);
    sampleTimer->startTimer(timer);

    status = setupCL();
    if(status != SDK_SUCCESS)
    {
        if(status == SDK_EXPECTED_FAILURE)
        {
            return SDK_EXPECTED_FAILURE;
        }
        return SDK_FAILURE;
    }

    sampleTimer->stopTimer(timer);

    // Compute setup time
    setupTime = (double)(sampleTimer->readTimer(timer));

    return SDK_SUCCESS;

}

int
RayCastCSG::run()
{
    cl_int status = 0;
    // create and initialize timers
    int timer = sampleTimer->createTimer();
    sampleTimer->resetTimer(timer);

    if(!sampleArgs->quiet)
    {
        std::cout << "Executing kernel for " << iterations <<
                  " iterations" <<std::endl;
        std::cout << "-------------------------------------------" << std::endl;
    }

    sampleTimer->startTimer(timer);

    for(int i = 0; i < iterations; i++)
    {
        // Set kernel arguments and run kernel
        if(runCLKernels() != SDK_SUCCESS)
        {
            return SDK_FAILURE;
        }
    }

    sampleTimer->stopTimer(timer);
    // Compute kernel time
    kernelTime = (double)(sampleTimer->readTimer(timer)) / iterations;

    if(!sampleArgs->verify && !sampleArgs->quiet)
    {
        // Start the main glut loop
        std::cout << "\nPress key w to increase noise \n";
        std::cout << "Press key s to decrease noise \n";
        std::cout << "Press ESC key to quit \n";
#ifndef _WIN32
        XSelectInput(displayName,
                     win,
                     ExposureMask | KeyPressMask | ButtonPressMask);
        while(1)
        {
            /* handle the events in the queue */
            while (XPending(displayName) > 0)
            {
                XNextEvent(displayName, &xev);
                switch(xev.type)
                {
                    /* exit in case of a mouse button press */
                case ButtonPress:
                    std::cout<<"Mouse click Event "<<std::endl;
                    glXMakeCurrent(displayName, None, NULL);
                    glXDestroyContext(displayName, gGlCtx);
                    XDestroyWindow(displayName, win);
                    XCloseDisplay(displayName);
                    exit(0);
                    break;
                case KeyPress:
                    char buf[2];
                    int len;
                    KeySym keysym_return;
                    len = XLookupString(&xev.xkey,
                                        buf,
                                        1,
                                        &keysym_return,
                                        NULL);
                    if (len != 0)
                    {
                        if(buf[0] == (char)(27))//Escape character
                        {
                            glXMakeCurrent(displayName, None, NULL);
                            glXDestroyContext(displayName, gGlCtx);
                            XDestroyWindow(displayName, win);
                            XCloseDisplay(displayName);
                            exit(0);
                        }
                        else if ((buf[0] == 'w') || (buf[0] == 'W'))
                        {
                            pos++;//'W' key is pressed
                        }
                        else if ((buf[0] == 's') || (buf[0] == 'S'))
                        {
                            pos--;//'S' key is pressed
                        }
                    }
                    break;
                }
            }
            t1 = clock() * CLOCKS_PER_SEC;
            frameCount++;

            // Execute the kernel which computes Noise
            if(runCLKernels() != SDK_SUCCESS)
            {
                return SDK_FAILURE;
            }

            // Bind PBO and texture
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
            glBindTexture(GL_TEXTURE_2D, tex);

            // Copy pixels from pbo to texture
            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            width,
                            height,
                            GL_RGBA,
                            GL_UNSIGNED_BYTE,
                            NULL);

            // Display image using texture
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_LIGHTING);
            glEnable(GL_TEXTURE_2D);
            glEnable(GL_MULTISAMPLE);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

            glMatrixMode( GL_MODELVIEW);
            glLoadIdentity();

            glViewport(0, 0, windowWidth, windowHeight);

            glBegin(GL_QUADS);

            glTexCoord2f(0.0, 0.0);
            glVertex3f(-1.0, -1.0, 0.5);

            glTexCoord2f(1.0, 0.0);
            glVertex3f(1.0, -1.0, 0.5);

            glTexCoord2f(1.0, 1.0);
            glVertex3f(1.0, 1.0, 0.5);

            glTexCoord2f(0.0, 1.0);
            glVertex3f(-1.0, 1.0, 0.5);

            glEnd();

            glMatrixMode(GL_PROJECTION);
            glPopMatrix();

            glDisable(GL_TEXTURE_2D);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            glXSwapBuffers (displayName, win);
            t2 = clock() * CLOCKS_PER_SEC;
            totalElapsedTime += (double)(t2 - t1);
            if(frameCount && frameCount > frameRefCount)
            {
                // set GLUT Window Title
                char title[256];
                double fMs = (double)((totalElapsedTime / (double)CLOCKS_PER_SEC) /
                                      (double) frameCount);
                int framesPerSec = (int)(1.0 / (fMs / CLOCKS_PER_SEC));
#if defined (_WIN32) && !defined(__MINGW32__)
                sprintf_s(title, 256, "RayCastCSG | %d fps ", framesPerSec);
#else
                sprintf(title, "RayCastCSG | %d fps ", framesPerSec);
#endif
                //glutSetWindowTitle(title);
                XStoreName(displayName, win, title);
                frameCount = 0;
                totalElapsedTime = 0.0;
            }
        }
#else //Windows

        // program main loop
        while (!quit)
        {
            // check for messages
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                // handle or dispatch messages
                if (msg.message == WM_QUIT)
                {
                    quit = TRUE;
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

            }
            else
            {
                t1 = clock() * CLOCKS_PER_SEC;
                frameCount++;

                // Execute the kernel which computes Noise
                if(runCLKernels() != SDK_SUCCESS)
                {
                    return SDK_FAILURE;
                }

                // Bind PBO and texture
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, pbo);
                glBindTexture(GL_TEXTURE_2D, tex);

                // Copy pixels from pbo to texture
                glTexSubImage2D(GL_TEXTURE_2D,
                                0,
                                0,
                                0,
                                width,
                                height,
                                GL_RGBA,
                                GL_UNSIGNED_BYTE,
                                NULL);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                // Display image using texture
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_LIGHTING);
                glEnable(GL_TEXTURE_2D);
				glEnable(GL_MULTISAMPLE);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

                glMatrixMode(GL_PROJECTION);
                glPushMatrix();
                glLoadIdentity();
                glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);

                glMatrixMode( GL_MODELVIEW);
                glLoadIdentity();

                glViewport(0, 0, windowWidth, windowHeight);

                glBegin(GL_QUADS);

                glTexCoord2f(0.0, 0.0);
                glVertex3f(-1.0, -1.0, 0.5);

                glTexCoord2f(1.0, 0.0);
                glVertex3f(1.0, -1.0, 0.5);

                glTexCoord2f(1.0, 1.0);
                glVertex3f(1.0, 1.0, 0.5);

                glTexCoord2f(0.0, 1.0);
                glVertex3f(-1.0, 1.0, 0.5);

                glEnd();

                glMatrixMode(GL_PROJECTION);
                glPopMatrix();

                glDisable(GL_TEXTURE_2D);
                glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

                SwapBuffers(gHdc);

                t2 = clock() * CLOCKS_PER_SEC;
                totalElapsedTime += (double)(t2 - t1);
                if(frameCount && frameCount > frameRefCount)
                {
                    // set GLUT Window Title
                    char title[256];
                    double fMs = (double)((totalElapsedTime / (double)CLOCKS_PER_SEC) /
                                          (double) frameCount);
                    int framesPerSec = (int)(1.0 / (fMs / CLOCKS_PER_SEC));
#if defined (_WIN32) && !defined(__MINGW32__)
                    sprintf_s(title, 256, "RayCastCSG | %d fps ", framesPerSec);
#else
                    sprintf(title, "RayCastCSG | %d fps ", framesPerSec);
#endif
                    //glutSetWindowTitle(title);
                    frameCount = 0;
                    totalElapsedTime = 0.0;
                    SetWindowText(gHwnd, title);
                }
            }

        }
#endif
    }

    return SDK_SUCCESS;
}

int
RayCastCSG::cleanup()
{
    if(!byteRWSupport)
    {
        return SDK_SUCCESS;
    }

    // release program resources (input memory etc.)
    //FREE(inputImageData);

    FREE(outputImageData);

    //FREE(verificationOutput);

    return SDK_SUCCESS;
}

// Initialize the value to NULL
RayCastCSG *RayCastCSG::RayCast = NULL;


void
RayCastCSG::keyboardFunc(unsigned char key, int /*x*/, int /*y*/)
{
	switch (key) {
		/* If the user hits escape or Q, then exit
		 ESCAPE_KEY = 27 */
	case 'w':
	{
		pos.y++;
		break;
	}
	case 's':
	{
		pos.y--;
		break;
	}
	case 0x41:
	{
		pos.x--;
		break;
	}
	case 0x44:
	{
		pos.x++;
		break;
	}
	case 0x45:
	{
		pos.z++;
		break;
	}
	case 0x51:
	{
		pos.z--;
		break;
	}
    case 'q':
    case 27:
    {
        if(cleanup() != SDK_SUCCESS)
        {
            exit(1);
        }
        else
        {
            exit(0);
        }
    }
    default:
        break;
    }
}
#ifdef _WIN32
int
RayCastCSG::enableGLAndGetGLContext(HWND hWnd, HDC &hDC, HGLRC &hRC,
                                     cl_platform_id platform, cl_context &context, cl_device_id &interopDevice)
{
    cl_int status;
    BOOL ret = FALSE;
    DISPLAY_DEVICE dispDevice;
    DWORD deviceNum;
    int  pfmt;
    PIXELFORMATDESCRIPTOR  pfd;
    pfd.nSize           = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion        = 1;
    pfd.dwFlags         = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL  |
                          PFD_DOUBLEBUFFER ;
    pfd.iPixelType      = PFD_TYPE_RGBA;
    pfd.cColorBits      = 24;
    pfd.cRedBits        = 8;
    pfd.cRedShift       = 0;
    pfd.cGreenBits      = 8;
    pfd.cGreenShift     = 0;
    pfd.cBlueBits       = 8;
    pfd.cBlueShift      = 0;
    pfd.cAlphaBits      = 8;
    pfd.cAlphaShift     = 0;
    pfd.cAccumBits      = 0;
    pfd.cAccumRedBits   = 0;
    pfd.cAccumGreenBits = 0;
    pfd.cAccumBlueBits  = 0;
    pfd.cAccumAlphaBits = 0;
    pfd.cDepthBits      = 32;
    pfd.cStencilBits    = 16;
    pfd.cAuxBuffers     = 0;
    pfd.iLayerType      = PFD_MAIN_PLANE;
    pfd.bReserved       = 0;
    pfd.dwLayerMask     = 0;
    pfd.dwVisibleMask   = 0;
    pfd.dwDamageMask    = 0;

    ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));

    dispDevice.cb = sizeof(DISPLAY_DEVICE);

    DWORD displayDevices = 0;
    DWORD connectedDisplays = 0;
    int xCoordinate = 0;
    int yCoordinate = 0;
    int xCoordinate1 = 0;

    for (deviceNum = 0; EnumDisplayDevices(NULL, deviceNum, &dispDevice, 0);
            deviceNum++)
    {
        if (dispDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)
        {
            continue;
        }

        if(!(dispDevice.StateFlags & DISPLAY_DEVICE_ACTIVE))
        {
            continue;
        }

        DEVMODE deviceMode;

        // initialize the DEVMODE structure
        ZeroMemory(&deviceMode, sizeof(deviceMode));
        deviceMode.dmSize = sizeof(deviceMode);
        deviceMode.dmDriverExtra = 0;


        EnumDisplaySettings(dispDevice.DeviceName, ENUM_CURRENT_SETTINGS, &deviceMode);

        xCoordinate = deviceMode.dmPosition.x;
        yCoordinate = deviceMode.dmPosition.y;

        WNDCLASS windowclass;

        windowclass.style = CS_OWNDC;
        windowclass.lpfnWndProc = WndProc;
        windowclass.cbClsExtra = 0;
        windowclass.cbWndExtra = 0;
        windowclass.hInstance = NULL;
        windowclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        windowclass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        windowclass.lpszMenuName = NULL;
        windowclass.lpszClassName = reinterpret_cast<LPCSTR>("URNGNoiceGL");
        RegisterClass(&windowclass);

        gHwnd = CreateWindow(reinterpret_cast<LPCSTR>("URNGNoiceGL"),
                             reinterpret_cast<LPCSTR>("URNGNoiceGL"),
                             WS_CAPTION | WS_POPUPWINDOW,
                             sampleArgs->isDeviceIdEnabled() ? xCoordinate1 : xCoordinate,
                             yCoordinate,
                             windowWidth,
                             windowHeight,
                             NULL,
                             NULL,
                             windowclass.hInstance,
                             NULL);
        hDC = GetDC(gHwnd);

        pfmt = ChoosePixelFormat(hDC,
                                 &pfd);
        if(pfmt == 0)
        {
            std::cout << "Failed choosing the requested PixelFormat.\n";
            return SDK_FAILURE;
        }

        ret = SetPixelFormat(hDC, pfmt, &pfd);

        if(ret == FALSE)
        {
            std::cout << "Failed to set the requested PixelFormat.\n";
            return SDK_FAILURE;
        }

        hRC = wglCreateContext(hDC);
        if(hRC == NULL)
        {
            std::cout << "Failed to create a GL context"<<std::endl;
            return SDK_FAILURE;
        }

        ret = wglMakeCurrent(hDC, hRC);
        if(ret == FALSE)
        {
            std::cout<<"Failed to bind GL rendering context";
            return SDK_FAILURE;
        }
        displayDevices++;

        cl_context_properties properties[] =
        {
            CL_CONTEXT_PLATFORM, (cl_context_properties) platform,
            CL_GL_CONTEXT_KHR,   (cl_context_properties) hRC,
            CL_WGL_HDC_KHR,      (cl_context_properties) hDC,
            0
        };

        if (!clGetGLContextInfoKHR)
        {
#ifdef CL_VERSION_1_2
            clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)
                                    clGetExtensionFunctionAddressForPlatform(platform,"clGetGLContextInfoKHR");
#else
            clGetGLContextInfoKHR = (clGetGLContextInfoKHR_fn)
                                    clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
#endif
            if (!clGetGLContextInfoKHR)
            {
                std::cout << "Failed to query proc address for clGetGLContextInfoKHR";
                return SDK_FAILURE;
            }
        }

        size_t deviceSize = 0;
        status = clGetGLContextInfoKHR(properties,
                                       CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR,
                                       0,
                                       NULL,
                                       &deviceSize);
        CHECK_OPENCL_ERROR(status, "clGetGLContextInfoKHR failed!!");

        if (deviceSize == 0)
        {
            // no interopable CL device found, cleanup
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(hRC);
            DeleteDC(hDC);
            hDC = NULL;
            hRC = NULL;
            DestroyWindow(gHwnd);
            // try the next display
            continue;
        }
        else
        {
            if (sampleArgs->deviceId == 0)
            {
                ShowWindow(gHwnd, SW_SHOW);
                //Found a winner
                break;
            }
            else if (sampleArgs->deviceId != connectedDisplays)
            {
                connectedDisplays++;
                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(hRC);
                DeleteDC(hDC);
                hDC = NULL;
                hRC = NULL;
                DestroyWindow(gHwnd);
                if (xCoordinate >= 0)
                {
                    xCoordinate1 += deviceMode.dmPelsWidth;
                    // try the next display
                }
                else
                {
                    xCoordinate1 -= deviceMode.dmPelsWidth;
                }

                continue;
            }
            else
            {
                ShowWindow(gHwnd, SW_SHOW);
                //Found a winner
                break;
            }
        }

    }

    if (!hRC || !hDC)
    {
        OPENCL_EXPECTED_ERROR("OpenGL interoperability is not feasible.");
    }

    cl_context_properties properties[] =
    {
        CL_CONTEXT_PLATFORM, (cl_context_properties) platform,
        CL_GL_CONTEXT_KHR,   (cl_context_properties) hRC,
        CL_WGL_HDC_KHR,      (cl_context_properties) hDC,
        0
    };


    if (sampleArgs->deviceType.compare("gpu") == 0)
    {
        status = clGetGLContextInfoKHR( properties,
                                        CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR,
                                        sizeof(cl_device_id),
                                        &interopDevice,
                                        NULL);
        CHECK_OPENCL_ERROR(status, "clGetGLContextInfoKHR failed!!");

        // Create OpenCL context from device's id
        context = clCreateContext(properties,
                                  1,
                                  &interopDevice,
                                  0,
                                  0,
                                  &status);
        CHECK_OPENCL_ERROR(status, "clCreateContext failed!!");
    }
    else
    {
        context = clCreateContextFromType(
                      properties,
                      CL_DEVICE_TYPE_CPU,
                      NULL,
                      NULL,
                      &status);
        CHECK_OPENCL_ERROR(status, "clCreateContextFromType failed!!");
    }

    // OpenGL animation code goes here
    // GL init
    glewInit();
    if (! glewIsSupported("GL_VERSION_2_0 " "GL_ARB_pixel_buffer_object"))
    {
        std::cout << "Support for necessary OpenGL extensions missing."
                  << std::endl;
        return SDK_FAILURE;
    }

    //glEnable(GL_TEXTURE_2D);
    glClearColor(0.0, 0.0, 0.0, 1.0);
	glEnable(GL_MULTISAMPLE);
    glDisable(GL_DEPTH_TEST);

    glViewport(0, 0, windowWidth, windowHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(
        60.0,
        (GLfloat)width / (GLfloat)height,
        0.1,
        10.0);
    return SDK_SUCCESS;
}

void
RayCastCSG::disableGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
    wglMakeCurrent( NULL, NULL );
    wglDeleteContext( hRC );
    ReleaseDC( hWnd, hDC );
}

#else

void
RayCastCSG::enableGLForLinux( )
{

}

void
RayCastCSG::disableGLForLinux()
{

}

#endif

int
RayCastCSG::initializeGL(int argc, char * argv[])
{
    int status = 0;
    // Allocate host memory and read input image
    status = createOutputImage();
    CHECK_ERROR(status, SDK_SUCCESS, "Read InputImage Failed");

    return SDK_SUCCESS;
}


int
main(int argc, char * argv[])
{
    RayCastCSG RayCast;
    RayCastCSG::RayCast = &RayCast;

    if(RayCast.initializeGL(argc, argv) != SDK_SUCCESS)
    {
        return SDK_FAILURE;
    }

    if(RayCast.initialize() != SDK_SUCCESS)
    {
        return SDK_FAILURE;
    }

    if(RayCast.sampleArgs->parseCommandLine(argc, argv) != SDK_SUCCESS)
    {
        return SDK_FAILURE;
    }

    if(RayCast.sampleArgs->isDumpBinaryEnabled())
    {
        return RayCast.genBinaryImage();
    }

    int status = RayCast.setup();
    if (status != SDK_SUCCESS)
    {
        return status;
    }

    if(RayCast.run() != SDK_SUCCESS)
    {
        return SDK_FAILURE;
    }


#ifdef _WIN32
    RayCast.disableGL(gHwnd, gHdc, gGlCtx);
#else
    //clURNG.disableGLForLinux();
#endif

    if(RayCast.cleanup() != SDK_SUCCESS)
    {
        return SDK_FAILURE;
    }

    //RayCast.printStats();
    return SDK_SUCCESS;
}
