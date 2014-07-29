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

static tlObject* _timeMap;

// TODO add microseconds
static tlHandle object_from_tm(struct tm* tm) {
    tlObject *res = tlClone(_timeMap);
    tlObjectSet_(res, _s_sec, tlINT(tm->tm_sec));
    tlObjectSet_(res, _s_min, tlINT(tm->tm_min));
    tlObjectSet_(res, _s_hour, tlINT(tm->tm_hour));
    tlObjectSet_(res, _s_mon, tlINT(tm->tm_mon));
    tlObjectSet_(res, _s_year, tlINT(tm->tm_year));
    tlObjectSet_(res, _s_wday, tlINT(tm->tm_wday));
    tlObjectSet_(res, _s_mday, tlINT(tm->tm_mday));
    tlObjectSet_(res, _s_yday, tlINT(tm->tm_yday));
    tlObjectSet_(res, _s_isdst, tlINT(tm->tm_isdst));
    tlHandle zone = tm->tm_zone? tlStringFromCopy(tm->tm_zone, 0) : tlNull;
    tlObjectSet_(res, _s_zone, zone);
    tlObjectSet_(res, _s_gmtoff, tlINT(tm->tm_gmtoff));
    return res;
}

static void object_to_tm(tlObject* from, struct tm* tm) {
    tm->tm_sec = tl_int(tlObjectGet(from, _s_sec));
    tm->tm_min = tl_int(tlObjectGet(from, _s_min));
    tm->tm_hour = tl_int(tlObjectGet(from, _s_hour));
    tm->tm_mon = tl_int(tlObjectGet(from, _s_mon));
    tm->tm_year = tl_int(tlObjectGet(from, _s_year));
    tm->tm_wday = tl_int(tlObjectGet(from, _s_wday));
    tm->tm_mday = tl_int(tlObjectGet(from, _s_mday));
    tm->tm_yday = tl_int(tlObjectGet(from, _s_yday));
    tm->tm_isdst = tl_int(tlObjectGet(from, _s_isdst));
    tlString* zone = tlStringCast(tlObjectGet(from, _s_isdst));
    tm->tm_zone = zone? (char*)tlStringData(zone) : null;
    tm->tm_gmtoff = tl_int(tlObjectGet(from, _s_gmtoff));
}

static tlHandle _localtime(tlArgs* args) {
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
    return object_from_tm(&tm);
}

static tlHandle _gmtime(tlArgs* args) {
    time_t time;
    if (tlArgsSize(args) > 0) {
        time = tl_double(tlArgsGet(args, 0));
    } else {
        struct timeval tv;
        gettimeofday(&tv, null);
        time = tv.tv_sec;
    }
    struct tm tm;
    gmtime_r(&time, &tm);
    return object_from_tm(&tm);
}

static tlHandle _time(tlArgs* args) {
    if (tlArgsSize(args) == 0) {
        struct timeval tv;
        gettimeofday(&tv, null);
        double seconds = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
        return tlFLOAT(seconds);
    }

    tlObject* from = tlObjectCast(tlArgsGet(args, 0));
    if (!from) TL_THROW("expect a map");
    struct tm tm;
    object_to_tm(from, &tm);
    return tlFLOAT(mktime(&tm));
}

static tlHandle _strptime(tlArgs* args) {
    tlString* format = tlStringCast(tlArgsGet(args, 0));
    if (!format) TL_THROW("strptime requires a format string arg[1]");
    tlString* input = tlStringCast(tlArgsGet(args, 1));
    if (!input) TL_THROW("strptime requires a time string as arg[2]");

    struct tm tm;
    strptime(tlStringData(input), tlStringData(format), &tm);
    return object_from_tm(&tm);
}

static tlHandle _strftime(tlArgs* args) {
    tlString* format = tlStringCast(tlArgsGet(args, 0));
    if (!format) TL_THROW("strftime requires a format string arg[1]");

    struct tm tm;
    tlHandle arg1 = tlArgsGet(args, 1);
    if (arg1) {
        if (tlNumberIs(arg1)) {
            time_t time;
            time = tl_double(arg1);
            localtime_r(&time, &tm);
        } else if (tlObjectIs(arg1)) {
            object_to_tm(tlObjectAs(arg1), &tm);
        } else {
            TL_THROW("strftime arg[1] must be a timestamp or a map");
        }
    } else {
        struct timeval tv;
        gettimeofday(&tv, null);
        time_t time = tv.tv_sec;
        localtime_r(&time, &tm);
    }

    char buf[1024];
    int size = strftime(buf, sizeof(buf), tlStringData(format), &tm);
    return tlStringFromCopy(buf, size);
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
    _timeMap = tlObjectNew(keys);
    tlObjectToObject_(_timeMap);

    tl_register_global("time", tlNATIVE(_time, "time"));
    tl_register_global("localtime", tlNATIVE(_localtime, "localtime"));
    tl_register_global("gmtime", tlNATIVE(_gmtime, "gmtime"));
    tl_register_global("strptime", tlNATIVE(_strptime, "strptime"));
    tl_register_global("strftime", tlNATIVE(_strftime, "strftime"));
}

