#pragma once
#include <CL/cl.hpp>


typedef struct sphere {
	cl_float3 p;
	cl_float3 R;
	cl_float4 color;

	sphere(float x, float y, float z, float r) {
		p.x = x;
		p.y = y;
		p.z = z;
		R.x = r;
		color.x = 1;
		color.y = 1;
		color.z = 1;
		color.w = 1;
	}

	sphere(float x, float y, float z, float R, float r, float g, float b) : sphere(x, y, z, R) {
		color.x = r;
		color.y = g;
		color.z = b;
		color.w = 1;
	}
} sphere;