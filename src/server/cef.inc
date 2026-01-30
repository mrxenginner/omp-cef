#if defined _INC_cef
	#endinput
#endif
#define _INC_cef

/**
 * Defines the possible status codes for a browser creation attempt.
 */
enum E_CEF_CREATE_STATUS
{
    SUCCESS,
    ERROR_GENERIC,
    ERROR_ID_ALREADY_IN_USE
};

/**
 * Defines the data types for arguments passed from Pawn to C++.
 */
enum E_CEF_ARGUMENT_TYPE 
{
    Argument_String,
    Argument_Integer,
    Argument_Float,
    Argument_Bool
};

/**
 * Helper macros to simplify passing typed arguments to natives.
 *
 * CEF_EmitEvent("eventName", CEF_STR("some_string"), CEF_INT(123), CEF_BOOL(true));
 */
#define CEF_STR(%0)     Argument_String, %0
#define CEF_INT(%0)     Argument_Integer, %0
#define CEF_FLOAT(%0)   Argument_Float, %0
#define CEF_BOOL(%0)    Argument_Bool, %0

/**
 * Defines the audio playback mode for a browser stream.
 */
enum E_CEF_AUDIO_MODE {
    /**
     * 3D World Audio: The sound is spatialized, positioned at the object's location,
     * and attenuates with distance. Ideal for in-game objects like TVs/boombox ...
     */
    AUDIO_MODE_WORLD,

    /**
     * 2D UI Audio: The sound is played directly to the player, non-spatialized,
     * like a user interface sound. Ideal for car radios (from inside), phone calls,
     * global server announcements ...
     */
    AUDIO_MODE_UI
};

/**
 * Defines the different components of the game's HUD that can be controlled.
 */
enum E_HUD_COMPONENT
{
    HUD_COMPONENT_ALL,
    HUD_COMPONENT_AMMO,
    HUD_COMPONENT_ARMOUR,
    HUD_COMPONENT_BREATH,
    HUD_COMPONENT_CROSSHAIR,
    HUD_COMPONENT_HEALTH,
    HUD_COMPONENT_MONEY,
    HUD_COMPONENT_RADAR,
    HUD_COMPONENT_WANTED_STARS,
    HUD_COMPONENT_WEAPON
};

 /**
 * Checks if a player has successfully completed the CEF "handshake".
 *
 * @param playerid      The ID of the player to check.
 *
 * @return              true if the player has the plugin and is ready, false otherwise.
*/
native bool:CEF_PlayerHasPlugin(playerid);

/**
 * Adds a resource (UI) to be downloaded by clients.
 * This must be called from OnGameModeInit or a similar early-stage callback.
 *
 * @param resourceName      The path to the directory (scriptfiles/cef/my_ui)
 */
native CEF_AddResource(const resourceName[]);

/**
 * Creates a 2D browser overlay for a specific player.
 *
 * @param playerid          The ID of the player who will see this browser.
 * @param browserid         A unique identifier for this browser.
 * @param url               The URL to load.
 * @param focused           If 'true', the browser will immediately gain input focus.
 * @param controls_chat     (Optional) If 'true' (default), the game's chat input will be
 * disabled when the player is typing in an HTML input field.
 * @param width             (Optional) The width of the browser viewport in pixels. Defaults to -1 for fullscreen width.
 * @param height            (Optional) The height of the browser viewport in pixels. Defaults to -1 for fullscreen height.
*/
native CEF_CreateBrowser(playerid, browserid, const url[], bool:focused, bool:controls_chat = true, Float:width = 0.0, Float:height = 0.0);

/**
 * Creates a 3D browser rendered in the world for a specific player.
 *
 * @param playerid          The ID of the player who will see this browser.
 * @param browserid         A unique identifier for this browser.
 * @param url               The URL to load.
 * @param textureName       The name of the texture that this browser will replace.
 * @param width             The width of the browser view in pixels.
 * @param height            The height of the browser view in pixels.
 */
native CEF_CreateWorldBrowser(playerid, browserid, const url[], const textureName[], Float:width, Float:height);

/**
 * Destroys a browser instance for a specific player.
 *
 * @param playerid          The ID of the player.
 * @param browserid         The ID of the browser to destroy.
 */
native CEF_DestroyBrowser(playerid, browserid);

/**
 * Registers an event that can be emitted from CEF (JavaScript) to Pawn.
 * This function defines the expected argument types for the event.
 *
 * @param eventName         The name of the event to register.
 * @param callback          The name of the event callback.
 * @param ...               A variadic list of argument types (e.g., Argument_String, Argument_Integer ...).
 */
native CEF_RegisterEvent(const eventName[], const callback[], E_CEF_ARGUMENT_TYPE:...);

/**
 * Emits an event from Pawn to a specific CEF browser instance (JavaScript).
 *
 * @param playerid          The ID of the player.
 * @param browserid         The ID of the browser to receive the event.
 * @param eventName         The name of the event to emit.
 * @param ...               A variadic list of arguments, using the CEF_* helper macros.
 */
native CEF_EmitEvent(playerid, browserid, const eventName[], {E_CEF_ARGUMENT_TYPE, Float, _}:...);

/**
 * Reloads the current page of a browser for a specific player.
 *
 * This is useful for restarting a video or for refreshing a web page's content.
 *
 * @param playerid      The ID of the player for whom the browser will be reloaded.
 * @param browserid     The ID of the browser to reload.
 * @param ignore_cache  If true, the page is reloaded bypassing the browser's cache. Defaults to false.
*/
native CEF_ReloadBrowser(playerid, browserid, bool:ignore_cache = false);

