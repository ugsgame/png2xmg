/*
 *  PNG2XMG - Conversion from PNG-file to XMG-file
 *  Author: ER1C <ER1C.Wang@hotmail.com>
 *
 *  v1.00 - 2007.09.18, ER1C
 *          Initial version, supports various PNG format(PNG24a,PNG24,PNG8,PNG8a)
 *  v1.01 - 2008.02.05, ER1C
 *          ...
 *  v1.02 - 2008.03.11, ER1C
 *          convert32b added;
 *          Bug fixed: maskColor conflicts with the image color while alphaBits!=0;
 *  v1.10 - 2009.07.18, ER1C
 *          32bits-TrueColor supported
 *  v1.11 - 2009.07.27, ER1C
 *          BMP-8 supported, for palette-RXImage used in avatar
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "png2xmg.h"

#define PNG2XMG_VERSION "1.10.90718"

const int _ARG_MAX = 8;
void xlog_file(const char* msg)
{
	printf(msg);
	FILE* fp = fopen("png2xmg.log", "a+");
	if (!fp) return;
	fwrite(msg, strlen(msg), 1, fp);
	fclose(fp);
}

#define xlog1(a)			xlog_file(a)

#define xlog2(a,b)			{ \
								char buf[1024]={0}; \
								sprintf(buf, a, b); \
								xlog_file(buf); \
							}

#define xlog3(a,b,c)		{ \
								char buf[1024]={0}; \
								sprintf(buf, a, b, c); \
								xlog_file(buf); \
							}

#define xlog4(a,b,c,d)		{ \
								char buf[1024]={0}; \
								sprintf(buf, a, b, c, d); \
								xlog_file(buf); \
							}

#define xlog5(a,b,c,d,e)	{ \
								char buf[1024]={0}; \
								sprintf(buf, a, b, c, d, e); \
								xlog_file(buf); \
							}

const int DATA_SIZE_MAX = 1024*400;	//最大400K数据

#define RGB888_TO_444(rgb)	(((rgb&0xf00000)>>12) | ((rgb&0xf000)>>8) | ((rgb&0xf0)>>4))

PNG2XMG::PNG2XMG(const char* png_filename,int alpha_bits,TUint16 mask_color)
:	XMG_HEADER(alpha_bits),
	fp_png(NULL),png_ptr(NULL),info_ptr(NULL),png_pixels(NULL),row_pointers(NULL),pix_ptr(NULL),
	png_filename(png_filename),mask_color(mask_color),iData(NULL),width(0),height(0)
{
}

// TODO: truncate the space at right-border & bottom-border
bool PNG2XMG::read()
{
	int i, ret;

	if ((fp_png = fopen(png_filename, "rb")) == NULL)
	{
		xlog1("PNG2XMG\n");
		xlog2("Error: file %s does not exist\n", png_filename);
		exit(1);
	}
	ret = fread(buf, 1, 8, fp_png);
	if (ret != 8)
	{
		return false;
	}
	ret = png_check_sig(buf, 8);
	if (!ret)
	{
		return false;
	}
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	NULL, NULL, NULL);
	if (!png_ptr)
	{
		return false;   /* out of memory */
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		return false;   /* out of memory */
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return false;
	}

	png_init_io(png_ptr, fp_png);
	png_set_sig_bytes(png_ptr, 8);  /* we already read the 8 signature bytes */
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
	if (color_type == PNG_COLOR_TYPE_PALETTE)
	{
		png_set_expand(png_ptr);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
	{
		png_set_expand(png_ptr);
	}
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
	{
		png_set_expand(png_ptr);
	}
	png_read_update_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);
	row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	if ((png_pixels = (png_byte *) malloc(row_bytes * height * sizeof(png_byte))) == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return false;
	}
	if ((row_pointers = (png_byte **) malloc(height * sizeof(png_bytep))) == NULL)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		free(png_pixels);
		png_pixels = NULL;
		return false;
	}
	for (i = 0; i < (int)height; i++)
	{
		row_pointers[i] = png_pixels + i * row_bytes;
	}
	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, info_ptr);
	//--- end of reading png ---

	return true;
}

PNG2XMG::~PNG2XMG()
{
	delete[] iData;
	if (!png_ptr) return;
	png_destroy_read_struct(&png_ptr, &info_ptr,(png_infopp) NULL);
	if (row_pointers != NULL)
	{
		free(row_pointers);
	}
	if (png_pixels != NULL)
	{
		free(png_pixels);
	}
	fclose(fp_png);
}

