#if !defined(UTILS_PARAM_TRAITS_HPP__)
#define UTILS_PARAM_TRAITS_HPP__

#include <toonz_plugin.h>
#include <toonz_hostif.h>
#include <toonz_params.h>

/* helper-types to define toonz_param_*_t's table */
struct param_desc_t : public toonz_param_desc_t {
	param_desc_t(const char *k, const char *l, int ttag, const char *nt = "")
	{
		base = {{1, 0} /* type version */, TOONZ_PARAM_DESC_TYPE_PARAM, l};
		key = k;
		note = nt;
		reserved_[0] = NULL; // must be zero
		reserved_[1] = NULL;
		traits_tag = ttag;
	}
};

template <typename T>
void init_param_traits_union(toonz_param_desc_t &t, const T &p){};

template <typename T>
struct param_desc_holder_t {
	typedef T realtype;
	param_desc_t t;
	param_desc_holder_t(const char *key, const char *label, const T &p, const char *note) : t{key, label, T::E, note} { init_param_traits_union<T>(t, p); }
};

template <>
void init_param_traits_union<toonz_param_traits_double_t>(toonz_param_desc_t &t, const toonz_param_traits_double_t &p) { t.traits.d = p; }

template <>
void init_param_traits_union<toonz_param_traits_int_t>(toonz_param_desc_t &t, const toonz_param_traits_int_t &p) { t.traits.i = p; }
template <>
void init_param_traits_union<toonz_param_traits_enum_t>(toonz_param_desc_t &t, const toonz_param_traits_enum_t &p) { t.traits.e = p; }
template <>
void init_param_traits_union<toonz_param_traits_range_t>(toonz_param_desc_t &t, const toonz_param_traits_range_t &p) { t.traits.rd = p; }
template <>
void init_param_traits_union<toonz_param_traits_bool_t>(toonz_param_desc_t &t, const toonz_param_traits_bool_t &p) { t.traits.b = p; }
template <>
void init_param_traits_union<toonz_param_traits_color_t>(toonz_param_desc_t &t, const toonz_param_traits_color_t &p) { t.traits.c = p; }
template <>
void init_param_traits_union<toonz_param_traits_point_t>(toonz_param_desc_t &t, const toonz_param_traits_point_t &p) { t.traits.p = p; }
template <>
void init_param_traits_union<toonz_param_traits_spectrum_t>(toonz_param_desc_t &t, const toonz_param_traits_spectrum_t &p) { t.traits.g = p; }
template <>
void init_param_traits_union<toonz_param_traits_string_t>(toonz_param_desc_t &t, const toonz_param_traits_string_t &p) { t.traits.s = p; }
template <>
void init_param_traits_union<toonz_param_traits_tonecurve_t>(toonz_param_desc_t &t, const toonz_param_traits_tonecurve_t &p) { t.traits.t = p; }

typedef param_desc_holder_t<toonz_param_traits_double_t> traits_double_t;
typedef param_desc_holder_t<toonz_param_traits_int_t> traits_int_t;
typedef param_desc_holder_t<toonz_param_traits_enum_t> traits_enum_t;
typedef param_desc_holder_t<toonz_param_traits_range_t> traits_range_t;
typedef param_desc_holder_t<toonz_param_traits_bool_t> traits_bool_t;
typedef param_desc_holder_t<toonz_param_traits_color_t> traits_color_t;
typedef param_desc_holder_t<toonz_param_traits_point_t> traits_point_t;
typedef param_desc_holder_t<toonz_param_traits_spectrum_t> traits_spectrum_t;
typedef param_desc_holder_t<toonz_param_traits_string_t> traits_string_t;
typedef param_desc_holder_t<toonz_param_traits_tonecurve_t> traits_tonecurve_t;

template <typename T>
static param_desc_t param_desc_ctor(const char *key, const char *label, const typename T::realtype &p, const char *note = "")
{
	param_desc_holder_t<typename T::realtype> t(key, label, p, note);
	return t.t;
}

static toonz_param_group_t param_group_ctor(const char *label, size_t n, toonz_param_desc_t *p)
{
	return {{{1, 0}, TOONZ_PARAM_DESC_TYPE_GROUP, label}, static_cast<int>(n), p};
}

static toonz_param_page_t param_page_ctor(const char *label, size_t n, toonz_param_group_t *g)
{
	return {{{1, 0}, TOONZ_PARAM_DESC_TYPE_PAGE, label}, static_cast<int>(n), g};
}

#endif
