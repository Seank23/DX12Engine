#include <DirectXTex.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <objbase.h>

#define NOMINMAX
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#include "meshoptimizer.h"

namespace fs = std::filesystem;

namespace
{
    constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;
    constexpr const char* kCacheVersion = "asset-cooker-cache-v2";

    enum class TextureSemantic
    {
        Albedo,
        Normal,
        Roughness,
        Metallic,
        MetallicRoughness,
        AO,
        Emissive,
        Generic
    };

    struct CookSettings
    {
        TextureSemantic semantic = TextureSemantic::Generic;
        bool srgbHint = false;
    };

    struct Args
    {
        fs::path inputRoot;
        fs::path outputRoot;
        bool force = false;
        bool cleanCache = false;
    };

    struct CookStats
    {
        std::size_t scanned = 0;
        std::size_t cooked = 0;
        std::size_t skipped = 0;
        std::size_t failed = 0;
    };

    using CacheTable = std::unordered_map<std::string, std::uint64_t>;

    std::string ToLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    std::string JsonEscape(const std::string& text)
    {
        std::string escaped;
        escaped.reserve(text.size() + 8);
        for (char c : text)
        {
            switch (c)
            {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
            }
        }
        return escaped;
    }

    std::string SanitizeName(std::string name)
    {
        if (name.empty())
        {
            return {};
        }

        for (char& c : name)
        {
            const bool isAllowed = std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_';
            if (!isAllowed)
            {
                c = '_';
            }
        }

        while (!name.empty() && name.back() == '_')
        {
            name.pop_back();
        }

        return name;
    }

    const char* SemanticToString(TextureSemantic semantic)
    {
        switch (semantic)
        {
        case TextureSemantic::Albedo:
            return "albedo";
        case TextureSemantic::Normal:
            return "normal";
        case TextureSemantic::Roughness:
            return "roughness";
        case TextureSemantic::Metallic:
            return "metallic";
        case TextureSemantic::MetallicRoughness:
            return "metallicRoughness";
        case TextureSemantic::AO:
            return "ao";
        case TextureSemantic::Emissive:
            return "emissive";
        default:
            return "generic";
        }
    }

    bool IsSupportedTextureExtension(const fs::path& path)
    {
        const std::string ext = ToLower(path.extension().string());
        return ext == ".dds" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".hdr" || ext == ".tif" || ext == ".tiff";
    }

    bool IsSupportedGlbExtension(const fs::path& path)
    {
        return ToLower(path.extension().string()) == ".glb";
    }

    CookSettings InferCookSettings(const fs::path& sourcePath)
    {
        const std::string filename = ToLower(sourcePath.filename().string());
        CookSettings settings;

        if (filename.find("normal") != std::string::npos || filename.find("_n.") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Normal;
            settings.srgbHint = false;
        }
        else if (filename.find("roughness") != std::string::npos || filename.find("rough.") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Roughness;
            settings.srgbHint = false;
        }
        else if (filename.find("metallic") != std::string::npos || filename.find("metalness") != std::string::npos || filename.find("_m.") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Metallic;
            settings.srgbHint = false;
        }
        else if (filename.find("ao") != std::string::npos || filename.find("occlusion") != std::string::npos)
        {
            settings.semantic = TextureSemantic::AO;
            settings.srgbHint = false;
        }
        else if (filename.find("emissive") != std::string::npos || filename.find("emission") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Emissive;
            settings.srgbHint = true;
        }
        else if (filename.find("albedo") != std::string::npos || filename.find("basecolor") != std::string::npos || filename.find("diffuse") != std::string::npos || filename.find("color") != std::string::npos)
        {
            settings.semantic = TextureSemantic::Albedo;
            settings.srgbHint = true;
        }
        else
        {
            settings.semantic = TextureSemantic::Generic;
            settings.srgbHint = false;
        }

        return settings;
    }

