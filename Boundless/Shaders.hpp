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

	// TODO: Finish this.
	class Shader {
	public:
		Shader() = default;
		explicit Shader( IDxcBlob* byteCode, ShaderType shaderType ) : m_ByteCode( byteCode ), m_Type( shaderType ) { }

		ShaderType GetShaderType() const { return m_Type; }

		void* GetBuffer() { return m_ByteCode->GetBufferPointer(); }
		size_t GetSize() { return m_ByteCode->GetBufferSize(); }
	private:
		IDxcBlob*  m_ByteCode = nullptr;
		ShaderType m_Type = ShaderType::PixelShader;
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