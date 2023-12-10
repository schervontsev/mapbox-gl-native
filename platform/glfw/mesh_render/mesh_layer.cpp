#include "mesh_layer.hpp"
#include <mbgl/util/color.hpp>
#include <mbgl/style/expression/type.hpp>
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/style/layers/fill_layer.hpp>
#include <mbgl/map/camera.hpp>
#include <mbgl/style/layers/background_layer.hpp>
#include <mbgl/gl/uniform.hpp>
#include <mbgl/util/projection.hpp>
#include <mbgl/map/transform_state.hpp>
#include <mbgl/util/mat2.hpp>
#include <mbgl/util/mat3.hpp>
#include <mbgl/util/mat4.hpp>
#include <mbgl/gl/defines.hpp>

#include <iostream>

#include "rapidjson/document.h"
#include <rapidjson/filereadstream.h>

namespace mbgl {
namespace platform {

static const GLchar* vertexShaderSource = R"MBGL_SHADER(
attribute vec3 a_pos;
attribute vec2 a_texCoord;
uniform mat4 u_matrix;
varying vec2 v_texCoord;
void main() {
    gl_Position = u_matrix * vec4(a_pos.x, a_pos.y, a_pos.z, 1.0);
    v_texCoord = a_texCoord;
}
)MBGL_SHADER";

static const GLchar* fragmentShaderSource = R"MBGL_SHADER(
uniform sampler2D u_Texture;
varying vec2 v_texCoord;

void main() {
    vec4 color = texture2D(u_Texture, v_texCoord);
    gl_FragColor = color;
}
)MBGL_SHADER";

void MeshLayer::initialize() {
    program = MBGL_CHECK_ERROR(glCreateProgram());
    vertexShader = MBGL_CHECK_ERROR(glCreateShader(GL_VERTEX_SHADER));
    fragmentShader = MBGL_CHECK_ERROR(glCreateShader(GL_FRAGMENT_SHADER));

    MBGL_CHECK_ERROR(glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr));
    MBGL_CHECK_ERROR(glCompileShader(vertexShader));
    MBGL_CHECK_ERROR(glAttachShader(program, vertexShader));
    MBGL_CHECK_ERROR(glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr));
    MBGL_CHECK_ERROR(glCompileShader(fragmentShader));
    MBGL_CHECK_ERROR(glAttachShader(program, fragmentShader));
    MBGL_CHECK_ERROR(glLinkProgram(program));

    vertex_pos_loc = MBGL_CHECK_ERROR(glGetAttribLocation(program, "a_pos"));
    proj_mat_loc = glGetUniformLocation(program, "u_matrix");

    texCoord_loc = MBGL_CHECK_ERROR(glGetAttribLocation(program, "a_texCoord"));
    image_loc = MBGL_CHECK_ERROR(glGetUniformLocation(program, "u_Texture"));

    LoadModels("platform/glfw/assets/models.json");
}

void MeshLayer::render(const mbgl::style::CustomLayerRenderParameters& param) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDepthMask(GL_TRUE);
    glDepthRangef(0.0, param.depthMin);


    auto test = glIsEnabled(GL_DEPTH_TEST);

    MBGL_CHECK_ERROR(glUseProgram(program));

    //every model in its own draw call. 
    //It's possible to do everything with one buffer, but we don't have a lot of models, especially with bounds test
    for (const auto& model : models) {
        RenderModel(model.get(), param);
    }
}

void MeshLayer::deinitialize() {
        if (program) {
            for (const auto& model : models) {
                MBGL_CHECK_ERROR(glDeleteBuffers(1, &model->bufferHandle));
            }
            MBGL_CHECK_ERROR(glDetachShader(program, vertexShader));
            MBGL_CHECK_ERROR(glDetachShader(program, fragmentShader));
            MBGL_CHECK_ERROR(glDeleteShader(vertexShader));
            MBGL_CHECK_ERROR(glDeleteShader(fragmentShader));
            MBGL_CHECK_ERROR(glDeleteProgram(program));
        }
}


