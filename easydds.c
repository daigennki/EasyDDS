﻿/*
    Copyright © 2019-2020 daigennki (@daigennki)

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <errno.h>
#include <stdint.h>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_DXT_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_resize.h"
#include "stb_dxt.h"

enum DDSFlags
{
	DDSD_CAPS = 0x00000001, 		// dwCaps/dwCaps2 is enabled.
	DDSD_HEIGHT = 0x00000002, 		// dwHeight is enabled.
	DDSD_WIDTH = 0x00000004, 		// dwWidth is enabled. Required for all textures.
	DDSD_PITCH = 0x00000008, 		// dwPitchOrLinearSize represents pitch.
	DDSD_PIXELFORMAT = 0x00001000, 	// dwPfSize/dwPfFlags/dwRGB/dwFourCC and such are enabled.
	DDSD_MIPMAPCOUNT = 0x00020000, 	// dwMipMapCount is enabled. Required for storing mipmaps.
	DDSD_LINEARSIZE = 0x00080000, 	// dwPitchOrLinearSize represents LinearSize.
	DDSD_DEPTH = 0x00800000 		// dwDepth is enabled. Used for 3D (Volume) Texture.
};

static void printHelp()
{
    printf(
        "EasyDDS - Convert various image formats to block-compressed DDS textures\n"
        "(c)2019-2020 daigennki\n"
        "Usage: <InputFile> [options]\n"
        "Supported input image formats: JPEG, PNG, TGA, BMP, PSD, GIF, HDR, PIC, PNM\n"
        "Options:\n"
        "\t-bc1: Output with BC1/DXT1 compression (RGB)\n"
        "\t-bc3: Output with BC3/DXT5 compression (RGBA)\n"
        "\t-bc4: Output with BC4/ATI1 compression (R)\n"
        "\t-bc5: Output with BC5/ATI2 compression (RG)\n"
        "\t-nomip: Don't generate mipmaps (by default, mipmaps are generated)"
        "If no -bc* option was specified, the output format will be chosen depending on the number of channels in the input.\n"
        "Note: Use -bc5 for normal maps. Results may appear incorrect if the other options are used.\n"
    );
}
static void writeHeader(FILE* outputFile, int w, int h, int channels, int mipCount)
{
    printf("Writing header...\n");
    /* write header */
    struct {
        char magic[4];	// always "DDS "
        uint32_t size;	// header size without magic number (==124)
        uint32_t flags;
        uint32_t height;
        uint32_t width;
        uint32_t pitchOrLinearSize;
        uint32_t depth;
        uint32_t mipMapCount;
        uint32_t reserved1[11];
        uint32_t pfSize;
        uint32_t pfFlags;
        char fourCC[4];
        uint32_t rgbBitCount;
        uint32_t rBitMask;
        uint32_t gBitMask;
        uint32_t bBitmask;
        uint32_t aBitmask;
        uint32_t caps;
        uint32_t caps2;
        uint32_t reservedCaps[2];
        uint32_t reserved2;
    } header;
    memset(&header, 0, sizeof(header));

    memcpy(header.magic, "DDS ", 4);
    header.size = 124;
    header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT;
    header.height = h;
    header.width = w;
    const int blockSize = (channels == 2 || channels == 4 ? 16 : 8);
    header.pitchOrLinearSize = (w / 4) * blockSize;
    header.mipMapCount = mipCount;
    memcpy(header.reserved1, "EasyDDS", 8);
    header.pfSize = 32;
    header.pfFlags = 0x4;    // DDPF_FOURCC

    /* Determine output FourCC from output channels */
    const char* fourccs[] = { "ATI1", "ATI2", "DXT1", "DXT5" };
    memcpy(header.fourCC, fourccs[channels - 1], 4);

    header.caps = 0x1000;   // DDSCAPS_TEXTURE
    if (mipCount > 1) header.caps |= 0x400000;
    
    fwrite(&header, sizeof(header), 1, outputFile);
}
static void writeData(FILE* outputFile, const int w, const int h, const int channels, const int mipCount, const stbi_uc* inRow)
{
    writeHeader(outputFile, w, h, channels, mipCount);

    unsigned char inDataBuf[64];
    memset(inDataBuf, 255, 64);
    unsigned char outDataBuf[16];
    printf("Writing data...\n");
    const int blockSize = (channels == 2 || channels == 4 ? 16 : 8);
    printf("Block size: %i\n", blockSize);
    const int pixBufStride = (channels == 3 ? 4 : channels);    // stb_compress_dxt_block expects RGBA even for no alpha
    int mw = w, mh = h;
    for (unsigned int m = 0; m < mipCount; ++m, mw /= 2, mh /= 2)
    {
        for (int l = 0; l < mh; l += 4, inRow += mw * 4 * 3)    /* every 4 rows */
        {
            for (int j = 0; j < mw; j += 4, inRow += 4 * 4)     /* every 4 columns */
            {
                for (int i = 0; i < 4; ++i)     /* every row in the 4x4 RGBA pixel block */
                {
                    for (int k = 0; k < 4; ++k)
                    {
                        memcpy(inDataBuf + (i * 4 + k) * pixBufStride, inRow + (i * mw + k) * 4, channels);
                    }
                }
                switch (channels)
                {
                case 3:
                case 4:
                    stb_compress_dxt_block(outDataBuf, inDataBuf, (channels == 4 ? 1 : 0), STB_DXT_DITHER | STB_DXT_HIGHQUAL);
                    break;
                case 1:
                    stb_compress_bc4_block(outDataBuf, inDataBuf);
                    break;
                case 2:
                    stb_compress_bc5_block(outDataBuf, inDataBuf);
                    break;
                }
                fwrite(outDataBuf, blockSize, 1, outputFile);
            }
        }
    }
}
static unsigned char* genMips(const int w, const int h, const int allowGenMips, const int srgb, const unsigned char* firstMip, int* const mipCountOut)
{
    assert(mipCountOut);
    
    int mipCount = 1;
    int mipSize = w * h * 4;
    int totalMipSize = mipSize;
    int mw = w / 2, mh = h / 2;
    unsigned char* mipData;
    if (allowGenMips)
    {
        /* Determine number of possible mipmaps */
        mipSize /= 4;
        for (; mw >= 4 && mh >= 4 && (mw % 4 == 0) && (mh % 4 == 0); mw /= 2, mh /= 2, mipSize /= 4) 
        {
            ++mipCount;
            totalMipSize += mipSize;
        }
        mipSize = w * h * 4;
    }

    /* Allocate memory for mipmaps */
    printf("Allocating output (%i bytes)...\n", totalMipSize);
    mipData = malloc(totalMipSize);

    /* Copy original data for first mipmap */
    memcpy(mipData, firstMip, mipSize);

    /* Generate mipmaps by resizing */
    printf("Generating %i mipmaps: 0 (%i)", mipCount, mipSize);
    unsigned char* currentMip = mipData + mipSize;
    unsigned char* mipEnd = mipData + totalMipSize;
    mipSize /= 4;
    mw = w / 2, mh = h / 2;
    for (int i = 1; i < mipCount; ++i)
    {
        assert((currentMip + mipSize) <= mipEnd);
        printf(", %i (%i)", i, mipSize);
        if (srgb) stbir_resize_uint8_srgb(firstMip, w, h, 0, currentMip, mw, mh, 0, 4, 3, 0);
        else stbir_resize_uint8(firstMip, w, h, 0, currentMip, mw, mh, 0, 4);
        currentMip += mipSize;
        mw /= 2, mh /= 2;
        mipSize /= 4;
    }
    printf("\n");

    *mipCountOut = mipCount;
    return mipData;
}

