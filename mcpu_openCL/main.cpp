//#include <stdio.h>
#include <stdlib.h>
#include "lodepng.h"
#include <chrono>
#include <vector>
#include <iostream>
#include <fstream>

using namespace std;



#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.hpp>
#endif

#define VECTOR_SIZE 735*504 //width*height*number_of_images
#define IMAGE_SIZE 2940*2016*4
//#define NON_OPTIMIZED

/////////////////////////////////////////////////////////////////////////non-optimized code///////////////////////////////////////////////////
#ifdef NON_OPTIMIZED
//Read kernel from file
int readKernel(char* &kernel_source)
{
	FILE *fp;
	size_t source_size, program_size;

	fp = fopen("kernel.cl", "rb");
	if (!fp) {
		printf("Failed to load kernel\n");
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	program_size = ftell(fp);
	rewind(fp);
	kernel_source = (char*)malloc(program_size + 1);
	kernel_source[program_size] = '\0';
	fread(kernel_source, sizeof(char), program_size, fp);
	fclose(fp);
	return 0;
}

//read images
void readImages(unsigned char* &img1, unsigned char* &img2, unsigned &width, unsigned &height)
{
	//get the image input
	printf("Loading images");
	const char* fileName1 = ".\\images\\im0.png";
	const char* fileName2 = ".\\images\\im1.png";
	unsigned error;
	error = lodepng_decode32_file(&img1, &width, &height, fileName1);
	error = lodepng_decode32_file(&img2, &width, &height, fileName2);


}
void downSample(vector <unsigned char> &image, vector <unsigned char> &R, vector <unsigned char> &G, vector <unsigned char> &B)
{
	//downsampling images by a factor of 1/16
	int j = 0;
	int k = 0;
	for (int j = 0; j < 2016; j += 4)
	{
		for (int i = 11760 * j; i < image.size(); i = i + 16)
		{
			if (i == 11760 * (j + 1))
			{
				break;
			}
			R.push_back(image[i]);
			G.push_back(image[i + 1]);
			B.push_back(image[i + 2]);
		}
	}
}

void acquireImage(unsigned &width, unsigned &height, vector<unsigned char> &img1, vector<unsigned char> &img2)
{
	printf("Loading images \n");
	const char* fileName1 = ".\\images\\im0.png";
	const char* fileName2 = ".\\images\\im1.png";
	//the raw pixels
	//decode
	lodepng::decode(img1, width, height, fileName1);
	lodepng::decode(img2, width, height, fileName2);
	printf("%d \n", width);


}
void saveImage(float* &img, string name)
{
	unsigned w = 735, h = 504;
	string path = ".\\images\\";
	string fileName1 = path + name + ".png";
	vector<unsigned char> output_img1, output_img2;
	for (int i = 0; i < VECTOR_SIZE; i++)
	{
			for (int k = 0; k < 3; k++)
			{
				output_img1.push_back((unsigned char)img[i]);
			}
			output_img1.push_back(255);
	}

	int error = lodepng::encode(fileName1, output_img1, w, h);
	if(error !=0)
		printf("error %s: %d \n", name, error);

}

int main(void) {

	//Read images into vectors
	vector<unsigned char> img1;
	vector<unsigned char> img2;
	unsigned w, h;
	acquireImage(w, h, img1, img2);
	//resize the input using linear execution --This could be optimized later in the next phase
	vector <unsigned char> R_1, G_1, B_1;
	vector <unsigned char> R_2, G_2, B_2;
	downSample(img1, R_1, G_1, B_1);
	downSample(img2, R_2, G_2, B_2);
	//load kernel into  a string
	char *kernel_string;
	readKernel(kernel_string);

	//Create gray images vectors that will store results for image1 and image2, float used for keeping accuracy in intermediate results
	float *gray_img1 = (float*)malloc(sizeof(float)*VECTOR_SIZE); //result gray image
	float *gray_img2 = (float*)malloc(sizeof(float)*VECTOR_SIZE); //result gray image
	//create disparity map vectors
	float *disp1 = (float*)malloc(sizeof(float)*VECTOR_SIZE); 
	float *disp2 = (float*)malloc(sizeof(float)*VECTOR_SIZE); 
	//create cross-checking and occlusion
	float *delta_disp = (float*)malloc(sizeof(float)*VECTOR_SIZE);
	float *occlusion = (float*)malloc(sizeof(float)*VECTOR_SIZE);

	
	////////////////////////////////////////////////////////////////////openCL specific functions/////////////////////////////////////////////////
	// Get platform and device information
	cl_platform_id * platforms = NULL;
	cl_uint     num_platforms;
	//Set up the Platform
	cl_int clStatus = clGetPlatformIDs(0, NULL, &num_platforms);
	platforms = (cl_platform_id *) malloc(sizeof(cl_platform_id)*num_platforms);
	clStatus = clGetPlatformIDs(num_platforms, platforms, NULL);

	//Get the devices list and choose the device you want to run on
	cl_device_id     *device_list = NULL;
	cl_uint           num_devices;

	clStatus = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);
	device_list = (cl_device_id *)malloc(sizeof(cl_device_id)*num_devices);
	clStatus = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, num_devices, device_list, NULL);

	// Create one OpenCL context for each device in the platform
	cl_context context;
	context = clCreateContext(NULL, num_devices, device_list, NULL, NULL, &clStatus);

	/////////// Create a program from the kernel source
	cl_program program = clCreateProgramWithSource(context, 1, (const char **)&kernel_string, NULL, &clStatus);

	/////////// Build the program
	clStatus = clBuildProgram(program, 1, device_list, NULL, NULL, NULL);
	if (clStatus != 0)
	{
		return clStatus;
	}

	/////////// Create a command queue
	cl_command_queue command_queue = clCreateCommandQueue(context, device_list[0], 0, &clStatus);
	////////////////////////////////////////////////// gray_kerel///////////////////////////////////////////////////////////////////////////////
	// Create memory buffers on the device for each vector RGB and the output vector gray for img1
	cl_mem R1_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem G1_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem B1_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem gray_img1_clmem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	// Create memory buffers on the device for each vector RGB and the output vector gray for img2
	cl_mem R2_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem G2_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem B2_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem gray_img2_clmem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);

	//////////// Copy buffers to the device memory
	////gray buffers
	//img1
	clStatus = clEnqueueWriteBuffer(command_queue, R1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(unsigned char), &R_1[0], 0, NULL, NULL);
	clStatus = clEnqueueWriteBuffer(command_queue, G1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(unsigned char), &G_1[0], 0, NULL, NULL);
	clStatus = clEnqueueWriteBuffer(command_queue, B1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(unsigned char), &B_1[0], 0, NULL, NULL);
	//img2
	clStatus = clEnqueueWriteBuffer(command_queue, R2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(unsigned char), &R_2[0], 0, NULL, NULL);
	clStatus = clEnqueueWriteBuffer(command_queue, G2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(unsigned char), &G_2[0], 0, NULL, NULL);
	clStatus = clEnqueueWriteBuffer(command_queue, B2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(unsigned char), &B_2[0], 0, NULL, NULL);


	

	/////////// Create the OpenCL kernel for function gray img1 and img2
	cl_kernel gray1_kernel = clCreateKernel(program, "gray", &clStatus);
	cl_kernel gray2_kernel = clCreateKernel(program, "gray", &clStatus);


	/////////// Set the arguments of the kernel for gray
	//img1
	clStatus = clSetKernelArg(gray1_kernel, 0, sizeof(cl_mem), (void *)&R1_clmem);
	clStatus = clSetKernelArg(gray1_kernel, 1, sizeof(cl_mem), (void *)&G1_clmem);
	clStatus = clSetKernelArg(gray1_kernel, 2, sizeof(cl_mem), (void *)&B1_clmem);
	clStatus = clSetKernelArg(gray1_kernel, 3, sizeof(cl_mem), (void *)&gray_img1_clmem);
	//img2
	clStatus = clSetKernelArg(gray2_kernel, 0, sizeof(cl_mem), (void *)&R2_clmem);
	clStatus = clSetKernelArg(gray2_kernel, 1, sizeof(cl_mem), (void *)&G2_clmem);
	clStatus = clSetKernelArg(gray2_kernel, 2, sizeof(cl_mem), (void *)&B2_clmem);
	clStatus = clSetKernelArg(gray2_kernel, 3, sizeof(cl_mem), (void *)&gray_img2_clmem);

	///////////define local and global sizes
	size_t global_size = VECTOR_SIZE; // Process the entire lists
	size_t local_size = 735;           // Process one item at a time

	//////////////////////////////////////////////////////start timer beginning///////////////////////////////////////////////////////
	auto start = chrono::high_resolution_clock::now();/////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////put the gray kernel into the command queue
	//img1
	clStatus = clEnqueueNDRangeKernel(command_queue, gray1_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	//img2
	clStatus = clEnqueueNDRangeKernel(command_queue, gray2_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);

	//Read gray vectors from device to local memory
	clStatus = clEnqueueReadBuffer(command_queue, gray_img1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), gray_img1, 0, NULL, NULL);
	clStatus = clEnqueueReadBuffer(command_queue, gray_img2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), gray_img2, 0, NULL, NULL);

	/////////// Clean up and wait for all the comands to complete.
	clStatus = clFlush(command_queue);
	clStatus = clFinish(command_queue);

	///////////////////////////////////////////////////////Stop timer and save image //////////////////////////////////////////////
	auto finish = chrono::high_resolution_clock::now();
	chrono::duration<double> elapsed_gray = finish - start;
	printf("gray: %f \n", elapsed_gray);
	//save gray images
	saveImage(gray_img1, "gray1");
	saveImage(gray_img2, "gray2");
	//Release memory from device and host
	clStatus = clReleaseKernel(gray1_kernel);
	clStatus = clReleaseKernel(gray2_kernel);
	clStatus = clReleaseMemObject(R1_clmem);
	clStatus = clReleaseMemObject(G1_clmem);
	clStatus = clReleaseMemObject(B1_clmem);
	clStatus = clReleaseMemObject(gray_img1_clmem);
	clStatus = clReleaseMemObject(gray_img2_clmem);


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	////////////////////////////////////////////////// zncc_slow_kerel///////////////////////////////////////////////////////////////////////////////



	/////////////zncc_kerel
	// Create memory buffers for disparity1, disparity2, delta_dsip
	cl_mem disp1_clmem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem disp2_clmem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem delta_disp_clmem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem zncc_gray_img1_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem zncc_gray_img2_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);

	//////////// Copy buffers to the device memory
	////gray1 and gray2
	clStatus = clEnqueueWriteBuffer(command_queue, zncc_gray_img1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), gray_img1, 0, NULL, NULL);
	clStatus = clEnqueueWriteBuffer(command_queue, zncc_gray_img2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), gray_img2, 0, NULL, NULL);


	/////////// Create the OpenCL kernel for function gray img1 and img2
	cl_kernel zncc_slow_kernel = clCreateKernel(program, "zncc_slow", &clStatus);

	///////////set the arguments of the kernel for zncc
	clStatus = clSetKernelArg(zncc_slow_kernel, 0, sizeof(cl_mem), (void *)&zncc_gray_img1_clmem);
	clStatus = clSetKernelArg(zncc_slow_kernel, 1, sizeof(cl_mem), (void *)&zncc_gray_img2_clmem);
	clStatus = clSetKernelArg(zncc_slow_kernel, 2, sizeof(cl_mem), (void *)&disp1_clmem);
	clStatus = clSetKernelArg(zncc_slow_kernel, 3, sizeof(cl_mem), (void *)&disp2_clmem);
	clStatus = clSetKernelArg(zncc_slow_kernel, 4, sizeof(cl_mem), (void *)&delta_disp_clmem);

	///////////define local and global sizes
	global_size = VECTOR_SIZE; // Process the entire lists
	//local_size = 735;           // Process one item at a time
	//////////////////////////////////////////////////////start timer beginning///////////////////////////////////////////////////////
	start = chrono::high_resolution_clock::now();/////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	///////////put the zncc_slow kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, zncc_slow_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	
	//Read disparity vectors from device to local memory
	clStatus = clEnqueueReadBuffer(command_queue, disp1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), disp1, 0, NULL, NULL);
	clStatus = clEnqueueReadBuffer(command_queue, disp2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), disp2, 0, NULL, NULL);
	clStatus = clEnqueueReadBuffer(command_queue, delta_disp_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float),delta_disp, 0, NULL, NULL);


	/////////// Clean up and wait for all the comands to complete.
	clStatus = clFlush(command_queue);
	clStatus = clFinish(command_queue);

	///////////////////////////////////////////////////////Stop timer and save image //////////////////////////////////////////////
	finish = chrono::high_resolution_clock::now();
	chrono::duration<double> elapsed_zncc = finish - start;
	printf("zncc_slow: %f \n", elapsed_zncc);
	//save gray images
	saveImage(disp1, "disp1");
	saveImage(disp2, "disp2");
	saveImage(delta_disp, "cross_checking");
	//Release memory from device and host
	clStatus = clReleaseKernel(zncc_slow_kernel);
	clStatus = clReleaseMemObject(disp1_clmem);
	clStatus = clReleaseMemObject(disp2_clmem);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////// occlusion_slow_kerel///////////////////////////////////////////////////////////////////////////////
	/////////////occlusion_kerel
	// Create memory buffers for delta_dsip and occlusion
	cl_mem occlusion_delta_disp_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem occlusion_clmem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	
	//////////// Copy buffers to the device memory
	////
	clStatus = clEnqueueWriteBuffer(command_queue, occlusion_delta_disp_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), delta_disp, 0, NULL, NULL);
	

	/////////// Create the OpenCL kernel for function gray img1 and img2
	cl_kernel occlusion_slow_kernel = clCreateKernel(program, "occlusion_fillig_slow", &clStatus);

	///////////set the arguments of the kernel for zncc
	clStatus = clSetKernelArg(occlusion_slow_kernel, 0, sizeof(cl_mem), (void *)&occlusion_delta_disp_clmem);
	clStatus = clSetKernelArg(occlusion_slow_kernel, 1, sizeof(cl_mem), (void *)&occlusion_clmem);
	
	///////////define local and global sizes
	global_size = VECTOR_SIZE; // Process the entire lists
	//local_size = 8;           // Process one item at a time

	///////////////////////////////////////////////////////start timer beginning///////////////////////////////////////////////////////
	start = chrono::high_resolution_clock::now();/////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////put the occlusion_slow kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, occlusion_slow_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);

	//Read occlusion vector from device to local memory
	clStatus = clEnqueueReadBuffer(command_queue, occlusion_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), occlusion, 0, NULL, NULL);

	
	/////////// Clean up and wait for all the comands to complete.
	clStatus = clFlush(command_queue);
	clStatus = clFinish(command_queue);

	///////////////////////////////////////////////////////Stop timer and save image //////////////////////////////////////////////
	finish = chrono::high_resolution_clock::now();
	chrono::duration<double> elapsed_occlusion = finish - start;
	printf("occlusion_slow: %f \n", elapsed_occlusion);
	//save gray images
	saveImage(occlusion, "occlusion");
	//Release memory from device and host
	clStatus = clReleaseKernel(occlusion_slow_kernel);
	clStatus = clReleaseMemObject(occlusion_delta_disp_clmem);
	clStatus = clReleaseMemObject(occlusion_clmem);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	//print display total time
	printf("total time: %f \n", elapsed_gray + elapsed_zncc + elapsed_occlusion);
	system("pause");
	// Finally release all OpenCL allocated objects and host buffers.
	clStatus = clReleaseProgram(program);
	clStatus = clReleaseCommandQueue(command_queue);
	clStatus = clReleaseContext(context);

	
	free(gray_img1);
	free(platforms);
	free(device_list);
	return 0;
}




