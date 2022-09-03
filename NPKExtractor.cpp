#include <filesystem>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "zlib/zlib.h"
#pragma comment(lib, "zlibstatic.lib")
#include "libpng/png.h"
#pragma comment(lib, "libpng_64.lib")

typedef struct FileInfo{
	uint32_t offset;
	uint32_t size;
	char name[0x100];
}FileInfo;
#define SIZEOF_FILENAME (sizeof (((FileInfo *)0)->name))

static const int8_t npkMagicStr[0x10] = "NeoplePack_Bill";
static const char fileNameKey[SIZEOF_FILENAME] =
	"puchikon@neople dungeon and fighter "
	"DNFDNFDNFDNFDNFDNFDNFDNFDNFDNFDNF"
	"DNFDNFDNFDNFDNFDNFDNFDNFDNFDNFDNF"
	"DNFDNFDNFDNFDNFDNFDNFDNFDNFDNFDNF"
	"DNFDNFDNFDNFDNFDNFDNFDNFDNFDNFDNF"
	"DNFDNFDNFDNFDNFDNFDNFDNFDNFDNFDNF"
	"DNFDNFDNFDNFDNFDNFDNFDNFDNFDNFDNF"
	"DNFDNFDNFDNFDNFDNFDNF"
;

bool extract_image(FILE* FP,int offset, const char* name);

int main(const int argc, const char * const * const argv){
	if (argc < 2){
		printf("Usage : %s <npkFilename>\n", argv[0]);
		return -1;
	}

	FILE * pNpk = fopen(argv[1], "rb");
	if (NULL == pNpk){
		fprintf(stderr, "ERROR : Failed to open npk file \"%s\".\n", argv[1]);
		return -2;
	}

	{
		int8_t buf[sizeof npkMagicStr];
		if ( fread(buf, sizeof (int8_t), sizeof npkMagicStr, pNpk) != sizeof npkMagicStr ){
			fprintf(stderr, "ERROR : Failed to load npk magic string.\n");
			return -3;
		}

		if ( memcmp(buf, npkMagicStr, sizeof npkMagicStr) != 0 ){
			fprintf(stderr, "ERROR : Failed to check npk magic string.\n");
			return -4;
		}
	}

	uint32_t fileAmount;
	if ( fread( (void *)&fileAmount, sizeof (uint32_t), 1, pNpk ) != 1 ){
		fprintf(stderr, "ERROR : Failed to load file amount.\n");
		return -5;
	}

	if (!( fileAmount > 0 )){
		fprintf(stderr, "ERROR : The amount of file(s) in npk is 0.\n");
		return -6;
	}

	FileInfo * pFileInfo = (FileInfo *)malloc(sizeof (FileInfo) * fileAmount);
	if (NULL == pFileInfo){
		fprintf(stderr, "ERROR : Failed to allocate memory for FileInfo array.\n");
		return -7;
	}

	if ( fread(pFileInfo, sizeof (FileInfo), fileAmount, pNpk) != fileAmount ){
		fprintf(stderr, "ERROR : Failed to load FileInfo.\n");
		return -8;
	}

	uint32_t maxFileSize = 0;
	for (uint32_t i = 0; i < fileAmount; i += 1){
		if ( pFileInfo[i].size > maxFileSize ){
			maxFileSize = pFileInfo[i].size;
		}

		if ( pFileInfo[i].name[SIZEOF_FILENAME - 1] != '\0' ){
			fprintf(stderr, "ERROR : The length of file name is greater than max.\n");
			return -8;
		}
		for (uint8_t j = 0; j < (SIZEOF_FILENAME - 1); j += 1){
			pFileInfo[i].name[j] ^= fileNameKey[j];
		}
		if ( '\0' == pFileInfo[i].name[0] ){
			fprintf(stderr, "ERROR : The file name in npk is empty.\n");
			return -8;
		}
	}

	if (!( maxFileSize > 0 )){
		fprintf(stderr, "ERROR : All the files in the npk are empty.\n");
		return -8;
	}

	uint8_t * pFileBuf = (uint8_t *)malloc(maxFileSize);
	if (NULL == pFileBuf){
		fprintf(stderr, "ERROR : Failed to allocate memory for file buffer.\n");
		return -8;
	}

	for (uint32_t i = 0; i < fileAmount; i += 1){
		fseek(pNpk, pFileInfo[i].offset, SEEK_SET);

		if (extract_image(pNpk, pFileInfo[i].offset, pFileInfo[i].name))
			continue;

		if ( fread(pFileBuf, sizeof (uint8_t), pFileInfo[i].size, pNpk) != pFileInfo[i].size ){
			fprintf(stderr, "ERROR : Failed to load file from npk.\n");
			return -8;
		}

		FILE * pWriteFile = fopen(pFileInfo[i].name, "wb");
		if (NULL == pWriteFile){
			fprintf(stderr, "ERROR : Failed to open file to be written.\n");
			return -8;
		}

		if ( fwrite(pFileBuf, sizeof (uint8_t), pFileInfo[i].size, pWriteFile) != pFileInfo[i].size ){
			fprintf(stderr, "ERROR : Failed to write file.\n");

			fclose(pWriteFile);
			pWriteFile = NULL;

			return -8;
		}

		fclose(pWriteFile);
		pWriteFile = NULL;
	}

	free(pFileBuf);
	pFileBuf = NULL;

	free(pFileInfo);
	pFileInfo = NULL;

	fclose(pNpk);
	pNpk = NULL;

	puts("Success!");

	return 0;
}

