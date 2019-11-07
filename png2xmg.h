#ifndef __PNG2XMG_H__
#define __PNG2XMG_H__

#include <png.h>

typedef unsigned char TUint8;
typedef unsigned short TUint16;
typedef unsigned int TUint32;
typedef unsigned int TUint;

#define ALPHA_BITS_DEFAULT		4
#define MASK_COLOR_DEFAULT		0xf0f

struct PALETTE
{
	int freq;
	TUint16 color;

	PALETTE() : freq(0), color(0) {}
	PALETTE& PALETTE::operator=(const PALETTE& pal)
	{
		freq = pal.freq;
		color = pal.color;
		return *this;
	}
};

struct XMG_HEADER
{
	TUint16		iVersion;	// reserved
	TUint8		iBPP;		// 1 or 12
	TUint8		iAlphaBits;	// 0 or 4 or 8
	int			iPalSize;
	short		iWidth;
	short		iHeight;
	int			iDataSize;

	XMG_HEADER(int alpha_bits) : iAlphaBits(alpha_bits) {}
};

class PNG2XMG : public XMG_HEADER
{
public:
	PNG2XMG(const char* png_filename,int alpha_bits,TUint16 mask_color);
	~PNG2XMG();
	bool read();
	void write(const char* xmg_filename);
//	void write32b(const char* xmg_filename);
//	void write16b(const char* xmg_filename);
	void convert();
	void convert32b();
//	void convert16b();
	bool read_ibm();
	void convert_ibm();

public:
	png_struct*   png_ptr;
	png_info*     info_ptr;
	png_byte      buf[8];
	png_byte*     png_pixels;
	png_byte**    row_pointers;
	png_byte*     pix_ptr;
	png_uint_32   row_bytes;

	png_uint_32   width;
	png_uint_32   height;
	int           bit_depth;
	int           color_type;

	FILE*         fp_png;
	const char*   png_filename;
	bool          alphaUsed;
	int           alpha_width;
	TUint         mask_color;

	TUint16*      iPalette;	// iPalette[0]存储mask_color(如果无,iPalette[0]=0), 如果iData[y][x]==0, 则此处(x,y)为100%透明
	TUint32*      iPalette32;	// iPalette[0]存储mask_color(如果无,iPalette[0]=0), 如果iData[y][x]==0, 则此处(x,y)为100%透明
	TUint8*       iData;
	TUint8**      iAlpha;	// iW*iH if alpha in 8bits; (iW/2+(iW&1))*iH if alpha in 4bits
};

#define min(a,b)			((a)<(b)?(a):(b))
#define RGB8B_TO4B(a)		(((a)&0xf)>0x7 ? min(0xf,(((a)>>4)&0xf)+1) : (((a)>>4)&0xf))
#define RGB444_R(a)			(((a)>>8)&0xf)
#define RGB444_G(a)			(((a)>>4)&0xf)
#define RGB444_B(a)			((a)&0xf)
#define RGB888_R(a)			(((a)>>16)&0xff)
#define RGB888_G(a)			(((a)>>8)&0xff)
#define RGB888_B(a)			((a)&0xff)

#endif	// __PNG2XMG_H__
