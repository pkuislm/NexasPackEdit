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
		//��÷��������Ϣ
		hd.file_count = ReadInt(f);
		hd.pack_version = ReadInt(f);

		PACIndex ind;
		int fs = GetFileSize(f);
		ind.packed_index_size = ReadInt(f, fs - 4);
		ind.unpacked_index_size = hd.file_count * 0x4C;
		ind.packed_index = new BYTE[ind.packed_index_size];
		ind.unpacked_index = new BYTE[ind.unpacked_index_size];
		//��ȡѹ����index
		ReadBytes_s(f, ind.packed_index, ind.packed_index_size, fs - ind.packed_index_size - 4, ind.packed_index_size);
		for (int i = 0; i < ind.packed_index_size; i++)
			ind.packed_index[i] = ~ind.packed_index[i];
		//��ѹindex
		HuffmanDecode hf(ind.packed_index, ind.unpacked_index_size, ind.unpacked_index);
		hf.Unpack();
		delete[] ind.packed_index;
		//fwrite(ind.unpacked_index, ind.unpacked_index_size, 1, fo);
		//fclose(fo);

		//��ѹ�ļ�
		PACEntry* p;
		p = (PACEntry*)ind.unpacked_index;
		for (int i = 0; i < hd.file_count; i++) {

			std::string str = file_base;
			str += p->entry_name;
			printf_s("Processing %s\n", str.c_str());
			err2 = fopen_s(&fo, str.c_str(), "wb");
			if (!err2) {
				//ѹ����С���ѹ��Сһ�£�ֱ�Ӹ�������
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

				//��ѹ��
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

	//�������������Ϣ
	PACHead pachead = { 0 };
	pachead.magic[0] = 'P';
	pachead.magic[1] = 'A';
	pachead.magic[2] = 'C';
	pachead.pack_version = mode;

	//��wbģʽ���ļ���ȷ���ļ����ڣ�����������ļ�
	err2 = fopen_s(&fpa, out.c_str(), "wb");
	fclose(fpa);
	err2 = fopen_s(&fpa, out.c_str(), "rb+");
	if (err2) exit(0);
	fseek(fpa, 0, SEEK_SET);
	//�״�д��Ĳ����������ļ�ͷ��������ѹ��ȫ�����֮���ٴ�д��һ��
	fwrite((void*)&pachead, sizeof(PACHead), 1, fpa);

	PACpackinfo* head, * headorg;
	head = new PACpackinfo;
	headorg = head;
	PACpackinfo* tmp = NULL;
	if (hFind2 != INVALID_HANDLE_VALUE) {
		while (bNext) {
			if (strcmp(result.cFileName, "..") && strcmp(result.cFileName, ".")) {
				//����ļ��������ѹ����ʽ��������ѹ��ģʽΪ��ѹ��
				ins_mode = (strstr(result.cFileName, ".png") || strstr(result.cFileName, ".fnt") || strstr(result.cFileName, ".ogg") || strstr(result.cFileName, ".wav")) ? 0 : mode;
				memset(&head->ent, 0, sizeof(PACEntry));
				printf_s("Processing: %s : Size:%d\n", result.cFileName, result.nFileSizeLow);
				//ƴ���ļ�����·��
				std::string str = compress_base;
				str += result.cFileName;
				//��&�����ļ�����
				err = fopen_s(&f, str.c_str(), "rb");
				if (!err && f) {
					byte* fb = new byte[result.nFileSizeLow];
					fread(fb, result.nFileSizeLow, 1, f);
					fclose(f);
					//���index��Ϣ���ļ���,ԭʼ��С,offset
					strcpy_s(head->ent.entry_name, result.cFileName);
					head->ent.unpacked_size = result.nFileSizeLow;
					head->ent.offset = cur_entry;

					//ѡ��ѹ����ʽѹ����д��ѹ�����ݣ����index��Ϣ��ѹ����С
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

		//���ڲ�����ļ��������Ϸ���������洢��Ϣ�����ｫ������Ϣͳ�ϵ�һ��
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

		//��index����ѹ��
		HuffmanEncode he(unc_index, unc_size);
		//д��Huffman��
		int packed_index_size = he.WriteBlock(he.serialized_huffman_tree, fpa);
		//д��Huffman��������
		packed_index_size += he.WriteData(fpa);
		fwrite(&packed_index_size, 4, 1, fpa);
		//ѹ���Ѿ�ȫ����ɣ��ٴ�д��ͷ������
		fseek(fpa, 0, SEEK_SET);
		fwrite((void*)&pachead, sizeof(PACHead), 1, fpa);
		fclose(fpa);
		FindClose(hFind2);
	}
}