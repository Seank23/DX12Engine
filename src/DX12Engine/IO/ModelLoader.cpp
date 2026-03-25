#define NOMINMAX
#define TINYOBJLOADER_IMPLEMENTATION
// Disable tinyobjloader's bundled fast_float which uses constexpr std::distance
// (only valid from C++17). The standard strtof fallback is always available.
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
// windows.h (pulled in via wrl.h ? Mesh.h) defines min/max macros even with
// NOMINMAX if it was already included by a prior translation unit's PCH.
// Unconditionally undefine them here so tiny_obj_loader.h sees clean names.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "tiny_obj_loader.h"
#include "ModelLoader.h"
#include <string>
#include <stdexcept>
#include <iostream>
#include <unordered_map>
#include "../Asset/MeshAsset.h"
#include "../Resources/ResourceManager.h"

namespace DX12Engine
{
    ModelLoader::ModelLoader()
    {
    }

    ModelLoader::~ModelLoader()
    {
    }

    MeshAsset ModelLoader::LoadObj(const std::string& filename)
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

        MeshAsset mesh;
        std::vector<Vertex> vertices;
        std::vector<UINT> indices;
        std::unordered_map<std::string, uint32_t> uniqueVertices;
		DirectX::XMFLOAT3 minBounds = { FLT_MAX, FLT_MAX, FLT_MAX };
		DirectX::XMFLOAT3 maxBounds = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

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
                        uniqueVertices[key] = static_cast<uint32_t>(vertices.size());
                        vertices.push_back({ position, normal, texCoord });
						minBounds.x = std::min(minBounds.x, position.x);
						minBounds.y = std::min(minBounds.y, position.y);
						minBounds.z = std::min(minBounds.z, position.z);
						maxBounds.x = std::max(maxBounds.x, position.x);
						maxBounds.y = std::max(maxBounds.y, position.y);
						maxBounds.z = std::max(maxBounds.z, position.z);
                    }
                    // Add the index for this vertex
                    indices.push_back(uniqueVertices[key]);
                }
                indexOffset += fv;
            }
        }

        // Compute tangents
        std::vector<DirectX::XMFLOAT3> tan1(vertices.size() * 2);
        std::vector<DirectX::XMFLOAT3> tan2(tan1.begin() + vertices.size(), tan1.end());

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            UINT i1 = indices[i];
            UINT i2 = indices[i + 1];
            UINT i3 = indices[i + 2];

            const DirectX::XMFLOAT3& v1 = vertices[i1].Position;
            const DirectX::XMFLOAT3& v2 = vertices[i2].Position;
            const DirectX::XMFLOAT3& v3 = vertices[i3].Position;

            const DirectX::XMFLOAT2& w1 = vertices[i1].TexCoord;
            const DirectX::XMFLOAT2& w2 = vertices[i2].TexCoord;
            const DirectX::XMFLOAT2& w3 = vertices[i3].TexCoord;

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

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const DirectX::XMFLOAT3& n = vertices[i].Normal;
            const DirectX::XMFLOAT3& t = tan1[i];

            // Gram-Schmidt orthogonalize
            vertices[i].Tangent =
            {
                t.x - n.x * DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0],
                t.y - n.y * DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0],
                t.z - n.z * DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0]
            };

            // Normalize the tangent
            DirectX::XMStoreFloat3(&vertices[i].Tangent, DirectX::XMVector3Normalize(XMLoadFloat3(&vertices[i].Tangent)));
        }

        MeshPrimitive prim(
            ResourceManager::GetInstance().CreateVertexBuffer(vertices),
            ResourceManager::GetInstance().CreateIndexBuffer(indices),
            (UINT)indices.size(), 0, 0, 0);
		prim.SetBounds({ minBounds, maxBounds });
        mesh.AddPrimitive(std::move(prim));
        return mesh;
    }
}