/**
 * Makes a browser focused for a specific player.
 *
 * @param playerid          The ID of the player.
 * @param id                The ID of the browser to focus or unfocus.
 * @param focused           Set to 'true' to give focus, 'false' to remove it.
 */
native CEF_FocusBrowser(playerid, id, bool:focused);

/**
 * Attaches a world browser to a specific object for a specific player.
 *
 * @param playerid          The ID of the player.
 * @param browserid         The ID of the world browser to attach.
 * @param objectid          The ID of the object to attach to.
 */
native CEF_AttachBrowserToObject(playerid, browserid, objectid);

/**
 * Detaches a world browser from an object for a specific player.
 *
 * @param playerid          The ID of the player.
 * @param browserid         The ID of the world browser to detach.
 * @param objectid          The ID of the object to detach from.
 */
native CEF_DetachBrowserFromObject(playerid, browserid, objectid);

/**
 * NATIVES (Audio)
 */

/**
 * Mutes or unmutes a specific browser's audio stream for a specific player.
 *
 * This function allows you to control the audio state of a browser for an specific player.
 * It does not affect what other players hear, making it ideal for personal settings or
 * player-specific interactions.
 *
 * @param playerid      The ID of the player for whom the audio will be muted or unmuted.
 * @param browserid     The ID of the browser to affect.
 * @param mute          Set to 'true' to mute the audio, 'false' to unmute.
*/
native CEF_SetBrowserMuted(playerid, browserid, bool:mute);

/**
 * Sets the audio playback mode for a specific browser stream for a specific player.
 *
 * This function allows you to dynamically switch a browser's audio between 3D spatialized
 * sound (AUDIO_MODE_WORLD) and 2D non-spatialized sound (AUDIO_MODE_UI).
 *
 * @param playerid      The ID of the player for whom the audio mode will be changed.
 * @param browserid     The ID of the browser to affect.
 * @param mode          The audio mode to set, using the E_CEF_AUDIO_MODE enum.
 *
 * @return              This native does not return any specific value.
*/
native CEF_SetBrowserAudioMode(playerid, browserid, E_CEF_AUDIO_MODE:mode);

/**
 * Adjusts the 3D audio settings for a specific browser stream.
 *
 * This function allows to define how a sound's volume attenuates over distance,
 * giving precise control over the audio experience for each world browser.
 *
 * @param playerid     			The ID of the player for whom the audio settings will be changed.
 * @param browserid             The ID of the browser to affect.
 * @param max_distance          The maximum distance (in meters) at which the sound can be heard.
 *                              Beyond this distance, the sound will be completely silent.
 * @param reference_distance    The distance (in meters) at which the sound is heard at its full volume (100%).
 *                              The volume will start to decrease beyond this point.
*/
native CEF_SetBrowserAudioSettings(playerid, browserid, Float:max_distance, Float:reference_distance);

/**
 * NATIVES (Utils)
 */

/**
 * Toggles the visibility of a specific component of the game's default HUD.
 *
 * @param playerid          The ID of the player.
 * @param componentid       The HUD component to toggle, from the E_HUD_COMPONENT enum.
 * @param toggle            Set to 'true' to show the component, 'false' to hide it.
 */
native CEF_ToggleHudComponent(playerid, E_HUD_COMPONENT:componentid, bool:toggle);

/**
 * Toggles the visibility of the default SA-MP class selection screen.
 *
 * This utility function allows you to hide the entire class selection interface,
 * including the skin preview, arrows, and the "Spawn" button. This is crucial
 * for creating a fully custom login or character selection UI with CEF for example.
 *
 * @param playerid      The ID of the player for whom the screen will be toggled.
 * @param toggle        Set to 'true' to show the screen, 'false' to hide it.
*/
native CEF_ToggleSpawnScreen(playerid, bool:toggle);

/**
 * CALLBACKS
 */

/**
 * Called when a player's CEF client has successfully initialized
 * and established a secure connection with the server plugin.
 *
 * This is the earliest and most reliable point to start sending CEF commands
 * to a player.
 *
 * @param playerid      The ID of the player whose client is ready.
 * @param success       Always true. (Reserved for future use).
*/
forward OnCefInitialize(playerid, bool:success);

/**
 * Called when a player's CEF client is fully ready (ressources downloaded ... etc)
 *
 * Useful when you want to start emit events (like for showing a login screen ...)
 *
 * @param playerid      The ID of the player.
*/
forward OnCefReady(playerid);

/**
 * Called when a browser creation request is processed by the client.
 *
 * This callback is the unified response for any browser creation attempt.
 * It informs the scripter about the success or failure of the operation.
 *
 * @param playerid      The ID of the player for whom the browser was created.
 * @param browserid     The ID of the browser that was requested.
 * @param success       'true' if the browser was created successfully, 'false' otherwise.
 * @param code          A status code from the E_CEF_CREATE_STATUS enum.
 * @param reason        A string explaining the result (e.g., "Successfully created" or "ID already in use").
*/
forward OnCefBrowserCreated(playerid, browserid, bool:success, E_CEF_CREATE_STATUS:code, const reason[]);

/**
 * Called when a player clicks on a world entity (player, vehicle, object) that
 * has a browser attached to it.
 *
 * @param playerid      The ID of the player who clicked.
 * @param entity_type   The type of the entity clicked (TODO: define enum).
 * @param network_id    The network ID of the entity.
 */
forward OnCefEntityClicked(playerid, entity_type, network_id);