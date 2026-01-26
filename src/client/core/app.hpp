#pragma once

#include "shared/packet.hpp"
#include "network/network_manager.hpp"

class BrowserManager;
class AudioManager;
class FocusManager;
class ResourceManager;

class App {
public:
    App(NetworkManager& network,
        BrowserManager& browser,
        AudioManager& audio,
        FocusManager& focus,
        ResourceManager& resources) noexcept
        : network_(network), browser_(browser), audio_(audio), focus_(focus), resources_(resources) {}
    ~App() = default;

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    void Initialize();
    void Shutdown();
    void Tick();
    void OnPacketReceived(const NetworkPacket& packet);

private:
    NetworkManager& network_;
    BrowserManager& browser_;
    AudioManager& audio_;
    FocusManager& focus_;
    ResourceManager& resources_;
};