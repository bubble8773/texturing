//#ifndef __SCENE_H__
//#define __SCENE_H__

//#define  __NOASSIMP__
//#define __NO_TEXTURE_SUPPORT__

#include <vector>

#include <glload/gl_3_3.h>
#include <GL/freeglut.h>

#include <map>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
//using namespace std;

struct Mesh{
	GLuint vao;
	int numFaces;
	glm::vec4 diffuseColor;
	int texCount;
	int texIndex;
};
struct Node{
	glm::mat4 m; // Model matrix
	int nMeshes;
	struct Mesh *ms;
};
struct Scene{
	static const GLint vertexLoc=0, normalLoc=1, texCoordLoc=2;// Vertex Attribute Locations
	std::vector <struct Node> nodes;
	glm::vec3 scene_min,scene_max;
	void *scenePointer;
};

struct FlattenStruct{
glm::vec3 position;
glm::vec3 normal;
glm::vec2 texCoord;
unsigned int materialIndex;
};

struct Texture{
	unsigned int width;
	unsigned int height;
	GLubyte* dataPointer;
};

struct gpu_texture
{
	float width;
	float height;
	float materialIndex;
	float offset;
};

//nodes
bool newflatten(const void *s, std::vector<struct FlattenStruct>& vertexStack);
void outputScene(const void *s,const std::string& pfile);
bool flatten(const void *s,std::vector<glm::vec4>& vertexStack);
void sceneDeInit(struct Scene *s);
bool initScene(const std::string& pathname,const std::string& filename, struct Scene *s);

// textures for cpu ray_tracing and gpu raytracing
std::map<unsigned int, Texture> ray_trace_texture(const struct aiScene* scene);
std::vector<GLfloat> Gpu_Ray_Texture(const struct aiScene* scene, size_t &size, std::vector<gpu_texture>& matStack);

//for movement
extern float steps;
const float radius = 1.5f;
extern glm::vec3 cam, lookAt;