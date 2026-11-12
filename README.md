# <img width="24" height="24" alt="unbound-64" src="https://github.com/user-attachments/assets/7b3fb809-6db7-41f7-9e32-8c1aec3c73eb" /> Unbound - Undocking panels in DaVinci Resolve

Unbound lets you undock and rearrange DaVinci Resolve panels and viewers.

<img width="1295" height="1391" alt="Unbound - Color page - Color controls 04" src="https://github.com/user-attachments/assets/9c223b85-5b6c-48d7-935b-b9c60844b517" />

## How to use

Unbound supports a range of panels and viewers on the Edit, Color and Fairlight pages (more to come).
You can arrange them across your displays, resize them, snap them alongside one another.

- An Unbound button in each supported panel header lets you undock/dock.

<img width="97" height="51" alt="Unbound - Button" src="https://github.com/user-attachments/assets/b275e6b3-60ac-4c85-81d0-7ead4d02579e" /> <img width="32" height="32" alt="unbound-64" src="https://github.com/user-attachments/assets/1be1d772-15ea-407d-9ace-058a1f17763e" />

- Move undocked panels by dragging their header bars or the Unbound button.

- Snapping is fine-tuned to feel helpful and still overridable. Hold Alt / Option to bypass magnetism altogether. Hold Shift to meet The Grid.

- Right-click on any of the Unbound buttons to manage layouts. Each Resolve page has its own set
of layouts. Hold a layout name for one second to rename it. Layouts are saved and restored
between sessions.

- In DaVinci Resolve's Keyboard Customization, search for Unbound to set keyboard shortcuts.

## Examples

<img width="3359" height="1391" alt="Unbound - Color page - Color controls 06" src="https://github.com/user-attachments/assets/2e883927-c87c-424d-bdef-bb3282ca276a" />

<img width="2480" height="1379" alt="Unbound - 9by16" src="https://github.com/user-attachments/assets/9ae5b1f9-e502-4c1b-8296-a63eb7048936" />

<img width="3288" height="1417" alt="Unbound - Edit page - Keyframes" src="https://github.com/user-attachments/assets/56679930-835b-41ec-ab50-7ca9b6779d80" />


## Design goals

- be borderline invisible
- be discoverable and self-explanatory
- keep it simple and don't add mental load

Ideally, Unbound would render itself obsolete because BM devs may agree with its design and ship
the feature natively. I think that would be a wonderful outcome.

## How does it work?

Unbound moves DaVinci Resolve's existing panels into floating windows, preserving their native
controls and behavior. It uses DaVinci Resolve's own UI framework to keep the integration
lightweight and minimally intrusive. Unbound adds a few conveniences on top: window snapping,
The Grid and saved layouts.

## Installation

Download from [Releases](../../releases) and run the Unbound installer.
Choose **Install** or **Update**, then open DaVinci Resolve.

Unbound starts automatically and adds the Unbound button to supported panels.

Run the installer again to uninstall. Uninstall always keeps your
settings and layouts.

Unbound supports Windows 10/11 (x64) and macOS. The universal Mac app includes Apple
Silicon and Intel code. No Linux port yet.

Tested with DaVinci Resolve versions from 20.3.1 through 21.2 (Free and Studio editions).
Unbound's integration has remained compatible across those versions.

## Is it safe to install Unbound?

Yes. _Says random stranger on the internet_.

Ask your agent to audit and check this repository and the related binaries.
This will give you a better and more reliable answer than the mechanisms
Microsoft and Apple are offering.

It is good to check, but do it the 2026 way.

Windows builds are unsigned; macOS builds are ad-hoc signed, without Apple notarization.

Operating systems still offer ways to decide for ourselves what to run, though.
For Unbound downloaded from this repository, use the per-app exception
when offered:

- **Windows:** in the SmartScreen warning, choose **More info**, then **Run anyway**.
  Availability depends on your system's security policy; see
  [Microsoft's SmartScreen guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation).
- **macOS:** try opening the app once, then go to **System Settings → Privacy &
  Security → Open Anyway** and confirm. See
  [Apple's instructions](https://support.apple.com/en-us/102445).

All that said: Unbound does not phone home, does not collect data and does not install
unrelated weird stuff. My name is on it. I stand behind it and I will take care of it.

## Discussion, articles, testimonials

- [Unbound thread on Blackmagic's user forum](https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=240898)
- [Digital Production](https://digitalproduction.com/) has an entertaining article on Unbound: [Unbound: Resolve’s Panels Get Their Own Place](https://digitalproduction.com/2026/11/09/unbound-davinci-resolve-floating-panels/) (_Bela Beier, Nov 9th 2026_)
- [TutKit.com](https://www.tutkit.com/en) has a kind note on Unbound: [Unbound turns panels in DaVinci Resolve into freely movable windows](https://www.tutkit.com/en/news/1021-unbound-resolve-floating-panels) (_Stefan Petri, Nov 10 2026_)

And I just love this testimonial:
> [Wow, this is not a thing I thought I wanted, but it's SO well executed and after using it a little I am really into it.](https://forum.blackmagicdesign.com/viewtopic.php?p=1240136#p1240136)
