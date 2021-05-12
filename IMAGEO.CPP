
#include "imageio.hpp"

#include <vector>
#include <cstdint>
#include <fstream>
#include <memory>

// =======================================================================================================

static inline unsigned char f2b(float x)
{
	if (x < 0.0f)
		return 0;
	else if (x > 255.0f)
		return 255;
	else
		return (unsigned char)x;
}

// =======================================================================================================

//this is interface to work with images

class ImageInterface {
public:
	virtual int Width() const = 0;
	virtual int Height() const = 0;
};

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)

class LockedBitmap : public ImageInterface
{
public:
	LockedBitmap(Gdiplus::Bitmap &B);
	LockedBitmap(Gdiplus::Bitmap &B, Gdiplus::PixelFormat pf, int pixelsize);
	~LockedBitmap();

	int Width() const;
	int Height() const;
	int Stride() const;
	void *Data() const;

private:
	Gdiplus::Bitmap &B;
	Gdiplus::BitmapData bitmapdata;
};

// =======================================================================================================

LockedBitmap::LockedBitmap(Gdiplus::Bitmap &B)
	: LockedBitmap(B, PixelFormat32bppRGB, 4) {}

LockedBitmap::LockedBitmap(Gdiplus::Bitmap &B, Gdiplus::PixelFormat pf, int pixelsize)
	: B(B)
{
	Gdiplus::RectF boundsF;
	Gdiplus::Unit unit = Gdiplus::UnitPixel;
	B.GetBounds(&boundsF, &unit);

	Gdiplus::Rect bounds((int)boundsF.X, (int)boundsF.Y, (int)boundsF.Width, (int)boundsF.Height);
	B.LockBits(&bounds, Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite, pf, &bitmapdata);
}

LockedBitmap::~LockedBitmap()
{
	B.UnlockBits(&bitmapdata);
}

int LockedBitmap::Width() const { return bitmapdata.Width; }
int LockedBitmap::Height() const { return bitmapdata.Height; }
int LockedBitmap::Stride() const { return bitmapdata.Stride; }
void* LockedBitmap::Data() const { return bitmapdata.Scan0; }

// =======================================================================================================

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	//if (size == 0)
	//	return -1;  // Failure
	assert(size > 0);

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	//if (pImageCodecInfo == NULL)
	//	return -1;  // Failure
	assert(pImageCodecInfo != NULL);
	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

// =======================================================================================================

template <typename PixelType, class Converter>
static ImageBase<PixelType> BitmapToImage(Gdiplus::Bitmap &B, Converter conv)
{
	int W = B.GetWidth(), H = B.GetHeight();
	ImageBase<PixelType> res(W, H);
	assert(W > 0 && H > 0);
	if (B.GetPixelFormat() == PixelFormat8bppIndexed)
	{
		int pal_size = B.GetPaletteSize();	// Get palette size in bytes
		Gdiplus::ColorPalette *palette = (Gdiplus::ColorPalette*)malloc(pal_size);
		B.GetPalette(palette, pal_size);
		LockedBitmap lbi(B, PixelFormat8bppIndexed, 1);

		for (int j = 0; j < lbi.Height(); j++)
		{
			unsigned char *line = (unsigned char*)lbi.Data() + j * lbi.Stride();
			for (int i = 0; i < lbi.Width(); i++)
			{
				unsigned char c = line[i];
				if (c >= palette->Count)
					continue;
				
				Gdiplus::ARGB color = palette->Entries[c];
				unsigned char b = (color & 0xFF);
				unsigned char g = ((color >> 8) & 0xFF);
				unsigned char r = ((color >> 16) & 0xFF);

				res(i, j) = conv(ColorBytePixel(b, g, r));
			}
		}
	}
	else
	{
		LockedBitmap lbi(B);
		for (int j = 0; j < lbi.Height(); j++)
		{
			unsigned char *line = (unsigned char*)lbi.Data() + j * lbi.Stride();
			for (int i = 0; i < lbi.Width(); i++)
			{
				unsigned char b = line[i * 4];
				unsigned char g = line[i * 4 + 1];
				unsigned char r = line[i * 4 + 2];
				assert(g >= 0 && r >= 0 && b >= 0);
				res(i, j) = conv(ColorBytePixel(b, g, r));
			}
		}
	}

	return res;
}

