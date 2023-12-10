#pragma once
#include <string>
#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/util/mat4.hpp>
#include <mbgl/util/geometry.hpp>
#include <mbgl/util/geo.hpp>
#include <mbgl/util/bounding_volumes.hpp>

namespace mbgl {
namespace platform {

struct Model {
public:
    bool LoadModel(const std::string& filePath, bool flipY);
    void SetCoordinates(double lat, double lng);
    void Rotate(vec3 euler);

    void UnloadBufferData();
    void LoadAndBindBufferData();
    void Clear();

    bool IsBufferLoaded() { return is_buffer_loaded; }
public:
    Point<double> world_pos;
    mbgl::mat4 model_matrix;
    
    bool cw_orient = false;
    size_t model_index = 0;

    float boundsRadius = 0;
    vec3 boundsCenter;

    GLuint bufferHandle = 0;

    GLuint indexBufferOffset = 0;
    GLuint texCoordBufferOffset = 0;
    GLuint normalBufferOffset = 0;

    GLuint texture_handle = 0;

    int numVertices = 0;
    size_t numIndices = 0;

  private:
    std::vector<unsigned char> saved_buffer_data;
    bool is_buffer_loaded = false;  
};

} //namespace platform
} //namespace mbgl
