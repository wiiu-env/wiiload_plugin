FROM ghcr.io/wiiu-env/devkitppc:20241128

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:0.8.2-dev-20250125-f8faa9f /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libwupsbackend:20240425 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20240425 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20241221 /artifacts $DEVKITPRO

WORKDIR project
