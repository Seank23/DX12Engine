#pragma once
#include <unordered_set>
#include "../Resources/Materials/Material.h"
#include "../Resources/ResourceManager.h"
#include "DrawItem.h"

namespace DX12Engine
{
	class RenderUtils
	{
	public:
		static void UpdateMaterialBindings(std::vector<DrawItem>& drawItems, int textureCount = 6)
		{
			std::unordered_set<Material*> seen;
			for (const DrawItem& item : drawItems)
			{
				if (!item.Material || !seen.insert(item.Material).second)
					continue;

				item.Material->ClearTextureTable();
				std::vector<GPUResource*> block;
				block.reserve(textureCount);
				for (int i = 0; i < textureCount; i++)
				{
					Texture* tex = item.Material->GetTexture(static_cast<TextureType>(i));
					if (tex && tex->GetIsReady())
						block.push_back(tex);
				}

				if (!block.empty())
				{
					GPUResource* padding = block.front();
					while (block.size() < textureCount)
						block.push_back(padding);

					DescriptorHeapHandle base = ResourceManager::GetInstance().UpdateSRVDescriptors(block);
					item.Material->SetTextureTableHandle(base.GetGPUHandle());
				}
			}
		}
	};
}