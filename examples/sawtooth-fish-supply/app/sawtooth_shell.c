/**
* @brief 
* @file sawtooth_shell.c
* @author J.H. 
* @date 2018-11-27
*/

/* system includes */
#include <assert.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <shell/shell.h>
#pragma GCC diagnostic pop
#include <stdlib.h>

/* local includes */
#include "sawtooth/st_transaction.h"
#include "payload/payload.h"
#include "zephyr-wallet/http_service.h"
#include "zephyr-wallet/config.h"
#include "zephyr-wallet/wallet.h"

#define PAYLOAD_BUF_SIZE    2048
static uint8_t _buf[PAYLOAD_BUF_SIZE];

static http_nfo_t http_client_nfo = {
    .host =         SERVER_ADDR,
    .port =         8020,
    .url =          "/transactions?wait",
    .content_type = "application/octet-stream",
    .headers =      NULL,
    .payload =      _buf,
    .payload_size = PAYLOAD_BUF_SIZE
};

static fish_record_t _rec = {
    .species =      "???",
    .length  =      0x0,
    .weight  =      0x0
};

static int sub_record_species(const struct shell *shell, size_t argc, char *argv[])
{
    if(argc < 2) {
        shell_print(shell, "missing argument: %d-char species type", sizeof(_rec.species)-1);
        return -1;
    }
    strncpy(_rec.species, argv[1], sizeof(_rec.species)-1);
    return 0;
}

static int sub_record_id(const struct shell *shell, size_t argc, char *argv[])
{
    if(argc < 2) {
        shell_print(shell, "missing argument: id");
        return -1;
    }
    strncpy(_rec.id, argv[1], sizeof(_rec.id)-1);
    return 0;
}

// macro to create a function that converts argv[1] to int and sets it as a specified
// field in the `fish_record_t` struct
#define CREATE_SUB_RECORD_INT(item)\
static int sub_record_##item(const struct shell *shell, size_t argc, char *argv[])\
{\
    if(argc < 2) {\
        shell_print(shell, "missing argument:"#item);\
        return -1;\
    }\
    errno = 0;\
    char *endptr = NULL;\
    _rec.item = strtoul(argv[1], &endptr, 10);\
    if((errno != 0) || ((endptr != NULL) && (*endptr != 0))) {\
        shell_fprintf(shell, SHELL_WARNING, "invalid length\n");\
        return -1;\
    }\
    return 0;\
}
CREATE_SUB_RECORD_INT(weight)
CREATE_SUB_RECORD_INT(length)


static int sub_record_print(const struct shell *shell, size_t argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

    shell_print(shell, "id\t%s", _rec.id);
    shell_print(shell, "species\t%s", _rec.species);
    shell_print(shell, "weight\t%x", _rec.weight);
    shell_print(shell, "length\t%x", _rec.length);

	if (shell_help_requested(shell)) {
		shell_help_print(shell, NULL, 0);
		return -ENOEXEC;
	}

	return 0;
}

static int __sign_and_send(const uint8_t *payload, size_t payload_size, uint8_t *buf, size_t buf_size)
{
    account_t *acc = wallet_get_account();
    assert(acc != NULL);
    st_privkey_t pk;
    memcpy(pk.k, acc->privkey.k, sizeof(pk));
    int tx_written = sign_and_encode(&pk, payload, payload_size, buf, buf_size);
    if(tx_written <= 0) {
        return -1;
    }
    uint8_t *body = NULL;
    size_t content_len = 0;
    http_client_nfo.payload = buf;
    http_client_nfo.payload_size = tx_written;
    if(http_send_data(&http_client_nfo, &body, &content_len) < 0) {
        printk("error: HTTP send\n");
        return -1;
    }
    return 0;
}

static int sub_create_agent(const struct shell *shell, size_t argc, char *argv[])
{
    if(argc < 2) {
        printk("missing argument: agent name\n");
        return -1;
    }
    size_t payload_written = encode_create_agent(argv[1], _buf, PAYLOAD_BUF_SIZE);
    if(payload_written <= 0) {
        return -1;
    }
    return __sign_and_send(_buf, payload_written, _buf+payload_written, PAYLOAD_BUF_SIZE-payload_written);
}

static int sub_create_record(const struct shell *shell, size_t argc, char *argv[])
{
    size_t payload_written = encode_createRecord(&_rec, _buf, PAYLOAD_BUF_SIZE);
    if(payload_written <= 0) {
        return -1;
    }
    return __sign_and_send(_buf, payload_written, _buf+payload_written, PAYLOAD_BUF_SIZE-payload_written);
}

static int sub_update_record(const struct shell *shell, size_t argc, char *argv[])
{
    size_t payload_written = encode_updateProperties(&_rec, _buf, PAYLOAD_BUF_SIZE);
    if(payload_written <= 0) {
        return -1;
    }
    return __sign_and_send(_buf, payload_written, _buf+payload_written, PAYLOAD_BUF_SIZE-payload_written);
}

SHELL_CREATE_STATIC_SUBCMD_SET(sub_st_record) {
    SHELL_CMD(id, NULL, "", sub_record_id),
    SHELL_CMD(length, NULL, "", sub_record_length),
    SHELL_CMD(species, NULL, "", sub_record_species),
    SHELL_CMD(weight, NULL, "", sub_record_weight),
	SHELL_SUBCMD_SET_END
};

SHELL_CREATE_STATIC_SUBCMD_SET(sub_st) {
    SHELL_CMD(create_agent, NULL, "", sub_create_agent),
    SHELL_CMD(create_record, NULL, "", sub_create_record),
    SHELL_CMD(record, &sub_st_record, "", sub_record_print),
    SHELL_CMD(update_record, NULL, "", sub_update_record),
	SHELL_SUBCMD_SET_END
};
SHELL_CMD_REGISTER(st, &sub_st, "sawtooth", NULL);