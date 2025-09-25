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

// ディレクトリ内のファイルリスト取得
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
        std::cout << "使い方: PixAssetPacker.exe <input_dir> <output.PixAssets>" << std::endl;
        return 1;
    }
    std::string inputDir = argv[1];
    std::string outputPak = argv[2];

    // ファイル一覧取得
    auto files = ListFiles(inputDir);
    if (files.empty()) {
        std::cout << "入力ディレクトリにファイルがありません。" << std::endl;
        return 1;
    }

    std::ofstream out(outputPak, std::ios::binary);
    if (!out) {
        std::cout << "出力ファイル作成失敗: " << outputPak << std::endl;
        return 1;
    }

    // ヘッダー: ファイル数
    uint32_t fileCount = static_cast<uint32_t>(files.size());
    out.write((char*)&fileCount, sizeof(fileCount));

    // エントリ情報（後で埋めるため一旦空で書き込む）
    std::vector<AssetEntry> entries(fileCount);
    size_t headerPos = out.tellp();
    size_t entryTableSize = (sizeof(uint32_t) + 260 + sizeof(uint64_t) * 2) * fileCount; // name長+name+size+offset
    std::vector<char> empty(entryTableSize, 0);
    out.write(empty.data(), entryTableSize);

    // データ書き込み
    uint64_t curOffset = sizeof(fileCount) + entryTableSize;
    for (size_t i = 0; i < files.size(); ++i) {
        std::string fullpath = inputDir + "\\" + files[i];
        std::ifstream in(fullpath, std::ios::binary | std::ios::ate);
        if (!in) {
            std::cout << "ファイルオープン失敗: " << fullpath << std::endl;
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

    // エントリテーブル書き込み
    out.seekp(headerPos, std::ios::beg);
    for (auto& e : entries) {
        uint32_t nameLen = static_cast<uint32_t>(e.name.size());
        char nameBuf[260] = {};
        memcpy(nameBuf, e.name.c_str(), nameLen);

        out.write((char*)&nameLen, sizeof(nameLen));
        out.write(nameBuf, 260); // 固定長
        out.write((char*)&e.size, sizeof(e.size));
        out.write((char*)&e.offset, sizeof(e.offset));
    }

    std::cout << "パッキング完了: " << outputPak << std::endl;
    return 0;
}