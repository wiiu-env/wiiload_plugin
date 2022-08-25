FROM wiiuenv/devkitppc:20220806

COPY --from=wiiuenv/libwupsbackend:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20220825 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiupluginsystem:20220724 /artifacts $DEVKITPRO

WORKDIR project