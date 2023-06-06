#ifndef _UTILS_H_
#define _UTILS_H_

#include <string>
#include <vector>

#define TIME_1_MILLSECOND  1     // 1ms
#define TIME_1_SECOND      (1000*TIME_1_MILLSECOND)  // 1000ms

#define UTILS_MIN(a, b) (((a) < (b))? (a) : (b))
#define UTILS_MAX(a, b) (((a) < (b))? (b) : (a))

namespace Utils {

  class Util {
    public:
      static std::string generateRandom(int len);
	  static std::string generateRandom1(int len);
	  static std::string stringTrimEnd(std::string str, std::string trim_chars);
	  static std::vector<std::string> split_str(const std::string& str, const std::string& delim);
	  static void skip_first_spaces(std::string& str);
	  static uint32_t generalSSRC();
	  static uint32_t generalCseqNum();
	  static uint16_t generalSeq();
	  static uint16_t generalPort();
	  static uint32_t getCurrentTimeMs();
	  static char* strDup(char const* str);
	  static char* strDupSize(char const* str);
  };

}

#endif // !_UTILS_H_

