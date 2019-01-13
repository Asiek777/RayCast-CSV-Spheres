/**********************************************************************
Copyright ©2015 Advanced Micro Devices, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

•	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
•	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************/


#define IA 16807    			// a
#define IM 2147483647 			// m
#define AM (1.0f/IM) 			// 1/m - To calculate floating point result
#define IQ 127773 
#define IR 2836
#define NTAB 4
#define NDIV (1 + (IM - 1)/ NTAB)
#define EPS 1.2e-7
#define RMAX (1.0f - EPS)
#define FACTOR 60			// Deviation factor
#define GROUP_SIZE 64




/* Generate uniform random deviation */
// Park-Miller with Bays-Durham shuffle and added safeguards
// Returns a uniform random deviate between (-FACTOR/2, FACTOR/2)
// input seed should be negative 
float ran1(int idum, __local int *iv)
{
    int j;
    int k;
    int iy = 0;
    //__local int iv[NTAB * GROUP_SIZE];
    int tid = get_local_id(0);

    for(j = NTAB; j >=0; j--)			//Load the shuffle
    {
        k = idum / IQ;
        idum = IA * (idum - k * IQ) - IR * k;

        if(idum < 0)
            idum += IM;

        if(j < NTAB)
            iv[NTAB* tid + j] = idum;
    }
    iy = iv[NTAB* tid];

    k = idum / IQ;
    idum = IA * (idum - k * IQ) - IR * k;

    if(idum < 0)
        idum += IM;

    j = iy / NDIV;
    iy = iv[NTAB * tid + j];
    return (AM * iy);	//AM *iy will be between 0.0 and 1.0
}




/* Use float4 as input and output rather than uchar4 to save unnecessary conversions */

__kernel void noise_uniform(__global uchar4* inputImage, __global uchar4* outputImage, int factor)
{
	
	int pos = get_global_id(0) + get_global_id(1) * get_global_size(0);
	int x = get_global_id(0);
	int y = get_global_id(1);
	float3 sphereCenter = (float3)(500., 500., 500.);
	float3 rayStart = (float3)(500., 500., 0.);
	float3 rayEnd = (float3)(x, y, 1000.);
	float Radius = 100.;
	//printf("%f %f %f", sphereCenter.x, sphereCenter.y, sphereCenter.z);
	float dx = rayEnd.x - rayStart.x;
	float dy = rayEnd.y - rayStart.y;
	float dz = rayEnd.z - rayStart.z;
	float a = dx * dx + dy * dy + dz * dz;
	float b = 2 * dx * (rayStart.x - sphereCenter.x) 
		+ 2 * dy * (rayStart.y - sphereCenter.y) 
		+ 2 * dz * (rayStart.z - sphereCenter.z);
	float c = sphereCenter.x * sphereCenter.x + sphereCenter.y * sphereCenter.y + sphereCenter.z * sphereCenter.z 
		+ rayStart.x * rayStart.x + rayStart.y * rayStart.y + rayStart.z * rayStart.z 
		- 2 * (sphereCenter.x * rayStart.x + sphereCenter.y * rayStart.y + sphereCenter.z * rayStart.z) 
		- Radius * Radius;
	float delta = b * b - 4 * a * c;
	float t = (-b - sqrt(b * b - 4 * a * c)) / (2 * a);
	outputImage[pos] = (uchar4)(255 ,0, 255, 255);
	//printf("%d %d: %f %f %f %f", x, y, a, b, c, delta);
	if (delta >= 0) {
		float3 point = rayStart + t * (float3)(dx, dy, dz);
		float3 normal = (point - sphereCenter) / Radius;
		float3 bright3 = normal * (float3)(0, 0, -1);
		float bright = bright3.x + bright3.y + bright3.z;
		outputImage[pos] = convert_uchar4_sat(bright * (float4)(255, 255, 255, 255));
	}
}



	

	 






	

	




	

	

	
	
