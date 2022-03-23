#include "tools.h"
#include <string>
#include <stdlib.h>
#include <direct.h>
#include <shlwapi.h>
#include <minwinbase.h>
#include <fileapi.h>
#include "zstd.h"
#include "zlib.h"
#include <io.h>
#include <direct.h>
#include <string>

#pragma comment(lib,"libzstd.lib")
#pragma comment(lib,"zlib.lib")
#pragma comment(lib,"Shlwapi.lib")

static char file_base[260];
static char compress_base[260];
void Unpack(FILE* f);
void Pack(WIN32_FIND_DATAA& result);

struct PACHead
{
	byte magic[3];
	byte unk1;
	int file_count;
	int pack_version;
};
struct PACEntry 
{
	char entry_name[64];
	int offset;
	int unpacked_size;
	int packed_size;
};
struct PACIndex
{
	byte* packed_index;
	int packed_index_size;
	byte* unpacked_index;
	int unpacked_index_size;
};
struct PACpackinfo {
	PACEntry ent;
	PACpackinfo*next;
};


int main(int argc, char** argv) {
	FILE* f = NULL;
	FILE* fpa = NULL;
	errno_t err;

	if (argc > 1) {
		WIN32_FIND_DATAA result = {0};
		HANDLE hFind = FindFirstFileA(argv[1], &result);
		if (hFind != INVALID_HANDLE_VALUE) {
			if (result.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
				FindClose(hFind);
				strcpy_s(file_base, argv[1]);
				strcat_s(file_base, "\\*.*");
				strcpy_s(compress_base, argv[1]);
				Pack(result);
			}
			else{
				printf_s("Input is not a directory. Performaning Decompress.\n");
				FindClose(hFind);
				err = fopen_s(&f, argv[1], "rb");
				if (!err && f) {
					strcpy_s(file_base, argv[1]);
					PathRemoveExtensionA(file_base);
					_mkdir(file_base);
					PathAddBackslashA(file_base);
					Unpack(f);
				}

			}
		}
		return 0;
	}
	else {
		printf_s("Usage: NexasPack.exe [File.pac / Folder]\n");
		return 0;
	}
}

void Unpack(FILE* f) {
	FILE* fo = NULL;
	errno_t err2;

	PACHead hd;
	IntToBytes(ReadInt(f), hd.magic);
	//err2 = fopen_s(&fo, "E:\\GalGames_Work\\OnWork\\GALKOI\\Ana2.bin", "wb");
	if (hd.magic[0] == 'P' && hd.magic[1] == 'A' && hd.magic[2] == 'C') {
		//获得封包基本信息
		hd.file_count = ReadInt(f);
		hd.pack_version = ReadInt(f);

		PACIndex ind;
		int fs = GetFileSize(f);
		ind.packed_index_size = ReadInt(f, fs - 4);
		ind.unpacked_index_size = hd.file_count * 0x4C;
		ind.packed_index = new BYTE[ind.packed_index_size];
		ind.unpacked_index = new BYTE[ind.unpacked_index_size];
		//读取压缩的index
		ReadBytes_s(f, ind.packed_index, ind.packed_index_size, fs - ind.packed_index_size - 4, ind.packed_index_size);
		for (int i = 0; i < ind.packed_index_size; i++)
			ind.packed_index[i] = ~ind.packed_index[i];
		//解压index
		HuffmanDecode hf(ind.packed_index, ind.unpacked_index_size, ind.unpacked_index);
		hf.Unpack();
		delete[] ind.packed_index;
		//fwrite(ind.unpacked_index, ind.unpacked_index_size, 1, fo);
		//fclose(fo);

		//解压文件
		PACEntry* p;
		p = (PACEntry*)ind.unpacked_index;
		for (int i = 0; i < hd.file_count; i++) {

			std::string str = file_base;
			str += p->entry_name;
			printf_s("Processing %s\n", str.c_str());
			err2 = fopen_s(&fo, str.c_str(), "wb");
			if (!err2) {
				//压缩大小与解压大小一致，直接复制数据
				if (p->packed_size == p->unpacked_size) {
					byte* tmp = new byte[p->unpacked_size];
					ReadBytes_s(f, tmp, p->unpacked_size, p->offset, p->unpacked_size);
					fwrite(tmp, p->unpacked_size, 1, fo);
					delete[] tmp;
					fclose(fo);
					p++;
					continue;
				}

				byte* tmp = new byte[p->packed_size];
				byte* unctmp = new byte[p->unpacked_size];
				memset(unctmp, 0, p->unpacked_size);
				ReadBytes_s(f, tmp, p->packed_size, p->offset, p->packed_size);

				//解压缩
				switch (hd.pack_version) {
				case 4:
					uncompress(unctmp, (uLongf*)&p->unpacked_size, tmp, p->packed_size);
					break;
				case 7:
					ZSTD_decompress(unctmp, p->unpacked_size, tmp, p->packed_size);
					break;
				default:
					printf_s("%s: Unrecognized/Unimplemented compressing format.\n", p->entry_name);
					delete[] tmp, unctmp;
					fclose(fo);
					p++;
					continue;
				}
				fwrite(unctmp, p->unpacked_size, 1, fo);
				delete[] tmp, unctmp;
				fclose(fo);
			}
			p++;
		}
		delete[] ind.unpacked_index;
	}
	fclose(f);
}