///////////////////////////////////////////////////////////////////////// Optimized code///////////////////////////////////////////////////
#else

//Read kernel from file
int readKernel(char* &kernel_source)
{/*
	FILE *fp;
	size_t source_size, program_size;

	fp = fopen("kernel.cl", "rb");
	if (!fp) {
		printf("Failed to load kernel\n");
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	program_size = ftell(fp);
	rewind(fp);
	kernel_source = (char*)malloc(program_size + 1);
	kernel_source[program_size] = '\0';
	fread(kernel_source, sizeof(char), program_size, fp);
	fclose(fp);*/
	return 0;
}

//read images
void readImages(unsigned char* &img1, unsigned char* &img2, unsigned &width, unsigned &height)
{
	//get the image input
	printf("Loading images");
	const char* fileName1 = ".\\images\\im0.png";
	const char* fileName2 = ".\\images\\im1.png";
	unsigned error;
	error = lodepng_decode32_file(&img1, &width, &height, fileName1);
	error = lodepng_decode32_file(&img2, &width, &height, fileName2);


}


void acquireImage(unsigned &width, unsigned &height, vector<unsigned char> &img1, vector<unsigned char> &img2)
{
	const char* fileName1 = ".\\images\\im0.png";
	const char* fileName2 = ".\\images\\im1.png";
	//the raw pixels
	//decode
	lodepng::decode(img1, width, height, fileName1);
	lodepng::decode(img2, width, height, fileName2);


}
void saveImage(float* &img, string name)
{
	unsigned w = 735, h = 504;
	string path = ".\\images\\";
	string fileName1 = path + name + ".png";
	vector<unsigned char> output_img1, output_img2;
	for (int i = 0; i < VECTOR_SIZE; i++)
	{
		for (int k = 0; k < 3; k++)
		{
			output_img1.push_back((unsigned char)img[i]);
		}
		output_img1.push_back(255);
	}

	int error = lodepng::encode(fileName1, output_img1, w, h);
	if (error != 0)
		printf("error %s: %d \n", name, error);

}


