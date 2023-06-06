#include "utils.h"
#include "string.h"
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <functional>
#include <algorithm>

namespace Utils {

	static int ssrc_num = 0;
	static int cseq_num = 9999;
  std::string Util::generateRandom(int len) {
    const char charset[] = "0123456789" "abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const size_t size = sizeof(charset) - 1;

    auto randomChar = [&]() {
      return charset[ rand() % size ];
    };

    auto resultSize = len;
    std::string result(resultSize, 0);
    std::generate_n(result.begin(), resultSize, randomChar);

    return result;
  }

  std::string Util::generateRandom1(int len) {
	  const char charset[] = "0123456789";
	  const size_t size = sizeof(charset) - 1;

	  auto randomChar = [&]() {
		  return charset[rand() % size];
	  };

	  auto resultSize = len;
	  std::string result(resultSize, 0);
	  std::generate_n(result.begin(), resultSize, randomChar);

	  return result;
  }

  std::string Util::stringTrimEnd(std::string str, std::string trim_chars)
  {
	  std::string ret = str;

	  for (int i = 0; i < (int)trim_chars.length(); i++) {
		  char ch = trim_chars.at(i);

		  while (!ret.empty() && ret.at(ret.length() - 1) == ch) {
			  ret.erase(ret.end() - 1);
			  i = -1;
		  }
	  }

	  return ret;
  }

  std::vector<std::string> Util::split_str(const std::string& str, const std::string& delim)
  {
	  std::vector<std::string> ret;
	  size_t pre_pos = 0;
	  std::string tmp;
	  size_t pos = 0;
	  do {
		  pos = str.find(delim, pre_pos);
		  tmp = str.substr(pre_pos, pos - pre_pos);
		  ret.push_back(tmp);
		  pre_pos = pos + delim.size();
	  } while (pos != std::string::npos);

	  return ret;
  }

  void Util::skip_first_spaces(std::string& str)
  {
	  while (!str.empty() && str[0] == ' ') {
		  str.erase(0, 1);
	  }
  }

  uint32_t Util::generalSSRC()
  {
	  if (ssrc_num == 0)
	  {
		  ssrc_num = getpid() * 10000 + getpid() * 100 + getpid();
	  }
	  return ++ssrc_num;
  }

  uint32_t Util::generalCseqNum()
  {
	  if (cseq_num == 9999)
	  {
		  cseq_num = getpid() * 10000 + getpid() * 100 + getpid();
	  }
	  return ++cseq_num;
  }

  uint16_t Util::generalSeq()
  {
	  struct timeval time;
	  uint64_t nowms;
	  gettimeofday(&time, NULL);
	  uint16_t seq;
	  nowms = time.tv_sec * 1000 + time.tv_usec / 1000;
	  seq = (uint16_t)((uint16_t)(nowms & 0x00FF) + 400);
	  return seq;
  }

  uint16_t Util::generalPort()
  {
	  int r = rand();
	  uint16_t port = (r % 60000) + 2000;  // 2000 ртио╤к©з╨е 
	  return port;
  }

  uint32_t Util::getCurrentTimeMs()
  {
	  struct timeval time;
	  uint32_t nowms;
	  gettimeofday(&time, NULL);
	  nowms = time.tv_sec * 1000 + time.tv_usec / 1000;
	  return nowms;
  }

  char* Util::strDup(char const* str)
  {
	  if (str == NULL) return NULL;
	  size_t len = strlen(str) + 1;
	  char* copy = new char[len];

	  if (copy != NULL) {
		  memcpy(copy, str, len);
	  }
	  return copy;
  }

  char* Util::strDupSize(char const* str)
  {
	  if (str == NULL) return NULL;
	  size_t len = strlen(str) + 1;
	  char* copy = new char[len];

	  return copy;
  }

}
