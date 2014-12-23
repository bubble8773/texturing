#pragma warning(push)
#pragma warning(disable: 4244) // possible loss of data
#include <xutility>
#pragma warning(pop)
using namespace std;
#include "sceneHandler.h"
#ifndef __NOTEXTURE_SUPPORT__ 
// include DevIL for image loading
#include <IL\il.h>
#endif

#ifdef __NOASSIMP__
#define aiTextureType_DIFFUSE 1
#define AI_SUCCESS true
typedef bool aiReturn;

struct aiString{
	std::string data;
};

struct aiMaterial{
	unsigned int mNumMaterials;
	unsigned int nDiffuseTextures;
	unsigned int texIndex;
	aiString *texturePath;
	aiReturn GetTexture(unsigned int texType, unsigned int index, aiString *path){
		if (index < texIndex) {
			*path = texturePath[index]; 
			return true; 
		}
		else return false;
	}
	glm::vec4 diffuse, specular, ambient, emission;
	float shininess;
};

struct aiFace{
	glm::ivec3 mIndices;
};

struct aiMesh{
	unsigned int mNumVertices;
	unsigned int posnFlag;
	bool HasPositions()const {return (posnFlag==1);}
	unsigned int normalFlag;
	bool HasNormals()const {return (normalFlag==1);}
	unsigned int texCoordFlag;
	bool HasTextureCoords(unsigned int i)const {return (i<texCoordFlag);}
	glm::vec3 *mVertices;
	glm::vec3 *mNormals;
	glm::vec2 **mTextureCoords;
	unsigned int mNumFaces;
	struct aiFace* mFaces;
	unsigned int mMaterialIndex;
};

struct aiNode{
	glm::mat4 mTransformation;
	unsigned int mNumMeshes;
	unsigned int *mMeshes;
	unsigned int mNumChildren;
	struct aiNode **mChildren;
};

struct aiScene{
	unsigned int mNumMaterials;
	struct aiMaterial **mMaterials;
	unsigned int mNumMeshes;
	struct aiMesh **mMeshes;
	struct aiNode *mRootNode;
};

#else
// assimp include files. These three are usually needed.
#include <assimp.hpp>	
#include <aiPostProcess.h>
#include <aiScene.h>
#endif
#include <math.h>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <iterator>

static std::string modelPath;

// Information to render each assimp node
struct MyMesh{

	GLuint vao;
	GLuint texIndex;
	glm::vec4 diffuseColor;
	int texCount;
	int numFaces;
};

// This is for a shader uniform block

struct MyMaterial{
	glm::vec4 diffuse;
	glm::vec4 ambient;
	glm::vec4 specular;
	glm::vec4 emissive;
	float shininess;
	int texCount;
};

vector<struct MyMesh> myMeshes;

// For push and pop matrix

vector<glm::mat4> matrixStack;

#ifdef __NOASSIMP__

struct Importer{
void input_node (struct aiNode* nd, FILE *fl)
{
	fscanf(fl,"%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f //Tranformation matrix\n",
		&(nd->mTransformation[0].x),&(nd->mTransformation[0].y),&(nd->mTransformation[0].z),&(nd->mTransformation[0].w),
		&(nd->mTransformation[1].x),&(nd->mTransformation[1].y),&(nd->mTransformation[1].z),&(nd->mTransformation[1].w),
		&(nd->mTransformation[2].x),&(nd->mTransformation[2].y),&(nd->mTransformation[2].z),&(nd->mTransformation[2].w),
		&(nd->mTransformation[3].x),&(nd->mTransformation[3].y),&(nd->mTransformation[3].z),&(nd->mTransformation[3].w)
	);

	fscanf(fl,"%d // Number of meshes\n",&(nd->mNumMeshes));
	nd->mMeshes = new unsigned int[nd->mNumMeshes];
	for (unsigned int n = 0; n < nd->mNumMeshes; ++n) {
		fscanf(fl,"%d // Mesh id\n",&(nd->mMeshes[n]));
	}

	fscanf(fl,"%d // Number of children\n",&(nd->mNumChildren));
	nd->mChildren = new struct aiNode * [nd->mNumChildren];
	for (unsigned int n = 0; n < nd->mNumChildren; ++n){
		nd->mChildren[n] = new struct aiNode;
		input_node(nd->mChildren[n],fl);
	}
}

struct aiScene * ReadFile(const std::string& pfile, int flag)
{
	FILE *fl = fopen((pfile).c_str(), "rt");
	if (fl == NULL) 
		return NULL;

	struct aiScene *sc = new (struct aiScene);

