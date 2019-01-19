#pragma once
#include <CL/cl.hpp>


typedef struct sphere {
	cl_float3 p;
	cl_float3 R;

	sphere(float x, float y, float z, float r) {
		p.x = x;
		p.y = y;
		p.z = z;
		R.x = r;
	}
} sphere;