void getAvailableDevicesInfo()
{
	// Gettings all available platforms
	vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);

	_ASSERT(platforms.size() > 0);

	vector <cl::Device> devices;

	for (auto platform : platforms)
	{
		// Get all platform devices 
		platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);

		cout << "* Platform name: ";
		cout << platform.getInfo<CL_PLATFORM_NAME>() << endl;
		cout << "CL version:" << platform.getInfo<CL_PLATFORM_VERSION>() << endl;

		_ASSERT(devices.size() > 0);

		cout << "Platform devices:" << endl;
		for (auto device : devices)
		{
			cout << "Device name: " << device.getInfo<CL_DEVICE_NAME>() << endl;
			cout << "Device vendor: " << device.getInfo<CL_DEVICE_VENDOR>() << endl;
			cout << "Device local memory type: " << device.getInfo<CL_DEVICE_LOCAL_MEM_TYPE>() << endl;
			cout << "Device local memory size: " << device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() << endl;
			cout << "Device max frequency: " << device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>() << endl;
			cout << "Device max constant buffer size: " << device.getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>() << endl;
			cout << "Device max work group size: " << device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>() << endl;
			cout << "Device max work item size: " << device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>()[0] << endl;
			cout << endl;
		}
		cout << endl;
	}

}
double getKernelExecutionTime(cl_event event,const char* kernel_name)
{
	cl_ulong time_start;
	cl_ulong time_end;

	clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(time_start), &time_start, NULL);
	clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(time_end), &time_end, NULL);
	double nanoSeconds = time_end - time_start;
	printf("OpenCl Execution time of %s is: %0.3f milliseconds \n", kernel_name, nanoSeconds / 1000000.0);
	return nanoSeconds / 1000000.0;
}

