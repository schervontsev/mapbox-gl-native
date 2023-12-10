#pragma once
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/platform/gl_functions.hpp>

#include "model.hpp"


namespace mbgl {
namespace platform {

class MeshLayer : public mbgl::style::CustomLayerHost {
public:
    static void make_shadow_matrix(mat4& result, vec3 light_point);

    void initialize() override;
    void render(const mbgl::style::CustomLayerRenderParameters& param) override;
    void contextLost() override {};
    void deinitialize() override;

    void LoadModels(const char* file_path);
    void RenderModel(Model* model, const mbgl::style::CustomLayerRenderParameters& param);

private:
    std::vector<std::unique_ptr<Model>> models;

    GLuint program = 0;
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
   
    GLuint proj_mat_loc = 0;
    GLuint vertex_pos_loc = 0;
    GLuint light_pos_loc = 0;
    
    GLuint texCoord_loc = 0;
    GLuint image_loc = 0;
    GLuint normal_loc = 0;

    GLuint shadowProgram = 0;
    GLuint shadowVertexShader = 0;
    GLuint shadowFragmentShader = 0;

    GLuint shadow_vertex_pos_loc = 0;
    GLuint shadow_mat_loc = 0;
};

} //namespace platform
} //namespace mbgl
