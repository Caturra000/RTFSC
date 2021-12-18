  /**
   *  @brief  Common %iterator class.
   *
   *  This class does nothing but define nested typedefs.  %Iterator classes
   *  can inherit from this class to save some work.  The typedefs are then
   *  used in specializations and overloading.
   *
   *  In particular, there are no default implementations of requirements
   *  such as @c operator++ and the like.  (How could there be?)
  */
  template<typename _Category, typename _Tp, typename _Distance = ptrdiff_t,
           typename _Pointer = _Tp*, typename _Reference = _Tp&>
    struct iterator
    {
      /// One of the @link iterator_tags tag types@endlink.
      typedef _Category  iterator_category;
      /// The type "pointed to" by the iterator.
      typedef _Tp        value_type;
      /// Distance between iterators is represented as this type.
      typedef _Distance  difference_type;
      /// This type represents a pointer-to-value_type.
      typedef _Pointer   pointer;
      /// This type represents a reference-to-value_type.
      typedef _Reference reference;
    };
