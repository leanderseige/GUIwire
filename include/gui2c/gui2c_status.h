#ifndef GUI2C_STATUS_H
#define GUI2C_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GUI2C_STATUS_OK = 0,
    GUI2C_STATUS_ERR_INVALID_ARGUMENT,
    GUI2C_STATUS_ERR_BUFFER_TOO_SMALL,
    GUI2C_STATUS_ERR_PROTOCOL,
    GUI2C_STATUS_ERR_NOT_SUPPORTED,
    GUI2C_STATUS_ERR_NOT_FOUND,
    GUI2C_STATUS_ERR_ALREADY_EXISTS,
    GUI2C_STATUS_ERR_IO,
    GUI2C_STATUS_ERR_NO_SPACE,
    GUI2C_STATUS_ERR_STATE,
    GUI2C_STATUS_ERR_TRANSPORT
} gui2c_status_t;

const char *gui2c_status_str(gui2c_status_t status);

#ifdef __cplusplus
}
#endif

#endif
