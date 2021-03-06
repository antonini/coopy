#ifndef COOPY_EFFICIENTMAP
#define COOPY_EFFICIENTMAP

#ifdef SHOULD_HAVE_TR1
#  ifndef HAVE_TR1
#    error "TR1 flag not set correctly"
#  endif
#else
#  ifdef HAVE_TR1
#    error "TR1 flag set incorrectly"
#  endif
#endif


#ifdef HAVE_TR1
#  include <unordered_map>
#  define efficient_map std::unordered_map
#else

#  ifdef __GNUC__
#    include <ext/hash_map>

#    define efficient_map __gnu_cxx::hash_map

  namespace __gnu_cxx {
    template <>
      struct hash<std::string> {
	size_t operator() (const std::string& x) const {
	  return hash<const char*>()(x.c_str());
	}
      };
    
    template <>
      struct hash<long long> {
	size_t operator() (long long x) const {
	  return hash<long>()(x);
	}
      };
  }

#  else
#    warning "Unfamiliar compiler, compiling without a hash map chosen"
#    include <map>
#    define efficient_map std::map
#  endif
#endif

#endif

