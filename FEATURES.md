# Feature coverage

Source for 3.2.0-rc.10. The 3.1.0 release excludes the newest native account actions and embedded website view.

| Area | Native controls | Embedded AO3 website |
| --- | --- | --- |
| Search and discovery | Work search, filters including single-chapter inclusion/exclusion, tag suggestions, profiles with AO3 pictures, public work/bookmark listings | Tag/people/bookmark search, fandoms, directories, feeds |
| Reading | Chapters, notes, inline styles/links, offline read-aloud with language/rate/pause/stop, configurable book-mode tap zones, auto-scroll with speed/pause, Android volume-button navigation and keep-screen-on, translation, text-only/fullscreen, scrolling/paged reading | Original HTML, images, tables, poetry, media, footnotes and work skins |
| Offline library | Downloads, progress, favorites, local bookmarks, backups, text export | Website view requires a connection |
| Account work actions | Kudos, Mark for Later/Read, work subscriptions; AO3 bookmark create/edit/delete with notes, tags, collections, pseuds, privacy and recommendations | Author/series subscriptions, list management, external/series bookmarks |
| Comments | Reading, paging, posting and replies | Editing/deletion, moderation, freezing and inbox |
| Series and collections | Public work cards | Series order/navigation, related works, gifts, collections and challenges |
| Account | Avatar account menu, sign-in, persistent native session, sign-out, verified website-session handoff | Preferences, profile/pseuds, mute/block, invitations and account management |
| Appearance | Reader theme/font/spacing/margins/alignment controls; local TrueType/OpenType font importing for reading and native interface text; optional system theme; native app-skin editor, preview and shared JSON import/export | Official site/work skin browsing, selection and editing |
| Creating works | — | Drafts, preview/post/edit/import, chapters, series, co-creators and permitted participant tools |
| Help and policies | Privacy/security/license documents | AO3 help, news, support and abuse forms |

Website tools use AO3’s interface and a separate saved website sign-in. The explicit website-session handoff can authenticate the native reader after a fresh server identity check. AO3 human-verification challenges require the user’s interaction. They are not native equivalents. The site controls availability and permissions; staff tools are not part of ordinary client parity.

Still missing from the native reader: faithful offline HTML/media/work skins, and native controls for the website-only areas above. Existing offline copies preserve text, notes and basic inline formatting.

Verification still required: authenticated account writes and role-specific website workflows; phone rendering, accessibility, rotation/Back/file pickers and long reading sessions. Automated fixture tests and builds do not establish full website parity or device acceptance.

Further reader customization not implemented: remappable volume-button actions. Android has optional fixed volume up/down navigation. Read-aloud uses installed eSpeak NG on desktop and system voices advertised as offline on Android; it stops when leaving the reader or changing chapters. Android pauses when backgrounded and on audio-focus loss. Book-mode tap zones can assign previous/next/controls/no action to left, center and right, with adjustable zone widths. Native app skins currently customize colors; AO3 CSS and layout skins remain in the website view. Profile-picture display is native; changing an AO3 icon uses the website pseud editor.

2026-11-23 source recheck: the official routes and work-search query still expose the gaps above. Highest-priority native reader gaps are series order/next-work navigation; inbox and comment management; saved subscription/bookmark/history/marked-for-later lists; and faithful work HTML/media/skins. Discovery (tags/people/bookmarks), profile/preferences/mute/block, and creator/collection/challenge tools remain website workflows. A source audit does not verify production account actions.

Chapter count is separate from the One Shot tag: the native chapter filter uses AO3's expected chapter count, while author-applied tags can be included/excluded through the tag fields. The app cannot infer whether a story is self-contained.

Auto-scroll is opt-in per reading session and stops at the chapter boundary. Speed is saved; starting automatically is not. Android volume navigation is opt-in: volume down advances and volume up goes back, with ordinary volume controls retained outside the reader and while reading dialogs are open. Keep-screen-on applies only while the reader is displayed.