struct NImgF_Header
{
	char flag[16]; // 文件标石"Neople Img File"
	int index_size;	// 索引表大小，以字节为单位
	int unknown1;
	int version;
	int index_count;// 索引表数目
};

struct NImgF_Index
{
	unsigned int dwType; //目前已知的类型有 0x0E(1555格式) 0x0F(4444格式) 0x10(8888格式) 0x11(不包含任何数据，可能是指内容同上一帧)
	unsigned int dwCompress; // 目前已知的类型有 0x06(zlib压缩) 0x05(未压缩)
	int width;        // 宽度
	int height;       // 高度
	int size;         // 压缩时size为压缩后大小，未压缩时size
	int key_x;        // X关键点，当前图片在整图中的X坐标
	int key_y;        // Y关键点，当前图片在整图中的Y坐标
	int max_width;    // 整图的宽度
	int max_height;   // 整图的高度，有此数据是为了对齐精灵
};

#define ARGB_1555 0x0e
#define ARGB_4444 0x0f
#define ARGB_8888 0x10
#define ARGB_INDEX 0x11

#define COMPRESS_ZLIB 0x06
#define COMPRESS_NONE 0x05

void convert_to_png(const std::filesystem::path& file_name, int width, int height, int type, png_byte* data, int size);


bool extract_image(FILE* fp, int offset, const char* name)
{
	NImgF_Header header;
	fread(header.flag, 16, 1, fp);
	fread(&header.index_size, 4, 1, fp);
	fread(&header.unknown1, 4, 1, fp);
	fread(&header.version, 4, 1, fp);
	fread(&header.index_count, 4, 1, fp);

	printf("%s\n", name);

	printf("\t image version:%d\n", header.version);

	if (stricmp(header.flag, "Neople Img File") != 0) {
		return false; 
	}

	printf("\t index count:%d\n", header.index_count);

	std::vector<NImgF_Index> all_file_index;
	for (int i = 0; i < header.index_count; ++i) {
		NImgF_Index index;
		fread(&index.dwType, 4, 1, fp);

		// 占位文件只包含这两个数据
		if (index.dwType == ARGB_INDEX) {
			fread(&index.width, 4, 1, fp);
			all_file_index.push_back(index);
			continue;
		}

		fread(&index.dwCompress, 4, 1, fp);
		fread(&index.width, 4, 1, fp);
		fread(&index.height, 4, 1, fp);
		fread(&index.size, 4, 1, fp);
		fread(&index.key_x, 4, 1, fp);
		fread(&index.key_y, 4, 1, fp);
		fread(&index.max_width, 4, 1, fp);
		fread(&index.max_height, 4, 1, fp);

		printf("\t\t %d type:%d compress:%u w:%d h:%d size:%d \n", i, index.dwType, index.dwCompress,index.width,index.height,index.size);

		all_file_index.push_back(index);
	}

	// 跳过文件头和索引表
	fseek(fp, offset + header.index_size + 32, SEEK_SET);

	std::filesystem::path file_dir(name);
	file_dir.replace_extension();

	std::printf(file_dir.string().c_str());
	std::filesystem::create_directories(file_dir);

	int i = 0;
	for (auto index : all_file_index)
	{
		auto file_path  = file_dir / (std::to_string(i++) + ".png");

		if (index.dwType == ARGB_INDEX)
			continue;

		int size = index.size;

		if (index.dwCompress == COMPRESS_NONE) {
			if (index.dwType == ARGB_8888) {
				size = index.size;
			}
			else if (index.dwType == ARGB_1555 || index.dwType == ARGB_4444) {
				//size = index.size / 2;
			}
		}

		switch (index.dwType)
		{
		case ARGB_1555:
		case ARGB_4444:
		case ARGB_8888:
			break;
		default:
			printf("%s %d unknown type:%d \n",name,i-1, index.dwType);
			continue;
			break;
		}

		auto temp_file_data = std::make_unique<std::byte[]>(size);
		fread(temp_file_data.get(), size, 1, fp);

		if (index.dwCompress == COMPRESS_ZLIB)
		{
			unsigned long zlib_len = index.width * index.height * 16;
			auto temp_zlib_dat = std::make_unique<std::byte[]>(index.width * index.height * 16);

			int ret = uncompress((unsigned char*)temp_zlib_dat.get(), &zlib_len, (unsigned char*)temp_file_data.get(), size);

			temp_file_data = std::make_unique < std::byte[]>(zlib_len);

			memcpy(temp_file_data.get(), temp_zlib_dat.get(), zlib_len);
			size = zlib_len;
		}

		convert_to_png(file_path, index.width, index.height, index.dwType, (png_byte*)temp_file_data.get(), size);
	}

	//读取索引表输出帧动画
	auto anim_path = file_dir / std::filesystem::path(name).filename().replace_extension(".dnfanim.txt");
	FILE* anim_file = fopen(anim_path.string().c_str(), "wb");

	i = 0;
	for (auto index : all_file_index)
	{
		if (index.dwType == ARGB_INDEX)
		{
			while (index.dwType == ARGB_INDEX)
			{
				if (index.width >= all_file_index.size() || index.width < 0)
					break;

				if (all_file_index[index.width].dwType != ARGB_INDEX)
					break;

				index = all_file_index[index.width];
			}

			fprintf(anim_file, "%d\n", index.width);
		}
		else
		{
			fprintf(anim_file, "%d\n", i);
		}
		++i;
	}
	fclose(anim_file);

	return true;
}

