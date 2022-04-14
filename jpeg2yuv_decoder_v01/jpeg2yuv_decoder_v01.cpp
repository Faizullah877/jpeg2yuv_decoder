// jpeg2yuv.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>

#include <jpeg/include/jpeglib.h>
#include <jpeg/include/jerror.h>
#include <setjmp.h>
#include <jpeg/jpeg.h>

using namespace std;
namespace fs = std::filesystem;

constexpr auto MAX_PATH = 512;
static const std::string opt_input = "-i";
static const std::string opt_output = "-o";
static const std::string opt_output_fmt = "-pack"; // 444I or 444P
static const std::string opt_pixels_format = "-pix_fmt";
typedef enum {
	MEDIA_RGB444I,
	MEDIA_RGB444P,
	MEDIA_YUV444I,
	MEDIA_YUV444P,      // Planar
} MEDIA_FORMAT;

struct my_error_mgr {
	struct jpeg_error_mgr pub;	/* "public" fields */

	jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr* my_error_ptr;

METHODDEF(void)
my_error_exit(j_common_ptr cinfo)
{
	my_error_ptr myerr = (my_error_ptr)cinfo->err;
	(*cinfo->err->output_message) (cinfo);
	longjmp(myerr->setjmp_buffer, 1);
}

class DataBuffer {
public:
	void* owner;
	unsigned char* data;
	unsigned int    size; // size in a Byte type

public:
	DataBuffer() : owner(0) { reset(); }
	virtual ~DataBuffer() {}

	void reset() {
		owner = 0;
		data = 0;
		size = 0;

	}
};

void read_jpeg_bytestream(const char* InputFile, DataBuffer* pBuffer) {
	std::ifstream jpg(InputFile, std::ifstream::binary);
	if (jpg) {
		// get length of file:
		jpg.seekg(0, jpg.end);
		unsigned long int length = jpg.tellg();
		jpg.seekg(0, jpg.beg);
		//cout << "File size is : " << length << endl;
		char* buffer = new char[length];
		jpg.read(buffer, length);
		if (jpg) {
			//std::cout << "all characters read successfully.";
			pBuffer->data = (unsigned char*)buffer;
			pBuffer->size = length;
		}
		else
			std::cout << "error: only " << jpg.gcount() << " could be read from input file";
		jpg.close();
	}
}

int	read_jpeg_memory(unsigned char* mem, unsigned long mem_size, bool ycbcr, JSAMPLE* image_buffer)
{

	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	/* More stuff */
	JSAMPARRAY buffer;		/* Output row buffer */
	int row_stride;		/* physical row width in output buffer */

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer)) {
		jpeg_destroy_decompress(&cinfo);

		return 0;
	}
	jpeg_create_decompress(&cinfo);
	jpeg_mem_src(&cinfo, mem, mem_size);
	(void)jpeg_read_header(&cinfo, TRUE);
	if (ycbcr == true) {
		cinfo.out_color_space = JCS_YCbCr;
	}

	(void)jpeg_start_decompress(&cinfo);
	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

	while (cinfo.output_scanline < cinfo.output_height) {
		(void)jpeg_read_scanlines(&cinfo, buffer, 1);
		JSAMPLE* ptr = buffer[0];
		for (int i = 0; i < row_stride; i += 3) {
			*image_buffer++ = *ptr++;
			*image_buffer++ = *ptr++;
			*image_buffer++ = *ptr++;
		}
	}
	(void)jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return 1;
}