// TODO: add dither/pattern/noise algorithms for converting from 24b to 12b
void PNG2XMG::convert()
{
	TUint32 x,y;
	int r,g,b,i,j;

	alphaUsed = !(info_ptr->channels==3 && info_ptr->pixel_depth==24);

	if (alphaUsed && info_ptr->channels==4 && info_ptr->pixel_depth==32)
	{
		alphaUsed = false;
		for (y=0; y<height; y++)
		{
			for (x=0; x<width; x++)
			{
				if (png_pixels[(y*width+x)*4+3] < 0xff)
				{
					alphaUsed = true;
					break;
				}
			}
			if (alphaUsed)
			{
				break;
			}
		}
	}

	if (!alphaUsed) iAlphaBits = 0;
	iBPP = 12;	// bit_depth 1 should from BMP
	iVersion = 1;
	iWidth = (short)width;
	iHeight = (short)height;

	TUint16** pxl_12b = new TUint16*[height];
	TUint8** pxl_idx = new TUint8*[height];
	TUint8** alpha = new TUint8*[height];
	TUint8* tmpData = new TUint8[DATA_SIZE_MAX];

	// STEP 1: convert 24bpp to 12bpp
	if (!alphaUsed)	// no alpha channel
	{
		for (y=0; y<height; y++)
		{
			pxl_12b[y] = new TUint16[width];
			alpha[y] = new unsigned char[width];
			memset(alpha[y], 0xff, width);

			for (x=0; x<width; x++)
			{
				int offset = (y*width+x)*info_ptr->channels;
				pxl_12b[y][x] =
					(RGB8B_TO4B(png_pixels[offset+0]) << 8) |
					(RGB8B_TO4B(png_pixels[offset+1]) << 4) |
					(RGB8B_TO4B(png_pixels[offset+2]) << 0);
				if (pxl_12b[y][x] == mask_color)
					alpha[y][x] = 0;
			}
		}
	}
	else if (iAlphaBits == 4 || iAlphaBits == 8)	// iAlpha data used only when iAlphaBits==4
	{
		if (iAlphaBits == 4)
		{
			iAlpha = new TUint8*[height];
			alpha_width = width/2 + (width&1);
		}
		for (y=0; y<height; y++)
		{
			pxl_12b[y] = new TUint16[width];
			alpha[y] = new unsigned char[width];
			if (iAlphaBits == 4)
			{
				iAlpha[y] = new TUint8[alpha_width];
				memset(iAlpha[y], 0, alpha_width);
			}
			memset(pxl_12b[y], 0, width*2);

			for (x=0; x<width; x++)
			{
				int offset = (y*width+x)*4;
				if (iAlphaBits == 8)
					alpha[y][x] = png_pixels[offset+3];
				else
					alpha[y][x] = (png_pixels[offset+3] & 0xf0) >> 4;
				/*
				if (alpha[y][x] == 0)
				{
					pxl_12b[y][x] = mask_color;
				}
				else
				*/
				if (alpha[y][x])
				{
					pxl_12b[y][x] =
						(RGB8B_TO4B(png_pixels[offset+0]) << 8) |
						(RGB8B_TO4B(png_pixels[offset+1]) << 4) |
						(RGB8B_TO4B(png_pixels[offset+2]) << 0);
				}
				if (iAlphaBits == 4)
					iAlpha[y][x/2] |= alpha[y][x] << (x&1)*4;
			}
		}
	}

	// STEP 1.5: guarantee the maskColor not duplicated on the image while alphaBits!=0
	/*
	// TODO: is this necessary ? If needed, should be improved.
	if (iAlphaBits != 0)
	{
_MASK_COLOR_REFRESH:
		for (y=0; y<height; y++)
		{
			for (x=0; x<width; x++)
			{
				if (pxl_12b[y][x] == mask_color)
				{
					xlog3("\n>>> WARNING: MaskColor changed from 0x%x to 0x%x !!!\n", mask_color, 0x800 | (mask_color>>1));
					mask_color = 0x800 | (mask_color>>1);
					goto _MASK_COLOR_REFRESH;
				}
			}
		}
	}
	*/

	// STEP 2: create palette[4096]
	PALETTE* pal4k = new PALETTE[4096];
	memset(pal4k, 0, sizeof(PALETTE)*4096);
	pal4k[0].color = mask_color;
	pal4k[0].freq=0;
	int palUsed=1;
	bool existed=false;
	for (y=0; y<height; y++)
	{
		for (x=0; x<width; x++)
		{
			if (pxl_12b[y][x]==mask_color)
			{
				pal4k[0].freq++;
				continue;
			}
			existed=false;
			for (int u=1; u<palUsed; u++)
			{
				if (pal4k[u].color==pxl_12b[y][x])
				{
					pal4k[u].freq++;
					existed=true;
					break;
				}
			}
			if (!existed)
			{
				pal4k[palUsed].color=pxl_12b[y][x];
				pal4k[palUsed].freq=1;
				palUsed++;
			}
		}
	}

	// STEP 2.1: sort by frequence
	PALETTE swap;
	for (i=1; i<palUsed; i++)
	{
		for (j=i; j<palUsed; j++)
		{
			if (pal4k[j].freq>pal4k[i].freq)
			{
				swap=pal4k[j];
				pal4k[j]=pal4k[i];
				pal4k[i]=swap;
			}
		}
	}

	// STEP 2.2: create iPalette data
	iPalSize = min(palUsed, 256);
	iPalette = new TUint16[iPalSize];
	for (i=0; i<iPalSize; i++)
	{
		iPalette[i] = pal4k[i].color;
	}

	// STEP 2.3: reduce palette from 4096 to 255
	TUint8 mapIndex[4096-256];
	int r1,g1,b1,diff,difference,nearestIndex;
	for (i=256; i<palUsed; i++)
	{
		nearestIndex=-1;
		difference = 0x7fffffff;	// max int
		for (j=1; j<256; j++)
		{
			r1=RGB444_R(pal4k[i].color);
			g1=RGB444_G(pal4k[i].color);
			b1=RGB444_B(pal4k[i].color);
			r=RGB444_R(pal4k[j].color);
			g=RGB444_G(pal4k[j].color);
			b=RGB444_B(pal4k[j].color);
			diff=(r1-r)*(r1-r)+(g1-g)*(g1-g)+(b1-b)*(b1-b);
			if (diff<difference)
			{
				difference=diff;
				nearestIndex=j;
			}
		}
		mapIndex[i-256]=nearestIndex;
	}
	for (y=0; y<height; y++)
	{
		pxl_idx[y]=new TUint8[width];
		for (x=0; x<width; x++)
		{
			for (i=0; i<palUsed; i++)
			{
				if (pal4k[i].color==pxl_12b[y][x])
				{
					if (i>=256)
					{
						pxl_idx[y][x]=mapIndex[i-256];
					}
					else
					{
						pxl_idx[y][x]=i;
					}
					break;
				}
			}
		}
	}
	delete[] pal4k;

	// STEP 4: RLE-compressing mask pixels
	int maskLen;
	iDataSize = 0;
	bool masking;

if (iAlphaBits == 4)
{
	for (y=0; y<height; y++)
	{
		masking=false;
		for (x=0; x<width; x++)
		{
			if (masking)
			{
				if (alpha[y][x]==0)
				{
					maskLen++;
				}
				else
				{	//end mask
					tmpData[iDataSize++]=maskLen&0xff;
					tmpData[iDataSize++]=(maskLen>>8)&0xff;
					tmpData[iDataSize++]=((pxl_12b[y][x]>>4)&0xf0) | alpha[y][x];	// RA
					tmpData[iDataSize++]=pxl_12b[y][x]&0xff;	// 再写入新的pixel, ARGB	// GB
					if (alpha[y][x] > 0xf)
					{
						xlog2("Alpha should be 4bits here! 0x%x\n", alpha[y][x]);
					}

					masking=false;
				}
			}
			else
			{
				if (alpha[y][x]==0)	//start mask
				{
					tmpData[iDataSize++]=0x00;
					masking=true;
					maskLen=1;
				}
				else
				{
					tmpData[iDataSize++]=((pxl_12b[y][x]>>4)&0xf0) | alpha[y][x];	// RA
					tmpData[iDataSize++]=pxl_12b[y][x] & 0xff;
					if (alpha[y][x] > 0xf)
					{
						xlog2("Alpha should be 4bits here! 0x%x\n", alpha[y][x]);
					}
				}
			}

			if (iDataSize >= DATA_SIZE_MAX)
			{
				xlog2("XMG too large!!! Exceeds MAX[%d bytes]!\n", DATA_SIZE_MAX);
				goto __BREAK;
			}
		}
		if (masking)
		{
			tmpData[iDataSize++]=maskLen&0xff;
			tmpData[iDataSize++]=(maskLen>>8)&0xff;
			masking=false;
		}
	}
}
else if (iAlphaBits==0 || iAlphaBits==8)
{
	for (y=0; y<height; y++)
	{
		masking=false;
		for (x=0; x<width; x++)
		{
			if (masking)
			{
				if (alpha[y][x]==0)
				{
					maskLen++;
				}
				else
				{	//end mask
					tmpData[iDataSize++]=maskLen&0xff;
					tmpData[iDataSize++]=(maskLen>>8)&0xff;
					tmpData[iDataSize++]=pxl_idx[y][x];	// 再写入新的pixel在palette中的index
					if (iAlphaBits==8)
						tmpData[iDataSize++]=alpha[y][x];

					masking=false;
				}
			}
			else
			{
				if (alpha[y][x]==0)	//start mask
				{
					tmpData[iDataSize++]=0x00;
					masking=true;
					maskLen=1;
				}
				else
				{
					tmpData[iDataSize++]=pxl_idx[y][x];
					if (iAlphaBits==8)
						tmpData[iDataSize++]=alpha[y][x];
				}
			}

			if (iDataSize >= DATA_SIZE_MAX)
			{
				xlog2("XMG too large!!! Exceeds MAX[%d bytes]!\n", DATA_SIZE_MAX);
				goto __BREAK;
			}
		}
		if (masking)
		{
			tmpData[iDataSize++]=maskLen&0xff;
			tmpData[iDataSize++]=(maskLen>>8)&0xff;
			masking=false;
		}
	}
}
else
{
	xlog1("ERROR! `rgbRLE' must be used with AlphaBits==4!\n");
}

__BREAK:

	iData = new TUint8[iDataSize];
	memcpy(iData, tmpData, iDataSize);

	// free temporary heap
	delete[] tmpData;
	for (y=0; y<height; y++)
	{
		delete[] pxl_12b[y];
		delete[] pxl_idx[y];
		if (alpha)
			delete[] alpha[y];
	}
	delete[] pxl_12b;
	delete[] pxl_idx;
	if (alpha)
		delete[] alpha;
}

