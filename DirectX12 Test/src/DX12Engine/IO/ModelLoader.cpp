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
                            attrib.normals[(3 * idx.normal_index) + 0],
                            attrib.normals[(3 * idx.normal_index) + 1],
                            attrib.normals[(3 * idx.normal_index) + 2]
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

        // Compute tangents
        std::vector<DirectX::XMFLOAT3> tan1(mesh.Vertices.size() * 2);
        std::vector<DirectX::XMFLOAT3> tan2(tan1.begin() + mesh.Vertices.size(), tan1.end());

        for (size_t i = 0; i < mesh.Indices.size(); i += 3)
        {
            UINT i1 = mesh.Indices[i];
            UINT i2 = mesh.Indices[i + 1];
            UINT i3 = mesh.Indices[i + 2];

            const DirectX::XMFLOAT3& v1 = mesh.Vertices[i1].Position;
            const DirectX::XMFLOAT3& v2 = mesh.Vertices[i2].Position;
            const DirectX::XMFLOAT3& v3 = mesh.Vertices[i3].Position;

            const DirectX::XMFLOAT2& w1 = mesh.Vertices[i1].TexCoord;
            const DirectX::XMFLOAT2& w2 = mesh.Vertices[i2].TexCoord;
            const DirectX::XMFLOAT2& w3 = mesh.Vertices[i3].TexCoord;

            float x1 = v2.x - v1.x;
            float x2 = v3.x - v1.x;
            float y1 = v2.y - v1.y;
            float y2 = v3.y - v1.y;
            float z1 = v2.z - v1.z;
            float z2 = v3.z - v1.z;

            float s1 = w2.x - w1.x;
            float s2 = w3.x - w1.x;
            float t1 = w2.y - w1.y;
            float t2 = w3.y - w1.y;

            float r = 1.0F / (s1 * t2 - s2 * t1);
            DirectX::XMFLOAT3 sdir =
            {
                (t2 * x1 - t1 * x2) * r,
                (t2 * y1 - t1 * y2) * r,
                (t2 * z1 - t1 * z2) * r
            };
            DirectX::XMFLOAT3 tdir =
            {
                (s1 * x2 - s2 * x1) * r,
                (s1 * y2 - s2 * y1) * r,
                (s1 * z2 - s2 * z1) * r
            };

            tan1[i1] = sdir;
            tan1[i2] = sdir;
            tan1[i3] = sdir;

            tan2[i1] = tdir;
            tan2[i2] = tdir;
            tan2[i3] = tdir;
        }

        for (size_t i = 0; i < mesh.Vertices.size(); ++i)
        {
            const DirectX::XMFLOAT3& n = mesh.Vertices[i].Normal;
            const DirectX::XMFLOAT3& t = tan1[i];

            // Gram-Schmidt orthogonalize
            mesh.Vertices[i].Tangent =
            {
                t.x - n.x * DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0],
                t.y - n.y * DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0],
                t.z - n.z * DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0]
            };

            // Normalize the tangent
            DirectX::XMStoreFloat3(&mesh.Vertices[i].Tangent, DirectX::XMVector3Normalize(XMLoadFloat3(&mesh.Vertices[i].Tangent)));
        }
        return mesh;
    }
}
