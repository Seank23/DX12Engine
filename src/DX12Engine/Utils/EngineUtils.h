#include <stdexcept>
#include <string>
#include <comdef.h>

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
	};
}