#pragma once

#include <unordered_map>
#include "bridge.hpp"

class CefPlugin;

class ResourceDialog
{
public:
    // ResourceDialog() = default;
    explicit ResourceDialog(CefPlugin& plugin);
    ~ResourceDialog() = default;

    ResourceDialog(const ResourceDialog&) = delete;
    ResourceDialog& operator=(const ResourceDialog&) = delete;

    void Initialize(IPlatformBridge* bridge);
    void OnDialogResponse(int playerid, int response);

    void StartForPlayer(int playerid, const std::vector<std::pair<std::string, size_t>>& files_to_download);
    void UpdateProgress(int playerid, uint32_t file_index, uint64_t bytes_received);
    void FinishForPlayer(int playerid);

private:
    enum class DialogType 
    {
        NONE,
        PROGRESS,
        RETRY_ERROR,
        PAUSE_CONFIRMATION
    };

    struct FileProgress 
    {
        std::string name;
        size_t total_size;
        size_t bytes_received = 0;
    };

    struct PlayerDownloadState 
    {
        DialogType active_dialog = DialogType::NONE;
        std::vector<FileProgress> files;
        uint64_t total_bytes_to_download = 0;
        uint64_t total_bytes_received = 0;
        uint32_t files_completed = 0;
        bool is_complete = false;
    };

    static constexpr int DIALOG_ID = 32767; // TODO

    IPlatformBridge* bridge_ = nullptr;
    CefPlugin& plugin_;
    std::unordered_map<int, PlayerDownloadState> player_states_;
};