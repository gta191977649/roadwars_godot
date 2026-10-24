extends SceneTree

var host
var failed := false

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        failed = true
        push_error(message)
        quit(1)
    return value

func step(command: String = "", elapsed: float = 0.31) -> bool:
    if not require(host.frontend_test_step(0.0, ""), "Menu input release failed"):
        return false
    if not require(host.frontend_test_step(elapsed, command), "Menu input failed: " + command):
        return false
    await process_frame
    return true

func capture(name: String, settle: bool = true) -> void:
    if DisplayServer.get_name() == "headless":
        return
    if settle and host.get_frontend_screen() != "loading":
        host.frontend_test_step(0.31, "")
    await RenderingServer.frame_post_draw
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--capture-dir="):
            require(root.get_texture().get_image().save_png(
                argument.trim_prefix("--capture-dir=").path_join(name + ".png")) == OK,
                "Menu capture failed")

func wait_loading(target: String, label: String) -> bool:
    var previous := -1.0
    var changes := 0
    var completed := false
    var middle_capture := false
    var audible := false
    for frame in range(240):
        var state: Dictionary = host.get_competition_state()
        if host.get_frontend_screen() != "loading":
            if not require(completed and changes >= 5 and audible,
                           "Loading did not present intermediate progress with audible character music"):
                return false
            return require(host.get_frontend_screen() == target or state.get("phase", "") == target,
                           "Loading completed at the wrong destination")
        var progress: float = state.get("loading_progress", -1.0)
        if not require(progress >= previous and progress >= 0.0 and progress <= 1.0,
                       "Loading progress moved backwards"):
            return false
        if progress > previous:
            previous = progress
            changes += 1
        if target == "countdown" and progress > 0.0 and not require(state.get("loading_intro_complete", false),
                                         "Resource loading began before character music faded in"):
            return false
        completed = completed or progress == 1.0
        if not require(str(state.get("frontend_music_path", "")).ends_with("/ThemeMusic.wav") and
                       state.get("frontend_music_playing", false), "Character loading music missing"):
            return false
        audible = audible or (progress < 1.0 and state.get("menu_music_gain", 0.0) > 0.0 and
                              state.get("menu_music_frames", 0) > 0)
        if progress >= 0.4 and not middle_capture:
            await capture(label + "_middle")
            middle_capture = true
        if not require(host.frontend_test_step(0.05, ""), "Staged resource preparation failed"):
            return false
        await process_frame
    return require(false, "Loading did not complete")

func key(code: Key) -> void:
    var event = InputEventKey.new()
    event.keycode = code
    event.physical_keycode = code
    event.pressed = true
    root.push_input(event)
    event = InputEventKey.new()
    event.keycode = code
    event.physical_keycode = code
    event.pressed = false
    root.push_input(event)
    for frame in range(3):
        await process_frame

func return_to_menu() -> bool:
    host.set_process(true)
    await key(KEY_ESCAPE)
    if not require(host.get_competition_state().get("pause_menu", false), "Race pause missing"):
        return false
    for row in range(2):
        await key(KEY_DOWN)
    # Live playback is unavailable, so the original No Replay menu omits its row.
    await key(KEY_ENTER)
    host.set_process(false)
    if not require(host.get_frontend_screen() == "main_menu", "Pause exit did not return to menu"):
        return false
    await create_timer(0.05).timeout
    return require(host.get_competition_state().get("menu_music_playing", false),
                   "Menu music did not resume after race")

