
#ifndef __UTIL_H__
#define __UTIL_H__ 1

namespace SE {

template <class T> void Swap(T & oLeft, T & oRight) {

  oLeft   ^= oRight;
  oRight  ^= oLeft;
  oLeft   ^= oRight;
}



uint64_t Hash64(const uint64_t & key) {

  uint64_t  result    = 5381;

  uint8_t * orig_key  = (uint8_t *)(&key);

  for (size_t i = 0; i < 8; ++i) {

    result += (result << 5);
    result ^= (uint64_t) *orig_key++;
  }
  return result;
}

uint64_t Hash64(char const * data, const uint32_t size) {

	uint64_t        result  = 5381;
	
  uint8_t const * stop    = (uint8_t const *) data + size;

	while ((uint8_t * const)data != stop) {

		result += (result << 5);
		result ^= (uint64_t) *data++;
	}
	
  return result;
}



struct Hasher64 {

  uint64_t operator ()(const uint64_t & key) const { return Hash64(key); }

  uint64_t operator ()(char * const data, const uint32_t size) const { return Hash64(data, size); }
};



} // namespace SE

#endif
