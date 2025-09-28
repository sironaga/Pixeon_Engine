// ���\�[�X�Ǘ��T�[�r�X
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

    // �g���q���玩���f�B�X�p�b�`�i��: ".fbx" �� Model / ".png" �� Texture�j
    bool AutoResolve(const std::string& name);

private:
    ResourceService() = default;
	static ResourceService* s_instance;
};