/*
iAlphaBits(8)都存储32bits真彩色
直接存储Raw格式, 以方便使用
每个pixel以1个32b的4字节存储, ARGB, 高位为Alpha
*/
void PNG2XMG::convert32b()
{
	// header
	iVersion = 32;	// reserved
	iBPP = 32;		// 1 or 12
	iAlphaBits = 8;	// 0 or 4 or 8
	iPalSize = 0;
	iWidth = (short)width;
	iHeight = (short)height;
	iDataSize = width*height*4;
	iData = new TUint8[iDataSize];
	TUint32* ptr = (TUint32*)iData;

	for (unsigned int y=0; y<height; ++y)
	{
		for (unsigned int x=0; x<width; ++x,++ptr)
		{
			int offset = (y*width+x)*4;
			*ptr =  (png_pixels[offset+3] << 24) |
					(png_pixels[offset+0] << 16) |
					(png_pixels[offset+1] << 8) |
					(png_pixels[offset+2] << 0);
		}
	}
}

void PNG2XMG::write(const char* xmg_filename)
{
	FILE* fp = fopen(xmg_filename, "w+b");
	if (!fp)
	{
		xlog2("Write %s failed!\n", xmg_filename);
		return;
	}

	int total = 0;
	if (fwrite((XMG_HEADER*)this, sizeof(XMG_HEADER), 1, fp) != 1)
	{
		xlog1("Write Header failed!\n");
		fclose(fp);
		return;
	}
	total += sizeof(XMG_HEADER);
	if (fwrite(iPalette, sizeof(TUint16), iPalSize, fp) != (TUint)iPalSize)
	{
		xlog1("Write Palette failed!\n");
		fclose(fp);
		return;
	}
	total += sizeof(TUint16)*iPalSize;
	if (fwrite(iData, sizeof(TUint8), iDataSize, fp) != sizeof(TUint8)*iDataSize)
	{
		xlog1("Write Data failed!\n");
		fclose(fp);
		return;
	}
	total += iDataSize;
	if (iAlphaBits==4)
	{
		for (TUint y=0; y<height; y++)
		{
			if (fwrite(iAlpha[y], sizeof(TUint8), alpha_width, fp) != (TUint)alpha_width)
			{
				xlog2("Write Alpha[%d] failed!\n", y);
				fclose(fp);
				return;
			}
			total += alpha_width;
		}
	}
	fclose(fp);
	xlog4("XMG file created: [A%d]\n\t%s %d bytes.\n", iAlphaBits, xmg_filename, total);
}

