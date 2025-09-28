// リソース管理サービス
#pragma once
#include <string>
#include <memory>
#include "AssetTypes.h"

class ResourceService {
public:
    static ResourceService& Instance();
    std::shared_ptr<TextureResource> GetTexture(const std::string& name);
    std::shared_ptr<ModelSharedResource> GetModel(const std::string& name);
    std::shared_ptr<SoundResource> GetSound(const std::string& name, bool streaming = false);

    // 拡張子から自動ディスパッチ（例: ".fbx" → Model / ".png" → Texture）
    bool AutoResolve(const std::string& name);

private:
    ResourceService() = default;
	static ResourceService* s_instance;
};