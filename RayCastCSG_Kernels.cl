#define SPHERECOUNT 17
#define SUM -1
#define INTERSEC -2
#define SUB -3

/* Use float4 as input and output rather than uchar4 to save unnecessary conversions */

typedef struct sphere {
	float3 p;
	float R;
	float4 color;
} sphere;

typedef struct cut {
	float2 t;
	int2 sphere;
} cut;

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
	float t1 = (-b - sqrt(delta)) / (2 * a);
	float t2 = (-b + sqrt(delta)) / (2 * a);

	return (float2)(t1, t2);
}

float dist(float3 p1, float3 p2) {
	float3 dis = p1 - p2;
	dis = dis * dis;
	return dis.x + dis.y + dis.z;
}

float calcBrithgness(sphere s, float3 point, uchar side) {
	float3 normal;
	if(side)
		normal = (point - s.p) / s.R;
	else
		normal = (s.p - point) / s.R;
	float3 bright3 = normal * (float3)(-cos(1.), 0, sin(-1.));
	return bright3.x + bright3.y + bright3.z;
}

char isCutGood(cut c) {
	return !isnan(c.t.x);
}

cut cutSum(cut c1, cut c2) {
	if (isCutGood(c1)) {
		cut c;
		if (c1.t.x > c2.t.x) {
			c.t.x = c2.t.x;
			c.sphere.x = c2.sphere.x;			
		}
		else {
			c.t.x = c1.t.x;
			c.sphere.x = c1.sphere.x;
		}
		if (c1.t.y < c2.t.y) {
			c.t.y = c2.t.y;
			c.sphere.y = c2.sphere.y;
			//printf(".  %d %d %f %f", c.sphere.x, c.sphere.y, c.t.x, c.t.y);
		}
		else {
			c.t.y = c1.t.y;
			c.sphere.y = c1.sphere.y;
			//printf("0  %d %d %f %f", c.sphere.x, c.sphere.y, c.t.x, c.t.y);
		}
		//printf("%d %d %f %f", c.sphere.x, c.sphere.y, c.t.x, c.t.y);
		return c;
	}
	else 
		return c2;
}

cut cutIntersec(cut c1, cut c2) {
	if (isCutGood(c1)) {
		if (isCutGood(c2)) {
			cut c;
			if (c1.t.x < c2.t.x) {
				c.t.x = c2.t.x;
				c.sphere.x = c2.sphere.x;
			}
			else {
				c.t.x = c1.t.x;
				c.sphere.x = c1.sphere.x;
			}
			if (c1.t.y > c2.t.y) {
				c.t.y = c2.t.y;
				c.sphere.y = c2.sphere.y;				
			}
			else {
				c.t.y = c1.t.y;
				c.sphere.y = c1.sphere.y;
			}
			return c;
		}
		else
			return c2;
	}
	else
		return c1;
}

cut cutSub(cut c1, cut c2) {
	if (isCutGood(c1) && isCutGood(c2)) {
		cut c;
		if (c1.t.x < c2.t.x) {
			c.t.x = c1.t.x;
			c.sphere.x = c1.sphere.x;
			if (c1.t.y < c2.t.x) {
				c.t.y = c1.t.y;
				c.sphere.y = c1.sphere.y;
			}
			else {
				c.t.y = c2.t.x;
				c.sphere.y = c2.sphere.x;
			}
		}
		else if (c2.t.y > c1.t.y) {
			c.t = (float2)(-1., -1.);
		}
		else {
			c.t.y = c1.t.y;
			c.sphere.y = c1.sphere.y;
			if (c2.t.y < c1.t.x) {
				c.t.x = c1.t.x;
				c.sphere.x = c1.sphere.x;
			}
			else {
				c.t.x = c2.t.y;
				c.sphere.x = c2.sphere.y;
			}
		}

		return c;
	}
	else
		return c1;
}




__kernel void noise_uniform(__global uchar4* outputImage, int3 pos,
	__global sphere* spheres, __global int* onpGlobal) {
	int2 offset = (int2)(get_global_size(0), get_global_size(1));
	int2 gid = (int2)(get_global_id(0), get_global_id(1));
	int2 lid = (int2)(get_local_id(0), get_local_id(1));
	int inx = gid.x + gid.y * offset.x;

	float3 rayStart = (float3)(pos.x, pos.y, pos.z);
	float3 rayEnd = rayStart + (float3)(gid.x - (offset.x / 2), gid.y - (offset.y / 2), 1000);
	float3 d = rayEnd - rayStart;

	outputImage[inx] = (uchar4)(10, 10, 10, 255);

	int onp[(SPHERECOUNT << 1) - 1];
	int onpCount = (SPHERECOUNT * 2) - 1;
	for (int i = 0; i < onpCount; i++)
		onp[i] = onpGlobal[i];
	cut cuts[SPHERECOUNT];
	int onpStack[SPHERECOUNT];

	for (int i = 0; i < SPHERECOUNT; i++) {
		cuts[i].t = cutWithSphere(rayStart, rayEnd, spheres[i], d);
		cuts[i].sphere = (int2)(i, (-i - 1));		
	}

	int stackPtr = 0;
	for (int i = 0; i < onpCount; i++) {
		if (onp[i] >= 0) {
			onpStack[stackPtr] = onp[i];
			stackPtr++;
		}
		else {
			if (onp[i] == SUM) {
				cuts[onpStack[stackPtr - 2]] =
					cutSum(cuts[onpStack[stackPtr - 2]], cuts[onpStack[stackPtr - 1]]);
			}
			else if (onp[i] == INTERSEC) {
				cuts[onpStack[stackPtr - 2]] =
					cutIntersec(cuts[onpStack[stackPtr - 2]], cuts[onpStack[stackPtr - 1]]);
			}
			else if (onp[i] == SUB) {
				cuts[onpStack[stackPtr - 2]] =
					cutSub(cuts[onpStack[stackPtr - 2]], cuts[onpStack[stackPtr - 1]]);
			}
			stackPtr--;
			barrier(CLK_LOCAL_MEM_FENCE);
		}
	}

	float t = cuts[onpStack[0]].t.x;
	if (t > 0.000001) {

		int sphinx = cuts[onpStack[0]].sphere.x;
		uchar side = 1;
		if (sphinx < 0) {
			side = 0;
			sphinx = -(sphinx + 1);
		}

		float bright = calcBrithgness(spheres[sphinx], rayStart + t * d, side);
		
		outputImage[inx] = convert_uchar4_sat(
			bright * 255 * spheres[sphinx].color);
	}
}