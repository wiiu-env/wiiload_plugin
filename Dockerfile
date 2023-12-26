FROM ghcr.io/wiiu-env/devkitppc:20231112

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:0.8.0-dev-20231221-ca17105 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libwupsbackend:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/librpxloader:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20230621 /artifacts $DEVKITPRO

WORKDIR project