	fscanf(fl,"%d // Number of Materials\n",&(sc->mNumMaterials));
	sc->mMaterials = new struct aiMaterial * [sc->mNumMaterials];
	// For each material
	for (unsigned int m=0; m < sc->mNumMaterials; ++m)
	{
		char path[128];	// filename
		// create material 
		sc->mMaterials[m] = new struct aiMaterial;
		struct aiMaterial *mtl = sc->mMaterials[m];
		fscanf(fl,"%d // number of diffuse textures\n", &(mtl->texIndex));
		if (mtl->texIndex){
			mtl->texturePath = new aiString [mtl->texIndex] ;
			for(unsigned int i=0; i< mtl->texIndex; i++){
				//fill map with textures, OpenGL image ids set to 0
				fscanf(fl,"%s // Diffuse texture \n",path); 
				mtl->texturePath[i].data.assign(path);
			}
		}
		fscanf(fl, "%f %f %f %f // diffuse reflectance.\n", &(mtl->diffuse.r), &(mtl->diffuse.g), &(mtl->diffuse.b), &(mtl->diffuse.a));
		fscanf(fl, "%f %f %f %f // ambient reflectance.\n", &(mtl->ambient.r), &(mtl->ambient.g), &(mtl->ambient.b), &(mtl->ambient.a));
		fscanf(fl, "%f %f %f %f // specular reflectance.\n", &(mtl->specular.r), &(mtl->specular.g), &(mtl->specular.b), &(mtl->specular.a));
		fscanf(fl, "%f %f %f %f // emission coeff.\n", &(mtl->emission.r), &(mtl->emission.g), &(mtl->emission.b), &(mtl->emission.a));
		fscanf(fl, "%f // Shininess.\n",&(mtl->shininess));
	}

	fscanf(fl,"%d // number of meshes.\n", &(sc->mNumMeshes));
	sc->mMeshes = new struct aiMesh * [sc->mNumMeshes];
	// For each mesh
	for (unsigned int n = 0; n < sc->mNumMeshes; ++n)
	{
		sc->mMeshes[n] = new struct aiMesh;
		struct aiMesh* mesh = sc->mMeshes[n];

		fscanf(fl,"%d // Has Position\n",&(mesh->posnFlag));
		fscanf(fl,"%d // Has Normal\n",&(mesh->normalFlag));
		fscanf(fl,"%d // Has TexCoord\n",&(mesh->texCoordFlag));
		fscanf(fl, "%d // Number of vertices.\n", &(mesh->mNumVertices));
		if (mesh->HasPositions()) mesh->mVertices = new glm::vec3[mesh->mNumVertices];
		if (mesh->HasNormals()) mesh->mNormals = new glm::vec3[mesh->mNumVertices];
		if (mesh->HasTextureCoords(0)) mesh->mTextureCoords = new glm::vec2 * [mesh->texCoordFlag];
		int k=0; 
		while (mesh->HasTextureCoords(k)) {
			mesh->mTextureCoords[k] = new glm::vec2 [mesh->mNumVertices]; 
			k++; 
		}
		for (unsigned int i=0; i<mesh->mNumVertices;i++)
		{
			if (mesh->HasPositions()) 
				fscanf(fl,"%f %f %f ", &(mesh->mVertices[i].x), &(mesh->mVertices[i].y), &(mesh->mVertices[i].z));
			if (mesh->HasNormals()) 
				fscanf(fl,"%f %f %f ", &(mesh->mNormals[i].x), &(mesh->mNormals[i].y), &(mesh->mNormals[i].z));
			int k=0;
			while (mesh->HasTextureCoords(k)) {
				fscanf(fl,"%f %f ", &(mesh->mTextureCoords[k][i].x), &(mesh->mTextureCoords[k][i].y));
				k++;
			}
			fscanf(fl,"\n");
		}
	
		fscanf(fl,"%d // Number of faces.\n", &(mesh->mNumFaces));
		mesh->mFaces = new struct aiFace[mesh->mNumFaces];
		for (unsigned int t = 0; t < mesh->mNumFaces; ++t) {
			const struct aiFace* face = &mesh->mFaces[t];
			fscanf(fl, "%d %d %d\n",&(face->mIndices[0]),&(face->mIndices[1]),&(face->mIndices[2]));
		}

		fscanf(fl,"%d // Material index\n",&(mesh->mMaterialIndex));
	}
	//fscanf(fl,"//-------------Model Hierarchy----------------\n");
	sc->mRootNode = new struct aiNode;
	input_node(sc->mRootNode,fl);
	fclose(fl);
	return sc;
}

char * GetErrorString(){ return "Error in nonAssimp Parser.";}
}importer;
#else
// Create an instance of the Importer class
Assimp::Importer importer;
#endif


// images / texture
// map image filenames to textureIds
// pointer to texture Array
map<std::string, GLuint> textureIdMap;	

void get_bounding_box_for_node (const aiScene* scene, const struct aiNode* nd, glm::vec3 &min, glm::vec3 &max, glm::mat4 &modelMatrix)
{
#ifdef __NOASSIMP__
	glm::mat4 m = nd->mTransformation;
#else
	glm::mat4 m = glm::transpose(
			glm::mat4(
				glm::vec4(nd->mTransformation.a1,nd->mTransformation.a2,nd->mTransformation.a3,nd->mTransformation.a4),
				glm::vec4(nd->mTransformation.b1,nd->mTransformation.b2,nd->mTransformation.b3,nd->mTransformation.b4),
				glm::vec4(nd->mTransformation.c1,nd->mTransformation.c2,nd->mTransformation.c3,nd->mTransformation.c4),
				glm::vec4(nd->mTransformation.d1,nd->mTransformation.d2,nd->mTransformation.d3,nd->mTransformation.d4)
			)
		);
#endif
	matrixStack.push_back(modelMatrix);
	modelMatrix = modelMatrix*m;

	unsigned int n = 0, t;

	for (; n < nd->mNumMeshes; ++n) {
		const struct aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];
		for (t = 0; t < mesh->mNumVertices; ++t) {

			glm::vec4 tmp4=modelMatrix*glm::vec4(mesh->mVertices[t].x,mesh->mVertices[t].y,mesh->mVertices[t].z,1.0); 
			glm::vec3 tmp = glm::vec3(tmp4.x,tmp4.y,tmp4.z);
			min = glm::min(min,tmp);
			max = glm::max(max,tmp);
		}
	}

	for (n = 0; n < nd->mNumChildren; ++n) {
		get_bounding_box_for_node(scene,nd->mChildren[n],min,max, modelMatrix);
	}
	modelMatrix = matrixStack.back(); matrixStack.pop_back();
}

