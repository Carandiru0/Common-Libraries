#include <iostream>
#include <filesystem>
#include <Utility/stringconv.h>

#include "Image.h"

Image::Image()
	: image(nullptr)
{}
Image::Image(std::string_view const path)
	: image(nullptr)
{
	if (!Load(path)) {
		std::cout << "unable to open [image] at: " << path << "\n";
	}
}

bool const Image::Load(std::string_view const path)
{
	namespace fs = std::filesystem;

	// only supporting .ktx2 files - replacing extension used here to override to .ktx2
	// eg.) If the .gltf model references a texture that is a .png
	//      -clone the png to a .ktx2 image w/ ImageViewer
	//      -delete the png
	//      -keep the .ktx2 in the same location as the .gltf file (or have the same relative path)
	//      -keep the file name the same, extension is now .ktx2
	//      -this (below) overrides the file extension to always load those new .ktx2 images with the same filename.
	//      -*currently* blender has no support for .ktx & .ktx2 files, however .gltf is well supported
	std::wstring szKTX2(fs::path(path).stem().wstring() + KTX2_FILE_EXT);
	
	size_t insert_point = path.find_last_of('/');
	if (insert_point) {
		insert_point++;
	}
	
	szKTX2 = stringconv::s2ws(path.substr(0, insert_point)) + szKTX2;

	image = ImagingLoadKTX(szKTX2);

	return(nullptr != image);
}


Image::~Image()
{
	if (image) {
		ImagingDelete(image); image = nullptr;
	}
}
	