#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "utils/logger.h"
#include "utils/utils.h"

#include "ipcclient.h"

int (*ipc_ioctl)(ipcmessage *message) = (int (*)(ipcmessage*)) *(uint32_t*)0x80800000;

#define ALIGN(align)       __attribute__((aligned(align)))

int32_t doIOCTL(int32_t command, uint32_t *in_buf, uint32_t in_length, uint32_t *io_buf, uint32_t io_length) {
    ALIGN(0x20) ipcmessage message;

    memset(&message,0,sizeof(message));

    message.command = command;

    message.ioctl.buffer_in = in_buf;
    message.ioctl.length_in = in_length;
    message.ioctl.buffer_io = io_buf;
    message.ioctl.length_io = io_length;

    DEBUG_FUNCTION_LINE("command: %d in_buf %08X size: %d io_buf %08X size: %d \n",command, in_buf,in_length,io_buf,io_length);

    //DCFlushRange(&message, sizeof(ipcmessage));
    //ICInvalidatRange(&message, sizeof(ipcmessage));

    return ((int (*)(ipcmessage *))((uint32_t*)*((uint32_t*)0x80800000)) )(&message);
}

plugin_loader_handle IPC_Open_Plugin_Loader(uint32_t startAddress, uint32_t endAddress) {
    uint32_t *io_buf = (uint32_t*)memalign(0x20, ROUNDUP(8,0x20));
    if(!io_buf) {
        return (plugin_loader_handle) NULL;
    }

    io_buf[0] = startAddress;
    io_buf[1] = endAddress;

    int32_t ret = doIOCTL(IOCTL_OPEN_PLUGIN_LOADER, io_buf, 8, io_buf, 4);
    if(ret < 0) {
        free(io_buf);
        return (plugin_loader_handle) NULL;
    }

    plugin_information_handle result = (plugin_loader_handle) io_buf[0];
    free(io_buf);
    return result;
}

bool IPC_Close_Plugin_Loader(plugin_loader_handle handle) {
    uint32_t *io_buf = (uint32_t*)memalign(0x20, ROUNDUP(4,0x20));
    if(!io_buf) {
        return false;
    }

    io_buf[0] = handle;

    int32_t ret = doIOCTL(IOCTL_CLOSE_PLUGIN_LOADER, io_buf, 4, NULL, 0);
    if(ret < 0) {
        free(io_buf);
        return false;
    }

    free(io_buf);
    return true;
}

int32_t IPC_Get_Plugin_Information(const char * path, plugin_information_handle ** handleList, uint32_t * handleListSize) {
    uint32_t buffersize = ROUNDUP((128 * sizeof(plugin_information_handle)) + 4,0x20);
    uint32_t *io_buf = (uint32_t*)memalign(0x20, buffersize);
    if(!io_buf) {
        return -1;
    }

    io_buf[0] = (uint32_t) path;

    int32_t ret = doIOCTL(IOCTL_PLUGIN_LOADER_GET_INFORMATION_FOR_PATH, io_buf, 4, io_buf, buffersize);
    if(ret < 0) {
        free(io_buf);
        return ret;
    }
    uint32_t length = io_buf[0];
    if(handleListSize != NULL) {
        *handleListSize = length;
    }

    uint32_t result = -1;

    if(handleList != NULL) {
        // we create a new buffer so the caller can free it properly
        uint32_t outbuffersize = ROUNDUP((length * sizeof(plugin_information_handle)),0x20);
        *handleList = (uint32_t*)memalign(0x20, outbuffersize);
        if(*handleList != NULL) {
            result = 0;
            memcpy(*handleList, &(io_buf[1]), length * sizeof(plugin_information_handle));
        }
    }

    free(io_buf);
    return result;
}

int32_t IPC_Get_Plugin_Information_Loaded(plugin_information_handle ** handleList, uint32_t * handleListSize) {
    uint32_t buffersize = ROUNDUP((128 * sizeof(plugin_information_handle)),0x20);
    uint32_t *io_buf = (uint32_t*)memalign(0x20, buffersize);
    if(!io_buf) {
        return -1;
    }

    int32_t ret = doIOCTL(IOCTL_PLUGIN_LOADER_GET_INFORMATION_LOADED, io_buf, 0, io_buf, buffersize);
    if(ret < 0) {
        free(io_buf);
        return ret;
    }
    // DEBUG_FUNCTION_LINE("IPC_Get_Plugin_Information_Loaded was fine\n");

    uint32_t length = io_buf[0];
    if(handleListSize != NULL) {
        // DEBUG_FUNCTION_LINE("length set to %d\n", length);
        *handleListSize = length;
    }
    uint32_t result = -1;

    if(handleList != NULL && length > 0) {
        // we create a new buffer so the caller can free it properly
        uint32_t outbuffersize = ROUNDUP((length * sizeof(plugin_information_handle)),0x20);
        *handleList = (uint32_t*)memalign(0x20, outbuffersize);
        if(*handleList != NULL) {
            result = 0;
            memcpy(*handleList, &(io_buf[1]), length * sizeof(plugin_information_handle));
        }
    }

    free(io_buf);
    return result;
}

