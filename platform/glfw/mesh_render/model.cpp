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

bool Model::LoadModel(const std::string& filePath, bool flipY) {
    std::string err;
    std::string warn;
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;

    if (filePath.size() >= 4 && filePath.substr(filePath.size() - 4) == ".glb") {
        if (!loader.LoadBinaryFromFile(&model, &err, &warn, filePath)) {
            return false;
        }
    } else {
        if (!loader.LoadASCIIFromFile(&model, &err, &warn, filePath)) {
            return false;
        }
    }

    //TODO: Most of this can be combined into one function/lambda

    //vertices

    const float* positionsData;
    GLsizeiptr verticesLen = 0;

    auto positionAccessorIt = model.meshes[model_index].primitives[0].attributes.find("POSITION");
    if (positionAccessorIt != model.meshes[model_index].primitives[0].attributes.end()) {
        const int positionAccessorIndex = positionAccessorIt->second;
        const tinygltf::Accessor& accessor = model.accessors[positionAccessorIndex];

        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        positionsData = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        verticesLen = bufferView.byteLength;
        indexBufferOffset = verticesLen;

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
    
    if (model.meshes[model_index].primitives[0].indices >= 0) {
        indexed = true;
        const tinygltf::Accessor &indexAccessor = model.accessors[model.meshes[0].primitives[0].indices];
        const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

        numIndices = indexAccessor.count;
        indexLen = indexBufferView.byteLength;
        indexData = reinterpret_cast<const unsigned short*>(&indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset]);
    }

    //animations (only one for now)
    GLsizeiptr bonesLen = 0;
    const unsigned short* bonesData;
    
    if (!model.animations.empty()) {
        tinygltf::Animation& firstAnimation = model.animations[0];
    }

    //texture (only one for now)
    texCoordBufferOffset = verticesLen + indexLen;
    GLsizeiptr texCoordLen = 0;
    float* texCoordData;

    if (!model.images.empty()) {
        auto texCoordAccessorIt = model.meshes[model_index].primitives[0].attributes.find("TEXCOORD_0");
        if (texCoordAccessorIt != model.meshes[model_index].primitives[0].attributes.end()) {
            const int accessorIndex = texCoordAccessorIt->second;
            const tinygltf::Accessor& accessor = model.accessors[accessorIndex];

            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            texCoordData = reinterpret_cast<float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
            if (flipY) {
                // Modify the texture coordinates here (flip Y-coordinate)
                for (size_t i = 0; i < accessor.count; ++i) {
                    float u = texCoordData[i * accessor.type];
                    float v = texCoordData[i * accessor.type + 1]; // Assuming 2 components for texture coordinates

                    // Flip the y-coordinate
                    v = 1.0f - v;

                    // Update the texCoordData directly in the buffer
                    texCoordData[i * accessor.type] = u;
                    texCoordData[i * accessor.type + 1] = v;
                }
            }
            texCoordLen = bufferView.byteLength;
            
            //Only one texture is supported
            const tinygltf::Image& image = model.images[0];
            glGenTextures(1, &texture_handle);
            glBindTexture(GL_TEXTURE_2D, texture_handle);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.image.data());
            //TODO: set texture parameters
            MBGL_CHECK_ERROR(glGenerateMipmap(GL_TEXTURE_2D));
            MBGL_CHECK_ERROR(glBindTexture(GL_TEXTURE_2D, 0));
        } else {
            return false;
        }
    } else {
        return false;
    }

    GLsizeiptr normalLen = 0;
    const float* normalData;

    normalBufferOffset = indexLen + verticesLen + texCoordLen;

    auto normalAccessorIt = model.meshes[model_index].primitives[0].attributes.find("NORMAL");
    if (normalAccessorIt != model.meshes[model_index].primitives[0].attributes.end()) {
        const int normalAccessorIndex = normalAccessorIt->second;
        const tinygltf::Accessor& accessor = model.accessors[normalAccessorIndex];

        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        normalData = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        normalLen = bufferView.byteLength;
    }

    MBGL_CHECK_ERROR(glGenBuffers(1, &bufferHandle));
    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, bufferHandle));

    MBGL_CHECK_ERROR(glBufferData(GL_ARRAY_BUFFER, verticesLen + indexLen + texCoordLen + normalLen, nullptr, GL_STATIC_DRAW));
    MBGL_CHECK_ERROR(glBufferSubData(GL_ARRAY_BUFFER, 0, verticesLen, positionsData));
    if (indexLen > 0) {
        MBGL_CHECK_ERROR(glBufferSubData(GL_ARRAY_BUFFER, indexBufferOffset, indexLen, indexData));
    }
    MBGL_CHECK_ERROR(glBufferSubData(GL_ARRAY_BUFFER, texCoordBufferOffset, texCoordLen, texCoordData));
    MBGL_CHECK_ERROR(glBufferSubData(GL_ARRAY_BUFFER, normalBufferOffset, normalLen, normalData));

    MBGL_CHECK_ERROR(glBindBuffer(GL_ARRAY_BUFFER, 0));

    return true;
}

void Model::SetCoordinates(double lat, double lng) {
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
