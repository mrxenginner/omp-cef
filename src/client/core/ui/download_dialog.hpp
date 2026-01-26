#pragma once

#include <vector>
#include <utility>
#include <cstdint>
#include <string>

class NetworkManager;
class HudManager;
class Samp;

class DownloadDialog
{
public:
    DownloadDialog(NetworkManager* net, HudManager* hud, Samp* samp);
    ~DownloadDialog() = default;

    void Start(std::vector<std::pair<std::string, size_t>> files);
    void Update(uint32_t index, uint64_t received);
    void ShowError(const char* filename, unsigned long status_code, int attempt, int max_retry);
    void Finish();

private:
    NetworkManager* network_;
    HudManager* hud_;
    Samp* samp_;
   
};

/*#pragma once

#include <utility>
#include <cstdint>

class NetworkManager;

namespace DownloadDialog
{
    void Start(NetworkManager* net, std::vector<std::pair<std::string, size_t>> files);
    void Update(NetworkManager* net, uint32_t index, uint64_t received);
    //void ShowError(const char* filename, unsigned long status_code, int attempt, int max_retry);
    //void Finish();
}
*/