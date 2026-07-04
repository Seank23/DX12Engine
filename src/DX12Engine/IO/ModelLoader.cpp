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
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include "ModelLoader.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <stdexcept>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cfloat>
#include <cmath>
#include <nlohmann/json.hpp>
#include "../Asset/CookedModelFormat.h"
#include "../Asset/MeshAsset.h"
#include "../Asset/ModelAsset.h"
#include "../Asset/MaterialAsset.h"
#include "../Asset/MaterialTemplate.h"
#include "../Resources/ResourceManager.h"
#include "TextureLoader.h"
#include "../Resources/Materials/PBRMaterial.h"
#include <DirectXTex.h>

namespace DX12Engine
{
    namespace fs = std::filesystem;

    namespace
    {
        ModelLoader::CookedFallbackMode g_CookedFallbackMode = ModelLoader::CookedFallbackMode::Auto;

        ModelLoader::CookedFallbackMode ParseCookedFallbackModeFromEnv(const char* envValue)
        {
            if (!envValue)
                return ModelLoader::CookedFallbackMode::Auto;

            std::string modeText(envValue);
            std::transform(modeText.begin(), modeText.end(), modeText.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (modeText == "allow" || modeText == "fallback" || modeText == "1" || modeText == "true")
                return ModelLoader::CookedFallbackMode::AllowGlbFallback;

            if (modeText == "strict" || modeText == "0" || modeText == "false")
                return ModelLoader::CookedFallbackMode::StrictCookedOnly;

            return ModelLoader::CookedFallbackMode::Auto;
        }

        ModelLoader::CookedFallbackMode GetDefaultCookedFallbackMode()
        {
#ifdef NDEBUG
            return ModelLoader::CookedFallbackMode::StrictCookedOnly;
#else
            return ModelLoader::CookedFallbackMode::AllowGlbFallback;
#endif
        }

        ModelLoader::CookedFallbackMode ResolveCookedFallbackMode(ModelLoader::CookedFallbackMode configured)
        {
            if (const char* envMode = std::getenv("DX12ENGINE_COOKED_FALLBACK_MODE"))
                return ParseCookedFallbackModeFromEnv(envMode);

            return configured;
        }

        bool ShouldAllowGlbFallback(ModelLoader::CookedFallbackMode mode)
        {
            mode = ResolveCookedFallbackMode(mode);

            if (mode == ModelLoader::CookedFallbackMode::Auto)
                mode = GetDefaultCookedFallbackMode();

            if (mode == ModelLoader::CookedFallbackMode::AllowGlbFallback)
                return true;

            return false;
        }
    }

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

