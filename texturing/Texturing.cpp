#define __FOR_CLASS__
#include <string>
#include <vector>
#include <math.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <glload/gl_3_3.h>
#include <GL/freeglut.h>
#include "../framework/framework.h"
#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#ifdef __FOR_CLASS__
#include "sceneHandler.h"
#else
#include "scene.h"
#endif
#include <map>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
using namespace std;

// Camera Parameters
struct CameraParameters{
	glm::vec3 cam, focus;
	glm::vec3 upVector;
	glm::vec3 uVector;
	glm::vec3 vVector;
	glm::vec3 wVector;
} cameraParam;

struct ViewParameters{
	float Near, Far;
	float fov;
	float aspect;
} viewParam;
struct Scene *scene=NULL;

// create a Ray
struct Ray{

	glm::vec3 ray_o;
	glm::vec3 ray_d;
	float tMin;
	float tMax;
}ray_r;

//bounding box
struct Bound 
{
	glm::vec3 minBound;
	glm::vec3 maxBound;
}B;

// create a Triangle
typedef struct 
{
	glm::vec3 p1;
	glm::vec3 p2;
	glm::vec3 p3;
	glm::vec2 texCoord1;
	glm::vec2 texCoord2;
	glm::vec2 texCoord3;
	unsigned int matIndex1;
	
}triangle;
int nTriangles;

// to map the textures after ray tracing
struct Ray_textures{
	float s,t;
	unsigned int MatIndex;
}ray_tex;

vector<triangle> T;
map<unsigned int, Texture> textureData;// for cpu raytracing 
vector<struct FlattenStruct> vertexStack; //vertex stack contains pair of vertex-color
vector<gpu_texture> gpuTextureData;// for gpu raytracing


// Program and Shader Identifiers

GLuint program;
// Uniform locations
GLint projMatrixLoc,viewMatrixLoc,modelMatrixLoc;
GLint diffuseColorLoc,texCountLoc;
// The sampler uniform for textured models
// we are assuming a single texture so this will
//always be texture unit 0
GLint texUnitLoc = 0;

GLuint rtProgram;
GLuint gpu_rtProgram;
bool rayTraceFlag = false;
bool gpu_rayTraceFlag = false;
GLuint rtTexIndex;

const int rtImageWidth = 1000, rtImageHeight = 1000; // This is the value set in Framework.cpp
GLint rtTexUnitLoc = 0;
GLuint rtvao;
glm::vec2 texCoord(0.0f);
GLuint texCoordUnif; 

 GLuint initializeProgram()
{
	std::vector<GLuint> shaderList;

	shaderList.push_back(Framework::LoadShader(GL_VERTEX_SHADER, "classProject.vert"));
	shaderList.push_back(Framework::LoadShader(GL_FRAGMENT_SHADER, "classProject.frag"));

	GLuint p = Framework::CreateProgram(shaderList);

	projMatrixLoc=glGetUniformLocation(p,"projMatrix");
	viewMatrixLoc=glGetUniformLocation(p,"viewMatrix");
	modelMatrixLoc=glGetUniformLocation(p,"modelMatrix");
	diffuseColorLoc= glGetUniformLocation(p,"diffuseColor");
	texCountLoc = glGetUniformLocation(p,"texCount");
	texUnitLoc = glGetUniformLocation(p,"texUnit");

	return p;
}

GLuint initializeRTprogram()
{
	std::vector<GLuint> shaderList;

	shaderList.push_back(Framework::LoadShader(GL_VERTEX_SHADER, "rayTrace.vert"));
	shaderList.push_back(Framework::LoadShader(GL_FRAGMENT_SHADER, "rayTrace.frag"));

	GLuint p = Framework::CreateProgram(shaderList);

	rtTexUnitLoc = glGetUniformLocation(p,"texUnit");
	texCoordUnif = glGetUniformLocation(p,"texCoord");
	glGenTextures(1, &rtTexIndex);
	glBindTexture(GL_TEXTURE_2D,rtTexIndex); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rtImageWidth,rtImageHeight, 0, GL_RGBA, GL_FLOAT, NULL); 

	GLuint rtvbo;
    glGenBuffers(1,&rtvbo);
	float quadVertexPositions[] = {
		 1.0f, 1.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, 1.0f, 0.0f, 1.0f,
		 1.0f, 1.0f, 0.0f, 1.0f,
	};	
	glGenVertexArrays(1, &rtvao);
	glBindVertexArray(rtvao);
    glBindBuffer( GL_ARRAY_BUFFER, rtvbo);
    glBufferData( GL_ARRAY_BUFFER,sizeof(quadVertexPositions), quadVertexPositions, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glBindVertexArray(0);

	return p;
}

