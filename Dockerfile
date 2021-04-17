FROM wiiuenv/devkitppc:20210414

COPY --from=wiiuenv/libwupsbackend:202101101720554d1bfe /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20210116 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiupluginsystem:20210417 /artifacts $DEVKITPRO

WORKDIR project