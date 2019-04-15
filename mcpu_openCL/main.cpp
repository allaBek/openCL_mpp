#include <stdio.h>
#include <stdlib.h>
#include "lodepng.h"
#include <chrono>
#include <vector>
using namespace std;

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#define VECTOR_SIZE 735*504 //width*height*number_of_images




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

	// Create a command queue
	cl_command_queue command_queue = clCreateCommandQueue(context, device_list[0], 0, &clStatus);

	/////////////gray_kerel
	// Create memory buffers on the device for each vector RGB and the output vector gray for img1
	cl_mem R1_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem G1_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem B1_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem gray_img1_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	// Create memory buffers on the device for each vector RGB and the output vector gray for img2
	cl_mem R2_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem G2_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem B2_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(unsigned char), NULL, &clStatus);
	cl_mem gray_img2_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);

	/////////////mean_kerel
	cl_mem mean1_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem mean2_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);


	/////////////zncc_kerel
	// Create memory buffers for disparity1, disparity2
	cl_mem disp1_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem disp2_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);



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
	////mean buffers 
	//none at the moment since gray is already in device memory

	////ZNCC buffers 
	//none at the moment since gray is already in device memory
	



	/////////// Create a program from the kernel source
	cl_program program = clCreateProgramWithSource(context, 1, (const char **)&kernel_string, NULL, &clStatus);

	/////////// Build the program
	clStatus = clBuildProgram(program, 1, device_list, NULL, NULL, NULL);
	if (clStatus != 0)
	{
		return clStatus;
	}

	/////////// Create the OpenCL kernel for function gray img1 and img2
	cl_kernel gray1_kernel = clCreateKernel(program, "gray", &clStatus);
	cl_kernel gray2_kernel = clCreateKernel(program, "gray", &clStatus);
	
	/////////// Create the OpenCL kernel for function mean
	cl_kernel mean_kernel = clCreateKernel(program, "mean", &clStatus);

	/////////// Create the OpenCL kernel for function zncc
	cl_kernel zncc_kernel = clCreateKernel(program, "zncc", &clStatus);

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

	///////////set the arguments of the kernel for zncc
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


	///////////////////////////////////////////////////////start timer beginning///////////////////////////////////////////////////////
	auto start = chrono::high_resolution_clock::now();/////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	///////////define local and global sizes
	size_t global_size = VECTOR_SIZE; // Process the entire lists
	size_t local_size = 8;           // Process one item at a time


	///////////put the gray kernel into the command queue
	//img1
	clStatus = clEnqueueNDRangeKernel(command_queue, gray1_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	//img2
	clStatus = clEnqueueNDRangeKernel(command_queue, gray2_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);

	///////////put the zncc kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, mean_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);

	///////////put the zncc kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, zncc_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);

	/////////Read memory from device to local host memory
	// Read gray images from the device into the local host memory
	clStatus = clEnqueueReadBuffer(command_queue, gray_img1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), gray_img1, 0, NULL, NULL);
	clStatus = clEnqueueReadBuffer(command_queue, gray_img2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), gray_img2, 0, NULL, NULL);

	//Read mean vectors from device to local memory
	clStatus = clEnqueueReadBuffer(command_queue, disp1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), disp1, 0, NULL, NULL);
	clStatus = clEnqueueReadBuffer(command_queue, disp2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), disp2, 0, NULL, NULL);

	/*
	//Read disparity vectors from device to local memory
	clStatus = clEnqueueReadBuffer(command_queue, disp1_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), disp1, 0, NULL, NULL);
	clStatus = clEnqueueReadBuffer(command_queue, disp2_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), disp2, 0, NULL, NULL);
	*/

	/////////// Clean up and wait for all the comands to complete.
	clStatus = clFlush(command_queue);
	clStatus = clFinish(command_queue);

	///////////////////////////////////////////////////////Stop timer and save image //////////////////////////////////////////////
	auto finish = chrono::high_resolution_clock::now();
	chrono::duration<double> elapsed = finish - start;
	printf("%f \n", elapsed);
	saveImage(disp1, "disp1");
	saveImage(disp2, "disp2");

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// Finally release all OpenCL allocated objects and host buffers.
	clStatus = clReleaseKernel(gray1_kernel);
	clStatus = clReleaseKernel(gray2_kernel);
	clStatus = clReleaseProgram(program);
	clStatus = clReleaseMemObject(R1_clmem);
	clStatus = clReleaseMemObject(G1_clmem);
	clStatus = clReleaseMemObject(B1_clmem);
	clStatus = clReleaseMemObject(gray_img1_clmem);
	clStatus = clReleaseCommandQueue(command_queue);
	clStatus = clReleaseContext(context);

	
	free(gray_img1);
	free(platforms);
	free(device_list);

	return 0;
}

