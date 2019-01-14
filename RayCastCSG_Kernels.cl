
/* Use float4 as input and output rather than uchar4 to save unnecessary conversions */

__kernel void noise_uniform(__global uchar4* outputImage, int factor, int AA_LEVEL)
{
	
	int pos = get_global_id(0) + get_global_id(1) * get_global_size(0);
	int x = get_global_id(0);
	int y = get_global_id(1);
	float3 sphereCenter = (float3)(500., 500., 500.);
	float3 rayStart = (float3)(500., 500. - factor, 0.);
	float3 rayEnd = (float3)(x / AA_LEVEL, (y - factor) / AA_LEVEL, 1000.);
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
		float3 bright3 = normal * (float3)(cos(1.), 0, sin(-1.));
		float bright = bright3.x + bright3.y + bright3.z;
		outputImage[pos] = convert_uchar4_sat(bright * (float4)(255, 255, 255, 255));
	}
}



	

	 






	

	




	

	

	
	