                    // Add the vertex if it hasn�t been added yet
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
            DirectX::XMFLOAT3 ortho =
            {
                t.x - n.x * DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0],
                t.y - n.y * DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0],
                t.z - n.z * DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0]
            };

            DirectX::XMFLOAT3 normalised;
            DirectX::XMStoreFloat3(&normalised, DirectX::XMVector3Normalize(XMLoadFloat3(&ortho)));
            // OBJ has no mirror/handedness info, so use w=+1.0.
            vertices[i].Tangent = { normalised.x, normalised.y, normalised.z, 1.0f };
        }

        MeshPrimitive prim(
            ResourceManager::GetInstance().CreateVertexBuffer(vertices),
            ResourceManager::GetInstance().CreateIndexBuffer(indices),
            (UINT)indices.size(), 0, 0, 0);
		prim.SetBounds({ minBounds, maxBounds });
        mesh.AddPrimitive(std::move(prim));
        return mesh;
    }

    // -------------------------------------------------------------------------
    // Accessor reader helpers
    // -------------------------------------------------------------------------

    // Returns a raw byte pointer to element [index] inside an accessor,
    // accounting for bufferView.byteOffset, accessor.byteOffset and stride.
    static const uint8_t* AccessorElement(const tinygltf::Model& model,
        const tinygltf::Accessor& acc, size_t index)
    {
        const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
        const tinygltf::Buffer& buf = model.buffers[bv.buffer];
        int stride = acc.ByteStride(bv);
        if (stride <= 0)
            stride = tinygltf::GetComponentSizeInBytes(acc.componentType)
                     * tinygltf::GetNumComponentsInType(acc.type);
        const uint8_t* base = buf.data.data() + bv.byteOffset + acc.byteOffset;
        return base + static_cast<size_t>(stride) * index;
    }

    // Read a scalar uint32 from any integer component type.
    static uint32_t ReadScalarUint(const uint8_t* p, int componentType)
    {
        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return *reinterpret_cast<const uint8_t*>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return *reinterpret_cast<const uint16_t*>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return *reinterpret_cast<const uint32_t*>(p);
        default:                                     return 0;
        }
    }

    // Read a float from a scalar accessor element (float or normalised int).
    static float ReadScalarFloat(const uint8_t* p, int componentType, bool normalized)
    {
        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:          return *reinterpret_cast<const float*>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return normalized ? *p / 255.0f : static_cast<float>(*p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return normalized ? *reinterpret_cast<const uint16_t*>(p) / 65535.0f
                                                                       : static_cast<float>(*reinterpret_cast<const uint16_t*>(p));
        default:                                     return 0.0f;
        }
    }

    static DirectX::XMFLOAT2 ReadVec2(const uint8_t* p, int componentType, bool normalized)
    {
        if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            const float* f = reinterpret_cast<const float*>(p);
            return { f[0], f[1] };
        }
        int sz = tinygltf::GetComponentSizeInBytes(componentType);
        return {
            ReadScalarFloat(p,          componentType, normalized),
            ReadScalarFloat(p + sz,     componentType, normalized)
        };
    }

    static DirectX::XMFLOAT3 ReadVec3(const uint8_t* p, int componentType, bool normalized)
    {
        if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            const float* f = reinterpret_cast<const float*>(p);
            return { f[0], f[1], f[2] };
        }
        int sz = tinygltf::GetComponentSizeInBytes(componentType);
        return {
            ReadScalarFloat(p,          componentType, normalized),
            ReadScalarFloat(p + sz,     componentType, normalized),
            ReadScalarFloat(p + 2 * sz, componentType, normalized)
        };
    }

    static DirectX::XMFLOAT4 ReadVec4(const uint8_t* p, int componentType, bool normalized)
    {
        if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            const float* f = reinterpret_cast<const float*>(p);
            return { f[0], f[1], f[2], f[3] };
        }
        int sz = tinygltf::GetComponentSizeInBytes(componentType);
        return {
            ReadScalarFloat(p,          componentType, normalized),
            ReadScalarFloat(p + sz,     componentType, normalized),
            ReadScalarFloat(p + 2 * sz, componentType, normalized),
            ReadScalarFloat(p + 3 * sz, componentType, normalized)
        };
    }

    static AnimationInterpolation ParseAnimationInterpolation(const std::string& interpolation)
    {
        if (interpolation == "STEP")
            return AnimationInterpolation::Step;
        if (interpolation == "CUBICSPLINE")
            return AnimationInterpolation::CubicSpline;
        return AnimationInterpolation::Linear;
    }

    // -------------------------------------------------------------------------
    // Default 1x1 texture helpers
    // -------------------------------------------------------------------------

    static std::shared_ptr<Texture> CreateSolidTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        // Build a 1x1 RGBA8 ScratchImage. InitializeFromImage copies the pixel
        // data into the ScratchImage's own buffer, so pixelBuf does not need to
        // outlive this call. Ownership of scratch is transferred to the Texture.
        uint8_t pixelBuf[4] = { r, g, b, a };
        DirectX::Image img{};
        img.width      = 1;
        img.height     = 1;
        img.format     = DXGI_FORMAT_R8G8B8A8_UNORM;
        img.rowPitch   = 4;
        img.slicePitch = 4;
        img.pixels     = pixelBuf;
        auto* scratch = new DirectX::ScratchImage();
        scratch->InitializeFromImage(img);
        return ResourceManager::GetInstance().CreateTexture(scratch);
    }

    // -------------------------------------------------------------------------
    // Normal/tangent generation
    // -------------------------------------------------------------------------

    static void GenerateFlatNormals(std::vector<Vertex>& vertices,
                                    const std::vector<UINT>& indices)
    {
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            Vertex& v0 = vertices[indices[i]];
            Vertex& v1 = vertices[indices[i + 1]];
            Vertex& v2 = vertices[indices[i + 2]];

            DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&v0.Position);
            DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&v1.Position);
            DirectX::XMVECTOR p2 = DirectX::XMLoadFloat3(&v2.Position);

            DirectX::XMVECTOR n = DirectX::XMVector3Normalize(
                DirectX::XMVector3Cross(
                    DirectX::XMVectorSubtract(p1, p0),
                    DirectX::XMVectorSubtract(p2, p0)));

            DirectX::XMFLOAT3 nf;
            DirectX::XMStoreFloat3(&nf, n);
            v0.Normal = nf;
            v1.Normal = nf;
            v2.Normal = nf;
        }
    }

    static void ComputeTangents(std::vector<Vertex>& vertices,
                                const std::vector<UINT>& indices)
    {
        std::vector<DirectX::XMFLOAT3> tan1(vertices.size() * 2);
        std::vector<DirectX::XMFLOAT3> tan2(tan1.begin() + vertices.size(), tan1.end());

        for (size_t i = 0; i + 2 < indices.size(); i += 3)
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

            float x1 = v2.x - v1.x, x2 = v3.x - v1.x;
            float y1 = v2.y - v1.y, y2 = v3.y - v1.y;
            float z1 = v2.z - v1.z, z2 = v3.z - v1.z;
            float s1 = w2.x - w1.x, s2 = w3.x - w1.x;
            float t1 = w2.y - w1.y, t2 = w3.y - w1.y;

            float denom = s1 * t2 - s2 * t1;
            float r = (std::abs(denom) > 1e-8f) ? 1.0f / denom : 0.0f;
            DirectX::XMFLOAT3 sdir = { (t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r };
            DirectX::XMFLOAT3 tdir = { (s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r };

            tan1[i1] = sdir; tan1[i2] = sdir; tan1[i3] = sdir;
            tan2[i1] = tdir; tan2[i2] = tdir; tan2[i3] = tdir;
        }

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const DirectX::XMFLOAT3& n = vertices[i].Normal;
            const DirectX::XMFLOAT3& t = tan1[i];
            float dot = DirectX::XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t)).m128_f32[0];
            DirectX::XMFLOAT3 tangent = { t.x - n.x * dot, t.y - n.y * dot, t.z - n.z * dot };
            DirectX::XMFLOAT3 normalised;
            DirectX::XMStoreFloat3(&normalised, DirectX::XMVector3Normalize(XMLoadFloat3(&tangent)));
            // Computed tangents have no mirror information; use w=+1.0.
            vertices[i].Tangent = { normalised.x, normalised.y, normalised.z, 1.0f };
        }
    }

    // -------------------------------------------------------------------------
    // Import context
    // -------------------------------------------------------------------------

    struct GltfImportContext
    {
        const tinygltf::Model&               model;
        std::shared_ptr<ModelAsset>          outModel;

        // Index maps: glTF index -> engine index / shared_ptr
        std::vector<std::shared_ptr<Texture>> textureCache;   // size == model.textures.size()
        std::vector<int>                      materialMap;    // gltfMatIdx -> modelAsset material index
        std::vector<int>                      meshMap;        // gltfMeshIdx -> modelAsset mesh index
        std::vector<int>                      nodeMap;        // gltfNodeIdx -> modelAsset node index

        // Default fallback textures (created once, shared)
        std::shared_ptr<Texture> defaultAlbedo;       // white
        std::shared_ptr<Texture> defaultNormal;       // flat (128,128,255)
        std::shared_ptr<Texture> defaultMetalRough;   // metallic=0, roughness=1 encoded as G/B  -> white
        std::shared_ptr<Texture> defaultAO;           // white
        std::shared_ptr<Texture> defaultEmissive;     // black

        // Counters for final log
        int skippedPrimitives = 0;
        int importedPrimitives = 0;
    };

    struct CookedLodLevelRecord
    {
        int level = 0;
        float ratio = 1.0f;
        float error = 0.0f;
        std::uint64_t indexCount = 0;
        std::string output;
    };

    struct CookedPrimitiveLodRecord
    {
        int meshIndex = -1;
        int primitiveIndex = -1;
        std::uint64_t vertexCount = 0;
        std::uint64_t sourceIndexCount = 0;
        bool skipped = false;
        std::vector<CookedLodLevelRecord> lods;
    };

    static bool ParseCookedLodManifest(const std::string& jsonText, std::vector<CookedPrimitiveLodRecord>& outRecords)
    {
        outRecords.clear();

        try
        {
            const nlohmann::json root = nlohmann::json::parse(jsonText);
            if (!root.contains("primitives") || !root["primitives"].is_array())
                return false;

            for (const nlohmann::json& primJson : root["primitives"])
            {
                if (!primJson.is_object()) continue;

                CookedPrimitiveLodRecord primRecord;
                primRecord.meshIndex = primJson.value("meshIndex", -1);
                primRecord.primitiveIndex = primJson.value("primitiveIndex", -1);
                primRecord.vertexCount = primJson.value("vertexCount", static_cast<std::uint64_t>(0));
                primRecord.sourceIndexCount = primJson.value("sourceIndexCount", static_cast<std::uint64_t>(0));
                primRecord.skipped = primJson.value("skipped", false);
                if (primRecord.meshIndex < 0 || primRecord.primitiveIndex < 0) continue;

                if (primJson.contains("lods") && primJson["lods"].is_array())
                {
                    for (const nlohmann::json& lodJson : primJson["lods"])
                    {
                        if (!lodJson.is_object()) continue;

                        CookedLodLevelRecord lodRecord;
                        lodRecord.level = lodJson.value("level", -1);
                        if (lodRecord.level < 0) continue;

                        lodRecord.ratio = lodJson.value("ratio", 1.0f);
                        lodRecord.error = lodJson.value("error", 0.0f);
                        lodRecord.indexCount = lodJson.value("indexCount", static_cast<std::uint64_t>(0));
                        lodRecord.output = lodJson.value("output", std::string{});
                        primRecord.lods.push_back(std::move(lodRecord));
                    }
                }
                outRecords.push_back(std::move(primRecord));
            }
            return true;
        }
        catch (const nlohmann::json::exception& ex)
        {
            std::cerr << "[glTF] Failed to parse cooked LOD JSON: " << ex.what() << "\n";
            return false;
        }
    }

    static bool ReadCookedIndexBuffer(const fs::path& filePath, std::vector<UINT>& outIndices, std::uint64_t& outFileBytes)
    {
        outFileBytes = 0;
        std::ifstream input(filePath, std::ios::binary);
        if (!input) return false;

        input.seekg(0, std::ios::end);
        const std::streamoff size = input.tellg();
        if (size <= 0 || (size % static_cast<std::streamoff>(sizeof(UINT))) != 0)
            return false;

        if (size > static_cast<std::streamoff>(std::numeric_limits<size_t>::max()))
            return false;

        input.seekg(0, std::ios::beg);
        outIndices.resize(static_cast<size_t>(size / static_cast<std::streamoff>(sizeof(UINT))));
        input.read(reinterpret_cast<char*>(outIndices.data()), size);
        if (!input.good())
            return false;

        outFileBytes = static_cast<std::uint64_t>(size);
        return true;
    }

    // -------------------------------------------------------------------------
    // Step 1 � textures
    // -------------------------------------------------------------------------

    static void ImportTextures(GltfImportContext& ctx)
    {
        const tinygltf::Model& model = ctx.model;
        ctx.textureCache.resize(model.textures.size());

        for (size_t ti = 0; ti < model.textures.size(); ++ti)
        {
            const tinygltf::Texture& gltfTex = model.textures[ti];
            if (gltfTex.source < 0 || gltfTex.source >= static_cast<int>(model.images.size()))
                continue;

            const tinygltf::Image& img = model.images[gltfTex.source];

            if (img.image.empty() || img.width <= 0 || img.height <= 0 || img.component <= 0)
            {
                std::cerr << "[glTF] Texture " << ti << " has no decoded pixel data � skipped.\n";
                continue;
            }

            // tinygltf always decodes to 8-bit RGBA via stb_image (component may be 1-4,
            // but the decoded buffer is always component-wide per pixel).
            // We convert to RGBA8 if needed.
            const int w = img.width;
            const int h = img.height;
            const int comp = img.component;

            std::vector<uint8_t> rgba;
            const uint8_t* src = img.image.data();

            if (comp == 4)
            {
                rgba.assign(src, src + static_cast<size_t>(w) * h * 4);
            }
            else
            {
                rgba.resize(static_cast<size_t>(w) * h * 4, 255);
                for (int px = 0; px < w * h; ++px)
                {
                    uint8_t r = (comp > 0) ? src[px * comp + 0] : 0;
                    uint8_t g = (comp > 1) ? src[px * comp + 1] : r;
                    uint8_t b = (comp > 2) ? src[px * comp + 2] : r;
                    uint8_t a = (comp > 3) ? src[px * comp + 3] : 255;
                    rgba[px * 4 + 0] = r;
                    rgba[px * 4 + 1] = g;
                    rgba[px * 4 + 2] = b;
                    rgba[px * 4 + 3] = a;
                }
            }

            DirectX::Image dxImg{};
            dxImg.width      = static_cast<size_t>(w);
            dxImg.height     = static_cast<size_t>(h);
            dxImg.format     = DXGI_FORMAT_R8G8B8A8_UNORM;
            dxImg.rowPitch   = static_cast<size_t>(w) * 4;
            dxImg.slicePitch = static_cast<size_t>(w) * h * 4;
            dxImg.pixels     = rgba.data();

            auto* scratch = new DirectX::ScratchImage();
            HRESULT hr = scratch->InitializeFromImage(dxImg);
            if (FAILED(hr))
            {
                delete scratch;
                std::cerr << "[glTF] Failed to initialise ScratchImage for texture " << ti << "\n";
                continue;
            }

            ctx.textureCache[ti] = ResourceManager::GetInstance().CreateTexture(scratch);
        }
    }

    // -------------------------------------------------------------------------
    // Step 2 � materials
    // -------------------------------------------------------------------------

    static void ImportMaterials(GltfImportContext& ctx)
    {
        const tinygltf::Model& model = ctx.model;
        ctx.materialMap.assign(model.materials.size(), -1);

        auto resolveTexture = [&](int texIndex) -> std::shared_ptr<Texture>
        {
            if (texIndex < 0 || texIndex >= static_cast<int>(ctx.textureCache.size()))
                return nullptr;
            return ctx.textureCache[texIndex];
        };

        for (size_t mi = 0; mi < model.materials.size(); ++mi)
        {
            const tinygltf::Material& gltfMat = model.materials[mi];
            const tinygltf::PbrMetallicRoughness& pbr = gltfMat.pbrMetallicRoughness;

            auto pbrMat = std::make_shared<PBRMaterial>();
			auto matTemplate = std::make_shared<MaterialTemplate>();

            // --- base colour factor ---
            if (pbr.baseColorFactor.size() == 4) 
            {
                pbrMat->SetAlbedo({ static_cast<float>(pbr.baseColorFactor[0]),
                                    static_cast<float>(pbr.baseColorFactor[1]),
                                    static_cast<float>(pbr.baseColorFactor[2]) });
                pbrMat->SetBaseColorAlpha(static_cast<float>(pbr.baseColorFactor[3]));
            }

            // --- metallic / roughness factors ---
            pbrMat->SetMetallic(static_cast<float>(pbr.metallicFactor));
            pbrMat->SetRoughness(static_cast<float>(pbr.roughnessFactor));

            // --- emissive factor ---
            if (gltfMat.emissiveFactor.size() == 3)
                pbrMat->SetEmissive({ static_cast<float>(gltfMat.emissiveFactor[0]),
                                      static_cast<float>(gltfMat.emissiveFactor[1]),
                                      static_cast<float>(gltfMat.emissiveFactor[2]) });

			pbrMat->SetAlphaMode(gltfMat.alphaMode == "MASK" ? 1 : (gltfMat.alphaMode == "BLEND" ? 2 : 0));
			matTemplate->SetBlendPolicy(gltfMat.alphaMode == "MASK" ? AlphaMode::Masked : (gltfMat.alphaMode == "BLEND" ? AlphaMode::Blend : AlphaMode::Opaque));

			if (gltfMat.alphaMode == "BLEND")
				matTemplate->SetPassTarget(PassTarget::Transparent);
			else
				matTemplate->SetPassTarget(PassTarget::Geometry);

            if (gltfMat.alphaMode == "MASK")
				pbrMat->SetAlphaCutoff(static_cast<float>(gltfMat.alphaCutoff));
            RasterizerPolicy rasterPolicy;
            if (gltfMat.doubleSided)
				rasterPolicy.CullMode = D3D12_CULL_MODE_NONE;
			matTemplate->SetRasterizerPolicy(rasterPolicy);

			pbrMat->SetNormalScale(static_cast<float>(gltfMat.normalTexture.scale));
			pbrMat->SetOcclusionStrength(static_cast<float>(gltfMat.occlusionTexture.strength));

			// --- KHR_materials_transmission ---
			{
				auto it = gltfMat.extensions.find("KHR_materials_transmission");
				if (it != gltfMat.extensions.end() && it->second.IsObject())
				{
					const auto& tf = it->second.Get("transmissionFactor");
					if (tf.IsReal() || tf.IsInt())
						pbrMat->SetTransmission(static_cast<float>(tf.GetNumberAsDouble()));
				}
			}

			// --- KHR_materials_ior ---
			{
				auto it = gltfMat.extensions.find("KHR_materials_ior");
				if (it != gltfMat.extensions.end() && it->second.IsObject())
				{
					const auto& ior = it->second.Get("ior");
					if (ior.IsReal() || ior.IsInt())
						pbrMat->SetIOR(static_cast<float>(ior.GetNumberAsDouble()));
				}
			}

			// --- KHR_materials_emissive_strength ---
			{
				auto it = gltfMat.extensions.find("KHR_materials_emissive_strength");
				if (it != gltfMat.extensions.end() && it->second.IsObject())
				{
					const auto& es = it->second.Get("emissiveStrength");
					if (es.IsReal() || es.IsInt())
						pbrMat->SetEmissiveStrength(static_cast<float>(es.GetNumberAsDouble()));
				}
			}

			// --- KHR_materials_clearcoat ---
			{
				auto it = gltfMat.extensions.find("KHR_materials_clearcoat");
				if (it != gltfMat.extensions.end() && it->second.IsObject())
				{
					const auto& cf = it->second.Get("clearcoatFactor");
					if (cf.IsReal() || cf.IsInt())
						pbrMat->SetClearcoat(static_cast<float>(cf.GetNumberAsDouble()));
					const auto& crf = it->second.Get("clearcoatRoughnessFactor");
					if (crf.IsReal() || crf.IsInt())
						pbrMat->SetClearcoatRoughness(static_cast<float>(crf.GetNumberAsDouble()));
				}
			}

            // --- albedo texture ---
            std::shared_ptr<Texture> albedoTex = resolveTexture(pbr.baseColorTexture.index);
            albedoTex ? pbrMat->SetAlbedoMap(albedoTex) : pbrMat->SetAlbedoMap(ctx.defaultAlbedo, false);

            // --- normal texture ---
            std::shared_ptr<Texture> normalTex = resolveTexture(gltfMat.normalTexture.index);
            pbrMat->SetNormalMap(normalTex ? normalTex : ctx.defaultNormal);

            // --- metallic-roughness texture (packed R=unused, G=roughness, B=metallic) ---
            std::shared_ptr<Texture> mrTex = resolveTexture(pbr.metallicRoughnessTexture.index);
            if (mrTex)
            {
                // Use the same packed texture for both channels; shader samples G for roughness, B for metallic.
                pbrMat->SetMetallicMap(mrTex);
                pbrMat->SetRoughnessMap(mrTex);
            }
            else
            {
                // No texture: bake the scalar factors into a 1x1 packed texture so the
                // shader can sample it uniformly. G = roughness, B = metallic (glTF packing).
                uint8_t roughnessByte = static_cast<uint8_t>(std::min(1.0, pbr.roughnessFactor) * 255.0 + 0.5);
                uint8_t metallicByte  = static_cast<uint8_t>(std::min(1.0, pbr.metallicFactor)  * 255.0 + 0.5);
                std::shared_ptr<Texture> bakedMR = CreateSolidTexture(0, roughnessByte, metallicByte, 255);
                pbrMat->SetMetallicMap(bakedMR);
                pbrMat->SetRoughnessMap(bakedMR);
            }

            // --- occlusion texture ---
            std::shared_ptr<Texture> aoTex = resolveTexture(gltfMat.occlusionTexture.index);
            aoTex ? pbrMat->SetAOMap(aoTex) : pbrMat->SetAOMap(ctx.defaultAO, false);

            std::shared_ptr<Texture> emissiveTex = resolveTexture(gltfMat.emissiveTexture.index);
            emissiveTex ? pbrMat->SetEmissiveMap(emissiveTex) : pbrMat->SetEmissiveMap(ctx.defaultEmissive, false);

            std::string matName = gltfMat.name.empty()
                ? ("material_" + std::to_string(mi))
                : gltfMat.name;

            auto matAsset = std::make_shared<MaterialAsset>(matName, std::move(pbrMat));
			matAsset->SetTemplate(std::move(matTemplate));
            matAsset->SetAlphaCutoff(static_cast<float>(gltfMat.alphaCutoff));
            matAsset->SetDoubleSided(gltfMat.doubleSided);

            int assetIdx = static_cast<int>(ctx.outModel->AddMaterial(std::move(matAsset)));
            ctx.materialMap[mi] = assetIdx;
        }
    }

    // -------------------------------------------------------------------------
    // Step 3 � meshes
    // -------------------------------------------------------------------------

    static void ImportMeshes(GltfImportContext& ctx)
    {
        const tinygltf::Model& model = ctx.model;
        ctx.meshMap.assign(model.meshes.size(), -1);

        // Default material index in the model asset (add a fallback if not present).
        UINT defaultMatIdx = 0;
        if (ctx.outModel->GetMaterialCount() == 0)
        {
            auto defMat = std::make_shared<PBRMaterial>();
            defMat->SetAlbedo({ 1.0f, 0.0f, 1.0f });
            auto defAsset = std::make_shared<MaterialAsset>("default_material", std::move(defMat));
            defaultMatIdx = static_cast<UINT>(ctx.outModel->AddMaterial(std::move(defAsset)));
        }

        for (size_t gi = 0; gi < model.meshes.size(); ++gi)
        {
            const tinygltf::Mesh& gltfMesh = model.meshes[gi];
            std::string meshName = gltfMesh.name.empty()
                ? ("mesh_" + std::to_string(gi))
                : gltfMesh.name;

            auto meshAsset = std::make_shared<MeshAsset>(meshName);

            for (size_t pi = 0; pi < gltfMesh.primitives.size(); ++pi)
            {
                const tinygltf::Primitive& prim = gltfMesh.primitives[pi];

                // Only triangles supported.
                int mode = prim.mode;
                if (mode == -1) mode = TINYGLTF_MODE_TRIANGLES; // default
                if (mode != TINYGLTF_MODE_TRIANGLES)
                {
                    std::cerr << "[glTF] Mesh \"" << meshName << "\" primitive " << pi
                              << " is not TRIANGLES (mode=" << mode << ") � skipped.\n";
                    ++ctx.skippedPrimitives;
                    continue;
                }

                // --- POSITION (required) ---
                auto posIt = prim.attributes.find("POSITION");
                if (posIt == prim.attributes.end())
                {
                    std::cerr << "[glTF] Mesh \"" << meshName << "\" primitive " << pi
                              << " has no POSITION � skipped.\n";
                    ++ctx.skippedPrimitives;
                    continue;
                }
                const tinygltf::Accessor& posAcc = model.accessors[posIt->second];

                // --- optional attributes ---
                const tinygltf::Accessor* normAcc     = nullptr;
                const tinygltf::Accessor* uvAcc       = nullptr;
                const tinygltf::Accessor* tangentAcc  = nullptr;

                auto normIt = prim.attributes.find("NORMAL");
                if (normIt != prim.attributes.end())
                    normAcc = &model.accessors[normIt->second];

                auto uvIt = prim.attributes.find("TEXCOORD_0");
                if (uvIt != prim.attributes.end())
                    uvAcc = &model.accessors[uvIt->second];

                auto tanIt = prim.attributes.find("TANGENT");
                if (tanIt != prim.attributes.end())
                    tangentAcc = &model.accessors[tanIt->second];

                size_t vertCount = posAcc.count;
                std::vector<Vertex> vertices(vertCount);

                DirectX::XMFLOAT3 minBounds = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
                DirectX::XMFLOAT3 maxBounds = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

                for (size_t vi = 0; vi < vertCount; ++vi)
                {
                    // Position
                    const uint8_t* pp = AccessorElement(model, posAcc, vi);
                    DirectX::XMFLOAT3 pos = ReadVec3(pp, posAcc.componentType, posAcc.normalized);
                    // Convert glTF right-handed (+Z toward viewer) to DirectX left-handed (+Z away).
                    pos.z = -pos.z;
                    vertices[vi].Position = pos;

                    minBounds.x = std::min(minBounds.x, pos.x);
                    minBounds.y = std::min(minBounds.y, pos.y);
                    minBounds.z = std::min(minBounds.z, pos.z);
                    maxBounds.x = std::max(maxBounds.x, pos.x);
                    maxBounds.y = std::max(maxBounds.y, pos.y);
                    maxBounds.z = std::max(maxBounds.z, pos.z);

                    // Normal
                    if (normAcc)
                    {
                        const uint8_t* np = AccessorElement(model, *normAcc, vi);
                        DirectX::XMFLOAT3 n = ReadVec3(np, normAcc->componentType, normAcc->normalized);
                        n.z = -n.z;
                        vertices[vi].Normal = n;
                    }

                    // TexCoord
                    if (uvAcc)
                    {
                        const uint8_t* tp = AccessorElement(model, *uvAcc, vi);
                        vertices[vi].TexCoord = ReadVec2(tp, uvAcc->componentType, uvAcc->normalized);
                    }

                    // Tangent (glTF tangent is XYZW, W = handedness sign)
                    if (tangentAcc)
                    {
                        const uint8_t* tp = AccessorElement(model, *tangentAcc, vi);
                        DirectX::XMFLOAT4 t4 = ReadVec4(tp, tangentAcc->componentType, tangentAcc->normalized);
                        // Negate Z for RH->LH conversion; preserve W (handedness sign).
                        vertices[vi].Tangent = { t4.x, t4.y, -t4.z, t4.w };
                    }
                }

                // --- indices ---
                std::vector<UINT> indices;
                if (prim.indices >= 0)
                {
                    const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
                    indices.resize(idxAcc.count);
                    for (size_t ii = 0; ii < idxAcc.count; ++ii)
                    {
                        const uint8_t* ep = AccessorElement(model, idxAcc, ii);
                        indices[ii] = ReadScalarUint(ep, idxAcc.componentType);
                    }
                }
                else
                {
                    // Non-indexed: generate sequential indices.
                    indices.resize(vertCount);
                    for (size_t ii = 0; ii < vertCount; ++ii)
                        indices[ii] = static_cast<UINT>(ii);
                }

                if (indices.empty() || indices.size() % 3 != 0)
                {
                    std::cerr << "[glTF] Mesh \"" << meshName << "\" primitive " << pi
                              << " has invalid index count (" << indices.size() << ") � skipped.\n";
                    ++ctx.skippedPrimitives;
                    continue;
                }

                // Flip winding order to match the Z-negation above (RH -> LH conversion).
                for (size_t ii = 0; ii + 2 < indices.size(); ii += 3)
                    std::swap(indices[ii + 1], indices[ii + 2]);

                // --- generate normals / tangents if missing ---
                if (!normAcc)
                    GenerateFlatNormals(vertices, indices);

                if (!tangentAcc)
                    ComputeTangents(vertices, indices);

                // --- material index ---
                UINT matIdx = defaultMatIdx;
                if (prim.material >= 0 && prim.material < static_cast<int>(ctx.materialMap.size()))
                {
                    int mapped = ctx.materialMap[prim.material];
                    if (mapped >= 0)
                        matIdx = static_cast<UINT>(mapped);
                }

                MeshPrimitive meshPrim(
                    ResourceManager::GetInstance().CreateVertexBuffer(vertices),
                    ResourceManager::GetInstance().CreateIndexBuffer(indices),
                    static_cast<UINT>(indices.size()), 0, 0, matIdx);
                meshPrim.SetBounds({ minBounds, maxBounds });

                meshAsset->AddPrimitive(std::move(meshPrim));
                ++ctx.importedPrimitives;
            }

            int assetIdx = static_cast<int>(ctx.outModel->AddMesh(std::move(meshAsset)));
            ctx.meshMap[gi] = assetIdx;
        }
    }

    // -------------------------------------------------------------------------
    // Step 4 � node hierarchy
    // -------------------------------------------------------------------------

    static void ImportNodeRecursive(GltfImportContext& ctx,
                                    int gltfNodeIdx,
                                    int parentAssetIdx)
    {
        const tinygltf::Model& model = ctx.model;
        if (gltfNodeIdx < 0 || gltfNodeIdx >= static_cast<int>(model.nodes.size()))
            return;

        const tinygltf::Node& gltfNode = model.nodes[gltfNodeIdx];

        ModelNode node;
        node.Name        = gltfNode.name.empty() ? ("node_" + std::to_string(gltfNodeIdx)) : gltfNode.name;
        node.ParentIndex = parentAssetIdx;

        // Mesh reference
        if (gltfNode.mesh >= 0 && gltfNode.mesh < static_cast<int>(ctx.meshMap.size()))
            node.MeshIndex = ctx.meshMap[gltfNode.mesh];

        // Local transform: prefer matrix, else TRS
        if (gltfNode.matrix.size() == 16)
        {
            // glTF stores matrices column-major. XMMatrixSet fills row-by-row,
            // so feed the 16 values with transposed indexing (column varies fastest).
            DirectX::XMMATRIX m = DirectX::XMMatrixSet(
                static_cast<float>(gltfNode.matrix[0]),  static_cast<float>(gltfNode.matrix[4]),
                static_cast<float>(gltfNode.matrix[8]),  static_cast<float>(gltfNode.matrix[12]),
                static_cast<float>(gltfNode.matrix[1]),  static_cast<float>(gltfNode.matrix[5]),
                static_cast<float>(gltfNode.matrix[9]),  static_cast<float>(gltfNode.matrix[13]),
                static_cast<float>(gltfNode.matrix[2]),  static_cast<float>(gltfNode.matrix[6]),
                static_cast<float>(gltfNode.matrix[10]), static_cast<float>(gltfNode.matrix[14]),
                static_cast<float>(gltfNode.matrix[3]),  static_cast<float>(gltfNode.matrix[7]),
                static_cast<float>(gltfNode.matrix[11]), static_cast<float>(gltfNode.matrix[15]));
            // Convert RH (glTF) -> LH (DirectX): negate Z row and Z column via F*M*F,
            // where F = diag(1,1,-1,1).
            DirectX::XMMATRIX F = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);
            m = F * m * F;
            DirectX::XMStoreFloat4x4(&node.LocalTransform, m);
        }
        else
        {
            DirectX::XMMATRIX T = DirectX::XMMatrixIdentity();
            DirectX::XMMATRIX R = DirectX::XMMatrixIdentity();
            DirectX::XMMATRIX S = DirectX::XMMatrixIdentity();

        if (gltfNode.translation.size() == 3)
                T = DirectX::XMMatrixTranslation(
                    static_cast<float>(gltfNode.translation[0]),
                    static_cast<float>(gltfNode.translation[1]),
                    -static_cast<float>(gltfNode.translation[2]));

            if (gltfNode.rotation.size() == 4)
            {
                // Convert RH quaternion (x,y,z,w) to LH by negating x and y.
                DirectX::XMVECTOR q = DirectX::XMVectorSet(
                    -static_cast<float>(gltfNode.rotation[0]),
                    -static_cast<float>(gltfNode.rotation[1]),
                    static_cast<float>(gltfNode.rotation[2]),
                    static_cast<float>(gltfNode.rotation[3]));
                R = DirectX::XMMatrixRotationQuaternion(q);
            }

            if (gltfNode.scale.size() == 3)
                S = DirectX::XMMatrixScaling(
                    static_cast<float>(gltfNode.scale[0]),
                    static_cast<float>(gltfNode.scale[1]),
                    static_cast<float>(gltfNode.scale[2]));

            // TRS matrices from XMMatrix* are row-major. Use S * R * T to match
            // the engine's row-vector convention (same as GameObject::UpdateModelMatrix).
            DirectX::XMStoreFloat4x4(&node.LocalTransform, S * R * T);
        }

        int assetIdx = static_cast<int>(ctx.outModel->AddNode(node));
        ctx.nodeMap[gltfNodeIdx] = assetIdx;

        for (int child : gltfNode.children)
            ImportNodeRecursive(ctx, child, assetIdx);
    }

    static void ImportNodes(GltfImportContext& ctx)
    {
        const tinygltf::Model& model = ctx.model;
        ctx.nodeMap.assign(model.nodes.size(), -1);

        // Determine which scene to use.
        int sceneIdx = model.defaultScene >= 0 ? model.defaultScene : 0;
        if (sceneIdx >= static_cast<int>(model.scenes.size()))
            return;

        const tinygltf::Scene& scene = model.scenes[sceneIdx];
        const std::vector<int>& roots = scene.nodes;

        // If there are multiple roots, insert a synthetic root node.
        int syntheticRoot = -1;
        if (roots.size() > 1)
        {
            ModelNode rootNode;
            rootNode.Name = scene.name.empty() ? "scene_root" : scene.name;
            rootNode.ParentIndex = -1;
            rootNode.MeshIndex   = -1;
            syntheticRoot = static_cast<int>(ctx.outModel->AddNode(rootNode));
        }

        for (int r : roots)
            ImportNodeRecursive(ctx, r, syntheticRoot);
    }

    static void ImportAnimations(GltfImportContext& ctx)
    {
        const tinygltf::Model& model = ctx.model;
        if (model.animations.empty() || ctx.nodeMap.empty())
            return;

        for (size_t ai = 0; ai < model.animations.size(); ++ai)
        {
            const tinygltf::Animation& gltfAnim = model.animations[ai];

            AnimationClip clip;
            clip.Name = gltfAnim.name.empty() ? ("animation_" + std::to_string(ai)) : gltfAnim.name;
            clip.Samplers.resize(gltfAnim.samplers.size());

            std::vector<int> samplerPathHint(gltfAnim.samplers.size(), -1); // 0=T,1=R,2=S
            for (const tinygltf::AnimationChannel& gltfChannel : gltfAnim.channels)
            {
                if (gltfChannel.sampler < 0 || gltfChannel.sampler >= static_cast<int>(clip.Samplers.size()))
                    continue;

                if (gltfChannel.target_node < 0 || gltfChannel.target_node >= static_cast<int>(ctx.nodeMap.size()))
                    continue;

                const int mappedNode = ctx.nodeMap[gltfChannel.target_node];
                if (mappedNode < 0)
                    continue;

                AnimationPath path;
                int pathHint = -1;
                if (gltfChannel.target_path == "translation")
                {
                    path = AnimationPath::Translation;
                    pathHint = 0;
                }
                else if (gltfChannel.target_path == "rotation")
                {
                    path = AnimationPath::Rotation;
                    pathHint = 1;
                }
                else if (gltfChannel.target_path == "scale")
                {
                    path = AnimationPath::Scale;
                    pathHint = 2;
                }
                else
                {
                    // Ignore morph targets and unsupported channel paths.
                    continue;
                }

                int& samplerHint = samplerPathHint[gltfChannel.sampler];
                if (samplerHint == -1)
                    samplerHint = pathHint;
                else if (samplerHint != pathHint)
                {
                    std::cerr << "[glTF] Animation \"" << clip.Name
                              << "\" reuses sampler " << gltfChannel.sampler
                              << " across incompatible target paths; channel skipped.\n";
                    continue;
                }

                AnimationChannel channel;
                channel.NodeIndex = static_cast<uint32_t>(mappedNode);
                channel.Path = path;
                channel.SamplerIndex = static_cast<uint32_t>(gltfChannel.sampler);
                clip.Channels.push_back(channel);
            }

            float maxTime = 0.0f;
            for (size_t si = 0; si < gltfAnim.samplers.size(); ++si)
            {
                const tinygltf::AnimationSampler& gltfSampler = gltfAnim.samplers[si];
                AnimationSampler& sampler = clip.Samplers[si];
                sampler.Interpolation = ParseAnimationInterpolation(gltfSampler.interpolation);

                if (gltfSampler.input < 0 || gltfSampler.input >= static_cast<int>(model.accessors.size()))
                    continue;
                if (gltfSampler.output < 0 || gltfSampler.output >= static_cast<int>(model.accessors.size()))
                    continue;

                const tinygltf::Accessor& timeAcc = model.accessors[gltfSampler.input];
                const tinygltf::Accessor& valueAcc = model.accessors[gltfSampler.output];

                const size_t keyCount = timeAcc.count;
                if (keyCount == 0)
                    continue;

                sampler.Times.resize(keyCount);
                for (size_t ki = 0; ki < keyCount; ++ki)
                {
                    const uint8_t* tp = AccessorElement(model, timeAcc, ki);
                    const float t = ReadScalarFloat(tp, timeAcc.componentType, timeAcc.normalized);
                    sampler.Times[ki] = t;
                    maxTime = (std::max)(maxTime, t);
                }

                const bool cubicSpline = sampler.Interpolation == AnimationInterpolation::CubicSpline;
                const size_t expectedOutputCount = cubicSpline ? keyCount * 3 : keyCount;
                if (valueAcc.count < expectedOutputCount)
                    continue;

                const int pathHint = samplerPathHint[si];
                if (pathHint == 0)
                {
                    sampler.Translations.resize(keyCount);
                    for (size_t ki = 0; ki < keyCount; ++ki)
                    {
                        const size_t valueIndex = cubicSpline ? (ki * 3 + 1) : ki;
                        const uint8_t* vp = AccessorElement(model, valueAcc, valueIndex);
                        DirectX::XMFLOAT3 v = ReadVec3(vp, valueAcc.componentType, valueAcc.normalized);
                        sampler.Translations[ki] = { v.x, v.y, -v.z };
                    }
                }
                else if (pathHint == 1)
                {
                    sampler.Rotations.resize(keyCount);
                    for (size_t ki = 0; ki < keyCount; ++ki)
                    {
                        const size_t valueIndex = cubicSpline ? (ki * 3 + 1) : ki;
                        DirectX::XMFLOAT4 q = ReadVec4(AccessorElement(model, valueAcc, valueIndex), valueAcc.componentType, valueAcc.normalized);
                        sampler.Rotations[ki] = { -q.x, -q.y, q.z, q.w };
                    }
                }
                else if (pathHint == 2)
                {
                    sampler.Scales.resize(keyCount);
                    for (size_t ki = 0; ki < keyCount; ++ki)
                    {
                        const size_t valueIndex = cubicSpline ? (ki * 3 + 1) : ki;
                        sampler.Scales[ki] = ReadVec3(AccessorElement(model, valueAcc, valueIndex), valueAcc.componentType, valueAcc.normalized);
                    }
                }
            }

            if (!clip.Channels.empty())
            {
                clip.Duration = maxTime;
                ctx.outModel->AddAnimation(clip);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Final validation
    // -------------------------------------------------------------------------

    static void ValidateAndLog(const GltfImportContext& ctx)
    {
        const ModelAsset& model = *ctx.outModel;
        int invalidPrimCount = 0;

        for (size_t mi = 0; mi < model.GetMeshCount(); ++mi)
        {
            const MeshAsset* mesh = model.GetMesh(mi);
            if (!mesh) continue;
            for (size_t pi = 0; pi < mesh->GetPrimitiveCount(); ++pi)
            {
                const MeshPrimitive* prim = mesh->GetPrimitive(pi);
                if (!prim) continue;

                if (!prim->HasGeometry())
                {
                    std::cerr << "[glTF] Mesh " << mi << " primitive " << pi << " has no geometry!\n";
                    ++invalidPrimCount;
                }
                if (prim->GetMaterialIndex() >= model.GetMaterialCount())
                {
                    std::cerr << "[glTF] Mesh " << mi << " primitive " << pi
                              << " has out-of-range material index " << prim->GetMaterialIndex() << "!\n";
                    ++invalidPrimCount;
                }
            }
        }

        printf("[glTF] Import complete: %zu nodes, %zu meshes, %d primitives imported"
               ", %d skipped, %d invalid, %zu materials, %zu textures, %zu animations (source clips: %zu)\n",
               model.GetNodeCount(),
               model.GetMeshCount(),
               ctx.importedPrimitives,
               ctx.skippedPrimitives,
               invalidPrimCount,
               model.GetMaterialCount(),
               ctx.model.textures.size(),
               model.GetAnimationCount(),
               ctx.model.animations.size());
    }

    struct CookedVertexChunkHeaderDisk
    {
        std::uint32_t PrimitiveCount = 0;
        std::uint32_t VertexCount = 0;
        std::uint64_t PrimitiveTableOffset = 0;
        std::uint64_t VertexDataOffset = 0;
    };

    struct CookedVertexPrimitiveRecordDisk
    {
        std::uint32_t PrimitiveIndex = 0;
        std::uint32_t VertexOffset = 0;
        std::uint32_t VertexCount = 0;
        std::uint32_t Reserved = 0;
    };

    struct CookedIndexChunkHeaderDisk
    {
        std::uint32_t LodRecordCount = 0;
        std::uint32_t TotalIndexCount = 0;
        std::uint64_t LodRecordOffset = 0;
        std::uint64_t IndexDataOffset = 0;
    };

    struct CookedIndexLodRecordDisk
    {
        std::uint32_t PrimitiveIndex = 0;
        std::uint32_t LodLevel = 0;
        std::uint32_t IndexOffset = 0;
        std::uint32_t IndexCount = 0;
        float Ratio = 1.0f;
        float Error = 0.0f;
        DirectX::XMFLOAT3 Min = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Max = { 0.0f, 0.0f, 0.0f };
    };

    struct CookedMeshesChunkHeaderDisk
    {
        std::uint32_t MeshCount = 0;
        std::uint32_t PrimitiveCount = 0;
        std::uint64_t MeshTableOffset = 0;
        std::uint64_t PrimitiveTableOffset = 0;
    };

    struct CookedMeshRecordDisk
    {
        std::uint32_t NameStringOffset = 0;
        std::uint32_t PrimitiveStart = 0;
        std::uint32_t PrimitiveCount = 0;
        std::uint32_t Reserved = 0;
    };

    struct CookedPrimitiveRecordDisk
    {
        std::uint32_t MeshIndex = 0;
        std::uint32_t MaterialIndex = 0;
        std::uint32_t VertexOffset = 0;
        std::uint32_t VertexCount = 0;
        std::uint32_t LodStart = 0;
        std::uint32_t LodCount = 0;
        DirectX::XMFLOAT3 Min = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Max = { 0.0f, 0.0f, 0.0f };
    };

    struct CookedNodeRecordDisk
    {
        std::uint32_t NameStringOffset = 0;
        std::int32_t ParentIndex = -1;
        std::int32_t MeshIndex = -1;
        std::uint32_t Reserved = 0;
        std::array<float, 16> LocalTransform = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
    };

    struct CookedAnimationsChunkHeaderDisk
    {
        std::uint32_t ClipCount = 0;
        std::uint32_t SamplerCount = 0;
        std::uint32_t ChannelCount = 0;
        std::uint32_t TimeKeyCount = 0;
        std::uint32_t TranslationKeyCount = 0;
        std::uint32_t RotationKeyCount = 0;
        std::uint32_t ScaleKeyCount = 0;
        std::uint32_t Reserved = 0;
        std::uint64_t ClipTableOffset = 0;
        std::uint64_t SamplerTableOffset = 0;
        std::uint64_t ChannelTableOffset = 0;
        std::uint64_t TimeDataOffset = 0;
        std::uint64_t TranslationDataOffset = 0;
        std::uint64_t RotationDataOffset = 0;
        std::uint64_t ScaleDataOffset = 0;
    };

    struct CookedAnimationClipRecordDisk
    {
        std::uint32_t NameStringOffset = 0;
        std::uint32_t SamplerStart = 0;
        std::uint32_t SamplerCount = 0;
        std::uint32_t ChannelStart = 0;
        std::uint32_t ChannelCount = 0;
        float Duration = 0.0f;
        std::uint32_t Reserved0 = 0;
        std::uint64_t Reserved1 = 0;
    };

    struct CookedAnimationSamplerRecordDisk
    {
        std::uint32_t Interpolation = 0;
        std::uint32_t Path = 0;
        std::uint32_t KeyCount = 0;
        std::uint32_t Reserved0 = 0;
        std::uint32_t TimeOffset = 0;
        std::uint32_t ValueOffset = 0;
        std::uint64_t Reserved1 = 0;
    };

    struct CookedAnimationChannelRecordDisk
    {
        std::uint32_t NodeIndex = 0;
        std::uint32_t Path = 0;
        std::uint32_t SamplerIndex = 0;
        std::uint32_t Reserved = 0;
    };

    struct CookedMaterialsChunkHeaderDisk
    {
        std::uint32_t MaterialCount = 0;
        std::uint32_t TextureRefCount = 0;
        std::uint64_t MaterialTableOffset = 0;
        std::uint64_t TextureRefTableOffset = 0;
    };

    struct CookedTextureRefRecordDisk
    {
        std::uint32_t PathStringOffset = 0;
        std::uint32_t Reserved0 = 0;
        std::uint64_t Reserved1 = 0;
    };

    struct CookedMaterialRecordDisk
    {
        std::uint32_t NameStringOffset = 0;
        std::uint32_t AlphaMode = 0;
        float AlphaCutoff = 0.5f;
        std::uint32_t DoubleSided = 0;
        std::array<float, 4> BaseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float Metallic = 1.0f;
        float Roughness = 1.0f;
        std::array<float, 3> EmissiveFactor = { 0.0f, 0.0f, 0.0f };
        std::uint32_t Padding = 0;
        std::array<std::uint32_t, 5> TextureRefIds = {
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max(),
            std::numeric_limits<std::uint32_t>::max()
        };
    };

    struct CookedChunkView
    {
        const std::uint8_t* Data = nullptr;
        std::size_t Size = 0;
        const CookedModelChunkDesc* Desc = nullptr;
    };

    struct CookedModelMappedView
    {
        CookedModelHeader Header{};
        std::vector<std::uint8_t> FileBytes;
        std::vector<CookedModelChunkDesc> Chunks;

        CookedChunkView VerticesChunk;
        CookedChunkView IndicesChunk;
        CookedChunkView MeshesChunk;
        CookedChunkView NodesChunk;
        CookedChunkView MaterialsChunk;
        CookedChunkView StringsChunk;
        CookedChunkView AnimationsChunk;

        const CookedVertexChunkHeaderDisk* VertexHeader = nullptr;
        const CookedVertexPrimitiveRecordDisk* VertexPrimitiveRecords = nullptr;
        const Vertex* Vertices = nullptr;

        const CookedIndexChunkHeaderDisk* IndexHeader = nullptr;
        const CookedIndexLodRecordDisk* IndexLodRecords = nullptr;
        const std::uint32_t* IndexData = nullptr;

        const CookedMeshesChunkHeaderDisk* MeshesHeader = nullptr;
        const CookedMeshRecordDisk* MeshRecords = nullptr;
        const CookedPrimitiveRecordDisk* PrimitiveRecords = nullptr;

        const CookedNodeRecordDisk* NodeRecords = nullptr;

        const CookedMaterialsChunkHeaderDisk* MaterialsHeader = nullptr;
        const CookedMaterialRecordDisk* MaterialRecords = nullptr;
        const CookedTextureRefRecordDisk* TextureRefRecords = nullptr;

        const CookedAnimationsChunkHeaderDisk* AnimationsHeader = nullptr;
        const CookedAnimationClipRecordDisk* AnimationClipRecords = nullptr;
        const CookedAnimationSamplerRecordDisk* AnimationSamplerRecords = nullptr;
        const CookedAnimationChannelRecordDisk* AnimationChannelRecords = nullptr;
        const float* AnimationTimes = nullptr;
        const DirectX::XMFLOAT3* AnimationTranslations = nullptr;
        const DirectX::XMFLOAT4* AnimationRotations = nullptr;
        const DirectX::XMFLOAT3* AnimationScales = nullptr;
    };

    static void ValidateAndLogCooked(
        const std::string& modelId,
        const CookedModelMappedView& mapped,
        const ModelAsset& model)
    {
        int invalidPrimCount = 0;
        std::size_t importedPrimitives = 0;

        for (size_t mi = 0; mi < model.GetMeshCount(); ++mi)
        {
            const MeshAsset* mesh = model.GetMesh(mi);
            if (!mesh) continue;
            importedPrimitives += mesh->GetPrimitiveCount();

            for (size_t pi = 0; pi < mesh->GetPrimitiveCount(); ++pi)
            {
                const MeshPrimitive* prim = mesh->GetPrimitive(pi);
                if (!prim) continue;

                if (!prim->HasGeometry())
                {
                    std::cerr << "[CookedModel] Mesh " << mi << " primitive " << pi << " has no geometry!\n";
                    ++invalidPrimCount;
                }
                if (prim->GetMaterialIndex() >= model.GetMaterialCount())
                {
                    std::cerr << "[CookedModel] Mesh " << mi << " primitive " << pi
                              << " has out-of-range material index " << prim->GetMaterialIndex() << "!\n";
                    ++invalidPrimCount;
                }
            }
        }

        printf("[CookedModel] Import complete (%s): %zu nodes, %zu meshes, %zu primitives imported"
               ", %d skipped, %d invalid, %zu materials, %u textures, %zu animations (cooked clips: %u)\n",
               modelId.c_str(),
               model.GetNodeCount(),
               model.GetMeshCount(),
               importedPrimitives,
               0,
               invalidPrimCount,
               model.GetMaterialCount(),
               mapped.MaterialsHeader ? mapped.MaterialsHeader->TextureRefCount : 0u,
               model.GetAnimationCount(),
               mapped.AnimationsHeader ? mapped.AnimationsHeader->ClipCount : 0u);
    }

    static std::string DeriveModelIdFromPath(const std::string& path)
    {
        std::string modelId = path;
        const std::size_t slash = modelId.find_last_of("/\\");
        if (slash != std::string::npos)
            modelId = modelId.substr(slash + 1);

        const std::size_t dot = modelId.rfind('.');
        if (dot != std::string::npos)
            modelId = modelId.substr(0, dot);

        return modelId;
    }

    static bool ReadFileBytes(const fs::path& filePath, std::vector<std::uint8_t>& outBytes, std::string& outError)
    {
        outBytes.clear();
        std::error_code sizeEc;
        const std::uintmax_t rawSize = fs::file_size(filePath, sizeEc);
        if (sizeEc)
        {
            outError = "Failed to query file size: " + filePath.string();
            return false;
        }

        if (rawSize == 0)
        {
            outError = "File is empty: " + filePath.string();
            return false;
        }

        if (rawSize > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
        {
            outError = "File is too large to map: " + filePath.string();
            return false;
        }

        const std::size_t size = static_cast<std::size_t>(rawSize);
        std::ifstream input(filePath, std::ios::binary);
        if (!input)
        {
            outError = "Failed to open file: " + filePath.string();
            return false;
        }

        outBytes.resize(size);
        std::size_t totalRead = 0;
        constexpr std::size_t kChunkSize = 8u * 1024u * 1024u;
        while (totalRead < size)
        {
            const std::size_t remaining = size - totalRead;
            const std::size_t toRead = std::min(remaining, kChunkSize);
            input.read(reinterpret_cast<char*>(outBytes.data() + totalRead), static_cast<std::streamsize>(toRead));
            const std::streamsize justRead = input.gcount();
            if (justRead <= 0)
            {
                outError = "Failed to read file bytes: " + filePath.string();
                outBytes.clear();
                return false;
            }

            totalRead += static_cast<std::size_t>(justRead);
        }

        return true;
    }

    static bool CheckRange(std::uint64_t offset, std::uint64_t size, std::size_t containerSize)
    {
        if (offset > static_cast<std::uint64_t>(containerSize))
            return false;
        if (size > static_cast<std::uint64_t>(containerSize))
            return false;
        return offset + size <= static_cast<std::uint64_t>(containerSize);
    }

    template <typename T>
    static bool MapChunkStruct(const CookedChunkView& chunk, std::uint64_t offset, const T*& outPtr, std::string& outError, const char* label)
    {
        outPtr = nullptr;
        if (!CheckRange(offset, sizeof(T), chunk.Size))
        {
            outError = std::string("Chunk ") + label + " struct range is out of bounds";
            return false;
        }

        outPtr = reinterpret_cast<const T*>(chunk.Data + static_cast<std::size_t>(offset));
        return true;
    }

    template <typename T>
    static bool MapChunkArray(const CookedChunkView& chunk, std::uint64_t offset, std::uint32_t count, const T*& outPtr, std::string& outError, const char* label)
    {
        outPtr = nullptr;
        const std::uint64_t bytes = static_cast<std::uint64_t>(count) * sizeof(T);
        if (!CheckRange(offset, bytes, chunk.Size))
        {
            outError = std::string("Chunk ") + label + " array range is out of bounds";
            return false;
        }

        outPtr = reinterpret_cast<const T*>(chunk.Data + static_cast<std::size_t>(offset));
        return true;
    }

    static bool ResolveCookedString(const CookedModelMappedView& mapped, std::uint32_t stringOffset, std::string& outString, std::string& outError)
    {
        outString.clear();
        if (stringOffset >= mapped.StringsChunk.Size)
        {
            outError = "Cooked string offset is out of bounds";
            return false;
        }

        const char* begin = reinterpret_cast<const char*>(mapped.StringsChunk.Data + stringOffset);
        const char* end = reinterpret_cast<const char*>(mapped.StringsChunk.Data + mapped.StringsChunk.Size);
        const char* cursor = begin;
        while (cursor < end && *cursor != '\0')
        {
            ++cursor;
        }

        if (cursor >= end)
        {
            outError = "Cooked string is not null-terminated";
            return false;
        }

        outString.assign(begin, cursor);
        return true;
    }

    static bool GetChunkViewByType(const CookedModelMappedView& mapped, CookedModelChunkType type, CookedChunkView& outView, std::string& outError)
    {
        outView = {};
        const std::uint32_t typeValue = static_cast<std::uint32_t>(type);
        for (const CookedModelChunkDesc& desc : mapped.Chunks)
        {
            if (desc.Type != typeValue)
                continue;

            if (!CheckRange(desc.Offset, desc.Size, mapped.FileBytes.size()))
            {
                outError = "Chunk payload is out of file bounds";
                return false;
            }

            outView.Desc = &desc;
            outView.Data = mapped.FileBytes.data() + static_cast<std::size_t>(desc.Offset);
            outView.Size = static_cast<std::size_t>(desc.Size);
            return true;
        }

        outError = "Missing required cooked chunk";
        return false;
    }

    static bool TryGetChunkViewByType(const CookedModelMappedView& mapped, CookedModelChunkType type, CookedChunkView& outView, std::string& outError)
    {
        outView = {};
        const std::uint32_t typeValue = static_cast<std::uint32_t>(type);
        for (const CookedModelChunkDesc& desc : mapped.Chunks)
        {
            if (desc.Type != typeValue)
                continue;

            if (!CheckRange(desc.Offset, desc.Size, mapped.FileBytes.size()))
            {
                outError = "Optional chunk payload is out of file bounds";
                return false;
            }

            outView.Desc = &desc;
            outView.Data = mapped.FileBytes.data() + static_cast<std::size_t>(desc.Offset);
            outView.Size = static_cast<std::size_t>(desc.Size);
            return true;
        }

        return true;
    }

    static bool MapCookedModelBinary(const fs::path& modelBinaryPath, CookedModelMappedView& outMapped, std::string& outError)
    {
        outMapped = {};
        if (!ReadFileBytes(modelBinaryPath, outMapped.FileBytes, outError))
            return false;

        if (outMapped.FileBytes.size() < sizeof(CookedModelHeader))
        {
            outError = "Cooked model file is too small for header";
            return false;
        }

        std::memcpy(&outMapped.Header, outMapped.FileBytes.data(), sizeof(CookedModelHeader));
        if (outMapped.Header.ChunkCount > (std::numeric_limits<std::uint64_t>::max() / sizeof(CookedModelChunkDesc)))
        {
            outError = "Cooked chunk table count overflows";
            return false;
        }

        const std::uint64_t chunkTableBytes = static_cast<std::uint64_t>(outMapped.Header.ChunkCount) * sizeof(CookedModelChunkDesc);
        if (!CheckRange(outMapped.Header.ChunkTableOffset, chunkTableBytes, outMapped.FileBytes.size()))
        {
            outError = "Cooked chunk table range is out of bounds";
            return false;
        }

        outMapped.Chunks.resize(static_cast<std::size_t>(outMapped.Header.ChunkCount));
        if (!outMapped.Chunks.empty())
        {
            std::memcpy(
                outMapped.Chunks.data(),
                outMapped.FileBytes.data() + static_cast<std::size_t>(outMapped.Header.ChunkTableOffset),
                static_cast<std::size_t>(chunkTableBytes));
        }

        std::string layoutError;
        if (!ValidateCookedModelLayout(outMapped.Header, outMapped.Chunks, outMapped.FileBytes.size(), &layoutError))
        {
            outError = "Cooked model layout validation failed: " + layoutError;
            return false;
        }

        if (!GetChunkViewByType(outMapped, CookedModelChunkType::Vertices, outMapped.VerticesChunk, outError)) return false;
        if (!GetChunkViewByType(outMapped, CookedModelChunkType::IndicesLOD, outMapped.IndicesChunk, outError)) return false;
        if (!GetChunkViewByType(outMapped, CookedModelChunkType::Meshes, outMapped.MeshesChunk, outError)) return false;
        if (!GetChunkViewByType(outMapped, CookedModelChunkType::Nodes, outMapped.NodesChunk, outError)) return false;
        if (!GetChunkViewByType(outMapped, CookedModelChunkType::Materials, outMapped.MaterialsChunk, outError)) return false;
        if (!GetChunkViewByType(outMapped, CookedModelChunkType::Strings, outMapped.StringsChunk, outError)) return false;

        if (!MapChunkStruct(outMapped.VerticesChunk, 0, outMapped.VertexHeader, outError, "vertices")) return false;
        if (!MapChunkArray(outMapped.VerticesChunk, outMapped.VertexHeader->PrimitiveTableOffset, outMapped.VertexHeader->PrimitiveCount, outMapped.VertexPrimitiveRecords, outError, "vertices table")) return false;
        if (!MapChunkArray(outMapped.VerticesChunk, outMapped.VertexHeader->VertexDataOffset, outMapped.VertexHeader->VertexCount, outMapped.Vertices, outError, "vertex stream")) return false;

        if (!MapChunkStruct(outMapped.IndicesChunk, 0, outMapped.IndexHeader, outError, "indices_lod")) return false;
        if (!MapChunkArray(outMapped.IndicesChunk, outMapped.IndexHeader->LodRecordOffset, outMapped.IndexHeader->LodRecordCount, outMapped.IndexLodRecords, outError, "lod table")) return false;
        if (!MapChunkArray(outMapped.IndicesChunk, outMapped.IndexHeader->IndexDataOffset, outMapped.IndexHeader->TotalIndexCount, outMapped.IndexData, outError, "index stream")) return false;

        if (!MapChunkStruct(outMapped.MeshesChunk, 0, outMapped.MeshesHeader, outError, "meshes")) return false;
        if (!MapChunkArray(outMapped.MeshesChunk, outMapped.MeshesHeader->MeshTableOffset, outMapped.MeshesHeader->MeshCount, outMapped.MeshRecords, outError, "mesh table")) return false;
        if (!MapChunkArray(outMapped.MeshesChunk, outMapped.MeshesHeader->PrimitiveTableOffset, outMapped.MeshesHeader->PrimitiveCount, outMapped.PrimitiveRecords, outError, "primitive table")) return false;

        if (!MapChunkArray(outMapped.NodesChunk, 0, outMapped.NodesChunk.Desc->ElementCount, outMapped.NodeRecords, outError, "node table")) return false;

        if (!MapChunkStruct(outMapped.MaterialsChunk, 0, outMapped.MaterialsHeader, outError, "materials")) return false;
        if (!MapChunkArray(outMapped.MaterialsChunk, outMapped.MaterialsHeader->MaterialTableOffset, outMapped.MaterialsHeader->MaterialCount, outMapped.MaterialRecords, outError, "material table")) return false;
        if (!MapChunkArray(outMapped.MaterialsChunk, outMapped.MaterialsHeader->TextureRefTableOffset, outMapped.MaterialsHeader->TextureRefCount, outMapped.TextureRefRecords, outError, "texture ref table")) return false;

        if (!TryGetChunkViewByType(outMapped, CookedModelChunkType::Animations, outMapped.AnimationsChunk, outError)) return false;
        if (outMapped.AnimationsChunk.Data && outMapped.AnimationsChunk.Size > 0)
        {
            if (!MapChunkStruct(outMapped.AnimationsChunk, 0, outMapped.AnimationsHeader, outError, "animations")) return false;
            if (!MapChunkArray(outMapped.AnimationsChunk, outMapped.AnimationsHeader->ClipTableOffset, outMapped.AnimationsHeader->ClipCount, outMapped.AnimationClipRecords, outError, "animation clip table")) return false;
            if (!MapChunkArray(outMapped.AnimationsChunk, outMapped.AnimationsHeader->SamplerTableOffset, outMapped.AnimationsHeader->SamplerCount, outMapped.AnimationSamplerRecords, outError, "animation sampler table")) return false;
            if (!MapChunkArray(outMapped.AnimationsChunk, outMapped.AnimationsHeader->ChannelTableOffset, outMapped.AnimationsHeader->ChannelCount, outMapped.AnimationChannelRecords, outError, "animation channel table")) return false;
            if (!MapChunkArray(outMapped.AnimationsChunk, outMapped.AnimationsHeader->TimeDataOffset, outMapped.AnimationsHeader->TimeKeyCount, outMapped.AnimationTimes, outError, "animation time keys")) return false;
            if (!MapChunkArray(outMapped.AnimationsChunk, outMapped.AnimationsHeader->TranslationDataOffset, outMapped.AnimationsHeader->TranslationKeyCount, outMapped.AnimationTranslations, outError, "animation translation keys")) return false;
            if (!MapChunkArray(outMapped.AnimationsChunk, outMapped.AnimationsHeader->RotationDataOffset, outMapped.AnimationsHeader->RotationKeyCount, outMapped.AnimationRotations, outError, "animation rotation keys")) return false;
            if (!MapChunkArray(outMapped.AnimationsChunk, outMapped.AnimationsHeader->ScaleDataOffset, outMapped.AnimationsHeader->ScaleKeyCount, outMapped.AnimationScales, outError, "animation scale keys")) return false;
        }

        return true;
    }

    static bool ValidateCookedRuntimeBinary(const fs::path& cookedBinaryPath)
    {
        std::error_code ec;
        const std::uint64_t fileSize = static_cast<std::uint64_t>(fs::file_size(cookedBinaryPath, ec));
        if (ec || fileSize < sizeof(CookedModelHeader))
        {
            std::cerr << "[glTF] Cooked runtime model is missing or too small: " << cookedBinaryPath.string() << "\n";
            return false;
        }

        std::ifstream input(cookedBinaryPath, std::ios::binary);
        if (!input)
        {
            std::cerr << "[glTF] Failed to open cooked runtime model: " << cookedBinaryPath.string() << "\n";
            return false;
        }

        CookedModelHeader header{};
        input.read(reinterpret_cast<char*>(&header), static_cast<std::streamsize>(sizeof(CookedModelHeader)));
        if (!input.good())
        {
            std::cerr << "[glTF] Failed to read cooked runtime model header: " << cookedBinaryPath.string() << "\n";
            return false;
        }

        const std::uint32_t knownRequiredMask = GetDefaultRequiredChunkMask();
        const bool hasUnknownRequiredFields = (header.RequiredChunkMask & ~knownRequiredMask) != 0;
        if (!IsCookedModelVersionSupported(header.VersionMajor, header.VersionMinor, hasUnknownRequiredFields))
        {
            std::cerr << "[glTF] Unsupported cooked model version " << header.VersionMajor << "." << header.VersionMinor
                      << " in " << cookedBinaryPath.string() << "\n";
            return false;
        }

        if (header.ChunkCount > (std::numeric_limits<std::uint64_t>::max() / sizeof(CookedModelChunkDesc)))
        {
            std::cerr << "[glTF] Cooked model chunk table count overflows: " << cookedBinaryPath.string() << "\n";
            return false;
        }

        const std::uint64_t chunkTableBytes = static_cast<std::uint64_t>(header.ChunkCount) * sizeof(CookedModelChunkDesc);
        if (header.ChunkTableOffset > fileSize || chunkTableBytes > fileSize || header.ChunkTableOffset + chunkTableBytes > fileSize)
        {
            std::cerr << "[glTF] Cooked model chunk table is out of bounds: " << cookedBinaryPath.string() << "\n";
            return false;
        }

        input.seekg(static_cast<std::streamoff>(header.ChunkTableOffset), std::ios::beg);
        std::vector<CookedModelChunkDesc> chunks(static_cast<size_t>(header.ChunkCount));
        if (!chunks.empty())
        {
            input.read(reinterpret_cast<char*>(chunks.data()), static_cast<std::streamsize>(chunkTableBytes));
            if (!input.good())
            {
                std::cerr << "[glTF] Failed to read cooked model chunk table: " << cookedBinaryPath.string() << "\n";
                return false;
            }
        }

        std::string validationError;
        if (!ValidateCookedModelLayout(header, chunks, fileSize, &validationError))
        {
            std::cerr << "[glTF] Invalid cooked runtime model layout (" << validationError << "): " << cookedBinaryPath.string() << "\n";
            return false;
        }

        return true;
    }

    static bool ValidateCookedManifestMetadata(const fs::path& cookedManifestPath)
    {
        std::ifstream input(cookedManifestPath);
        if (!input)
        {
            std::cerr << "[glTF] Failed to open cooked manifest: " << cookedManifestPath.string() << "\n";
            return false;
        }

        nlohmann::json manifest;
        try
        {
            input >> manifest;
        }
        catch (const nlohmann::json::exception& ex)
        {
            std::cerr << "[glTF] Failed to parse cooked manifest: " << ex.what() << "\n";
            return false;
        }

        if (!manifest.contains("runtimeFormat") || !manifest["runtimeFormat"].is_object())
        {
            std::cerr << "[glTF] Cooked manifest missing runtimeFormat block: " << cookedManifestPath.string() << "\n";
            return false;
        }

        const nlohmann::json& format = manifest["runtimeFormat"];
        const std::uint16_t versionMajor = format.value("versionMajor", static_cast<std::uint16_t>(0));
        const std::uint16_t versionMinor = format.value("versionMinor", static_cast<std::uint16_t>(0));
        const bool hasUnknownRequiredFields = false;
        if (!IsCookedModelVersionSupported(versionMajor, versionMinor, hasUnknownRequiredFields))
        {
            std::cerr << "[glTF] Unsupported cooked manifest version " << versionMajor << "." << versionMinor
                      << " in " << cookedManifestPath.string() << "\n";
            return false;
        }

        if (!manifest.contains("chunkSizes") || !manifest["chunkSizes"].is_object())
        {
            std::cerr << "[glTF] Cooked manifest missing chunkSizes block: " << cookedManifestPath.string() << "\n";
            return false;
        }

        const nlohmann::json& chunkSizes = manifest["chunkSizes"];
        constexpr std::array<const char*, 6> kRequiredChunkNames = {
            "vertices",
            "indicesLod",
            "meshes",
            "nodes",
            "materials",
            "strings"
        };

        for (const char* chunkName : kRequiredChunkNames)
        {
            if (!chunkSizes.contains(chunkName))
            {
                std::cerr << "[glTF] Cooked manifest missing required chunk entry '" << chunkName
                          << "': " << cookedManifestPath.string() << "\n";
                return false;
            }
        }

        return true;
    }

    static AlphaMode ConvertCookedAlphaMode(std::uint32_t alphaMode)
    {
        switch (alphaMode)
        {
        case 1: return AlphaMode::Masked;
        case 2: return AlphaMode::Blend;
        default: return AlphaMode::Opaque;
        }
    }

    static bool ResolveCookedTextureReferencePath(
        const CookedModelMappedView& mapped,
        std::uint32_t textureRefId,
        std::string& outPath,
        std::string& outError)
    {
        outPath.clear();
        if (textureRefId == std::numeric_limits<std::uint32_t>::max())
            return true;

        if (textureRefId >= mapped.MaterialsHeader->TextureRefCount)
        {
            outError = "Cooked texture reference index is out of range";
            return false;
        }

        const CookedTextureRefRecordDisk& textureRef = mapped.TextureRefRecords[textureRefId];
        return ResolveCookedString(mapped, textureRef.PathStringOffset, outPath, outError);
    }

    static std::shared_ptr<Texture> LoadCookedDdsTexture(
        const std::string& texturePath,
        TextureLoader& textureLoader,
        std::unordered_map<std::string, std::shared_ptr<Texture>>& textureCache,
        std::string& outError)
    {
        outError.clear();
        if (texturePath.empty())
            return nullptr;

        const auto cacheIt = textureCache.find(texturePath);
        if (cacheIt != textureCache.end())
            return cacheIt->second;

        fs::path resolvedPath = fs::path(texturePath);
        if (!resolvedPath.is_absolute())
            resolvedPath = fs::path("res") / resolvedPath;

        const std::string ext = resolvedPath.extension().string();
        if (ext != ".dds" && ext != ".DDS")
        {
            outError = "Cooked texture reference is not a DDS file: " + resolvedPath.string();
            return nullptr;
        }

        if (!fs::exists(resolvedPath))
        {
            outError = "Cooked DDS texture does not exist: " + resolvedPath.string();
            return nullptr;
        }

        try
        {
            std::shared_ptr<Texture> texture(textureLoader.LoadDDS(resolvedPath.wstring()));
            if (!texture)
            {
                outError = "Failed to create cooked DDS texture resource: " + resolvedPath.string();
                return nullptr;
            }

            textureCache.emplace(texturePath, texture);
            return texture;
        }
        catch (const std::exception& ex)
        {
            outError = "Failed to load cooked DDS texture: " + resolvedPath.string() + " (" + ex.what() + ")";
            return nullptr;
        }
    }

    static bool CopyCookedIndices(
        const CookedModelMappedView& mapped,
        const CookedIndexLodRecordDisk& lodRecord,
        std::uint32_t primitiveVertexCount,
        std::vector<UINT>& outIndices,
        std::string& outError)
    {
        outIndices.clear();
        if (primitiveVertexCount == 0)
        {
            outError = "Cooked primitive has zero vertices";
            return false;
        }

        if (lodRecord.IndexCount == 0)
        {
            outError = "Cooked LOD record has zero indices";
            return false;
        }

        if ((lodRecord.IndexCount % 3u) != 0u)
        {
            outError = "Cooked LOD record index count is not a triangle multiple";
            return false;
        }

        if (lodRecord.IndexOffset > mapped.IndexHeader->TotalIndexCount)
        {
            outError = "LOD index offset is out of range";
            return false;
        }

        if (lodRecord.IndexCount > mapped.IndexHeader->TotalIndexCount - lodRecord.IndexOffset)
        {
            outError = "LOD index range exceeds cooked index stream";
            return false;
        }

        outIndices.resize(static_cast<std::size_t>(lodRecord.IndexCount));
        std::vector<std::uint32_t> indices32(outIndices.size());
        for (std::size_t i = 0; i < outIndices.size(); ++i)
        {
            const std::uint32_t idx = mapped.IndexData[lodRecord.IndexOffset + i];
            indices32[i] = idx;
            outIndices[i] = static_cast<UINT>(idx);
        }

        if (!ValidateIndicesInRange(indices32, primitiveVertexCount))
        {
            outError = "LOD index range validation failed against primitive vertex count";
            outIndices.clear();
            return false;
        }

        return true;
    }

    static std::shared_ptr<ModelAsset> BuildModelAssetFromCooked(
        const std::string& modelId,
        const CookedModelMappedView& mapped,
        std::string& outError)
    {
        auto outModel = std::make_shared<ModelAsset>(modelId);

        std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache;
        TextureLoader textureLoader;

        const std::shared_ptr<Texture> defaultAlbedo = CreateSolidTexture(255, 0, 255, 255);
        const std::shared_ptr<Texture> defaultNormal = CreateSolidTexture(128, 128, 255, 255);
        const std::shared_ptr<Texture> defaultAO = CreateSolidTexture(255, 255, 255, 255);
        const std::shared_ptr<Texture> defaultEmissive = CreateSolidTexture(0, 0, 0, 255);

        for (std::uint32_t materialIndex = 0; materialIndex < mapped.MaterialsHeader->MaterialCount; ++materialIndex)
        {
            const CookedMaterialRecordDisk& materialRecord = mapped.MaterialRecords[materialIndex];

            std::string materialName;
            std::string stringError;
            if (!ResolveCookedString(mapped, materialRecord.NameStringOffset, materialName, stringError) || materialName.empty())
                materialName = "material_" + std::to_string(materialIndex);

            auto pbrMaterial = std::make_shared<PBRMaterial>();
            auto materialTemplate = std::make_shared<MaterialTemplate>();

            pbrMaterial->SetAlbedo({
                materialRecord.BaseColorFactor[0],
                materialRecord.BaseColorFactor[1],
                materialRecord.BaseColorFactor[2] });
            pbrMaterial->SetBaseColorAlpha(materialRecord.BaseColorFactor[3]);
            pbrMaterial->SetMetallic(materialRecord.Metallic);
            pbrMaterial->SetRoughness(materialRecord.Roughness);
            pbrMaterial->SetEmissive({
                materialRecord.EmissiveFactor[0],
                materialRecord.EmissiveFactor[1],
                materialRecord.EmissiveFactor[2] });
            pbrMaterial->SetAlphaMode(static_cast<int>(materialRecord.AlphaMode));
            pbrMaterial->SetAlphaCutoff(materialRecord.AlphaCutoff);

            const AlphaMode alphaMode = ConvertCookedAlphaMode(materialRecord.AlphaMode);
            materialTemplate->SetBlendPolicy(alphaMode);
            materialTemplate->SetPassTarget(alphaMode == AlphaMode::Blend ? PassTarget::Transparent : PassTarget::Geometry);

            RasterizerPolicy rasterPolicy;
            if (materialRecord.DoubleSided != 0)
                rasterPolicy.CullMode = D3D12_CULL_MODE_NONE;
            materialTemplate->SetRasterizerPolicy(rasterPolicy);

            auto resolveTextureMap = [&](std::uint32_t textureRefId) -> std::shared_ptr<Texture>
            {
                std::string textureRefPath;
                std::string textureError;
                if (!ResolveCookedTextureReferencePath(mapped, textureRefId, textureRefPath, textureError))
                {
                    std::cerr << "[CookedModel] " << textureError << "\n";
                    return nullptr;
                }

                if (textureRefPath.empty())
                    return nullptr;

                std::shared_ptr<Texture> loadedTexture = LoadCookedDdsTexture(textureRefPath, textureLoader, textureCache, textureError);
                if (!loadedTexture)
                    std::cerr << "[CookedModel] " << textureError << "\n";
                return loadedTexture;
            };

            std::shared_ptr<Texture> albedoMap = resolveTextureMap(materialRecord.TextureRefIds[0]);
            pbrMaterial->SetAlbedoMap(albedoMap ? albedoMap : defaultAlbedo, albedoMap != nullptr);

            std::shared_ptr<Texture> normalMap = resolveTextureMap(materialRecord.TextureRefIds[1]);
            pbrMaterial->SetNormalMap(normalMap ? normalMap : defaultNormal, normalMap != nullptr);

            std::shared_ptr<Texture> metallicRoughnessMap = resolveTextureMap(materialRecord.TextureRefIds[2]);
            if (metallicRoughnessMap)
            {
                pbrMaterial->SetMetallicMap(metallicRoughnessMap);
                pbrMaterial->SetRoughnessMap(metallicRoughnessMap);
            }
            else
            {
                const float roughnessClamped = (std::max)(0.0f, (std::min)(1.0f, materialRecord.Roughness));
                const float metallicClamped = (std::max)(0.0f, (std::min)(1.0f, materialRecord.Metallic));
                const std::uint8_t roughnessByte = static_cast<std::uint8_t>(roughnessClamped * 255.0f + 0.5f);
                const std::uint8_t metallicByte = static_cast<std::uint8_t>(metallicClamped * 255.0f + 0.5f);
                std::shared_ptr<Texture> packedMaterialMap = CreateSolidTexture(0, roughnessByte, metallicByte, 255);
                pbrMaterial->SetMetallicMap(packedMaterialMap);
                pbrMaterial->SetRoughnessMap(packedMaterialMap);
            }

            std::shared_ptr<Texture> aoMap = resolveTextureMap(materialRecord.TextureRefIds[3]);
            pbrMaterial->SetAOMap(aoMap ? aoMap : defaultAO, aoMap != nullptr);

            std::shared_ptr<Texture> emissiveMap = resolveTextureMap(materialRecord.TextureRefIds[4]);
            pbrMaterial->SetEmissiveMap(emissiveMap ? emissiveMap : defaultEmissive, emissiveMap != nullptr);

            auto materialAsset = std::make_shared<MaterialAsset>(materialName, pbrMaterial);
            materialAsset->SetTemplate(materialTemplate);
            materialAsset->SetAlphaCutoff(materialRecord.AlphaCutoff);
            materialAsset->SetDoubleSided(materialRecord.DoubleSided != 0);
            materialAsset->SetAlphaMode(alphaMode);

            outModel->AddMaterial(materialAsset);
        }

        std::size_t fallbackMaterialIndex = 0;
        if (outModel->GetMaterialCount() == 0)
        {
            auto fallbackPbr = std::make_shared<PBRMaterial>();
            fallbackPbr->SetAlbedo({ 1.0f, 0.0f, 1.0f });
            auto fallbackAsset = std::make_shared<MaterialAsset>("default_material", fallbackPbr);
            fallbackMaterialIndex = outModel->AddMaterial(fallbackAsset);
        }

        std::vector<const CookedVertexPrimitiveRecordDisk*> vertexRecordsByPrimitive(
            static_cast<std::size_t>(mapped.MeshesHeader->PrimitiveCount), nullptr);

        for (std::uint32_t i = 0; i < mapped.VertexHeader->PrimitiveCount; ++i)
        {
            const CookedVertexPrimitiveRecordDisk& vertexRecord = mapped.VertexPrimitiveRecords[i];
            if (vertexRecord.PrimitiveIndex >= vertexRecordsByPrimitive.size())
            {
                outError = "Cooked vertex primitive mapping references an invalid primitive index";
                return nullptr;
            }

            if (vertexRecordsByPrimitive[vertexRecord.PrimitiveIndex] != nullptr)
            {
                outError = "Cooked vertex primitive mapping contains duplicate primitive indices";
                return nullptr;
            }

            vertexRecordsByPrimitive[vertexRecord.PrimitiveIndex] = &vertexRecord;
        }

        for (std::uint32_t meshIndex = 0; meshIndex < mapped.MeshesHeader->MeshCount; ++meshIndex)
        {
            const CookedMeshRecordDisk& meshRecord = mapped.MeshRecords[meshIndex];

            std::string meshName;
            std::string stringError;
            if (!ResolveCookedString(mapped, meshRecord.NameStringOffset, meshName, stringError) || meshName.empty())
                meshName = "mesh_" + std::to_string(meshIndex);

            if (meshRecord.PrimitiveStart > mapped.MeshesHeader->PrimitiveCount ||
                meshRecord.PrimitiveCount > mapped.MeshesHeader->PrimitiveCount - meshRecord.PrimitiveStart)
            {
                outError = "Cooked mesh primitive range is out of bounds";
                return nullptr;
            }

            auto meshAsset = std::make_shared<MeshAsset>(meshName);

            for (std::uint32_t localPrimitive = 0; localPrimitive < meshRecord.PrimitiveCount; ++localPrimitive)
            {
                const std::uint32_t globalPrimitiveIndex = meshRecord.PrimitiveStart + localPrimitive;
                const CookedPrimitiveRecordDisk& primitiveRecord = mapped.PrimitiveRecords[globalPrimitiveIndex];

                const CookedVertexPrimitiveRecordDisk* vertexRecord = vertexRecordsByPrimitive[globalPrimitiveIndex];
                if (!vertexRecord)
                {
                    outError = "Cooked primitive is missing a vertex stream mapping";
                    return nullptr;
                }

                if (vertexRecord->VertexOffset != primitiveRecord.VertexOffset ||
                    vertexRecord->VertexCount != primitiveRecord.VertexCount)
                {
                    outError = "Cooked primitive vertex range mismatch between mesh and vertex chunks";
                    return nullptr;
                }

                if (primitiveRecord.VertexOffset > mapped.VertexHeader->VertexCount ||
                    primitiveRecord.VertexCount > mapped.VertexHeader->VertexCount - primitiveRecord.VertexOffset)
                {
                    outError = "Cooked primitive vertex range is out of bounds";
                    return nullptr;
                }
                if (primitiveRecord.VertexCount == 0)
                {
                    outError = "Cooked primitive has zero vertices";
                    return nullptr;
                }

                if (primitiveRecord.LodStart > mapped.IndexHeader->LodRecordCount ||
                    primitiveRecord.LodCount > mapped.IndexHeader->LodRecordCount - primitiveRecord.LodStart)
                {
                    outError = "Cooked primitive LOD range is out of bounds";
                    return nullptr;
                }

                std::vector<Vertex> vertices(primitiveRecord.VertexCount);
                std::memcpy(
                    vertices.data(),
                    mapped.Vertices + primitiveRecord.VertexOffset,
                    vertices.size() * sizeof(Vertex));

                const CookedIndexLodRecordDisk* baseLod = nullptr;
                std::vector<const CookedIndexLodRecordDisk*> extraLods;

                for (std::uint32_t lodIndex = 0; lodIndex < primitiveRecord.LodCount; ++lodIndex)
                {
                    const CookedIndexLodRecordDisk& lodRecord = mapped.IndexLodRecords[primitiveRecord.LodStart + lodIndex];
                    if (lodRecord.PrimitiveIndex != globalPrimitiveIndex)
                    {
                        outError = "Cooked LOD record primitive mapping mismatch";
                        return nullptr;
                    }

                    if (lodRecord.LodLevel == 0 && !baseLod)
                        baseLod = &lodRecord;
                    else
                        extraLods.push_back(&lodRecord);
                }

                if (!baseLod)
                {
                    outError = "Cooked primitive is missing LOD0 index data";
                    return nullptr;
                }

                std::vector<UINT> baseIndices;
                if (!CopyCookedIndices(mapped, *baseLod, primitiveRecord.VertexCount, baseIndices, outError))
                    return nullptr;

                std::size_t materialIndex = primitiveRecord.MaterialIndex;
                if (materialIndex >= outModel->GetMaterialCount())
                    materialIndex = fallbackMaterialIndex;

                auto baseVertexBuffer = ResourceManager::GetInstance().CreateVertexBuffer(vertices);
                if (!baseVertexBuffer)
                {
                    outError = "Failed to create GPU vertex buffer for cooked primitive";
                    return nullptr;
                }

                auto baseIndexBuffer = ResourceManager::GetInstance().CreateIndexBuffer(baseIndices);
                if (!baseIndexBuffer)
                {
                    outError = "Failed to create GPU index buffer for cooked primitive LOD0";
                    return nullptr;
                }

                MeshPrimitive primitive(
                    std::move(baseVertexBuffer),
                    std::move(baseIndexBuffer),
                    static_cast<UINT>(baseIndices.size()),
                    0,
                    0,
                    static_cast<UINT>(materialIndex));
                primitive.SetBounds({ primitiveRecord.Min, primitiveRecord.Max });

                for (const CookedIndexLodRecordDisk* extraLod : extraLods)
                {
                    if (!extraLod || extraLod->LodLevel == 0)
                        continue;

                    std::vector<UINT> lodIndices;
                    if (!CopyCookedIndices(mapped, *extraLod, primitiveRecord.VertexCount, lodIndices, outError))
                        return nullptr;

                    std::unique_ptr<IndexBuffer> lodIndexBuffer = ResourceManager::GetInstance().CreateIndexBuffer(lodIndices);
                    if (!lodIndexBuffer)
                    {
                        outError = "Failed to create GPU index buffer for cooked LOD";
                        return nullptr;
                    }

                    primitive.AddLODBuffer(std::move(lodIndexBuffer), static_cast<UINT>(lodIndices.size()), extraLod->Ratio, extraLod->Error);
                }
                primitive.SetActiveLOD(0);
                meshAsset->AddPrimitive(std::move(primitive));
            }
            outModel->AddMesh(meshAsset);
        }

        const std::size_t nodeCount = mapped.NodesChunk.Desc ? mapped.NodesChunk.Desc->ElementCount : 0;
        for (std::size_t nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex)
        {
            const CookedNodeRecordDisk& nodeRecord = mapped.NodeRecords[nodeIndex];

            ModelNode node;
            std::string nodeName;
            std::string stringError;
            if (ResolveCookedString(mapped, nodeRecord.NameStringOffset, nodeName, stringError) && !nodeName.empty())
                node.Name = nodeName;
            else
                node.Name = "node_" + std::to_string(nodeIndex);

            if (nodeRecord.ParentIndex >= 0 && static_cast<std::size_t>(nodeRecord.ParentIndex) < nodeCount)
                node.ParentIndex = nodeRecord.ParentIndex;
            else
                node.ParentIndex = -1;

            if (nodeRecord.MeshIndex >= 0 && static_cast<std::size_t>(nodeRecord.MeshIndex) < outModel->GetMeshCount())
                node.MeshIndex = nodeRecord.MeshIndex;
            else
                node.MeshIndex = -1;

            std::memcpy(&node.LocalTransform, nodeRecord.LocalTransform.data(), sizeof(node.LocalTransform));
            outModel->AddNode(node);
        }

        if (mapped.AnimationsHeader)
        {
            for (std::uint32_t clipIndex = 0; clipIndex < mapped.AnimationsHeader->ClipCount; ++clipIndex)
            {
                const CookedAnimationClipRecordDisk& clipRecord = mapped.AnimationClipRecords[clipIndex];

                if (clipRecord.SamplerStart > mapped.AnimationsHeader->SamplerCount ||
                    clipRecord.SamplerCount > mapped.AnimationsHeader->SamplerCount - clipRecord.SamplerStart)
                {
                    outError = "Cooked animation sampler range is out of bounds";
                    return nullptr;
                }

                if (clipRecord.ChannelStart > mapped.AnimationsHeader->ChannelCount ||
                    clipRecord.ChannelCount > mapped.AnimationsHeader->ChannelCount - clipRecord.ChannelStart)
                {
                    outError = "Cooked animation channel range is out of bounds";
                    return nullptr;
                }

                AnimationClip clip;
                std::string clipName;
                std::string stringError;
                if (ResolveCookedString(mapped, clipRecord.NameStringOffset, clipName, stringError) && !clipName.empty())
                    clip.Name = clipName;
                else
                    clip.Name = "animation_" + std::to_string(clipIndex);
                clip.Duration = clipRecord.Duration;
                clip.Samplers.resize(clipRecord.SamplerCount);

                for (std::uint32_t localSampler = 0; localSampler < clipRecord.SamplerCount; ++localSampler)
                {
                    const CookedAnimationSamplerRecordDisk& samplerRecord = mapped.AnimationSamplerRecords[clipRecord.SamplerStart + localSampler];
                    AnimationSampler& sampler = clip.Samplers[localSampler];

                    switch (samplerRecord.Interpolation)
                    {
                    case 1: sampler.Interpolation = AnimationInterpolation::Step; break;
                    case 2: sampler.Interpolation = AnimationInterpolation::CubicSpline; break;
                    default: sampler.Interpolation = AnimationInterpolation::Linear; break;
                    }

                    const std::uint32_t keyCount = samplerRecord.KeyCount;
                    if (samplerRecord.TimeOffset > mapped.AnimationsHeader->TimeKeyCount ||
                        keyCount > mapped.AnimationsHeader->TimeKeyCount - samplerRecord.TimeOffset)
                    {
                        outError = "Cooked animation time key range is out of bounds";
                        return nullptr;
                    }

                    sampler.Times.resize(keyCount);
                    std::memcpy(
                        sampler.Times.data(),
                        mapped.AnimationTimes + samplerRecord.TimeOffset,
                        static_cast<size_t>(keyCount) * sizeof(float));

                    if (samplerRecord.Path == 0)
                    {
                        if (samplerRecord.ValueOffset > mapped.AnimationsHeader->TranslationKeyCount ||
                            keyCount > mapped.AnimationsHeader->TranslationKeyCount - samplerRecord.ValueOffset)
                        {
                            outError = "Cooked animation translation key range is out of bounds";
                            return nullptr;
                        }

                        sampler.Translations.resize(keyCount);
                        std::memcpy(
                            sampler.Translations.data(),
                            mapped.AnimationTranslations + samplerRecord.ValueOffset,
                            static_cast<size_t>(keyCount) * sizeof(DirectX::XMFLOAT3));
                    }
                    else if (samplerRecord.Path == 1)
                    {
                        if (samplerRecord.ValueOffset > mapped.AnimationsHeader->RotationKeyCount ||
                            keyCount > mapped.AnimationsHeader->RotationKeyCount - samplerRecord.ValueOffset)
                        {
                            outError = "Cooked animation rotation key range is out of bounds";
                            return nullptr;
                        }

                        sampler.Rotations.resize(keyCount);
                        std::memcpy(
                            sampler.Rotations.data(),
                            mapped.AnimationRotations + samplerRecord.ValueOffset,
                            static_cast<size_t>(keyCount) * sizeof(DirectX::XMFLOAT4));
                    }
                    else if (samplerRecord.Path == 2)
                    {
                        if (samplerRecord.ValueOffset > mapped.AnimationsHeader->ScaleKeyCount ||
                            keyCount > mapped.AnimationsHeader->ScaleKeyCount - samplerRecord.ValueOffset)
                        {
                            outError = "Cooked animation scale key range is out of bounds";
                            return nullptr;
                        }

                        sampler.Scales.resize(keyCount);
                        std::memcpy(
                            sampler.Scales.data(),
                            mapped.AnimationScales + samplerRecord.ValueOffset,
                            static_cast<size_t>(keyCount) * sizeof(DirectX::XMFLOAT3));
                    }
                }

                for (std::uint32_t localChannel = 0; localChannel < clipRecord.ChannelCount; ++localChannel)
                {
                    const CookedAnimationChannelRecordDisk& channelRecord = mapped.AnimationChannelRecords[clipRecord.ChannelStart + localChannel];
                    if (channelRecord.NodeIndex >= outModel->GetNodeCount())
                        continue;
                    if (channelRecord.SamplerIndex >= clip.Samplers.size())
                        continue;

                    AnimationChannel channel;
                    channel.NodeIndex = channelRecord.NodeIndex;
                    channel.SamplerIndex = channelRecord.SamplerIndex;
                    switch (channelRecord.Path)
                    {
                    case 1: channel.Path = AnimationPath::Rotation; break;
                    case 2: channel.Path = AnimationPath::Scale; break;
                    default: channel.Path = AnimationPath::Translation; break;
                    }
                    clip.Channels.push_back(channel);
                }

                if (!clip.Channels.empty())
                    outModel->AddAnimation(clip);
            }
        }
        return outModel;
    }

    std::shared_ptr<ModelAsset> ModelLoader::LoadCookedModel(const std::string& modelId)
    {
        if (modelId.empty())
        {
            std::cerr << "[CookedModel] Cannot load cooked model with an empty model id.\n";
            return nullptr;
        }

        const fs::path cookedModelRoot = fs::path(ResourceManager::GetCookedModelPath(modelId));
        const fs::path cookedBinaryPath = cookedModelRoot / "model.dxmd";
        if (!fs::exists(cookedBinaryPath))
        {
            std::cerr << "[CookedModel] Cooked model binary is missing for model id '" << modelId
                      << "' at " << cookedBinaryPath.string() << "\n";
            return nullptr;
        }

        const fs::path cookedManifestPath = cookedModelRoot / "manifest.json";
        if (fs::exists(cookedManifestPath) && !ValidateCookedManifestMetadata(cookedManifestPath))
        {
            std::cerr << "[CookedModel] Cooked manifest metadata validation failed for " << cookedManifestPath.string() << "\n";
            return nullptr;
        }

        CookedModelMappedView mapped;
        std::string mapError;
        if (!MapCookedModelBinary(cookedBinaryPath, mapped, mapError))
        {
            std::cerr << "[CookedModel] Failed to map cooked model binary: " << mapError << "\n";
            return nullptr;
        }

        std::string buildError;
        std::shared_ptr<ModelAsset> cookedModel = BuildModelAssetFromCooked(modelId, mapped, buildError);
        if (!cookedModel)
        {
            std::cerr << "[CookedModel] Failed to build runtime model from cooked chunks: " << buildError << "\n";
            return nullptr;
        }

        ValidateAndLogCooked(modelId, mapped, *cookedModel);

        return cookedModel;
    }

    void ModelLoader::TryApplyCookedMeshLods(const std::string& modelName, ModelAsset& modelAsset)
    {
        const fs::path lodManifestPath = fs::path(ResourceManager::GetCookedModelLodsPath(modelName));
        if (!fs::exists(lodManifestPath)) return;

        const fs::path cookedManifestPath = lodManifestPath.parent_path() / "manifest.json";
        if (fs::exists(cookedManifestPath) && !ValidateCookedManifestMetadata(cookedManifestPath))
        {
            std::cerr << "[glTF] Skipping cooked LOD application due to invalid manifest metadata.\n";
            return;
        }

        const fs::path cookedBinaryPath = lodManifestPath.parent_path() / "model.dxmd";
        if (fs::exists(cookedBinaryPath) && !ValidateCookedRuntimeBinary(cookedBinaryPath))
        {
            std::cerr << "[glTF] Skipping cooked LOD application due to invalid runtime binary validation.\n";
            return;
        }

        std::ifstream manifestInput(lodManifestPath);
        if (!manifestInput)
        {
            std::cerr << "[glTF] Failed to open cooked LOD manifest: " << lodManifestPath.string() << "\n";
            return;
        }

        std::string jsonText((std::istreambuf_iterator<char>(manifestInput)), std::istreambuf_iterator<char>());
        std::vector<CookedPrimitiveLodRecord> lodRecords;
        if (!ParseCookedLodManifest(jsonText, lodRecords))
        {
            std::cerr << "[glTF] Failed to parse cooked LOD manifest: " << lodManifestPath.string() << "\n";
            return;
        }

        int appliedBuffers = 0;
        for (const CookedPrimitiveLodRecord& primRecord : lodRecords)
        {
            if (primRecord.meshIndex < 0 || primRecord.primitiveIndex < 0) continue;

            MeshAsset* meshAsset = modelAsset.GetMesh(static_cast<size_t>(primRecord.meshIndex));
            if (!meshAsset) continue;

            MeshPrimitive* primitive = meshAsset->GetPrimitive(static_cast<size_t>(primRecord.primitiveIndex));
            if (!primitive || !primitive->HasGeometry()) continue;

            if (!primRecord.skipped && primRecord.lods.empty())
            {
                std::cerr << "[glTF] Missing required LOD entries for mesh " << primRecord.meshIndex
                          << " primitive " << primRecord.primitiveIndex << " in " << lodManifestPath.string() << "\n";
                continue;
            }

            if (primRecord.vertexCount == 0 || primRecord.vertexCount > static_cast<std::uint64_t>(std::numeric_limits<UINT>::max()))
            {
                std::cerr << "[glTF] Invalid cooked vertexCount for mesh " << primRecord.meshIndex
                          << " primitive " << primRecord.primitiveIndex << " in " << lodManifestPath.string() << "\n";
                continue;
            }

            const UINT primitiveVertexCount = static_cast<UINT>(primRecord.vertexCount);

            primitive->ClearAdditionalLODs();
            for (const CookedLodLevelRecord& lodRecord : primRecord.lods)
            {
                if (lodRecord.level <= 0 || lodRecord.output.empty()) continue;

                fs::path lodBinaryPath = fs::path(lodRecord.output);
                if (!lodBinaryPath.is_absolute())
                    lodBinaryPath = "res" / lodBinaryPath;

                std::vector<UINT> lodIndices;
                std::uint64_t lodFileBytes = 0;
                if (!ReadCookedIndexBuffer(lodBinaryPath, lodIndices, lodFileBytes))
                {
                    std::cerr << "[glTF] Failed to read cooked LOD indices: " << lodBinaryPath.string() << "\n";
                    continue;
                }

                if (lodRecord.indexCount != 0 && lodRecord.indexCount != lodIndices.size())
                {
                    std::cerr << "[glTF] LOD index count mismatch for " << lodBinaryPath.string()
                              << " (declared=" << lodRecord.indexCount << ", actual=" << lodIndices.size() << ")\n";
                    continue;
                }

                const std::uint64_t expectedBytes = static_cast<std::uint64_t>(lodIndices.size()) * sizeof(UINT);
                if (expectedBytes != lodFileBytes)
                {
                    std::cerr << "[glTF] LOD binary size validation failed for " << lodBinaryPath.string() << "\n";
                    continue;
                }

                if (lodIndices.empty())
                {
                    std::cerr << "[glTF] Skipping empty cooked LOD index buffer for " << lodBinaryPath.string() << "\n";
                    continue;
                }

                if (!ValidateIndicesInRange(lodIndices, primitiveVertexCount))
                {
                    std::cerr << "[glTF] LOD index range validation failed for " << lodBinaryPath.string()
                              << " (vertexCount=" << primitiveVertexCount << ")\n";
                    continue;
                }

                auto lodIndexBuffer = ResourceManager::GetInstance().CreateIndexBuffer(lodIndices);
                if (!lodIndexBuffer)
                {
                    std::cerr << "[glTF] Failed to create GPU index buffer for LOD: " << lodBinaryPath.string() << "\n";
                    continue;
                }

                if (primitive->AddLODBuffer(std::move(lodIndexBuffer), static_cast<UINT>(lodIndices.size()), lodRecord.ratio, lodRecord.error))
                    ++appliedBuffers;
            }
            primitive->SetActiveLOD(0);
        }
        if (appliedBuffers > 0)
            std::cout << "[glTF] Applied " << appliedBuffers << " cooked LOD index buffers from " << lodManifestPath.string() << "\n";
    }

    // -------------------------------------------------------------------------
    // Public entry point
    // -------------------------------------------------------------------------

    void ModelLoader::SetCookedFallbackMode(CookedFallbackMode mode)
    {
        g_CookedFallbackMode = mode;
    }

    ModelLoader::CookedFallbackMode ModelLoader::GetCookedFallbackMode()
    {
        return g_CookedFallbackMode;
    }

    std::shared_ptr<ModelAsset> ModelLoader::LoadGlb(const std::string& filename)
    {
        const std::string modelId = DeriveModelIdFromPath(filename);
        const bool allowGlbFallback = ShouldAllowGlbFallback(GetCookedFallbackMode());

        if (!modelId.empty())
        {
            std::shared_ptr<ModelAsset> cookedModel = LoadCookedModel(modelId);
            if (cookedModel)
            {
                std::cout << "[CookedModel] Loaded cooked model: " << modelId << "\n";
                return cookedModel;
            }

            if (!allowGlbFallback)
            {
                std::cerr << "[CookedModel] Strict cooked loading is enabled and cooked load failed for model '"
                          << modelId << "'. Aborting without glTF fallback.\n";
                return nullptr;
            }

            std::cerr << "[CookedModel] Falling back to source glTF load for model '" << modelId << "'.\n";
        }

        tinygltf::Model glbModel;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;
        bool ret = loader.LoadBinaryFromFile(&glbModel, &err, &warn, filename);

        if (!warn.empty())
            printf("[glTF] Warn: %s\n", warn.c_str());
        if (!err.empty())
            printf("[glTF] Err: %s\n", err.c_str());
        if (!ret)
        {
            printf("[glTF] Failed to parse: %s\n", filename.c_str());
            return nullptr;
        }

        // Derive model name from filename.
        const std::string modelName = modelId.empty() ? DeriveModelIdFromPath(filename) : modelId;

        GltfImportContext ctx{ glbModel, std::make_shared<ModelAsset>(modelName) };
        ctx.nodeMap.assign(glbModel.nodes.size(), -1);

        // Create default 1x1 fallback textures.
        ctx.defaultAlbedo     = CreateSolidTexture(255, 0, 255, 255);
        ctx.defaultNormal     = CreateSolidTexture(128, 128, 255, 255);
        ctx.defaultMetalRough = CreateSolidTexture(0, 200, 0, 255);
        ctx.defaultAO         = CreateSolidTexture(255, 255, 255, 255);
        ctx.defaultEmissive   = CreateSolidTexture(0, 0, 0, 255);

        ImportTextures(ctx);
        ImportMaterials(ctx);
        ImportMeshes(ctx);
        TryApplyCookedMeshLods(modelName, *ctx.outModel);
        ImportNodes(ctx);
        ImportAnimations(ctx);
        ValidateAndLog(ctx);

        return ctx.outModel;
    }
}
