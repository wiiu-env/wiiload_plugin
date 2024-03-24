FROM ghcr.io/wiiu-env/devkitppc:20231112

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:0.8.0-dev-20240302-3b5cc2f /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libwupsbackend:0.1.3-dev-20240323-9493c4f /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20230621 /artifacts $DEVKITPRO

WORKDIR project