void usage()
{
	xlog2("PNG2XMG - Conversion from PNG-file to XMG-file v%s\n", PNG2XMG_VERSION);
	xlog1("Author: ER1C<er1c.wang@hotmail.com>\n");
	xlog1("\n");
	xlog1("Usage:  png2xmg.exe [options] <input>.png [<output>.xmg]\n");
	xlog1("Options:\n");
	xlog1("        -aX       Alpha Bits, 0, 4 or 8\n");
	xlog1("        -m0xRGB   Mask color, eg. -m0xf0f\n");
	xlog1("Note:   [<output>.xmg] is optional, if not specified, <input>.xmg will be used\n\n");
}

char _lc(char c)
{
	if (c>='A' && c<='Z') return c+('a'-'A');
	else return c;
}

int _get_alpha_bits(const char* arg)
{
	int len = strlen(arg);
	switch (arg[len-1])
	{
	case '0':	return 0;
	case '4':	return 4;
	case '8':	return 8;
	default:
		xlog2("Unexpected AlphaBits: %c\n", arg[len-1]);
		return ALPHA_BITS_DEFAULT;
	}
}

int _hex2int(char c)
{
	if (c>='0' && c<='9') return c-'0';
	else if (_lc(c)>='a' && _lc(c)<='f') return c-'a'+0xa;
	else xlog2("Unexpected RGB hex char: %c\n", c);
	return 0;
}

