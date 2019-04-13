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

#define VECTOR_SIZE 735*504*2 //width*height*number_of_images




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
	error = lodepng_decode32_file(&img2, &width, &height, fileName1);


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
			B.push_back(image[i+2]);
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
void saveImage(float* &img)
{
	unsigned w = 735, h = 504;
	const char* fileName1 = ".\\images\\out_im1.png";
	const char* fileName2 = ".\\images\\out_im2.png";
	vector<unsigned char> output_img1, output_img2;
	for (int i = 0;  i< VECTOR_SIZE; i++ )
	{
		if(i < VECTOR_SIZE / 2)
		{
		for (int k = 0; k < 3; k++)
		{
			//int m = (int) img[i];
			output_img1.push_back((unsigned char)img[i]);
		}
		output_img1.push_back(255);
		}
		else
		{
			for (int k = 0; k < 3; k++)
			{
				//int m = (int) img[i];
				output_img2.push_back((unsigned char)img[i]);
			}
			output_img2.push_back(255);

		}
	}


	int error = lodepng::encode(fileName1, output_img1, w, h);
	error = lodepng::encode(fileName2, output_img2, w, h);


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

	//create memory allocations for RGB vectors of the image, I used float for better accuracy  --This can be optimized in next phase
	float alpha = 2.0;
	float *R = (float*)malloc(sizeof(float)*VECTOR_SIZE);
	float *G = (float*)malloc(sizeof(float)*VECTOR_SIZE);
	float *B = (float*)malloc(sizeof(float)*VECTOR_SIZE);
	float *gray = (float*)malloc(sizeof(float)*VECTOR_SIZE); //result gray image
	//Copy RGB vectors of both images into a concatenated vector, that is [IMG1,IMG2]
	for (int i = 0; i < VECTOR_SIZE; i++)
	{
		//Copy first image RGB elements
		if(i < VECTOR_SIZE/2)
		{
		R[i] = (float) R_1[i];
		G[i] = (float) G_1[i];
		B[i] = (float) B_1[i];
		}
		//Copy second image RGB elements
		else
		{
			int j = i % (VECTOR_SIZE / 2);
			R[i] = (float)R_2[j];
			G[i] = (float)G_2[j];
			B[i] = (float)B_2[j];
		}
		gray[i] = 0;
	}

	////////////////////////////////////////////////////////////////////openCL specific functions/////////////////////////////////////////////////
	// Get platform and device information
	cl_platform_id * platforms = NULL;
	cl_uint     num_platforms;
	//Set up the Platform
	cl_int clStatus = clGetPlatformIDs(0, NULL, &num_platforms);
	platforms = (cl_platform_id *)
	malloc(sizeof(cl_platform_id)*num_platforms);
	clStatus = clGetPlatformIDs(num_platforms, platforms, NULL);

	//Get the devices list and choose the device you want to run on
	cl_device_id     *device_list = NULL;
	cl_uint           num_devices;

	clStatus = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);
	device_list = (cl_device_id *) malloc(sizeof(cl_device_id)*num_devices);
	clStatus = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, num_devices, device_list, NULL);

	// Create one OpenCL context for each device in the platform
	cl_context context;
	context = clCreateContext(NULL, num_devices, device_list, NULL, NULL, &clStatus);

	// Create a command queue
	cl_command_queue command_queue = clCreateCommandQueue(context, device_list[0], 0, &clStatus);

	// Create memory buffers on the device for each vector
	cl_mem R_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem G_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem B_clmem = clCreateBuffer(context, CL_MEM_READ_ONLY, VECTOR_SIZE * sizeof(float), NULL, &clStatus);
	cl_mem gray_clmem = clCreateBuffer(context, CL_MEM_READ_WRITE, VECTOR_SIZE * sizeof(float), NULL, &clStatus);

	// Copy the Buffer A and B to the device
	clStatus = clEnqueueWriteBuffer(command_queue, R_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), R, 0, NULL, NULL);
	clStatus = clEnqueueWriteBuffer(command_queue, G_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), G, 0, NULL, NULL);
	clStatus = clEnqueueWriteBuffer(command_queue, B_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), B, 0, NULL, NULL);

	// Create a program from the kernel source
	cl_program program = clCreateProgramWithSource(context, 1, (const char **)&kernel_string, NULL, &clStatus);

	// Build the program
	clStatus = clBuildProgram(program, 1, device_list, NULL, NULL, NULL);

	// Create the OpenCL kernel for function gray
	cl_kernel gray_kernel = clCreateKernel(program, "gray", &clStatus);
	
	// Create the OpenCL kernel for function zncc
	cl_kernel zncc_kernel = clCreateKernel(program, "zncc", &clStatus);

	// Set the arguments of the kernel for gray
	clStatus = clSetKernelArg(gray_kernel, 0, sizeof(cl_mem), (void *)&R_clmem);
	clStatus = clSetKernelArg(gray_kernel, 1, sizeof(cl_mem), (void *)&G_clmem);
	clStatus = clSetKernelArg(gray_kernel, 2, sizeof(cl_mem), (void *)&B_clmem);
	clStatus = clSetKernelArg(gray_kernel, 3, sizeof(cl_mem), (void *)&gray_clmem);
	//set the arguments of the kernel for zncc
	clStatus = clSetKernelArg(zncc_kernel, 0, sizeof(cl_mem), (void *)&gray_clmem);


	//start timer beginning
	auto start = chrono::high_resolution_clock::now();


	//define local and global sizes
	size_t global_size = VECTOR_SIZE; // Process the entire lists
	size_t local_size = 8;           // Process one item at a time

	
	//put the gray kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, gray_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
	//put the zncc kernel into the command queue
	clStatus = clEnqueueNDRangeKernel(command_queue, zncc_kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);

	// Read the cl memory C_clmem on device to the host variable C
	clStatus = clEnqueueReadBuffer(command_queue, gray_clmem, CL_TRUE, 0, VECTOR_SIZE * sizeof(float), gray, 0, NULL, NULL);

	// Clean up and wait for all the comands to complete.
	clStatus = clFlush(command_queue);
	clStatus = clFinish(command_queue);

	//
	auto finish = chrono::high_resolution_clock::now();
	chrono::duration<double> elapsed = finish - start;
	printf("%f", elapsed);
	saveImage(gray);
	system("pause");


	// Finally release all OpenCL allocated objects and host buffers.
	clStatus = clReleaseKernel(gray_kernel);
	clStatus = clReleaseProgram(program);
	clStatus = clReleaseMemObject(R_clmem);
	clStatus = clReleaseMemObject(G_clmem);
	clStatus = clReleaseMemObject(B_clmem);
	clStatus = clReleaseMemObject(gray_clmem);
	clStatus = clReleaseCommandQueue(command_queue);
	clStatus = clReleaseContext(context);
	
	free(R);
	free(G);
	free(B);
	free(gray);
	free(platforms);
	free(device_list);

	return 0;
}
