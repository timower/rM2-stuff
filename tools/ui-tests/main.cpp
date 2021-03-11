#include <stdio.h>
/// Ideas: (most stolen from flutter)
// * Widgets are cheap to create, so have no real state.
// * StatefulWidget has state in seperate object, making it still cheap
// * The state is actually associated with the underlying render object in the
//    scene tree.
#if 1

struct TilemCalc;

class Spacer : public RenderWidget {
public:
  auto createRenderObject() {}
};

class CloseButton : public StatelessWidget {
public:
  auto build() {}
};

template<typename... Children>
class HStack : public StatelessWidget {
public:
  HStack(Children... children) {}

  auto build() {}
};

template<typename... Children>
class VStack : public StatelessWidget {
public:
  VStack(Children... children) {}

  auto build() {}
};

class Screen : public RenderWidget {
public:
  Screen(TilemCalc* calc) {
    // Save screen from calc
  }

  auto createRenderObject() {
    // Create render object that draws the saved calc screen
  }
};

template<typename Callable>
class KeyPad : public StatelessWidget {
public:
  KeyPad(Callable callable) {}

  auto build() {}
};

class TilemState {
private:
  // TODO: where/how?
  auto timerCallback() {
    // run calc for time...
    // mark state changed
  }

  auto build() {
    return VStack(HStack(Spacer(), CloseButton()),
                  Screen(calc),
                  KeyPad(/* onTouch */ [](auto key) {}));
  }

  TilemCalc* calc;
};

class Tilem : public StatefulWidget {
public:
  auto createState() { return TilemState(); }
};

template<typename App>
void
runApp(App app) {}

void
runTilem() {
  runApp(Tilem());
}

#endif

int
main() {
  puts("Hello world!");
  return 0;
}