void Pack(WIN32_FIND_DATAA& result) {
	FILE* f = NULL;
	FILE* fpa = NULL;
	errno_t err, err2;
	BOOL bNext = true;
	int mode = 0, ins_mode = 0, count = 0, cur_entry = sizeof(PACHead);

	printf_s("Determing the compress mode you prefered.( Only support 0:no Compress and 4:Zlib and 7:Zstd )\n");
	scanf_s("%d", &mode);

	std::string out = compress_base;
	out += ".pacNew";
	strcat_s(compress_base, "\\");
	HANDLE hFind2 = FindFirstFileA(file_base, &result);

	//创建封包基本信息
	PACHead pachead = { 0 };
	pachead.magic[0] = 'P';
	pachead.magic[1] = 'A';
	pachead.magic[2] = 'C';
	pachead.pack_version = mode;

	//以wb模式打开文件：确保文件存在，并清空现有文件
	err2 = fopen_s(&fpa, out.c_str(), "wb");
	fclose(fpa);
	err2 = fopen_s(&fpa, out.c_str(), "rb+");
	if (err2) exit(0);
	fseek(fpa, 0, SEEK_SET);
	//首次写入的不是完整的文件头，将会在压缩全部完成之后再次写入一遍
	fwrite((void*)&pachead, sizeof(PACHead), 1, fpa);

	PACpackinfo* head, * headorg;
	head = new PACpackinfo;
	headorg = head;
	PACpackinfo* tmp = NULL;
	if (hFind2 != INVALID_HANDLE_VALUE) {
		while (bNext) {
			if (strcmp(result.cFileName, "..") && strcmp(result.cFileName, ".")) {
				//如果文件本身就是压缩格式，就设置压缩模式为不压缩
				ins_mode = (strstr(result.cFileName, ".png") || strstr(result.cFileName, ".fnt") || strstr(result.cFileName, ".ogg") || strstr(result.cFileName, ".wav")) ? 0 : mode;
				memset(&head->ent, 0, sizeof(PACEntry));
				printf_s("Processing: %s : Size:%d\n", result.cFileName, result.nFileSizeLow);
				//拼接文件所在路径
				std::string str = compress_base;
				str += result.cFileName;
				//打开&读入文件内容
				err = fopen_s(&f, str.c_str(), "rb");
				if (!err && f) {
					byte* fb = new byte[result.nFileSizeLow];
					fread(fb, result.nFileSizeLow, 1, f);
					fclose(f);
					//填充index信息：文件名,原始大小,offset
					strcpy_s(head->ent.entry_name, result.cFileName);
					head->ent.unpacked_size = result.nFileSizeLow;
					head->ent.offset = cur_entry;

					//选择压缩方式压缩、写入压缩数据，填充index信息：压缩大小
					switch (ins_mode) {
					default:
					case 0:
						fwrite(fb, result.nFileSizeLow, 1, fpa);
						head->ent.packed_size = result.nFileSizeLow;
						cur_entry += result.nFileSizeLow;
						delete[] fb;
						break;
					case 4: {
						uLongf compressed_size = compressBound(result.nFileSizeLow);
						byte* comp = new byte[compressed_size];
						compress(comp, &compressed_size, fb, result.nFileSizeLow);
						fwrite(comp, compressed_size, 1, fpa);
						head->ent.packed_size = compressed_size;
						cur_entry += compressed_size;
						delete[] comp;
						break;
					}
					case 7: {
						size_t Zcompressed_size = ZSTD_compressBound(result.nFileSizeLow);
						byte* Zcomp = new byte[Zcompressed_size];
						size_t actual_size = ZSTD_compress(Zcomp, Zcompressed_size, fb, result.nFileSizeLow, ZSTD_defaultCLevel());
						fwrite(Zcomp, actual_size, 1, fpa);
						head->ent.packed_size = actual_size;
						cur_entry += actual_size;
						delete[] Zcomp;
						break;
					}
					}
				}
				head->next = new PACpackinfo;
				head = head->next;
				count++;
			}
			bNext = FindNextFileA(hFind2, &result);
		}
		pachead.file_count = count;
		head->next = NULL;

		//由于不清楚文件个数，上方采用链表存储信息。这里将链表信息统合到一起。
		byte* unc_index = new byte[count * 0x4C];
		int unc_size = count * 0x4C;
		memset(unc_index, 0, unc_size);
		int i = 0, p = count;
		while (count--) {
			head = headorg;
			memcpy((unc_index + i), &headorg->ent, sizeof(PACEntry));
			i += 0x4C;
			headorg = headorg->next;
		}

		//对index进行压缩
		HuffmanEncode he(unc_index, unc_size);
		//写入Huffman树
		int packed_index_size = he.WriteBlock(he.serialized_huffman_tree, fpa);
		//写入Huffman编码数据
		packed_index_size += he.WriteData(fpa);
		fwrite(&packed_index_size, 4, 1, fpa);
		//压缩已经全部完成，再次写入头部数据
		fseek(fpa, 0, SEEK_SET);
		fwrite((void*)&pachead, sizeof(PACHead), 1, fpa);
		fclose(fpa);
		FindClose(hFind2);
	}
}