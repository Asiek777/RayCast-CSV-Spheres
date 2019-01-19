
/* Use float4 as input and output rather than uchar4 to save unnecessary conversions */

typedef struct sphere {
	float3 p;
	float R;
} sphere;

float2 cutWithSphere(float3 rayStart, float3 rayEnd, sphere S, float3 d) {

	float a = d.x * d.x + d.y * d.y + d.z * d.z;
	float b = 2 * d.x * (rayStart.x - S.p.x)
		+ 2 * d.y * (rayStart.y - S.p.y)
		+ 2 * d.z * (rayStart.z - S.p.z);
	float c = S.p.x * S.p.x + S.p.y * S.p.y + S.p.z * S.p.z
		+ rayStart.x * rayStart.x + rayStart.y * rayStart.y + rayStart.z * rayStart.z
		- 2 * (S.p.x * rayStart.x + S.p.y * rayStart.y + S.p.z * rayStart.z)
		- S.R * S.R;
	float delta = b * b - 4 * a * c;
	if (delta <= 0)
		return (float2)(-1., -1.);
	float t1 = (-b - sqrt(delta)) / (2 * a);
	float t2 = (-b + sqrt(delta)) / (2 * a);
	//if (delta <= 0)
	// printf("%f", delta);
	return (float2)(t1, t2);
}


__kernel void noise_uniform(__global uchar4* outputImage, int3 pos, int AA_LEVEL,
	__global sphere* spheres, int sphereCount, __local float2* cuts)
{
	int2 offset = (int2)(get_global_size(0), get_global_size(1));
	int2 gid = (int2)(get_global_id(0), get_global_id(1));
	int2 lid = (int2)(get_local_id(0), get_local_id(1));
	int inx = gid.x + gid.y * offset.x;

	float3 rayStart = (float3)(pos.x, pos.y, pos.z);
	float3 rayEnd = rayStart  + (float3)(gid.x - (offset.x / 2), gid.y - (offset.y / 2), 1000);//(float3)((x - pos.x), (y - pos.y), 1000.);
	float3 d = rayEnd - rayStart;

	outputImage[inx] = (uchar4)(255, 0, 255, 255);
	//printf("%d %d %d", sizeof(sphere), sizeof(float3), sizeof(float));

	for (int i = 0; i < sphereCount; i++) {
		cuts[i] = cutWithSphere(rayStart, rayEnd, spheres[i], d);
		float t = cuts[i].x;
		if (t > 0) {
			float3 point = rayStart + t * d;
			float3 normal = (point - spheres[i].p) / spheres[i].R;
			float3 bright3 = normal * (float3)(cos(1.), 0, sin(-1.));
			float bright = bright3.x + bright3.y + bright3.z;
			outputImage[inx] = convert_uchar4_sat(bright * (float4)(255, 255, 255, 255));
		}
	}
	//for (int i = 0; i < sphereCount; i++) {
	//	float t = cuts[i].x;
	//	if (t > 0) {
	//		float3 point = rayStart + t * d;
	//		float3 normal = (point - spheres[i].p) / spheres[i].R;
	//		float3 bright3 = normal * (float3)(cos(1.), 0, sin(-1.));
	//		float bright = bright3.x + bright3.y + bright3.z;
	//		outputImage[inx] = convert_uchar4_sat(bright * (float4)(255, 255, 255, 255));
	//	}
	//}
}

	
	
	//printf("%d %d: %f %f %f %f", x, y, a, b, c, delta);
	



	

	 






	

	




	

	

	
	
