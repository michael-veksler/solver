#ifndef ASSERT_HPP
#define ASSERT_HPP


namespace solver {
static constexpr bool ASSERT_ENABLED =
#ifdef NDEBUG
  false
#else
  true
#endif
  ;

} // namespace solver

#endif // ASSERT_HPP
