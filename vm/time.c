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

static tlValue _Time(tlArgs* args) {
    struct timeval tv;
    gettimeofday(&tv, null);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);

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
    tlMapSetSym_(res, _s_zone, tlTextFromCopy(tm.tm_zone, 0));
    tlMapSetSym_(res, _s_gmtoff, tlINT(tm.tm_gmtoff));
    return res;
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

    tl_register_global("Time", tlNATIVE(_Time, "Time"));
}

