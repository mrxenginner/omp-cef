#pragma once

#include <memory>

class App;

class Logger;
class ConfigManager;

class Gta;
class HudManager;

class NetworkManager;
class BrowserManager;
class AudioManager;
class FocusManager;

class ResourceManager;
class DownloadDialog;

class HookManager;
class WndProcHook;
class RenderHook;

class SampVersionManager;
class SampAddresses;
class Samp;

class NetGameHook;

class Runtime
{
public:
	~Runtime();

	static std::unique_ptr<Runtime> CreateDefault();

	bool Start();
	void Stop();

private:
	Runtime() = default;
	Runtime(const Runtime&) = delete;
	Runtime& operator=(const Runtime&) = delete;
	Runtime(Runtime&&) = delete;
	Runtime& operator=(Runtime&&) = delete;

	std::unique_ptr<Logger> logger_;
	std::unique_ptr<ConfigManager> config_;

	std::unique_ptr<Gta> gta_;
	std::unique_ptr<HudManager> hud_;

	std::unique_ptr<AudioManager> audio_;
	std::unique_ptr<BrowserManager> browser_;
	std::unique_ptr<FocusManager> focus_;
	std::unique_ptr<NetworkManager> network_;

	std::unique_ptr<ResourceManager> resources_;
	std::unique_ptr<DownloadDialog> download_dialog_;

	std::unique_ptr<HookManager> hooks_;
	std::unique_ptr<WndProcHook> wndproc_;
	std::unique_ptr<RenderHook> renderhook_;

	std::unique_ptr<SampVersionManager> samp_version_;
	std::unique_ptr<Samp> samp_;

	std::unique_ptr<NetGameHook> netgame_hook_;

	std::unique_ptr<App> app_;
};