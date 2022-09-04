FROM wiiuenv/devkitppc:20220806

COPY --from=wiiuenv/libwupsbackend:20220904 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20220903 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiupluginsystem:20220904 /artifacts $DEVKITPRO

WORKDIR project
