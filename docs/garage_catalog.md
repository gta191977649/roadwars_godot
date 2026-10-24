# Reference Garage catalog

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
The installed archive/configuration catalog contains 89 product/quality records.
Tables below give raw configuration prices, before character adjustment and
trade-in. The implementation reads the original resources rather than embedding
these tables. See [the shared interface](garage.md) for transaction semantics and
`docs/agent/evidence/garage_rules.md` for address-linked arithmetic evidence.

## Tires and drivetrain

| Item | Poor | Good | Best |
|---|---:|---:|---:|
| Street | 500 | 1500 | 3000 |
| Slicks | 500 | 1500 | 3000 |
| Off Road | 500 | 1500 | 3000 |
| Nitrous Boost | 7500 | 20000 | 30000 |
| Air Filter | 100 | 150 | 300 |
| Exhaust | 500 | 750 | 1500 |
| Intake Manifold | 500 | 1000 | 2000 |
| Camshaft | 1000 | 1500 | 3000 |
| Cylinder Heads | 2000 | 3000 | 5000 |
| Supercharger | 5000 | 7500 | 15000 |

Nitrous capacities are 6, 8 and 10. Tires cannot be sold back. Drivetrain
products can replace or sell the installed quality through the same quote API.

## Weapons

| Slot | Weapon | Raw price | Initial ammunition |
|---|---|---:|---:|
| Front | Machine Gun | 10000 | 100 |
| Front | 20mm Cannon | 10000 | 100 |
| Front | Battering Spikes | 15000 | 0 |
| Front | Plasma Blaster | 18000 | 100 |
| Front | Electron Cannon | 20000 | 100 |
| Front | Hydraulic Ram | 25000 | 100 |
| Front | Flame Thrower | 35000 | 100 |
| Front | Vulcanized Gun | 40000 | 100 |
| Side | Rocket Launcher | 5000 | 10 |
| Side | Swarmers | 15000 | 8 |
| Side | Back Missile | 20000 | 5 |
| Side | Guided Missile | 20000 | 5 |
| Side | Gatling Gun | 30000 | 80 |
| Side | The Excalibur | 50000 | 6 |
| Rear | Smoke Screen | 5000 | 5 |
| Rear | Missile Jammer | 15000 | 5 |
| Rear | Oil Slick | 10000 | 5 |
| Rear | Land Mines | 15000 | 5 |
| Rear | Grenades | 20000 | 10 |
| Rear | Reactive Mines | 20000 | 5 |
| Rear | Arachnoids | 30000 | 5 |

Only the configured slot accepts each weapon. Battering Spikes and Hydraulic
Ram use the additional character SPIKES price scalar. Selecting an already
installed weapon offers its sale rather than creating inventory.

## Armor, repair and paint

| Armor | Poor | Good | Best |
|---|---:|---:|---:|
| Steel | 2000 | 4000 | 5000 |
| Aluminum Alloy | 7000 | 8500 | 10000 |
| Reinforced Steel | 12000 | 15000 | 18000 |
| Reinforced Alloy | 19000 | 22000 | 27500 |

Armor resale includes its current condition. Installing or selling armor
rebuilds maximum health using the original vehicle rules.

Repair regions occur in this order: All Damage, Front, Right, Rear, Left.
Each offers Minor, Medium, Major and Complete, restoring 25%, 50%, 75% and 100%
of the remaining damage. All Damage includes internal damage. The 20 catalog
records have raw PRICE=10; the actual bill is calculated from restored HP and
the character REPAIR scalar, not that flat catalog value. Zero-price repairs
still execute, including visual-only restoration.

Paint Jobs 1 through 6 are free in the reference configuration. Repainting
preserves existing damage; selecting the current paint is unavailable.

Every one of these 89 reference records is available. Synthetic unavailable
catalog tests exercise the original N/A path without modifying installation
assets. Twelve characters across twelve tracks produce 12,816 catalog quotes
and independent transaction checks.
