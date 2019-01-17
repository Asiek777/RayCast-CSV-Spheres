
/* Use float4 as input and output rather than uchar4 to save unnecessary conversions */

typedef struct sphere {
	float3 p;
	float R;
} sphere;

float2 cutWithSphere(float3 rayStart, float3 rayEnd, sphere S) {
	float dx = rayEnd.x - rayStart.x;
	float dy = rayEnd.y - rayStart.y;
	float dz = rayEnd.z - rayStart.z;
	float a = dx * dx + dy * dy + dz * dz;
	float b = 2 * dx * (rayStart.x - S.p.x)
		+ 2 * dy * (rayStart.y - S.p.y)
		+ 2 * dz * (rayStart.z - S.p.z);
	float c = S.p.x * S.p.x + S.p.y * S.p.y + S.p.z * S.p.z
		+ rayStart.x * rayStart.x + rayStart.y * rayStart.y + rayStart.z * rayStart.z
		- 2 * (S.p.x * rayStart.x + S.p.y * rayStart.y + S.p.z * rayStart.z)
		- S.R * S.R;
	float delta = b * b - 4 * a * c;
	float t1 = (-b - sqrt(delta)) / (2 * a);
	float t2 = (-b + sqrt(delta)) / (2 * a);
	return (float2)(t1, t2);
}


__kernel void noise_uniform(__global uchar4* outputImage, int factor, int AA_LEVEL)
{
	
	int pos = get_global_id(0) + get_global_id(1) * get_global_size(0);
	int x = get_global_id(0);
	int y = get_global_id(1);

	float3 rayStart = (float3)(500., 500. - factor, 0.);
	float3 rayEnd = (float3)(x / AA_LEVEL, (y - factor) / AA_LEVEL, 1000.);
	sphere S;
	S.p = (float3)(500., 500., 500.);
	S.R = 100;
	float2 t2 = cutWithSphere(rayStart, rayEnd, S);


	float t = t2.x;
	outputImage[pos] = (uchar4)(255 ,0, 255, 255);
	//printf("%d %d: %f %f %f %f", x, y, a, b, c, delta);
	if (t>0) {
		float3 point = rayStart + t * (float3)(dx, dy, dz);
		float3 normal = (point - sphereCenter) / Radius;
		float3 bright3 = normal * (float3)(cos(1.), 0, sin(-1.));
		float bright = bright3.x + bright3.y + bright3.z;
		outputImage[pos] = convert_uchar4_sat(bright * (float4)(255, 255, 255, 255));
	}
}



	

	 






	

	




	

	

	
	