GLint scene_min_loc, scene_max_loc, viewParm_loc,cameraParm_loc,cameraParm_w_loc,cameraParm_u_loc,
	viewParm_fov_loc,cameraParm_v_loc;

GLint unif_tri_p1_loc, unif_tri_p2_loc, unif_tri_p3_loc;
GLint tex_info_loc;

GLint unif_tri_loc;

GLuint gpu_rtPosTexLoc;
GLuint gpu_rtTexCoordLoc;
GLuint gpu_rtModelTexLoc;
GLuint gpu_rtTexInfoLoc;

GLuint gpu_rtTexIndex;
GLuint gpu_rtPositionTex;
GLuint gpu_rtTexCoordTex;
GLuint gpu_rtModelTex;
GLuint gpu_rtTexInfoTex;

GLubyte* tex_Index;
int texWidth, texHeight, texelX, texelY, offset;

GLuint create_and_send_buffer(void *data,unsigned int size, GLenum format)
{
	GLuint texBuferIndex;
	glGenBuffers(1, &texBuferIndex);
	glBindBuffer(GL_TEXTURE_BUFFER, texBuferIndex);
	glBufferData( GL_TEXTURE_BUFFER, size, data, GL_STATIC_DRAW);
	
	GLuint gpuRtTexIndex;
	glGenTextures(1, &gpuRtTexIndex);
	glBindTexture( GL_TEXTURE_BUFFER, gpuRtTexIndex);
	glTexBuffer(GL_TEXTURE_BUFFER, format, texBuferIndex);
    return gpuRtTexIndex;
}

GLuint gpuModelTex, gpuTexInfo;

void getGpuTexture(std::vector <GLfloat> &data, std::vector<gpu_texture> TexStack)
{
	//need to write these into a function.
	//set the texture buffer for the real textures
	glGenTextures(1, &gpuModelTex);
	GLuint textureBuffer;
	glGenBuffers(1, &textureBuffer);
	glBindBuffer(GL_TEXTURE_BUFFER, textureBuffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(GLfloat)*data.size(), &data[0], GL_STATIC_DRAW);
	glBindTexture(GL_TEXTURE_BUFFER, gpuModelTex);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, textureBuffer);

	//set the texture buffer for materialInfo
	glGenTextures(1, &gpuTexInfo);
	GLuint texInfoBuffer;
	glGenBuffers(1, &texInfoBuffer);
	glBindBuffer(GL_TEXTURE_BUFFER, texInfoBuffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(gpu_texture)*TexStack.size(), &TexStack[0], GL_STATIC_DRAW);
	glBindTexture(GL_TEXTURE_BUFFER, gpuTexInfo);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, texInfoBuffer);

}

