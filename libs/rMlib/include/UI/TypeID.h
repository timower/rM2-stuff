#pragma once

namespace rmlib::typeID {
class type_id_t {
  using sig = type_id_t();

  sig* id;
  type_id_t(sig* id) : id{ id } {}

public:
  template<typename T>
  friend type_id_t type_id();

  bool operator==(type_id_t o) const { return id == o.id; }
  bool operator!=(type_id_t o) const { return id != o.id; }
};

template<typename T>
type_id_t
type_id() {
  return &type_id<T>;
}

} // namespace rmlib::typeID
