# Floating panels after leaving Dual Screen mode

## Report and confirmation status

Andy reports this with Unbound 0.2.0, Windows 11 and DaVinci Resolve 20.3.1:
Media Pool and Index become inaccessible when first
undocked in Dual Screen mode and then switching to Single Screen. The native
slot still says the panel is floating. Returning to Dual Screen brings it back;
docking there and undocking again in Single Screen avoids the problem afterward.

The behavior has not been reproduced in Resolve on the development system.
The page, Full Screen Timeline state and display arrangement have not yet been
established. Inspection of tag `v0.2.0` confirms the same retained page-container
pointer and Hide/Show handling as the current implementation, with no rebinding
when the slot changes page containers. The candidate therefore applies to the
reported version too; this is not confirmation of the reporter's cause.

## Confirmed lifecycle weakness

`SidebarFloatSession::start()` retains a page-container pointer in `m_root`.
Its Hide event hides the floating host; a subsequent Show reveals that host.
The session does not update this pointer if the native slot moves to another
page container. The controller observes supported-pane ParentChange events but
does not rebind an active session's page context.

An isolated Qt fixture using the production Windows backend and
`SidebarFloatSession` establishes these distinct cases for Edit Media Pool and
Edit Index:

| Transition | Result |
| --- | --- |
| Move the original page container from a secondary window to the main window, then hide the secondary window | Floating panel stays visible |
| Move the native slot into a different visible page container, then hide the old container | Session stays active; host is hidden; placeholder remains |
| Show the former container again | Floating panel returns |
| Dock and undock using the new container | Hiding the former container no longer hides the panel |

These Edit floating hosts have neither a Qt parent nor a native HWND owner.
Automatic hiding of windows owned by the secondary window therefore does not
explain the fixture failure. Fairlight Media Pool has different ownership for
audition behavior and must be considered separately if the report concerns it.

## Evidence needed before choosing a production fix

Capture native slot and page-container identities/ancestry before and after a
failing mode switch. Also distinguish a hidden host from a visible off-screen
host, hidden transferred content, or destruction/replacement of the slot.
If the slot changes page containers, the fix should rebind visibility tracking
to its current container after the native transition, preserving the existing
host, content, geometry and saved state. It must still hide on actual page
changes and must not add polling or unconditional host-show behavior.

The fixture reproduces one way to obtain the reported symptoms. It does not
establish that Resolve takes that path during the reported Dual Screen switch.
No production workaround has been applied.