void get_bounding_box (const aiScene* scene, glm::vec3 &min, glm::vec3 &max)
{
	min = glm::vec3(1e10f); 
	max = glm::vec3(-1e10f);
	glm::mat4 modelMatrix = glm::mat4(1.0);
	get_bounding_box_for_node(scene, scene->mRootNode,min,max,modelMatrix);
}

const aiScene* Import3DFromFile( const std::string& pFile, glm::vec3 &scene_min, glm::vec3 &scene_max)

{	//check if file exists
	std::ifstream fin(pFile.c_str());
	if(!fin.fail()) {
		fin.close();
	}
	else{
		printf("Couldn't open file: %s\n", pFile);
		printf("%s\n", importer.GetErrorString());
		return false;
	}
#ifdef __NOASSIMP__
	#define aiProcessPreset_TargetRealtime_Quality 1
#endif
// the global Assimp scene object
	const aiScene* scene =  importer.ReadFile( pFile, aiProcessPreset_TargetRealtime_Quality);

	// If the import failed, report it
	if( !scene)
	{
		printf("%s\n", importer.GetErrorString());
		return NULL;
	}

	// Now we can access the file's contents.
	printf("Import of scene %s succeeded.\n",pFile.c_str());

	//Scene bounds
	get_bounding_box(scene, scene_min, scene_max);

	// We're done. Everything will be cleaned up by the importer destructor
	return scene;
}

bool LoadGLTextures(const aiScene* scene)
{
	/* scan scene's materials for textures */
	for (unsigned int m=0; m<scene->mNumMaterials; ++m)
	{
		int texIndex = 0;
		aiString path;	// filename

		aiReturn texFound ;
		while ((texFound = scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, texIndex, &path))== AI_SUCCESS) {
			//fill map with textures, OpenGL image ids set to 0
			textureIdMap[path.data] = 0; 
			// more textures?
			texIndex++;
			//texFound = scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, texIndex, &path);
		}
	}

	int numTextures = textureIdMap.size();

#ifdef __NO_TEXTURE_SUPPORT__
	const int texSize = 16;
	GLubyte randomTexture[texSize*texSize*4];
	for(int i=0,k=0; i< texSize; i++)
		for (int j = 0; j< texSize; j++,k++){
			randomTexture[k*4] = rand() % 255;
			randomTexture[k*4+1] = rand() % 255;
			randomTexture[k*4+2] = rand() % 255;
			randomTexture[k*4+3] = 255;
		}

	GLuint textureId;
	glGenTextures(1,&textureId);
	glBindTexture(GL_TEXTURE_2D, textureId); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16,16, 0, GL_RGBA, GL_UNSIGNED_BYTE,randomTexture); 
	glBindTexture(GL_TEXTURE_2D,0);
	/* get iterator */
	std::map<std::string, GLuint>::iterator itr = textureIdMap.begin();
	int i=0;
	for (; itr != textureIdMap.end(); ++i, ++itr)
	{
		//save IL image ID
		std::string filename = modelPath+(*itr).first;  // get filename
		(*itr).second = textureId;	  // save texture id for filename in map
	}
#else
	ILboolean success;

	/* initialization of DevIL */
	ilInit(); 

	/* create and fill array with DevIL texture ids */
	ILuint* imageIds = new ILuint[numTextures];
	ilGenImages(numTextures, imageIds); 

	/* create and fill array with GL texture ids */
	GLuint* textureIds = new GLuint[numTextures];
	glGenTextures(numTextures, textureIds); /* Texture name generation */

	/* get iterator */
	std::map<std::string, GLuint>::iterator itr = textureIdMap.begin();
	int i=0;
	for (; itr != textureIdMap.end(); ++i, ++itr)
	{
		//save IL image ID
		std::string filename = modelPath+(*itr).first;  // get filename
		(*itr).second = textureIds[i];	  // save texture id for filename in map

		ilBindImage(imageIds[i]); /* Binding of DevIL image name */
		ilEnable(IL_ORIGIN_SET);
		ilOriginFunc(IL_ORIGIN_LOWER_LEFT); 
		success = ilLoadImage((ILstring)filename.c_str());

		if (success) {
			/* Convert image to RGBA */
			ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE); 

			/* Create and load textures to OpenGL */
			glBindTexture(GL_TEXTURE_2D, textureIds[i]); 
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ilGetInteger(IL_IMAGE_WIDTH),
				ilGetInteger(IL_IMAGE_HEIGHT), 0, GL_RGBA, GL_UNSIGNED_BYTE,
				ilGetData()); 
		}
		else 
			printf("Couldn't load Image: %s\n", filename.c_str());
	}
	/* Because we have already copied image data into texture data
	we can release memory used by image. */
	ilDeleteImages(numTextures, imageIds); 

	//Cleanup
	delete [] imageIds;
	delete [] textureIds;