int32_t IPC_Get_Plugin_Information_Details(plugin_information_handle * handles, uint32_t handlesize, plugin_information ** informationList, uint32_t * informationListSize) {
    uint32_t buffersize = ROUNDUP((handlesize * sizeof(plugin_information)),0x20);
    if(buffersize < 8){
        buffersize = 8;
    }
    uint32_t *io_buf = (uint32_t*)memalign(0x20, buffersize);
    if(!io_buf) {
        if(io_buf != NULL) {
            free(io_buf);
        }
        return -1;
    }

    io_buf[0] = (uint32_t) handles;
    io_buf[1] = handlesize;

    int32_t ret = doIOCTL(IOCTL_PLUGIN_LOADER_GET_INFORMATION_DETAILS, io_buf, 8, io_buf, buffersize);
    if(ret < 0) {
        free(io_buf);
        return ret;
    }

    uint32_t result = -1;

    if(informationListSize != NULL) {
        *informationListSize = handlesize;
    }

    if(informationList != NULL) {
        // we create a new buffer so the caller can free it properly
        uint32_t outbuffersize = ROUNDUP((handlesize * sizeof(plugin_information)),0x20);
        *informationList = (plugin_information*)memalign(0x20, outbuffersize);
        if(*informationList != NULL) {
            result = 0;
            memcpy(*informationList, &(io_buf[0]), handlesize * sizeof(plugin_information));
        }
    }

    free(io_buf);
    return result;
}

int32_t IPC_Delete_Plugin_Information(plugin_information_handle handle) {
    uint32_t *io_buf = (uint32_t*)memalign(0x20, ROUNDUP(4,0x20));
    if(!io_buf) {
        if(io_buf != NULL) {
            free(io_buf);
        }
        return -1;
    }

    io_buf[0] = (uint32_t) handle;

    int32_t ret = doIOCTL(IOCTL_PLUGIN_LOADER_DELETE_INFORMATION, io_buf, 4, NULL, 0);
    if(ret < 0) {
        free(io_buf);
        return ret;
    }

    free(io_buf);
    return 0;
}

int32_t IPC_Link_Plugin_Information(plugin_loader_handle handle, plugin_information_handle * handleList, uint32_t listSize) {
    uint32_t buffersize = ROUNDUP((listSize * sizeof(plugin_information_handle)),0x20);
    uint32_t io_buffersize = ROUNDUP(12,0x20);
    uint32_t *io_buf = (uint32_t*)memalign(0x20, io_buffersize);
    uint32_t * buf = (uint32_t*)memalign(0x20, buffersize);
    if(!io_buf || !buf) {
        if(buf != NULL) {
            free(buf);
        }
        if(io_buf != NULL) {
            free(io_buf);
        }
        return -1;
    }

    memcpy(buf, handleList, listSize * sizeof(plugin_information_handle*));

    io_buf[0] = handle;
    io_buf[1] = (uint32_t) buf;
    io_buf[2] = listSize;

    int32_t ret = doIOCTL(IOCTL_PLUGIN_LOADER_LINK_VIA_INFORMATION, io_buf, 12, io_buf, io_buffersize);
    if(ret < 0) {
        free(io_buf);
        free(buf);
        return ret;
    }
    int32_t result = (int32_t) io_buf[0];

    free(io_buf);
    free(buf);
    return result;
}

int32_t IPC_Link_Plugin_Information_On_Restart(plugin_information_handle * handleList, uint32_t listSize) {
    uint32_t buffersize = ROUNDUP((listSize * sizeof(plugin_information_handle)),0x20);
    uint32_t io_buffersize = ROUNDUP(8,0x20);
    uint32_t *io_buf = (uint32_t*)memalign(0x20, io_buffersize);
    uint32_t * buf = (uint32_t*)memalign(0x20, buffersize);
    if(!io_buf || !buf) {
        if(buf != NULL) {
            free(buf);
        }
        if(io_buf != NULL) {
            free(io_buf);
        }
        return -1;
    }

    memcpy(buf, handleList, listSize * sizeof(plugin_information_handle*));

    io_buf[0] = (uint32_t) buf;
    io_buf[1] = listSize;

    int32_t ret = doIOCTL(IOCTL_PLUGIN_LOADER_LINK_VIA_INFORMATION_ON_RESTART, io_buf, 8, io_buf, io_buffersize);
    if(ret < 0) {
        free(io_buf);
        free(buf);
        return ret;
    }
    int32_t result = (int32_t) io_buf[0];

    free(io_buf);
    free(buf);
    return result;
}



int32_t IPC_Get_Plugin_Information_For_Filepath(const char * path, plugin_information_handle * handle) {
    uint32_t buffersize = ROUNDUP((sizeof(plugin_information_handle)),0x20);
    uint32_t *io_buf = (uint32_t*)memalign(0x20, buffersize);
    if(!io_buf) {
        return -1;
    }

    io_buf[0] = (uint32_t) path;

    int32_t ret = doIOCTL(IOCTL_PLUGIN_INFORMATION_GET_INFORMATION_FOR_FILEPATH, io_buf, 4, io_buf, 4);
    if(ret < 0) {
        free(io_buf);
        return ret;
    }
    *handle = io_buf[0];

    free(io_buf);
    return 0;
}


