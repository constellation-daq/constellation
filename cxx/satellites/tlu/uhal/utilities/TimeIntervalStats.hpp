
#ifndef _uhal_TimeIntervalStats_hpp_
#define _uhal_TimeIntervalStats_hpp_


#include <chrono>
#include <iosfwd>                          // for ostream
#include <queue>
#include <stddef.h>                        // for size_t


namespace uhal {

class TimeIntervalStats {
public:
  typedef std::chrono::steady_clock Clock_t;

  TimeIntervalStats();
  ~TimeIntervalStats();

  size_t size() const;

  const Clock_t::duration& min() const;

  const Clock_t::duration& max() const;

  Clock_t::duration mean() const;

  const std::queue<Clock_t::duration>& getLatestMeasurements() const;

  void add(const Clock_t::time_point& aT1, const Clock_t::time_point& aT2);

  void clear();

private:
  std::queue<Clock_t::duration> mLatestMeasurements;

  Clock_t::duration mMin;
  Clock_t::duration mMax;
  Clock_t::duration mSum;

  size_t nMeasurements;
};

std::ostream& operator<<(std::ostream&, const TimeIntervalStats&);

} // end ns uhal


#endif
