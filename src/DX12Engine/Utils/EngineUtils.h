#include <stdexcept>
#include <string>
#include <comdef.h>
#include <DirectXMath.h>

namespace DX12Engine
{
	class EngineUtils
	{
	public:
		static void ThrowIfFailed(HRESULT hr)
		{
			if (FAILED(hr))
			{
				_com_error err(hr);
				std::string errMsg = err.ErrorMessage();
				throw std::runtime_error(std::string(errMsg.begin(), errMsg.end()));
			}
		}

		static void Assert(bool condition)
		{
			if (!condition)
			{
				throw std::runtime_error("Assertion failed.");
			}
		}

		static UINT AlignUINT(UINT value, UINT placement)
		{
			UINT alignmentCount = ceil((float)value / placement);
			return alignmentCount * placement;
		}

		template<typename T>
		static std::vector<T*> VectorSharedPtrToPtrs(const std::vector<std::shared_ptr<T>>& vec)
		{
			std::vector<T*> result;
			result.reserve(vec.size());
			for (const auto& item : vec)
			{
				result.push_back(item.get());
			}
			return result;
		}

		static DirectX::XMFLOAT3 ConvertToXMFLOAT3(const DirectX::XMVECTOR& vec)
		{
			return DirectX::XMFLOAT3(DirectX::XMVectorGetX(vec), DirectX::XMVectorGetY(vec), DirectX::XMVectorGetZ(vec));
		}
	};
}