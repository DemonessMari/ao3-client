# Feature coverage

Unreleased source after 3.2.0-rc.14. The 3.1.0 release excludes the newest native account actions and embedded website view.

| Area | Native controls | Embedded AO3 website |
| --- | --- | --- |
| Search and discovery | Work search, filters including single-chapter inclusion/exclusion, tag suggestions, profiles with AO3 pictures, public work/bookmark listings | Tag/people/bookmark search, fandoms, directories, feeds |
| Reading | Chapters, notes, inline styles/links, offline read-aloud with language/rate/pause/stop, configurable book-mode tap zones, auto-scroll with speed/pause, remappable Android volume-button navigation and keep-screen-on, translation, text-only/fullscreen, scrolling/paged reading; isolated original-formatting view with tables, poetry, images, footnotes and work-skin CSS | Embedded audio/video and external work-skin resources |
| Offline library | Downloads with chapter HTML/work-skin CSS and optional images, progress, favorites, local bookmarks, backups, text export | Website view requires a connection |
| Account work actions | Kudos, Mark for Later/Read, work/author/series subscriptions; own AO3 bookmark and subscription lists; AO3 bookmark create/edit/delete with notes, tags, collections, pseuds, privacy and recommendations | Bulk list management, external/series bookmark editing |
| Comments | Reading, paging, posting, replies, editing, deletion, offered moderation/freezing controls and paginated inbox with individual and bulk read/unread/delete | Role-specific tools |
| Series and collections | Full series listings, memberships and previous/next visible work navigation from work details and the reader | Related works, gifts, collections and challenges |
| Account | Avatar account menu, sign-in, persistent native session, sign-out, verified website-session handoff; paginated AO3 Reading History and Marked for Later lists with activity and deleted-work entries | Preferences, profile/pseuds, mute/block, invitations and account management |
| Appearance | Reader theme/font/spacing/margins/alignment controls; local TrueType/OpenType font importing for reading and native interface text; optional system theme; native app-skin editor, preview and shared JSON import/export with interface scale and control corners | Official site/work skin browsing, selection and editing |
| Creating works | — | Drafts, preview/post/edit/import, chapters, series, co-creators and permitted participant tools |
| Help and policies | Privacy/security/license documents | AO3 help, news, support and abuse forms |

Website tools use AO3’s interface and a separate saved website sign-in. The explicit website-session handoff can authenticate the native reader after a fresh server identity check. AO3 human-verification challenges require the user’s interaction. They are not native equivalents. The site controls availability and permissions; staff tools are not part of ordinary client parity.

Still missing from the native reader: embedded audio/video, external work-skin assets and native controls for the website-only areas above. Original formatting preserves chapter HTML and available inline work-skin CSS; Save offline with images embeds supported HTTPS images. External CSS resources are blocked, and older saved copies need downloading again to gain HTML. Simplified scrolling/book mode retains text, notes and basic inline formatting.

Verification still required: authenticated account writes and role-specific website workflows; phone rendering, accessibility, rotation/Back/file pickers and long reading sessions. Automated fixture tests and builds do not establish full website parity or device acceptance.

Read-aloud uses installed eSpeak NG on desktop and system voices advertised as offline on Android; it stops when leaving the reader or changing chapters. Android pauses when backgrounded and on audio-focus loss. Book-mode tap zones can assign previous/next/controls/no action to left, center and right, with adjustable zone widths. Android volume keys can be assigned previous page, next page or system volume separately. Profile-picture display is native; changing an AO3 icon uses the website pseud editor.

Chapter count is separate from the One Shot tag: the native chapter filter uses AO3's expected chapter count, while author-applied tags can be included/excluded through the tag fields. The app cannot infer whether a story is self-contained.

Auto-scroll is opt-in per reading session and stops at the chapter boundary. Speed is saved; starting automatically is not. Android volume navigation is opt-in: each key can advance, go back or control system volume, with ordinary volume controls retained outside the reader and while reading dialogs are open. Keep-screen-on applies only while the reader is displayed.

Series navigation fetches AO3 work metadata on request and uses its supplied previous/next visible-work links for each membership. It opens the chosen work in native details before reading. Full series listings and subscriptions are native. Series lists require a connection. Native skin layout tokens are documented in SKINS.md.

AO3 Reading History and Marked for Later are fetched on request after verifying the current account in the response. They remain separate from local recent reads. Activity text is displayed in memory and excluded from local library metadata/backups. Viewing these lists does not submit deletion or clear-history forms. Removing history entries, clearing history and changing the AO3 history preference remain website workflows.

Native AO3 lists can filter entries on the current page. Inbox selections use AO3’s repeated record fields in one bulk request. Native AO3 lists verify the response account before displaying private data or submitting an action. They are fetched on request and not included in library backups. Unsubscribe and inbox actions fetch fresh forms/tokens and submit once; uncertain results require a refresh. Comment actions only appear when supplied by AO3 for the current user, and recheck permissions before submission. Fixture tests do not establish production account permissions or successful live writes.
