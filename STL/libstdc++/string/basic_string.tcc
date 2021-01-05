#ifndef _BASIC_STRING_TCC
#define _BASIC_STRING_TCC 1

#pragma GCC system_header

#include <bits/cxxabi_forced.h>

namespace std _GLIBCXX_VISIBILITY(default)
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

#if _GLIBCXX_USE_CXX11_ABI

  template<typename _CharT, typename _Traits, typename _Alloc>
    const typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::npos;

  template<typename _CharT, typename _Traits, typename _Alloc>
    void
    basic_string<_CharT, _Traits, _Alloc>::
    swap(basic_string& __s) _GLIBCXX_NOEXCEPT
    {
      if (this == &__s)
	return;

      _Alloc_traits::_S_on_swap(_M_get_allocator(), __s._M_get_allocator());

      if (_M_is_local())
	if (__s._M_is_local())
	  {
	    if (length() && __s.length())
	      {
		_CharT __tmp_data[_S_local_capacity + 1];
		traits_type::copy(__tmp_data, __s._M_local_buf,
				  _S_local_capacity + 1);
		traits_type::copy(__s._M_local_buf, _M_local_buf,
				  _S_local_capacity + 1);
		traits_type::copy(_M_local_buf, __tmp_data,
				  _S_local_capacity + 1);
	      }
	    else if (__s.length())
	      {
		traits_type::copy(_M_local_buf, __s._M_local_buf,
				  _S_local_capacity + 1);
		_M_length(__s.length());
		__s._M_set_length(0);
		return;
	      }
	    else if (length())
	      {
		traits_type::copy(__s._M_local_buf, _M_local_buf,
				  _S_local_capacity + 1);
		__s._M_length(length());
		_M_set_length(0);
		return;
	      }
	  }
	else
	  {
	    const size_type __tmp_capacity = __s._M_allocated_capacity;
	    traits_type::copy(__s._M_local_buf, _M_local_buf,
			      _S_local_capacity + 1);
	    _M_data(__s._M_data());
	    __s._M_data(__s._M_local_buf);
	    _M_capacity(__tmp_capacity);
	  }
      else
	{
	  const size_type __tmp_capacity = _M_allocated_capacity;
	  if (__s._M_is_local())
	    {
	      traits_type::copy(_M_local_buf, __s._M_local_buf,
				_S_local_capacity + 1);
	      __s._M_data(_M_data());
	      _M_data(_M_local_buf);
	    }
	  else
	    {
	      pointer __tmp_ptr = _M_data();
	      _M_data(__s._M_data());
	      __s._M_data(__tmp_ptr);
	      _M_capacity(__s._M_allocated_capacity);
	    }
	  __s._M_capacity(__tmp_capacity);
	}

      const size_type __tmp_length = length();
      _M_length(__s.length());
      __s._M_length(__tmp_length);
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::pointer
    basic_string<_CharT, _Traits, _Alloc>::
    _M_create(size_type& __capacity, size_type __old_capacity)
    {
      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // 83.  String::npos vs. string::max_size()
      if (__capacity > max_size())
	std::__throw_length_error(__N("basic_string::_M_create"));

      // The below implements an exponential growth policy, necessary to
      // meet amortized linear time requirements of the library: see
      // http://gcc.gnu.org/ml/libstdc++/2001-07/msg00085.html.
      if (__capacity > __old_capacity && __capacity < 2 * __old_capacity)
	{
	  __capacity = 2 * __old_capacity;
	  // Never allocate a string bigger than max_size.
	  if (__capacity > max_size())
	    __capacity = max_size();
	}

      // NB: Need an array of char_type[__capacity], plus a terminating
      // null char_type() element.
      return _Alloc_traits::allocate(_M_get_allocator(), __capacity + 1);
    }

  // NB: This is the special case for Input Iterators, used in
  // istreambuf_iterators, etc.
  // Input Iterators have a cost structure very different from
  // pointers, calling for a different coding style.
  template<typename _CharT, typename _Traits, typename _Alloc>
    template<typename _InIterator>
      void
      basic_string<_CharT, _Traits, _Alloc>::
      _M_construct(_InIterator __beg, _InIterator __end,
		   std::input_iterator_tag)
      {
	size_type __len = 0;
	size_type __capacity = size_type(_S_local_capacity);

	while (__beg != __end && __len < __capacity)
	  {
	    _M_data()[__len++] = *__beg;
	    ++__beg;
	  }

	__try
	  {
	    while (__beg != __end)
	      {
		if (__len == __capacity)
		  {
		    // Allocate more space.
		    __capacity = __len + 1;
		    pointer __another = _M_create(__capacity, __len);
		    this->_S_copy(__another, _M_data(), __len);
		    _M_dispose();
		    _M_data(__another);
		    _M_capacity(__capacity);
		  }
		_M_data()[__len++] = *__beg;
		++__beg;
	      }
	  }
	__catch(...)
	  {
	    _M_dispose();
	    __throw_exception_again;
	  }

	_M_set_length(__len);
      }

  template<typename _CharT, typename _Traits, typename _Alloc>
    template<typename _InIterator>
      void
      basic_string<_CharT, _Traits, _Alloc>::
      _M_construct(_InIterator __beg, _InIterator __end,
		   std::forward_iterator_tag)
      {
	// NB: Not required, but considered best practice.
	if (__gnu_cxx::__is_null_pointer(__beg) && __beg != __end)
	  std::__throw_logic_error(__N("basic_string::"
				       "_M_construct null not valid"));

	size_type __dnew = static_cast<size_type>(std::distance(__beg, __end));

	if (__dnew > size_type(_S_local_capacity))
	  {
	    _M_data(_M_create(__dnew, size_type(0)));
	    _M_capacity(__dnew);
	  }

	// Check for out_of_range and length_error exceptions.
	__try
	  { this->_S_copy_chars(_M_data(), __beg, __end); }
	__catch(...)
	  {
	    _M_dispose();
	    __throw_exception_again;
	  }

	_M_set_length(__dnew);
      }

  template<typename _CharT, typename _Traits, typename _Alloc>
    void
    basic_string<_CharT, _Traits, _Alloc>::
    _M_construct(size_type __n, _CharT __c)
    {
      if (__n > size_type(_S_local_capacity))
	{
	  _M_data(_M_create(__n, size_type(0)));
	  _M_capacity(__n);
	}

      if (__n)
	this->_S_assign(_M_data(), __n, __c);

      _M_set_length(__n);
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    void
    basic_string<_CharT, _Traits, _Alloc>::
    _M_assign(const basic_string& __str)
    {
      if (this != &__str)
	{
	  const size_type __rsize = __str.length();
	  const size_type __capacity = capacity();

	  if (__rsize > __capacity)
	    {
	      size_type __new_capacity = __rsize;
	      pointer __tmp = _M_create(__new_capacity, __capacity);
	      _M_dispose();
	      _M_data(__tmp);
	      _M_capacity(__new_capacity);
	    }

	  if (__rsize)
	    this->_S_copy(_M_data(), __str._M_data(), __rsize);

	  _M_set_length(__rsize);
	}
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    void
    basic_string<_CharT, _Traits, _Alloc>::
    reserve(size_type __res)
    {
      // Make sure we don't shrink below the current size.
      if (__res < length())
	__res = length();

      const size_type __capacity = capacity();
      if (__res != __capacity)
	{
	  if (__res > __capacity
	      || __res > size_type(_S_local_capacity))
	    {
	      pointer __tmp = _M_create(__res, __capacity);
	      this->_S_copy(__tmp, _M_data(), length() + 1);
	      _M_dispose();
	      _M_data(__tmp);
	      _M_capacity(__res);
	    }
	  else if (!_M_is_local())
	    {
	      this->_S_copy(_M_local_data(), _M_data(), length() + 1);
	      _M_destroy(__capacity);
	      _M_data(_M_local_data());
	    }
	}
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    void
    basic_string<_CharT, _Traits, _Alloc>::
    _M_mutate(size_type __pos, size_type __len1, const _CharT* __s,
	      size_type __len2)
    {
      const size_type __how_much = length() - __pos - __len1;

      size_type __new_capacity = length() + __len2 - __len1;
      pointer __r = _M_create(__new_capacity, capacity());

      if (__pos)
	this->_S_copy(__r, _M_data(), __pos);
      if (__s && __len2)
	this->_S_copy(__r + __pos, __s, __len2);
      if (__how_much)
	this->_S_copy(__r + __pos + __len2,
		      _M_data() + __pos + __len1, __how_much);

      _M_dispose();
      _M_data(__r);
      _M_capacity(__new_capacity);
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    void
    basic_string<_CharT, _Traits, _Alloc>::
    _M_erase(size_type __pos, size_type __n)
    {
      const size_type __how_much = length() - __pos - __n;

      if (__how_much && __n)
	this->_S_move(_M_data() + __pos, _M_data() + __pos + __n, __how_much);

      _M_set_length(length() - __n);
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    void
    basic_string<_CharT, _Traits, _Alloc>::
    resize(size_type __n, _CharT __c)
    {
      const size_type __size = this->size();
      if (__size < __n)
	this->append(__n - __size, __c);
      else if (__n < __size)
	this->_M_set_length(__n);
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    basic_string<_CharT, _Traits, _Alloc>&
    basic_string<_CharT, _Traits, _Alloc>::
    _M_append(const _CharT* __s, size_type __n)
    {
      const size_type __len = __n + this->size();

      if (__len <= this->capacity())
	{
	  if (__n)
	    this->_S_copy(this->_M_data() + this->size(), __s, __n);
	}
      else
	this->_M_mutate(this->size(), size_type(0), __s, __n);

      this->_M_set_length(__len);
      return *this;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    template<typename _InputIterator>
      basic_string<_CharT, _Traits, _Alloc>&
      basic_string<_CharT, _Traits, _Alloc>::
      _M_replace_dispatch(const_iterator __i1, const_iterator __i2,
			  _InputIterator __k1, _InputIterator __k2,
			  std::__false_type)
      {
	const basic_string __s(__k1, __k2);
	const size_type __n1 = __i2 - __i1;
	return _M_replace(__i1 - begin(), __n1, __s._M_data(),
			  __s.size());
      }

  template<typename _CharT, typename _Traits, typename _Alloc>
    basic_string<_CharT, _Traits, _Alloc>&
    basic_string<_CharT, _Traits, _Alloc>::
    _M_replace_aux(size_type __pos1, size_type __n1, size_type __n2,
		   _CharT __c)
    {
      _M_check_length(__n1, __n2, "basic_string::_M_replace_aux");

      const size_type __old_size = this->size();
      const size_type __new_size = __old_size + __n2 - __n1;

      if (__new_size <= this->capacity())
	{
	  pointer __p = this->_M_data() + __pos1;

	  const size_type __how_much = __old_size - __pos1 - __n1;
	  if (__how_much && __n1 != __n2)
	    this->_S_move(__p + __n2, __p + __n1, __how_much);
	}
      else
	this->_M_mutate(__pos1, __n1, 0, __n2);

      if (__n2)
	this->_S_assign(this->_M_data() + __pos1, __n2, __c);

      this->_M_set_length(__new_size);
      return *this;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    basic_string<_CharT, _Traits, _Alloc>&
    basic_string<_CharT, _Traits, _Alloc>::
    _M_replace(size_type __pos, size_type __len1, const _CharT* __s,
	       const size_type __len2)
    {
      _M_check_length(__len1, __len2, "basic_string::_M_replace");

      const size_type __old_size = this->size();
      const size_type __new_size = __old_size + __len2 - __len1;

      if (__new_size <= this->capacity())
	{
	  pointer __p = this->_M_data() + __pos;

	  const size_type __how_much = __old_size - __pos - __len1;
	  if (_M_disjunct(__s))
	    {
	      if (__how_much && __len1 != __len2)
		this->_S_move(__p + __len2, __p + __len1, __how_much);
	      if (__len2)
		this->_S_copy(__p, __s, __len2);
	    }
	  else
	    {
	      // Work in-place.
	      if (__len2 && __len2 <= __len1)
		this->_S_move(__p, __s, __len2);
	      if (__how_much && __len1 != __len2)
		this->_S_move(__p + __len2, __p + __len1, __how_much);
	      if (__len2 > __len1)
		{
		  if (__s + __len2 <= __p + __len1)
		    this->_S_move(__p, __s, __len2);
		  else if (__s >= __p + __len1)
		    this->_S_copy(__p, __s + __len2 - __len1, __len2);
		  else
		    {
		      const size_type __nleft = (__p + __len1) - __s;
		      this->_S_move(__p, __s, __nleft);
		      this->_S_copy(__p + __nleft, __p + __len2,
				    __len2 - __nleft);
		    }
		}
	    }
	}
      else
	this->_M_mutate(__pos, __len1, __s, __len2);

      this->_M_set_length(__new_size);
      return *this;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    copy(_CharT* __s, size_type __n, size_type __pos) const
    {
      _M_check(__pos, "basic_string::copy");
      __n = _M_limit(__pos, __n);
      __glibcxx_requires_string_len(__s, __n);
      if (__n)
	_S_copy(__s, _M_data() + __pos, __n);
      // 21.3.5.7 par 3: do not append null.  (good.)
      return __n;
    }

#else  // !_GLIBCXX_USE_CXX11_ABI
    // 略

#endif  // !_GLIBCXX_USE_CXX11_ABI
   
  template<typename _CharT, typename _Traits, typename _Alloc>
    basic_string<_CharT, _Traits, _Alloc>
    operator+(const _CharT* __lhs,
	      const basic_string<_CharT, _Traits, _Alloc>& __rhs)
    {
      __glibcxx_requires_string(__lhs);
      typedef basic_string<_CharT, _Traits, _Alloc> __string_type;
      typedef typename __string_type::size_type	  __size_type;
      const __size_type __len = _Traits::length(__lhs);
      __string_type __str;
      __str.reserve(__len + __rhs.size());
      __str.append(__lhs, __len);
      __str.append(__rhs);
      return __str;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    basic_string<_CharT, _Traits, _Alloc>
    operator+(_CharT __lhs, const basic_string<_CharT, _Traits, _Alloc>& __rhs)
    {
      typedef basic_string<_CharT, _Traits, _Alloc> __string_type;
      typedef typename __string_type::size_type	  __size_type;
      __string_type __str;
      const __size_type __len = __rhs.size();
      __str.reserve(__len + 1);
      __str.append(__size_type(1), __lhs);
      __str.append(__rhs);
      return __str;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    find(const _CharT* __s, size_type __pos, size_type __n) const
    _GLIBCXX_NOEXCEPT
    {
      __glibcxx_requires_string_len(__s, __n);
      const size_type __size = this->size();

      if (__n == 0)
	return __pos <= __size ? __pos : npos;
      if (__pos >= __size)
	return npos;

      const _CharT __elem0 = __s[0];
      const _CharT* const __data = data();
      const _CharT* __first = __data + __pos;
      const _CharT* const __last = __data + __size;
      size_type __len = __size - __pos;

      while (__len >= __n)
	{
	  // Find the first occurrence of __elem0:
	  __first = traits_type::find(__first, __len - __n + 1, __elem0);
	  if (!__first)
	    return npos;
	  // Compare the full strings from the first occurrence of __elem0.
	  // We already know that __first[0] == __s[0] but compare them again
	  // anyway because __s is probably aligned, which helps memcmp.
	  if (traits_type::compare(__first, __s, __n) == 0)
	    return __first - __data;
	  __len = __last - ++__first;
	}
      return npos;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    find(_CharT __c, size_type __pos) const _GLIBCXX_NOEXCEPT
    {
      size_type __ret = npos;
      const size_type __size = this->size();
      if (__pos < __size)
	{
	  const _CharT* __data = _M_data();
	  const size_type __n = __size - __pos;
	  const _CharT* __p = traits_type::find(__data + __pos, __n, __c);
	  if (__p)
	    __ret = __p - __data;
	}
      return __ret;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    rfind(const _CharT* __s, size_type __pos, size_type __n) const
    _GLIBCXX_NOEXCEPT
    {
      __glibcxx_requires_string_len(__s, __n);
      const size_type __size = this->size();
      if (__n <= __size)
	{
	  __pos = std::min(size_type(__size - __n), __pos);
	  const _CharT* __data = _M_data();
	  do
	    {
	      if (traits_type::compare(__data + __pos, __s, __n) == 0)
		return __pos;
	    }
	  while (__pos-- > 0);
	}
      return npos;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    rfind(_CharT __c, size_type __pos) const _GLIBCXX_NOEXCEPT
    {
      size_type __size = this->size();
      if (__size)
	{
	  if (--__size > __pos)
	    __size = __pos;
	  for (++__size; __size-- > 0; )
	    if (traits_type::eq(_M_data()[__size], __c))
	      return __size;
	}
      return npos;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    find_first_of(const _CharT* __s, size_type __pos, size_type __n) const
    _GLIBCXX_NOEXCEPT
    {
      __glibcxx_requires_string_len(__s, __n);
      for (; __n && __pos < this->size(); ++__pos)
	{
	  const _CharT* __p = traits_type::find(__s, __n, _M_data()[__pos]);
	  if (__p)
	    return __pos;
	}
      return npos;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    find_last_of(const _CharT* __s, size_type __pos, size_type __n) const
    _GLIBCXX_NOEXCEPT
    {
      __glibcxx_requires_string_len(__s, __n);
      size_type __size = this->size();
      if (__size && __n)
	{
	  if (--__size > __pos)
	    __size = __pos;
	  do
	    {
	      if (traits_type::find(__s, __n, _M_data()[__size]))
		return __size;
	    }
	  while (__size-- != 0);
	}
      return npos;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    find_first_not_of(const _CharT* __s, size_type __pos, size_type __n) const
    _GLIBCXX_NOEXCEPT
    {
      __glibcxx_requires_string_len(__s, __n);
      for (; __pos < this->size(); ++__pos)
	if (!traits_type::find(__s, __n, _M_data()[__pos]))
	  return __pos;
      return npos;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    find_first_not_of(_CharT __c, size_type __pos) const _GLIBCXX_NOEXCEPT
    {
      for (; __pos < this->size(); ++__pos)
	if (!traits_type::eq(_M_data()[__pos], __c))
	  return __pos;
      return npos;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    find_last_not_of(const _CharT* __s, size_type __pos, size_type __n) const
    _GLIBCXX_NOEXCEPT
    {
      __glibcxx_requires_string_len(__s, __n);
      size_type __size = this->size();
      if (__size)
	{
	  if (--__size > __pos)
	    __size = __pos;
	  do
	    {
	      if (!traits_type::find(__s, __n, _M_data()[__size]))
		return __size;
	    }
	  while (__size--);
	}
      return npos;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    typename basic_string<_CharT, _Traits, _Alloc>::size_type
    basic_string<_CharT, _Traits, _Alloc>::
    find_last_not_of(_CharT __c, size_type __pos) const _GLIBCXX_NOEXCEPT
    {
      size_type __size = this->size();
      if (__size)
	{
	  if (--__size > __pos)
	    __size = __pos;
	  do
	    {
	      if (!traits_type::eq(_M_data()[__size], __c))
		return __size;
	    }
	  while (__size--);
	}
      return npos;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    int
    basic_string<_CharT, _Traits, _Alloc>::
    compare(size_type __pos, size_type __n, const basic_string& __str) const
    {
      _M_check(__pos, "basic_string::compare");
      __n = _M_limit(__pos, __n);
      const size_type __osize = __str.size();
      const size_type __len = std::min(__n, __osize);
      int __r = traits_type::compare(_M_data() + __pos, __str.data(), __len);
      if (!__r)
	__r = _S_compare(__n, __osize);
      return __r;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    int
    basic_string<_CharT, _Traits, _Alloc>::
    compare(size_type __pos1, size_type __n1, const basic_string& __str,
	    size_type __pos2, size_type __n2) const
    {
      _M_check(__pos1, "basic_string::compare");
      __str._M_check(__pos2, "basic_string::compare");
      __n1 = _M_limit(__pos1, __n1);
      __n2 = __str._M_limit(__pos2, __n2);
      const size_type __len = std::min(__n1, __n2);
      int __r = traits_type::compare(_M_data() + __pos1,
				     __str.data() + __pos2, __len);
      if (!__r)
	__r = _S_compare(__n1, __n2);
      return __r;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    int
    basic_string<_CharT, _Traits, _Alloc>::
    compare(const _CharT* __s) const _GLIBCXX_NOEXCEPT
    {
      __glibcxx_requires_string(__s);
      const size_type __size = this->size();
      const size_type __osize = traits_type::length(__s);
      const size_type __len = std::min(__size, __osize);
      int __r = traits_type::compare(_M_data(), __s, __len);
      if (!__r)
	__r = _S_compare(__size, __osize);
      return __r;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    int
    basic_string <_CharT, _Traits, _Alloc>::
    compare(size_type __pos, size_type __n1, const _CharT* __s) const
    {
      __glibcxx_requires_string(__s);
      _M_check(__pos, "basic_string::compare");
      __n1 = _M_limit(__pos, __n1);
      const size_type __osize = traits_type::length(__s);
      const size_type __len = std::min(__n1, __osize);
      int __r = traits_type::compare(_M_data() + __pos, __s, __len);
      if (!__r)
	__r = _S_compare(__n1, __osize);
      return __r;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    int
    basic_string <_CharT, _Traits, _Alloc>::
    compare(size_type __pos, size_type __n1, const _CharT* __s,
	    size_type __n2) const
    {
      __glibcxx_requires_string_len(__s, __n2);
      _M_check(__pos, "basic_string::compare");
      __n1 = _M_limit(__pos, __n1);
      const size_type __len = std::min(__n1, __n2);
      int __r = traits_type::compare(_M_data() + __pos, __s, __len);
      if (!__r)
	__r = _S_compare(__n1, __n2);
      return __r;
    }

  // 21.3.7.9 basic_string::getline and operators
  template<typename _CharT, typename _Traits, typename _Alloc>
    basic_istream<_CharT, _Traits>&
    operator>>(basic_istream<_CharT, _Traits>& __in,
	       basic_string<_CharT, _Traits, _Alloc>& __str)
    {
      typedef basic_istream<_CharT, _Traits>		__istream_type;
      typedef basic_string<_CharT, _Traits, _Alloc>	__string_type;
      typedef typename __istream_type::ios_base         __ios_base;
      typedef typename __istream_type::int_type		__int_type;
      typedef typename __string_type::size_type		__size_type;
      typedef ctype<_CharT>				__ctype_type;
      typedef typename __ctype_type::ctype_base         __ctype_base;

      __size_type __extracted = 0;
      typename __ios_base::iostate __err = __ios_base::goodbit;
      typename __istream_type::sentry __cerb(__in, false);
      if (__cerb)
	{
	  __try
	    {
	      // Avoid reallocation for common case.
	      __str.erase();
	      _CharT __buf[128];
	      __size_type __len = 0;	      
	      const streamsize __w = __in.width();
	      const __size_type __n = __w > 0 ? static_cast<__size_type>(__w)
		                              : __str.max_size();
	      const __ctype_type& __ct = use_facet<__ctype_type>(__in.getloc());
	      const __int_type __eof = _Traits::eof();
	      __int_type __c = __in.rdbuf()->sgetc();

	      while (__extracted < __n
		     && !_Traits::eq_int_type(__c, __eof)
		     && !__ct.is(__ctype_base::space,
				 _Traits::to_char_type(__c)))
		{
		  if (__len == sizeof(__buf) / sizeof(_CharT))
		    {
		      __str.append(__buf, sizeof(__buf) / sizeof(_CharT));
		      __len = 0;
		    }
		  __buf[__len++] = _Traits::to_char_type(__c);
		  ++__extracted;
		  __c = __in.rdbuf()->snextc();
		}
	      __str.append(__buf, __len);

	      if (_Traits::eq_int_type(__c, __eof))
		__err |= __ios_base::eofbit;
	      __in.width(0);
	    }
	  __catch(__cxxabiv1::__forced_unwind&)
	    {
	      __in._M_setstate(__ios_base::badbit);
	      __throw_exception_again;
	    }
	  __catch(...)
	    {
	      // _GLIBCXX_RESOLVE_LIB_DEFECTS
	      // 91. Description of operator>> and getline() for string<>
	      // might cause endless loop
	      __in._M_setstate(__ios_base::badbit);
	    }
	}
      // 211.  operator>>(istream&, string&) doesn't set failbit
      if (!__extracted)
	__err |= __ios_base::failbit;
      if (__err)
	__in.setstate(__err);
      return __in;
    }

  template<typename _CharT, typename _Traits, typename _Alloc>
    basic_istream<_CharT, _Traits>&
    getline(basic_istream<_CharT, _Traits>& __in,
	    basic_string<_CharT, _Traits, _Alloc>& __str, _CharT __delim)
    {
      typedef basic_istream<_CharT, _Traits>		__istream_type;
      typedef basic_string<_CharT, _Traits, _Alloc>	__string_type;
      typedef typename __istream_type::ios_base         __ios_base;
      typedef typename __istream_type::int_type		__int_type;
      typedef typename __string_type::size_type		__size_type;

      __size_type __extracted = 0;
      const __size_type __n = __str.max_size();
      typename __ios_base::iostate __err = __ios_base::goodbit;
      typename __istream_type::sentry __cerb(__in, true);
      if (__cerb)
	{
	  __try
	    {
	      __str.erase();
	      const __int_type __idelim = _Traits::to_int_type(__delim);
	      const __int_type __eof = _Traits::eof();
	      __int_type __c = __in.rdbuf()->sgetc();

	      while (__extracted < __n
		     && !_Traits::eq_int_type(__c, __eof)
		     && !_Traits::eq_int_type(__c, __idelim))
		{
		  __str += _Traits::to_char_type(__c);
		  ++__extracted;
		  __c = __in.rdbuf()->snextc();
		}

	      if (_Traits::eq_int_type(__c, __eof))
		__err |= __ios_base::eofbit;
	      else if (_Traits::eq_int_type(__c, __idelim))
		{
		  ++__extracted;		  
		  __in.rdbuf()->sbumpc();
		}
	      else
		__err |= __ios_base::failbit;
	    }
	  __catch(__cxxabiv1::__forced_unwind&)
	    {
	      __in._M_setstate(__ios_base::badbit);
	      __throw_exception_again;
	    }
	  __catch(...)
	    {
	      // _GLIBCXX_RESOLVE_LIB_DEFECTS
	      // 91. Description of operator>> and getline() for string<>
	      // might cause endless loop
	      __in._M_setstate(__ios_base::badbit);
	    }
	}
      if (!__extracted)
	__err |= __ios_base::failbit;
      if (__err)
	__in.setstate(__err);
      return __in;
    }

  // Inhibit implicit instantiations for required instantiations,
  // which are defined via explicit instantiations elsewhere.
#if _GLIBCXX_EXTERN_TEMPLATE
  // The explicit instantiations definitions in src/c++11/string-inst.cc
  // are compiled as C++14, so the new C++17 members aren't instantiated.
  // Until those definitions are compiled as C++17 suppress the declaration,
  // so C++17 code will implicitly instantiate std::string and std::wstring
  // as needed.
# if __cplusplus <= 201703L && _GLIBCXX_EXTERN_TEMPLATE > 0
  extern template class basic_string<char>;
# elif ! _GLIBCXX_USE_CXX11_ABI
  // Still need to prevent implicit instantiation of the COW empty rep,
  // to ensure the definition in libstdc++.so is unique (PR 86138).
  extern template basic_string<char>::size_type
    basic_string<char>::_Rep::_S_empty_rep_storage[];
# endif

  extern template
    basic_istream<char>&
    operator>>(basic_istream<char>&, string&);
  extern template
    basic_ostream<char>&
    operator<<(basic_ostream<char>&, const string&);
  extern template
    basic_istream<char>&
    getline(basic_istream<char>&, string&, char);
  extern template
    basic_istream<char>&
    getline(basic_istream<char>&, string&);

#ifdef _GLIBCXX_USE_WCHAR_T
# if __cplusplus <= 201703L && _GLIBCXX_EXTERN_TEMPLATE > 0
  extern template class basic_string<wchar_t>;
# elif ! _GLIBCXX_USE_CXX11_ABI
  extern template basic_string<wchar_t>::size_type
    basic_string<wchar_t>::_Rep::_S_empty_rep_storage[];
# endif

  extern template
    basic_istream<wchar_t>&
    operator>>(basic_istream<wchar_t>&, wstring&);
  extern template
    basic_ostream<wchar_t>&
    operator<<(basic_ostream<wchar_t>&, const wstring&);
  extern template
    basic_istream<wchar_t>&
    getline(basic_istream<wchar_t>&, wstring&, wchar_t);
  extern template
    basic_istream<wchar_t>&
    getline(basic_istream<wchar_t>&, wstring&);
#endif // _GLIBCXX_USE_WCHAR_T
#endif // _GLIBCXX_EXTERN_TEMPLATE

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std

#endif
