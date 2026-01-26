#include "app.hpp"

#include <windows.h>

#include "shared/events.hpp"
#include "shared/packet.hpp"

#include "browser/manager.hpp"
#include "browser/audio.hpp"
#include "browser/focus.hpp"
#include "system/resource_manager.hpp"
#include "network/network_manager.hpp"

#include "system/logger.hpp"
#include <samp/components/netgame.hpp>

void App::Initialize()
{
	LOG_INFO("Init omp-cef client app ...");

	if (!network_.Initialize("127.0.0.1", 7779))
	{
		MessageBoxA(nullptr, "Failed to initialize network manager.", "Fatal error", MB_ICONERROR);
		ExitProcess(0);
		return;
	}

	network_.SetPacketHandler([this](const NetworkPacket& p) {
		OnPacketReceived(p);
	});

	resources_.Initialize();
}

void App::Tick()
{
	if (network_.GetState() == ConnectionState::DISCONNECTED)
	{
		auto* netGame = GetComponent<NetGameComponent>();
		if (netGame) {
			const int localPlayerId = netGame->GetLocalPlayerId();
			if (localPlayerId >= 0) {
				network_.Connect(localPlayerId);
			}
		}
	}

	focus_.Update();
	browser_.RenderAll();
}

void App::OnPacketReceived(const NetworkPacket& packet)
{
	switch (packet.type)
	{
		case PacketType::ServerConfig:
		{
			const auto& cfg = std::get<ServerConfigPacket>(packet.payload);
			resources_.SetMasterKey(cfg.master_resource_key);
			resources_.MarkAsReadyToDownload();
			break;
		}
		case PacketType::FileData: {
			resources_.OnFileData(std::get<FileDataPacket>(packet.payload));
			break;
		}
		case PacketType::EmitEvent: {
			const auto& event = std::get<EmitEventPacket>(packet.payload);

			if (event.name == CefEvent::Server::CreateBrowser && event.args.size() >= 4) {
				int id = event.args[0].intValue;
				const std::string& url = event.args[1].stringValue;
				bool focused = event.args[2].boolValue;
				bool controls_chat = event.args[3].boolValue;

				browser_.CreateBrowser(id, url, focused, controls_chat, -1, -1);
			}
			else if (event.name == CefEvent::Server::CreateWorldBrowser && event.args.size() >= 4) {
				int id = event.args[0].intValue;
				const std::string& url = event.args[1].stringValue;
				const std::string& textureName = event.args[2].stringValue;
				float width = event.args[3].floatValue;
				float height = event.args[4].floatValue;

				browser_.CreateWorldBrowser(id, url, textureName, width, height);
			}
			else if (event.name == CefEvent::Server::DestroyBrowser && event.args.size() >= 1) {
				browser_.DestroyBrowser(event.args[0].intValue);
			}
			else if (event.name == CefEvent::Server::AttachBrowserToObject && event.args.size() >= 2)
			{
				int browserId = event.args[0].intValue;
				int objectId = event.args[1].intValue;

				browser_.AttachBrowserToObject(browserId, objectId);
			}
			break;
		}
		case PacketType::EmitBrowserEvent: {
			LOG_INFO("----------------------- EmitBrowserEvent");

			const auto& event = std::get<EmitEventPacket>(packet.payload);
			
			const int browserId = event.browserId;
            const std::string& eventName = event.name;

			auto* browser = browser_.GetBrowserInstance(browserId);
			if (browser && browser->browser) {
				CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("emit_event");
                CefRefPtr<CefListValue> list = msg->GetArgumentList();

				list->SetString(0, eventName);

                for (size_t i = 0; i < event.args.size(); ++i) {
                    const auto& arg = event.args[i];
                    const size_t list_index = i + 1;

                    switch (arg.type) {
						case ArgumentType::Integer:
							list->SetInt(list_index, arg.intValue);
							break;
						case ArgumentType::Float:
							list->SetDouble(list_index, arg.floatValue);
							break;
						case ArgumentType::Bool:
							list->SetBool(list_index, arg.boolValue);
							break;
						case ArgumentType::String:
							list->SetString(list_index, arg.stringValue);
							break;
						default:
							list->SetNull(list_index);
							break;
                    }
                }

                browser->browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, msg);
                LOG_DEBUG("[CEF] EmitEvent '%s' sent to browser %d with %zu args", eventName.c_str(), browserId, event.args.size());
			}
			else {
                LOG_WARN("[CEF] EmitEvent received but browser %d not found.", browserId);
            }

			break;
		}
		default:
			break;
	}
}

void App::Shutdown()
{
	LOG_INFO("Shutdown omp-cef client app ...");

	browser_.Shutdown();
	audio_.Shutdown();

	LOG_INFO("Good bye! See you soon.");
}