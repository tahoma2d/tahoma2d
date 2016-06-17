#pragma once

#ifndef TATOMICVAR_H
#define TATOMICVAR_H

#undef DVAPI
#undef DVVAR
#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#include <atomic>

typedef std::atomic<int> atomic_t;

static __inline__ void atomic_set(atomic_t *v, const int value) {
  v->store(value);
}

static __inline__ int atomic_inc_return(atomic_t *v) {
  return v->fetch_add(1) + 1;  // post increment atomic
}

static __inline__ int atomic_dec_return(atomic_t *v) {
  return v->fetch_sub(1) - 1;  // post decriment atomic
}

static __inline__ int atomic_read(const atomic_t *v) { return v->load(); }

static __inline__ int atomic_add(int num, const atomic_t *v) {
  return const_cast<atomic_t *>(v)->fetch_add(num) +
         num; /* なんで const つけた? */
}

class DVAPI TAtomicVar {
public:
  TAtomicVar() : m_var(0) {}

  long operator++() {
    return m_var++ + 1;
  }

  long operator+=(long value) {
    m_var += value;
    return m_var;
  }

  long operator--() {
    return m_var-- - 1;
  }
  bool operator<=(const long &rhs) {
    return m_var <= rhs;
  };
  operator long() const {
    return m_var;
  };

  atomic_t m_var;

#if !defined(LINUX) || defined(LINUX) && (__GNUC__ == 3) && (__GNUC_MINOR__ > 1)
private:  // to avoid well known bug in gcc3 ... fixed in later versions..
#endif
  TAtomicVar &operator=(const TAtomicVar &);  // not implemented
  TAtomicVar(const TAtomicVar &v);            // not implemented
};

#endif
