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