bool gpu_GetVertices(vector<struct FlattenStruct>& vertexStack);
GLuint initializeRTprogram_GPU()
{
	std::vector<GLuint> shaderList;
	
	shaderList.push_back(Framework::LoadShader(GL_VERTEX_SHADER, "GPU.vert"));
	shaderList.push_back(Framework::LoadShader(GL_FRAGMENT_SHADER, "GPU.frag"));


	GLuint p = Framework::CreateProgram(shaderList);

	
	//Vertices
	
	unif_tri_loc =  glGetUniformLocation(p,"triangle");

	gpu_rtPosTexLoc = glGetUniformLocation(p, "positionBuffer");
	gpu_rtTexCoordLoc = glGetUniformLocation(p, "texCoordBuffer");
	gpu_rtModelTexLoc = glGetUniformLocation(p, "textureBuffer");
	gpu_rtTexInfoLoc = glGetUniformLocation(p, "texInfoBuffer");

	// Bounding Box
	scene_min_loc= glGetUniformLocation(p,"Bmin");
	scene_max_loc= glGetUniformLocation(p,"Bmax"); 

	//Camera Parameters
	viewParm_loc= glGetUniformLocation(p,"viewParam_Near"); 
	viewParm_fov_loc= glGetUniformLocation(p,"viewParam_fov");
    cameraParm_loc= glGetUniformLocation(p,"cameraParam_cam");
	cameraParm_w_loc= glGetUniformLocation(p,"cameraParam_w");
	cameraParm_u_loc= glGetUniformLocation(p,"cameraParam_u");
	cameraParm_v_loc= glGetUniformLocation(p,"cameraParam_v");

	
	
	B.minBound = scene->scene_min;
	B.maxBound = scene->scene_max;
		
	// for loading model and getting vertices from it 	
	bool vertexFlag = gpu_GetVertices(vertexStack);

	//store the scene's textures into a texture buffer
	size_t size;
	std::vector<GLfloat> &data = Gpu_Ray_Texture((aiScene*)(scene->scenePointer), size, gpuTextureData);

	
	int nData = vertexStack.size();
	//// making a position buffer
	std::vector<float> positionBuffer;
	//// making texture buffer
	std::vector<float> texCoordBuffer;
	

	if(vertexFlag)
	{
		cout << "GPU Ray tracing has begun:\n";
				
		for (int i = 0; i < nData; i++) {
			
			positionBuffer.push_back(vertexStack[i].position.x);
			positionBuffer.push_back(vertexStack[i].position.y);
			positionBuffer.push_back(vertexStack[i].position.z);
			positionBuffer.push_back(float(vertexStack[i].materialIndex));

			texCoordBuffer.push_back(vertexStack[i].texCoord.x);
			texCoordBuffer.push_back(vertexStack[i].texCoord.y);
			texCoordBuffer.push_back(0.0f);
			texCoordBuffer.push_back(0.0f);

		}

	
	}

	//Send vertex data to GPU
	
	//init position texture buffer
	glGenTextures(1, &gpu_rtPositionTex);
	GLuint gpu_rtPositionBuffer;
	glGenBuffers(1, &gpu_rtPositionBuffer);
	glBindBuffer(GL_TEXTURE_BUFFER, gpu_rtPositionBuffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*positionBuffer.size(), &positionBuffer[0], GL_STATIC_DRAW);
	glBindTexture(GL_TEXTURE_BUFFER, gpu_rtPositionTex);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, gpu_rtPositionBuffer);

	//init texture coordiantes texture buffer
	glGenTextures(1, &gpu_rtTexCoordTex);
	GLuint gpu_rtTexCoordBuffer;
	glGenBuffers(1, &gpu_rtTexCoordBuffer);
	glBindBuffer(GL_TEXTURE_BUFFER, gpu_rtTexCoordBuffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(float)*texCoordBuffer.size(), &texCoordBuffer[0], GL_STATIC_DRAW);
	glBindTexture(GL_TEXTURE_BUFFER, gpu_rtTexCoordTex);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, gpu_rtTexCoordBuffer);
		
	getGpuTexture(data, gpuTextureData);

	GLuint rtvbo;
	glGenBuffers(1, &rtvbo);
	float quadVertexPositions[] = {
		1.0f, 1.0f, 0.0f, 1.0f,
		1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 1.0f,
		-1.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 0.0f, 1.0f,
	};

	glGenVertexArrays(1, &rtvao);
	glBindVertexArray(rtvao);
    glBindBuffer( GL_ARRAY_BUFFER, rtvbo);
    glBufferData( GL_ARRAY_BUFFER,sizeof(quadVertexPositions), quadVertexPositions, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
	glBindVertexArray(0);

	return p;
}

int step;
float animateFlag=false;

void idle()
{
	step=(step+1)%360;
}

// Replace the model name by your model's filename
static const std::string modelPath = "./ariel/";
//static const std::string modelPath = "./";
//static const std::string modelPath = "./bulb/models/";
//static const std::string modelPath = "./house/models/";
#ifdef __NOASSIMP__
//static const std::string modelname = "Ariel.obj.txt";
//static const std::string modelname = "House.3ds.txt";
//static const std::string modelname = "model.dae.txt";
//static const std::string modelname = "bench.obj.txt";
#else
static const std::string modelname = "Ariel.obj";
//static const std::string modelname = "House.3ds";
//static const std::string modelname = "model.dae";
//static const std::string modelname = "lamp.obj";
#endif

