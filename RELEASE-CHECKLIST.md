# Release checks

- Review [feature coverage](FEATURES.md); distinguish native controls, website tools and unimplemented features.
- Build and test both editions; run Android lint.
- Verify release signatures, versions, checksums and desktop installation.
- Check source and assets for credentials or private data.
- Test live search, sign-in and translation.
- Verify account writes on a test account: bookmarks, kudos, subscriptions and later/read.
- Test website sign-in persistence, uploads, downloads, Back/rotation, external links and Clear website sign-in.
- On a phone: test page turns in all themes, chapter navigation, appearance, large fonts, rotation, fullscreen, Back and accessibility.
- Test restart recovery, airplane-mode reading, bookmarks and backup migration.
- Read a large work for an extended session; check memory, battery and responsiveness.

Keep the release marked prerelease until device checks pass.
