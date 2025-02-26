#include "ModelLoader.h"
#include <string>
#include <stdexcept>
#include <iostream>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace DX12Engine
{
    ModelLoader::ModelLoader()
    {
    }

    ModelLoader::~ModelLoader()
    {
    }

    Mesh ModelLoader::LoadObj(const std::string& filename)
    {
        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(filename)) 
        {
            if (!reader.Error().empty())
                throw std::runtime_error("TinyObjReader: " + reader.Error());
            throw std::runtime_error("Failed to load OBJ file: " + filename);
        }

        if (!reader.Warning().empty())
            std::cerr << "TinyObjReader Warning: " << reader.Warning() << std::endl;

        const auto& attrib = reader.GetAttrib();
        const auto& shapes = reader.GetShapes();
        const auto& materials = reader.GetMaterials();

        Mesh mesh;
        std::unordered_map<std::string, uint32_t> uniqueVertices;

        // Iterate over shapes (e.g., cube, sphere, etc.)
        for (const auto& shape : shapes) 
        {
            // Iterate over faces
            size_t indexOffset = 0;
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) 
            {
                size_t fv = shape.mesh.num_face_vertices[f]; // Typically 3 (triangles)

                for (size_t v = 0; v < fv; v++) 
                {
                    // Access indices for vertex, texture coord, and normal
                    tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

                    // Extract vertex position
                    DirectX::XMFLOAT3 position = 
                    {
                        attrib.vertices[3 * idx.vertex_index + 0],
                        attrib.vertices[3 * idx.vertex_index + 1],
                        attrib.vertices[3 * idx.vertex_index + 2]
                    };

                    // Extract normal
                    DirectX::XMFLOAT3 normal = { 0.0f, 0.0f, 0.0f };
                    if (idx.normal_index >= 0) 
                    {
                        normal = 
                        {
                            attrib.normals[3 * idx.normal_index + 0],
                            attrib.normals[3 * idx.normal_index + 1],
                            attrib.normals[3 * idx.normal_index + 2]
                        };
                    }

                    // Extract texture coordinate
                    DirectX::XMFLOAT2 texCoord = { 0.0f, 0.0f };
                    if (idx.texcoord_index >= 0) 
                    {
                        texCoord = 
                        {
                            attrib.texcoords[2 * idx.texcoord_index + 0],
                            attrib.texcoords[2 * idx.texcoord_index + 1]
                        };
                    }

                    // Construct a unique key for this vertex
                    std::string key = std::to_string(idx.vertex_index) + "/" +
                        std::to_string(idx.normal_index) + "/" +
                        std::to_string(idx.texcoord_index);

                    // Add the vertex if it hasn’t been added yet
                    if (uniqueVertices.find(key) == uniqueVertices.end()) 
                    {
                        uniqueVertices[key] = static_cast<uint32_t>(mesh.Vertices.size());
                        mesh.Vertices.push_back({ position, normal, texCoord });
                    }
                    // Add the index for this vertex
                    mesh.Indices.push_back(uniqueVertices[key]);
                }
                indexOffset += fv;
            }
        }
        return mesh;
    }
}