GLenum polyMode = GL_FILL;

void initGLstates()
{
	glPointSize(2.0f);
	glLineWidth(1.0f);
	glEnable(GL_DEPTH_TEST);		
	glClearColor(0.1f, 0.05f, 0.1f, 0.0f);
	glPolygonMode(GL_FRONT_AND_BACK, polyMode);
	//glCullFace(GL_BACK);
	//if (cullFlag) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	//if (multisampleFlag) glEnable(GL_MULTISAMPLE); else glDisable(GL_MULTISAMPLE);
}

void setCameraParams(const glm::vec3 &scene_min, const glm::vec3 &scene_max)
{
	float scene_diagonal = glm::length(scene_max - scene_min);
	glm::vec3 scene_center = glm::vec3(0.5f*(scene_min + scene_max));
	// init our eye location
	cameraParam.focus = scene_center;
	cameraParam.cam = scene_center + glm::vec3(0.0f, 5.0f, scene_diagonal);// 4*scene_diagonal); for cpu
	cameraParam.upVector = glm::vec3(0.0f, 1.0f, 0.0f);
	cameraParam.wVector = glm::normalize(cameraParam.cam - cameraParam.focus); //compute w
	cameraParam.uVector = glm::normalize(glm::cross(cameraParam.upVector,cameraParam.wVector)); //compute v
	cameraParam.vVector = glm::normalize(glm::cross(cameraParam.wVector,cameraParam.uVector)); //compute v
	viewParam.Near = 0.01f*scene_diagonal;
	viewParam.Far = 10.0f*scene_diagonal;
	viewParam.fov = 53.13f;
	viewParam.aspect = 1.0f;
}
//Called after the window and OpenGL are initialized. Called exactly once, before the main loop.


void init()
{
	program = initializeProgram();
	rtProgram = initializeRTprogram();
	
	scene = new struct Scene;
	if (!initScene(modelPath,modelname,scene)) exit(0);
	setCameraParams(scene->scene_min, scene->scene_max);

	initGLstates();
}

//Called to update the display.
//You should call glutSwapBuffers after all of your rendering to display what you rendered.
//If you need continuous updates of the screen, call glutPostRedisplay() at the end of the function.

//Called whenever the window is resized. The new window size is given, in pixels.
//This is an opportunity to call glViewport or glScissor to keep up with the change in size.

void reshape (int w, int h)
{
	//float ratio;
	// Prevent a divide by zero, when window is too short
	// (you cant make a window of zero width).
	if(h == 0)
		h = 1;

	// Set the viewport to be the entire window
    glViewport(0, 0, w, h);
	viewParam.aspect = (1.0f * w) / h;
}


void fillInboxColor(float *fb,bool box)
{	
    fb[0] = (box)?1.0f:0.0f;
	fb[1] = (box)?1.0f:0.0f;
	fb[2] = (box)?1.0f:0.0f;
	fb[3] = 1.0f;
}

Ray get_Ray(float row, float col)
 {
	Ray ray;
	ray.ray_o = cameraParam.cam;//eye point
	ray.tMin = 0.0f;
	ray.tMax = 10000000000.0f;
	float H=2*viewParam.Near*tan(glm::radians(viewParam.fov/2));//=500;
	float W=H; //assume H=W
	float deltaH=H/rtImageWidth;
	float deltaW=W/rtImageHeight;

	ray.ray_d = viewParam.Near*(-cameraParam.wVector) 
						+ 0.5f*H*(-cameraParam.vVector) 
						+ 0.5f*W*(-cameraParam.uVector) 
						+ deltaW*row*cameraParam.vVector
						+ deltaH*col*cameraParam.uVector;
	
	return ray;
}