    std::uint64_t HashBytes(std::uint64_t hash, const std::uint8_t* bytes, std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            hash ^= bytes[i];
            hash *= kFnvPrime;
        }
        return hash;
    }

    std::uint64_t HashText(std::uint64_t hash, const std::string& text)
    {
        return HashBytes(hash, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    }

    std::optional<std::uint64_t> HashFileWithSettings(const fs::path& sourcePath, const CookSettings& settings)
    {
        std::ifstream input(sourcePath, std::ios::binary);
        if (!input)
        {
            return std::nullopt;
        }

        std::uint64_t hash = kFnvOffsetBasis;
        std::vector<std::uint8_t> buffer(64 * 1024);
        while (input)
        {
            input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            const auto readCount = static_cast<std::size_t>(input.gcount());
            if (readCount > 0)
            {
                hash = HashBytes(hash, buffer.data(), readCount);
            }
        }

        hash = HashText(hash, kCacheVersion);
        hash = HashText(hash, SemanticToString(settings.semantic));
        hash = HashText(hash, settings.srgbHint ? "srgb=1" : "srgb=0");
        return hash;
    }

    bool ParseArgs(int argc, wchar_t* argv[], Args& outArgs)
    {
        if (argc <= 1)
        {
            return false;
        }

        for (int i = 1; i < argc; ++i)
        {
            const std::wstring arg = argv[i];
            if (arg == L"--in" && i + 1 < argc)
            {
                outArgs.inputRoot = argv[++i];
            }
            else if (arg == L"--out" && i + 1 < argc)
            {
                outArgs.outputRoot = argv[++i];
            }
            else if (arg == L"--force")
            {
                outArgs.force = true;
            }
            else if (arg == L"--clean-cache")
            {
                outArgs.cleanCache = true;
            }
            else if (arg == L"--help" || arg == L"-h")
            {
                return false;
            }
            else
            {
                std::wcerr << L"Unknown argument: " << arg << L"\n";
                return false;
            }
        }

        return !outArgs.inputRoot.empty() && !outArgs.outputRoot.empty();
    }

    void PrintUsage()
    {
        std::wcout << L"AssetCooker usage:\n"
                   << L"  AssetCooker --in <raw-assets-dir> --out <cooked-assets-dir> [--force] [--clean-cache]\n";
    }

    CacheTable LoadCache(const fs::path& cacheFile)
    {
        CacheTable cache;
        std::ifstream input(cacheFile);
        if (!input)
        {
            return cache;
        }

        std::string versionLine;
        std::getline(input, versionLine);
        if (versionLine != kCacheVersion)
        {
            return cache;
        }

        std::string line;
        while (std::getline(input, line))
        {
            if (line.empty())
            {
                continue;
            }

            const auto delim = line.find('\t');
            if (delim == std::string::npos)
            {
                continue;
            }

            const std::string hashText = line.substr(0, delim);
            const std::string key = line.substr(delim + 1);
            try
            {
                const std::uint64_t hash = std::stoull(hashText, nullptr, 16);
                cache[key] = hash;
            }
            catch (...)
            {
                // Skip malformed cache rows.
            }
        }

        return cache;
    }

    bool SaveCache(const fs::path& cacheFile, const CacheTable& cache)
    {
        std::ofstream output(cacheFile, std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << kCacheVersion << "\n";
        for (const auto& [key, hash] : cache)
        {
            output << std::hex << hash << std::dec << '\t' << key << "\n";
        }

        return true;
    }

    HRESULT LoadImageFile(const fs::path& sourcePath, DirectX::ScratchImage& image)
    {
        const std::string ext = ToLower(sourcePath.extension().string());
        if (ext == ".dds")
        {
            return DirectX::LoadFromDDSFile(sourcePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        }

        if (ext == ".tga")
        {
            return DirectX::LoadFromTGAFile(sourcePath.c_str(), nullptr, image);
        }

        if (ext == ".hdr")
        {
            return DirectX::LoadFromHDRFile(sourcePath.c_str(), nullptr, image);
        }

        return DirectX::LoadFromWICFile(sourcePath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
    }

    HRESULT LoadImageMemory(const std::vector<unsigned char>& encodedBytes, const std::string& mimeHint, DirectX::ScratchImage& image)
    {
        if (encodedBytes.empty())
        {
            return E_INVALIDARG;
        }

        const std::string mime = ToLower(mimeHint);
        if (mime.find("dds") != std::string::npos)
        {
            return DirectX::LoadFromDDSMemory(encodedBytes.data(), encodedBytes.size(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        }

        if (mime.find("tga") != std::string::npos)
        {
            return DirectX::LoadFromTGAMemory(encodedBytes.data(), encodedBytes.size(), nullptr, image);
        }

        if (mime.find("hdr") != std::string::npos)
        {
            return DirectX::LoadFromHDRMemory(encodedBytes.data(), encodedBytes.size(), nullptr, image);
        }

        return DirectX::LoadFromWICMemory(encodedBytes.data(), encodedBytes.size(), DirectX::WIC_FLAGS_NONE, nullptr, image);
    }

    bool WriteMetadataFile(const fs::path& metadataPath, const std::string& sourceTag, const CookSettings& settings, const DirectX::TexMetadata& metadata)
    {
        std::ofstream output(metadataPath, std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << "{\n";
        output << "  \"source\": \"" << JsonEscape(sourceTag) << "\",\n";
        output << "  \"semantic\": \"" << SemanticToString(settings.semantic) << "\",\n";
        output << "  \"srgbHint\": " << (settings.srgbHint ? "true" : "false") << ",\n";
        output << "  \"width\": " << metadata.width << ",\n";
        output << "  \"height\": " << metadata.height << ",\n";
        output << "  \"depth\": " << metadata.depth << ",\n";
        output << "  \"arraySize\": " << metadata.arraySize << ",\n";
        output << "  \"mipLevels\": " << metadata.mipLevels << ",\n";
        output << "  \"isCubemap\": " << (metadata.IsCubemap() ? "true" : "false") << ",\n";
        output << "  \"dxgiFormat\": " << static_cast<int>(metadata.format) << "\n";
        output << "}\n";

        return true;
    }

    bool SaveCookedTexture(const DirectX::ScratchImage& sourceImage, const fs::path& outputDdsPath, const fs::path& metadataPath, const std::string& sourceTag, const CookSettings& settings)
    {
        const DirectX::TexMetadata sourceMetadata = sourceImage.GetMetadata();
        DirectX::ScratchImage generatedMipChain;
        const DirectX::ScratchImage* finalImage = &sourceImage;

        HRESULT hr = S_OK;
        if (sourceMetadata.mipLevels <= 1 && sourceMetadata.dimension != DirectX::TEX_DIMENSION_TEXTURE3D)
        {
            hr = DirectX::GenerateMipMaps(
                sourceImage.GetImages(),
                sourceImage.GetImageCount(),
                sourceMetadata,
                DirectX::TEX_FILTER_FANT,
                0,
                generatedMipChain);

            if (SUCCEEDED(hr) && generatedMipChain.GetImageCount() > 0)
            {
                finalImage = &generatedMipChain;
            }
        }

        hr = DirectX::SaveToDDSFile(
            finalImage->GetImages(),
            finalImage->GetImageCount(),
            finalImage->GetMetadata(),
            DirectX::DDS_FLAGS_NONE,
            outputDdsPath.c_str());

        if (FAILED(hr))
        {
            std::wcerr << L"[ERROR] Failed to save DDS: " << outputDdsPath << L"\n";
            return false;
        }

        if (!WriteMetadataFile(metadataPath, sourceTag, settings, finalImage->GetMetadata()))
        {
            std::wcerr << L"[ERROR] Failed to write metadata: " << metadataPath << L"\n";
            return false;
        }

        return true;
    }

    bool CookTextureFile(const fs::path& sourcePath, const fs::path& outputDdsPath, const fs::path& metadataPath, const fs::path& sourceRelativePath, const CookSettings& settings)
    {
        DirectX::ScratchImage sourceImage;
        const HRESULT hr = LoadImageFile(sourcePath, sourceImage);
        if (FAILED(hr))
        {
            std::wcerr << L"[ERROR] Failed to load: " << sourcePath << L"\n";
            return false;
        }

        return SaveCookedTexture(sourceImage, outputDdsPath, metadataPath, sourceRelativePath.generic_string(), settings);
    }

    void TryAssignSemantic(std::vector<TextureSemantic>& semantics, int textureIndex, TextureSemantic semantic)
    {
        if (textureIndex < 0 || static_cast<std::size_t>(textureIndex) >= semantics.size())
        {
            return;
        }

        if (semantics[textureIndex] == TextureSemantic::Generic)
        {
            semantics[textureIndex] = semantic;
        }
    }

    std::vector<TextureSemantic> InferGlbTextureSemantics(const tinygltf::Model& model)
    {
        std::vector<TextureSemantic> semantics(model.textures.size(), TextureSemantic::Generic);
        for (const tinygltf::Material& material : model.materials)
        {
            TryAssignSemantic(semantics, material.pbrMetallicRoughness.baseColorTexture.index, TextureSemantic::Albedo);
            TryAssignSemantic(semantics, material.normalTexture.index, TextureSemantic::Normal);
            TryAssignSemantic(semantics, material.occlusionTexture.index, TextureSemantic::AO);
            TryAssignSemantic(semantics, material.emissiveTexture.index, TextureSemantic::Emissive);
            TryAssignSemantic(semantics, material.pbrMetallicRoughness.metallicRoughnessTexture.index, TextureSemantic::MetallicRoughness);
        }
        return semantics;
    }

    std::optional<std::vector<unsigned char>> GetGlbEncodedImageBytes(const tinygltf::Model& model, const tinygltf::Image& image)
    {
        if (image.bufferView < 0 || static_cast<std::size_t>(image.bufferView) >= model.bufferViews.size())
        {
            return std::nullopt;
        }

        const tinygltf::BufferView& bufferView = model.bufferViews[image.bufferView];
        if (bufferView.buffer < 0 || static_cast<std::size_t>(bufferView.buffer) >= model.buffers.size())
        {
            return std::nullopt;
        }

        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        const std::size_t begin = bufferView.byteOffset;
        const std::size_t end = begin + bufferView.byteLength;
        if (end > buffer.data.size() || begin >= end)
        {
            return std::nullopt;
        }

        return std::vector<unsigned char>(buffer.data.begin() + static_cast<std::ptrdiff_t>(begin), buffer.data.begin() + static_cast<std::ptrdiff_t>(end));
    }

    bool LoadGlbDecodedPixels(const tinygltf::Image& image, DirectX::ScratchImage& outScratch)
    {
        if (image.bits != 8 || image.width <= 0 || image.height <= 0 || image.component <= 0 || image.component > 4 || image.image.empty())
        {
            return false;
        }

        const std::size_t pixelCount = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
        const std::size_t expectedBytes = pixelCount * static_cast<std::size_t>(image.component);
        if (image.image.size() < expectedBytes)
        {
            return false;
        }

        std::vector<std::uint8_t> rgba(pixelCount * 4, 255);
        const std::uint8_t* src = image.image.data();
        for (std::size_t i = 0; i < pixelCount; ++i)
        {
            const std::size_t srcOffset = i * static_cast<std::size_t>(image.component);
            const std::size_t dstOffset = i * 4;
            const std::uint8_t r = src[srcOffset + 0];
            const std::uint8_t g = image.component > 1 ? src[srcOffset + 1] : r;
            const std::uint8_t b = image.component > 2 ? src[srcOffset + 2] : r;
            const std::uint8_t a = image.component > 3 ? src[srcOffset + 3] : 255;
            rgba[dstOffset + 0] = r;
            rgba[dstOffset + 1] = g;
            rgba[dstOffset + 2] = b;
            rgba[dstOffset + 3] = a;
        }

        HRESULT hr = outScratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, static_cast<size_t>(image.width), static_cast<size_t>(image.height), 1, 1);
        if (FAILED(hr))
        {
            return false;
        }

        const DirectX::Image* dst = outScratch.GetImage(0, 0, 0);
        if (!dst || !dst->pixels)
        {
            return false;
        }

        const std::size_t srcRowPitch = static_cast<std::size_t>(image.width) * 4;
        for (int y = 0; y < image.height; ++y)
        {
            std::memcpy(dst->pixels + static_cast<std::size_t>(y) * dst->rowPitch, rgba.data() + static_cast<std::size_t>(y) * srcRowPitch, srcRowPitch);
        }

        return true;
    }

    bool LoadGlbImage(const tinygltf::Model& model, const tinygltf::Image& image, const fs::path& sourcePath, DirectX::ScratchImage& outScratch)
    {
        if (const auto encoded = GetGlbEncodedImageBytes(model, image))
        {
            const HRESULT hr = LoadImageMemory(*encoded, image.mimeType, outScratch);
            if (SUCCEEDED(hr))
            {
                return true;
            }
        }

        if (!image.uri.empty())
        {
            const fs::path uriPath = sourcePath.parent_path() / fs::path(image.uri);
            if (fs::exists(uriPath))
            {
                const HRESULT hr = LoadImageFile(uriPath, outScratch);
                if (SUCCEEDED(hr))
                {
                    return true;
                }
            }
        }

        return LoadGlbDecodedPixels(image, outScratch);
    }

    bool WriteGlbMaterialManifest(
        const fs::path& manifestPath,
        const fs::path& sourceRelativePath,
        const tinygltf::Model& model,
        const std::vector<TextureSemantic>& semantics,
        const std::vector<std::string>& textureOutputs)
    {
        std::ofstream output(manifestPath, std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << "{\n";
        output << "  \"source\": \"" << JsonEscape(sourceRelativePath.generic_string()) << "\",\n";
        output << "  \"textures\": [\n";
        for (std::size_t i = 0; i < model.textures.size(); ++i)
        {
            const tinygltf::Texture& texture = model.textures[i];
            const std::string textureName = !texture.name.empty() ? texture.name : ("texture_" + std::to_string(i));
            output << "    {\n";
            output << "      \"index\": " << i << ",\n";
            output << "      \"name\": \"" << JsonEscape(textureName) << "\",\n";
            output << "      \"semantic\": \"" << SemanticToString(semantics[i]) << "\",\n";
            if (!textureOutputs[i].empty())
            {
                output << "      \"output\": \"" << JsonEscape(textureOutputs[i]) << "\"\n";
            }
            else
            {
                output << "      \"output\": null\n";
            }
            output << "    }" << (i + 1 == model.textures.size() ? "\n" : ",\n");
        }
        output << "  ],\n";
        output << "  \"materials\": [\n";

        for (std::size_t i = 0; i < model.materials.size(); ++i)
        {
            const tinygltf::Material& material = model.materials[i];
            auto writeTextureRef = [&](const char* key, int textureIndex, bool trailingComma)
            {
                output << "      \"" << key << "\": ";
                if (textureIndex >= 0 && static_cast<std::size_t>(textureIndex) < textureOutputs.size() && !textureOutputs[textureIndex].empty())
                {
                    output << "\"" << JsonEscape(textureOutputs[textureIndex]) << "\"";
                }
                else
                {
                    output << "null";
                }

                output << (trailingComma ? ",\n" : "\n");
            };

            output << "    {\n";
            output << "      \"index\": " << i << ",\n";
            output << "      \"name\": \"" << JsonEscape(material.name.empty() ? ("material_" + std::to_string(i)) : material.name) << "\",\n";
            writeTextureRef("baseColorTexture", material.pbrMetallicRoughness.baseColorTexture.index, true);
            writeTextureRef("normalTexture", material.normalTexture.index, true);
            writeTextureRef("metallicRoughnessTexture", material.pbrMetallicRoughness.metallicRoughnessTexture.index, true);
            writeTextureRef("occlusionTexture", material.occlusionTexture.index, true);
            writeTextureRef("emissiveTexture", material.emissiveTexture.index, false);
            output << "    }" << (i + 1 == model.materials.size() ? "\n" : ",\n");
        }

        output << "  ]\n";
        output << "}\n";

        return true;
    }

    std::string BuildGlbTextureTag(const fs::path& sourceRelativePath, std::size_t textureIndex)
    {
        std::ostringstream tag;
        tag << sourceRelativePath.generic_string() << "#texture/" << textureIndex;
        return tag.str();
    }

    fs::path BuildGlbTextureOutputRelativePath(const fs::path& sourceRelativePath, std::size_t textureIndex, const std::string& imageName)
    {
        fs::path modelRoot = sourceRelativePath;
        modelRoot.replace_extension();

        std::ostringstream filename;
        filename << "tex_" << std::setw(3) << std::setfill('0') << textureIndex;
        if (!imageName.empty())
        {
            filename << "_" << imageName;
        }
        filename << ".dds";

        return modelRoot / "Textures" / filename.str();
    }

    struct GlbPrimitiveMeshData
    {
        std::vector<std::uint32_t> indices;
        std::vector<float> positions; // xyz, tightly packed
    };

    struct GlbLodLevelInfo
    {
        int level = 0;
        float ratio = 1.0f;
        float error = 0.0f;
        std::size_t indexCount = 0;
        std::string output;
    };

    struct GlbPrimitiveLodInfo
    {
        std::size_t meshIndex = 0;
        std::size_t primitiveIndex = 0;
        std::size_t vertexCount = 0;
        std::size_t sourceIndexCount = 0;
        bool skipped = false;
        std::string reason;
        std::vector<GlbLodLevelInfo> lods;
    };

    const std::uint8_t* AccessorElementPointer(const tinygltf::Model& model, const tinygltf::Accessor& accessor, std::size_t index)
    {
        if (accessor.bufferView < 0 || static_cast<std::size_t>(accessor.bufferView) >= model.bufferViews.size())
        {
            return nullptr;
        }

        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        if (bufferView.buffer < 0 || static_cast<std::size_t>(bufferView.buffer) >= model.buffers.size())
        {
            return nullptr;
        }

        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        int stride = accessor.ByteStride(bufferView);
        if (stride <= 0)
        {
            stride = tinygltf::GetComponentSizeInBytes(accessor.componentType) * tinygltf::GetNumComponentsInType(accessor.type);
        }
        if (stride <= 0)
        {
            return nullptr;
        }

        const std::size_t byteOffset = static_cast<std::size_t>(bufferView.byteOffset) + static_cast<std::size_t>(accessor.byteOffset) + index * static_cast<std::size_t>(stride);
        if (byteOffset + static_cast<std::size_t>(stride) > buffer.data.size())
        {
            return nullptr;
        }

        return buffer.data.data() + byteOffset;
    }

    std::uint32_t ReadIndexScalar(const std::uint8_t* bytes, int componentType)
    {
        if (!bytes)
        {
            return 0;
        }

        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return *reinterpret_cast<const std::uint8_t*>(bytes);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return *reinterpret_cast<const std::uint16_t*>(bytes);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            return *reinterpret_cast<const std::uint32_t*>(bytes);
        default:
            return 0;
        }
    }

    float ReadScalarFloat(const std::uint8_t* bytes, int componentType, bool normalized)
    {
        if (!bytes)
        {
            return 0.0f;
        }

        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return *reinterpret_cast<const float*>(bytes);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return normalized ? static_cast<float>(*reinterpret_cast<const std::uint8_t*>(bytes)) / 255.0f : static_cast<float>(*reinterpret_cast<const std::uint8_t*>(bytes));
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return normalized ? static_cast<float>(*reinterpret_cast<const std::uint16_t*>(bytes)) / 65535.0f : static_cast<float>(*reinterpret_cast<const std::uint16_t*>(bytes));
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            return normalized ? std::max(-1.0f, static_cast<float>(*reinterpret_cast<const std::int8_t*>(bytes)) / 127.0f) : static_cast<float>(*reinterpret_cast<const std::int8_t*>(bytes));
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            return normalized ? std::max(-1.0f, static_cast<float>(*reinterpret_cast<const std::int16_t*>(bytes)) / 32767.0f) : static_cast<float>(*reinterpret_cast<const std::int16_t*>(bytes));
        default:
            return 0.0f;
        }
    }

    std::array<float, 3> ReadVec3(const std::uint8_t* bytes, int componentType, bool normalized)
    {
        if (!bytes)
        {
            return { 0.0f, 0.0f, 0.0f };
        }

        if (componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
            const float* values = reinterpret_cast<const float*>(bytes);
            return { values[0], values[1], values[2] };
        }

        const int componentSize = tinygltf::GetComponentSizeInBytes(componentType);
        return {
            ReadScalarFloat(bytes, componentType, normalized),
            ReadScalarFloat(bytes + componentSize, componentType, normalized),
            ReadScalarFloat(bytes + componentSize * 2, componentType, normalized)
        };
    }

    bool ExtractGlbPrimitiveMeshData(const tinygltf::Model& model, const tinygltf::Primitive& primitive, GlbPrimitiveMeshData& outData, std::string& reason)
    {
        int mode = primitive.mode;
        if (mode < 0)
        {
            mode = TINYGLTF_MODE_TRIANGLES;
        }
        if (mode != TINYGLTF_MODE_TRIANGLES)
        {
            reason = "non-triangle primitive";
            return false;
        }

        const auto positionIt = primitive.attributes.find("POSITION");
        if (positionIt == primitive.attributes.end())
        {
            reason = "missing POSITION accessor";
            return false;
        }

        if (positionIt->second < 0 || static_cast<std::size_t>(positionIt->second) >= model.accessors.size())
        {
            reason = "invalid POSITION accessor index";
            return false;
        }

        const tinygltf::Accessor& positionAccessor = model.accessors[positionIt->second];
        if (tinygltf::GetNumComponentsInType(positionAccessor.type) != 3 || positionAccessor.count == 0)
        {
            reason = "POSITION accessor is not VEC3 or empty";
            return false;
        }

        const std::size_t vertexCount = positionAccessor.count;
        outData.positions.resize(vertexCount * 3);
        for (std::size_t i = 0; i < vertexCount; ++i)
        {
            const std::uint8_t* elem = AccessorElementPointer(model, positionAccessor, i);
            if (!elem)
            {
                reason = "POSITION accessor points outside buffer";
                return false;
            }

            const std::array<float, 3> p = ReadVec3(elem, positionAccessor.componentType, positionAccessor.normalized);
            outData.positions[i * 3 + 0] = p[0];
            outData.positions[i * 3 + 1] = p[1];
            outData.positions[i * 3 + 2] = p[2];
        }

        outData.indices.clear();
        if (primitive.indices >= 0)
        {
            if (static_cast<std::size_t>(primitive.indices) >= model.accessors.size())
            {
                reason = "invalid index accessor";
                return false;
            }

            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            if (tinygltf::GetNumComponentsInType(indexAccessor.type) != 1)
            {
                reason = "index accessor is not scalar";
                return false;
            }

            outData.indices.resize(indexAccessor.count);
            for (std::size_t i = 0; i < indexAccessor.count; ++i)
            {
                const std::uint8_t* elem = AccessorElementPointer(model, indexAccessor, i);
                if (!elem)
                {
                    reason = "index accessor points outside buffer";
                    return false;
                }

                outData.indices[i] = ReadIndexScalar(elem, indexAccessor.componentType);
            }
        }
        else
        {
            outData.indices.resize(vertexCount);
            for (std::size_t i = 0; i < vertexCount; ++i)
            {
                outData.indices[i] = static_cast<std::uint32_t>(i);
            }
        }

        if (outData.indices.size() < 3 || (outData.indices.size() % 3) != 0)
        {
            reason = "index count is not a triangle list";
            return false;
        }

        return true;
    }

    fs::path BuildGlbLodOutputRelativePath(const fs::path& sourceRelativePath, std::size_t meshIndex, std::size_t primitiveIndex, int lodLevel)
    {
        fs::path modelRoot = sourceRelativePath;
        modelRoot.replace_extension();

        std::ostringstream filename;
        filename << "mesh_" << std::setw(3) << std::setfill('0') << meshIndex
                 << "_prim_" << std::setw(3) << std::setfill('0') << primitiveIndex
                 << "_lod" << lodLevel << ".indices.bin";
        return modelRoot / "LODs" / filename.str();
    }

    bool WriteIndicesBinary(const fs::path& outputPath, const std::vector<std::uint32_t>& indices)
    {
        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output.write(reinterpret_cast<const char*>(indices.data()), static_cast<std::streamsize>(indices.size() * sizeof(std::uint32_t)));
        return output.good();
    }

    bool WriteGlbLodManifest(const fs::path& manifestPath, const fs::path& sourceRelativePath, const std::vector<GlbPrimitiveLodInfo>& primitiveLods)
    {
        std::ofstream output(manifestPath, std::ios::trunc);
        if (!output)
        {
            return false;
        }

        output << "{\n";
        output << "  \"source\": \"" << JsonEscape(sourceRelativePath.generic_string()) << "\",\n";
        output << "  \"primitives\": [\n";
        for (std::size_t i = 0; i < primitiveLods.size(); ++i)
        {
            const GlbPrimitiveLodInfo& prim = primitiveLods[i];
            output << "    {\n";
            output << "      \"meshIndex\": " << prim.meshIndex << ",\n";
            output << "      \"primitiveIndex\": " << prim.primitiveIndex << ",\n";
            output << "      \"vertexCount\": " << prim.vertexCount << ",\n";
            output << "      \"sourceIndexCount\": " << prim.sourceIndexCount << ",\n";
            output << "      \"skipped\": " << (prim.skipped ? "true" : "false") << ",\n";
            output << "      \"reason\": " << (prim.reason.empty() ? "null" : ("\"" + JsonEscape(prim.reason) + "\"")) << ",\n";
            output << "      \"lods\": [\n";
            for (std::size_t li = 0; li < prim.lods.size(); ++li)
            {
                const GlbLodLevelInfo& lod = prim.lods[li];
                output << "        {\n";
                output << "          \"level\": " << lod.level << ",\n";
                output << "          \"ratio\": " << lod.ratio << ",\n";
                output << "          \"error\": " << lod.error << ",\n";
                output << "          \"indexCount\": " << lod.indexCount << ",\n";
                output << "          \"output\": \"" << JsonEscape(lod.output) << "\"\n";
                output << "        }" << (li + 1 == prim.lods.size() ? "\n" : ",\n");
            }
            output << "      ]\n";
            output << "    }" << (i + 1 == primitiveLods.size() ? "\n" : ",\n");
        }
        output << "  ]\n";
        output << "}\n";

        return true;
    }

    bool CookGlbMeshLods(const tinygltf::Model& model, const fs::path& outputRoot, const fs::path& sourceRelativePath, std::vector<GlbPrimitiveLodInfo>& outPrimitiveLods)
    {
        constexpr std::array<float, 4> kLodRatios = { 1.0f, 0.5f, 0.25f, 0.125f };
        constexpr std::array<float, 4> kTargetErrors = { 0.0f, 0.01f, 0.02f, 0.04f };

        bool success = true;
        for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
        {
            const tinygltf::Mesh& mesh = model.meshes[meshIndex];
            for (std::size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex)
            {
                const tinygltf::Primitive& primitive = mesh.primitives[primitiveIndex];
                GlbPrimitiveMeshData primitiveData;
                GlbPrimitiveLodInfo record;
                record.meshIndex = meshIndex;
                record.primitiveIndex = primitiveIndex;

                std::string reason;
                if (!ExtractGlbPrimitiveMeshData(model, primitive, primitiveData, reason))
                {
                    record.skipped = true;
                    record.reason = reason;
                    outPrimitiveLods.push_back(std::move(record));
                    continue;
                }

                record.vertexCount = primitiveData.positions.size() / 3;
                record.sourceIndexCount = primitiveData.indices.size();

                const std::vector<std::uint32_t>& sourceIndices = primitiveData.indices;
                for (std::size_t lodLevel = 0; lodLevel < kLodRatios.size(); ++lodLevel)
                {
                    const float ratio = kLodRatios[lodLevel];
                    std::size_t targetIndexCount = static_cast<std::size_t>(static_cast<double>(sourceIndices.size()) * ratio);
                    if (targetIndexCount < 3)
                    {
                        targetIndexCount = 3;
                    }
                    targetIndexCount = (targetIndexCount / 3) * 3;
                    targetIndexCount = std::max<std::size_t>(3, targetIndexCount);
                    targetIndexCount = std::min(targetIndexCount, sourceIndices.size());

                    std::vector<std::uint32_t> lodIndices(sourceIndices.size());
                    float lodError = 0.0f;
                    std::size_t lodIndexCount = sourceIndices.size();
                    if (lodLevel == 0)
                    {
                        lodIndices = sourceIndices;
                        lodError = 0.0f;
                    }
                    else
                    {
                        lodIndexCount = meshopt_simplify(
                            lodIndices.data(),
                            sourceIndices.data(),
                            sourceIndices.size(),
                            primitiveData.positions.data(),
                            record.vertexCount,
                            sizeof(float) * 3,
                            targetIndexCount,
                            kTargetErrors[lodLevel],
                            0,
                            &lodError);

                        if (lodIndexCount < 3 || (lodIndexCount % 3) != 0)
                        {
                            lodIndexCount = sourceIndices.size();
                            lodIndices = sourceIndices;
                            lodError = 0.0f;
                        }
                        else
                        {
                            lodIndices.resize(lodIndexCount);
                        }
                    }

                    const fs::path lodRelativePath = BuildGlbLodOutputRelativePath(sourceRelativePath, meshIndex, primitiveIndex, static_cast<int>(lodLevel));
                    const fs::path lodOutputPath = outputRoot / lodRelativePath;

                    std::error_code ec;
                    fs::create_directories(lodOutputPath.parent_path(), ec);
                    if (ec)
                    {
                        std::wcerr << L"[ERROR] Failed to create LOD output directory: " << lodOutputPath.parent_path() << L"\n";
                        success = false;
                        continue;
                    }

                    if (!WriteIndicesBinary(lodOutputPath, lodIndices))
                    {
                        std::wcerr << L"[ERROR] Failed to write LOD index file: " << lodOutputPath << L"\n";
                        success = false;
                        continue;
                    }

                    GlbLodLevelInfo lodInfo;
                    lodInfo.level = static_cast<int>(lodLevel);
                    lodInfo.ratio = ratio;
                    lodInfo.error = lodError;
                    lodInfo.indexCount = lodIndices.size();
                    lodInfo.output = lodRelativePath.generic_string();
                    record.lods.push_back(std::move(lodInfo));
                }

                outPrimitiveLods.push_back(std::move(record));
            }
        }

        return success;
    }

    bool CookGlbFile(const fs::path& sourcePath, const fs::path& outputRoot, const fs::path& sourceRelativePath)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string warn;
        std::string err;
        const bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, sourcePath.string());

        if (!warn.empty())
        {
            std::wcerr << L"[WARN] glb parse warning (" << sourcePath << L"): " << warn.c_str() << L"\n";
        }

        if (!ok)
        {
            std::wcerr << L"[ERROR] Failed to parse glb: " << sourcePath << L"\n";
            if (!err.empty())
            {
                std::wcerr << L"[ERROR] tinygltf: " << err.c_str() << L"\n";
            }
            return false;
        }

        std::vector<TextureSemantic> semantics = InferGlbTextureSemantics(model);
        std::vector<std::string> textureOutputs(model.textures.size());
        bool success = true;

        for (std::size_t textureIndex = 0; textureIndex < model.textures.size(); ++textureIndex)
        {
            const tinygltf::Texture& texture = model.textures[textureIndex];
            if (texture.source < 0 || static_cast<std::size_t>(texture.source) >= model.images.size())
            {
                continue;
            }

            const tinygltf::Image& image = model.images[texture.source];
            DirectX::ScratchImage decodedImage;
            if (!LoadGlbImage(model, image, sourcePath, decodedImage))
            {
                std::wcerr << L"[ERROR] Failed to decode glb texture #" << textureIndex << L" in " << sourcePath << L"\n";
                success = false;
                continue;
            }

            CookSettings settings;
            settings.semantic = semantics[textureIndex];
            settings.srgbHint = settings.semantic == TextureSemantic::Albedo || settings.semantic == TextureSemantic::Emissive;

            std::string imageName = image.name;
            if (imageName.empty())
            {
                imageName = texture.name;
            }
            imageName = SanitizeName(imageName);

            const fs::path outputRelativePath = BuildGlbTextureOutputRelativePath(sourceRelativePath, textureIndex, imageName);
            const fs::path outputDdsPath = outputRoot / outputRelativePath;
            fs::path metadataPath = outputDdsPath;
            metadataPath += L".meta";

            std::error_code ec;
            fs::create_directories(outputDdsPath.parent_path(), ec);
            if (ec)
            {
                std::wcerr << L"[ERROR] Failed to create directory: " << outputDdsPath.parent_path() << L"\n";
                success = false;
                continue;
            }

            const std::string sourceTag = BuildGlbTextureTag(sourceRelativePath, textureIndex);
            if (!SaveCookedTexture(decodedImage, outputDdsPath, metadataPath, sourceTag, settings))
            {
                success = false;
                continue;
            }

            textureOutputs[textureIndex] = outputRelativePath.generic_string();
            std::wcout << L"[COOKED] " << sourceRelativePath.c_str() << L" texture #" << textureIndex << L" -> " << outputRelativePath.c_str() << L"\n";
        }

        fs::path manifestRelative = sourceRelativePath;
        manifestRelative.replace_extension();
        manifestRelative /= "materials.json";
        const fs::path manifestPath = outputRoot / manifestRelative;
        std::error_code ec;
        fs::create_directories(manifestPath.parent_path(), ec);
        if (ec)
        {
            std::wcerr << L"[ERROR] Failed to create manifest directory: " << manifestPath.parent_path() << L"\n";
            return false;
        }

        if (!WriteGlbMaterialManifest(manifestPath, sourceRelativePath, model, semantics, textureOutputs))
        {
            std::wcerr << L"[ERROR] Failed to write glb material manifest: " << manifestPath << L"\n";
            return false;
        }

        std::vector<GlbPrimitiveLodInfo> primitiveLods;
        const bool lodCookSuccess = CookGlbMeshLods(model, outputRoot, sourceRelativePath, primitiveLods);
        fs::path lodManifestRelative = sourceRelativePath;
        lodManifestRelative.replace_extension();
        lodManifestRelative /= "lods.json";
        const fs::path lodManifestPath = outputRoot / lodManifestRelative;

        if (!WriteGlbLodManifest(lodManifestPath, sourceRelativePath, primitiveLods))
        {
            std::wcerr << L"[ERROR] Failed to write glb LOD manifest: " << lodManifestPath << L"\n";
            return false;
        }

        success = success && lodCookSuccess;
        return success;
    }
} // namespace

int wmain(int argc, wchar_t* argv[])
{
    bool shouldUninitializeCom = false;
    {
        const HRESULT comInitHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(comInitHr))
        {
            shouldUninitializeCom = true;
        }
        else if (comInitHr != RPC_E_CHANGED_MODE)
        {
            std::wcerr << L"Failed to initialize COM/WIC (HRESULT=0x" << std::hex << static_cast<unsigned long>(comInitHr) << std::dec << L")\n";
            return 1;
        }
    }

    Args args;
    if (!ParseArgs(argc, argv, args))
    {
        PrintUsage();
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return 1;
    }

    const fs::path inputRoot = fs::absolute(args.inputRoot).lexically_normal();
    const fs::path outputRoot = fs::absolute(args.outputRoot).lexically_normal();
    const fs::path cacheFile = outputRoot / "asset_cooker.cache";

    if (!fs::exists(inputRoot) || !fs::is_directory(inputRoot))
    {
        std::wcerr << L"Input directory does not exist: " << inputRoot << L"\n";
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return 1;
    }

    std::error_code ec;
    fs::create_directories(outputRoot, ec);
    if (ec)
    {
        std::wcerr << L"Failed to create output directory: " << outputRoot << L"\n";
        if (shouldUninitializeCom)
        {
            CoUninitialize();
        }
        return 1;
    }

    CacheTable existingCache;
    if (!args.cleanCache)
    {
        existingCache = LoadCache(cacheFile);
    }
    CacheTable newCache;

    CookStats stats;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(inputRoot))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const fs::path sourcePath = entry.path();
        const bool isTextureSource = IsSupportedTextureExtension(sourcePath);
        const bool isGlbSource = IsSupportedGlbExtension(sourcePath);
        if (!isTextureSource && !isGlbSource)
        {
            continue;
        }

        ++stats.scanned;
        const fs::path relativePath = fs::relative(sourcePath, inputRoot).lexically_normal();
        const std::string cacheKey = relativePath.generic_string();

        const CookSettings hashSettings = isGlbSource ? CookSettings{} : InferCookSettings(sourcePath);
        const std::optional<std::uint64_t> sourceHash = HashFileWithSettings(sourcePath, hashSettings);
        if (!sourceHash.has_value())
        {
            ++stats.failed;
            std::wcerr << L"[ERROR] Failed to hash: " << sourcePath << L"\n";
            continue;
        }

        bool upToDate = false;
        if (!args.force)
        {
            const auto cacheIt = existingCache.find(cacheKey);
            if (cacheIt != existingCache.end() && cacheIt->second == *sourceHash)
            {
                if (isTextureSource)
                {
                    fs::path outputDdsPath = outputRoot / relativePath;
                    outputDdsPath.replace_extension(".dds");
                    fs::path metadataPath = outputDdsPath;
                    metadataPath += L".meta";
                    upToDate = fs::exists(outputDdsPath) && fs::exists(metadataPath);
                }
                else
                {
                    fs::path manifestPath = outputRoot / relativePath;
                    manifestPath.replace_extension();
                    manifestPath /= "materials.json";
                    upToDate = fs::exists(manifestPath);
                }
            }
        }

        if (upToDate)
        {
            ++stats.skipped;
            newCache[cacheKey] = *sourceHash;
            continue;
        }

        bool cookOk = false;
        if (isTextureSource)
        {
            const CookSettings settings = InferCookSettings(sourcePath);
            fs::path outputDdsPath = outputRoot / relativePath;
            outputDdsPath.replace_extension(".dds");
            fs::path metadataPath = outputDdsPath;
            metadataPath += L".meta";

            fs::create_directories(outputDdsPath.parent_path(), ec);
            if (ec)
            {
                std::wcerr << L"[ERROR] Failed to create directory: " << outputDdsPath.parent_path() << L"\n";
                ec.clear();
                ++stats.failed;
                continue;
            }

            cookOk = CookTextureFile(sourcePath, outputDdsPath, metadataPath, relativePath, settings);
            if (cookOk)
            {
                std::wcout << L"[COOKED] " << relativePath.c_str() << L" -> " << fs::relative(outputDdsPath, outputRoot).c_str() << L"\n";
            }
        }
        else
        {
            cookOk = CookGlbFile(sourcePath, outputRoot, relativePath);
        }

        if (cookOk)
        {
            ++stats.cooked;
            newCache[cacheKey] = *sourceHash;
        }
        else
        {
            ++stats.failed;
        }
    }

    if (!SaveCache(cacheFile, newCache))
    {
        std::wcerr << L"[WARN] Failed to save cache file: " << cacheFile << L"\n";
    }

    std::wcout << L"\nAssetCooker summary:\n";
    std::wcout << L"  Scanned: " << stats.scanned << L"\n";
    std::wcout << L"  Cooked:  " << stats.cooked << L"\n";
    std::wcout << L"  Skipped: " << stats.skipped << L"\n";
    std::wcout << L"  Failed:  " << stats.failed << L"\n";

    if (shouldUninitializeCom)
    {
        CoUninitialize();
    }

    return stats.failed > 0 ? 1 : 0;
}
