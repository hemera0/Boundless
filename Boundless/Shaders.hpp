#pragma once
#include "VkUtil.hpp"
#include <dxcapi.h>

namespace Boundless {
	enum class ShaderType {
		PixelShader,
		VertexShader,
		ComputeShader
	};

	class ShaderCompiler {
	public:
		static ShaderCompiler* s_Instance;

		ShaderCompiler() {
			s_Instance = this;

			Create();
		}

		~ShaderCompiler() {
			Destroy();
		}

		static ShaderCompiler* GetInstance() {
			return s_Instance;
		}

		void Create();
		void Destroy();

		IDxcBlob* CompileShader(const std::wstring& shaderPath, ShaderType shaderType);
	private:
		IDxcLibrary* m_Library = nullptr;
		IDxcCompiler3* m_Compiler = nullptr;
		IDxcUtils* m_Utils = nullptr;
	};
}