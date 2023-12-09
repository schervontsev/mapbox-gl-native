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
    bool LoadModel(const std::string& path);
    void SetCoordinates(double lat, double lng);
    void Rotate(vec3 euler);

public:
    LatLng latLng;
    Point<double> world_pos;
    mbgl::mat4 model_matrix;

    float boundsRadius = 0;
    vec3 boundsCenter;

    GLuint bufferHandle = 0;
    
    GLuint indexBufferOffset = 0;

    GLuint texture_handle = 0;

    int numVertices = 0;
    size_t numIndices = 0;
};

} //namespace platform
} //namespace mbgl
