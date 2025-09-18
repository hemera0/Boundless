#include "Shaders.hpp"

using namespace Microsoft::WRL;

namespace Boundless {
	ShaderCompiler::ShaderCompiler() {
		HRESULT hres = DxcCreateInstance( CLSID_DxcLibrary, IID_PPV_ARGS( m_Library.GetAddressOf() ) );
		if ( FAILED( hres ) ) {
			printf( "Could not init DXC Library\n" );
		}

		// Initialize DXC compiler
		hres = DxcCreateInstance( CLSID_DxcCompiler, IID_PPV_ARGS( m_Compiler.GetAddressOf() ) );
		if ( FAILED( hres ) ) {
			printf( "Could not init DXC Compiler\n" );
		}

		// Initialize DXC utility
		hres = DxcCreateInstance( CLSID_DxcUtils, IID_PPV_ARGS( m_Utils.GetAddressOf() ) );
		if ( FAILED( hres ) ) {
			printf( "Could not init DXC Utiliy\n" );
		}
	}

	IDxcBlob* ShaderCompiler::CompileShader( const std::wstring& shaderPath, ShaderType shaderType ) {
		uint32_t codePage = DXC_CP_ACP;

		ComPtr<IDxcBlobEncoding> sourceBlob;
		HRESULT hres = m_Utils->LoadFile( shaderPath.c_str(), &codePage, sourceBlob.GetAddressOf() );
		if ( FAILED( hres ) ) {
			printf( "Could not load shader file: %ls\n", shaderPath.c_str() );
			return nullptr;
		}

		ComPtr<IDxcIncludeHandler> includeHandler;
		m_Utils->CreateDefaultIncludeHandler( includeHandler.GetAddressOf() );

		DxcBuffer buffer{};
		buffer.Encoding = DXC_CP_ACP;
		buffer.Ptr = sourceBlob->GetBufferPointer();
		buffer.Size = sourceBlob->GetBufferSize();

		LPCWSTR targetProfile = nullptr;
		switch ( shaderType ) {
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

		ComPtr<IDxcResult> result = nullptr;
		hres = m_Compiler->Compile( &buffer, arguments.data(), uint32_t( arguments.size() ), includeHandler.Get(), IID_PPV_ARGS( result.GetAddressOf() ) );
		if ( SUCCEEDED( hres ) ) {
			result->GetStatus( &hres );
		}

		if ( FAILED( hres ) && ( result ) ) {
			ComPtr<IDxcBlobEncoding> errorBlob;
			hres = result->GetErrorBuffer( errorBlob.GetAddressOf() );
			if ( SUCCEEDED( hres ) && errorBlob ) {
				printf( "Shader compilation failed: \n\n %s", ( const char* )errorBlob->GetBufferPointer() );
				return nullptr;
			}
		}

		printf( "Compiled shader: %ls\n", shaderPath.c_str() );

		IDxcBlob* code = nullptr;
		result->GetResult( &code );
		return code;
	}
}