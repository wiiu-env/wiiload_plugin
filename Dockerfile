FROM wiiuenv/devkitppc:20220507

COPY --from=wiiuenv/libwupsbackend:20220514 /artifacts $DEVKITPRO
COPY --from=wiiuenv/librpxloader:20220422 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiupluginsystem:20220513 /artifacts $DEVKITPRO

WORKDIR project