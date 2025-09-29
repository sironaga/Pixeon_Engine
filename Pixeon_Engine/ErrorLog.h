#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <Windows.h>

class ErrorLogger {
public:
    struct Entry {
        std::string tag;
        std::string message;
        int         count = 0;
        bool        shownMessageBox = false;
    };

    static ErrorLogger& Instance() {
        static ErrorLogger inst;
        return inst;
    }

    // �G���[�L�^
    void LogError(const std::string& tag,
        const std::string& message,
        bool fatal = false,
        int perMsgLimit = 1)
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        std::string key = tag + "||" + message;
        auto& e = m_entries[key];
        if (e.count == 0) {
            e.tag = tag;
            e.message = message;
        }
        e.count++;

        // ��Ƀf�o�b�O�o��
        std::string dbg = "[ERROR][" + tag + "] " + message + " (count=" + std::to_string(e.count) + ")\n";
        OutputDebugStringA(dbg.c_str());

        // �\������:
        // fatal �̏ꍇ �� ��� MessageBox
        // ����ȊO �� ���� (e.count==1) �� perMsgLimit �����\���̊Ԃ̂�
        bool needBox = false;
        if (fatal) {
            needBox = true;
        }
        else if (!e.shownMessageBox && e.count <= perMsgLimit) {
            needBox = true;
        }

        if (needBox) {
            std::string boxTitle = fatal ? ("FATAL: " + tag) : ("Error: " + tag);
            MessageBoxA(nullptr, message.c_str(), boxTitle.c_str(), MB_OK | MB_ICONERROR);
            if (!fatal && e.count >= perMsgLimit) {
                e.shownMessageBox = true;
            }
        }
    }

    // �x���i�K�v�Ȃ�g���j
    void LogWarning(const std::string& tag, const std::string& message, int perMsgLimit = 2) {
        std::lock_guard<std::mutex> lk(m_mtx);
        std::string key = "W:" + tag + "||" + message;
        auto& e = m_entries[key];
        if (e.count == 0) {
            e.tag = tag;
            e.message = message;
        }
        e.count++;
        std::string dbg = "[WARN][" + tag + "] " + message + " (count=" + std::to_string(e.count) + ")\n";
        OutputDebugStringA(dbg.c_str());
        if (e.count <= perMsgLimit) {
            MessageBoxA(nullptr, message.c_str(), ("Warning: " + tag).c_str(), MB_OK | MB_ICONWARNING);
        }
    }

    // GUI �o�͗p
    void DrawDebugGUI(bool* pOpen = nullptr);

private:
    ErrorLogger() = default;
    std::unordered_map<std::string, Entry> m_entries;
    std::mutex m_mtx;
};