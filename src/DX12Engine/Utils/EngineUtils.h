#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>
#define NOMINMAX
#include <windows.h>
#include <DirectXMath.h>

namespace DX12Engine
{
	class EngineUtils
	{
	public:
		static std::string FormatHRESULT(HRESULT hr)
		{
			std::ostringstream oss;
			oss << "HRESULT 0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);

			LPSTR messageBuffer = nullptr;
			DWORD messageLength = FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr,
				static_cast<DWORD>(hr),
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				reinterpret_cast<LPSTR>(&messageBuffer),
				0,
				nullptr);

			if (messageLength > 0 && messageBuffer)
			{
				while (messageLength > 0 && (messageBuffer[messageLength - 1] == '\r' || messageBuffer[messageLength - 1] == '\n'))
					messageLength--;

				oss << ": " << std::string(messageBuffer, messageLength);
				LocalFree(messageBuffer);
			}
			else if (messageBuffer)
			{
				LocalFree(messageBuffer);
			}

			return oss.str();
		}

		static void ThrowIfFailed(HRESULT hr)
		{
			if (FAILED(hr))
			{
				throw std::runtime_error(FormatHRESULT(hr));
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

		template <typename T>
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