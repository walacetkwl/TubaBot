#include "timekeeping.h"

namespace tb {

// Algoritmo de dias civis (Howard Hinnant), inteiro e sem tabelas.
CivilClock::Local civilFromEpoch(int64_t t) {
  CivilClock::Local L{};
  int64_t days = t >= 0 ? t / 86400 : (t - 86399) / 86400;
  int64_t secs = t - days * 86400;
  L.hour = (int)(secs / 3600); L.minute = (int)((secs / 60) % 60); L.second = (int)(secs % 60);
  L.weekday = (int)((days + 4) % 7); if (L.weekday < 0) L.weekday += 7;  // 1970-01-01 foi quinta (4)
  int64_t z = days + 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  int64_t doe = z - era * 146097;
  int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int64_t y = yoe + era * 400;
  int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int64_t mp = (5 * doy + 2) / 153;
  int64_t d = doy - (153 * mp + 2) / 5 + 1;
  int64_t m = mp < 10 ? mp + 3 : mp - 9;
  L.year = (int)(y + (m <= 2)); L.month = (int)m; L.day = (int)d;
  return L;
}

CivilClock::Local CivilClock::local(ms_t now, int offsetMin) const {
  return civilFromEpoch(epoch(now) + (int64_t)offsetMin * 60);
}

}  // namespace tb