void convert_yuv444I_to_yuv444p(unsigned char* rawFrameData, unsigned int rawFrameSize, unsigned int  width, unsigned int height) {
	unsigned char* odata = new unsigned char[rawFrameSize];
	for (int i = 0; i < rawFrameSize; ++i)
	{
		if (i % 3 == 0) odata[i / 3] = rawFrameData[i];
		if (i % 3 == 1) odata[(i / 3) + (width * height)] = rawFrameData[i];
		if (i % 3 == 2) odata[(i / 3) + (width * height * 2)] = rawFrameData[i];
	}
	memcpy(rawFrameData, odata, rawFrameSize);
	delete[] odata;
}
int main(int argc, char* argv[])
{
	unsigned int width = 0;
	unsigned int height = 0;
	char input_filename[MAX_PATH] = {};
	char ref_filename[MAX_PATH] = {};
	char output_filename[MAX_PATH] = {};
	MEDIA_FORMAT out_format = MEDIA_YUV444I;
	bool ycbcr = TRUE;


	if (argc < 3) {
		cout << "Please Specify arguments" << endl;
		return -1;
	}
	for (int i = 1; i < argc; i++) {
		std::string arg = argv[i++];
		if (arg == opt_input) {
			snprintf(input_filename, MAX_PATH, argv[i]);
		}
		else if (arg == opt_output) {
			snprintf(output_filename, MAX_PATH, argv[i]);
		}
		else if (arg == opt_output_fmt) {
			std::string opt = argv[i];
			if (opt == "444P" || opt == "444p")
				out_format = MEDIA_YUV444P;
			else if (opt == "444I" || opt == "444i")
				out_format = MEDIA_YUV444I;
			else
			{
				cout << "Output format not recognized" << endl;
				//print_usage();
				return -1;
			}
		}
		else if (arg == opt_pixels_format)
		{
			std::string opt = argv[i];
			if (opt == "RGB" || opt == "rgb")
				ycbcr = false;
			else if (opt == "YUV" || opt == "yuv")
				ycbcr = true;
			else {
				cout << "Only RGB and YUV onput Pixels Color space are supported\n" << endl;
				//print_usage();
				return -1;
			}
		}
		else {
			printf("Invalid argument: %s\n", argv[i - 1]);
			//print_usage();
			return 0;
		}
	}
	vector<string> images_names;

	//string input_folder = "C:\\Users\\FAIZULLAH\\source\\repos\\gmis_jpeg_v01\\jpeg2yuv\\baseball_throw";

	if (!fs::exists(input_filename))
	{
		cout << "path does not exits" << endl;
		return 0;
	}

	for (const auto& entry : fs::directory_iterator(input_filename))
	{
		const string s = entry.path().string();
		std::size_t a = s.rfind(".jpg");
		if (a != std::string::npos)
		{
			images_names.push_back(s);
		}
		else {
			cout << "Input file : " << s << " is not JPEG." << endl;
			continue;
		}
	}

	unsigned int infiles_total_size = 0;
	int no_of_images = images_names.size();
	//cout << "Total Number of Input images : " << no_of_images << endl;

	unsigned int* sizes_array = new unsigned int[no_of_images];

	//char out_file_name[256] = "test_jpg2yuv.yuv";
	FILE* outFile;
	errno_t err1 = fopen_s(&outFile, output_filename, "wb");
	if (err1 == 0)
	{
		//printf("\Input File  : %s ", InputFile);
		//cout << endl;
	}
	else
	{
		//fprintf(resulttxt, "\nError -> Failed to open output file. Name            : %s\n", out_file_name);
		printf("The %s for Output was not opened ", output_filename);
		cout << endl;
		return 0;
	}
	for (int i = 0; i < no_of_images; ++i) {
		unsigned int raw_size = 0;

		DataBuffer* ptr = new DataBuffer();
		read_jpeg_bytestream(images_names[i].c_str(), ptr);
		infiles_total_size += ptr->size;
		//cout<<"Image No : "<<i<<"  => "<<images_names[i].c_str()<< "  => size = "<< ptr->size <<endl;


		struct jpeg_decompress_struct cinfo;
		struct my_error_mgr jerr;
		/* More stuff */
		JSAMPARRAY buffer;		/* Output row buffer */
		int row_stride;		/* physical row width in output buffer */

		cinfo.err = jpeg_std_error(&jerr.pub);
		jerr.pub.error_exit = my_error_exit;
		if (setjmp(jerr.setjmp_buffer)) {
			jpeg_destroy_decompress(&cinfo);

			return 0;
		}
		jpeg_create_decompress(&cinfo);
		jpeg_mem_src(&cinfo, ptr->data, ptr->size);
		(void)jpeg_read_header(&cinfo, TRUE);

		width = cinfo.image_width;
		height = cinfo.image_height;
		raw_size = width * height * cinfo.num_components;

		unsigned char* image_buffer = new unsigned char[raw_size];


		read_jpeg_memory(ptr->data, ptr->size, ycbcr, image_buffer);

		if (out_format == MEDIA_YUV444P)
		{
			convert_yuv444I_to_yuv444p(image_buffer, raw_size, width, height);
		}

		fwrite(image_buffer, sizeof(unsigned char), raw_size, outFile);

	}

	fclose(outFile);

	cout << no_of_images << "  JPEG images converted to YUV " << endl;
	cout << "Width of output video is : " << width << endl;
	cout << "Height of output video is : " << height << endl;
	//cout << "Combined Size of all input images is : " << infiles_total_size << endl;


}

