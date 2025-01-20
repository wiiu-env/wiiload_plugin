FROM ghcr.io/wiiu-env/devkitppc:20241128

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20240505 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libwupsbackend:20240425 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20240425 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20241221 /artifacts $DEVKITPRO

WORKDIR project
