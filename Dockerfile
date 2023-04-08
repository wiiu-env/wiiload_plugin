FROM ghcr.io/wiiu-env/devkitppc:20220917

COPY --from=ghcr.io/wiiu-env/libwupsbackend:20220904 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20220903 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20220924 /artifacts $DEVKITPRO

WORKDIR project
