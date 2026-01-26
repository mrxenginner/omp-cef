#pragma once

#include <asio.hpp>
#include <memory>
#include <shared/packet.hpp>

#include "api.hpp"
#include "bridge.hpp"
#include "logger.hpp"
#include "network.hpp"
#include "resource_dialog.hpp"
#include "resource_manager.hpp"
#include "security.hpp"
#include "session.hpp"

class CefPlugin
{
public:
	CefPlugin();
	CefPlugin(const CefPlugin&) = delete;
	CefPlugin& operator=(const CefPlugin&) = delete;
	~CefPlugin();

	void Initialize(std::unique_ptr<IPlatformBridge> bridge, std::vector<uint8_t> master_resource_key);
	void Shutdown();

	void OnPlayerConnect(int playerid);
	void OnPlayerClientInit(int playerid);
	void OnPlayerDisconnect(int playerid);
	void OnDialogResponse(int playerid, int dialogid, int response, int listitem, const std::string& inputtext);

	void OnPacketReceived(const asio::ip::udp::endpoint& from, const char* data, int len);

	void HandleFileRequest(int playerid, const RequestFilesPacket& request);
	void ProcessFileTransfers();
	void ScheduleFileTransferTick();

	void PauseDownload(int playerid, bool pause);

	void SendRawPacketToEndpoint(const asio::ip::udp::endpoint& endpoint, PacketType type, const PacketPayload& payload);
	void SendPacketToPlayer(int playerid, PacketType type, const PacketPayload& payload);

	void NotifyCefInitialize(std::shared_ptr<NetworkSession> session, bool ok);
	void HandleClientEvent(int playerid, const ClientEmitEventPacket& payload);

	ResourceManager& GetResourceManager()
	{
		return *resource_;
	}

	NetworkSessionManager& GetNetworkSessionManager()
	{
		return *sessions_;
	}

	const std::vector<uint8_t>& GetMasterKey() const { return master_resource_key_; }

private:
	void HandleRequestJoin(const asio::ip::udp::endpoint& from, const RequestJoinPacket& packet);
	void HandleHandshakeFinalize(const asio::ip::udp::endpoint& from, const HandshakeFinalizePacket& finalize_packet, std::shared_ptr<NetworkSession> session);
	void HandleKcpInput(std::shared_ptr<NetworkSession> session);

private:
	std::unique_ptr<IPlatformBridge> bridge_;
	std::unique_ptr<SecurityManager> security_;
	std::unique_ptr<NetworkSessionManager> sessions_;
	std::unique_ptr<CefApi> api_;
	std::unique_ptr<ResourceManager> resource_;
	std::unique_ptr<ResourceDialog> resource_dialog_;

	Logger logger_;

	std::vector<uint8_t> master_resource_key_;

	asio::io_context io_context_;
	asio::steady_timer transfer_timer_{ io_context_ };
	std::unique_ptr<NetworkServer> network_server_;
	std::thread network_thread_;
	std::atomic<bool> running_{ false };
};