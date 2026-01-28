#include "plugin.hpp"

#include <shared/crypto.hpp>

#include "resource_manager.hpp"
#include "session.hpp"
#include "shared/packet-serializer.hpp"
#include "shared/packet.hpp"
#include "shared/utils.hpp"
#include <shared/events.hpp>

CefPlugin::CefPlugin()
{
	security_ = std::make_unique<SecurityManager>();
	sessions_ = std::make_unique<NetworkSessionManager>();
	api_ = std::make_unique<CefApi>(*this);
	resource_ = std::make_unique<ResourceManager>();
	resource_dialog_ = std::make_unique<ResourceDialog>(*this);
}

void CefPlugin::Initialize(std::unique_ptr<IPlatformBridge> bridge, std::vector<uint8_t> master_resource_key)
{
	if (running_)
		return;

	bridge_ = std::move(bridge);
	master_resource_key_ = master_resource_key;

	logger_.SetBridge(bridge_.get());
	logger_.SetLevel(CefLogLevel::Debug); // TODO: debug_enabled
	logging::SetLogger(&logger_);

	security_->Initialize(io_context_);
	resource_dialog_->Initialize(bridge_.get());

	try
	{
		network_server_ = std::make_unique<NetworkServer>(
			7779,
			io_context_,
			[this](const asio::ip::udp::endpoint& from, const char* data, int len)
			{ 
				this->OnPacketReceived(from, data, len); 
			},
			[this](uint32_t now_ms)
			{
				sessions_->UpdateAllKcpInstances(now_ms);
				this->ProcessFileTransfers();
			});

		sessions_->SetSender(
			[this](const asio::ip::udp::endpoint& addr, const char* data, int len)
			{
				if (!network_server_)
					return;

				network_server_->SendTo(addr, data, len);
			});

		network_server_->Start();

		io_context_.restart();

		network_thread_ = std::thread([this]() {
			io_context_.run();
		});

		running_ = true;

		LOG_INFO("Network Server started successfully on port %d.", 7779);
	}
	catch (const std::exception& e)
	{
		network_server_.reset();
		logging::SetLogger(nullptr);
		bridge_.reset();

		LOG_ERROR("Failed to start Network Server: %s", e.what());
	}
}

void CefPlugin::Shutdown()
{
	if (!running_)
		return;

	running_ = false;

    if (network_server_) {
        network_server_->Stop();
    }

    if (security_) {
        security_->Shutdown();
        security_.reset();
    }

    io_context_.stop();

    if (network_thread_.joinable()) {
        network_thread_.join();
    }

    network_server_.reset();
    sessions_.reset();
    api_.reset();
    resource_.reset();
    resource_dialog_.reset();

    logging::SetLogger(nullptr);
    bridge_.reset();
}

void CefPlugin::OnPlayerConnect(int playerid)
{
	sessions_->RegisterPlayer(playerid);
}

void CefPlugin::OnPlayerClientInit(int playerid)
{
	auto session = sessions_->GetSession(playerid);
	if (session && session->handshake_complete) {
		LOG_INFO("[SERVER] Player %d connected, CEF handshake complete.", playerid);
		NotifyCefInitialize(session, true);
		return;
	}

	auto timer = std::make_shared<asio::steady_timer>(io_context_);
    timer->expires_after(std::chrono::seconds(10));
    timer->async_wait([this, playerid, timer](const std::error_code& error_code) {
        if (error_code || !running_) return;

        auto session = sessions_->GetSession(playerid);
        if (!session) return;

        if (!session->handshake_complete) {
            NotifyCefInitialize(session, false);
        }
    });
}

void CefPlugin::OnPlayerDisconnect(int playerid)
{
	sessions_->RemovePlayer(playerid);
}

void CefPlugin::OnDialogResponse(int playerid, int dialogid, int response, int listitem, const std::string& inputtext) 
{
	if (dialogid == 32767) // TODO
	{
		resource_dialog_->OnDialogResponse(playerid, response);
	}
}

void CefPlugin::OnPacketReceived(const asio::ip::udp::endpoint& from, const char* data, int len)
{
	auto network_session = sessions_->GetSessionFromAddress(from);
	if (network_session && network_session->handshake_status == HandshakeStatus::CONNECTED && network_session->kcp_instance) {
		{
			std::lock_guard<std::mutex> lock(network_session->kcp_mutex);
			ikcp_input(network_session->kcp_instance, data, len);
		}
		HandleKcpInput(network_session);
		return;
	}

	NetworkPacket packet;
	if (!DeserializePacket(data, len, packet))
		return;

	switch (packet.type)
	{
		case PacketType::RequestJoin: {
			HandleRequestJoin(from, std::get<RequestJoinPacket>(packet.payload));
			break;
		}
		case PacketType::HandshakeFinalize:
			if (network_session && network_session->handshake_status == HandshakeStatus::CHALLENGED) {
				HandleHandshakeFinalize(from, std::get<HandshakeFinalizePacket>(packet.payload), network_session);
			}
		default:
			break;
	}
}

