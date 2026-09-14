#pragma once
#include <DirectXMath.h>
#include "./ConstantBuffer.h"
#include "../../Resources/Light.h"
#include <unordered_map>

namespace DX12Engine
{
	class LightBuffer
	{
	public:
		LightBuffer();
		~LightBuffer();

		void Update();
		void AddLight(std::shared_ptr<Light> light);

		const std::vector<LightData>& GetUploadData() const { return m_LightData; }
		uint32_t GetDirectionalCount() const { return m_DirectionalLightCount; }

		Light* GetLight(int index) { return m_Lights[index].get(); }
		int GetLightCount() { return m_Lights.size(); }

		std::vector<Light*> GetAllLights();
		std::vector<Light*> GetLightsByType(std::vector<LightType> types);
		std::vector<Light*> GetShadowCastingLightsByType(std::vector<LightType> types);

	private:
		std::unordered_map<LightType, std::vector<int>> m_LightsByTypeMap;
		std::vector<std::shared_ptr<Light>> m_Lights;
		std::vector<LightData> m_LightData;
		uint32_t m_DirectionalLightCount = 0;
		uint32_t m_SpotShadowSlots = 0, m_PointShadowSlots = 0;
	};
}
