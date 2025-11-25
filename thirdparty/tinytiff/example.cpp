#include <tinytiffreader.h>
#include <iostream>
#include <filesystem>
int main(int argc, char* argv[])
{
    TinyTIFFReaderFile* tiffr = NULL;
    tiffr = TinyTIFFReader_open((std::filesystem::path{argv[0]}.parent_path() / "Export.tif").string().c_str());
    if (!tiffr)
    {
        std::cout << "    ERROR reading (not existent, not accessible or no TIFF file)\n";
    }
    else
    {
        if (TinyTIFFReader_wasError(tiffr))
        {
            std::cout << "   ERROR:" << TinyTIFFReader_getLastError(tiffr) << "\n";
        }
        else
        {
            std::cout << "    ImageDescription:\n"
                      << TinyTIFFReader_getImageDescription(tiffr) << "\n";
            uint32_t frames = TinyTIFFReader_countFrames(tiffr);
            std::cout << "    frames: " << frames << "\n";
            uint32_t frame = 0;
            if (TinyTIFFReader_wasError(tiffr))
            {
                std::cout << "   ERROR:" << TinyTIFFReader_getLastError(tiffr) << "\n";
            }
            else
            {
                do
                {
                    const uint32_t width = TinyTIFFReader_getWidth(tiffr);
                    const uint32_t height = TinyTIFFReader_getHeight(tiffr);
                    const uint16_t samples = TinyTIFFReader_getSamplesPerPixel(tiffr);
                    const uint16_t bitspersample = TinyTIFFReader_getBitsPerSample(tiffr, 0);
                    bool ok = true;
                    std::cout << "    size of frame " << frame << ": " << width << "x" << height << "\n";
                    std::cout << "    each pixel has " << samples << " samples with " << bitspersample << " bits each\n";
                    if (ok)
                    {
                        frame++;
                        // allocate memory for 1 sample from the image
                        uint8_t* image = (uint8_t*)calloc(width * height, bitspersample / 8);

                        for (uint16_t sample = 0; sample < samples; sample++)
                        {
                            // read the sample
                            TinyTIFFReader_getSampleData(tiffr, image, sample);
                            if (TinyTIFFReader_wasError(tiffr))
                            {
                                ok = false;
                                std::cout << "   ERROR:" << TinyTIFFReader_getLastError(tiffr) << "\n";
                                break;
                            }

                            ///////////////////////////////////////////////////////////////////
                            // HERE WE CAN DO SOMETHING WITH THE SAMPLE FROM THE IMAGE
                            // IN image (ROW-MAJOR!)
                            // Note: That you may have to typecast the array image to the
                            // datatype used in the TIFF-file. You can get the size of each
                            // sample in bits by calling TinyTIFFReader_getBitsPerSample() and
                            // the datatype by calling TinyTIFFReader_getSampleFormat().
                            ///////////////////////////////////////////////////////////////////
                        }

                        free(image);
                    }
                } while (TinyTIFFReader_readNext(tiffr)); // iterate over all frames
                std::cout << "    read " << frame << " frames\n";
            }
        }
    }
    TinyTIFFReader_close(tiffr);
}