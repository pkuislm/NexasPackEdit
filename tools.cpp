#include "tools.h"

//一些被封装起来的读取函数
int ReadInt(FILE* fd, int pos) {
	int ret = 0;
	fseek(fd, pos, SEEK_SET);
	fread_s(&ret, 4, 4, 1, fd);
	return ret;
}
int ReadInt(FILE* fd) {
	int ret = 0;
	fread_s(&ret, 4, 4, 1, fd);
	return ret;
}
unsigned char ReadByte(FILE* fd, int pos){
	unsigned char ret;
	fseek(fd, pos, SEEK_SET);
	fread_s(&ret, 1, 1, 1, fd);
	return ret;
}
unsigned char ReadByte(FILE* fd) {
	unsigned char ret;
	fread_s(&ret, 1, 1, 1, fd);
	return ret;
}
void ReadBytes_s(FILE* fd, void* ret, int retsize, int pos, int size) {
	fseek(fd, pos, SEEK_SET);
	fread_s(ret, retsize, size, 1, fd);
}
void ReadBytes_s(FILE* fd, void* ret, int retsize, int size) {
	fread_s(ret, retsize, size, 1, fd);
}
//转换函数
void IntToBytes(int num, unsigned char* bytes)
{
	for (int i = 4 - 1; i >= 0; i--)
	{
		int offset = i * 8;
		bytes[i] = (num >> offset) & 0xFF;
	}
}
int GetFileSize(FILE* fd){
	int pos = ftell(fd);
	fseek(fd, 0, SEEK_END);
	int s = ftell(fd);
	fseek(fd, pos, SEEK_SET);
	return s;
}
int ToInt8(byte* src, int offset) {
    int t = 0;
    byte* p = (byte*)&t;
    p[0] = src[offset];
    return t;
}



HuffmanDecode::HuffmanDecode(byte* src, int length, byte* dst)
{        
    m_input = src;
    m_output = dst;

    m_length = length;
    m_src = 0;
    m_bit_count = 0;
}

byte* HuffmanDecode::Unpack(){
    int dst = 0;
    token = 256;
    ushort root = CreateTree();
    while (dst < m_length)
    {
        ushort symbol = root;
        while (symbol >= 0x100)
        {
            if (0 != GetBits(1))
                symbol = rhs[symbol];
            else
                symbol = lhs[symbol];
        }
        m_output[dst++] = (byte)symbol;
    }
    return m_output;
}

ushort HuffmanDecode::CreateTree(){
    if (0 != GetBits(1)){
        //非叶子结点，继续递归
        ushort v = token++;
        if (v >= 512) {
            printf("Invalid Compress data.");
            return 0;
        }
        lhs[v] = CreateTree();
        rhs[v] = CreateTree();
        return v;
    }
    else{
        //叶子结点，取出节点值
        return (ushort)GetBits(8);
    }
}

int HuffmanDecode::GetBits(int count)
{
    while (m_bit_count < count)
    {
        int b = ToInt8(m_input, m_src);
        m_src++;
        m_bits = (m_bits << 8) | b;
        m_bit_count += 8;
    }
    int mask = (1 << count) - 1;
    m_bit_count -= count;
    return (m_bits >> m_bit_count) & mask;
}


