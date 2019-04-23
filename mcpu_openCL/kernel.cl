








//////////////////////////////////////////////////First OpenCL attempt, non-optimized code//////////////////////////////////////////
__kernel void gray(__global unsigned char * R, __global unsigned char * G, __global unsigned char * B, __global float * gray) 
{
//Get the index of the work-item
int index = get_global_id(0);
gray[index] = 0.2126*R[index] + 0.7152*G[index] + 0.0722*B[index];
}


__kernel void zncc_slow(__global float * gray1,__global float * gray2, __global float * disp1, __global float * disp2, __global float * delta_disp) 
{
	
	int index = get_global_id(0);
	
//define variables

	//define constants
	const int HEIGHT = 504, WIDTH = 735, MAX_DISP = 65, B = 9, DISP_DIFF = 11*255/65;

	//locate the current pixel by finding its row&col
	const int position_within_row = index % 735;
	const int current_row = (index - position_within_row) / WIDTH;

	//initialiaz zncc
	float zncc1 = -10, zncc2 = -10;
	int d1 = 0, d2 = 0;


	//find mean 
	float lmean1 = 0, lmean2 = 0;
		int mcounter1=1,mcounter2=1;
		for (int f = current_row - (B - 1) / 2; f <= current_row + (B - 1) / 2 && f < HEIGHT; f++)
		{
			
			if (f >= 0)
			{
				//get the current pixel to be processed
				const int current_pixel = f * WIDTH + position_within_row;
				//
				for (int g = current_pixel - (B - 1) / 2 ; g <= current_pixel+ (B - 1) / 2 && g < WIDTH*(f + 1); g++)
				{
					if (g >= f * WIDTH)
					{
					
						lmean1 = lmean1 + gray1[g];
						lmean2 = lmean2 + gray2[g];
						mcounter1++;
					} //if x is positive		
				}// for loop of x
			} // if f is positive
		} //for loop of f
		
		float mean1 =  lmean1/mcounter1;
		float mean2 = lmean2/mcounter1
		;
		
	//calculate zncc
	for (int d = 0; d < MAX_DISP; d++)
	{
		
		float map1_sum1 = 0, map1_sum2 = 0, map1_sum3 = 0, map2_sum1 = 0, map2_sum2 = 0, map2_sum3 = 0;
		for (int y = current_row - (B - 1) / 2; y <= (current_row + (B - 1) / 2) && y < HEIGHT; y++)
		{

			if (y >= 0)
			{
				//get the current pixel to be processed
				const int current_pixel = y * WIDTH + position_within_row;
				//
				for (int x = current_pixel - (B - 1) / 2; x <= (current_pixel + (B - 1) / 2) && x < WIDTH*(y + 1); x++)
				{
					int x_d = x - d;
					if (x >= y * WIDTH && x_d >= y * WIDTH)
					{
						//calculate the ZNCC sums
						map1_sum1 = map1_sum1 + (gray1[x] - mean1)*(gray2[x_d] - mean2);
						map1_sum2 = map1_sum2 + (gray1[x] - mean1)*(gray1[x] - mean1);
						map1_sum3 = map1_sum3 + (gray2[x_d] - mean2)*(gray2[x_d] - mean2);

					} //if x is positive		
				}// for loop of x


			} // if y is positive
		} //for loop of y

		for (int y = current_row - (B - 1) / 2; y <= (current_row + (B - 1) / 2) && y < HEIGHT; y++)
		{

			if (y >= 0)
			{
				//get the current pixel to be processed
				const int current_pixel = y * WIDTH + position_within_row;
				//
				for (int x = current_pixel - (B - 1) / 2; x <= (current_pixel + (B - 1) / 2) && x < WIDTH*(y + 1); x++)
				{
					int x_d = x + d;
					if (x >= y * WIDTH && x_d < (y + 1) * WIDTH)
					{
						//calculate the ZNCC sums
						map2_sum1 = map2_sum1 + (gray2[x] - mean2)*(gray1[x_d] - mean1);
						map2_sum2 = map2_sum2 + (gray2[x] - mean2)*(gray2[x] - mean2);
						map2_sum3 = map2_sum3 + (gray1[x_d] - mean1)*(gray1[x_d] - mean1);

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
		if (tmp1 >= zncc1)
		{
			disp1[index] = d * 255 / 65;
			d1 = d;
			zncc1 = tmp1;
		}

		if (tmp2 >= zncc2)
		{
			disp2[index] = d * 255 / 65;
			zncc2 = tmp2;
			d2 = d;
		}

	}
	
	//cross checking
	 
	if( abs(d1-d2) < DISP_DIFF)
	{
		delta_disp[index] = max(d1, d2) *255/65;
	}
	else
	{
		delta_disp[index] = 0;
	}

}


__kernel void occlusion_fillig_slow(__global float * delta_disp, __global float * occlusion) 
{
	int index = get_global_id(0);
	const int HEIGHT = 504, WIDTH = 735;

	//locate the current pixel by finding its row&col
	const int position_within_row = index % 735;
	const int current_row = (index - position_within_row) / WIDTH;
	float x = delta_disp[index];
	
	if(x != 0 )
	{
	occlusion[index] = delta_disp[index];
	}
	else
	{
		bool b = 0;
		for(int k = 1; b == 0 ; k++)
		{
			
			int i = k + index;
			int j = index - k;
			if(j > WIDTH * (current_row))
			{
			float y = delta_disp[j];
			if(y !=0)
			{
				occlusion[index] = y;
				b = 1;
			}				
			}
			else if( i < WIDTH * (current_row + 1))
			{
			float y = delta_disp[i];
			if(y !=0)
			{
				occlusion[index] = y;
				b = 1;
			}
			}
				
		}
	}
	
}


////////////////////////////////////////////////// Optimized code //////////////////////////////////////////

__kernel void downsample_and_gray(__global unsigned char * image, __global float * gray) 
{
//Get the index of the work-item
	int index = get_global_id(0);
	const int FACTOR = 4, GRAY_HEIGHT = 504, GRAY_WIDTH = 735, WIDTH = 2940, HEIGHT = 2016;
	//locate the current pixel by finding its row&col
	
	const int red_position_within_row = index% (WIDTH * 4);
	const int current_row = (index - red_position_within_row) / (WIDTH*4);
	if((current_row & 3) ==0 && (red_position_within_row & 15) == 0)
	{
		gray[GRAY_WIDTH*current_row/4 + red_position_within_row/16] = (unsigned char) (0.2126*image[index] + 0.7152*image[index+1] + 0.0722*image[index+2]);
	}
	
	
}

//OPT1: create separate kernel for mean calculation
__kernel void mean(__global float * gray1,__global float * gray2, __global float * mean1, __global float * mean2) 
{
//Get the index of the work-item
int index = get_global_id(0);
	
	const int HEIGHT = 504, WIDTH = 735, B = 9;

	//locate the current pixel by finding its row&col
	const int position_within_row = index % 735;
	float lmean1 = 0, lmean2 = 0;
	int mcounter=1;
	const int current_row = (index - position_within_row) / WIDTH;
	for (int f = current_row - (B - 1) / 2; f <= current_row + (B - 1) / 2 && f < HEIGHT; f++)
	{
		
		if (f >= 0)
		{
			//get the current pixel to be processed
			const int current_pixel = f * WIDTH + position_within_row;
			//
			for (int g = current_pixel - (B - 1) / 2; g <= current_pixel + (B - 1) / 2 && g < WIDTH*(f + 1); g++)
			{
				if (g >= f * WIDTH)
				{
				
					lmean1 = lmean1 + gray1[g];
					lmean2 = lmean2 + gray2[g];
					mcounter++;
				} //if x is positive		
			}// for loop of x
		} // if f is positive
	} //for loop of f
	
	mean1[index] =  lmean1/mcounter;
	mean2[index] = lmean2/mcounter;
	


}




__kernel void zncc(__global float * gray1,__global float * gray2, __global float * mean1, __global float * mean2, __global float * disp1, __global float * disp2) 
{
//Get the index of the work-item
int index = get_global_id(0);

	//define variables

	//define constants
	const int HEIGHT = 504, WIDTH = 735, MAX_DISP = 65,B = 9, DISP_DIFF = 38;

	//locate the current pixel by finding its row&col
	const int position_within_row = index % 735;
	const int current_row = (index - position_within_row) / WIDTH;
	
	//initialiaz zncc
	float zncc1 = -10, zncc2 = -10;
	int d1 = 0, d2 =0;
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
		map1_sum2 = rsqrt(map1_sum2);
		map1_sum3 = rsqrt(map1_sum3);
		map2_sum2 = rsqrt(map2_sum2);
		map2_sum3 = rsqrt(map2_sum3);
		//calculate zncc
		double tmp1 = map1_sum1 * (map1_sum2 * map1_sum3);
		double tmp2 = map2_sum1 * (map2_sum2 * map2_sum3);
		
		//pick disparity value in here
		if(tmp1 >= zncc1)
		{
			disp1[index] = d*255/65;
			d1 = d;
			zncc1 = tmp1;
		}
		
			if(tmp2 >= zncc2)
		{
			disp2[index] = d*255/65;
			zncc2 = tmp2;
			d2 = d;
		}
		
	}

	
}



__kernel void cross_checking( __global float * disp1, __global float * disp2,__global float * delta_disp) 
{
	int index = get_global_id(0);
	const int DIFF = 38;
	
	float delta  = fabs(disp1[index] - disp2[index]);
	if(delta < DIFF)
	{
		delta_disp[index] = max(disp1[index],disp2[index]);
	}
	else
	{
		delta_disp[index] = 0;
	}
	
}


__kernel void occlusion_fillig(__global float * delta_disp, __global float * occlusion) 
{
	int index = get_global_id(0);
	const int HEIGHT = 504, WIDTH = 735;

	//locate the current pixel by finding its row&col
	const int position_within_row = index % 735;
	const int current_row = (index - position_within_row) / WIDTH;
	float x = delta_disp[index];
	if(x != 0 )
	{
	occlusion[index] = delta_disp[index];
	}
	else
	{
		bool b = 0;
		for(int k = 1; b == 0 ; k++)
		{
			
			int i = k + index;
			int j = index - k;
			if(j > WIDTH * (current_row))
			{
			float y = delta_disp[j];
			if(y !=0)
			{
				occlusion[index] = y;
				b = 1;
			}				
			}
			else if( i < WIDTH * (current_row + 1))
			{
			float y = delta_disp[i];
			if(y !=0)
			{
				occlusion[index] = y;
				b = 1;
			}
			}
				
		}
	}
	
}
;


