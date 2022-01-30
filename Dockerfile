FROM wiiuenv/devkitppc:20211229

COPY --from=wiiuenv/libwupsbackend:20211001 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20211002 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiupluginsystem:20220123 /artifacts $DEVKITPRO

WORKDIR project