bool ray_box_intersection(Ray ray, Bound box , float tMin, float tMax)
{
 		float txmin, tymin, tzmin;
		float txmax, tymax, tzmax;   

	   if(ray.ray_d.x >= 0.0f)
	   {
			txmin = (box.minBound.x - ray.ray_o.x)/ray.ray_d.x;
			txmax = (box.maxBound.x - ray.ray_o.x)/ray.ray_d.x;
	   }
	   else
	   {
		  txmin = (box.maxBound.x - ray.ray_o.x)/ray.ray_d.x;
		  txmax = (box.minBound.x - ray.ray_o.x)/ray.ray_d.x;
	   }

	   if(ray.ray_d.y >= 0.0f)
	   {
			tymin = (box.minBound.y - ray.ray_o.y)/ray.ray_d.y;
			tymax = (box.maxBound.y - ray.ray_o.y)/ray.ray_d.y;
	   }
	   else
	   {
		   tymin = (box.maxBound.y - ray.ray_o.y)/ray.ray_d.y;
		   tymax = (box.minBound.y - ray.ray_o.y)/ray.ray_d.y;
	   }
	    if(ray.ray_d.z >= 0.0f)
	   {
			 tzmin = (box.minBound.z - ray.ray_o.z)/ray.ray_d.z;
			 tzmax = (box.maxBound.z - ray.ray_o.z)/ray.ray_d.z;
	   }
	   else
	   {
		   tzmin = (box.maxBound.z - ray.ray_o.z)/ray.ray_d.z;
		   tzmax = (box.minBound.z - ray.ray_o.z)/ray.ray_d.z;
	   }
		
	   if(txmin > tymax || tymin > txmax)
		   return false;
	   if(tymin > txmin)
		   txmin = tymin;
	   if(tymax < txmax)
		   txmax = tymax;
	   	  
	   if(txmin > tzmax || tzmin > txmax)
		   return false;
	   if(tzmin > txmin)
		   txmin = tzmin;
	   if(tzmax < txmax)
		   txmax = tzmax;
	   //test for valid intersection
    if (txmin < tMax && txmax > tMin)
	{
		if (txmin > tMin) tMin = txmin;
		if (txmax < tMax) tMax = txmax;
		return true;
	}
	else return false;
}

bool triangleIntersect2(Ray ray, vector<triangle> triangles)
{
	float alpha,beta,t;
	bool hit_tri = false; 
	
	for(unsigned i= 0; i < T.size(); i++)
	{
		glm::mat3 matA=glm::mat3(glm::vec3(triangles[i].p2-triangles[i].p1),
								 glm::vec3(triangles[i].p3-triangles[i].p1),
								 glm::vec3(-ray.ray_d));
		glm::vec3 params = glm::vec3(alpha,beta,t);
		glm::vec3 x =glm::vec3(ray.ray_o - triangles[i].p1);
		
		params = glm::inverse(matA)*(x);

		if(params.z<ray.tMin || params.z>ray.tMax)// test for t
			hit_tri= false;
			
		if(params.y < 0.0f || params.y > 1.0f)//test for beta 
			hit_tri = false;
			
		if(params.x < 0.0f || params.x > 1.0f-params.y)//test for alpha
			hit_tri = false;
			
		if(params.x > 0.0f && params.x+params.y < 1.0f && params.y > 0.0f && params.t > 0.0f )
		{
			texCoord = triangles[i].texCoord1 + params.x*(triangles[i].texCoord2 - triangles[i].texCoord1)
							+ params.y*(triangles[i].texCoord3 - triangles[i].texCoord1);
			ray_tex.s = texCoord.x;
			ray_tex.t = texCoord.y;
			ray_tex.s = ray_tex.s - floor(ray_tex.s);
			ray_tex.t = ray_tex.t - floor(ray_tex.t);
		
			//set the materialIndex for the current triangle

			ray_tex.MatIndex = triangles[i].matIndex1;
			
			return hit_tri = true;
		}
	}
		return  hit_tri;
}