#endif
	//return success;
	return true;
}

// texture for cpu raytracing
std::map<unsigned int, Texture> ray_trace_texture(const struct aiScene* scene)
{
	ILboolean success;
	//init DevIL
	ilInit();
	std::map<unsigned int, Texture> tex_Data;
	size_t offset = 0;//total offset from the beginning
	size_t size = 0;//total returned size 
	const unsigned materialNum = scene->mNumMaterials; 
	
	Texture Tex_Info;

	for (unsigned int i = 0; i < materialNum; i++)
	{
		int texIndex = 0;
		aiString path;

		aiReturn texFound = scene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, texIndex, &path);
		if (texFound == AI_SUCCESS){
			std::string filename = modelPath+path.data;

			ILuint imageId;
			ilGenImages(1, &imageId);
			ilBindImage(imageId);
			ilEnable(IL_ORIGIN_SET);
			ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
			success = ilLoadImage((ILstring)filename.c_str());
			if (success) {
				//convert image to RGBAt
				ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
				unsigned int width = ilGetInteger(IL_IMAGE_WIDTH);
				unsigned int height = ilGetInteger(IL_IMAGE_HEIGHT);
				size_t offsetAcc = width * height;
				size_t sizeTemp = offsetAcc * 4;
				size +=  sizeTemp;
				GLubyte* pixmap = new GLubyte[size];
				ilCopyPixels(0, 0, 0, width, height, 1, IL_RGBA, IL_UNSIGNED_BYTE, pixmap);

				Tex_Info.dataPointer = pixmap;
				Tex_Info.width = width;
				Tex_Info.height = height;
				//fill textureDataMap
				tex_Data[i] = Tex_Info;
				
				
				GLubyte *source = new GLubyte[sizeTemp];
				ilCopyPixels(0, 0, 0, width, height, 1, IL_RGBA, IL_UNSIGNED_BYTE, source);
				if (i != 0) {
				    memcpy(&pixmap[offset * 4], source, sizeTemp);
			    }
				//offset for ith texture.
				offset += offsetAcc;
				printf("Image loaded:%s\n", filename.c_str());
			} else {
				printf("Couldn't load Image: %s\n", filename.c_str());
            }
			ilBindImage(0);
			ilDeleteImage(imageId);
		}
	}
	//return success;
	return tex_Data;
}

//gpu textures we need GLfloat for gpu raytracing 


std::vector<GLfloat> Gpu_Ray_Texture(const struct aiScene* scene, size_t &size, std::vector<gpu_texture>& texStack)
{
	ILboolean success;
	ilInit();

	std::vector<GLfloat> pixelStack;//pointer to the big texture
	size_t offset = 0;//total offset from the begining
	size = 0;//total returned size 
	int texIndex = 0;
	gpu_texture matInfo;
	for (unsigned int i = 0; i < scene->mNumMaterials; i++)
	{
		
		aiString path;

		aiReturn texFound = scene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, texIndex, &path);

		if (texFound == AI_SUCCESS){
			std::string filename = modelPath + path.data;

			ILuint imageId;
			ilGenImages(1, &imageId);
			ilBindImage(imageId);
			ilEnable(IL_ORIGIN_SET);
			ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
			success = ilLoadImage((ILstring)filename.c_str());
			if (success){
				ilConvertImage(IL_RGBA, IL_FLOAT);
				unsigned int width = ilGetInteger(IL_IMAGE_WIDTH);
				unsigned int height = ilGetInteger(IL_IMAGE_HEIGHT);
				size_t offsetAcc = width * height;

				//update materialInfo
				matInfo.width = float(width);
				matInfo.height = float(height);
				matInfo.materialIndex = float(i);
				matInfo.offset = float(offset);

				//filling the texture data
				texStack.push_back(matInfo);
				// for next image
				size_t sizeTemp = offsetAcc * 4;
				size += sizeTemp;

				GLfloat *source = new GLfloat[sizeTemp];
				ilCopyPixels(0, 0, 0, width, height, 1, IL_RGBA, IL_FLOAT, source);
				
				std::copy(source, source + sizeTemp, std::back_inserter(pixelStack));
				
				//offset for ith texture.
				offset += offsetAcc;

				printf("Image loaded:%s\n", filename.c_str());
			}
			else{
				printf("Couldn't load Image: %s\n", filename.c_str());
				continue;
			}

			ilBindImage(0);
			ilDeleteImage(imageId);
		}
	}
	return pixelStack;
}


