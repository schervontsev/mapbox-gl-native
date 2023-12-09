#include "model.hpp"
#include <mbgl/util/color.hpp>
#include <mbgl/style/expression/type.hpp>
#include <mbgl/gl/custom_layer.hpp>
#include <mbgl/style/layers/fill_layer.hpp>
#include <mbgl/map/camera.hpp>
#include <mbgl/style/layers/background_layer.hpp>
#include <mbgl/util/mat4.hpp>
#include <mbgl/util/mat3.hpp>
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

    //vertices

    const float* positionsData;

    auto positionAccessorIt = model.meshes[0].primitives[0].attributes.find("POSITION");
    if (positionAccessorIt != model.meshes[0].primitives[0].attributes.end()) {
        const int positionAccessorIndex = positionAccessorIt->second;
        const tinygltf::Accessor& accessor = model.accessors[positionAccessorIndex];

        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        positionsData = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        indexBufferOffset = bufferView.byteLength;

        //find aabb for bounds
        numVertices = static_cast<int>(accessor.count);
        if (accessor.minValues.size() >= 3 && accessor.maxValues.size() >= 3) {
            vec3 maxPoint {accessor.maxValues[0], accessor.maxValues[1], accessor.maxValues[2] };
            boundsCenter = {
                (accessor.minValues[0] + accessor.maxValues[0]) * 0.5f, 
                (accessor.minValues[1] + accessor.maxValues[1]) * 0.5f, 
                (accessor.minValues[2] + accessor.maxValues[2]) * 0.5f
            };
            boundsRadius = mbgl::vec3Length(vec3Sub(maxPoint, boundsCenter));
        }

    } else {
        return false;
    }

    //indices (model without indices is possible)
    bool indexed = false;
    GLsizeiptr indexLen = 0;
    const unsigned short* indexData;
    
    if (model.meshes[0].primitives[0].indices >= 0) {
        indexed = true;
        const tinygltf::Accessor &indexAccessor = model.accessors[model.meshes[0].primitives[0].indices];
        const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

        numIndices = indexAccessor.count;
        indexLen = indexBufferView.byteLength;
        indexData = reinterpret_cast<const unsigned short*>(&indexBuffer.data.at(0) + indexBufferView.byteOffset);
    }


    MBGL_CHECK_ERROR(glGenBuffers(1, &bufferHandle));
    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, bufferHandle));

    MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, indexBufferOffset + indexLen, nullptr, GL_STATIC_DRAW));
    MBGL_CHECK_ERROR(glBufferSubData(GL_ARRAY_BUFFER, 0, indexBufferOffset, positionsData));
    MBGL_CHECK_ERROR(glBufferSubData(GL_ARRAY_BUFFER, indexBufferOffset, indexLen, indexData));
    
    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, 0));

    if (!model.images.empty()) {
        const tinygltf::Image& image = model.images[0];
        //const tinygltf::Accessor &texturePosAccessor = model.accessors[model.]
        glGenTextures(1, &texture_handle);
        glBindTexture(GL_TEXTURE_2D, texture_handle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.image.data());
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
