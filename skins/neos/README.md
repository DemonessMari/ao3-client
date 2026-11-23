# neos native adaptations

Original design: **neos**, by ZerafinaCSS.
Source: https://github.com/ZerafinaCSS/neos
Source revision: f4f5751ba953a28b4ba15386c753367b1f5ec633
License: MIT; original copyright and license are in LICENSE.

These JSON skins adapt the Tumblr Blue and Charcoal Rose variants to AO3 Client's native components. They preserve the original 14-unit button corners, 8-unit content corners, 4-unit fields, compact 10-unit card inset, slim header and distinct form/content surfaces. Tumblr Blue retains the upstream 103% interface scale. Select them in App skin, or import the JSON files in either edition.

Adaptation changes: fixed native units replace rem/em values; transparent surfaces become opaque; borders are strengthened to meet the client's contrast checks and keep entries distinct. Tumblr Blue uses the upstream hover blue as its accent so it meets 4.5:1 contrast on content surfaces. Card padding is applied once. Fonts and reading colors remain independently configurable.

The native format does not reproduce neos's CSS gradients, shadows, tag-specific colors, replacement icons, header images or rearranged website navigation. These are partial native adaptations of the credited design, not the original CSS theme. The files load no scripts or remote assets.
