# <img width="24" height="24" alt="unbound-64" src="https://github.com/user-attachments/assets/7b3fb809-6db7-41f7-9e32-8c1aec3c73eb" /> Unbound - Floating panels for DaVinci Resolve

Unbound lets you undock and rearrange DaVinci Resolve panels and viewers.

<img width="979" height="1062" alt="Unbound - Flock of Panels" src="https://github.com/user-attachments/assets/faf08d33-34e2-49ad-9782-d92643aebfba" />

It supports a range of panels and viewers on the Edit, Color and Fairlight pages (more to come).
You can arrange them across your displays, resize them, snap them alongside one another.

An Unbound button in each supported panel header (look for the button next to the ellipsis
button) lets you undock/dock.

<img width="97" height="51" alt="Unbound - Button" src="https://github.com/user-attachments/assets/b275e6b3-60ac-4c85-81d0-7ead4d02579e" /> <img width="32" height="32" alt="unbound-64" src="https://github.com/user-attachments/assets/1be1d772-15ea-407d-9ace-058a1f17763e" />


Right-click on any of the Unbound buttons to manage layouts. Each Resolve page has its own set
of layouts. Hold a layout name for one second to rename it. Layouts are saved and restored
between sessions.

<img width="1230" height="628" alt="Unbound - Fairlight Page" src="https://github.com/user-attachments/assets/12c82fde-ba4e-446b-af31-ae30a119e582" />

<img width="2480" height="1379" alt="Unbound - 9by16" src="https://github.com/user-attachments/assets/9ae5b1f9-e502-4c1b-8296-a63eb7048936" />

<img width="2560" height="1400" alt="Unbound - Footage Review" src="https://github.com/user-attachments/assets/cb0325e2-a2ba-4f71-acf7-23b27fde0dda" />

## Unbound design goals

- be borderline invisible
- be discoverable and self-explanatory
- keep it simple and don't add mental load

Ideally, Unbound would render itself obsolete because BM devs may agree with its design and ship
the feature natively. I think that would be a wonderful outcome.

## Installation

Download from [Releases](../../releases) and run the Unbound installer.
Choose **Install**, then open DaVinci Resolve.

Unbound starts automatically and adds the Unbound button to supported panels.

Run the installer again to update or uninstall. Uninstall always keeps your
settings and layouts.

Unbound supports Windows 10/11 (x64) and macOS. The universal Mac app includes Apple
Silicon and Intel code. No Linux port yet.

## Is it safe to install Unbound?

Yes. _Says random stranger on the internet_.

Ask your agent to audit and check this repository and the related binaries.
This will give you a better and more reliable answer than the mechanisms
Microsoft and Apple are offering.

It is good to check, but do it the 2026 way.

Windows builds are unsigned; macOS builds are ad-hoc signed, without Apple notarization.

Our operating systems still offer ways to decide for ourselves what to run, though.
For Unbound downloaded from this repository, use the per-app exception
when offered:

- **Windows:** in the SmartScreen warning, choose **More info**, then **Run anyway**.
  Availability depends on your system's security policy; see
  [Microsoft's SmartScreen guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation).
- **macOS:** try opening the app once, then go to **System Settings → Privacy &
  Security → Open Anyway** and confirm. See
  [Apple's instructions](https://support.apple.com/en-us/102445).
