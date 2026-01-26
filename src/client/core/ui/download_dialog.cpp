#include "download_dialog.hpp"

#include "network/network_manager.hpp"
#include "ui/hud_manager.hpp"
#include "samp/samp.hpp"

DownloadDialog::DownloadDialog(NetworkManager* net, HudManager* hud, Samp* samp)
    : network_(net), hud_(hud), samp_(samp)
{
}

void DownloadDialog::Start(std::vector<std::pair<std::string, size_t>> files)
{
    /*m_isDownloading = true;
    m_files = files; // Garde une copie si nécessaire
    
    m_samp->SetControlsLocked(true);
    m_hudManager->ToggleComponent(EHudComponent::ALL, false);

    DownloadStartedPacket packet{};
    packet.files_to_download = std::move(files);
    m_network->SendPacket(PacketType::DownloadStarted, packet);*/
}

void DownloadDialog::Update(uint32_t index, uint64_t received)
{
    /*DownloadProgressPacket packet{};
    packet.file_index = index;
    packet.bytes_received = received;

    m_network->SendPacket(PacketType::DownloadProgress, packet);*/
}

void DownloadDialog::ShowError(const char* filename, unsigned long status_code, int attempt, int max_retry)
{
    /*DownloadFailedPacket packet{};
    packet.statusCode = status_code;
    packet.attempt = attempt;
    packet.max_retry = max_retry;

    if (filename) {
        packet.filename = filename;
    }

    m_network->SendPacket(PacketType::DownloadFailed, packet);*/
}

void DownloadDialog::Finish()
{
    /*m_isDownloading = false;
    
    m_samp->SetControlsLocked(false);
    m_hudManager->ToggleComponent(EHudComponent::ALL, true);
    
    m_network->SendPacket(PacketType::DownloadComplete, {});*/
}



/*#include "download_dialog.hpp"

#include "network/network_manager.hpp"

namespace DownloadDialog
{
    void Start(NetworkManager* net, std::vector<std::pair<std::string, size_t>> files)
    {
        Samp::Instance().SetControlsLocked(true);
        HudManager::Instance().ToggleComponent(EHudComponent::ALL, false);

        DownloadStartedPacket packet{};
        packet.files_to_download = std::move(files);
        net->SendPacket(PacketType::DownloadStarted, packet);
    }

    void Update(NetworkManager* net, uint32_t index, uint64_t received)
    {
        DownloadProgressPacket packet{};
        packet.file_index = index;
        packet.bytes_received = received;

        net->SendPacket(PacketType::DownloadProgress, packet);
    }

    void ShowError(const char* filename, unsigned long status_code, int attempt, int max_retry)
    {
        DownloadFailedPacket failed_payload{};
        failed_payload.statusCode = status_code;
        failed_payload.attempt = attempt;
        failed_payload.max_retry = max_retry;

        if (filename) {
            failed_payload.filename = filename;
        }

        CefPacket packet;
        packet.type = PacketType::DownloadFailed;
        packet.payload = failed_payload;

        NetworkManager::Instance().SendPacketToServer(packet);

        DownloadFailedPacket failed_payload;
        std::memset(failed_payload.filename, 0, sizeof(failed_payload.filename));
        if (filename && *filename) {
            strncpy_s(failed_payload.filename, filename, sizeof(failed_payload.filename) - 1);
        }

        failed_payload.statusCode = status_code;
        failed_payload.attempt = attempt;
        failed_payload.max_retry = max_retry;

        CefPacket packet;
        packet.type = PacketType::DownloadFailed;
        packet.payload = failed_payload;

        NetworkManager::Instance().SendPacketToServer(packet);
    }

    void Finish()
    {
        CefPacket complete_packet;
        complete_packet.type = PacketType::DownloadComplete;
        NetworkManager::Instance().SendPacketToServer(complete_packet);
    }
}*/