bool GetVertices(vector<struct FlattenStruct>& vertexStack, std::map<unsigned, Texture>& textureData)
{	
	std::map<unsigned, Texture>::iterator iter;
	bool flatten = newflatten(scene->scenePointer, vertexStack);// get vertexStack
	
	if(flatten)
	{
		
		triangle t;
			for(unsigned i=0; i < vertexStack.size();i+=3)
			{	
				t.p1 = vertexStack[i+0].position;
				t.texCoord1 = vertexStack[i+0].texCoord;
				t.matIndex1 = vertexStack[i+0].materialIndex;
				ray_tex.MatIndex = t.matIndex1; 
				iter = textureData.find(vertexStack[i+0].materialIndex);
				if (iter == textureData.end())
					continue;
				t.p2 = vertexStack[i+1].position;
				t.texCoord2 = vertexStack[i+1].texCoord;
				
				ray_tex.s = vertexStack[i+1].texCoord.x;

				t.p3 = vertexStack[i+2].position;
				t.texCoord3 = vertexStack[i+2].texCoord;
				ray_tex.t = vertexStack[i+2].texCoord.y;
											
				T.push_back(t);
				
			}
			
		T.resize(vertexStack.size()/3);
	   cout<<"Triangles:"<< T.size()<<" ";
	   cout<<"vertices:"<<vertexStack.size()<<"\n";
	return true;
	}
	return false;
}

bool gpu_GetVertices(vector<struct FlattenStruct>& vertexStack)
{
	bool flatten = newflatten(scene->scenePointer, vertexStack);// get vertexStack

	if (flatten)
	{
		
		triangle t;
		for (unsigned i = 0; i < vertexStack.size(); i += 3)
		{
			t.p1 = vertexStack[i + 0].position;
			t.texCoord1 = vertexStack[i + 0].texCoord;
			t.matIndex1 = vertexStack[i + 0].materialIndex;
			ray_tex.MatIndex = t.matIndex1;
			
			t.p2 = vertexStack[i + 1].position;
			t.texCoord2 = vertexStack[i + 1].texCoord;

			ray_tex.s = vertexStack[i + 1].texCoord.x;

			t.p3 = vertexStack[i + 2].position;
			t.texCoord3 = vertexStack[i + 2].texCoord;
			ray_tex.t = vertexStack[i + 2].texCoord.y;

			T.push_back(t);

		}

		T.resize(vertexStack.size() / 3);
		cout << "Triangles:" << T.size() << " ";
		cout << "vertices:" << vertexStack.size() << "\n";
		return true;
	}
	return false;
}


float *rayTrace(int width, int height)
{
	float *frameBuffer = new  float [width*height *4];
	
	int index = 0;
	
	B.minBound = scene->scene_min;
	B.maxBound = scene->scene_max;

	
	bool hitIndex = false;
	bool hitScene = false;
	bool test_box = false;
	
	triangle tri;
	
	textureData = ray_trace_texture((aiScene*)(scene->scenePointer));
	bool vertexFlag = GetVertices(vertexStack,textureData);
		if(vertexFlag)
		{	cout<<"Ray tracing has begun:\n";
			for (int row=0; row < height; row++)
			{
				cout<<"*";
				for (int col=0; col < width; col++, index+=4)
				{
					ray_r = get_Ray(row+0.5f, col+0.5f);
					test_box = ray_box_intersection(ray_r, B, ray_r.tMin, ray_r.tMax);
					if(test_box)
					{	
						//fillInboxColor(frameBuffer+index,test_box);
						hitIndex = triangleIntersect2(ray_r, T);
						
						if(hitIndex)
						{
							if(ray_tex.MatIndex >=0)// *if we dont do this check texelIndex becomes a bad pointer.*
								{
									tex_Index = textureData[ray_tex.MatIndex].dataPointer;
						
									////get texture index for the intersection point
									texWidth = textureData[ray_tex.MatIndex].width;
									texHeight = textureData[ray_tex.MatIndex].height;
									texelX = int(ray_tex.s * texWidth);
									texelY = int(ray_tex.t * texHeight);
									frameBuffer[index] = tex_Index[(texelX + texelY * texWidth) * 4]/256.0f;
									frameBuffer[index+1] = tex_Index[(texelX + texelY * texWidth) * 4 + 1 ]/256.0f;
									frameBuffer[index+2] = tex_Index[(texelX + texelY * texWidth) * 4 + 2]/256.0f;
									frameBuffer[index+3] = 1.0f;
								}
						}
					}
				}
			}
		}
		printf("\nDone with ray tracing");
		return frameBuffer;
		textureData.clear();
		
	}
	