template <typename PixelType, class Converter>
static std::unique_ptr<Gdiplus::Bitmap> ImageToBitmap(const ImageBase<PixelType> &image, Converter conv)
{
	std::unique_ptr<Gdiplus::Bitmap> B(new Gdiplus::Bitmap(image.Width(), image.Height(), PixelFormat24bppRGB));
	LockedBitmap lbi(*B);
	assert(image.Width() * image.Height() > 0);

	for (int j = 0; j < image.Height(); j++)
	{
		unsigned char *line = (unsigned char*)lbi.Data() + j * lbi.Stride();
		for (int i = 0; i < image.Width(); i++)
		{
			ColorBytePixel c = conv(image(i, j));
			line[i * 4] = c.b;
			line[i * 4 + 1] = c.g;
			line[i * 4 + 2] = c.r;
		}
	}
	return B;
}

// -----------------------------------------------------------------------------------------------

GrayscaleFloatImage ImageIO::BitmapToGrayscaleFloatImage(Gdiplus::Bitmap &B)
{
	return BitmapToImage<float>(B, [](ColorBytePixel p) { return 0.114f * p.b + 0.587f * p.g + 0.299f * p.r; });
}

GrayscaleByteImage ImageIO::BitmapToGrayscaleByteImage(Gdiplus::Bitmap &B)
{
	return BitmapToImage<unsigned char>(B, [](ColorBytePixel p) { return (unsigned char)(0.114f * p.b + 0.587f * p.g + 0.299f * p.r); });
}

ColorFloatImage ImageIO::BitmapToColorFloatImage(Gdiplus::Bitmap &B)
{
	return BitmapToImage<ColorFloatPixel>(B, [](ColorBytePixel p) { return ColorFloatPixel(p.b, p.g, p.r); });
}

ColorByteImage ImageIO::BitmapToColorByteImage(Gdiplus::Bitmap &B)
{
	return BitmapToImage<ColorBytePixel>(B, [](ColorBytePixel p) { return p; });
}

// -----------------------------------------------------------------------------------------------


GrayscaleFloatImage ImageIO::FileToGrayscaleFloatImage(const char* filename)
{
	assert(filename != "");
	int bufsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
	std::unique_ptr<wchar_t[]> buf(new wchar_t[bufsize + 1]);
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, buf.get(), bufsize);
	buf[bufsize] = 0;

	Gdiplus::Bitmap B(buf.get());
	return BitmapToGrayscaleFloatImage(B);
}

GrayscaleByteImage ImageIO::FileToGrayscaleByteImage(const char* filename)
{
	assert(filename != "");
	int bufsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
	std::unique_ptr<wchar_t[]> buf(new wchar_t[bufsize + 1]);
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, buf.get(), bufsize);
	buf[bufsize] = 0;

	Gdiplus::Bitmap B(buf.get());
	return BitmapToGrayscaleByteImage(B);
}

ColorFloatImage ImageIO::FileToColorFloatImage(const char* filename)
{
	assert(filename != "");
	int bufsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
	std::unique_ptr<wchar_t[]> buf(new wchar_t[bufsize + 1]);
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, buf.get(), bufsize);
	buf[bufsize] = 0;

	Gdiplus::Bitmap B(buf.get());
	return BitmapToColorFloatImage(B);
}

ColorByteImage ImageIO::FileToColorByteImage(const char* filename)
{
	assert(filename != "");
	int bufsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
	std::unique_ptr<wchar_t[]> buf(new wchar_t[bufsize + 1]);
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, buf.get(), bufsize);
	buf[bufsize] = 0;

	Gdiplus::Bitmap B(buf.get());
	return BitmapToColorByteImage(B);
}

// -----------------------------------------------------------------------------------------------

std::unique_ptr<Gdiplus::Bitmap> ImageIO::ImageToBitmap(const GrayscaleFloatImage &image)
{
	return ::ImageToBitmap(image, [](float x)
	{
		unsigned char v = f2b(x);
		return ColorBytePixel(v, v, v);	
	});
}

std::unique_ptr<Gdiplus::Bitmap> ImageIO::ImageToBitmap(const GrayscaleByteImage &image)
{
	return ::ImageToBitmap(image, [](unsigned char x) { return ColorBytePixel(x, x, x); });
}

std::unique_ptr<Gdiplus::Bitmap> ImageIO::ImageToBitmap(const ColorFloatImage &image)
{
	return ::ImageToBitmap(image, [](ColorFloatPixel x) { return ColorBytePixel(f2b(x.b), f2b(x.g), f2b(x.r)); });
}

