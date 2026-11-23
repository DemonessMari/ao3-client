# App skins

Open **App skin** from Home or Settings. Choose Light, Dark or Sepia, or enter colors. Preview before applying. Import and Export use the same JSON format on Android and desktop. Skin files contain colors and validated native layout settings, without scripts, CSS or external resources.

Colors use opaque `#RRGGBB` values. Text must have at least 4.5:1 contrast with both backgrounds and with its accent button. Accents must have at least 4.5:1 contrast with both backgrounds. Invalid files leave the active skin unchanged.

```json
{
  "format": "ao3-client-skin",
  "version": 1,
  "name": "Blue",
  "background": "#f5f9ff",
  "surface": "#ffffff",
  "text": "#17243a",
  "accent": "#174a8b",
  "onAccent": "#ffffff"
}
```

AO3 site/work CSS skins apply in the AO3 website view. They cannot be imported as native app skins. Downloaded fics currently preserve text and basic inline formatting, not complete work skins.

Choose **System** in Android reading appearance, or **Follow system theme** in the desktop app-skin editor, to switch between light and dark with the operating system. Desktop keeps the previous custom skin for when you turn this off. Side margins and paragraph alignment are separate reading settings, available inside an open work, with a live preview. They apply in scrolling and book mode.

Use **Import font** in Reading settings to select a `.ttf` or `.otf` file (up to 20 MB). The app keeps a local copy and selects it for the preview, scrolling and book mode. Select Serif or Sans serif to switch back, or **Remove font** to delete the selected imported copy. Up to 20 fonts can be kept. Font files are separate from color skins and library backups.

Imported fonts also apply to native menus, titles and work previews by default. Turn off **Use imported font in interface** to keep the system interface font while retaining the imported font for reading. Interface sizes and heading weights are preserved.

## Native layout skins

Version 2 adds `uiScale` (1–1.5) and `cornerRadius` (whole numbers 0–24) to the fields above. Set `version` to `2` and include both fields. Version 1 files remain supported. The editor previews and exports version 2.

Interface scale enlarges native interface text and controls while retaining system font accessibility scaling. Reader font size can also be adjusted separately. Android scales the entire native view, including the reading area; desktop scales interface fonts and standard layout spacing. Corner radius controls native buttons on desktop and Material component shapes on Android. Menus retain their platform layout and are not clipped to a rounded container. Desktop uses native style drawing, fonts and spacing; Android uses Compose density and shapes. These settings do not inject CSS.

Skins are settings, not arbitrary layout templates: they cannot move controls, load external resources, execute code or reproduce arbitrary AO3 CSS. Font importing remains separate. Named component rules and bundled image/font assets are not implemented.
