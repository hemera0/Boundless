#include "Shaders.hpp"

Boundless::ShaderCompiler* Boundless::ShaderCompiler::s_Instance = nullptr;

void Boundless::ShaderCompiler::Create() {
	HRESULT hres = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&m_Library));
	if (FAILED(hres)) {
		printf("Could not init DXC Library\n");
	}

	// Initialize DXC compiler
	hres = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_Compiler));
	if (FAILED(hres)) {
		printf("Could not init DXC Compiler\n");
	}

	// Initialize DXC utility
	hres = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_Utils));
	if (FAILED(hres)) {
		printf("Could not init DXC Utiliy\n");
	}
}

void Boundless::ShaderCompiler::Destroy() {
	m_Library->Release();
	m_Compiler->Release();
	m_Utils->Release();

	m_Library = nullptr;
	m_Compiler = nullptr;
	m_Utils = nullptr;
}

IDxcBlob* Boundless::ShaderCompiler::CompileShader(const std::wstring& shaderPath, ShaderType shaderType)
{
	uint32_t codePage = DXC_CP_ACP;
	IDxcBlobEncoding* sourceBlob;
	HRESULT hres = m_Utils->LoadFile(shaderPath.c_str(), &codePage, &sourceBlob);
	if (FAILED(hres)) {
		printf("Could not load shader file: %ls\n", shaderPath.c_str());
		return nullptr;
	}

	DxcBuffer buffer{};
	buffer.Encoding = DXC_CP_ACP;
	buffer.Ptr = sourceBlob->GetBufferPointer();
	buffer.Size = sourceBlob->GetBufferSize();

	LPCWSTR targetProfile = nullptr;
	switch (shaderType) {
	case ShaderType::PixelShader:
		targetProfile = L"ps_6_9";
		break;
	case ShaderType::VertexShader:
		targetProfile = L"vs_6_9";
		break;
	case ShaderType::ComputeShader:
		targetProfile = L"cs_6_9";
		break;
	}

	std::vector<LPCWSTR> arguments = {
		shaderPath.c_str(), // (Optional) name of the shader file to be displayed e.g. in an error message
		L"-E", L"main", // Shader main entry point
		L"-T", targetProfile, // Shader target profile
		L"-spirv" // Compile to SPIRV
	};

	IDxcResult* result = nullptr;
	hres = m_Compiler->Compile(&buffer, arguments.data(), uint32_t(arguments.size()), nullptr, IID_PPV_ARGS(&result));
	if (SUCCEEDED(hres)) {
		result->GetStatus(&hres);
	}

	if (FAILED(hres) && (result)) {
		IDxcBlobEncoding* errorBlob = nullptr;
		hres = result->GetErrorBuffer(&errorBlob);
		if (SUCCEEDED(hres) && errorBlob) {
			printf("Shader compilation failed: \n\n %s", (const char*)errorBlob->GetBufferPointer());
			return nullptr;
		}
	}

	printf("Compiled shader: %ls\n", shaderPath.c_str());

	IDxcBlob* code = nullptr;
	result->GetResult(&code);

	return code;
}