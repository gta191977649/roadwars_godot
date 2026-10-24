extends SceneTree

# Rendering/input acceptance only. All Garage rules and layout remain in portable C++.
var host
var failed := false
var capture_dir := ""

func _initialize() -> void:
    call_deferred("run_test")

func require(value: bool, message: String) -> bool:
    if not value:
        failed = true
        push_error(message)
        quit(1)
    return value

func step(command: String = "", elapsed: float = 0.31) -> bool:
    if not require(host.frontend_test_step(0.0, "") and
                   host.frontend_test_step(elapsed, command), "Garage input failed: " + command):
        return false
    await process_frame
    return true

func capture(name: String) -> void:
    var state: Dictionary = host.get_competition_state()
    print("GARAGE_UI_STATE ", name, " row=", state.get("garage_selected", -1),
          " rows=", state.get("garage_rows", []), " cash=", state.get("garage_cash", 0),
          " confirmation=", state.get("garage_confirmation", false),
          " yes=", state.get("garage_confirmation_yes", false),
          " notice=", state.get("garage_notice", false))
    if DisplayServer.get_name() == "headless":
        return
    for size in [Vector2i(640, 480), Vector2i(960, 540)]:
        root.size = size
        if not require(host.frontend_test_step(0.0, ""), "Garage resize redraw failed"):
            return
        await process_frame
        await RenderingServer.frame_post_draw
        var suffix := "_%dx%d.png" % [size.x, size.y]
        if not require(root.get_texture().get_image().save_png(
            capture_dir.path_join("garage_ui_" + name + suffix)) == OK, "Garage capture failed"):
            return
    root.size = Vector2i(640, 480)
    host.frontend_test_step(0.0, "")

func select_row(index: int) -> bool:
    for attempt in range(12):
        var selected: int = host.get_competition_state().get("garage_selected", -1)
        if selected == index:
            return true
        if not await step("down" if selected < index else "up"):
            return false
    return require(false, "Garage row selection did not converge")

func open_store(index: int) -> bool:
    return await select_row(index) and await step("accept")

func close_store() -> bool:
    return await step("back") and require(host.get_frontend_screen() == "modify_vehicle",
                                         "Store back did not return to Garage")

func action_row() -> bool:
    return await select_row(host.get_competition_state().get("garage_rows", []).size() - 1)

