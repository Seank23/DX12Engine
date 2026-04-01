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
#include <string>
#include <stdexcept>
#include <iostream>
#include <unordered_map>
#include <cfloat>
#include <cmath>
#include "../Asset/MeshAsset.h"
#include "../Asset/ModelAsset.h"
#include "../Asset/MaterialAsset.h"
#include "../Asset/MaterialTemplate.h"
#include "../Resources/ResourceManager.h"
#include "../Resources/Materials/PBRMaterial.h"
#include <DirectXTex.h>

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

    // -------------------------------------------------------------------------
    // Step 1 – textures
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
                std::cerr << "[glTF] Texture " << ti << " has no decoded pixel data – skipped.\n";
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
    // Step 2 – materials
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
            if (albedoTex) pbrMat->SetAlbedoMap(albedoTex);

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
            pbrMat->SetAOMap(aoTex ? aoTex : ctx.defaultAO);

            std::shared_ptr<Texture> emissiveTex = resolveTexture(gltfMat.emissiveTexture.index);
            pbrMat->SetEmissiveMap(emissiveTex ? emissiveTex : ctx.defaultEmissive);

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
    // Step 3 – meshes
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
                              << " is not TRIANGLES (mode=" << mode << ") – skipped.\n";
                    ++ctx.skippedPrimitives;
                    continue;
                }

                // --- POSITION (required) ---
                auto posIt = prim.attributes.find("POSITION");
                if (posIt == prim.attributes.end())
                {
                    std::cerr << "[glTF] Mesh \"" << meshName << "\" primitive " << pi
                              << " has no POSITION – skipped.\n";
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
                              << " has invalid index count (" << indices.size() << ") – skipped.\n";
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
    // Step 4 – node hierarchy
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
               ", %d skipped, %d invalid, %zu materials, %zu textures\n",
               model.GetNodeCount(),
               model.GetMeshCount(),
               ctx.importedPrimitives,
               ctx.skippedPrimitives,
               invalidPrimCount,
               model.GetMaterialCount(),
               ctx.model.textures.size());
    }

    // -------------------------------------------------------------------------
    // Public entry point
    // -------------------------------------------------------------------------

    std::shared_ptr<ModelAsset> ModelLoader::LoadGlb(const std::string& filename)
    {
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
        std::string modelName = filename;
        size_t slash = modelName.find_last_of("/\\");
        if (slash != std::string::npos)
            modelName = modelName.substr(slash + 1);
        size_t dot = modelName.rfind('.');
        if (dot != std::string::npos)
            modelName = modelName.substr(0, dot);

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
        ImportNodes(ctx);
        ValidateAndLog(ctx);

        return ctx.outModel;
    }
}
