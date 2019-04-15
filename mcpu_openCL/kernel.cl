



__kernel void gray(__global unsigned char * R, __global unsigned char * G, __global unsigned char * B, __global float * gray) 
{
//Get the index of the work-item
int index = get_global_id(0);
gray[index] = 0.2126*R[index] + 0.7152*G[index] + 0.0722*B[index];
}


__kernel void mean(__global float * gray1,__global float * gray2, __global float * mean1, __global float * mean2) 
{
//Get the index of the work-item
int index = get_global_id(0);
	
	const int HEIGHT = 504, WIDTH = 735, B = 9;

	//locate the current pixel by finding its row&col
	const int position_within_row = index % 735;
	float lmean1 = 0, lmean2 = 0;
	int counter=1;
	const int current_row = (index - position_within_row) / WIDTH;
	for (int y = current_row - (B - 1) / 2; y <= current_row + (B - 1) / 2 && y < HEIGHT; y++)
	{
		
		if (y >= 0)
		{
			//get the current pixel to be processed
			const int current_pixel = y * WIDTH + position_within_row;
			//
			for (int x = current_pixel - (B - 1) / 2; x <= current_pixel + (B - 1) / 2 && x < WIDTH*(y + 1); x++)
			{
				if (x >= y * WIDTH)
				{
				
					lmean1 = lmean1 + gray1[x];
					lmean2 = lmean2 + gray2[x];
					counter++;
				} //if x is positive		
			}// for loop of x
		} // if y is positive
	} //for loop of y
	
	mean1[index] =  lmean1/counter;
	mean2[index] = lmean2/counter;
	


}

__kernel void zncc(__global float * gray1,__global float * gray2, __global float * mean1, __global float * mean2, __global float * disp1, __global float * disp2) 
{
//Get the index of the work-item
int index = get_global_id(0);

	//define variables

	//define constants
	const int HEIGHT = 504, WIDTH = 735, MAX_DISP = 65,B = 9, DISP_DIFF = 11;

	//locate the current pixel by finding its row&col
	const int position_within_row = index % 735;
	const int current_row = (index - position_within_row) / WIDTH;
	
	//initialiaz zncc
	float zncc1 = -10, zncc2 = -10;
	for(int d = 0; d < MAX_DISP; d++)
	{
		float map1_sum1 = 0,map1_sum2 = 0,map1_sum3 = 0,map2_sum1 = 0,map2_sum2 = 0,map2_sum3 = 0;
		for (int y = current_row - (B - 1) / 2; y <= (current_row + (B - 1) / 2 )&& y < HEIGHT; y++)
		{
			
			if (y >= 0)
			{
				//get the current pixel to be processed
				const int current_pixel = y * WIDTH + position_within_row;
				//
				for (int x = current_pixel - (B - 1) / 2 ; x <= (current_pixel + (B - 1) / 2) && x < WIDTH*(y + 1); x++)
				{
					int x_d = x -d;
					if (x >= y * WIDTH && x_d  >= y * WIDTH)
					{
						//calculate the ZNCC sums
					map1_sum1 = map1_sum1 + (gray1[x] - mean1[x])*(gray2[x_d] - mean2[x_d]);
					map1_sum2 = map1_sum2 + (gray1[x] - mean1[x])*(gray1[x] - mean1[x]);
					map1_sum3 = map1_sum3 + (gray2[x_d] - mean2[x_d])*(gray2[x_d] - mean2[x_d]);
					
					} //if x is positive		
				}// for loop of x
				

			} // if y is positive
		} //for loop of y
		
		for (int y = current_row - (B - 1) / 2; y <= (current_row + (B - 1) / 2 )&& y < HEIGHT; y++)
		{
			
			if (y >= 0)
			{
				//get the current pixel to be processed
				const int current_pixel = y * WIDTH + position_within_row;
				//
				for (int x = current_pixel - (B - 1) / 2; x <= (current_pixel + (B - 1) / 2) && x < WIDTH*(y + 1); x++)
				{
					int x_d = x +d;
					if (x >= y * WIDTH && x_d  < (y+1) * WIDTH)
					{
						//calculate the ZNCC sums
					map2_sum1 = map2_sum1 + (gray2[x] - mean2[x])*(gray1[x_d] - mean1[x_d]);
					map2_sum2 = map2_sum2 + (gray2[x] - mean2[x])*(gray2[x] - mean2[x]);
					map2_sum3 = map2_sum3 + (gray1[x_d] - mean1[x_d])*(gray1[x_d] - mean1[x_d]);	
					
					} //if x is positive		
				}// for loop of x
				

			} // if y is positive
		} //for loop of y
		
		//sqrt
		map1_sum2 = sqrt(map1_sum2);
		map1_sum3 = sqrt(map1_sum3);
		map2_sum2 = sqrt(map2_sum2);
		map2_sum3 = sqrt(map2_sum3);
		//calculate zncc
		double tmp1 = map1_sum1 / (map1_sum2 * map1_sum3);
		double tmp2 = map2_sum1 / (map2_sum2 * map2_sum3);
		
		//pick disparity value in here
		if(tmp1 >= zncc1)
		{
			disp1[index] = d*255/65;
			zncc1 = tmp1;
		}
		
			if(tmp2 >= zncc2)
		{
			disp2[index] = d*255/65;
			zncc2 = tmp2;
		}
		
	}
};




/*

									
				


*/
