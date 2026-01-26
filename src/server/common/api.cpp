#include "api.hpp"
#include "plugin.hpp"
#include "shared/events.hpp"

CefApi* CefApi::instance_ = nullptr;

CefApi::CefApi(CefPlugin& plugin) : plugin_(plugin)
{
    instance_ = this;
}

CefApi::~CefApi()
{
    instance_ = nullptr;
}

bool CefApi::PlayerHasPlugin(int playerid)
{
	return plugin_.GetNetworkSessionManager().HasPlayerPlugin(playerid);
}

void CefApi::AddResource(const std::string& resourceName)
{
    plugin_.GetResourceManager().AddResource(
        resourceName,
        plugin_.GetMasterKey()
    );
}

// native CEF_CreateBrowser(playerid, browserid, const url[], bool:focused, bool:controls_chat = true, Float:width = 0.0, Float:height = 0.0);
void CefApi::CreateBrowser(int playerid, int browserid, const std::string& url, bool focused, bool controls_chat)
{
    EmitEventPacket event;

    event.name = CefEvent::Server::CreateBrowser;
    event.args.emplace_back(browserid);
    event.args.emplace_back(url);
    event.args.emplace_back(focused);
    event.args.emplace_back(controls_chat);

    plugin_.SendPacketToPlayer(playerid, PacketType::EmitEvent, event);
}

// native CEF_CreateWorldBrowser(playerid, browserid, const url[], const textureName[], Float:width, Float:height);
void CefApi::CreateWorldBrowser(int playerid, int browserid, const std::string& url, const std::string& textureName, float width, float height)
{
	EmitEventPacket event;

	event.name = CefEvent::Server::CreateWorldBrowser;
	event.args.emplace_back(browserid);
	event.args.emplace_back(std::string(url));
	event.args.emplace_back(std::string(textureName));
	event.args.emplace_back(width);
	event.args.emplace_back(height);

	plugin_.SendPacketToPlayer(playerid, PacketType::EmitEvent, event);
}

void CefApi::DestroyBrowser(int playerid, int browserid)
{
	EmitEventPacket event;

	event.name = CefEvent::Server::DestroyBrowser;
	event.args.emplace_back(browserid);

	plugin_.SendPacketToPlayer(playerid, PacketType::EmitEvent, event);
}

void CefApi::RegisterEvent(const std::string& name, const std::vector<ArgumentType>& signature) 
{

}

void CefApi::AttachBrowserToObject(int playerid, int browserid, int objectId)
{
	EmitEventPacket event;

	event.name = CefEvent::Server::AttachBrowserToObject;
	event.args.emplace_back(browserid);
	event.args.emplace_back(objectId);

	plugin_.SendPacketToPlayer(playerid, PacketType::EmitEvent, event);
}

void CefApi::DetachBrowserFromObject(int playerid, int browserid, int objectid)
{
	EmitEventPacket event;

	event.name = CefEvent::Server::DetachBrowserFromObject;
	event.args.emplace_back(browserid);
	event.args.emplace_back(objectid);

	plugin_.SendPacketToPlayer(playerid, PacketType::EmitEvent, event);
}