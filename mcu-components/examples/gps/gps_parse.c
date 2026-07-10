#include "GPS_Parse.h"
#include <string.h>
#include <stdio.h>

#define GPS_LINE_MAX 128

static char     gLineBuf[GPS_LINE_MAX];
static uint8_t  gLineIdx  = 0;
static GpsData_t gGps;
static uint8_t  gUpdated  = 0;

/* diagnostic counters */
uint16_t GPS_ByteCount  = 0;
uint16_t GPS_LineCount  = 0;
uint8_t  GPS_CksumFail  = 0;
uint8_t  GPS_ParseOk    = 0;
char     GPS_RawSnippet[17] = {0};
char     GPS_LastSentId[7]  = {0};
static uint8_t  gRawDone = 0;

/* ── helpers ────────────────────────────────────────────── */

static const char *next_field(const char *p)
{
    while (*p && *p != ',' && *p != '*') p++;
    return (*p == ',') ? p + 1 : (void*)0;
}

static int32_t parse_int(const char **pp)
{
    int32_t v = 0;
    while (**pp >= '0' && **pp <= '9') {
        v = v * 10 + (**pp - '0');
        (*pp)++;
    }
    return v;
}

/*
 * Parse NMEA coordinate "ddmm.mmmm" (deg_digits=2) or "dddmm.mmmm" (3).
 * Returns microdegrees = degrees * 1 000 000, always >= 0.
 * Caller applies sign based on N/S or E/W indicator.
 */
static int32_t parse_coord(const char *f, uint8_t deg_digits)
{
    int32_t deg = 0, min_int = 0, min_frac = 0;
    uint8_t fd = 0, i;
    const char *p = f;

    if (!f || *f == ',' || *f == '\0') return 0;

    for (i = 0; i < deg_digits && *p >= '0' && *p <= '9'; i++)
        deg = deg * 10 + (*p++ - '0');

    while (*p >= '0' && *p <= '9')
        min_int = min_int * 10 + (*p++ - '0');

    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9' && fd < 4) {
            min_frac = min_frac * 10 + (*p++ - '0');
            fd++;
        }
    }
    while (fd < 4) { min_frac *= 10; fd++; }

    /* min_10000 = minutes * 10000;  convert to microdeg: * 10 / 6 */
    return deg * 1000000L + ((int32_t)min_int * 10000L + min_frac) * 10L / 6;
}

/* Parse "x.xx" -> value * 100 */
static uint16_t parse_fixed2(const char *f)
{
    int32_t ip = 0, fp = 0;
    uint8_t fd = 0;
    const char *p = f;

    if (!f || *f == ',' || *f == '\0') return 0;

    while (*p >= '0' && *p <= '9') ip = ip * 10 + (*p++ - '0');
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9' && fd < 2) {
            fp = fp * 10 + (*p++ - '0'); fd++;
        }
    }
    while (fd < 2) { fp *= 10; fd++; }
    return (uint16_t)(ip * 100 + fp);
}

/* Parse altitude in cm, handles negative */
static int32_t parse_alt_cm(const char *f)
{
    int32_t ip = 0, fp = 0;
    uint8_t fd = 0;
    int8_t sign = 1;
    const char *p = f;

    if (!f || *f == ',' || *f == '\0') return 0;
    if (*p == '-') { sign = -1; p++; }

    while (*p >= '0' && *p <= '9') ip = ip * 10 + (*p++ - '0');
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9' && fd < 2) {
            fp = fp * 10 + (*p++ - '0'); fd++;
        }
    }
    while (fd < 2) { fp *= 10; fd++; }
    return sign * (ip * 100 + fp);
}

static uint8_t verify_checksum(const char *line)
{
    uint8_t ck = 0, exp_val;
    const char *p;

    if (*line != '$') return 0;
    for (p = line + 1; *p && *p != '*'; p++)
        ck ^= (uint8_t)*p;

    if (*p != '*') return 0; /* no checksum → reject (NMEA always has *XX) */
    p++;

    exp_val = 0;
    if      (*p >= '0' && *p <= '9') exp_val = (uint8_t)(*p - '0') << 4;
    else if (*p >= 'A' && *p <= 'F') exp_val = (uint8_t)(*p - 'A' + 10) << 4;
    else if (*p >= 'a' && *p <= 'f') exp_val = (uint8_t)(*p - 'a' + 10) << 4;
    else return 0;
    p++;
    if      (*p >= '0' && *p <= '9') exp_val |= (uint8_t)(*p - '0');
    else if (*p >= 'A' && *p <= 'F') exp_val |= (uint8_t)(*p - 'A' + 10);
    else if (*p >= 'a' && *p <= 'f') exp_val |= (uint8_t)(*p - 'a' + 10);
    else return 0;

    return (ck == exp_val) ? 1u : 0u;
}

/* ── sentence parsers ───────────────────────────────────── */

/* $G?RMC,time,status,lat,N/S,lng,E/W,speed,course,date,... */
static void parse_rmc(const char *line)
{
    const char *f = line;
    const char *lat_f, *ns_f, *lng_f, *ew_f, *sta_f, *spd_f;

    f = next_field(f); if (!f) return; /* 1 time   */
    f = next_field(f); if (!f) return; /* 2 status */  sta_f = f;
    f = next_field(f); if (!f) return; /* 3 lat    */  lat_f = f;
    f = next_field(f); if (!f) return; /* 4 N/S    */  ns_f  = f;
    f = next_field(f); if (!f) return; /* 5 lng    */  lng_f = f;
    f = next_field(f); if (!f) return; /* 6 E/W    */  ew_f  = f;
    f = next_field(f); if (!f) return; /* 7 speed  */  spd_f = f;

    if (*sta_f != 'A') { gGps.valid = 0; return; }

    gGps.lat   = parse_coord(lat_f, 2);
    if (*ns_f == 'S') gGps.lat = -gGps.lat;

    gGps.lng   = parse_coord(lng_f, 3);
    if (*ew_f == 'W') gGps.lng = -gGps.lng;

    gGps.speed = parse_fixed2(spd_f);
    gGps.valid = 1;
    gUpdated   = 1;
}

