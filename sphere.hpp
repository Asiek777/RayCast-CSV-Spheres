#pragma once
#include <CL/cl.hpp>


struct sphere {
	cl_float3 p;
	cl_float R;

	sphere(float x, float y, float z, float r) {
		p.x = x;
		p.y = y;
		p.z = z;
		R = r;
	}
};