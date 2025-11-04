#pragma once
#include "VkUtil.hpp"

#include <dxcapi.h>
#include <wrl/client.h>

namespace Boundless {
	enum class ShaderType {
		PixelShader,
		VertexShader,
		ComputeShader
	};

	class ShaderCompiler {
	public:
		ShaderCompiler();

		IDxcBlob* CompileShader( const std::wstring& shaderPath, ShaderType shaderType );
	private:
		Microsoft::WRL::ComPtr<IDxcLibrary>	  m_Library = nullptr;
		Microsoft::WRL::ComPtr<IDxcCompiler3> m_Compiler = nullptr;
		Microsoft::WRL::ComPtr<IDxcUtils>	  m_Utils = nullptr;
	};
}