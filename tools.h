#pragma once
#include <stdio.h>
#include <map>
#include <vector>
#include <algorithm>
#include <queue>
#include <string>
typedef unsigned char BYTE;
typedef BYTE byte;
typedef unsigned short ushort;
#define rol(value, bits) ((value << bits) | (value >> (sizeof(value)*8 - bits)))
#define LEN 512

int ReadInt(FILE* fd, int pos);
int ReadInt(FILE* fd);
unsigned char ReadByte(FILE* fd, int pos);
unsigned char ReadByte(FILE* fd);
void ReadBytes_s(FILE* fd, void* ret, int retsize, int pos, int size);
void ReadBytes_s(FILE* fd, void* ret, int retsize, int size);
void IntToBytes(int num, unsigned char* bytes);
int GetFileSize(FILE* fd);
int ToInt32(byte* src, int offset);

struct huffman_node {
    byte c;
    int freq;
    char huffman_code[LEN];
    huffman_node* left_child,* right_child;
    bool leaf;
};

class HuffmanDecode {
    byte* m_input;
    byte* m_output;

    int m_length;
    int m_src;
    int m_bits;
    int m_bit_count;

    ushort* lhs = new ushort[LEN];
    ushort* rhs = new ushort[LEN];
    ushort token = 256;
public:
    HuffmanDecode(byte* src, int length, byte* dst);
    byte* Unpack();
private:
    ushort CreateTree();
    int GetBits(int count);
};


class HuffmanEncode
{
    
    std::map<byte, int> huffman_table;
    int bits_pos;
    byte pack_tmp;
    huffman_node* root;
    byte* src;
    int size;
public:
    std::map<byte, std::string> huffman_code;
    std::vector<byte> serialized_huffman_tree;
    HuffmanEncode(byte* source, int src_size);
    int WriteBlock(std::vector<byte>& vec, FILE* fd);
    int WriteData(FILE* fd);

private:
    void CalFreq();
    void CreateTree();
    void Packbyte(std::vector<byte> &vec, byte b, int len = 8);
    void DestoryHuffmanTree(huffman_node* node);
    void GetCode();
    void Serialize(huffman_node* node);
};


        
