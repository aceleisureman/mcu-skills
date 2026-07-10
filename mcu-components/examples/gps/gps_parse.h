#ifndef __GPS_PARSE_H
#define __GPS_PARSE_H

#include <stdint.h>

typedef struct {
    int32_t  lat;        /* microdegrees (deg * 1000000), negative = S */
    int32_t  lng;        /* microdegrees (deg * 1000000), negative = W */
    uint16_t speed;      /* knots * 100 */
    int32_t  alt;        /* altitude in centimeters */
    uint8_t  satellites;
    uint8_t  fix;        /* 0=none 1=GPS 2=DGPS */
    uint8_t  valid;      /* 1 when $GPRMC status='A' */
} GpsData_t;

/* diagnostic counters (readable from NetStatus for OLED display) */
extern uint16_t GPS_ByteCount;
extern uint16_t GPS_LineCount;
extern uint8_t  GPS_CksumFail;
extern uint8_t  GPS_ParseOk;
extern char     GPS_RawSnippet[17];
extern char     GPS_LastSentId[7];

void            GPS_Parse_Init(void);
void            GPS_Parse_Feed(uint8_t byte);
const GpsData_t*GPS_Parse_GetData(void);
uint8_t         GPS_Parse_IsUpdated(void);
uint16_t        GPS_Format_Json(char *buf, uint16_t size, const GpsData_t *g);

#endif
