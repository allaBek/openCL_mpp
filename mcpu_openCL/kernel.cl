__kernel void gray(__global float * R, __global float * G, __global float * B, __global float * gray) 
{
//Get the index of the work-item
int index = get_global_id(0);
gray[index] = 0.2126*R[index] + 0.7152*G[index] + 0.0722*B[index];
}



__kernel void zncc(__global float * g, __global float * disp1, __global float * disp2) 
{
//Get the index of the work-item
int index = get_global_id(0);

g[index] =  fmod(g[index], 120);

};