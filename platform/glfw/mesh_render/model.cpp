#include "model.hpp"
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

bool Model::LoadModel(const std::string& path) {
    std::string err;
    std::string warn;
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;

    if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) {
        return false;
    }

    //rendering only the first primitive of the first model
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
    return true;
}

void Model::SetCoordinates(double lat, double lng) {
    latLng = {lat, lng};
    //need to convert x and y from meters to tile coordinates
    const float prescale = 1.0 / (std::cos(lat * mbgl::util::DEG2RAD) * mbgl::util::M2PI * mbgl::util::EARTH_RADIUS_M);

    mbgl::matrix::identity(model_matrix);
    mbgl::matrix::scale(model_matrix, model_matrix, prescale, prescale, 1.0);

    world_pos = {
        (180.0 + lng) / 360.0,
        (180.0 - (180.0 / M_PI * std::log(std::tan(M_PI_4 + lat * M_PI / 360.0)))) / 360.0
    };
}

void Model::Rotate(vec3 euler) {
    mbgl::matrix::rotate_x(model_matrix, model_matrix, euler[0] * mbgl::util::DEG2RAD);
    mbgl::matrix::rotate_y(model_matrix, model_matrix, euler[1] * mbgl::util::DEG2RAD);
    mbgl::matrix::rotate_z(model_matrix, model_matrix, euler[2] * mbgl::util::DEG2RAD);
}

} //namespace platform
} //namespace mbgl
