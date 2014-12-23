#version 330

//#extension GL_EXT_gpu_shader4:require
//#extension GL_EXT_gpu_shader4: enable
//#extension GL_EXT_texture_buffer_object: enable


out vec4 outputColor;

uniform vec3 Bmin,Bmax;
uniform float viewParam_Near;
uniform float viewParam_fov;
uniform vec3 cameraParam_w, cameraParam_v,cameraParam_u;
uniform vec3 cameraParam_cam;
//uniform vec2 texCoord;
const int im_w = 500, im_h = 500;


uniform samplerBuffer positionBuffer;
uniform samplerBuffer texCoordBuffer;
uniform samplerBuffer textureBuffer;
uniform samplerBuffer texInfoBuffer;

int  texWidth,texHeight, offset;
int texelX,texelY;


struct Ray{
	vec3 ray_o, ray_d;
	float tMin,tMax,t;
}ray_r;

struct triangle
{
	vec3 p1,p2,p3;
	vec2 texCoord1,texCoord2,texCoord3;
	unsigned int matIndex1;
};
triangle T;

struct Ray_textures{
	float s,t;
	unsigned int MatIndex;
}ray_tex;


Ray get_Ray(float row, float col);
void min_max( float ox, float dx, float xmin, float xmax,out float tMin_x,out float tMax_x);
bool ray_box_intersection(inout Ray ray,in vec3 Bmin,in vec3 Bmax);

bool triangle_Intersect( inout Ray ray, in triangle triangles);

void main()
{
	float  col = gl_FragCoord.x;
    float  row = gl_FragCoord.y;
	//int index = 0;
	bool box_hit;
    bool tri_hit;
		
	ray_r = get_Ray(row + 0.5f,col + 0.5f);
	box_hit = ray_box_intersection(ray_r,Bmin,Bmax); // pass the ray to intersection box function
	if (box_hit) //if bounding box hit
		{  
			
			tri_hit = triangle_Intersect(ray_r,  T);
			if(tri_hit)
			{
				texelX = int(ray_tex.s * texWidth);
				texelY = int(ray_tex.t * texHeight);

 				outputColor.xyz = texelFetch( textureBuffer,  offset + texelX + texelY * texWidth).xyz;
				outputColor.w = 1.0f;
				
				   				
			}
			
			
		}
}

Ray get_Ray(float row, float col)
{ 
  float H = 2*viewParam_Near*tan(radians(viewParam_fov/2)); 
  float W, DH, DW; //DH delta h and delta w
  Ray ray; 
  W = H;
  //each pixels wideth and hight, where no of row ans col =500
  DH = H/im_w;
  DW = W/im_h;

  ray.tMin=0.0f; //t=0
  ray.tMax=10000000000.0f; // huge amnount of t
   
  // ray origion and ray_dection
  ray.ray_o = cameraParam_cam;  // rays origion is the same camera position
	
  // near(-w) + H/2(-v)+w/2 (-u)+ + DH*j(column)*v+  DW*i(row)*u
  ray.ray_d= viewParam_Near*(-cameraParam_w) + ((H/2.0f)*(-cameraParam_v))+((W/2.0f)*(-cameraParam_u))+
		                                         DH*col*cameraParam_u   + DW*row*cameraParam_v; 
	return ray;
}

 //function for biunding box to compute min and max
void min_max( float ox,  float dx,float xmin, float xmax,out float tMin_x,out float tMax_x)
{
     if(dx >=0)
	{
		tMin_x= (xmin -  ox) / dx;
		tMax_x= (xmax -  ox) / dx;

	}
	   else{
		tMax_x= (xmin - ox) / dx;
		tMin_x= (xmax - ox) / dx;
	}
}

