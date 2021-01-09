FROM wiiuenv/devkitppc:20210101

COPY --from=wiiuenv/libwupsbackend:20210109 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiupluginsystem:20210109 /artifacts $DEVKITPRO

WORKDIR project