void MeshLayer::LoadModels(const char* file_path) {
    FILE* file = fopen(file_path, "r");
    char buffer[65536];
    rapidjson::FileReadStream inputStream(file, buffer, sizeof(buffer));
    rapidjson::Document document;
    document.ParseStream(inputStream);

    // Access the models array
    const auto& modelsJson = document["models"];
    if (!modelsJson.IsArray()) {
        throw std::runtime_error(("Input file (" + std::string(file_path) + ") is not valid json"));
    }
    for (rapidjson::SizeType i = 0; i < modelsJson.Size(); ++i) {
        const auto& modelJson = modelsJson[i];
        models.push_back(std::make_unique<Model>());
        bool textureFlip = modelJson.HasMember("flipY") && modelJson["flipY"].GetBool();
        models.back()->cw_orient = modelJson.HasMember("cw_orient") && modelJson["cw_orient"].GetBool();
        auto glbPath = modelJson["path"].GetString();
        if (!models.back()->LoadModel(glbPath, textureFlip)) {
            throw std::runtime_error(("Can't load model (" + std::string(glbPath) + ")"));
        }
        double latitude = modelJson["latitude"].GetDouble();
        double longitude = modelJson["longitude"].GetDouble();
        models.back()->SetCoordinates(latitude, longitude);
        if (modelJson.HasMember("rotation")) {
            auto& rotationJson = modelJson["rotation"];
            vec3 rotation;
            if (rotationJson.HasMember("x")) {
                rotation[0] = rotationJson["x"].GetDouble();
            }
            if (rotationJson.HasMember("y")) {
                rotation[1] = rotationJson["y"].GetDouble();
            }
            if (rotationJson.HasMember("z")) {
                rotation[2] = rotationJson["z"].GetDouble();
            }
            models.back()->Rotate(rotation);
        }
    }
    fclose(file);
}

void MeshLayer::RenderModel(Model* model, const mbgl::style::CustomLayerRenderParameters& param) {
    if (!model) {
        assert(model);
        return;
    }

    const auto scale = std::pow(2.0, param.zoom);
    const double worldSize = mbgl::Projection::worldSize(scale);

    mbgl::mat4 world_matrix;
    mbgl::matrix::identity(world_matrix);

    mbgl::matrix::scale(world_matrix, world_matrix, worldSize, worldSize, 1.0);
    mbgl::matrix::translate(world_matrix, world_matrix, model->world_pos.x, model->world_pos.y, 0.f);
    
    mbgl::mat4 resultMatrix {param.projectionMatrix};
    mbgl::matrix::multiply(resultMatrix, resultMatrix, world_matrix);
    mbgl::matrix::multiply(resultMatrix, resultMatrix, model->model_matrix);

    //check culling (sphere)
    mat4 screen_matrix;
    matrix::invert(screen_matrix, resultMatrix);

    util::Frustum frustum = util::Frustum::fromInvProjMatrix(screen_matrix, 1.0, 0.0, true);

    const float boundRadius = model->boundsRadius;
    auto boundsCenter = model->boundsCenter;

    auto isSphereIntersect = [boundRadius, boundsCenter](const vec4& plane)-> bool {
        auto dist = mbgl::vec3Dot({plane[0], plane[1], plane[2]}, boundsCenter) + plane[3];
        return dist < -boundRadius || std::abs(dist) < boundRadius;
    };

    //can't use default frustum.intersects because it expects aabb.z to be 0
    auto planes = frustum.getPlanes();
    for (const auto& plane : planes) {
        if (!isSphereIntersect(plane)) {
            //culling happened
            return;
        }
    }

    if (model->cw_orient) {
        glFrontFace(GL_CW);
    } else {
        glFrontFace(GL_CCW);
    }

    //actual rendering
    mbgl::gl::bindUniform(proj_mat_loc, resultMatrix);

    MBGL_CHECK_ERROR(glBindTexture(GL_TEXTURE_2D, model->texture_handle));
    
    MBGL_CHECK_ERROR(glEnableVertexAttribArray(vertex_pos_loc));
    MBGL_CHECK_ERROR(glEnableVertexAttribArray(texCoord_loc));

    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, model->bufferHandle));

    MBGL_CHECK_ERROR(glVertexAttribPointer(vertex_pos_loc, 3, GL_FLOAT, GL_FALSE, 0, (void*)0));
    MBGL_CHECK_ERROR(glVertexAttribPointer(texCoord_loc, 2, GL_FLOAT, GL_FALSE, 0, (void*)(size_t)model->texCoordBufferOffset));

    if (model->numIndices == 0) {
        MBGL_CHECK_ERROR(glDrawArrays(GL_TRIANGLES, 0, model->numVertices));
    } else {
        MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model->bufferHandle));
        glDrawElements(
            GL_TRIANGLE_STRIP,
            model->numIndices,
            GL_UNSIGNED_SHORT,
            (void*)(size_t)model->indexBufferOffset
        );
    }
    MBGL_CHECK_ERROR(glDisableVertexAttribArray(vertex_pos_loc));
    MBGL_CHECK_ERROR(glDisableVertexAttribArray(texCoord_loc));
    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

} //namespace platform
} //namespace mbgl
