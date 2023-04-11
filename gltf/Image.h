#pragma once
#include <string_view>
#include <Imaging/Imaging/Imaging.h>

#ifndef KTX_FILE_EXT
#define KTX_FILE_EXT L".ktx"
#endif

#ifndef KTX2_FILE_EXT
#define KTX2_FILE_EXT L".ktx2"
#endif

class Image
{

public:
	ImagingMemoryInstance const* const Get() const { return(image); }

	bool const Load(std::string_view const path);

private:
	Imaging     image;

public:
	Image();
	Image(std::string_view const path);
	~Image();
};

struct Texture
{
	uint32_t image_index;

	Texture()
		: image_index(0)
	{}

	Texture(int32_t const index)
		: image_index(index)
	{}
};

struct Material
{
	float   metallic, 
		    roughness;
	bool    emission,
		    transparent;

	uint32_t image_index;

	Material()
		: metallic(0.0f), roughness(0.0f), emission(false), transparent(false), image_index(0)
	{}

	Material(uint32_t const index)
		: metallic(0.0f), roughness(0.0f), emission(false), transparent(false),
		  image_index(index)
	{}
};