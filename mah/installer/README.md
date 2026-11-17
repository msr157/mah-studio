# MAH Studio installer & updates (stubs)

## Windows installer (future signed build)

`MahStudio.iss` — Inno Setup script skeleton. Fill `Source` paths after CI produces `MAHStudio-*.zip` with OBS + plugins.

Signing requires your code-signing cert (`SignTool`).

## Auto-update channel

`update-manifest.example.json` — shape for a future update feed (version + download URL + sha256).

Until then: pull `master` / GitHub Releases manually.