void genVAOs(const struct aiScene *sc)
{

	struct MyMesh aMesh;
	struct MyMaterial aMat; 
	GLuint buffer;
	
	// For each mesh
	for (unsigned int n = 0; n < sc->mNumMeshes; ++n) {
		const struct aiMesh* mesh = sc->mMeshes[n];
		
		// create array with faces
		// have to convert from Assimp format to array
		unsigned int *faceArray;
		faceArray = (unsigned int *)malloc(sizeof(unsigned int) * mesh->mNumFaces * 3);
		unsigned int faceIndex = 0;

		for (unsigned int t = 0; t < mesh->mNumFaces; ++t) {
			const struct aiFace* face = &mesh->mFaces[t];

			memcpy(&faceArray[faceIndex], &face->mIndices[0], 3 * sizeof(unsigned int));
			faceIndex += 3;
		}
		aMesh.numFaces = sc->mMeshes[n]->mNumFaces;

		// generate Vertex Array for mesh
		glGenVertexArrays(1,&(aMesh.vao));
		glBindVertexArray(aMesh.vao);

		// buffer for faces
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * mesh->mNumFaces * 3, faceArray, GL_STATIC_DRAW);

		// buffer for vertex positions
		if (mesh->HasPositions()) {
			glGenBuffers(1, &buffer);
			glBindBuffer(GL_ARRAY_BUFFER, buffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)*3*mesh->mNumVertices, mesh->mVertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(Scene::vertexLoc);
			glVertexAttribPointer(Scene::vertexLoc, 3, GL_FLOAT, 0, 0, 0);
		}
	
		// buffer for vertex normals
		if (mesh->HasNormals()) {
			glGenBuffers(1, &buffer);
			glBindBuffer(GL_ARRAY_BUFFER, buffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)*3*mesh->mNumVertices, mesh->mNormals, GL_STATIC_DRAW);
			glEnableVertexAttribArray(Scene::normalLoc);
			glVertexAttribPointer(Scene::normalLoc, 3, GL_FLOAT, 0, 0, 0);
		}
		
		// buffer for vertex texture coordinates
		if (mesh->HasTextureCoords(0)) {
			float *texCoords = (float *)malloc(sizeof(float)*2*mesh->mNumVertices);
			for (unsigned int k = 0; k < mesh->mNumVertices; ++k) {
				texCoords[k*2]   = mesh->mTextureCoords[0][k].x;
				texCoords[k*2+1] = mesh->mTextureCoords[0][k].y; 
			}
			glGenBuffers(1, &buffer);
			glBindBuffer(GL_ARRAY_BUFFER, buffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)*2*mesh->mNumVertices, texCoords, GL_STATIC_DRAW);
			glEnableVertexAttribArray(Scene::texCoordLoc);
			glVertexAttribPointer(Scene::texCoordLoc, 2, GL_FLOAT, 0, 0, 0);
		}

		// unbind buffers
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
		// create material uniform buffer
		struct aiMaterial *mtl = sc->mMaterials[mesh->mMaterialIndex];
			
		aiString texPath;	//contains filename of texture
		if (AI_SUCCESS == mtl->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
				//bind texture
				unsigned int texId = textureIdMap[texPath.data];
				aMesh.texIndex = texId;
				aMat.texCount = 1;
		} else {
			aMat.texCount = 0;
		}
#ifdef __NOASSIMP__
		aMat.diffuse = mtl->diffuse;
		aMat.ambient = mtl->ambient;
		aMat.specular = mtl->specular;
		aMat.shininess = mtl->shininess;
		aMat.emissive = mtl->emission;
#else
		aiColor4D diffuse;
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
			aMat.diffuse = glm::vec4(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
		else aMat.diffuse = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);

		aiColor4D ambient;
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &ambient))
			aMat.ambient = glm::vec4(ambient.r, ambient.g, ambient.b, ambient.a);
		else aMat.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);

		aiColor4D specular;
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &specular))
			aMat.specular = glm::vec4(specular.r, specular.g, specular.b, specular.a);
		else aMat.specular = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		aiColor4D emission;
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &emission))
			aMat.emissive = glm::vec4(emission.r, emission.g, emission.b, emission.a);
		else aMat.emissive = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

		float shininess = 0.0;
		unsigned int max;
		aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS, &shininess, &max);
		aMat.shininess = shininess;
#endif
		aMesh.diffuseColor = aMat.diffuse;
		aMesh.texCount = aMat.texCount;
		myMeshes.push_back(aMesh);
	}
}

// ------------------------------------------------------------
//
// Render stuff
//