void CefPlugin::HandleRequestJoin(const asio::ip::udp::endpoint& from, const RequestJoinPacket& join_packet)
{
	int playerid = join_packet.playerid;

	std::string official_ip = bridge_->GetPlayerIp(playerid);
	if (!official_ip.empty() && official_ip != from.address().to_string())
		return;

	auto session = sessions_->GetOrCreateSession(playerid);
	session->address = from;
	session->handshake_status = HandshakeStatus::CHALLENGED;

	sessions_->MapAddressToPlayer(playerid, from);

	HandshakeChallengePacket response;
	response.cookie = security_->GenerateCookie(from);
	response.server_public_key = security_->InitiateKeyExchange(playerid);

	SendRawPacketToEndpoint(from, PacketType::HandshakeChallenge, response);
}

void CefPlugin::HandleHandshakeFinalize(const asio::ip::udp::endpoint& from, 
	const HandshakeFinalizePacket& finalize_packet, 
	std::shared_ptr<NetworkSession> session)
{
	if (!security_->ValidateCookie(from, finalize_packet.cookie))
		return;

	auto session_keys = security_->FinalizeKeyExchange(session->playerid, finalize_packet.client_public_key);
	if (!session_keys)
		return;

	session->rx_key = std::move(session_keys->rx);
	session->tx_key = std::move(session_keys->tx);

	JoinResponsePacket join_response;
	join_response.accepted = true;
	join_response.kcp_conv_id = session->playerid;

	nlohmann::json manifest = resource_->GetManifestAsJson();
	if (!manifest.is_null()) {
		join_response.manifest_json = manifest.dump();
	}

	SendRawPacketToEndpoint(from, PacketType::JoinResponse, join_response);

	session->kcp_instance = ikcp_create(session->playerid, session.get());
	session->kcp_instance->output = kcp_output_callback;

	ikcp_nodelay(session->kcp_instance, 1, 10, 2, 1);
	ikcp_wndsize(session->kcp_instance, 256, 256);

	session->handshake_status = HandshakeStatus::CONNECTED;

	ServerConfigPacket config_packet;
	config_packet.master_resource_key = master_resource_key_;
	SendPacketToPlayer(session->playerid, PacketType::ServerConfig, config_packet);

	session->handshake_complete = true;

	LOG_INFO("Network handshake for player %d is complete.", session->playerid);
}

void CefPlugin::HandleKcpInput(std::shared_ptr<NetworkSession> session)
{
    if (!session)
        return;

    std::vector<NetworkPacket> pendingPackets;

    {
        std::lock_guard<std::mutex> lock(session->kcp_mutex);

        if (!session->kcp_instance)
            return;

        std::vector<char> kcp_buffer(65535);
        int msg_size;

        while ((msg_size = ikcp_recv(session->kcp_instance, kcp_buffer.data(),
            static_cast<int>(kcp_buffer.size()))) > 0)
        {
            std::vector<uint8_t> decrypted =
                DecryptPacket({ kcp_buffer.begin(), kcp_buffer.begin() + msg_size }, session->rx_key);

            if (decrypted.empty())
                continue;

            NetworkPacket packet;
            if (!DeserializePacket(reinterpret_cast<const char*>(decrypted.data()), decrypted.size(), packet))
                continue;

            pendingPackets.emplace_back(std::move(packet));
        }
    }

    for (auto& packet : pendingPackets)
    {
        switch (packet.type)
        {
            case PacketType::RequestFiles:
            {
                HandleFileRequest(session->playerid, std::get<RequestFilesPacket>(packet.payload));
                break;
            }
            case PacketType::DownloadStarted:
            {
                const auto& download_started_packet = std::get<DownloadStartedPacket>(packet.payload);
                resource_dialog_->StartForPlayer(session->playerid, download_started_packet.files_to_download);
                break;
            }
            case PacketType::DownloadProgress:
            {
                const auto& download_progress_packet = std::get<DownloadProgressPacket>(packet.payload);
                resource_dialog_->UpdateProgress(session->playerid, download_progress_packet.file_index, download_progress_packet.bytes_received);
                break;
            }
            case PacketType::ClientEmitEvent:
            {
                if (auto* event = std::get_if<ClientEmitEventPacket>(&packet.payload)) {
                    HandleClientEvent(session->playerid, *event);
                }
                else {
                    LOG_ERROR("ClientEmitEvent payload mismatch (variant index=%zu)", packet.payload.index());
                }
                break;
            }
            default:
                break;
        }
    }
}