//ray bounding box intersection
bool ray_box_intersection(inout Ray ray, in vec3 Bmin, in vec3 Bmax)
{ 

	float tMin_x,tMin_y,tMin_z,tMax_x, tMax_y, tMax_z, tMin,tMax; 
	min_max(ray.ray_o.x,ray.ray_d.x,Bmin.x,Bmax.x,tMin_x,tMax_x);
	min_max(ray.ray_o.y,ray.ray_d.y,Bmin.y,Bmax.y,tMin_y,tMax_y);
	min_max(ray.ray_o.z,ray.ray_d.z,Bmin.z,Bmax.z,tMin_z,tMax_z);

	tMin = max(tMin_x,max(tMin_y,tMin_z));
	tMax = min(tMax_x,min(tMax_y,tMax_z));
	if (tMin > tMax) return false;

	if(tMin_x<ray.tMax && tMax_x>ray.tMin){
		if(tMin_x>ray.tMin) 
			ray.tMin=tMin_x; 
		if(tMax_x<ray.tMax) 
			ray.tMax=tMax_x; 
	}
	else return false; 
	ray.t = ray.tMax;
	return true;
}

bool triangle_Intersect(inout Ray ray, in triangle triangles)
{
	float alpha,beta,t;
	bool hit_tri = false; 
	bool has_tex = false;	
	int size = textureSize(positionBuffer);
	int pix_Size = textureSize(texInfoBuffer);

	for(int i= 0; i < size; i+=3)
	{
		// init the triangle vertices and textures

		triangles.p1 = texelFetch(positionBuffer, i+0).xyz;
		triangles.p2 = texelFetch(positionBuffer, i+1).xyz;
		triangles.p3 = texelFetch(positionBuffer, i+2).xyz;

		triangles.texCoord1 = texelFetch(texCoordBuffer, i+0).xy;
		triangles.texCoord2 = texelFetch(texCoordBuffer, i+1).xy;
		triangles.texCoord3 = texelFetch(texCoordBuffer, i+2).xy;

		mat3 matA = mat3(vec3(triangles.p2 - triangles.p1),
						 vec3(triangles.p3 - triangles.p1),
						 vec3(-ray.ray_d));

		vec3 params= vec3(alpha,beta,t);
		vec3 x = vec3(ray.ray_o - triangles.p1);
		
		params = inverse(matA)*(x);

		if(params.z<ray.tMin || params.z>ray.tMax)// test for t
			hit_tri= false;
			
		if(params.y < 0.0f || params.y > 1)//test for beta 
			hit_tri = false;
			
		if(params.x < 0.0f || params.x > 1-params.y)//test for alpha
			hit_tri = false;
					
		if(params.x>0 && params.x+params.y<1 && params.y>0 && params.t>0 )
		{
			ray_tex.s = triangles.texCoord1.x + params.x*(triangles.texCoord2.x - triangles.texCoord1.x)
							+ params.y*(triangles.texCoord3.x - triangles.texCoord1.x);

            ray_tex.t = triangles.texCoord1.y + params.x*(triangles.texCoord2.y - triangles.texCoord1.y)
							+ params.y*(triangles.texCoord3.y - triangles.texCoord1.y);
		
			ray_tex.s = ray_tex.s - floor(ray_tex.s);
			ray_tex.t = ray_tex.t - floor(ray_tex.t);
		
			//set the materialIndex for the current triangle

			ray_tex.MatIndex = triangles.matIndex1;

			//get materialInfo, width, height, offset
		float materialIndex = texelFetch(positionBuffer, i).w;
	
	   
		for ( int j = 0; j < pix_Size; j++ )
		{
			if ( texelFetch(texInfoBuffer, j).z == materialIndex )
			{
				has_tex = true;
				texWidth = int(texelFetch(texInfoBuffer, j).x);
				texHeight = int(texelFetch(texInfoBuffer, j).y);
				offset = int(texelFetch(texInfoBuffer, j).w);
				break;
			}
			else 
			{ continue;  }
		}
			
		return hit_tri = true;
		}
		  
	}

	return  hit_tri;
}
