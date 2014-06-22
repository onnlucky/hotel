// author: Onne Gorter, license: MIT (see license.txt)
// a time module

#include "trace-off.h"

static tlSym _s_sec;
static tlSym _s_min;
static tlSym _s_hour;
static tlSym _s_mday;
static tlSym _s_mon;
static tlSym _s_year;
static tlSym _s_wday;
static tlSym _s_yday;
static tlSym _s_isdst;
static tlSym _s_zone;
static tlSym _s_gmtoff;

static tlMap* _timeMap;

static tlHandle _Date(tlArgs* args) {
    time_t time;
    if (tlArgsSize(args) > 0) {
        time = tl_double(tlArgsGet(args, 0));
    } else {
        struct timeval tv;
        gettimeofday(&tv, null);
        time = tv.tv_sec;
    }
    struct tm tm;
    localtime_r(&time, &tm);

    // TODO add tv using a float ...
    tlMap *res = tlClone(_timeMap);
    tlMapSetSym_(res, _s_sec, tlINT(tm.tm_sec));
    tlMapSetSym_(res, _s_min, tlINT(tm.tm_min));
    tlMapSetSym_(res, _s_hour, tlINT(tm.tm_hour));
    tlMapSetSym_(res, _s_mon, tlINT(tm.tm_mon));
    tlMapSetSym_(res, _s_year, tlINT(tm.tm_year));
    tlMapSetSym_(res, _s_wday, tlINT(tm.tm_wday));
    tlMapSetSym_(res, _s_mday, tlINT(tm.tm_mday));
    tlMapSetSym_(res, _s_yday, tlINT(tm.tm_yday));
    tlMapSetSym_(res, _s_isdst, tlINT(tm.tm_isdst));
    tlMapSetSym_(res, _s_zone, tlStringFromCopy(tm.tm_zone, 0));
    tlMapSetSym_(res, _s_gmtoff, tlINT(tm.tm_gmtoff));
    return res;
}

static tlHandle _seconds(tlArgs* args) {
    if (tlArgsSize(args) == 0) {
        struct timeval tv;
        gettimeofday(&tv, null);
        double seconds = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
        return tlFLOAT(seconds);
    }

    tlMap* from = tlMapOrObjectCast(tlArgsGet(args, 0));
    if (!from) TL_THROW("expect a map");
    struct tm tm;
    tm.tm_sec = tl_int(tlMapGet(from, _s_sec));
    tm.tm_min = tl_int(tlMapGet(from, _s_min));
    tm.tm_hour = tl_int(tlMapGet(from, _s_hour));
    tm.tm_mon = tl_int(tlMapGet(from, _s_mon));
    tm.tm_year = tl_int(tlMapGet(from, _s_year));
    tm.tm_wday = tl_int(tlMapGet(from, _s_wday));
    tm.tm_mday = tl_int(tlMapGet(from, _s_mday));
    tm.tm_yday = tl_int(tlMapGet(from, _s_yday));
    tm.tm_isdst = tl_int(tlMapGet(from, _s_isdst));
    tlString* zone = tlStringCast(tlMapGet(from, _s_isdst));
    if (zone) tm.tm_zone = (char*)tlStringData(zone);
    tm.tm_gmtoff = tl_int(tlMapGet(from, _s_gmtoff));
    return tlFLOAT(mktime(&tm));
}

static void time_init() {
    tlSet* keys = tlSetNew(11);
    _s_sec = tlSYM("sec"); tlSetAdd_(keys, _s_sec);
    _s_min = tlSYM("min"); tlSetAdd_(keys, _s_min);
    _s_hour = tlSYM("hour"); tlSetAdd_(keys, _s_hour);
    _s_mon = tlSYM("month"); tlSetAdd_(keys, _s_mon);
    _s_year = tlSYM("year"); tlSetAdd_(keys, _s_year);
    _s_wday = tlSYM("wday"); tlSetAdd_(keys, _s_wday);
    _s_mday = tlSYM("mday"); tlSetAdd_(keys, _s_mday);
    _s_yday = tlSYM("yday"); tlSetAdd_(keys, _s_yday);
    _s_isdst = tlSYM("isdst"); tlSetAdd_(keys, _s_isdst);
    _s_zone = tlSYM("zone"); tlSetAdd_(keys, _s_zone);
    _s_gmtoff = tlSYM("gmtoff"); tlSetAdd_(keys, _s_gmtoff);
    _timeMap = tlMapNew(keys);
    tlMapToObject_(_timeMap);

    tl_register_global("Date", tlNATIVE(_Date, "Date"));
    tl_register_global("seconds", tlNATIVE(_seconds, "seconds"));
}