void CefPlugin::HandleFileRequest(int playerid, const RequestFilesPacket& request)
{
	auto session = sessions_->GetSession(playerid);
	if (!session)
		return;

	for (const auto& [resourceName, relativePath] : request.files) {
		if (resource_->IsFileValid(resourceName, relativePath)) {

			std::vector<uint8_t> content;
			if (!resource_->GetPakContent(resourceName, content)) {
				continue;
			}

			auto transfer = std::make_shared<FileTransfer>();
			transfer->resourceName = resourceName;
			transfer->relativePath = relativePath;
			transfer->fileHash = CalculateSHA256FromData(content);
			transfer->content = std::move(content);
			transfer->totalChunks = (transfer->content.size() + FILE_CHUNK_SIZE - 1) / FILE_CHUNK_SIZE;
			transfer->currentChunkIndex = 0;

			session->download_queue.push(transfer);
		}
	}
}

void CefPlugin::ProcessFileTransfers()
{
    static constexpr int MAX_CHUNKS_PER_TICK = 64;
    static constexpr int MAX_IN_FLIGHT_SEGMENTS = 220; 

    auto all_sessions = sessions_->GetAllSessions();
    for (auto& session : all_sessions)
    {
        if (!session || !session->kcp_instance || !session->handshake_complete)
            continue;

		if (session->is_download_paused.load(std::memory_order_relaxed)) {
            continue;
        }

        if (!session->current_transfer && !session->download_queue.empty())
        {
            session->current_transfer = session->download_queue.front();
            session->download_queue.pop();

            LOG_INFO("[Transfer] Starting transfer for player %d - file '%s'",
                session->playerid,
                session->current_transfer->relativePath);
        }

        auto& transfer = session->current_transfer;
        if (!transfer) continue;

        int sent_this_tick = 0;

        while (transfer && sent_this_tick < MAX_CHUNKS_PER_TICK)
        {
			if (session->is_download_paused.load(std::memory_order_relaxed)) {
                break;
            }

            if (transfer->currentChunkIndex >= transfer->totalChunks)
            {
                LOG_INFO("[Transfer] Completed transfer for player %d - file '%s'",
                    session->playerid,
                    transfer->relativePath);

                session->current_transfer = nullptr;
                break;
            }

            int in_flight = 0;
            {
                std::lock_guard<std::mutex> guard(session->kcp_mutex);
                if (!session->kcp_instance) break;
                in_flight = ikcp_waitsnd(session->kcp_instance);
            }

            if (in_flight >= MAX_IN_FLIGHT_SEGMENTS)
            {
                break;
            }


            size_t chunkOffset = transfer->currentChunkIndex * FILE_CHUNK_SIZE;
            size_t remaining   = transfer->content.size() - chunkOffset;
            size_t chunkSize   = std::min(static_cast<size_t>(FILE_CHUNK_SIZE), remaining);

            FileDataPacket packet;
            packet.resourceName = transfer->resourceName;
            packet.relativePath = transfer->relativePath;
            packet.fileHash     = transfer->fileHash;
            packet.chunkIndex   = transfer->currentChunkIndex;
            packet.totalChunks  = transfer->totalChunks;
            packet.data.assign(
                transfer->content.begin() + chunkOffset,
                transfer->content.begin() + chunkOffset + chunkSize
            );

            SendPacketToPlayer(session->playerid, PacketType::FileData, packet);

            ++transfer->currentChunkIndex;
            ++sent_this_tick;
        }
    }
}

void CefPlugin::ScheduleFileTransferTick()
{
	/*transfer_timer_.expires_after(std::chrono::milliseconds(10));
    transfer_timer_.async_wait(
        asio::bind_executor(strand_,
            [this](const std::error_code& ec) {
                if (!running_) return;
                if (!ec) {
                    ProcessFileTransfers();
                    // Réarmer depuis le même strand
                    ScheduleFileTransferTick();
                }
            }
        )
    );*/

	/*transfer_timer_.expires_after(std::chrono::milliseconds(10));
	transfer_timer_.async_wait([this](const std::error_code& ec) {
		if (!ec && running_) {
			ProcessFileTransfers();
			ScheduleFileTransferTick();
		}
	});*/
}