void gpu_rayTrace()
{
	// Done in fragment shader
}
// ------------------------------------------------------------
//
// Events from the Keyboard
//
//Called whenever a key on the keyboard was pressed.
//The key is given by the ''key'' parameter, which is in ASCII.
//It's often a good idea to have the escape key (ASCII value 27) call glutLeaveMainLoop() to 
//exit the program.


void keyboard(unsigned char key, int xx, int yy)
{
	switch(key) {

		case 27: 
			glutLeaveMainLoop();
			break;
		case 'a' : animateFlag = !animateFlag;
			if (animateFlag) glutIdleFunc(idle);
			else glutIdleFunc(NULL);
			break;
		case 'g': //geomType = (geomType == GL_POINTS)? GL_LINES : ((geomType == GL_LINES)? GL_TRIANGLES: GL_POINTS); break;
			polyMode = (polyMode == GL_POINT)? GL_LINE : ((polyMode == GL_LINE)? GL_FILL: GL_POINT); 
			glPolygonMode(GL_FRONT_AND_BACK, polyMode);
			break;
		case 'o': // Output model
			printf("Output begin...");
			outputScene(scene->scenePointer,modelPath+modelname);
			printf("end.\n");
			break;
		case 't': // Ray tracing
			rayTraceFlag = !rayTraceFlag;
			if(rayTraceFlag)
			{
				float *data;
				data = rayTrace(rtImageWidth,rtImageHeight);
				glBindTexture(GL_TEXTURE_2D, rtTexIndex);
				glTexSubImage2D(GL_TEXTURE_2D,0,0,0,rtImageWidth,rtImageHeight,GL_RGBA,GL_FLOAT,data);
				glBindTexture(GL_TEXTURE_2D,0);
				delete [] data;
			}
			break;
		case 'p':
			gpu_rayTraceFlag = !gpu_rayTraceFlag;
			
			if(gpu_rayTraceFlag)
				gpu_rayTrace();
			break; 
	}
}

// Frame counting and FPS computation
long timebase = 0,frame = 0;
float frameRate=0.0f;
char s[128];
void showTitle()
{
	sprintf(s,
		"FPS:%4.2f :%s: (g)disply mode %s: (a)animation %s",
		frameRate, modelname.c_str(),
		(polyMode==GL_LINE)?"line":((polyMode==GL_FILL)?"solid":"point"),
		(animateFlag)?"on":"off"
	);
	glutSetWindowTitle(s);
}

void setWindowTitle()
{
	// FPS computation and display
	frame++;
	long frametime=glutGet(GLUT_ELAPSED_TIME);
	if (frametime - timebase > 1000) {
		frameRate = frame*1000.0f/(frametime-timebase);
		showTitle();
		timebase = frametime;
		frame = 0;
	}
}

void setCamera() 
{
	glm::mat4 viewMatrix = glm::lookAt( cameraParam.cam, cameraParam.focus, cameraParam.upVector);
	glUniformMatrix4fv(viewMatrixLoc,1,false, glm::value_ptr(viewMatrix));
}

void setProjection()
{	
	glm::mat4 projMatrix = glm::perspective(viewParam.fov, viewParam.aspect, viewParam.Near, viewParam.Far); // 53.
	glUniformMatrix4fv(projMatrixLoc,1,false, glm::value_ptr(projMatrix));
}

// Unlike Projection Matrix and View Matrix, Model Matrix varies with every node.
void setModelMatrix(const glm::mat4 &m) 
{
	glUniformMatrix4fv(modelMatrixLoc, 1,false, glm::value_ptr(m));
}

void renderScene(glm::mat4 m)
{
	glUniform1i(texUnitLoc,0);
	for (unsigned int i=0; i<scene->nodes.size(); i++)
	{
		setModelMatrix(m*scene->nodes[i].m);
		for (int j=0; j<scene->nodes[i].nMeshes; j++){
			glUniform4fv(diffuseColorLoc,1,glm::value_ptr(scene->nodes[i].ms[j].diffuseColor));
			glUniform1i(texCountLoc,scene->nodes[i].ms[j].texCount);
			// bind texture
			glBindTexture(GL_TEXTURE_2D, scene->nodes[i].ms[j].texIndex);
			// bind VAO
			glBindVertexArray(scene->nodes[i].ms[j].vao);
			// draw
			glDrawElements(GL_TRIANGLES,scene->nodes[i].ms[j].numFaces*3,GL_UNSIGNED_INT,0);
		}
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D,0);
	}
}