void recursive_getNodes(struct Scene *s, const struct aiNode* nd, glm::mat4 &modelMatrix)
{
	const struct aiScene *sc = (struct aiScene *)(s->scenePointer);
	//// Get node transformation matrix
	//struct aiMatrix4x4 m = nd->mTransformation;
	//// OpenGL matrices are column major
#ifdef __NOASSIMP__
	glm::mat4 m = nd->mTransformation;
#else
	glm::mat4 m = glm::transpose(
			glm::mat4(
				glm::vec4(nd->mTransformation.a1,nd->mTransformation.a2,nd->mTransformation.a3,nd->mTransformation.a4),
				glm::vec4(nd->mTransformation.b1,nd->mTransformation.b2,nd->mTransformation.b3,nd->mTransformation.b4),
				glm::vec4(nd->mTransformation.c1,nd->mTransformation.c2,nd->mTransformation.c3,nd->mTransformation.c4),
				glm::vec4(nd->mTransformation.d1,nd->mTransformation.d2,nd->mTransformation.d3,nd->mTransformation.d4)
			)
		);
#endif
	matrixStack.push_back(modelMatrix);

	modelMatrix = modelMatrix*m;
	struct Node ns;
	if (nd->mNumMeshes){
		ns.nMeshes=nd->mNumMeshes;
		ns.m = modelMatrix;
		ns.ms = new struct Mesh [nd->mNumMeshes];
		// draw all meshes assigned to this node
		for (unsigned int n=0; n < nd->mNumMeshes; ++n){
			// bind material uniform
			ns.ms[n].diffuseColor = myMeshes[nd->mMeshes[n]].diffuseColor;
			ns.ms[n].texCount = myMeshes[nd->mMeshes[n]].texCount;
			ns.ms[n].texIndex = myMeshes[nd->mMeshes[n]].texIndex;
			ns.ms[n].vao = myMeshes[nd->mMeshes[n]].vao;
			ns.ms[n].numFaces=myMeshes[nd->mMeshes[n]].numFaces;
		}
		s->nodes.push_back(ns);
	}
	// draw all children
	for (unsigned int n=0; n < nd->mNumChildren; ++n){
		recursive_getNodes(s, nd->mChildren[n],modelMatrix);
	}
	//popMatrix();
	modelMatrix = matrixStack.back(); matrixStack.pop_back();
// collect Rendering related material
}

void collectNodesForRendering(struct Scene *s)
{
	const struct aiScene *scene = (struct aiScene *)s->scenePointer;
	glm::mat4 modelMatrix(1.0f);
	recursive_getNodes(s, scene->mRootNode, modelMatrix);
}

// ------------------------------------------------------------
//
// Model loading and OpenGL setup
//

bool initScene(const std::string& pPath, const std::string& pFile, struct Scene *s)
{
	modelPath.assign(pPath);
	const aiScene* scene;
	if (scene=Import3DFromFile(pPath+pFile,s->scene_min,s->scene_max)){ 
		LoadGLTextures(scene);
		genVAOs(scene);
		s->scenePointer = (void *)scene;
		collectNodesForRendering(s);
		return true;
	}
	else
		return false;
}

void sceneDeInit(struct Scene *s)
{
	// cleaning up
	textureIdMap.clear();  
	//textureDataMap.clear();
	matrixStack.clear();

	// clear myMeshes stuff
	for (unsigned int i = 0; i < myMeshes.size(); ++i) {
			
		glDeleteVertexArrays(1,&(myMeshes[i].vao));
		glDeleteTextures(1,&(myMeshes[i].texIndex));
	}
	myMeshes.clear();
	for (unsigned int i = 0; i < s->nodes.size(); ++i) 
	{		
		delete [] s->nodes[i].ms;
	}
	s->nodes.clear();
	delete s;
}
#include <stdlib.h>
#include <time.h>

float frand(){
	static bool firstTime=true;
	if (firstTime){
		srand ( (unsigned int)time(NULL) );
		firstTime = false;
	}
	return ((rand()%10)/9.0f);
}


void flatten_and_output_node (const aiScene* scene,const struct aiNode* nd,std::vector<glm::vec4>& vertexStack, glm::mat4 &modelMatrix)
{
#ifdef __NOASSIMP__
	glm::mat4 m = nd->mTransformation;
#else
	glm::mat4 m = glm::transpose(
			glm::mat4(
				glm::vec4(nd->mTransformation.a1,nd->mTransformation.a2,nd->mTransformation.a3,nd->mTransformation.a4),
				glm::vec4(nd->mTransformation.b1,nd->mTransformation.b2,nd->mTransformation.b3,nd->mTransformation.b4),
				glm::vec4(nd->mTransformation.c1,nd->mTransformation.c2,nd->mTransformation.c3,nd->mTransformation.c4),
				glm::vec4(nd->mTransformation.d1,nd->mTransformation.d2,nd->mTransformation.d3,nd->mTransformation.d4)
			)
		);
#endif
	matrixStack.push_back(modelMatrix);
	modelMatrix = modelMatrix*m;

	unsigned int n = 0, t;

	for (; n < nd->mNumMeshes; ++n) {
		const struct aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]]; 
		struct aiMaterial *mtl = scene->mMaterials[mesh->mMaterialIndex];
		glm::vec4 diffuseColor;
#ifdef __NOASSIMP__
		diffuseColor = mtl->diffuse;