void CefPlugin::PauseDownload(int playerid, bool pause) 
{
	sessions_->SetDownloadPaused(playerid, pause);
	LOG_DEBUG("Player %d download has been %s.", playerid, pause ? "paused" : "resumed");
}

void CefPlugin::SendRawPacketToEndpoint(const asio::ip::udp::endpoint& endpoint, PacketType type, const PacketPayload& payload)
{
	NetworkPacket packet{ type, payload };

	std::string raw_data;
	if (!SerializePacket(packet, raw_data))
		return;

	if (network_server_)
		network_server_->SendTo(endpoint, raw_data.data(), static_cast<int>(raw_data.size()));
}

void CefPlugin::SendPacketToPlayer(int playerid, PacketType type, const PacketPayload& payload)
{
    auto session = sessions_->GetSession(playerid);
    if (!session)
        return;

    NetworkPacket packet{ type, payload };

    std::string raw_data;
    if (!SerializePacket(packet, raw_data)) {
        LOG_ERROR("Failed to serialize packet (type %d) for player %d", (int)type, playerid);
        return;
    }

    std::vector<uint8_t> encrypted = EncryptPacket({ raw_data.begin(), raw_data.end() }, session->tx_key);
    if (encrypted.empty())
        return;

    std::lock_guard<std::mutex> lock(session->kcp_mutex);
    if (!session->kcp_instance)
        return;

    ikcp_send(session->kcp_instance, (const char*)encrypted.data(), (int)encrypted.size());

	// TODO: Not always flush immediately (for file transfer?)
    ikcp_flush(session->kcp_instance);
}

void CefPlugin::NotifyCefInitialize(std::shared_ptr<NetworkSession> session, bool ok)
{
	if (!session || !bridge_)
		return;

	if (session->cef_init_notified)
		return;

    session->cef_init_notified = true;
    session->cef_ok = ok;

    std::vector<Argument> args;
    args.emplace_back(session->playerid);
    args.emplace_back(ok);
    bridge_->CallPawnPublic("OnCefInitialize", args);
}

void CefPlugin::HandleClientEvent(int playerid, const ClientEmitEventPacket& payload)
{
	if (payload.name == CefEvent::Client::BrowserCreateResult)
	{
		if (payload.args.size() >= 3)
		{
			int browserId = payload.browserId;
			bool success = payload.args[0].boolValue;
			int code = payload.args[1].intValue;
			const std::string& reason = payload.args[2].stringValue;

			bridge_->CallOnBrowserCreated(playerid, browserId, success, code, reason);
		}

		return;
	}

    auto it = registered_events_.find(payload.name);
    if (it == registered_events_.end())
        return;

    const auto& reg = it->second;
    const auto& signature = reg.signature;

    if (signature.size() != payload.args.size())
    {
        LOG_WARN("Argument count mismatch for event '%s' (callback '%s'). Expected %zu, got %zu.",
            payload.name.c_str(), reg.callback.c_str(), signature.size(), payload.args.size());
        return;
    }

    for (size_t i = 0; i < payload.args.size(); ++i)
    {
        const auto& arg = payload.args[i];
        const auto expected = signature[i];

        if (arg.type != expected)
        {
            LOG_WARN("Type mismatch for event '%s' (callback '%s') at arg %zu. Expected %d, got %d.",
                payload.name.c_str(), reg.callback.c_str(), i,
                static_cast<int>(expected), static_cast<int>(arg.type));
            return;
        }
    }

    std::vector<Argument> final_args;
    final_args.reserve(2 + payload.args.size());
    final_args.emplace_back(playerid);
    final_args.emplace_back(payload.browserId);
    final_args.insert(final_args.end(), payload.args.begin(), payload.args.end());

    bridge_->CallPawnPublic(reg.callback, final_args);
}

void CefPlugin::RegisterEvent(const std::string& name, const std::string& callback, const std::vector<ArgumentType>& signature)
{
	for (size_t i = 0; i < signature.size(); ++i)
	{
		const auto& type = signature[i];
		const char* typeName = "Unknown";

		switch (type)
		{
			case ArgumentType::String:
				typeName = "String";
				break;
			case ArgumentType::Integer:
				typeName = "Integer";
				break;
			case ArgumentType::Float:
				typeName = "Float";
				break;
			case ArgumentType::Bool:
				typeName = "Bool";
				break;
		}
	}

	RegisteredEvent event;
    event.callback = callback.empty() ? name : callback;
    event.signature = signature;

    registered_events_[name] = std::move(event);
}

CefPlugin::~CefPlugin()
{
	Shutdown();
}