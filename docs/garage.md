# Shared Garage interface

See [the reference catalog](garage_catalog.md) and
[executed validation and remaining limits](single_race_validation.md).
Garage typography, layout and modal corrections are documented in
[Garage UI reconstruction](garage_ui.md).

`game_src/unresolved/garage_session.hpp` defines the portable Garage interface.
`GarageSession` is a new coordinator, not a recovered original class name.
Original source/class ownership remains unresolved and address-linked evidence is
kept separately from the public code.

## Using the session

- Single Race calls `start_single(character, track, error)` to create the original
  $25,000 stock player with Street/Poor tires, NONE/NONE armor, no weapons or
  drivetrain upgrades, and the original body texture (`paint_job == 0`).
- A future Season caller passes its existing `GarageVehicleState` to `restore`.
  State contains cash, selected equipment, current/maximum health, ammunition and
  optional body/paint damage records. State snapshots contain values and no
  renderer or host handles. Invalid identities, equipment and snapshots fail
  without replacing the current state.
- `items(category)` enumerates original item/quality ordering and availability;
  `initial_selection(category)` locates the installed selection. Weapons use
  Front/Side/Rear as items and the weapon names as their products.
- `quote(selection, quote, error)` supplies action, adjusted price, trade-in,
  affordability, ammunition, description, advice and required confirmations.
  Browsing does not install equipment or change funds.
- Present every confirmation before `transact(selection, true, changed, error)`.
  Pass false while confirmation is pending. The session recalculates the quote
  before applying the transaction. Installed tires/paint produce the unavailable sound without changing state.
  Available repairs execute even when their rounded price is zero. A repair with
  fully healthy equipment can still restore visual damage; only restored health
  triggers the purchase sound. Insufficient funds return
  the original message and `changed == false`.
- `state()` supplies the committed result. `race_loadout()` exports selected
  equipment for existing race initialization; cash and damage are separate state.
  A Season integration must retain the complete state across races rather than
  recreate it using `start_single` or persist only `RaceLoadout`.
- Drain `take_sound_events()` once after transactions and route the relative
  original asset paths through the host's PCM/ADPCM playback adapter.

For imported visual damage, supply matching vehicle/paint resources and valid
`body_width`/`body_length`. Repair restores the appropriate health, native pixel
pairs, rectangle marks and body dents; repaint replays existing damage onto the
new livery. No original installation files are written by Garage.

## Ownership and presentation

`MenuFrontend` routes input and confirmations. `GarageSession` owns business
rules and values. Existing vehicle/equipment classes apply exported equipment to
the simulation. `VehicleDamageVisuals` owns damaged paint and mesh records.
`FrontendScene` renders the committed vehicle and the store product.

Changing item/category calls `transition_store_product`: DoorTop and DoorBottom
close toward DoorPoint, the product changes at the recovered threshold, and the
doors reopen. Quality changes use immediate `select_store_product` and the
selection sound. `drain_store_door_events()` reports the actual swap sound trigger.
Presentation runs on the existing fixed presentation timeline and retains elapsed
time; it never advances race physics.

Godot adapts input, RenderingServer resources and sample playback. No Garage
economy, animation state machine or equipment algorithm lives in GDScript.

## Validation

The dependency-free `garage_session` test checks transaction arithmetic, state
restoration, rejected operations and catalog boundaries. With the configured
installation as an optional argument it additionally checks every original
character, available track, item and quality, including actual transactions.

`menu_frontend` verifies navigation, purchase, cancelled sell-back and exported
race equipment. Scene and damage tests check original assets, animated doors,
N/A products, repair/repaint snapshots and teardown. The native race-menu smoke
visits all six stores, purchases real upgrades, verifies their live race effects,
restarts, returns to main, and measures Garage SFX separately from music.