std::unique_ptr<Gdiplus::Bitmap> ImageIO::ImageToBitmap(const ColorByteImage &image)
{
	return ::ImageToBitmap(image, [](ColorBytePixel x) { return x; });
}

// -----------------------------------------------------------------------------------------------

static void BitmapToFile(Gdiplus::Bitmap &B, const char *filename)
{
	assert(filename != "");
	int bufsize = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
	std::unique_ptr<wchar_t[]> buf(new wchar_t[bufsize + 1]);
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, buf.get(), bufsize);
	buf[bufsize] = 0;

	CLSID pngClsid;
	GetEncoderClsid(L"image/png", &pngClsid);
	B.Save(buf.get(), &pngClsid);
}

void ImageIO::ImageToFile(const GrayscaleFloatImage &image, const char *filename)
{
	assert(filename != "");
	std::unique_ptr<Gdiplus::Bitmap> B = ImageToBitmap(image);
	BitmapToFile(*B, filename);
}

void ImageIO::ImageToFile(const GrayscaleByteImage &image, const char *filename)
{
	assert(filename != "");
	std::unique_ptr<Gdiplus::Bitmap> B = ImageToBitmap(image);
	BitmapToFile(*B, filename);
}

void ImageIO::ImageToFile(const ColorFloatImage &image, const char *filename)
{
	assert(filename != "");
	std::unique_ptr<Gdiplus::Bitmap> B = ImageToBitmap(image);
	BitmapToFile(*B, filename);
}

void ImageIO::ImageToFile(const ColorByteImage &image, const char *filename)
{
	assert(filename != "");
	std::unique_ptr<Gdiplus::Bitmap> B = ImageToBitmap(image);
	BitmapToFile(*B, filename);
}

#else

#pragma pack(push, 1)

struct BITMAPFILEHEADER
{
	int16_t bfType;
	int32_t bfSize;
	int16_t bfReserved1;
	int16_t bfReserved2;
	int32_t bfOffBits;
};

struct BITMAPCOREHEADER
{
	int32_t bcSize;
	int16_t bcWidth;
	int16_t bcHeight;
	int16_t bcPlanes;
	int16_t bcBitCount;
};

struct BITMAPINFOHEADER
{
	int32_t biSize;
	int32_t biWidth;
	int32_t biHeight;
	int16_t biPlanes;
	int16_t biBitCount;
	int32_t biCompression;
	int32_t biSizeImage;
	int32_t biXPelsPerMeter;
	int32_t biYPelsPerMeter;
	int32_t biClrUsed;
	int32_t biClrImportant;
};

#pragma pack(pop)


GrayscaleFloatImage ImageIO::FileToGrayscaleFloatImage(const char* filename)
{
	assert(filename != "");
	ColorByteImage img = FileToColorByteImage(filename);
	GrayscaleFloatImage res(img.Width(), img.Height());

	for (int j = 0; j < img.Height(); j++)
		for (int i = 0; i < img.Width(); i++)
		{
			auto p = img(i, j);
			res(i, j) = 0.114f * p.b + 0.587f * p.g + 0.299f * p.r;
			assert(res(i, j) >= 0);
		}

	return res;
}

GrayscaleByteImage ImageIO::FileToGrayscaleByteImage(const char* filename)
{
	assert(filename != "");
	ColorByteImage img = FileToColorByteImage(filename);
	GrayscaleByteImage res(img.Width(), img.Height());

	for (int j = 0; j < img.Height(); j++)
		for (int i = 0; i < img.Width(); i++)
		{
			auto p = img(i, j);
			res(i, j) = (unsigned char)(0.114f * p.b + 0.587f * p.g + 0.299f * p.r);
			assert(res(i, j) >= 0);
		}

	return res;
}

ColorFloatImage ImageIO::FileToColorFloatImage(const char* filename)
{
	assert(filename != "");
	ColorByteImage img = FileToColorByteImage(filename);
	ColorFloatImage res(img.Width(), img.Height());

	for (int j = 0; j < img.Height(); j++)
		for (int i = 0; i < img.Width(); i++)
		{
			auto p = img(i, j);
			res(i, j) = ColorFloatPixel(p.b, p.g, p.r, p.a);
			assert(res(i, j) >= 0);
		}

	return res;
}

