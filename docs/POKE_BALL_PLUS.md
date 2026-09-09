# Poke Ball Plus

Available in PokeMulti 0.23.1 on Windows 10/11 with Bluetooth LE. This is the ball-shaped **Poke Ball Plus with a control stick**, not Pokemon GO Plus or GO Plus +.

## Connect

1. Launch `run.bat` and start your game.
2. Open **Options > Controller** in the game sidebar.
3. Disconnect the ball from any phone, Switch or other controller utility currently using it. Press its top button to wake it.
4. Select **Find controller**. The scan lasts eight seconds. Each detected ball appears with an address suffix so you can choose the correct one.
5. Select the ball and **Connect**. Input is enabled only after valid reports arrive. Release the controls to play. Successful connection automatically returns control to the game unless you have since clicked elsewhere, started typing or switched windows.

**Windows setup** opens Windows Bluetooth settings if the PC's radio is off. This uses Windows' built-in BLE APIs directly; no ViGEm, Steam mapping, extra driver or background utility is required. The app does not silently choose a nearby ball. **Cancel** stops an attempt; **Disconnect** closes this client's connection. Find/connect again after the ball sleeps or goes out of range. Settings persist, but the app does not automatically scan or reconnect at launch.

## Controls

| Ball input | Game action |
| --- | --- |
| Control stick | Move / navigate |
| Stick click | A / confirm / talk |
| Top button | B / cancel / run where available |
| Press both buttons together | Start / original game menu |

Use the native Start menu for Camp and Save. Keyboard controls remain available, including Select and L/R. **Adjust controls** saves the stick dead zone (10-60%), A/B swap and Y inversion in that profile's `controller.cfg`. The default dead zone is 25%, with release hysteresis to prevent flickering near center. Individual buttons wait 90ms to recognize the Start combination; quick individual taps are preserved on release. Press both together promptly; the chord emits Start once and suppresses A/B until both are released.

The live stick dot and two button lights show received input even while the sidebar is focused. Editing the sidebar/chat or switching away from the window blocks game input. Select **Resume game** (or click the game picture / press Esc) and release the controls before moving again. The status distinguishes active controls, paused input and waiting for release. Neutral arming uses the same dead zone as movement; the smaller hysteresis threshold only governs releasing an existing direction. A missing report for 750ms releases accessory input; disconnect also releases it. A reconnect requires neutral controls before it can move or select anything. A ball can be reserved by only one PokeMulti window on the same Windows session..

Battery percentage appears only when the device returns a valid reading. A battery-service failure does not disable the controller. This implementation covers stick/buttons and battery. Lights, rumble, speaker audio and Pokemon stored in the accessory are not implemented or modified.

## Supplied model

The preview reads `Poke Ball Plus` (with the accented e in the actual folder name) beside the application or in its nearby project root. It loads `ob0008_00_gadgets.dae` and `ob0008_00_obj_col.png`, preserving the supplied closed-ball bind-pose geometry, normals and UVs, including the strap. Both materials in that export use the same diffuse atlas. Drag the preview to rotate; its camera and shading affect presentation only. No model or texture files are rewritten. The original reference model stays in the local project and is not added to the distribution allowlist; copy that supplied folder alongside the application when moving your local installation. This does not restore the removed 3D gameplay mode.

## Implementation and validation

A bounded `controller-status.txt` in the local profile records the latest phase, report age/count, window/game focus, typing state, neutral-arming state, stick/buttons and emitted GBA keys once a second while diagnosing a used controller. It contains no controller address or game/save data.

Discovery matches BLE advertisement name `Pokemon PBP`. The input service/characteristic UUIDs end in `23e5` / `23e6` within `6675e16c-f36d-4567-bb55-6b51e27a23e5`. Input uses the reported button bits and packed stick bytes. Only input/battery notification descriptors are enabled; there are no vendor commands to accessory storage. Windows API operations run on a cancellable worker, with generation-checked callbacks and weak ownership. Network protocol remains FRMP 17; controller packets and settings remain local.

Protocol references: [Poke Ball Plus controller implementation](https://github.com/rna0/pokeball-plus-4-windows/blob/main/PokeballPlus4Windows/PokeballController.cs), [original BLE investigation and captures](https://gist.github.com/Q-Bert-Reynolds/3ae3d151bd17cf5ac7bfc689159b716a). Windows lifecycle reference: [Microsoft Bluetooth GATT client documentation](https://learn.microsoft.com/en-us/windows/apps/develop/devices-sensors/gatt-client). These were used to check the protocol; no third-party controller utility or source is bundled.

Software tests cover packet parsing, four directions, dead zone/hysteresis, short taps, held buttons, the Start chord, remapping, focus/reconnect release and native input merging. The native UI test feeds synthetic BLE reports into a private profile and verifies actual trainer movement, native Start-menu presentation, chat capture, stale-input release and disconnect. Screenshots were checked at 1140x760 and 940x650.

The user subsequently confirmed that the physical ball connects and its live stick dot responds, but had not clicked the game after connecting in 0.23.0. That version left input focused on the sidebar. Version 0.23.1 fixes that focus handoff and the neutral-zone mismatch. Native movement and focus regressions use synthetic input; a physical end-to-end gameplay check of the patch still requires restarting the game and reconnecting the ball. Battery readings and physical reconnection have not been verified by the automated tests. The UI reports discovery, connection and live input separately rather than presenting discovery alone as a working controller.