func run_test() -> void:
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    # Start at exactly zero gain; large navigation steps used to hide ignored updates.
    if not require(host.preview_frontend_screen("loading", 0.0, "empty"),
                   "Loading audio fixture failed"):
        return
    if not require(host.get_competition_state().get("menu_music_gain", -1.0) == 0.0,
                   "Loading theme did not start at zero gain"):
        return
    if not require(host.frontend_test_step(0.15, ""), "Loading fade step failed"):
        return
    var half_gain: float = host.get_competition_state().get("menu_music_gain", 0.0)
    if not require(half_gain > 0.0, "Loading theme stayed silent after its gain update"):
        return
    if not require(host.frontend_test_step(0.15, "") and
                   host.get_competition_state().get("menu_music_gain", 0.0) > half_gain,
                   "Loading theme fade did not reach full gain"):
        return
    # Isolate the theme so menu SFX cannot satisfy the native mixer assertion.
    var bus := AudioServer.bus_count
    AudioServer.add_bus()
    AudioServer.set_bus_name(bus, "LoadingThemeProbe")
    host.get_node("OriginalFrontendMusic").bus = "LoadingThemeProbe"
    var peak := -200.0
    var deadline := Time.get_ticks_msec() + 1000
    while Time.get_ticks_msec() < deadline:
        await process_frame
        peak = maxf(peak, AudioServer.get_bus_peak_volume_left_db(bus, 0))
    if DisplayServer.get_name() != "headless" and not require(peak > -70.0,
                       "Loading theme produced no audible native mixer signal"):
        return
    host.get_node("OriginalFrontendMusic").bus = "Master"
    AudioServer.remove_bus(bus)
    print("ROADWARS_LOADING_THEME_MIXER_PEAK_DB: ", peak)
    host.restart_smoke_scene()
    host.set_process(false)
    for page in range(6):
        if host.get_frontend_screen() == "main_menu":
            break
        if not require(host.frontend_test_step(0, "") and host.frontend_test_step(0.31, "accept"),
                       "Frontend startup input failed"):
            return
    await create_timer(0.1).timeout
    var music: Dictionary = host.get_competition_state()
    if not require(host.get_frontend_screen() == "main_menu" and
                   music.get("menu_music_playing", false) and
                   music.get("menu_music_frames", 0) > 0 and
                   music.get("menu_music_gain", 0.0) > 0,
                   "Original main menu BGM missing"):
        return
    var position: float = music.get("menu_music_position", 0.0)
    await create_timer(0.15).timeout
    if not require(host.get_competition_state().get("menu_music_position", 0.0) > position,
                   "Menu BGM playback position did not advance"):
        return
    await capture("menu_music")
    # Missing business operations use one modal assertion without changing routes.
    if not await step("right") or not await step("right") or not await step("accept"):
        return
    if not require(host.get_frontend_screen() == "main_menu" and
                   host.get_competition_state().get("frontend_assertion", "") == "SEASON RACE",
                   "Unimplemented frontend did not show its assertion"):
        return
    await capture("not_implemented")
    if not await step("back") or not require(
        host.get_competition_state().get("frontend_assertion", "") == "",
        "Assertion dismissal did not preserve current menu"):
        return
    # Return selection to Quick Race before testing the two live launch routes.
    if not await step("left") or not await step("left"):
        return
    # Quick must also work on the first launch, before Garage has warmed the theme.
    for single in [false, true]:
        if single and not await step("right"):
            return
        if not await step("accept") or not require(host.get_frontend_screen() == "select_character",
                                                  "Race character page missing"):
            return
        if not await step("back") or not require(host.get_frontend_screen() == "main_menu",
                                                 "Character back route incorrect"):
            return
        if single and not await step("right"):
            return
        if not await step("accept") or not await step("accept"):
            return
        if not require(host.get_frontend_screen() == "select_track", "Track page missing"):
            return
        if not await step("down") or not await step("accept"):
            return
        if single:
            if not require(host.get_frontend_screen() == "start_race_summary", "Single skipped Summary"):
                return
            await capture("single_summary")
            if not await step("accept") or not require(host.get_frontend_screen() == "loading",
                                                       "Garage loading route missing"):
                return
            if not await wait_loading("modify_vehicle", "garage_loading"):
                return
            await capture("single_garage")
            var theme_path: String = host.get_competition_state().get("frontend_music_path", "")
            var theme_position: float = host.get_competition_state().get("menu_music_position", 0.0)
            var garage_music: Dictionary = host.get_competition_state()
            if not require(theme_path.ends_with("/ThemeMusic.wav") and
                           garage_music.get("frontend_music_playing", false) and
                           garage_music.get("menu_music_gain", 0.0) > 0.0,
                           "Garage character music is missing or silent"):
                return
            var music_deadline := Time.get_ticks_msec() + 1000
            while Time.get_ticks_msec() < music_deadline and host.get_competition_state().get("menu_music_position", 0.0) <= theme_position:
                await process_frame
            if not require(host.get_competition_state().get("menu_music_position", 0.0) > theme_position,
                           "Garage character music playback did not advance"):
                return
            if not await step("down") or not await step("down") or not await step("accept"):
                return
            if not require(host.get_frontend_screen() == "modify_vehicle" and
                           host.get_competition_state().get("frontend_music_path", "") == theme_path and
                           host.get_competition_state().get("menu_music_position", 0.0) > theme_position and
                           host.get_competition_state().get("frontend_music_playing", false) and
                           host.get_competition_state().get("menu_music_gain", 0.0) > 0.0,
                           "Weapon store restarted or lost character music"):
                return
            await capture("weapon_store_theme")
            var before_purchase: Dictionary = host.get_competition_state()
            if not require(before_purchase.get("garage_cash", 0) == 25000 and
                           before_purchase.get("garage_weapons", []) == ["", "", ""],
                           "Fresh garage must have actual stock vehicle and funds"):
                return
            # The original Front/Machine Gun selection has item, weapon and action rows.
            if not await step("down") or not await step("down") or not await step("accept"):
                return
            for confirmation in range(3):
                if not host.get_competition_state().get("garage_confirmation", false):
                    break
                if not await step("left") or not await step("accept"):
                    return
            var purchased: Dictionary = host.get_competition_state()
            if not require(purchased.get("garage_cash", 0) == 15000 and
                           str(purchased.get("garage_weapons", [""])[0]).to_upper() == "MACHINE GUN" and
                           purchased.get("frontend_sfx_started", 0) > before_purchase.get("frontend_sfx_started", 0),
                           "Garage purchase did not debit money, install weapon and play sound"):
                return
            # Observe actual SFX mixing on a separate bus, excluding character music.
            var sfx_bus := AudioServer.bus_count
            AudioServer.add_bus()
            AudioServer.set_bus_name(sfx_bus, "GarageSfxProbe")
            for child in host.get_children():
                if child is AudioStreamPlayer and child.name != "OriginalFrontendMusic":
                    child.bus = "GarageSfxProbe"
            var sfx_peak := -200.0
            var sfx_deadline := Time.get_ticks_msec() + 300
            while Time.get_ticks_msec() < sfx_deadline:
                await process_frame
                sfx_peak = maxf(sfx_peak, AudioServer.get_bus_peak_volume_left_db(sfx_bus, 0))
            if DisplayServer.get_name() != "headless" and not require(sfx_peak > -70.0,
                                   "Garage buy SFX produced no native mixer signal"):
                return
            for child in host.get_children():
                if child is AudioStreamPlayer and child.bus == "GarageSfxProbe":
                    child.bus = "Master"
            AudioServer.remove_bus(sfx_bus)
            print("ROADWARS_GARAGE_SFX_PEAK_DB: ", sfx_peak)
            await capture("weapon_purchased")
            # Selecting the installed weapon offers sell-back; cancelling preserves it.
            if not await step("accept") or not require(host.get_competition_state().get("garage_confirmation", false),
                                                       "Installed weapon did not offer sell-back confirmation"):
                return
            await capture("weapon_sell_confirmation")
            # The native modal defaults to No; Escape does not dismiss it.
            if not await step("accept") or not require(
                not host.get_competition_state().get("garage_confirmation", true) and
                host.get_competition_state().get("garage_cash", 0) == 15000,
                                                     "Cancelled sale changed garage money"):
                return
            if not await step("up") or not await step("up") or not await step("right", 0.1):
                return
            await capture("weapon_board_closing", false)
            if not await step("", 1.0):
                return
            var door_voice = null
            for child in host.get_children():
                if child is AudioStreamPlayer and str(child.name).begins_with("OriginalGarageDoor_") and child.playing:
                    door_voice = child
            if not require(door_voice is AudioStreamPlayer and door_voice.playing,
                           "Door threshold did not start the original stereo ADPCM voice"):
                return
            var door_bus := AudioServer.bus_count
            AudioServer.add_bus()
            AudioServer.set_bus_name(door_bus, "GarageDoorProbe")
            door_voice.bus = "GarageDoorProbe"
            var door_peak := -200.0
            var door_deadline := Time.get_ticks_msec() + 300
            while Time.get_ticks_msec() < door_deadline:
                await process_frame
                door_peak = maxf(door_peak, AudioServer.get_bus_peak_volume_left_db(door_bus, 0))
            if DisplayServer.get_name() != "headless" and not require(door_peak > -70.0,
                                    "Door ADPCM voice produced no native mixer signal"):
                return
            door_voice.bus = "Master"
            AudioServer.remove_bus(door_bus)
            print("ROADWARS_GARAGE_DOOR_PEAK_DB: ", door_peak)
            await capture("weapon_board_open")
            if not await step("back"):
                return
            # Tires: stock Street/Poor trade-in is $250; Best costs $3000.
            if not await step("accept") or not await step("down") or not await step("right") or not await step("right") or not await step("down") or not await step("accept"):
                return
            if not require(host.get_competition_state().get("garage_cash", 0) == 12250 and
                           str(host.get_competition_state().get("garage_tire_quality", "")).to_upper() == "BEST",
                           "Tire upgrade did not use original trade-in and selected quality"):
                return
            if not await step("back") or not await step("down") or not await step("accept") or not await step("right") or not await step("", 1.0) or not await step("down") or not await step("down") or not await step("accept"):
                return
            if not require(host.get_competition_state().get("garage_cash", 0) == 12150,
                           "Air Filter purchase did not use original component price"):
                return
            if not await step("back"):
                return
            for row in range(3):
                if not await step("down"):
                    return
            if not await step("accept") or not await step("down") or not await step("down") or not await step("accept"):
                return
            if not require(host.get_competition_state().get("garage_cash", 0) == 10150 and
                           str(host.get_competition_state().get("garage_armor", "")).to_upper() == "STEEL",
                           "Armor purchase did not commit the real vehicle configuration"):
                return
            if not await step("back"):
                return
            for row in range(4):
                if not await step("down"):
                    return
            if not await step("accept") or not await step("down") or not await step("down") or not await step("accept"):
                return
            if not require(host.get_competition_state().get("garage_cash", 0) == 10150,
                           "Healthy repair should not deduct cash"):
                return
            await capture("garage_repair")
            if not await step("back"):
                return
            for row in range(5):
                if not await step("down"):
                    return
            if not await step("accept") or not await step("right") or not await step("", 1.0) or not await step("down") or not await step("accept"):
                return
            if not require(host.get_competition_state().get("garage_paint", 0) == 2 and
                           host.get_competition_state().get("garage_cash", 0) == 10150,
                           "Paint store failed to commit original free livery selection"):
                return
            if not await step("back"):
                return
            await capture("garage_modified_vehicle")
            if not await step("back") or not require(host.get_frontend_screen() == "start_race_summary",
                                                     "Garage back route incorrect"):
                return
            if not await step("right") or not await step("accept"):
                return
        if not require(host.get_frontend_screen() == "loading", "Race loading page skipped"):
            return
        if not require(not host.get_competition_state().get("menu_music_playing", false),
                       "Menu BGM continued into loading"):
            return
        await capture("single_loading" if single else "quick_loading")
        if not await wait_loading("countdown", "single_loading" if single else "quick_loading"):
            return
        if DisplayServer.get_name() != "headless":
            await RenderingServer.frame_post_draw
            var black_frame := root.get_texture().get_image()
            for y in range(0, black_frame.get_height(), 32):
                for x in range(0, black_frame.get_width(), 32):
                    var pixel := black_frame.get_pixel(x, y)
                    if not require(maxf(pixel.r, maxf(pixel.g, pixel.b)) < 0.01,
                                   "Loading-to-race first frame was not black at %s: %s" % [Vector2i(x, y), pixel]):
                        return
        if not require(host.competition_test_step(0.5, false), "Race fade-in step failed"):
            return
        var state: Dictionary = host.get_competition_state()
        if not require(state.get("phase", "") == "countdown" and
                       str(state.get("track", "")).to_upper() == "DARK FOREST" and
                       state.get("laps", 0) == 2 and state.get("participants", 0) == 6,
                       "Menu race did not preserve selected track/laps/grid"):
            return
        var loadouts: Array = state.get("loadouts", [])
        if not require(loadouts.size() == 6, "Menu race loadouts missing"):
            return
        for racer in range(6):
            if single and racer == 5:
                if not require(loadouts[racer].paint == 2 and
                               str(loadouts[racer].weapons[0]).to_upper() == "MACHINE GUN" and
                               loadouts[racer].weapons[1] == "" and loadouts[racer].weapons[2] == "",
                               "Single discarded the actual Garage loadout"):
                    return
                if not require(str(loadouts[racer].tire_quality).to_upper() == "BEST" and
                               str(loadouts[racer].armor).to_upper() == "STEEL" and
                               loadouts[racer].engine_upgrades[0] == 2 and
                               loadouts[racer].maximum_armor_hp == [125.0, 125.0, 125.0, 125.0],
                               "Garage tires, armor and drivetrain did not reach real race effects"):
                    return
            elif not require(loadouts[racer].paint >= 1 and loadouts[racer].paint <= 6 and
                             not "" in loadouts[racer].weapons, "Random race loadout missing"):
                return
        await capture("single_grid" if single else "quick_grid")
        if single:
            host.set_process(true)
            await key(KEY_ESCAPE)
            await key(KEY_DOWN)
            await key(KEY_ENTER)
            host.set_process(false)
            var restarted: Array = host.get_competition_state().get("loadouts", [])
            if not require(restarted == loadouts and not host.get_competition_state().get("paused", false),
                           "Restart discarded modified equipment or left the race paused"):
                return
        if not await return_to_menu():
            return
    host.queue_free()
    await process_frame
    await create_timer(0.1).timeout
    if not failed:
        print("ROADWARS_RACE_MENU_SMOKE_OK: Single/Quick, loading, stock/random loadouts, Garage/back, pause exit and BGM")
        quit(0)
