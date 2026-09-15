#ifndef TC_PROTOBUF_IMPLEMENTATION
#define TC_PROTOBUF_IMPLEMENTATION
#include "library.c"

/* All views borrow the input. Writers own their buffer until take/destroy. */
#define TC_PB_LIMIT (64u * 1024u * 1024u)
int32_t tc_pb_valid_utf8(TinyString text) {
    size_t p = 0;
    if (!text.data && text.length)
        return 0;
    while (p < text.length) {
        unsigned char byte = (unsigned char)text.data[p++];
        uint32_t value;
        int count, i;
        if (byte < 128)
            continue;
        if (byte >= 0xc2 && byte <= 0xdf) {
            count = 1;
            value = byte & 31;
        } else if (byte >= 0xe0 && byte <= 0xef) {
            count = 2;
            value = byte & 15;
        } else if (byte >= 0xf0 && byte <= 0xf4) {
            count = 3;
            value = byte & 7;
        } else
            return 0;
        if ((size_t)count > text.length - p)
            return 0;
        for (i = 0; i < count; i++) {
            byte = (unsigned char)text.data[p++];
            if ((byte & 0xc0) != 0x80)
                return 0;
            value = (value << 6) | (byte & 63);
        }
        if ((count == 2 && value < 2048) || (count == 3 && value < 65536) || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff))
            return 0;
    }
    return 1;
}
typedef struct TcPbWriter {
    unsigned char *data;
    size_t length, capacity;
    int error;
} TcPbWriter;
typedef struct TcPbReader {
    TinyString input;
    size_t position;
    uint32_t wire;
    int error;
} TcPbReader;
void *tc_pb_writer(void) {
    return tc_alloc_checked(sizeof(TcPbWriter));
}
void tc_pb_writer_destroy(void *p) {
    TcPbWriter *w = (TcPbWriter *)p;
    if (w) {
        free(w->data);
        free(w);
    }
}
static void tc_pb_append(TcPbWriter *w, const void *data, size_t length) {
    size_t capacity;
    unsigned char *p;
    if (w->error)
        return;
    if (length > TC_PB_LIMIT - w->length) {
        w->error = 8;
        return;
    }
    if (w->length + length > w->capacity) {
        capacity = w->capacity ? w->capacity : 128;
        while (capacity < w->length + length)
            capacity *= 2;
        p = (unsigned char *)realloc(w->data, capacity);
        if (!p) {
            w->error = 8;
            return;
        }
        w->data = p;
        w->capacity = capacity;
    }
    if (length)
        memcpy(w->data + w->length, data, length);
    w->length += length;
}
void tc_pb_write_varint(void *writer, uint64_t value) {
    unsigned char bytes[10];
    size_t count = 0;
    while (value > 127) {
        bytes[count++] = (unsigned char)((value & 127) | 128);
        value >>= 7;
    }
    bytes[count++] = (unsigned char)value;
    tc_pb_append((TcPbWriter *)writer, bytes, count);
}
void tc_pb_write_fixed(void *writer, uint64_t value, int32_t count) {
    unsigned char bytes[8];
    int i;
    if (count != 4 && count != 8) {
        ((TcPbWriter *)writer)->error = 3;
        return;
    }
    for (i = 0; i < count; i++) {
        bytes[i] = (unsigned char)value;
        value >>= 8;
    }
    tc_pb_append((TcPbWriter *)writer, bytes, (size_t)count);
}
void tc_pb_write_bytes(void *writer, TinyString bytes) {
    tc_pb_write_varint(writer, bytes.length);
    tc_pb_append((TcPbWriter *)writer, bytes.data, bytes.length);
}
int32_t tc_pb_writer_error(void *writer) {
    return ((TcPbWriter *)writer)->error;
}
TinyString tc_pb_writer_take(void *writer) {
    TcPbWriter *w = (TcPbWriter *)writer;
    TinyString result = {NULL, 0};
    if (w->error)
        return result;
    if (!w->data) {
        w->data = (unsigned char *)malloc(1);
        if (!w->data) {
            w->error = 8;
            return result;
        }
    }
    result.data = (const char *)w->data;
    result.length = w->length;
    w->data = NULL;
    w->length = w->capacity = 0;
    return result;
}
void *tc_pb_reader(TinyString input) {
    TcPbReader *r = (TcPbReader *)tc_alloc_checked(sizeof(*r));
    r->input = input;
    if (input.length > TC_PB_LIMIT || (!input.data && input.length))
        r->error = 3;
    return r;
}
void tc_pb_reader_destroy(void *reader) {
    free(reader);
}
int32_t tc_pb_reader_error(void *reader) {
    return ((TcPbReader *)reader)->error;
}
void tc_pb_reader_fail(void *reader) {
    ((TcPbReader *)reader)->error = 3;
}
int32_t tc_pb_reader_done(void *reader) {
    TcPbReader *r = (TcPbReader *)reader;
    return r->error || r->position == r->input.length;
}
uint64_t tc_pb_read_varint(void *reader) {
    TcPbReader *r = (TcPbReader *)reader;
    uint64_t value = 0;
    int i;
    if (r->error)
        return 0;
    for (i = 0; i < 10; i++) {
        unsigned char byte;
        if (r->position >= r->input.length)
            break;
        byte = (unsigned char)r->input.data[r->position++];
        if (i == 9 && byte > 1)
            break;
        value |= (uint64_t)(byte & 127) << (7 * i);
        if (!(byte & 128))
            return value;
    }
    r->error = 3;
    return 0;
}
uint64_t tc_pb_read_fixed(void *reader, int32_t count) {
    TcPbReader *r = (TcPbReader *)reader;
    uint64_t value = 0;
    int i;
    if (r->error)
        return 0;
    if ((count != 4 && count != 8) || (size_t)count > r->input.length - r->position) {
        r->error = 3;
        return 0;
    }
    for (i = 0; i < count; i++)
        value |= (uint64_t)(unsigned char)r->input.data[r->position++] << (8 * i);
    return value;
}
TinyString tc_pb_read_bytes(void *reader) {
    TcPbReader *r = (TcPbReader *)reader;
    uint64_t length = tc_pb_read_varint(reader);
    TinyString result = {NULL, 0};
    if (r->error || length > r->input.length - r->position) {
        r->error = 3;
        return result;
    }
    result.data = r->input.data + r->position;
    result.length = (size_t)length;
    r->position += (size_t)length;
    return result;
}
uint32_t tc_pb_next(void *reader) {
    TcPbReader *r = (TcPbReader *)reader;
    uint64_t tag;
    if (tc_pb_reader_done(reader))
        return 0;
    tag = tc_pb_read_varint(reader);
    r->wire = (uint32_t)tag & 7;
    if (r->error || tag >> 3 == 0 || tag >> 3 > 536870911 || r->wire > 5) {
        r->error = 3;
        return 0;
    }
    return (uint32_t)(tag >> 3);
}
uint32_t tc_pb_wire(void *reader) {
    return ((TcPbReader *)reader)->wire;
}
static void tc_pb_skip_depth(TcPbReader *r, uint32_t field, int depth) {
    uint32_t wire = r->wire;
    if (depth > 64) {
        r->error = 3;
        return;
    }
    if (wire == 0)
        tc_pb_read_varint(r);
    else if (wire == 1)
        tc_pb_read_fixed(r, 8);
    else if (wire == 2)
        tc_pb_read_bytes(r);
    else if (wire == 5)
        tc_pb_read_fixed(r, 4);
    else if (wire == 3) {
        while (!tc_pb_reader_done(r)) {
            uint32_t next = tc_pb_next(r);
            if (r->wire == 4) {
                if (field != next)
                    r->error = 3;
                return;
            }
            tc_pb_skip_depth(r, next, depth + 1);
        }
        r->error = 3;
    } else
        r->error = 3;
}
void tc_pb_skip(void *reader, uint32_t field) {
    tc_pb_skip_depth((TcPbReader *)reader, field, 0);
}
uint64_t tc_pb_zigzag(int64_t n) {
    return ((uint64_t)n << 1) ^ (uint64_t)-(n < 0);
}
int64_t tc_pb_unzigzag(uint64_t n) {
    return (int64_t)((n >> 1) ^ (uint64_t)-(int64_t)(n & 1));
}
uint64_t tc_pb_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, 8);
    return bits;
}
uint32_t tc_pb_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    return bits;
}
double tc_pb_bits_double(uint64_t bits) {
    double value;
    memcpy(&value, &bits, 8);
    return value;
}
float tc_pb_bits_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
#endif