TUint16 _get_mask_color(const char* arg)
{
	unsigned int LEN = strlen("-m0xRGB");
	if (strlen(arg) != LEN)
	{
		xlog1("Option [mask color] should in format '-m0xRGB'\n");
		return MASK_COLOR_DEFAULT;
	}
	return (_hex2int(arg[LEN-3]) << 8) | (_hex2int(arg[LEN-2]) << 4) | _hex2int(arg[LEN-1]);
}

bool PNG2XMG::read_ibm()
{
	const char* fileName = png_filename;
	FILE* fp = fopen(fileName, "r+b");
	if (fp == NULL) {
		printf("Open file failed! %s\n", fileName);
		return false;
	}

	char header[18];
	fread(&header, sizeof(char), 18, fp);
	if (header[0]!='B' || header[1]!='M') {
		printf("Corrupt! %s - Not `BM'\n", fileName);
		fclose(fp);
		return false;
	}
	if (header[14]!=0x28 || header[15]!=0x00) {
		printf("Corrupt! %s - Not 0x2800\n", fileName);
		fclose(fp);
		return false;
	}

	fread(&width, 4, 1, fp);
	fread(&height, 4, 1, fp);

	int bpp = 0;
	fread(&bpp, 4, 1, fp);
	bpp >>= 16;
	if (bpp!=8)
	{
		printf("Not BMP-8 file! %s[%d]\n",fileName,bpp);
		fclose(fp);
		return false;
	}
	fseek(fp, 20, SEEK_CUR);

	fread(&iPalSize, 4, 1, fp);
	TUint* palette = new TUint[iPalSize];
	fread(palette, iPalSize*4, 1, fp);

	iPalette = new TUint16[iPalSize];
	for (int i=0; i<iPalSize; i++)
	{
		iPalette[i] = RGB888_TO_444(palette[i]);
	}

	int BPR = width + (!(width%4) ? 0:(4-width%4));	//Bytes Per Row
	int size = BPR*height;
	iData = new TUint8[size];
	fread(iData, 1, size, fp);

	fclose(fp);
	return true;
}