ColorByteImage ImageIO::FileToColorByteImage(const char* filename)
{
	assert(filename != "");
	std::fstream f(filename, std::ios::in | std::ios::binary);

	if (!f.is_open())
		return ColorByteImage(0, 0);

	BITMAPFILEHEADER header;
	f.read((char*)&header, sizeof(BITMAPFILEHEADER));
	if (header.bfType != 0x4D42)
		return ColorByteImage(0, 0);

	int32_t size;
	f.read((char*)&size, sizeof(int32_t));

	int width = 0, height = 0;

	if (size == sizeof(BITMAPCOREHEADER))
	{
		BITMAPCOREHEADER info;
		info.bcSize = size;
		f.read((char*)&info + sizeof(int32_t), sizeof(BITMAPCOREHEADER) - sizeof(int32_t));
		if (info.bcPlanes != 1 || info.bcBitCount != 24)
			return ColorByteImage(0, 0);

		width = info.bcWidth;
		height = info.bcHeight;
		assert(width * height > 0);
	}
	else if (size == sizeof(BITMAPINFOHEADER))
	{
		BITMAPINFOHEADER info;
		info.biSize = size;
		f.read((char*)&info + sizeof(int32_t), sizeof(BITMAPINFOHEADER) - sizeof(int32_t));
		if (info.biPlanes != 1 || info.biBitCount != 24 || info.biCompression != 0)
			return ColorByteImage(0, 0);

		width = info.biWidth;
		height = info.biHeight;
	}
	else
		return ColorByteImage(0, 0);

	f.seekg(header.bfOffBits, std::ios::beg);

	int stride = (width * 3 + 3) / 4 * 4;
	std::unique_ptr<char[]> buffer(new char[stride]);

	ColorByteImage res(width, height);

	for (int j = height - 1; j >= 0; j--)
	{
		f.read(buffer.get(), stride);

		for (int i = 0; i < width; i++)
		{
			res(i, j) = ColorBytePixel(buffer[i * 3], buffer[i * 3 + 1], buffer[i * 3 + 1], 255);
		}
	}

	return res;
}

void ImageIO::ImageToFile(const GrayscaleFloatImage &image, const char *filename)
{
	assert(filename != "");
	ColorByteImage res(image.Width(), image.Height());

	for (int j = 0; j < image.Height(); j++)
		for (int i = 0; i < image.Width(); i++)
		{
			auto p = f2b(image(i, j));
			res(i, j) = ColorBytePixel(p, p, p);
		}

	ImageToFile(res, filename);
}

void ImageIO::ImageToFile(const GrayscaleByteImage &image, const char *filename)
{
	assert(filename != "");
	ColorByteImage res(image.Width(), image.Height());

	for (int j = 0; j < image.Height(); j++)
		for (int i = 0; i < image.Width(); i++)
		{
			auto p = image(i, j);
			res(i, j) = ColorBytePixel(p, p, p);
		}

	ImageToFile(res, filename);

}

void ImageIO::ImageToFile(const ColorFloatImage &image, const char *filename)
{
	assert(filename != "");
	ColorByteImage res(image.Width(), image.Height());

	for (int j = 0; j < image.Height(); j++)
		for (int i = 0; i < image.Width(); i++)
		{
			auto p = image(i, j);
			res(i, j) = ColorBytePixel(f2b(p.b), f2b(p.g), f2b(p.r));
		}

	ImageToFile(res, filename);
}

void ImageIO::ImageToFile(const ColorByteImage &image, const char *filename)
{
	assert(filename != "");
	std::fstream f(filename, std::ios::out | std::ios::trunc | std::ios::binary);

	int stride = (image.Width() * 3 + 3) / 4 * 4;

	BITMAPFILEHEADER header;
	header.bfType = 0x4D42;
	header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPCOREHEADER) + stride * image.Height();
	header.bfReserved1 = 0;
	header.bfReserved2 = 0;
	header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPCOREHEADER);

	BITMAPCOREHEADER info;
	info.bcSize = sizeof(BITMAPCOREHEADER);
	info.bcWidth = image.Width();
	info.bcHeight = image.Height();
	info.bcPlanes = 1;
	info.bcBitCount = 24;

	f.write((char*)&header, sizeof(BITMAPFILEHEADER));
	f.write((char*)&info, sizeof(BITMAPCOREHEADER));

	std::unique_ptr<char[]> buffer(new char[stride]);

	for (int j = image.Height() - 1; j >= 0; j--)
	{
		for (int i = 0; i < image.Width(); i++)
		{
			ColorBytePixel p = image(i, j);
			buffer[i * 3] = p.b;
			buffer[i * 3 + 1] = p.g;
			buffer[i * 3 + 2] = p.r;
		}

		f.write(buffer.get(), stride);
	}

	f.close();
}

#endif
