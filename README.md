# EasyDDS
## a simple utility for generating DDS files from various image formats
©2019-2020 daigennki<br>
licensed under the MIT license<br>

## Usage
```
./easydds <InputFile> [options]
```
### Options
<ul>
<li>-bc1: Output with BC1/DXT1 compression (RGB)</li>
<li>-bc3: Output with BC3/DXT5 compression (RGBA)</li>
<li>-bc4: Output with BC4/ATI1 compression (R)</li>
<li>-bc5: Output with BC5/ATI2 compression (RG)</li>
<li>-nomip: Don't generate mipmaps (by default, mipmaps are generated)</li>
</ul>
If no options were specified, the application will automatically determine the output format depending on the number of channels in the input image. For example, JPEG (which usually is RGB and has no alpha) will be converted to BC1, PNG images with an alpha channel will be converted to BC3, and greyscale PNG images will be converted to BC4. The output file will be placed in the same location as the input file, with the extension replaced with ".dds".<br>
Additionally, on Windows, you can simply drag and drop the input image onto the executable if you want automatic conversion.<br>
<br>
Do note that only BC1 and BC3 are in the sRGB color space. BC4 and BC5 support exists with the assumption that they will not be used for color, such as metallic maps and normal maps. In fact, only color maps can use BC1 and BC3, otherwise the output may appear incorrect. This means you should force BC5 for normal maps.<br>