void renderRayTracedScene()
{
	glUniform1i(rtTexUnitLoc,0);
	glBindTexture(GL_TEXTURE_2D, rtTexIndex);
	glBindVertexArray(rtvao);
	glDrawArrays(GL_TRIANGLES, 0, 2*3);
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void display()
{
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (rayTraceFlag)
	{
		glUseProgram(rtProgram);
		renderRayTracedScene();
	}
	else if (gpu_rayTraceFlag)
	//if (gpu_rayTraceFlag)
	{
		if (!gpu_rtProgram) gpu_rtProgram = initializeRTprogram_GPU();

		glUseProgram(gpu_rtProgram );

		//Linking variables// view parameters
		glUniform1fv(viewParm_loc,1,&viewParam.Near); // for 1 float (1fv)
		glUniform1fv(viewParm_fov_loc,1,&viewParam.fov);

		
		// camera parameters
		glUniform3f(cameraParm_loc,cameraParam.cam.x,cameraParam.cam.y,cameraParam.cam.z);
		glUniform3f(cameraParm_w_loc,cameraParam.wVector.x,cameraParam.wVector.y,cameraParam.wVector.z);
		glUniform3f(cameraParm_u_loc,cameraParam.uVector.x,cameraParam.uVector.y,cameraParam.uVector.z);
		glUniform3f(cameraParm_v_loc,cameraParam.vVector.x,cameraParam.vVector.y,cameraParam.vVector.z);
			
		// triangles
		glUniform3f(unif_tri_p1_loc,T[0].p1.x,T[0].p1.y,T[0].p1.z);
		glUniform3f(unif_tri_p2_loc,T[1].p2.x,T[1].p2.y,T[1].p2.z);
		glUniform3f(unif_tri_p3_loc,T[2].p2.x,T[2].p3.y,T[2].p3.z);

		//bounding box
		glUniform3fv(scene_min_loc,1, glm::value_ptr(scene->scene_min));  // for vec3 (3fv)
		glUniform3fv(scene_max_loc,1, glm::value_ptr(scene->scene_max));

		
		glActiveTexture(GL_TEXTURE0);
		glUniform1i(gpu_rtPosTexLoc, 0);
		glBindTexture(GL_TEXTURE_BUFFER, gpu_rtPositionTex);

		glActiveTexture(GL_TEXTURE1);
		glUniform1i(gpu_rtTexCoordLoc, 1);
		glBindTexture(GL_TEXTURE_BUFFER, gpu_rtTexCoordTex);

		glActiveTexture(GL_TEXTURE2);
		glUniform1i(gpu_rtModelTexLoc, 2);
		glBindTexture(GL_TEXTURE_BUFFER, gpuModelTex);
		
		glActiveTexture(GL_TEXTURE3);
		glUniform1i(gpu_rtTexInfoLoc, 3);
		glBindTexture(GL_TEXTURE_BUFFER, gpuTexInfo);
		
		
		renderRayTracedScene();
	}
	else{
		// use our shader
		glUseProgram(program);

		// set projection matrix
		setProjection();
		// set camera matrix
		setCamera();

		// keep rotating the model
		glm::mat4 modelMatrix = glm::rotate(
				glm::mat4(1.0),
				(float)step, 
				glm::vec3(0.0f, 1.0f, 0.0f));

		renderScene(modelMatrix);
	}
	glUseProgram(0);

	setWindowTitle();
	// swap buffers
	glutSwapBuffers();
	glutPostRedisplay();
}
void cleanup()
{
	sceneDeInit(scene);
}
//Called before FreeGLUT is initialized. It should return the FreeGLUT
//display mode flags that you want to use. The initial value are the standard ones
//used by the framework. You can modify it or just return you own set.
//This function can also set the width/height of the window. The initial
//value of these variables is the default, but you can change it.
unsigned int defaults(unsigned int displayMode, int &width, int &height) {return displayMode;}