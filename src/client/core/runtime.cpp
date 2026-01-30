#include "runtime.hpp"

#include <windows.h>

#include "app.hpp"
#include "browser/audio.hpp"
#include "browser/focus.hpp"
#include "browser/manager.hpp"
#include "hooks/hook_manager.hpp"
#include "hooks/render_hook.hpp"
#include "hooks/wndproc.hpp"
#include "network/network_manager.hpp"
#include "rendering/render_manager.hpp"
#include "samp/addresses.hpp"
#include "samp/samp.hpp"
#include "samp/samp_version_manager.hpp"
#include "samp/hooks/netgame.hpp"
#include "system/config_manager.hpp"
#include "system/gta.hpp"
#include "system/logger.hpp"
#include "system/resource_manager.hpp"
#include "ui/hud_manager.hpp"
#include "ui/download_dialog.hpp"

static constexpr const char* kConfigPath = "cef/config.json";
static constexpr const char* kClientLog = "cef/client.log";

std::unique_ptr<Runtime> Runtime::CreateDefault()
{
	auto runtime = std::unique_ptr<Runtime>(new Runtime());

	runtime->logger_ = std::make_unique<Logger>();
	logging::SetLogger(runtime->logger_.get());

	runtime->config_ = std::make_unique<ConfigManager>();
	runtime->gta_ = std::make_unique<Gta>();

	runtime->hud_ = std::make_unique<HudManager>();

	runtime->audio_ = std::make_unique<AudioManager>();

	return runtime;
}

bool Runtime::Start()
{
	(void)config_->Load(kConfigPath);

	logger_->SetLogFile(kClientLog);
	// logger_->SetDebugMode(config_->Get<bool>("debug", false));
	logger_->SetDebugMode(true);

	gta_->Initialize();
	hud_->Initialize();

	// Audio
	if (!audio_->Initialize())
	{
		MessageBoxA(nullptr, "Failed to initialize AudioManager (OpenAL).", "Fatal error", MB_ICONERROR);
		return false;
	}

	resources_ = std::make_unique<ResourceManager>(*gta_);
	network_ = std::make_unique<NetworkManager>(*resources_);
	resources_->SetNetworkManager(*network_);

	// Browser
	browser_ = std::make_unique<BrowserManager>(*audio_, *gta_, *resources_, *network_);
	focus_ = std::make_unique<FocusManager>(*browser_);
	browser_->SetFocusManager(focus_.get());
	browser_->Initialize();

	// Hooks
	hooks_ = std::make_unique<HookManager>();
	hooks_->Initialize();

	RenderManager::Instance().SetHookManager(hooks_.get());
	if (!RenderManager::Instance().Initialize())
	{
		MessageBoxA(nullptr, "Failed to initialize RenderManager (D3D9 hook).", "Fatal error", MB_ICONERROR);
		return false;
	}

	RenderManager::Instance().OnPresent = [this]()
	{
		if (app_)
			app_->Tick();
	};

	wndproc_ = std::make_unique<WndProcHook>(gta_->GetHwnd());
	if (!wndproc_->Initialize())
	{
		MessageBoxA(nullptr, "Failed to initialize WndProc hook.", "Fatal error", MB_ICONERROR);
		return false;
	}

	wndproc_->OnMessage = [this](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
		{
			if (browser_ && browser_->OnWndProcMessage(hwnd, msg, wParam, lParam))
				return TRUE;

			if (msg == WM_ACTIVATE)
			{
				const bool active = (LOWORD(wParam) != WA_INACTIVE);

				if (browser_)
					browser_->SetDrawEnabled(active);

				return 0;
			}

			return FALSE;
		};

	samp_version_ = std::make_unique<SampVersionManager>();
	samp_version_->Initialize();

	SampAddresses::Instance().Initialize(*samp_version_);

	samp_ = std::make_unique<Samp>(*hooks_);
	samp_->OnLoaded = [this]()
		{
			renderhook_ = std::make_unique<RenderHook>(*hooks_, *browser_);
			if (!renderhook_->Initialize())
			{
				LOG_FATAL("Failed to install RenderHook after SA-MP loaded.");
			}
		};

	samp_->OnExit = [this]()
		{
			if (renderhook_)
			{
				renderhook_->Shutdown();
				renderhook_.reset();
			}
		};

	if (!samp_->Initialize())
	{
		MessageBoxA(nullptr, "Failed to initialize SA-MP hooks.", "Fatal error", MB_ICONERROR);
		return false;
	}

	netgame_hook_ = std::make_unique<NetGameHook>(*hooks_, *resources_);
	netgame_hook_->Initialize();

	download_dialog_ = std::make_unique<DownloadDialog>(network_.get(), hud_.get(), samp_.get(), browser_.get());
	resources_->SetDownloadDialog(*download_dialog_);

	app_ = std::make_unique<App>(*network_, *browser_, *audio_, *focus_, *resources_);
	app_->Initialize();

	return true;
}

void Runtime::Stop()
{
	if (app_)
	{
		app_->Shutdown();
	}

	if (renderhook_)
	{
		renderhook_->Shutdown();
		renderhook_.reset();
	}

	if (wndproc_)
	{
		wndproc_->Shutdown();
		wndproc_.reset();
	}

	if (hooks_)
	{
		hooks_->Shutdown();
		hooks_.reset();
	}

	RenderManager::Instance().Shutdown();
	netgame_hook_->Shutdown();
}

Runtime::~Runtime()
{
	Stop();
}