HuffmanEncode::HuffmanEncode(byte* source, int src_size)
{
    root = NULL;
    src = source;
    size = src_size;
    bits_pos = 0;
    pack_tmp = 0;
    CalFreq();
    CreateTree();
    GetCode();
    Serialize(root);
    //这里真的要补0吗
    //if (bits_pos != 0)
    //    Packbyte(serialized_huffman_tree, 0, 8 - bits_pos);
    DestoryHuffmanTree(root);

}
void HuffmanEncode::CalFreq() {
    //计算频度
    huffman_table.clear();
    huffman_code.clear();
    byte* p = src;
    for (int i = 0; i < size; i++) {
        auto it = huffman_table.find(*p);
        if (it == huffman_table.end())
            huffman_table.insert(std::make_pair(*p, 1));
        else
            it->second++;
        p++;
    }
}
bool SortByFreq(huffman_node* x, huffman_node* y) {
    return x->freq < y->freq;
}
void HuffmanEncode::CreateTree() {
    std::vector<huffman_node*> huffman_tree_node;

    //创建所有叶子节点
    std::map<byte, int>::iterator it_t;
    for (it_t = huffman_table.begin(); it_t != huffman_table.end(); it_t++) {
        huffman_node* node = new huffman_node;
        node->c = it_t->first;
        node->freq = it_t->second;
        node->left_child = NULL;
        node->right_child = NULL;
        memset(node->huffman_code, 0, LEN);
        node->leaf = true;
        huffman_tree_node.push_back(node);
    }

    //构建哈夫曼树
    while (huffman_tree_node.size() > 0) {
        //按照频率升序排序
        sort(huffman_tree_node.begin(), huffman_tree_node.end(), SortByFreq);

        if (huffman_tree_node.size() == 1) {
            //哈夫曼树已经生成完成
            root = huffman_tree_node[0];
            huffman_tree_node.erase(huffman_tree_node.begin());
        }
        else {
            //取出前两个
            huffman_node* node_1 = huffman_tree_node[0];
            huffman_node* node_2 = huffman_tree_node[1];
            //删除
            huffman_tree_node.erase(huffman_tree_node.begin());
            huffman_tree_node.erase(huffman_tree_node.begin());
            //生成新的节点
            huffman_node* node = new huffman_node;
            node->leaf = false;
            node->freq = node_1->freq + node_2->freq;
            (node_1->freq < node_2->freq) ? (node->left_child = node_1, node->right_child = node_2) : (node->left_child = node_2, node->right_child = node_1);
            huffman_tree_node.push_back(node);
        }
    }
}
void HuffmanEncode::GetCode() {
    if (root == NULL) return;
    //利用层次遍历，构造每一个节点的前缀码
    huffman_node* p = root;
    std::queue<huffman_node*> q;
    q.push(p);
    while (q.size() > 0) {
        p = q.front();
        q.pop();
        if (p->left_child != NULL) {
            q.push(p->left_child);
            strcpy_s((p->left_child)->huffman_code, p->huffman_code);
            char* ptr = (p->left_child)->huffman_code;
            while (*ptr != '\0') {
                ptr++;
            }
            *ptr = '0';
            *(ptr + 1) = '\0';
        }
        if (p->right_child != NULL) {
            q.push(p->right_child);
            strcpy_s((p->right_child)->huffman_code, p->huffman_code);
            char* ptr = (p->right_child)->huffman_code;
            while (*ptr != '\0') {
                ptr++;
            }
            *ptr = '1';
            *(ptr + 1) = '\0';
        }
    }
}

void HuffmanEncode::Serialize(huffman_node* node) {
    //序列化哈夫曼树，下面注释掉的三句话可以打印 http://mshang.ca/syntree/ 能识别的语法树。
    if (node != NULL) {
        if (!node->leaf) {
            Packbyte(serialized_huffman_tree, 1, 1);
            //printf_s("[Node ");
            Serialize(node->left_child);
            Serialize(node->right_child);
            //printf_s("]");
        }else {
            Packbyte(serialized_huffman_tree, 0, 1);
            Packbyte(serialized_huffman_tree, node->c);
            huffman_code.insert(std::make_pair(node->c, node->huffman_code));
            //printf_s("[0x%02X] ", node->c);
        }
    }
}

void HuffmanEncode::Packbyte(std::vector<byte> &vec, byte b, int len) {
    //按比特存储，满一字节后输出
    while (len--) {
        pack_tmp = pack_tmp << 1 | ((b >> len) & 1);
        bits_pos++;
        if (bits_pos == 8) {
            pack_tmp = ~pack_tmp;
            vec.push_back(pack_tmp);
            bits_pos = 0;
            pack_tmp = 0;
        }
    }
}

void HuffmanEncode::DestoryHuffmanTree(huffman_node* node) {
    if (node != NULL) {
        DestoryHuffmanTree(node->left_child);
        DestoryHuffmanTree(node->right_child);
        delete[] node;
        node = NULL;
    }
}

int HuffmanEncode::WriteBlock(std::vector<byte>& vec, FILE* fd) {
    int total = 0;
    auto it = vec.begin();
    for (; it != vec.end(); ++it) {
        fwrite(&(*it), 1, 1, fd);
        total++;
    }    
    return total;
}

int HuffmanEncode::WriteData(FILE* fd) {
    byte* p = src;
    std::vector<byte> tmp;
    for (int i = 0; i < size; i++) {
        auto it = huffman_code.find(*p);
        if (it == huffman_code.end()) {
            printf("Error: HuffmanTable is missing element.");
            return 0;
        }
        else {
            for (int i = 0; i < it->second.size(); i++) {
                //将以ASCII存储的前缀码转换为二进制
                switch (it->second.data()[i]) {
                case '0':
                    Packbyte(tmp, 0, 1);
                    break;
                case '1':
                    Packbyte(tmp, 1, 1);
                    break;
                default:
                    break;
                }
            }
        } 
        p++;
    }
    if (bits_pos != 0)
        Packbyte(tmp, 0, 8 - bits_pos);
    return WriteBlock(tmp, fd);
}