void convert_to_png(const std::filesystem::path& file_name, int width, int height, int type, png_byte* data, int size)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep* row_pointers;

	/* create file */
	FILE* fp = fopen(file_name.string().c_str(), "wb");
	if (!fp) {
		printf("[write_png_file] File %s could not be opened for writing", file_name.string().c_str());
		return;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr) {
		printf("[write_png_file] png_create_write_struct failed");
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		printf("[write_png_file] png_create_info_struct failed");
		return;
	}
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		printf("[write_png_file] Error during init_io");
		return;
	}
	png_init_io(png_ptr, fp);

	/* write header */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		printf("[write_png_file] Error during writing header");
		return;
	}

	png_set_IHDR(png_ptr, info_ptr, width, height,
		8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	/* write bytes */
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		printf("[write_png_file] Error during writing bytes");
		return;
	}

	row_pointers = (png_bytep*)malloc(height * sizeof(png_bytep));
	for (int i = 0; i < height; i++)
	{
		row_pointers[i] = (png_bytep)malloc(sizeof(unsigned char) * 4 * width);
		for (int j = 0; j < width; ++j)
		{
			// png is rgba
			switch (type)
			{
			case ARGB_1555://1555
				row_pointers[i][j * 4 + 0] = ((data[i * width * 2 + j * 2 + 1] & 127) >> 2) << 3;   // red  
				row_pointers[i][j * 4 + 1] = (((data[i * width * 2 + j * 2 + 1] & 0x0003) << 3) | ((data[i * width * 2 + j * 2] >> 5) & 0x0007)) << 3; // green  
				row_pointers[i][j * 4 + 2] = (data[i * width * 2 + j * 2] & 0x003f) << 3; // blue 
				row_pointers[i][j * 4 + 3] = (data[i * width * 2 + j * 2 + 1] >> 7) == 0 ? 0 : 255; // alpha
				break;
			case ARGB_4444://4444
				row_pointers[i][j * 4 + 0] = (data[i * width * 2 + j * 2 + 1] & 0x0f) << 4;   // red  
				row_pointers[i][j * 4 + 1] = ((data[i * width * 2 + j * 2 + 0] & 0xf0) >> 4) << 4; // green  
				row_pointers[i][j * 4 + 2] = (data[i * width * 2 + j * 2 + 0] & 0x0f) << 4;; // blue  
				row_pointers[i][j * 4 + 3] = ((data[i * width * 2 + j * 2 + 1] & 0xf0) >> 4) << 4; // alpha
				break;
			case ARGB_8888://8888
				row_pointers[i][j * 4 + 0] = data[i * width * 4 + j * 4 + 2]; // red
				row_pointers[i][j * 4 + 1] = data[i * width * 4 + j * 4 + 1]; // green
				row_pointers[i][j * 4 + 2] = data[i * width * 4 + j * 4 + 0]; // blue
				row_pointers[i][j * 4 + 3] = data[i * width * 4 + j * 4 + 3]; // alpha
				break;
			}
		}
	}
	png_write_image(png_ptr, row_pointers);

	/* end write */
	if (setjmp(png_jmpbuf(png_ptr))) {
		printf("[write_png_file] Error during end of write");
		return;
	}
	png_write_end(png_ptr, NULL);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	/* cleanup heap allocation */
	for (int j = 0; j < height; j++)
		free(row_pointers[j]);
	free(row_pointers);

	fclose(fp);
}
