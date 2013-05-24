# Admiral

A Window Manager.

*This is a horrible mess of terrible code.*

## Commands

* **M-Return** Run $TERMINAL or gnome-terminal.
* **M-c** Close the window under the pointer. Or crash for no reason.
* **M-q** Exit admiral immediately.
* **M-[number]** Switch to desktop [number].
* **M-S-[number]** Move window under pointer to desktop [number].
* **M-Mouse1** Start moving the window under the pointer.
* **M-Mouse2** Raise the window under the pointer.
* **M-S-Mouse2** Bury the window under the pointer.
* **M-Mouse3** Start resizing the window under the pointer.
* **M-f** Make the window under the pointer fullscreen.
* **M-m** Toggle decorations for the window under the pointer.

## Known Bugs

* Size increment hints are ignored.
* Most of ICCM is unimplemented.
* Most of EWHM is unimplemented.
* Pointing at root and using window-related keychords crashes Admiral.

