#ifndef DS_1307_H_
#define DS_1307_H_

#include <time.h>
void ds1307_set_time(const struct tm *time);
void ds1307_get_time(struct tm *time);
const char* get_day_of_week(const struct tm *time);
const char* get_date_string(const struct tm *time);
const char* get_time_string(const struct tm *time);


#endif