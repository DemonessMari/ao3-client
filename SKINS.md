# App skins

Open **App skin** from Home or Settings. Choose Light, Dark or Sepia, or enter colors. Preview before applying. Import and Export use the same JSON format on Android and desktop. Skin files contain colors and a name, without scripts, CSS or external resources.

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