// TODO: add dither/pattern/noise algorithms for converting from 24b to 12b
void PNG2XMG::convert_ibm()
{
	TUint8* output = new TUint8[DATA_SIZE_MAX];
	int BPR = width + (!(width%4) ? 0:(4-width%4));	//Bytes Per Row
	int maskLen = 0;
	bool masking;
	iDataSize = 0;
	iBPP = 12;
	iWidth = (short)width;
	iHeight = (short)height;

	for (int y=iHeight-1; y>=0; y--)
	{
		masking=false;
		for (int x=0; x<iWidth; x++)
		{
			TUint8 idx = iData[y*BPR+x];
			if (masking)
			{
				if (!idx)
				{
					maskLen++;
				}
				else
				{	//end mask
					output[iDataSize++]=maskLen&0xff;
					output[iDataSize++]=(maskLen>>8)&0xff;
					output[iDataSize++]=idx;	// 再写入新的pixel在palette中的index

					masking=false;
				}
			}
			else
			{
				if (!idx)	//start mask
				{
					output[iDataSize++]=0x00;
					masking=true;
					maskLen=1;
				}
				else
				{
					output[iDataSize++]=idx;
				}
			}

			if (iDataSize >= DATA_SIZE_MAX)
			{
				xlog2("XMG too large!!! Exceeds MAX[%d bytes]!\n", DATA_SIZE_MAX);
				return;
			}
		}
		if (masking)
		{
			output[iDataSize++]=maskLen&0xff;
			output[iDataSize++]=(maskLen>>8)&0xff;
			masking=false;
		}
	}

	delete[] iData;
	iData = new TUint8[iDataSize];
	memcpy(iData, output, iDataSize);
	delete[] output;
}

// ext should in uppercase, and without '.', eg. "XYZ"
bool ExtMatch(const char* filename, const char* ext)
{
	int len = strlen(filename);
	int lenExt = strlen(ext);
	if (len < 5 || lenExt<1 || ext[0]=='.') return false;
	for (int i=len-lenExt; i<len; i++,ext++)
	{
		if (filename[i]!=*ext && (filename[i]-('a'-'A'))!=*ext)
			return false;
	}
	return true;
}

//----------------------------------------------------------------------------
// main
//----------------------------------------------------------------------------
int main(int argc, char** argv)
{
	char* png_filename = NULL;
	char xmg_filename[256] = {0};
	int alpha_bits = ALPHA_BITS_DEFAULT;
	TUint16 mask_color = MASK_COLOR_DEFAULT;

	for (int argi = 1; argi < argc; argi++)
	{
		if (argv[argi][0] == '-')
		{
			switch (_lc( argv[argi][1]) )
			{
			case 'a':
				alpha_bits = _get_alpha_bits(argv[argi]);
				break;
			case 'm':
				mask_color = _get_mask_color(argv[argi]);
				break;
			default:
				xlog2("Unknown Option %s\n\n", argv[argi]);
				usage();
			//	exit(1);
				break;
			}
		}
		else if (png_filename == NULL)
		{
			png_filename = argv[argi];
		}
		else if (xmg_filename[0] == '\0')
		{
			strcpy(xmg_filename, argv[argi]);
		}
		else
		{
			xlog2("Unknown Parameter: %s\n\n", argv[argi]);
			usage();
		//	exit(1);
		}
	}

	if (png_filename == NULL)
	{
		usage();
		exit(1);
	}

	int len = strlen(png_filename);
	if (xmg_filename[0] == '\0')
	{
		strcpy(xmg_filename, png_filename);
		xmg_filename[len-3] = 'x';
		xmg_filename[len-2] = 'm';
		xmg_filename[len-1] = 'g';
	}

	PNG2XMG* png2xmg = NULL;

	if (ExtMatch(png_filename, "BMP"))
	{
		png2xmg = new PNG2XMG(png_filename, 0, -1);
		if (!png2xmg->read_ibm())
		{
			xlog1("Read BMP failed.\n");
			delete png2xmg;
			return -1;
		}
		png2xmg->convert_ibm();
	}
	else
	{
		png2xmg = new PNG2XMG(png_filename, alpha_bits, mask_color);
		if (!png2xmg->read())
		{
			xlog1("Read PNG failed.\n");
			delete png2xmg;
			return -1;
		}
		if (alpha_bits == 8)
		{
			png2xmg->convert32b();
		}
		else
		{
			png2xmg->convert();
		}
	}

	png2xmg->write(xmg_filename);
	delete png2xmg;

	return 0;
}

// end of file
