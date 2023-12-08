#pragma once
#include <string>
#include <mbgl/platform/gl_functions.hpp>
#include <mbgl/util/mat4.hpp>
#include <mbgl/util/geometry.hpp>
#include <mbgl/util/geo.hpp>

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

    GLuint bufferHandle = 0;
    GLuint indexBufferHandle = 0;

    GLuint a_pos_loc = 0;

    int numVertices = 0;
    size_t numIndices = 0;
};

} //namespace platform
} //namespace mbgl
