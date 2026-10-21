# Valve Index controls

This candidate uses the Index's physical **A/B buttons on each hand**. The overlay's Xbox labels **X/Y** mean **left A/B**. These mappings assume the game's **Alternative** controller scheme. Changes to the game's bindings can change the resulting actions.

## On foot

| Index input | Action |
| --- | --- |
| Left stick | Move; hold fully forward briefly to sprint |
| Right stick left/right | Turn using the configured turn mode |
| Right stick fully up | Dash/dodge once per push |
| Right stick fully down | Toggle game crouch; return to centre before another push |
| Right A | Jump; confirm in menus |
| Right B | Release magazine with a weapon equipped; back when holstered |
| Left A | Interact/reload |
| Left B | Switch weapon |
| Left oval touchpad press | Open the equipment menu, then choose Inventory |
| Right trigger | Fire |
| Left trigger | Aim/block |
| Right grip near hip/back holster | Put away the held weapon, or draw when empty |
| Left grip at the magazine | Grab during physical reload |
| D-pad Up shortcut | Toggle scanner: hold left-stick click and tilt the right stick up |
| Left stick click + left grip, held briefly | Activate cyberware |
| Both stick clicks, held one second | Open/close the VR overlay |

To put a weapon away, bring the right hand to the hip or back zone and give the grip a **fresh squeeze**. Keeping the grip squeezed while moving does not generate another command. A held weapon can be put away without special holster clothing. Drawing in immersive mode still follows the visual holster outfit; simple mode uses back/right/left for weapon slots 1/2/3.

After inserting a magazine, move the magazine hand to the slide and pull it to rack the weapon. You can keep squeezing during this handoff; the hand must move at least 8 cm relative to the weapon and reach the slide's grab zone. Releasing and squeezing again at the slide also works. To put away an **unarmed grenade**, open the holding hand. Releasing a grenade whose pin has already been pulled still throws it.

## Scanner

Toggle the scanner with **D-pad Up**: hold the **left stick click**, tilt the **right stick up**, then release the direction. Repeat to close it. This uses the existing D-pad shortcut; right-stick up without the modifier still performs dash/dodge. Scanner toggling consumes the VR D-pad Up action so it cannot also use a consumable.

**Scanner: D-pad Up toggle** is enabled by default. Both grips remain available for grabbing, physical reloads and belt interactions. The former dual-grip and ear gestures are inactive while this option is enabled. The optional dual-grip controls remain in CONTROLS for users who deliberately turn D-pad mode off.

| Index input while scanning | Action |
| --- | --- |
| Left stick up/down | Select quickhack; centre the stick between steps |
| Left A | Apply selected quickhack |
| Right trigger | Tag target |
| Right stick click | Change scanner tab |
| Left trigger + right stick | Scanner zoom |

Entering menus, losing valid controller input, or leaving an eligible gameplay context cancels the scanner toggle and requires releasing the direction before reuse. With the optional dual-grip mode enabled instead, active reloads and supported two-hand weapon grips retain priority and the scanner claims its grip chord until release.

## Menus and recentering

Press the **left oval touchpad** (beside the stick) to open the equipment menu, then choose **Inventory** to change equipped weapons. Release before another press. The shortcut sends a short menu press even if you keep the pad pressed; it does not invoke the game's hold-to-open-map action.

The native menu pointer follows the right controller's aim on the displayed menu plane. Release the trigger, point at an option, then squeeze and release to click. To drag an inventory item, **hold the right trigger**, point at its destination, then **release**. The pointer moves to the destination before releasing the mouse button. Leaving the menu plane, losing tracking or game focus, recentering, or opening the VR overlay releases the held button; release the trigger before trying again. Right A remains available for confirmation and hold-to-skip prompts. The pointer uses the game cursor and requires the game window to have focus. A held trigger is suppressed when returning to gameplay until released. Disable **Native menus: right-hand pointer** in CONTROLS to restore trigger-as-A confirmation; stick navigation remains available in either mode.

Stand upright and face forward before pressing **F7** to recenter. F7 calibrates the tracking origin; it does not toggle the game's crouch state. If a save loads crouched, return the right stick to centre and push it fully down once to stand. Physical crouching is measured relative to the recentered pose, independent of the player's absolute height.

## HUD and fingers

For a more central HUD, try **HUD FOV = 55 degrees** in the overlay, with distance left at **1.5 m**. The previous FOV was 65.4 degrees. A smaller FOV brings the edges inward and makes the text smaller; individual HUD element controls remain available for further adjustment.

Individual finger bending uses controller-derived OpenXR hand joints when the runtime supplies them. It applies to free hands; weapon, reload, ladder and other interaction poses retain their own constraints. Unsupported, inactive or stale tracking falls back to the existing animations. This is not optical hand tracking, and headset testing is still required to check the retargeted appearance. `CVR_XR_FINGER_TRACKING=0` in the game's environment disables the new path for comparison.