int main(int argc, char** argv)
{
    if (argc <= 1)
    {
        printHelp();
        return 0;
    }
    
    const char* inFilePath = NULL;
    int channels = 0;
    int allowGenMips = 1;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            if (!strcmp(argv[i], "-help"))
            {
                printHelp();
                return 0;
            }
            else if (!strcmp(argv[i], "-bc1")) channels = 3;
            else if (!strcmp(argv[i], "-bc3")) channels = 4;
            else if (!strcmp(argv[i], "-bc4")) channels = 1;
            else if (!strcmp(argv[i], "-bc5")) channels = 2;
            else if (!strcmp(argv[i], "-nomip")) allowGenMips = 0;
            else 
            {
                printf("Error: Unknown parameter '%s'\n", argv[i]);
                return 1;
            }
        }
        else if (!inFilePath) inFilePath = argv[i];
    }
    if (!inFilePath)
    {
        printf("Error: No input file given\n");
        return 1;
    }

    /* generate output file name */
    char outFilePath[1024];
    strncpy(outFilePath, inFilePath, 1023);
    char* periodPos = strrchr(outFilePath, '.');
    if (!periodPos)
    {
        printf("Error: Invalid file name (no period)\n");
        return 2;
    }
    if (periodPos - outFilePath > 1020)
    {
        printf("Error: File path too long\n");
        return 3;
    }
    strcpy(periodPos, ".dds");

    /* load file and its parameters; only set channels parameter if it was not specified in the options */
    printf("Loading file '%s'...\n", inFilePath);
    int w, h;
	stbi_uc *loadedFile = stbi_load(inFilePath, &w, &h, (channels == 0 ? &channels : NULL), 4);
    if (!loadedFile)
    {
        printf("Error: Failed to load input file '%s': %s\n", inFilePath, stbi_failure_reason());
        return 4;
    }
    assert(channels >= 1 && channels <= 4);
    if (w % 4 != 0 || h % 4 != 0)
    {
        printf("Error: Input file '%s' has dimensions that are not a multiple of 4\n", inFilePath);
        stbi_image_free(loadedFile);
        return 5;
    }

    /* Generate mipmaps if requested */
    int mipCount;
    unsigned char* mipData = genMips(w, h, allowGenMips, channels >= 3, loadedFile, &mipCount);

    /* Free original image here since we don't need it anymore */
    stbi_image_free(loadedFile);

    /* open output file */
    printf("Opening output file...\n");
    FILE* outputFile = fopen(outFilePath, "wb");
    if (!outputFile)
    {
        printf("Error: Failed to open output file: %s\n", strerror(errno));
        free(mipData);
        return 6;
    }

    /* write to output file */
    writeData(outputFile, w, h, channels, mipCount, mipData);

    fclose(outputFile);
    free(mipData);

    printf("Success.\n");

    return 0;
}