#else
		aiColor4D diffuse;
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
			diffuseColor = glm::vec4(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
		else
		{
			float r = frand();
			float g = frand();
			float b = frand();
			diffuseColor = glm::vec4(r, g, b, 1.0f);
		}
#endif
		for (t = 0; t < mesh->mNumFaces; ++t) {
			const struct aiFace* face = &mesh->mFaces[t];
			int k=face->mIndices[0];
			glm::vec4 tmp=modelMatrix*glm::vec4(mesh->mVertices[k].x,mesh->mVertices[k].y,mesh->mVertices[k].z,1.0f); 
			vertexStack.push_back(tmp); vertexStack.push_back(diffuseColor);
			k=face->mIndices[1];
			tmp=modelMatrix*glm::vec4(mesh->mVertices[k].x,mesh->mVertices[k].y,mesh->mVertices[k].z,1.0f); 
			vertexStack.push_back(tmp); vertexStack.push_back(diffuseColor);
			k=face->mIndices[2];
			tmp=modelMatrix*glm::vec4(mesh->mVertices[k].x,mesh->mVertices[k].y,mesh->mVertices[k].z,1.0f); 
			vertexStack.push_back(tmp); vertexStack.push_back(diffuseColor);
		}
	}
	for (n = 0; n < nd->mNumChildren; ++n) {
		flatten_and_output_node(scene,nd->mChildren[n],vertexStack,modelMatrix);
	}
	modelMatrix = matrixStack.back(); matrixStack.pop_back();
}
bool flatten(const void *s, std::vector<glm::vec4>& vertexStack) // For outputting a flat list of vertex color pairs. For uGrad Assignment 4.
{
	const aiScene* scene = (struct aiScene *) s;
	if (scene==NULL) return false;
	glm::mat4 modelMatrix = glm::mat4(1.0);
	flatten_and_output_node(scene,scene->mRootNode,vertexStack,modelMatrix);
	return true;
}
#ifdef __NOASSIMP__
void outputScene(const void *s, const std::string& pfile){printf("You are already using translated scene input.\n");}
#else

void output_node (const void *s, const struct aiNode* nd, FILE *fl)
{
	const aiScene* scene = (struct aiScene *)s;
	fprintf(fl,"%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f //Tranformation matrix\n",
		nd->mTransformation.a1,nd->mTransformation.b1,nd->mTransformation.c1,nd->mTransformation.d1,
		nd->mTransformation.a2,nd->mTransformation.b2,nd->mTransformation.c2,nd->mTransformation.d2,
		nd->mTransformation.a3,nd->mTransformation.b3,nd->mTransformation.c3,nd->mTransformation.d3,
		nd->mTransformation.a4,nd->mTransformation.b4,nd->mTransformation.c4,nd->mTransformation.d4
	);

	unsigned int n = 0;
	fprintf(fl,"%d // Number of meshes\n",nd->mNumMeshes);
	for (; n < nd->mNumMeshes; ++n) {
		fprintf(fl,"%d // Mesh id\n",nd->mMeshes[n]);
	}
	fprintf(fl,"%d // Number of children\n",nd->mNumChildren);
	for (n = 0; n < nd->mNumChildren; ++n)
		output_node(scene,nd->mChildren[n],fl);
}

