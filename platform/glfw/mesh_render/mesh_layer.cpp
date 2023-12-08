#include "mesh_layer.hpp"
#include <mbgl/util/color.hpp>
#include <mbgl/style/expression/type.hpp>
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/style/layers/fill_layer.hpp>
#include <mbgl/map/camera.hpp>
#include <mbgl/style/layers/background_layer.hpp>
#include <mbgl/util/mat4.hpp>
#include <mbgl/gl/uniform.hpp>
#include <mbgl/util/projection.hpp>
#include <mbgl/map/transform_state.hpp>
#include <mbgl/util/mat2.hpp>
#include <mbgl/gl/defines.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <../../../vendor/tinygltf/tiny_gltf.h>

#include <iostream>

namespace mbgl {
namespace platform {

static const GLchar* vertexShaderSource = R"MBGL_SHADER(
attribute vec3 a_pos;
uniform mat4 proj_mat;
void main() {
    gl_Position = proj_mat * vec4(a_pos, 1.0);
}
)MBGL_SHADER";

static const GLchar* fragmentShaderSource = R"MBGL_SHADER(
void main() {
    gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);
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
    a_pos_loc = MBGL_CHECK_ERROR(glGetAttribLocation(program, "a_pos"));

    std::string err;
    std::string warn;
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, "platform/glfw/assets/Fox.glb");
    
    const tinygltf::BufferView &bufferView = model.bufferViews[0];
    const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
    bool indexed = false;
    if (model.meshes[0].primitives[0].indices >= 0) {
        indexed = true;
        const tinygltf::Accessor &indexAccessor = model.accessors[model.meshes[0].primitives[0].indices];
        const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

        numIndices = indexAccessor.count;

        MBGL_CHECK_ERROR(glGenBuffers(1, &indexBufferHandle));
        MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferHandle));
        MBGL_CHECK_ERROR(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferView.byteLength, &indexBuffer.data.at(0) + indexBufferView.byteOffset, GL_STATIC_DRAW));
    }

    auto positionAccessorIt = model.meshes[0].primitives[0].attributes.find("POSITION");
    if (positionAccessorIt != model.meshes[0].primitives[0].attributes.end()) {
        const int positionAccessorIndex = positionAccessorIt->second;
        const tinygltf::Accessor& accessor = model.accessors[positionAccessorIndex];

        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        const float* positionsData = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);

        numVertices = static_cast<int>(accessor.count);
        MBGL_CHECK_ERROR(glGenBuffers(1, &bufferHandle));
        MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, bufferHandle));
        MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, numVertices * 3 * sizeof(GLfloat), positionsData, GL_STATIC_DRAW));

    }

    proj_mat_loc = glGetUniformLocation(program, "proj_mat");
}

void MeshLayer::render(const mbgl::style::CustomLayerRenderParameters& param) {
    MBGL_CHECK_ERROR(glUseProgram(program));
    
    //object's lat/lng coordinates
    mbgl::LatLng latLngMesh { 38.889814, -77.035915 };

    //camera's lat/lng coordinates
    mbgl::LatLng latLng {param.latitude, param.longitude };

    //need to convert x and y from meters to tile coordinates
    const float prescale = 1.0 / (std::cos(latLngMesh.latitude() * mbgl::util::DEG2RAD) * mbgl::util::M2PI * mbgl::util::EARTH_RADIUS_M);

    mbgl::mat4 model_matrix;
    mbgl::matrix::identity(model_matrix);
    mbgl::matrix::scale(model_matrix, model_matrix, prescale, prescale, 1.0);

    //TODO: for fox only
    mbgl::matrix::rotate_x(model_matrix, model_matrix, M_PI_2);

    mbgl::mat4 world_matrix;
    mbgl::matrix::identity(world_matrix);

    mbgl::Point<double> world_pos {
        (180.0 + latLngMesh.longitude()) / 360.0,
        (180.0 - (180.0 / M_PI * std::log(std::tan(M_PI_4 + latLngMesh.latitude() * M_PI / 360.0)))) / 360.0
    };

    const double worldSize = mbgl::Projection::worldSize(std::pow(2.0, param.zoom));
    mbgl::matrix::scale(world_matrix, world_matrix, worldSize, worldSize, 1.0);
    mbgl::matrix::translate(world_matrix, world_matrix, world_pos.x, world_pos.y, 0.f);
    
    mbgl::mat4 resultMatrix {param.projectionMatrix};
    mbgl::matrix::multiply(resultMatrix, resultMatrix, world_matrix);
    mbgl::matrix::multiply(resultMatrix, resultMatrix, model_matrix);

    mbgl::gl::bindUniform(proj_mat_loc, resultMatrix);
    
    MBGL_CHECK_ERROR(glEnableVertexAttribArray(a_pos_loc));
    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, bufferHandle));
    MBGL_CHECK_ERROR(glVertexAttribPointer(a_pos_loc, 3, GL_FLOAT, GL_FALSE, 0, (void*)0));

    if (numIndices == 0) {
        MBGL_CHECK_ERROR(glDrawArrays(GL_TRIANGLES, 0, numVertices));
    } else {
        MBGL_CHECK_ERROR(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferHandle));
        glDrawElements(
            GL_TRIANGLE_STRIP,
            numIndices,
            GL_UNSIGNED_SHORT,
            (void*)0
        );
    }
    MBGL_CHECK_ERROR(glDisableVertexAttribArray(a_pos_loc));
}

void MeshLayer::deinitialize() {
        if (program) {
            MBGL_CHECK_ERROR(glDeleteBuffers(1, &bufferHandle));
            MBGL_CHECK_ERROR(glDeleteBuffers(1, &indexBufferHandle));
            MBGL_CHECK_ERROR(glDetachShader(program, vertexShader));
            MBGL_CHECK_ERROR(glDetachShader(program, fragmentShader));
            MBGL_CHECK_ERROR(glDeleteShader(vertexShader));
            MBGL_CHECK_ERROR(glDeleteShader(fragmentShader));
            MBGL_CHECK_ERROR(glDeleteProgram(program));
        }
}
} //namespace platform
} //namespace mbgl