func run_test() -> void:
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--capture-dir="):
            capture_dir = argument.trim_prefix("--capture-dir=")
    if not require(capture_dir != "", "Garage UI capture directory missing"):
        return
    host = ClassDB.instantiate("GameHost")
    root.add_child(host)
    host.set_process(false)
    var initial_rids = host.get_live_rids()
    for page in range(6):
        if host.get_frontend_screen() == "main_menu":
            break
        if not await step("accept"):
            return
    if not require(host.get_frontend_screen() == "main_menu", "Garage startup route failed"):
        return
    # Enter actual Single state, rather than relying on preview-only money/equipment.
    for command in ["right", "accept", "accept", "down", "accept", "accept"]:
        if not await step(command):
            return
    for frame in range(240):
        if host.get_frontend_screen() != "loading":
            break
        if not await step("", 0.05):
            return
    if not require(host.get_frontend_screen() == "modify_vehicle" and
                   host.get_competition_state().get("garage_cash", 0) == 25000,
                   "Garage did not receive a fresh Single vehicle"):
        return
    await capture("main")
    var stores := ["tires", "drive_train", "weapons", "armor", "repair", "paint"]
    for index in range(stores.size()):
        if not await select_row(index):
            return
        await capture("main_row" + str(index))
        if not await step("accept"):
            return
        var rows: Array = host.get_competition_state().get("garage_rows", [])
        if not require(rows.size() >= 2, "Garage store has no actionable rows"):
            return
        for row in range(rows.size()):
            if not await select_row(row):
                return
            await capture(stores[index] + "_row" + str(row))
        # Also exercise changed item/description and changed quality/weapon text.
        if not await select_row(0) or not await step("right") or not await step("", 1.0):
            return
        await capture(stores[index] + "_next_item")
        if rows.size() > 2:
            if not await select_row(1) or not await step("right"):
                return
            await capture(stores[index] + "_next_quality")
        if not await close_store():
            return
    # A real purchase followed by the installed-weapon sell-back confirmation.
    if not await open_store(2) or not await action_row() or not await step("accept"):
        return
    if not require(host.get_competition_state().get("garage_cash", 0) == 15000 and
                   str(host.get_competition_state().get("garage_weapons", [""])[0]).to_upper() == "MACHINE GUN",
                   "Garage UI purchase did not install Machine Gun"):
        return
    if not await step("accept") or not require(host.get_competition_state().get("garage_confirmation", false) and
                                               not host.get_competition_state().get("garage_confirmation_yes", true),
                                               "Sell-back confirmation missing"):
        return
    await capture("confirmation_no")
    if not await step("left"):
        return
    if not require(host.get_competition_state().get("garage_confirmation_yes", false),
                   "Left did not select confirmation Yes"):
        return
    await capture("confirmation_yes")
    # Explicit No acceptance declines. Native Escape leaves this modal pending.
    if not await step("right") or not await step("accept"):
        return
    if not require(not host.get_competition_state().get("garage_confirmation", false) and
                   host.get_competition_state().get("garage_cash", 0) == 15000,
                   "Declined sale changed funds or left a dialog open"):
        return
    if not await step("accept") or not await step("back"):
        return
    if not require(host.get_competition_state().get("garage_confirmation", false) and
                   host.get_competition_state().get("garage_cash", 0) == 15000,
                   "Escape dismissed the native confirmation or changed funds"):
        return
    if not await step("accept"):
        return
    # The last front weapon costs more than remaining funds. Native rejection
    # must preserve state and may not invent a popup absent in the original.
    if not await select_row(1) or not await step("left") or not await action_row():
        return
    var before: Dictionary = host.get_competition_state()
    if not await step("accept"):
        return
    var rejected: Dictionary = host.get_competition_state()
    if not require(rejected.get("garage_cash", 0) == before.get("garage_cash", 0) and
                   rejected.get("garage_weapons", []) == before.get("garage_weapons", []) and
                   not rejected.get("garage_confirmation", false) and rejected.get("garage_notice", false),
                   "Unaffordable purchase changed equipment or did not open native notice"):
        return
    await capture("insufficient_funds")
    if not await step("back") or not require(host.get_competition_state().get("garage_notice", false),
                                             "Escape dismissed native OK notice"):
        return
    if not await step("accept"):
        return
    if not require(not host.get_competition_state().get("garage_notice", false), "OK did not dismiss native notice"):
        return
    # Electron Cannon consumes the remaining $15000 after the installed weapon's
    # $5000 trade-in. Stock tires require the warning before the replacement prompt.
    if not await select_row(1):
        return
    for product in range(3):
        if not await step("left"):
            return
    if not await action_row() or not await step("accept") or not require(
        host.get_competition_state().get("garage_confirmation", false), "Tire warning missing"):
        return
    await capture("warning_tires_no")
    if not await step("left"):
        return
    await capture("warning_tires_yes")
    if not await step("accept") or not require(host.get_competition_state().get("garage_confirmation", false) and
                                               not host.get_competition_state().get("garage_confirmation_yes", true),
                                               "Replacement confirmation did not follow tire warning"):
        return
    await capture("warning_replace_no")
    if not await step("left"):
        return
    await capture("warning_replace_yes")
    if not await step("back") or not require(
        host.get_competition_state().get("garage_confirmation", false),
        "Escape dismissed the second native warning"):
        return
    if not await step("right") or not await step("accept") or not require(
        host.get_competition_state().get("garage_cash", 0) == 15000 and
        str(host.get_competition_state().get("garage_weapons", [""])[0]).to_upper() == "MACHINE GUN",
        "Cancelling the second warning changed the committed vehicle"):
        return
    if not await close_store():
        return
    await capture("main_after_purchase")
    if not await step("back") or not require(host.get_frontend_screen() == "start_race_summary",
                                             "Garage did not return to Summary"):
        return
    if not await step("accept"):
        return
    for frame in range(240):
        if host.get_frontend_screen() != "loading":
            break
        if not await step("", 0.05):
            return
    if not require(host.get_frontend_screen() == "modify_vehicle" and
                   host.get_competition_state().get("garage_cash", 0) == 15000,
                   "Garage reentry discarded committed funds"):
        return
    await capture("reentered")
    for cycle in range(2):
        host.restart_smoke_scene()
        host.set_process(false)
        await process_frame
        if not require(host.get_live_rids() == initial_rids, "Garage UI restart leaked renderer resources"):
            return
    host.queue_free()
    await process_frame
    await create_timer(0.1).timeout
    if not failed:
        print("ROADWARS_GARAGE_UI_SMOKE_OK: six stores, rows, products, modal Yes/No, ignored Escape, rejection, reentry and lifecycle; two capture resolutions")
        quit(0)
