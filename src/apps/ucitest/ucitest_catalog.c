#include "ucitest_catalog.h"

#define FS_END 0

static const char *yes_no_choices[] = { "no", "yes" };
static const char *http_verb_choices[] = {
    "invalid", "GET", "PUT", "POST", "PATCH", "DELETE", "HEAD", "OPTIONS",
    "CONNECT", "TRACE"
};
static const char *http_body_choices[] = {
    "invalid", "binary", "json obj", "json arr", "url enc"
};

const UciTestTargetSpec ucitest_targets[] = {
    { "transport", UC_KIND_TRANSPORT, 0x00u },
    { "dos 1", UC_KIND_DOS, 0x01u },
    { "dos 2", UC_KIND_DOS, 0x02u },
    { "network", UC_KIND_NETWORK, 0x03u },
    { "control", UC_KIND_CONTROL, 0x04u },
    { "softiec", UC_KIND_SOFTIEC, 0x05u },
    { "http", UC_KIND_HTTP, 0x06u }
};

const unsigned char ucitest_target_count =
    sizeof(ucitest_targets) / sizeof(ucitest_targets[0]);

static const UciTestFieldSpec f_raw[] = {
    { "bytes", UC_FIELD_RAW, 0, 0, "04 01", 0, 0 }
};
static const UciTestFieldSpec f_filename[] = {
    { "name", UC_FIELD_TEXT, 0, 0, "test.txt", 0, 0 }
};
static const UciTestFieldSpec f_drive_name[] = {
    { "iec id", UC_FIELD_BYTE, 8u, 0, 0, 0, 0 },
    { "name", UC_FIELD_TEXT, 0, 0, "/usb1/readyos.d81", 0, 0 }
};
static const UciTestFieldSpec f_drive[] = {
    { "iec id", UC_FIELD_BYTE, 8u, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_dirname[] = {
    { "dir", UC_FIELD_TEXT, 0, 0, ".", 0, 0 }
};
static const UciTestFieldSpec f_attr_file[] = {
    { "attr", UC_FIELD_BYTE, 0x01u, 0, 0, 0, 0 },
    { "name", UC_FIELD_TEXT, 0, 0, "test.txt", 0, 0 }
};
static const UciTestFieldSpec f_len[] = {
    { "len", UC_FIELD_WORD, 0x0040u, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_write_data[] = {
    { "", UC_FIELD_CONST, 0x00u, 0, 0, 0, 0 },
    { "", UC_FIELD_CONST, 0x00u, 0, 0, 0, 0 },
    { "data", UC_FIELD_TEXT, 0, 0, "hello from ucitest", 0, 0 }
};
static const UciTestFieldSpec f_seek[] = {
    { "pos", UC_FIELD_DWORD, 0x0000u, 0x0000u, 0, 0, 0 }
};
static const UciTestFieldSpec f_rename[] = {
    { "old", UC_FIELD_TEXTZ, 0, 0, "old.txt", 0, 0 },
    { "new", UC_FIELD_TEXT, 0, 0, "new.txt", 0, 0 }
};
static const UciTestFieldSpec f_copy[] = {
    { "src", UC_FIELD_TEXTZ, 0, 0, "src.txt", 0, 0 },
    { "dst", UC_FIELD_TEXT, 0, 0, "dst.txt", 0, 0 }
};
static const UciTestFieldSpec f_reu_addr_len[] = {
    { "addr", UC_FIELD_DWORD, 0x0000u, 0x0000u, 0, 0, 0 },
    { "len", UC_FIELD_DWORD, 0x0100u, 0x0000u, 0, 0, 0 }
};
static const UciTestFieldSpec f_time_format[] = {
    { "format", UC_FIELD_BYTE, 0u, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_set_time[] = {
    { "year-1900", UC_FIELD_BYTE, 126u, 0, 0, 0, 0 },
    { "month", UC_FIELD_BYTE, 1u, 0, 0, 0, 0 },
    { "day", UC_FIELD_BYTE, 1u, 0, 0, 0, 0 },
    { "hour", UC_FIELD_BYTE, 12u, 0, 0, 0, 0 },
    { "minute", UC_FIELD_BYTE, 0u, 0, 0, 0, 0 },
    { "second", UC_FIELD_BYTE, 0u, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_iface[] = {
    { "iface", UC_FIELD_BYTE, 0, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_ip_set[] = {
    { "bytes", UC_FIELD_RAW, 0, 0,
      "00 C0 A8 01 40 FF FF FF 00 C0 A8 01 01", 0, 0 }
};
static const UciTestFieldSpec f_sock[] = {
    { "sock", UC_FIELD_BYTE, 0, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_sock_read[] = {
    { "sock", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "len", UC_FIELD_WORD, 0x0040u, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_sock_write[] = {
    { "sock", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "data", UC_FIELD_TEXT, 0, 0, "ping", 0, 0 }
};
static const UciTestFieldSpec f_open_net[] = {
    { "port", UC_FIELD_WORD, 80u, 0, 0, 0, 0 },
    { "host", UC_FIELD_TEXTZ, 0, 0, "example.com", 0, 0 }
};
static const UciTestFieldSpec f_decode_track[] = {
    { "bytes", UC_FIELD_RAW, 0, 0, "15 00 00 00 00 00 00 00 00 00", 0, 0 }
};
static const UciTestFieldSpec f_drive_file[] = {
    { "name", UC_FIELD_TEXT, 0, 0, "/temp/ucitest.bin", 0, 0 }
};
static const UciTestFieldSpec f_soft_load_su[] = {
    { "sa", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "verify", UC_FIELD_BOOL, 0, 0, 0, yes_no_choices, 2 },
    { "addr", UC_FIELD_WORD, 0x0801u, 0, 0, 0, 0 },
    { "name", UC_FIELD_TEXT, 0, 0, "boot", 0, 0 }
};
static const UciTestFieldSpec f_soft_load_ex[] = {
    { "sa", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "verify", UC_FIELD_BOOL, 0, 0, 0, yes_no_choices, 2 }
};
static const UciTestFieldSpec f_soft_save[] = {
    { "verify", UC_FIELD_BOOL, 0, 0, 0, yes_no_choices, 2 },
    { "sa", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "start", UC_FIELD_WORD, 0x0801u, 0, 0, 0, 0 },
    { "end", UC_FIELD_WORD, 0x0900u, 0, 0, 0, 0 },
    { "name", UC_FIELD_TEXT, 0, 0, "ucisave", 0, 0 }
};
static const UciTestFieldSpec f_soft_open[] = {
    { "sa", UC_FIELD_BYTE, 2u, 0, 0, 0, 0 },
    { "", UC_FIELD_CONST, 0x00u, 0, 0, 0, 0 },
    { "name", UC_FIELD_TEXT, 0, 0, "$", 0, 0 }
};
static const UciTestFieldSpec f_sec[] = {
    { "sa", UC_FIELD_BYTE, 2u, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_soft_chkout[] = {
    { "sa", UC_FIELD_BYTE, 2u, 0, 0, 0, 0 },
    { "", UC_FIELD_CONST, 0x00u, 0, 0, 0, 0 },
    { "data", UC_FIELD_TEXT, 0, 0, "hello from ucitest", 0, 0 }
};
static const UciTestFieldSpec f_soft_partition[] = {
    { "index", UC_FIELD_BYTE, 3u, 0, 0, 0, 0 },
    { "map", UC_FIELD_TEXT, 0, 0, "READYOS:/usb1", 0, 0 }
};
static const UciTestFieldSpec f_partition_index[] = {
    { "index", UC_FIELD_BYTE, 3u, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_soft_name[] = {
    { "channel", UC_FIELD_BYTE, 0u, 0, 0, 0, 0 },
    { "iec name", UC_FIELD_TEXT, 0, 0, "READYOS.D81", 0, 0 }
};
static const UciTestFieldSpec f_soft_fat_name[] = {
    { "fat name", UC_FIELD_TEXT, 0, 0, "readyos.d81", 0, 0 }
};
static const UciTestFieldSpec f_http_header_create[] = {
    { "verb", UC_FIELD_BYTE, 1u, 0, 0, http_verb_choices, 10 },
    { "url", UC_FIELD_TEXT, 0, 0, "example.com/", 0, 0 }
};
static const UciTestFieldSpec f_handle[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_header_add[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "line", UC_FIELD_TEXT, 0, 0, "User-Agent: ucitest", 0, 0 }
};
static const UciTestFieldSpec f_header_query[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "key", UC_FIELD_TEXT, 0, 0, "Content-Type", 0, 0 }
};
static const UciTestFieldSpec f_header_list[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "index", UC_FIELD_BYTE, 0, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_body_create[] = {
    { "format", UC_FIELD_BYTE, 2u, 0, 0, http_body_choices, 5 }
};
static const UciTestFieldSpec f_body_add_raw[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "items", UC_FIELD_RAW, 0, 0, "05 77 69 64 74 68 01 F4 01 00 00", 0, 0 }
};
static const UciTestFieldSpec f_body_add_int[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "key", UC_FIELD_KEY, 0, 0, "width", 0, 0 },
    { "value", UC_FIELD_DWORD, 500u, 0, 0, 0, 0 }
};
static const UciTestFieldSpec f_body_add_bool[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "key", UC_FIELD_KEY, 0, 0, "visible", 0, 0 },
    { "value", UC_FIELD_BOOL, 1u, 0, 0, yes_no_choices, 2 }
};
static const UciTestFieldSpec f_body_add_string[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "key", UC_FIELD_KEY, 0, 0, "title", 0, 0 },
    { "value", UC_FIELD_LTEXT, 0, 0, "Commodore", 0, 0 }
};
static const UciTestFieldSpec f_body_add_obj[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "key", UC_FIELD_KEY, 0, 0, "user", 0, 0 }
};
static const UciTestFieldSpec f_body_path[] = {
    { "handle", UC_FIELD_BYTE, 0, 0, 0, 0, 0 },
    { "path", UC_FIELD_TEXT, 0, 0, "user/name", 0, 0 }
};
static const UciTestFieldSpec f_http_exchange[] = {
    { "header", UC_FIELD_BYTE, 0u, 0, 0, 0, 0 },
    { "body", UC_FIELD_BYTE, 0xFFu, 0, 0, 0, 0 }
};

const UciTestCommandSpec ucitest_commands[] = {
    { "detect", UC_KIND_TRANSPORT, UC_SPECIAL_DETECT, UC_FLAG_SPECIAL, UC_DEC_TEXT, 0, 0 },
    { "read id", UC_KIND_TRANSPORT, UC_SPECIAL_ID, UC_FLAG_SPECIAL, UC_DEC_HEX, 0, 0 },
    { "read status", UC_KIND_TRANSPORT, UC_SPECIAL_STATUS, UC_FLAG_SPECIAL, UC_DEC_HEX, 0, 0 },
    { "abort", UC_KIND_TRANSPORT, UC_SPECIAL_ABORT, UC_FLAG_SPECIAL | UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "clear error", UC_KIND_TRANSPORT, UC_SPECIAL_CLEAR, UC_FLAG_SPECIAL | UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "protocol norms", UC_KIND_TRANSPORT, UC_SPECIAL_NORMS, UC_FLAG_SPECIAL, UC_DEC_TEXT, 0, 0 },
    { "raw bytes", UC_KIND_TRANSPORT, UC_SPECIAL_RAW, UC_FLAG_SPECIAL, UC_DEC_HEX, f_raw, 1 },

    { "identify", UC_KIND_DOS, 0x01u, 0, UC_DEC_TEXT, 0, 0 },
    { "open file", UC_KIND_DOS, 0x02u, UC_FLAG_HANDLE, UC_DEC_TEXT, f_attr_file, 2 },
    { "close file", UC_KIND_DOS, 0x03u, UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "read data", UC_KIND_DOS, 0x04u, 0, UC_DEC_TEXT, f_len, 1 },
    { "write data", UC_KIND_DOS, 0x05u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_write_data, 3 },
    { "file seek", UC_KIND_DOS, 0x06u, 0, UC_DEC_TEXT, f_seek, 1 },
    { "file info", UC_KIND_DOS, 0x07u, 0, UC_DEC_FILE_INFO, 0, 0 },
    { "file stat", UC_KIND_DOS, 0x08u, 0, UC_DEC_FILE_INFO, f_filename, 1 },
    { "delete", UC_KIND_DOS, 0x09u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_filename, 1 },
    { "rename", UC_KIND_DOS, 0x0Au, UC_FLAG_MUTATING, UC_DEC_TEXT, f_rename, 2 },
    { "copy", UC_KIND_DOS, 0x0Bu, UC_FLAG_MUTATING | UC_FLAG_LONG, UC_DEC_TEXT, f_copy, 2 },
    { "change dir", UC_KIND_DOS, 0x11u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_dirname, 1 },
    { "get path", UC_KIND_DOS, 0x12u, 0, UC_DEC_TEXT, 0, 0 },
    { "open dir", UC_KIND_DOS, 0x13u, UC_FLAG_HANDLE, UC_DEC_TEXT, 0, 0 },
    { "read dir", UC_KIND_DOS, 0x14u, 0, UC_DEC_DIR, 0, 0 },
    { "copy ui path", UC_KIND_DOS, 0x15u, UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "create dir", UC_KIND_DOS, 0x16u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_dirname, 1 },
    { "copy home", UC_KIND_DOS, 0x17u, UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "load reu", UC_KIND_DOS, 0x21u, UC_FLAG_MUTATING | UC_FLAG_LONG, UC_DEC_TEXT, f_reu_addr_len, 2 },
    { "save reu", UC_KIND_DOS, 0x22u, UC_FLAG_MUTATING | UC_FLAG_LONG, UC_DEC_TEXT, f_reu_addr_len, 2 },
    { "mount disk", UC_KIND_DOS, 0x23u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_drive_name, 2 },
    { "umount disk", UC_KIND_DOS, 0x24u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_drive, 1 },
    { "swap disk", UC_KIND_DOS, 0x25u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_drive, 1 },
    { "get time", UC_KIND_DOS, 0x26u, 0, UC_DEC_TEXT, f_time_format, 1 },
    { "set time", UC_KIND_DOS, 0x27u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_set_time, 6 },
    { "echo", UC_KIND_DOS, 0xF0u, 0, UC_DEC_HEX, 0, 0 },

    { "identify", UC_KIND_NETWORK, 0x01u, UC_FLAG_NETWORK, UC_DEC_TEXT, 0, 0 },
    { "iface count", UC_KIND_NETWORK, 0x02u, UC_FLAG_NETWORK, UC_DEC_HEX, 0, 0 },
    { "get mac", UC_KIND_NETWORK, 0x04u, UC_FLAG_NETWORK, UC_DEC_MAC, f_iface, 1 },
    { "get ip", UC_KIND_NETWORK, 0x05u, UC_FLAG_NETWORK, UC_DEC_IP, f_iface, 1 },
    { "set ip", UC_KIND_NETWORK, 0x06u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_ip_set, 1 },
    { "open tcp", UC_KIND_NETWORK, 0x07u, UC_FLAG_NETWORK | UC_FLAG_HANDLE, UC_DEC_HANDLE, f_open_net, 2 },
    { "open udp", UC_KIND_NETWORK, 0x08u, UC_FLAG_NETWORK | UC_FLAG_HANDLE, UC_DEC_HANDLE, f_open_net, 2 },
    { "close socket", UC_KIND_NETWORK, 0x09u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_sock, 1 },
    { "read socket", UC_KIND_NETWORK, 0x10u, UC_FLAG_NETWORK, UC_DEC_SOCKET_READ, f_sock_read, 2 },
    { "write socket", UC_KIND_NETWORK, 0x11u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_WORD, f_sock_write, 2 },

    { "identify", UC_KIND_CONTROL, 0x01u, 0, UC_DEC_TEXT, 0, 0 },
    { "finish tape", UC_KIND_CONTROL, 0x03u, UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "freeze", UC_KIND_CONTROL, 0x05u, UC_FLAG_MUTATING | UC_FLAG_REBOOTS, UC_DEC_TEXT, 0, 0 },
    { "reboot", UC_KIND_CONTROL, 0x06u, UC_FLAG_MUTATING | UC_FLAG_REBOOTS, UC_DEC_TEXT, 0, 0 },
    { "load reu file", UC_KIND_CONTROL, 0x08u, UC_FLAG_MUTATING | UC_FLAG_LONG, UC_DEC_DWORD, f_drive_file, 1 },
    { "save reu file", UC_KIND_CONTROL, 0x09u, UC_FLAG_MUTATING | UC_FLAG_LONG, UC_DEC_DWORD, f_drive_file, 1 },
    { "u64 savemem", UC_KIND_CONTROL, 0x0Fu, UC_FLAG_MUTATING | UC_FLAG_LONG, UC_DEC_TEXT, f_drive_file, 1 },
    { "decode track", UC_KIND_CONTROL, 0x11u, UC_FLAG_MUTATING, UC_DEC_HEX, f_decode_track, 1 },
    { "enable drive a", UC_KIND_CONTROL, 0x30u, UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "disable drive a", UC_KIND_CONTROL, 0x31u, UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "enable drive b", UC_KIND_CONTROL, 0x32u, UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "disable drive b", UC_KIND_CONTROL, 0x33u, UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "power drive a", UC_KIND_CONTROL, 0x34u, 0, UC_DEC_TEXT, 0, 0 },
    { "power drive b", UC_KIND_CONTROL, 0x35u, 0, UC_DEC_TEXT, 0, 0 },
    { "ramdisk info", UC_KIND_CONTROL, 0x40u, 0, UC_DEC_HEX, 0, 0 },

    { "identify", UC_KIND_SOFTIEC, 0x01u, 0, UC_DEC_TEXT, 0, 0 },
    { "load setup", UC_KIND_SOFTIEC, 0x10u, UC_FLAG_HANDLE, UC_DEC_HEX, f_soft_load_su, 4 },
    { "load exec", UC_KIND_SOFTIEC, 0x11u, UC_FLAG_MUTATING, UC_DEC_HEX, f_soft_load_ex, 2 },
    { "save", UC_KIND_SOFTIEC, 0x12u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_soft_save, 5 },
    { "open", UC_KIND_SOFTIEC, 0x13u, UC_FLAG_HANDLE, UC_DEC_TEXT, f_soft_open, 3 },
    { "close", UC_KIND_SOFTIEC, 0x14u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_sec, 1 },
    { "chkin", UC_KIND_SOFTIEC, 0x15u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_sec, 1 },
    { "chkout", UC_KIND_SOFTIEC, 0x16u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_soft_chkout, 3 },
    { "add partition", UC_KIND_SOFTIEC, 0x20u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_soft_partition, 2 },
    { "del partition", UC_KIND_SOFTIEC, 0x21u, UC_FLAG_MUTATING, UC_DEC_TEXT, f_partition_index, 1 },
    { "get fat name", UC_KIND_SOFTIEC, 0x22u, 0, UC_DEC_TEXT, f_soft_name, 2 },
    { "get iec name", UC_KIND_SOFTIEC, 0x23u, 0, UC_DEC_IEC_NAME, f_soft_fat_name, 1 },

    { "identify", UC_KIND_HTTP, 0x01u, UC_FLAG_NETWORK, UC_DEC_TEXT, 0, 0 },
    { "free all", UC_KIND_HTTP, 0x10u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, 0, 0 },
    { "header create", UC_KIND_HTTP, 0x11u, UC_FLAG_NETWORK | UC_FLAG_HANDLE, UC_DEC_HANDLE, f_http_header_create, 2 },
    { "header free", UC_KIND_HTTP, 0x12u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_handle, 1 },
    { "header add", UC_KIND_HTTP, 0x13u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_header_add, 2 },
    { "header query", UC_KIND_HTTP, 0x14u, UC_FLAG_NETWORK, UC_DEC_TEXT, f_header_query, 2 },
    { "header list", UC_KIND_HTTP, 0x15u, UC_FLAG_NETWORK, UC_DEC_TEXT, f_header_list, 2 },
    { "body create", UC_KIND_HTTP, 0x21u, UC_FLAG_NETWORK | UC_FLAG_HANDLE, UC_DEC_HANDLE, f_body_create, 1 },
    { "body free", UC_KIND_HTTP, 0x22u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_handle, 1 },
    { "body add", UC_KIND_HTTP, 0x2Du, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_body_add_raw, 2 },
    { "add int", UC_KIND_HTTP, 0x23u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_body_add_int, 3 },
    { "add bool", UC_KIND_HTTP, 0x24u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_body_add_bool, 3 },
    { "add string", UC_KIND_HTTP, 0x25u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_body_add_string, 3 },
    { "add object", UC_KIND_HTTP, 0x26u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_body_add_obj, 2 },
    { "add array", UC_KIND_HTTP, 0x27u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_body_add_obj, 2 },
    { "body up", UC_KIND_HTTP, 0x28u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_handle, 1 },
    { "body remove", UC_KIND_HTTP, 0x29u, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_body_path, 2 },
    { "body query", UC_KIND_HTTP, 0x2Au, UC_FLAG_NETWORK, UC_DEC_HTTP_VALUE, f_body_path, 2 },
    { "body move", UC_KIND_HTTP, 0x2Bu, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_body_path, 2 },
    { "add binary", UC_KIND_HTTP, 0x2Cu, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_body_add_raw, 2 },
    { "body clear", UC_KIND_HTTP, 0x2Eu, UC_FLAG_NETWORK | UC_FLAG_MUTATING, UC_DEC_TEXT, f_handle, 1 },
    { "exchange obj", UC_KIND_HTTP, 0x31u, UC_FLAG_NETWORK | UC_FLAG_MUTATING | UC_FLAG_LONG, UC_DEC_HTTP_HANDLES, f_http_exchange, 2 },
    { "exchange raw", UC_KIND_HTTP, 0x32u, UC_FLAG_NETWORK | UC_FLAG_MUTATING | UC_FLAG_LONG, UC_DEC_TEXT, f_http_exchange, 2 }
};

const unsigned char ucitest_command_count =
    sizeof(ucitest_commands) / sizeof(ucitest_commands[0]);

const UciTestExampleSpec ucitest_examples[] = {
    { "detect uci interface", "Safe first check.", "Shows the mapped register base.",
      UC_KIND_TRANSPORT, UC_SPECIAL_DETECT, 0u, 0u, 0u, 0, 0 },
    { "explain uci protocol", "Read the transport rules in-app.", "Use F6 and up/down to scroll.",
      UC_KIND_TRANSPORT, UC_SPECIAL_NORMS, 0u, 0u, 0u, 0, 0 },
    { "identify ultimate dos", "Safe target discovery.", "Expected: Ultimate DOS version text.",
      UC_KIND_DOS, 0x01u, 0u, 0u, 0u, 0, 0 },
    { "show current dos path", "Safe filesystem query.", "DOS 1 and DOS 2 keep separate state.",
      UC_KIND_DOS, 0x12u, 0u, 0u, 0u, 0, 0 },
    { "inspect readyos image", "Prefills FILE STAT for the D81.", "Edit the path if your image moved.",
      UC_KIND_DOS, 0x08u, 0u, 0u, 0u, "/usb1/readyos.d81", 0 },
    { "show network address", "Prefills interface zero.", "Returns IP, mask, and gateway.",
      UC_KIND_NETWORK, 0x05u, 0x01u, 0u, 0u, 0, 0 },
    { "open tcp example.com", "Prefills TCP port 80 and host.", "A returned socket handle is remembered.",
      UC_KIND_NETWORK, 0x07u, 0x01u, 80u, 0u, 0, "example.com" },
    { "prepare http get", "Prefills GET example.com/.", "Run FREE ALL first on a fresh session.",
      UC_KIND_HTTP, 0x11u, 0x01u, 1u, 0u, 0, "example.com/" },
    { "query last http body", "Uses the remembered body handle.", "An empty path queries the root value.",
      UC_KIND_HTTP, 0x2Au, 0u, 0u, 0u, 0, "" },
    { "raw control identify", "Prefills bytes 04 01.", "Same request as Control / Identify.",
      UC_KIND_TRANSPORT, UC_SPECIAL_RAW, 0u, 0u, 0u, "04 01", 0 }
};

const unsigned char ucitest_example_count =
    sizeof(ucitest_examples) / sizeof(ucitest_examples[0]);

unsigned char ucitest_command_count_for_kind(unsigned char kind) {
    unsigned char i;
    unsigned char count;

    count = 0u;
    for (i = 0u; i < ucitest_command_count; ++i) {
        if (ucitest_commands[i].kind == kind) {
            ++count;
        }
    }
    return count;
}

unsigned char ucitest_command_index_for_kind(unsigned char kind,
                                             unsigned char rel_index) {
    unsigned char i;
    unsigned char count;

    count = 0u;
    for (i = 0u; i < ucitest_command_count; ++i) {
        if (ucitest_commands[i].kind == kind) {
            if (count == rel_index) {
                return i;
            }
            ++count;
        }
    }
    return 0u;
}

unsigned char ucitest_command_rel_for_kind_cmd(unsigned char kind,
                                               unsigned char cmd) {
    unsigned char i;
    unsigned char rel;

    rel = 0u;
    for (i = 0u; i < ucitest_command_count; ++i) {
        if (ucitest_commands[i].kind != kind) {
            continue;
        }
        if (ucitest_commands[i].cmd == cmd) {
            return rel;
        }
        ++rel;
    }
    return 0u;
}

const char *ucitest_flag_text(unsigned char flags) {
    if ((flags & UC_FLAG_REBOOTS) != 0u) {
        return "reboot";
    }
    if ((flags & UC_FLAG_MUTATING) != 0u) {
        return "writes";
    }
    if ((flags & UC_FLAG_NETWORK) != 0u) {
        return "net";
    }
    if ((flags & UC_FLAG_LONG) != 0u) {
        return "long";
    }
    if ((flags & UC_FLAG_HANDLE) != 0u) {
        return "handle";
    }
    return "read";
}
