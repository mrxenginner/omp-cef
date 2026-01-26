#include <thread>
#include <chrono>
#include "shared/utils.hpp"
#include "resource_dialog.hpp"
#include "bridge.hpp"
#include "plugin.hpp"

ResourceDialog::ResourceDialog(CefPlugin& plugin) : plugin_(plugin)
{

}

void ResourceDialog::Initialize(IPlatformBridge* bridge)
{
    bridge_ = bridge;
}

void ResourceDialog::OnDialogResponse(int playerid, int response)
{
    auto it = player_states_.find(playerid);
    if (it == player_states_.end())
        return;

    auto& state = it->second;
    bool left_button_clicked = (response == 1);

    switch (state.active_dialog)
    {
        case DialogType::PROGRESS:
        {
            /*if (!left_button_clicked) {
                state.active_dialog = DialogType::PAUSE_CONFIRMATION;

                // CefComponent::Instance().PauseDownload(playerid, true);
                // CefComponent::Instance().LockControls(playerid, false);

                plugin_.PauseDownload(playerid, true);

                bridge_->ShowDialog(playerid, DIALOG_ID, 0,
                    "Cancel Download",
                    "Are you sure you want to cancel the resource download?\n\nYou will be disconnected from the server.",
                    "Disconnect", "Continue");
            }*/

            break;
        }
        case DialogType::PAUSE_CONFIRMATION:
        {
            if (left_button_clicked) {
                FinishForPlayer(playerid);
                bridge_->KickPlayer(playerid);
            }
            else {
                state.active_dialog = DialogType::PROGRESS;

                plugin_.PauseDownload(playerid, false);
                // CefComponent::Instance().LockControls(playerid, true);

                UpdateProgress(playerid, 0, 0);
            }
            break;
        }
    }
}

void ResourceDialog::StartForPlayer(int playerid, const std::vector<std::pair<std::string, size_t>>& files_to_download)
{
    PlayerDownloadState state;
    for (const auto& file_info : files_to_download) {
        state.files.push_back({ file_info.first, file_info.second, 0 });
        state.total_bytes_to_download += file_info.second;
    }

    state.active_dialog = DialogType::PROGRESS;
    player_states_[playerid] = std::move(state);

    UpdateProgress(playerid, 0, 0);
}

void ResourceDialog::UpdateProgress(int playerid, uint32_t file_index, uint64_t bytes_received)
{
    auto it = player_states_.find(playerid);
    if (it == player_states_.end()) 
        return;

    auto& state = it->second;

    if (file_index < state.files.size()) {
        state.files[file_index].bytes_received = bytes_received;
    }

    if (state.active_dialog != DialogType::PROGRESS) {
        return;
    }

    uint64_t total_bytes_received = 0;
    uint32_t files_completed = 0;
    for (const auto& file : state.files) {
        total_bytes_received += file.bytes_received;
        if (file.bytes_received > 0 && file.bytes_received == file.total_size) {
            files_completed++;
        }
    }

    std::ostringstream title;
    title << "Downloading resources... (" << files_completed << "/" << state.files.size() << ")";
    
    int percentage = (state.total_bytes_to_download > 0)
        ? static_cast<int>((static_cast<double>(total_bytes_received) / state.total_bytes_to_download) * 100.0)
        : 0;

    std::string button = std::to_string(percentage) + "%";

    std::string content = "File\tStatus\n";
    for (const auto& progress : state.files) {
        content += progress.name + "\t";

        if (state.is_complete || (progress.bytes_received == progress.total_size && progress.total_size > 0)) {
            content += "{88AA62}Done (" + FormatBytes(progress.total_size) + ")\n";
        }
        else if (progress.bytes_received == 0) {
            content += "{808080}Pending... (" + FormatBytes(progress.total_size) + ")\n";
        }
        else {
            content += "{FFFFFF}" + FormatBytes(progress.bytes_received) + " / " + FormatBytes(progress.total_size) + "\n";
        }
    }

    bridge_->ShowDialog(playerid, DIALOG_ID, 5, title.str(), content, button, "");
}

void ResourceDialog::FinishForPlayer(int playerid)
{
    if (!bridge_) 
        return;

    auto it = player_states_.find(playerid);
    if (it == player_states_.end())
        return;

    it->second.is_complete = true;
    UpdateProgress(playerid, 0, 0);

    std::thread([this, playerid]() {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        if (bridge_) {
            bridge_->HideDialog(playerid);
        }

        // CefComponent::Instance().LockControls(playerid, false);

        player_states_.erase(playerid);
    }).detach();
}