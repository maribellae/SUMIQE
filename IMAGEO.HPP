#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)

#include <Windows.h>
#include <GdiPlus.h>

#endif

#include "imageformats.hpp"

class ImageIO
{
public:

	static GrayscaleFloatImage FileToGrayscaleFloatImage(const char* filename);
	static GrayscaleByteImage FileToGrayscaleByteImage(const char* filename);
	static ColorFloatImage FileToColorFloatImage(const char* filename);
	static ColorByteImage FileToColorByteImage(const char* filename);

	static void ImageToFile(const GrayscaleFloatImage &image, const char *filename);
	static void ImageToFile(const GrayscaleByteImage &image, const char *filename);
	static void ImageToFile(const ColorFloatImage &image, const char *filename);
	static void ImageToFile(const ColorByteImage &image, const char *filename);


#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)

private:
	static GrayscaleFloatImage BitmapToGrayscaleFloatImage(Gdiplus::Bitmap &B);
	static GrayscaleByteImage BitmapToGrayscaleByteImage(Gdiplus::Bitmap &B);
	static ColorFloatImage BitmapToColorFloatImage(Gdiplus::Bitmap &B);
	static ColorByteImage BitmapToColorByteImage(Gdiplus::Bitmap &B);

	static std::unique_ptr<Gdiplus::Bitmap> ImageToBitmap(const GrayscaleFloatImage &image);
	static std::unique_ptr<Gdiplus::Bitmap> ImageToBitmap(const GrayscaleByteImage &image);
	static std::unique_ptr<Gdiplus::Bitmap> ImageToBitmap(const ColorFloatImage &image);
	static std::unique_ptr<Gdiplus::Bitmap> ImageToBitmap(const ColorByteImage &image);

#endif
};
