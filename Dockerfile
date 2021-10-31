FROM wiiuenv/devkitppc:20210920

COPY --from=wiiuenv/libwupsbackend:20211001 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20210924 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiupluginsystem:20211001 /artifacts $DEVKITPRO

WORKDIR project