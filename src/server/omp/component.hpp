#pragma once

#include "Server/Components/Pawn/pawn.hpp"
#include "common/plugin.hpp"

struct ICefOmpComponent : IComponent
{
    PROVIDE_UID(0xD9607C6728B33464);
};

class CefOmpComponent final : public ICefOmpComponent,
                              public PawnEventHandler,
                              public PlayerConnectEventHandler,
                              public PlayerDialogEventHandler
{
public:
    StringView componentName() const override;
    SemanticVersion componentVersion() const override;

    ~CefOmpComponent();

    void onLoad(ICore* core) override;
    void onInit(IComponentList* components) override;
    void onReady() override;
    void onFree(IComponent* component) override;
    void provideConfiguration(ILogger& logger, IEarlyConfig& config, bool defaults) override;
    void free() override;
    void reset() override;

    void onAmxLoad(IPawnScript& script) override;
    void onAmxUnload(IPawnScript& script) override;

    void onPlayerConnect(IPlayer& player) override;
    void onPlayerClientInit(IPlayer& player) override;
    void onPlayerDisconnect(IPlayer& player, PeerDisconnectReason reason) override;
    void onDialogResponse(IPlayer& player, int dialogId, DialogResponse response, int listItem, StringView inputText) override;

private:
    ICore* core_ = nullptr;
    IPawnComponent* pawn_;

    std::unique_ptr<CefPlugin> plugin_;

    bool debug_enabled_ = false;
    uint16_t http_port_ = 7780;
    std::vector<uint8_t> master_resource_key_;
};