/* $G?GGA,time,lat,N/S,lng,E/W,fix,sat,hdop,alt,M,... */
static void parse_gga(const char *line)
{
    const char *f = line;
    const char *fix_f, *sat_f, *alt_f;
    const char *p;

    f = next_field(f); if (!f) return; /* 1 time */
    f = next_field(f); if (!f) return; /* 2 lat  */
    f = next_field(f); if (!f) return; /* 3 N/S  */
    f = next_field(f); if (!f) return; /* 4 lng  */
    f = next_field(f); if (!f) return; /* 5 E/W  */
    f = next_field(f); if (!f) return; /* 6 fix  */  fix_f = f;
    f = next_field(f); if (!f) return; /* 7 sat  */  sat_f = f;
    f = next_field(f); if (!f) return; /* 8 hdop */
    f = next_field(f); if (!f) return; /* 9 alt  */  alt_f = f;

    if (*fix_f >= '0' && *fix_f <= '9')
        gGps.fix = (uint8_t)(*fix_f - '0');

    p = sat_f;
    gGps.satellites = (uint8_t)parse_int(&p);
    gGps.alt = parse_alt_cm(alt_f);
}

/* ── line processing ────────────────────────────────────── */

static void process_line(void)
{
    if (gLineIdx < 6) return;
    gLineBuf[gLineIdx] = '\0';

    GPS_LineCount++;

    if (!verify_checksum(gLineBuf)) { GPS_CksumFail++; return; }

    GPS_ParseOk++;

    /* Save sentence ID for debug (e.g. "$GPGGA" → "GPGGA\0") */
    if (gLineIdx >= 6) {
        uint8_t j;
        for (j = 0; j < 6 && gLineBuf[j] != ','; j++)
            GPS_LastSentId[j] = gLineBuf[j];
        GPS_LastSentId[j] = '\0';
    }

    /* Match any talker: $GPRMC / $GNRMC / $BDRMC / … */
    if (gLineBuf[3] == 'R' && gLineBuf[4] == 'M' && gLineBuf[5] == 'C')
        parse_rmc(gLineBuf);
    else if (gLineBuf[3] == 'G' && gLineBuf[4] == 'G' && gLineBuf[5] == 'A')
        parse_gga(gLineBuf);
}

/* ── public API ─────────────────────────────────────────── */

void GPS_Parse_Init(void)
{
    memset(&gGps, 0, sizeof(gGps));
    gLineIdx = 0;
    gUpdated = 0;
    GPS_ByteCount  = 0;
    GPS_LineCount  = 0;
    GPS_CksumFail  = 0;
    GPS_ParseOk    = 0;
}

void GPS_Parse_Feed(uint8_t byte)
{
    GPS_ByteCount++;

    /* Capture first 16 printable bytes as raw snippet for OLED debug */
    if (!gRawDone && GPS_ByteCount <= 16u) {
        char c = (char)byte;
        if (c >= 0x20 && c <= 0x7E)
            GPS_RawSnippet[GPS_ByteCount - 1] = c;
        else
            GPS_RawSnippet[GPS_ByteCount - 1] = '?';
        GPS_RawSnippet[GPS_ByteCount] = '\0';
        if (GPS_ByteCount == 16u) gRawDone = 1;
    }

    if (byte == '$')
        gLineIdx = 0;

    if (gLineIdx < GPS_LINE_MAX - 1)
        gLineBuf[gLineIdx++] = (char)byte;

    if (byte == '\n') {
        process_line();
        gLineIdx = 0;
    }
}

const GpsData_t *GPS_Parse_GetData(void) { return &gGps; }

uint8_t GPS_Parse_IsUpdated(void)
{
    if (gUpdated) { gUpdated = 0; return 1; }
    return 0;
}

/*
 * Format GPS data as JSON using only integer format specifiers
 * (avoids pulling in the heavy floating-point printf library on Cortex-M3).
 */
uint16_t GPS_Format_Json(char *buf, uint16_t size, const GpsData_t *g)
{
    int32_t la, lo, aa;

    la = g->lat < 0 ? -g->lat : g->lat;
    lo = g->lng < 0 ? -g->lng : g->lng;
    aa = g->alt < 0 ? -g->alt : g->alt;

    return (uint16_t)snprintf(buf, size,
        "{\"lat\":%s%ld.%06ld,\"lng\":%s%ld.%06ld,"
        "\"spd\":%u.%02u,\"alt\":%s%ld.%02ld,"
        "\"sat\":%u,\"fix\":%u}",
        (g->lat < 0 ? "-" : ""), (long)(la / 1000000L), (long)(la % 1000000L),
        (g->lng < 0 ? "-" : ""), (long)(lo / 1000000L), (long)(lo % 1000000L),
        (unsigned)(g->speed / 100u), (unsigned)(g->speed % 100u),
        (g->alt < 0 ? "-" : ""), (long)(aa / 100L), (long)(aa % 100L),
        (unsigned)g->satellites, (unsigned)g->fix);
}
