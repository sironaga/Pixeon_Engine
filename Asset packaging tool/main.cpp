#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

struct AssetEntry {
    std::string name;
    uint64_t size;
    uint64_t offset;
};

// �f�B���N�g�����̃t�@�C�����X�g�擾
std::vector<std::string> ListFiles(const std::string& dir) {
    std::vector<std::string> files;
    WIN32_FIND_DATAA fd;
    std::string pattern = dir + "\\*";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return files;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            files.push_back(fd.cFileName);
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return files;
}

int main(int argc, char* argv[]){
    if (argc != 3) {
        std::cout << "�g����: PixAssetPacker.exe <input_dir> <output.PixAssets>" << std::endl;
        return 1;
    }
    std::string inputDir = argv[1];
    std::string outputPak = argv[2];

    // �t�@�C���ꗗ�擾
    auto files = ListFiles(inputDir);
    if (files.empty()) {
        std::cout << "���̓f�B���N�g���Ƀt�@�C��������܂���B" << std::endl;
        return 1;
    }

    std::ofstream out(outputPak, std::ios::binary);
    if (!out) {
        std::cout << "�o�̓t�@�C���쐬���s: " << outputPak << std::endl;
        return 1;
    }

    // �w�b�_�[: �t�@�C����
    uint32_t fileCount = static_cast<uint32_t>(files.size());
    out.write((char*)&fileCount, sizeof(fileCount));

    // �G���g�����i��Ŗ��߂邽�߈�U��ŏ������ށj
    std::vector<AssetEntry> entries(fileCount);
    size_t headerPos = out.tellp();
    size_t entryTableSize = (sizeof(uint32_t) + 260 + sizeof(uint64_t) * 2) * fileCount; // name��+name+size+offset
    std::vector<char> empty(entryTableSize, 0);
    out.write(empty.data(), entryTableSize);

    // �f�[�^��������
    uint64_t curOffset = sizeof(fileCount) + entryTableSize;
    for (size_t i = 0; i < files.size(); ++i) {
        std::string fullpath = inputDir + "\\" + files[i];
        std::ifstream in(fullpath, std::ios::binary | std::ios::ate);
        if (!in) {
            std::cout << "�t�@�C���I�[�v�����s: " << fullpath << std::endl;
            return 1;
        }
        uint64_t sz = in.tellg();
        in.seekg(0, std::ios::beg);
        std::vector<char> buf(sz);
        in.read(buf.data(), sz);
        out.write(buf.data(), sz);

        entries[i].name = files[i];
        entries[i].size = sz;
        entries[i].offset = curOffset;
        curOffset += sz;
    }

    // �G���g���e�[�u����������
    out.seekp(headerPos, std::ios::beg);
    for (auto& e : entries) {
        uint32_t nameLen = static_cast<uint32_t>(e.name.size());
        char nameBuf[260] = {};
        memcpy(nameBuf, e.name.c_str(), nameLen);

        out.write((char*)&nameLen, sizeof(nameLen));
        out.write(nameBuf, 260); // �Œ蒷
        out.write((char*)&e.size, sizeof(e.size));
        out.write((char*)&e.offset, sizeof(e.offset));
    }

    std::cout << "�p�b�L���O����: " << outputPak << std::endl;
    return 0;
}