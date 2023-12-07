#pragma once
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/platform/gl_functions.hpp>


namespace mbgl {
namespace platform {

class MeshLayer : public mbgl::style::CustomLayerHost {
public:
    void initialize() override;
    void render(const mbgl::style::CustomLayerRenderParameters& param) override;
    void contextLost() override {};
    void deinitialize() override;

private:

    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    
    GLuint bufferHandle = 0;
    GLuint indexBufferHandle = 0;

    GLuint a_pos_loc = 0;
    GLuint proj_mat_loc = 0;

    std::vector<unsigned short> indices;
    std::vector<GLfloat> all_vertices;
    std::vector<GLfloat> indexed_vertices;

    int numVertices = 0;
    size_t numIndices = 0;
};

} //namespace platform
} //namespace mbgl
