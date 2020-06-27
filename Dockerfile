FROM wiiuenv/devkitppc:20200625

COPY --from=wiiuenv/libwupsbackend:20200627 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiupluginsystem:20200626 /artifacts $DEVKITPRO

WORKDIR project