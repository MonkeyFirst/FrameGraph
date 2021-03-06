// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#include "../FGApp.h"

namespace FG
{

	static EPixelFormat ChooseDepthStencilFormat (const VulkanDevice &dev)
	{
		const Pair<VkFormat, EPixelFormat> formats[] = {
			{ VK_FORMAT_D24_UNORM_S8_UINT, EPixelFormat::Depth24_Stencil8 },
			{ VK_FORMAT_D32_SFLOAT_S8_UINT, EPixelFormat::Depth32F_Stencil8 },
			{ VK_FORMAT_D16_UNORM_S8_UINT, EPixelFormat::Depth16_Stencil8 }
		};

		for (auto fmt : formats)
		{
			VkFormatProperties2	props = {};
			props.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
			vkGetPhysicalDeviceFormatProperties2( dev.GetVkPhysicalDevice(), fmt.first, OUT &props );

			VkFormatFeatureFlags	curr	 = props.formatProperties.optimalTilingFeatures;
			VkFormatFeatureFlags	required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

			if ( EnumEq( curr, required ) )
				return fmt.second;
		}
		RETURN_ERR( "no suitable depth-stencil format found" );
	}


	bool FGApp::Test_ReadAttachment1 ()
	{
		GraphicsPipelineDesc	ppln;

		ppln.AddShader( EShader::Vertex, EShaderLangFormat::VKSL_100, "main", R"#(
#pragma shader_stage(vertex)
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

out vec3  v_Color;

const vec2	g_Positions[3] = vec2[](
	vec2(0.0, -0.5),
	vec2(0.5, 0.5),
	vec2(-0.5, 0.5)
);

const vec3	g_Colors[3] = vec3[](
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);

void main() {
	gl_Position	= vec4( g_Positions[gl_VertexIndex], 0.0, 1.0 );
	v_Color		= g_Colors[gl_VertexIndex];
}
)#" );
		
		ppln.AddShader( EShader::Fragment, EShaderLangFormat::VKSL_100, "main", R"#(
#pragma shader_stage(fragment)
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(binding=0) uniform sampler2D  un_DepthImage;

layout(location=0) out vec4  out_Color;

in  vec3  v_Color;

void main() {
	out_Color = vec4(v_Color * texelFetch(un_DepthImage, ivec2(gl_FragCoord.xy), 0).r, 1.0);
}
)#" );
		
		const uint2		view_size	= {800, 600};
		EPixelFormat	ds_format	= ChooseDepthStencilFormat( _vulkan );

		ImageID			color_image	= _frameGraph->CreateImage( ImageDesc{ EImage::Tex2D, uint3{view_size.x, view_size.y, 1}, EPixelFormat::RGBA8_UNorm,
																			EImageUsage::ColorAttachment | EImageUsage::TransferSrc }, Default, "ColorTarget" );
		ImageID			depth_image	= _frameGraph->CreateImage( ImageDesc{ EImage::Tex2D, uint3{view_size.x, view_size.y, 1}, ds_format,
																			EImageUsage::DepthStencilAttachment | EImageUsage::TransferDst | EImageUsage::Sampled },
																			Default, "DepthTarget" );

		SamplerID		sampler		= _frameGraph->CreateSampler( SamplerDesc{} );

		GPipelineID		pipeline	= _frameGraph->CreatePipeline( ppln );
		
		PipelineResources	resources;
		CHECK_ERR( _frameGraph->InitPipelineResources( pipeline, DescriptorSetID("0"), OUT resources ));

		
		bool		data_is_correct = false;

		const auto	OnLoaded =	[OUT &data_is_correct] (const ImageView &imageData)
		{
			const auto	TestPixel = [&imageData] (float x, float y, const RGBA32f &color)
			{
				uint	ix	 = uint( (x + 1.0f) * 0.5f * float(imageData.Dimension().x) + 0.5f );
				uint	iy	 = uint( (y + 1.0f) * 0.5f * float(imageData.Dimension().y) + 0.5f );

				RGBA32f	col;
				imageData.Load( uint3(ix, iy, 0), OUT col );

				bool	is_equal	= Equals( col.r, color.r, 0.1f ) and
									  Equals( col.g, color.g, 0.1f ) and
									  Equals( col.b, color.b, 0.1f ) and
									  Equals( col.a, color.a, 0.1f );
				ASSERT( is_equal );
				return is_equal;
			};

			data_is_correct  = true;
			data_is_correct &= TestPixel( 0.00f, -0.49f, RGBA32f{1.0f, 0.0f, 0.0f, 1.0f} );
			data_is_correct &= TestPixel( 0.49f,  0.49f, RGBA32f{0.0f, 1.0f, 0.0f, 1.0f} );
			data_is_correct &= TestPixel(-0.49f,  0.49f, RGBA32f{0.0f, 0.0f, 1.0f, 1.0f} );
			
			data_is_correct &= TestPixel( 0.00f, -0.51f, RGBA32f{0.0f} );
			data_is_correct &= TestPixel( 0.51f,  0.51f, RGBA32f{0.0f} );
			data_is_correct &= TestPixel(-0.51f,  0.51f, RGBA32f{0.0f} );
			data_is_correct &= TestPixel( 0.00f,  0.51f, RGBA32f{0.0f} );
			data_is_correct &= TestPixel( 0.51f, -0.51f, RGBA32f{0.0f} );
			data_is_correct &= TestPixel(-0.51f, -0.51f, RGBA32f{0.0f} );
		};

		
		CommandBuffer	cmd = _frameGraph->Begin( CommandBufferDesc{}.SetDebugFlags( EDebugFlags::Default ));
		CHECK_ERR( cmd );

		LogicalPassID	render_pass	= cmd->CreateRenderPass( RenderPassDesc( view_size )
												.AddTarget( RenderTargetID::Color_0, color_image, RGBA32f(0.0f), EAttachmentStoreOp::Store )
												.AddTarget( RenderTargetID::Depth, depth_image, EAttachmentLoadOp::Load, EAttachmentStoreOp::Store )
												.SetDepthTestEnabled( true ).SetDepthWriteEnabled( false )
												.AddViewport( view_size ) );
		
		ImageViewDesc	view_desc;	view_desc.aspectMask = EImageAspect::Depth;
		resources.BindTexture( UniformID("un_DepthImage"), depth_image, sampler, view_desc );

		cmd->AddTask( render_pass, DrawVertices().Draw( 3 ).SetPipeline( pipeline )
												.SetTopology( EPrimitive::TriangleList )
												.AddResources( DescriptorSetID("0"), &resources ));

		Task	t_clear	= cmd->AddTask( ClearDepthStencilImage{}.SetImage( depth_image ).Clear( 1.0f ).AddRange( 0_mipmap, 1, 0_layer, 1 ));
		Task	t_draw	= cmd->AddTask( SubmitRenderPass{ render_pass }.DependsOn( t_clear ));
		Task	t_read	= cmd->AddTask( ReadImage().SetImage( color_image, int2(), view_size ).SetCallback( OnLoaded ).DependsOn( t_draw ) );
		FG_UNUSED( t_read );

		CHECK_ERR( _frameGraph->Execute( cmd ));
		CHECK_ERR( _frameGraph->WaitIdle() );
		
		CHECK_ERR( CompareDumps( TEST_NAME ));
		CHECK_ERR( Visualize( TEST_NAME ));

		CHECK_ERR( data_is_correct );

		DeleteResources( color_image, depth_image, sampler, pipeline );

		FG_LOGI( TEST_NAME << " - passed" );
		return true;
	}

}	// FG