int main(void) {

	//get devices information
	//getAvailableDevicesInfo();

	//Read images into vectors
	vector<unsigned char> img1;
	vector<unsigned char> img2;
	unsigned w, h;
	cout << "Reading images ...." << endl;
	acquireImage(w, h, img1, img2);
	//resize the input using linear execution --This could be optimized later in the next phase
	//load kernel into  a string
	const char *kernel_string;
	ifstream kernel_file("kernel.cl");
	string src(istreambuf_iterator<char>(kernel_file), (istreambuf_iterator<char>()));
	kernel_string = src.c_str();

	//Create gray images vectors that will store results for image1 and image2, float used for keeping accuracy in intermediate results
	float *gray_img1 = (float*)malloc(sizeof(float)*VECTOR_SIZE); //result gray image
	float *gray_img2 = (float*)malloc(sizeof(float)*VECTOR_SIZE); //result gray image
	//create disparity map vectors
	float *disp1 = (float*)malloc(sizeof(float)*VECTOR_SIZE);
	float *disp2 = (float*)malloc(sizeof(float)*VECTOR_SIZE);
	float *occlusion = (float*)malloc(sizeof(float)*VECTOR_SIZE);


	////////////////////////////////////////////////////////////////////openCL specific functions/////////////////////////////////////////////////
	// Get platform and device information
	cl_platform_id * platforms = NULL;
	cl_uint     num_platforms;
	//Set up the Platform
	cl_int clStatus = clGetPlatformIDs(0, NULL, &num_platforms);
	platforms = (cl_platform_id *)malloc(sizeof(cl_platform_id)*num_platforms);
	clStatus = clGetPlatformIDs(num_platforms, platforms, NULL);

	//Get the devices list and choose the device you want to run on
	cl_device_id     *device_list = NULL;
	cl_uint           num_devices;

	clStatus = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);
	device_list = (cl_device_id *)malloc(sizeof(cl_device_id)*num_devices);
	clStatus = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, num_devices, device_list, NULL);

	// Create one OpenCL context for each device in the platform
	cl_context context;
	context = clCreateContext(NULL, num_devices, device_list, NULL, NULL, &clStatus);
	
	// Create a command queue
	
	cl_command_queue command_queue = clCreateCommandQueue(context, device_list[0], CL_QUEUE_PROFILING_ENABLE, &clStatus);
	char name[50];
	clGetDeviceInfo(device_list[0], CL_DEVICE_NAME, size(name), name, NULL);
	cout << "Device name: " << name << endl;
	/////////////dowsample_gray_kerel
	// Create memory buffers on the device for images and the output vector gray
	//img1
	cl_mem img1_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, (IMAGE_SIZE) * sizeof(unsigned char), NULL, &clStatus);
	cl_mem gray_img1_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	//img2
	cl_mem img2_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, (IMAGE_SIZE) * sizeof(unsigned char), NULL, &clStatus);
	cl_mem gray_img2_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);


	/////////////mean_kerel
	cl_mem mean1_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE , VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem mean2_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE , VECTOR_SIZE * sizeof(float), NULL, &clStatus);


	/////////////zncc_kerel
	// Create memory buffers for disparity1, disparity2, delta_dsip
	cl_mem disp1_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem disp2_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);

	/////////////cross_checking_kerel
	// Create memory buffers for cross_checking
	cl_mem delta_disp_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);

	/////////////occlusion_kerel
	// Create memory buffers for occlusion
	cl_mem occlusion_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);


	//////////// Copy buffers to the device memory
	////gray buffers
	//img1
	clStatus = clEnqueueWriteBuffer(command_queue, img1_clmem, CL_TRUE, 0, (IMAGE_SIZE) * sizeof(unsigned char), &img1[0], 0, NULL, NULL);
	//img2
	clStatus = clEnqueueWriteBuffer(command_queue, img2_clmem, CL_TRUE, 0, (IMAGE_SIZE) * sizeof(unsigned char), &img2[0], 0, NULL, NULL);



	////mean buffers 
	//none at the moment since gray is already in device memory

	////ZNCC buffers 
	//none at the moment since gray and mean are already in device memory

	////cross_checking buffers 
	//none at the moment since disp is already in device memory

	////occlusion buffers 
	//none at the moment since delta_disp is already in device memory



	/////////// Create a program from the kernel source
	cl_program program = clCreateProgramWithSource(context, 1, (const char **)&kernel_string, NULL, &clStatus);

	/////////// Build the program
	cout << "Building kernel..." << endl;
	clStatus = clBuildProgram(program, 1, device_list, "-cl-mad-enable", NULL, NULL);
	if (clStatus != 0)
	{
		return clStatus;
	}
	cout << "Kernel build is done" << endl;


	/////////// Create the OpenCL kernel for function gray img1 and img2
	cl_kernel gray1_kernel = clCreateKernel(program, "downsample_and_gray", &clStatus);
	cl_kernel gray2_kernel = clCreateKernel(program, "downsample_and_gray", &clStatus);

	/////////// Create the OpenCL kernel for function mean
	cl_kernel mean_kernel = clCreateKernel(program, "mean", &clStatus);

	/////////// Create the OpenCL kernel for function zncc
	cl_kernel zncc_kernel = clCreateKernel(program, "zncc", &clStatus);

	/////////// Create the OpenCL kernel for function cross_checking
	cl_kernel cross_checking_kernel = clCreateKernel(program, "cross_checking", &clStatus);

	/////////// Create the OpenCL kernel for function occlusion
	cl_kernel occlusion_kernel = clCreateKernel(program, "occlusion_fillig", &clStatus);

	/////////// Set the arguments of the kernel for downsample_gray
	//img1
	clStatus = clSetKernelArg(gray1_kernel, 0, sizeof(cl_mem), (void *)&img1_clmem);
	clStatus = clSetKernelArg(gray1_kernel, 1, sizeof(cl_mem), (void *)&gray_img1_clmem);
	//img2
	clStatus = clSetKernelArg(gray2_kernel, 0, sizeof(cl_mem), (void *)&img2_clmem);
	clStatus = clSetKernelArg(gray2_kernel, 1, sizeof(cl_mem), (void *)&gray_img2_clmem);


	///////////set the arguments of the kernel for mean
	clStatus = clSetKernelArg(mean_kernel, 0, sizeof(cl_mem), (void *)&gray_img1_clmem);
	clStatus = clSetKernelArg(mean_kernel, 1, sizeof(cl_mem), (void *)&gray_img2_clmem);
	clStatus = clSetKernelArg(mean_kernel, 2, sizeof(cl_mem), (void *)&mean1_clmem);
	clStatus = clSetKernelArg(mean_kernel, 3, sizeof(cl_mem), (void *)&mean2_clmem);


	///////////set the arguments of the kernel for zncc
	clStatus = clSetKernelArg(zncc_kernel, 0, sizeof(cl_mem), (void *)&gray_img1_clmem);
	clStatus = clSetKernelArg(zncc_kernel, 1, sizeof(cl_mem), (void *)&gray_img2_clmem);
	clStatus = clSetKernelArg(zncc_kernel, 2, sizeof(cl_mem), (void *)&mean1_clmem);
	clStatus = clSetKernelArg(zncc_kernel, 3, sizeof(cl_mem), (void *)&mean2_clmem);
	clStatus = clSetKernelArg(zncc_kernel, 4, sizeof(cl_mem), (void *)&disp1_clmem);
	clStatus = clSetKernelArg(zncc_kernel, 5, sizeof(cl_mem), (void *)&disp2_clmem);

	///////////set the arguments of the kernel for cross_checking
	clStatus = clSetKernelArg(cross_checking_kernel, 0, sizeof(cl_mem), (void *)&disp1_clmem);
	clStatus = clSetKernelArg(cross_checking_kernel, 1, sizeof(cl_mem), (void *)&disp2_clmem);
	clStatus = clSetKernelArg(cross_checking_kernel, 2, sizeof(cl_mem), (void *)&delta_disp_clmem);

	///////////set the arguments of the kernel for occlusion
	clStatus = clSetKernelArg(occlusion_kernel, 0, sizeof(cl_mem), (void *)&delta_disp_clmem);
	clStatus = clSetKernelArg(occlusion_kernel, 1, sizeof(cl_mem), (void *)&occlusion_clmem);


	///////////////////////////////////////////////////////start timer beginning///////////////////////////////////////////////////////
	cout << "starting the timer now ......" << endl;
	auto start = chrono::high_resolution_clock::now();/////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////define local and global sizes
	size_t global_size = IMAGE_SIZE; // Process the entire lists
	size_t local_size_1 = 120;           // Process one item at a time

	//using event for time
	cl_event event_gray1, event_gray2, event_mean, event_zncc, event_cross_checking, event_occlusion;


	///////////put the gray kernel into the command queue
	//img1
	clStatus = clEnqueueNDRangeKernel(command_queue, gray1_kernel, 1, NULL, &global_size, &local_size_1, 0, NULL, &event_gray1);
	//img2
	clStatus = clEnqueueNDRangeKernel(command_queue, gray2_kernel, 1, NULL, &global_size, &local_size_1, 0, NULL, &event_gray2);

	global_size = VECTOR_SIZE; // Process the entire lists
	size_t local_size = 441;           // Process one item at a time

	///////////put the zncc kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, mean_kernel, 1, NULL, &global_size, &local_size, 0, NULL, &event_mean);

	///////////put the zncc kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, zncc_kernel, 1, NULL, &global_size, &local_size, 0, NULL, &event_zncc);

	///////////put the cross_checking kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, cross_checking_kernel, 1, NULL, &global_size, &local_size, 0, NULL, &event_cross_checking);

	///////////put the occlusion kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, occlusion_kernel, 1, NULL, &global_size, &local_size, 0, NULL, &event_occlusion);

	/////////Read memory from device to local host memory

	//Read mean vectors from device to local memory
	clStatus = clEnqueueReadBuffer(command_queue, gray_img1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), gray_img1, 0, NULL, NULL);
	clStatus = clEnqueueReadBuffer(command_queue, disp1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), disp1, 0, NULL, NULL);
	clStatus = clEnqueueReadBuffer(command_queue, disp2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), disp2, 0, NULL, NULL);
	clStatus = clEnqueueReadBuffer(command_queue, occlusion_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), occlusion, 0, NULL, NULL);


	/////////// Clean up and wait for all the comands to complete.

	clStatus = clFlush(command_queue);
	clWaitForEvents(1, &event_gray1);
	clWaitForEvents(1, &event_gray2);
	clWaitForEvents(1, &event_mean);
	clWaitForEvents(1, &event_zncc);
	clWaitForEvents(1, &event_cross_checking);
	clWaitForEvents(1, &event_occlusion);
	clStatus = clFinish(command_queue);

	///////////////////////////////////////////////////////Stop timer and save image //////////////////////////////////////////////
	auto finish = chrono::high_resolution_clock::now();
	chrono::duration<double> elapsed = finish - start;
	printf("finished, total execution time using timer: %f  milliseconds\n", elapsed*1000);

	//testing another timer
	double gray1_time = getKernelExecutionTime(event_gray1, "gray1");
	double gray2_time = getKernelExecutionTime(event_gray2, "gray2");
	double mean_time = getKernelExecutionTime(event_mean, "mean");
	double zncc_time = getKernelExecutionTime(event_zncc, "zncc");
	double cross_checking_time = getKernelExecutionTime(event_cross_checking, "cross_checking");
	double occlusion_time = getKernelExecutionTime(event_occlusion, "occlusion");
	double total_time = gray1_time + gray2_time + zncc_time + mean_time + cross_checking_time + occlusion_time;
	cout << "\n Total execution time using openCL event is: " << total_time << "milliseconds"<< endl;
	cout << "saving images ......" << endl;
	system("pause");
	saveImage(occlusion, "occlusion_optimized");
	
	//saveImage(gray_img1, "gray1");
	//saveImage(gray_img2, "gray2");
	saveImage(disp1, "disp1_optimized");
	saveImage(disp2, "disp2_optimized");
	cout << "Releasing memory elemnts ......" << endl;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Finally release all OpenCL allocated objects and host buffers.
	clStatus = clReleaseKernel(gray1_kernel);
	clStatus = clReleaseKernel(gray2_kernel);
	clStatus = clReleaseKernel(zncc_kernel);
	clStatus = clReleaseKernel(cross_checking_kernel);
	clStatus = clReleaseKernel(occlusion_kernel);
	clStatus = clReleaseProgram(program);
	clStatus = clReleaseMemObject(gray_img1_clmem);
	clStatus = clReleaseMemObject(gray_img2_clmem);
	clStatus = clReleaseMemObject(disp1_clmem);
	clStatus = clReleaseMemObject(disp2_clmem);
	clStatus = clReleaseMemObject(delta_disp_clmem);
	clStatus = clReleaseMemObject(occlusion_clmem);
	clStatus = clReleaseCommandQueue(command_queue);
	clStatus = clReleaseContext(context);
	free(platforms);
	free(device_list);
	//to do: release rest of the memory, although not needed, since OS would realocate that anyways
	return 0;
}



#endif