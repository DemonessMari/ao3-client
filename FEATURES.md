# Feature coverage

Source for 3.2.0-rc.7. The 3.1.0 release excludes the newest native account actions and embedded website view.

| Area | Native controls | Embedded AO3 website |
| --- | --- | --- |
| Search and discovery | Work search, filters, tag suggestions, profiles with AO3 pictures, public work/bookmark listings | Tag/people/bookmark search, fandoms, directories, feeds |
| Reading | Chapters, notes, inline styles/links, translation, text-only/fullscreen, scrolling/paged reading | Original HTML, images, tables, poetry, media, footnotes and work skins |
| Offline library | Downloads, progress, favorites, local bookmarks, backups, text export | Website view requires a connection |
| Account work actions | Kudos, Mark for Later/Read, work subscriptions; AO3 bookmark create/edit/delete with notes, tags, collections, pseuds, privacy and recommendations | Author/series subscriptions, list management, external/series bookmarks |
| Comments | Reading, paging, posting and replies | Editing/deletion, moderation, freezing and inbox |
| Series and collections | Public work cards | Series order/navigation, related works, gifts, collections and challenges |
| Account | Sign-in, persistent native session, sign-out, verified website-session handoff | Preferences, profile/pseuds, mute/block, invitations and account management |
| Appearance | Reader theme/font/spacing/margins/alignment controls; local TrueType/OpenType font importing for reading and native interface text; optional system theme; native app-skin editor, preview and shared JSON import/export | Official site/work skin browsing, selection and editing |
| Creating works | — | Drafts, preview/post/edit/import, chapters, series, co-creators and permitted participant tools |
| Help and policies | Privacy/security/license documents | AO3 help, news, support and abuse forms |

Website tools use AO3’s interface and a separate saved website sign-in. The explicit website-session handoff can authenticate the native reader after a fresh server identity check. AO3 human-verification challenges require the user’s interaction. They are not native equivalents. The site controls availability and permissions; staff tools are not part of ordinary client parity.

Still missing from the native reader: faithful offline HTML/media/work skins, and native controls for the website-only areas above. Existing offline copies preserve text, notes and basic inline formatting.

Verification still required: authenticated account writes and role-specific website workflows; phone rendering, accessibility, rotation/Back/file pickers and long reading sessions. Automated fixture tests and builds do not establish full website parity or device acceptance.

Further reader customization not implemented: configurable tap zones or volume-button actions. Text-to-speech and auto-scroll are also absent. Native app skins currently customize colors; AO3 CSS and layout skins remain in the website view. Profile-picture display is native; changing an AO3 icon uses the website pseud editor.
