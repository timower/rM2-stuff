#pragma once

namespace rmlib::type_id {
class TypeIdT {
  using Sig = TypeIdT();

  Sig* id;
  TypeIdT(Sig* id) : id{ id } {}

public:
  template<typename T>
  friend TypeIdT typeId();

  bool operator==(TypeIdT o) const { return id == o.id; }
  bool operator!=(TypeIdT o) const { return id != o.id; }
};

template<typename T>
TypeIdT
typeId() {
  return &typeId<T>;
}

} // namespace rmlib::type_id