std::vector<char *> texturePaths;
void outputScene(const void *s, const std::string& pfile)
{
	FILE *fl = fopen((pfile+".txt").c_str(), "wt");
	const struct aiScene *sc = (struct aiScene *)s;

	fprintf(fl,"%d // Number of Materials\n",sc->mNumMaterials);
	// For each material
	for (unsigned int m=0; m<sc->mNumMaterials; ++m)
	{
		aiString path;	// filename
		int texIndex = 0;
		// create material  
		struct aiMaterial *mtl = sc->mMaterials[m];
		while (mtl->GetTexture(aiTextureType_DIFFUSE, texIndex, &path)== AI_SUCCESS) {
			texIndex++;
			char *localTextureFile = (char *) malloc (strlen(path.data));
			strcpy(localTextureFile,path.data);
			texturePaths.push_back(localTextureFile);
		}
		fprintf(fl,"%d // number of diffuse textures\n",texIndex);
		for (int i=0; i<texIndex; i++){
			fprintf(fl,"%s // Diffuse texture \n",texturePaths[i]);
			char *localTextureFile = texturePaths[i];
			//free(localTextureFile);
		}
		texturePaths.clear();
		aiColor4D diffuse;
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
			fprintf(fl, "%f %f %f %f // diffuse reflectance.\n", diffuse.r, diffuse.g, diffuse.b, diffuse.a);
		else 
			fprintf(fl, "0.8 0.8 0.8 1.0 // diffuse reflectance.\n");

		aiColor4D ambient;
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &ambient))
			fprintf(fl, "%f %f %f %f // ambient reflectance.\n", ambient.r, ambient.g, ambient.b, ambient.a);
		else 
			fprintf(fl, "0.2 0.2 0.2 1.0 // ambient reflectance.\n");

		aiColor4D specular;
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &specular))
			fprintf(fl, "%f %f %f %f // specular reflectance.\n", specular.r, specular.g, specular.b, specular.a);
		else 
			fprintf(fl, "0.0 0.0 0.0 1.0 // specular reflectance.\n");

		aiColor4D emission;
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &emission))
			fprintf(fl, "%f %f %f %f // emission coeff.\n", emission.r, emission.g, emission.b, emission.a);
		else 
			fprintf(fl, "0.0 0.0 0.0 1.0 // emission coeff.\n");

		float shininess = 0.0;
		unsigned int max;
		aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS, &shininess, &max);
		fprintf(fl, "%f // Shininess.\n");
	}

	fprintf(fl,"%d // number of meshes.\n", sc->mNumMeshes);
	// For each mesh
	for (unsigned int n = 0; n < sc->mNumMeshes; ++n)
	{
		//fprintf(fl,"%d // meshid.\n", n);

		const struct aiMesh* mesh = sc->mMeshes[n];

		fprintf(fl,"%d // Has Position\n",mesh->HasPositions()?1:0);
		fprintf(fl,"%d // Has Normal\n",mesh->HasNormals()?1:0);
		int k=0; while (mesh->HasTextureCoords(k)) k++; fprintf(fl,"%d // Has TexCoord\n",k);

		fprintf(fl, "%d // Number of vertices.\n", mesh->mNumVertices);
		for (unsigned int i=0; i<mesh->mNumVertices;i++)
		{
			if (mesh->HasPositions()) {
				fprintf(fl,"%f %f %f ", 
					mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
			}
			if (mesh->HasNormals()){
				fprintf(fl,"%f %f %f ", 
					mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
			}
			int k=0;
			while (mesh->HasTextureCoords(k)) {
				fprintf(fl,"%f %f ", 
					mesh->mTextureCoords[k][i].x, mesh->mTextureCoords[k][i].y);
				k++;
			}
			fprintf(fl,"\n");
		}
	
		fprintf(fl,"%d // Number of faces.\n", mesh->mNumFaces);

		for (unsigned int t = 0; t < mesh->mNumFaces; ++t) {
			const struct aiFace* face = &mesh->mFaces[t];

			fprintf(fl, "%d %d %d\n",face->mIndices[0],face->mIndices[1],face->mIndices[2]);
		}

		fprintf(fl,"%d // Material index\n",mesh->mMaterialIndex);;
	}
	//fprintf(fl,"//-------------Model Hierarchy----------------\n");
	output_node(sc,sc->mRootNode,fl);
	fclose(fl);

}
void addVertex(unsigned int k, const struct aiMesh* mesh, std::vector<struct FlattenStruct>& vertexStack, const glm::mat4 &modelMatrix)
{
struct FlattenStruct fs;
glm::vec4 tmp;
if (mesh->HasPositions()){
tmp=modelMatrix*glm::vec4(mesh->mVertices[k].x,mesh->mVertices[k].y,mesh->mVertices[k].z,1.0f);
fs.position = glm::vec3(tmp.x,tmp.y,tmp.z);
}
else fs.position = glm::vec3(0,0,0);
if (mesh->HasNormals()){
tmp=modelMatrix*glm::vec4(mesh->mNormals[k].x,mesh->mNormals[k].y,mesh->mNormals[k].z,0.0f);
fs.normal = glm::vec3(tmp.x,tmp.y,tmp.z);
}
else fs.normal=glm::vec3(0,1,0);
if (mesh->HasTextureCoords(0))
fs.texCoord = glm::vec2(mesh->mTextureCoords[0][k].x, mesh->mTextureCoords[0][k].y);
else fs.texCoord = glm::vec2(0,0);
fs.materialIndex = mesh->mMaterialIndex;
vertexStack.push_back(fs);
}
void newflatten_and_output_node (const aiScene* scene,const struct aiNode* nd,std::vector<struct FlattenStruct>& vertexStack, glm::mat4 &modelMatrix)
{
#ifdef __NOASSIMP__
glm::mat4 m = nd->mTransformation;
#else
glm::mat4 m = glm::transpose(
glm::mat4(
glm::vec4(nd->mTransformation.a1,nd->mTransformation.a2,nd->mTransformation.a3,nd->mTransformation.a4),
glm::vec4(nd->mTransformation.b1,nd->mTransformation.b2,nd->mTransformation.b3,nd->mTransformation.b4),
glm::vec4(nd->mTransformation.c1,nd->mTransformation.c2,nd->mTransformation.c3,nd->mTransformation.c4),
glm::vec4(nd->mTransformation.d1,nd->mTransformation.d2,nd->mTransformation.d3,nd->mTransformation.d4)
)
);
#endif
matrixStack.push_back(modelMatrix);
modelMatrix = modelMatrix*m;

unsigned int n = 0, t;
//printf("\t%d\n",nd->mNumMeshes);
for (; n < nd->mNumMeshes; ++n) {
const struct aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];
//printf("\t\t%d\n",mesh->mNumFaces);
for (t = 0; t < mesh->mNumFaces; ++t) {
const struct aiFace* face = &mesh->mFaces[t];
addVertex(face->mIndices[0],mesh,vertexStack,modelMatrix);
addVertex(face->mIndices[1],mesh,vertexStack,modelMatrix);
addVertex(face->mIndices[2],mesh,vertexStack,modelMatrix);
}
}
//printf("%d\n",nd->mNumChildren);
for (n = 0; n < nd->mNumChildren; ++n) {
newflatten_and_output_node(scene,nd->mChildren[n],vertexStack,modelMatrix);
}
modelMatrix = matrixStack.back(); matrixStack.pop_back();
}
bool newflatten(const void *s, std::vector<struct FlattenStruct>& vertexStack) // For outputting a flat list of vertex color pairs. For uGrad Assignment 4.
{
const aiScene* scene = (struct aiScene *) s;
if (scene==NULL) return false;
glm::mat4 modelMatrix = glm::mat4(1.0);
newflatten_and_output_node(scene,scene->mRootNode,vertexStack,modelMatrix);
return true;
}
#endif